//
// Simulação de incêndio em uma floresta.
// Baseada no código proposto por David Joiner.
//
// Uso: firesim <tamanho-do-problema> <nro. experimentos> <probab. maxima>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include "Random.h"
#include "Forest.h"
#include <mpi.h>
#include <sys/time.h>

#define MASTER 0

void
checkCommandLine(int argc, char** argv, int& size, int& trials, int& probs)
{
   if (argc > 1) {
      size = atoi(argv[1]);
   }
   if (argc > 2) {
      trials = atoi(argv[2]);
   }
   if (argc > 3) {
      probs = atoi(argv[3]);
   }
}

long wtime()
{
   struct timeval t;
   gettimeofday(&t, NULL);
   return t.tv_sec*1000000 + t.tv_usec;
}

int
main(int argc, char* argv[])
{
   int i;
   int msg_tag = 0;
   int taskid, ntasks;
   MPI_Status status;

   // parâmetros dos experimentos
   int forest_size = 30;
   int n_trials = 5000;
   int n_probs = 101;

   double* percent_burned; // percentuais queimados (saída)
   double* prob_spread;    // probabilidades (entrada)
   double prob_min = 0.0;
   double prob_max = 1.0;
   double prob_step;
   int base_seed = 100;

   int part_size;
   double partial;
   double mypart = 0.0;

   long start_time, end_time;

   checkCommandLine(argc, argv, forest_size, n_trials, n_probs);

   MPI_Init(&argc, &argv);
   MPI_Comm_rank(MPI_COMM_WORLD, &taskid);
   MPI_Comm_size(MPI_COMM_WORLD, &ntasks);

   try {

      if ((n_trials % ntasks) != 0) {
         if (taskid == MASTER)
            printf("Numero de experimentos deve ser multiplo do numero de processos!\n");
         MPI_Finalize();
         return EXIT_FAILURE;
      }

      Forest* forest = new Forest(forest_size);
      Random rand;

      prob_spread = new double[n_probs];
      percent_burned = new double[n_probs];

      prob_step = (prob_max - prob_min)/(double)(n_probs-1);

      if (taskid == MASTER)
      {
          printf("Probabilidade, Percentual Queimado\n");
          start_time = wtime();
      }

      part_size = n_trials/ntasks;

      // para cada probabilidade, calcula o percentual de árvores queimadas
      for (int ip = 0; ip < n_probs; ip++) {

         prob_spread[ip] = prob_min + (double) ip * prob_step;
         percent_burned[ip] = 0.0;
         rand.setSeed(base_seed+ip); // nova seqüência de números aleatórios

         mypart = 0.0;

         // executa vários experimentos
         for (int it = 0; it < part_size; it++) {
            // queima floresta até o fogo apagar
            forest->burnUntilOut(forest->centralTree(), prob_spread[ip], rand);
            mypart += forest->getPercentBurned();
         }

         if (taskid == MASTER)
         {
            for (i = 0; i < ntasks-1; i++)
            {
                MPI_Recv(&partial, 1, MPI_DOUBLE, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
                mypart += partial;
            }
            percent_burned[ip] = mypart;
             // calcula média dos percentuais de árvores queimadas
            percent_burned[ip] /= n_trials;
            // mostra resultado para esta probabilidade
            printf("%lf, %lf\n", prob_spread[ip], percent_burned[ip]);

            mypart = 0.0;
         }
         else
         {
            MPI_Send(&mypart, 1, MPI_DOUBLE, MASTER, msg_tag, MPI_COMM_WORLD);
         }

      }

      if (taskid == MASTER)
      {
          end_time = wtime();
          printf("Tempo de execucao = %ld usec\n", (long)(end_time - start_time));
      }

      delete[] prob_spread;
      delete[] percent_burned;
   }
   catch (std::bad_alloc)
   {
      std::cerr << "Erro: alocacao de memoria" << std::endl;
      return 1;
   }


   MPI_Finalize();
   return EXIT_SUCCESS;
}

