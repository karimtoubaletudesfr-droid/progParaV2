#include <mpi.h>

#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "ile_portable.hpp"
#include "pseudo_monde.hpp"
#include "round_robin.hpp"
#include "pont.hpp"

const int NUMBER_OF_ARGS_IS_NOT_CORRECT = 1;
const int ROOT_RANK_NOT_AVAILABLE = 2;
const int NUMBER_OF_YEARS_TO_SIMULATE_IS_NOT_POSITIVE = 3;
const int FILE_NOT_FOUND = 4;
const int MEMORY_ALLOCATION_ERROR = 5;

int main(int argc, char** argv) {
    MPI_Init(&argc, &argv);

    int rank, nprocs;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);

    if (argc != 4 && argc != 5) {
        if (rank == 0) {
            std::cerr << "Utilisation : ./monde_mpi <fichier> <root> <annees> [seed]\n";
        }
        MPI_Abort(MPI_COMM_WORLD, NUMBER_OF_ARGS_IS_NOT_CORRECT);
    }

    const char* fichier = argv[1];
    int root = std::atoi(argv[2]);
    int annees = std::atoi(argv[3]);

    if (root < 0 || root >= nprocs) {
        if (rank == 0) {
            std::cerr << "Le rang root est invalide.\n";
        }
        MPI_Abort(MPI_COMM_WORLD, ROOT_RANK_NOT_AVAILABLE);
    }

    if (annees <= 0) {
        if (rank == 0) {
            std::cerr << "Le nombre d'années doit être strictement positif.\n";
        }
        MPI_Abort(MPI_COMM_WORLD, NUMBER_OF_YEARS_TO_SIMULATE_IS_NOT_POSITIVE);
    }

    unsigned int seed_globale;
    ilePortable* envoi = nullptr;
    int * tailles = nullptr;
    int* deplacement = nullptr;

    int nb_ponts = 0;
    pont* ponts = nullptr;


    if(rank == root ){
         int nb_iles;
        std::ifstream entree(fichier);
        if(!entree){
            std::cerr << "Impossible d'ouvrire le fichier " <<fichier<<".\n";
            MPI_Abort(MPI_COMM_WORLD, FILE_NOT_FOUND);
            
        }
        entree>> nb_iles;

        ilePortable* temp1=(ilePortable*)malloc(nb_iles*sizeof(ilePortable));
        if(temp1==NULL){
            std::cerr<<"Erreur lors d'alocation memoir de envoi\n";
            MPI_Abort(MPI_COMM_WORLD, MEMORY_ALLOCATION_ERROR);
        }
        envoi=temp1;

        int* temp2 =(int*)malloc(nprocs*sizeof(int));
        if(temp2==NULL){
            std::cerr<<"Erreur lors d'alocation memoir de tailles";
            MPI_Abort(MPI_COMM_WORLD, MEMORY_ALLOCATION_ERROR);
        }
        tailles=temp2;


        temp2=(int*)malloc(nprocs*sizeof(int));
          if(temp2==NULL){
            std::cerr<<"Erreur lors d'alocation memoir de deplacement";
            MPI_Abort(MPI_COMM_WORLD, MEMORY_ALLOCATION_ERROR);
        }
        deplacement=temp2;


        int position=0;
        int nbrestant=nb_iles%nprocs;
        for(int p=0; p< nprocs;++p){


            int nb_local=nb_iles/nprocs;
            if(nbrestant>0){
                nb_local++;
                nbrestant--;
            }

            tailles[p]=nb_local*sizeof(ilePortable);


            deplacement[p]=position*sizeof(ilePortable);

            position+=nb_local;
        }


        std::unordered_map<int, int> rang_par_ile;
        rang_par_ile.reserve(nb_iles);
        for(int i = 0; i<nb_iles; i++){
            int destination= rang_round_robin(i,nprocs);
            int indice= (deplacement[destination]/sizeof(ilePortable))+i/nprocs;
            envoi[indice] = creer_ile_portable(entree);
            rang_par_ile[envoi[indice].num] = destination;
        }
        entree >> nb_ponts;
        ponts = (pont*) std::malloc(nb_ponts * sizeof(pont));

        if (nb_ponts > 0 && ponts == nullptr) {
            std::cerr << "Impossible d'allouer les ponts.\n";
            MPI_Abort(MPI_COMM_WORLD, MEMORY_ALLOCATION_ERROR);
        }
        for (int i = 0; i < nb_ponts; ++i) {
            entree >> ponts[i].ile_a >> ponts[i].ile_b;
        }

        std::vector<int> nb_ponts_par_rang(nprocs, 0);
        for (int i = 0; i < nb_ponts; ++i) {
            int rang_a = rang_par_ile[ponts[i].ile_a];
            int rang_b = rang_par_ile[ponts[i].ile_b];

            ++nb_ponts_par_rang[rang_a];
            if (rang_b != rang_a) {
                ++nb_ponts_par_rang[rang_b];
            }
        }
        if(argc==5){
            seed_globale=(unsigned)atoi(argv[4]);
        }else{
            seed_globale=(unsigned)(std::time(nullptr));
        }

    }

     

     
    MPI_Bcast(&seed_globale, 1, MPI_UNSIGNED, root, MPI_COMM_WORLD);
    int taille_local;
    MPI_Scatter(
        tailles,
        1,
        MPI_INT,
        &taille_local,
        1,
        MPI_INT,
        root,
        MPI_COMM_WORLD
    );


    ilePortable* temp3=(ilePortable*)malloc(taille_local) ;
    if(taille_local > 0 && temp3==NULL){
        std::cerr<<"je sui le proc num "<<rank<<" et j'ai pas pus alouer mes description\n";
        MPI_Abort(MPI_COMM_WORLD, MEMORY_ALLOCATION_ERROR);
    }

    ilePortable* mes_descriptions = temp3;

    MPI_Scatterv(
    envoi,
    tailles,
    deplacement,
    MPI_BYTE,
    mes_descriptions,
    taille_local,
    MPI_BYTE,
    root,
    MPI_COMM_WORLD
);

    std::free(envoi);
    std::free(tailles);
    std::free(deplacement);
    std::free(ponts);

    pseudoMonde monde_local = {};
    monde_local.nb_iles = taille_local / sizeof(ilePortable);
    monde_local.liste_iles = (ile**) std::malloc(monde_local.nb_iles * sizeof(ile*));
    if (monde_local.nb_iles > 0 && monde_local.liste_iles == nullptr) {
        std::cerr << "Impossible d'allouer les îles du rang " << rank << '\n';
        MPI_Abort(MPI_COMM_WORLD, MEMORY_ALLOCATION_ERROR);
    }
    for (int i = 0; i < monde_local.nb_iles; ++i) {
        monde_local.liste_iles[i] = convertir_ile(&mes_descriptions[i], inflation_initiale,
                                                seed_globale);
    }
    std::free(mes_descriptions);

    for (int i = 0; i < monde_local.nb_iles; ++i) {
        liberer_ile(monde_local.liste_iles[i]);
    }
    std::free(monde_local.liste_iles);

    MPI_Finalize();
    return EXIT_SUCCESS;
}
