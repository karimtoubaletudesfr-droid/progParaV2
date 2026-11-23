#include <mpi.h>

#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <unordered_map>

#include "ile_portable.hpp"
#include "pseudo_monde.hpp"
#include "round_robin.hpp"
#include "pont.hpp"
#include "voisin.hpp"
#include "ghost.hpp"

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
    pont* envoi_ponts = nullptr;
    int* tailles_ponts = nullptr;
    int* deplacements_ponts = nullptr;


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

        int* nb_ponts_par_rang = (int*) std::calloc(nprocs, sizeof(int));
        int* prochaine_case = (int*) std::malloc(nprocs * sizeof(int));
        tailles_ponts = (int*) std::malloc(nprocs * sizeof(int));
        deplacements_ponts = (int*) std::malloc(nprocs * sizeof(int));

        if (!nb_ponts_par_rang || !prochaine_case || !tailles_ponts || !deplacements_ponts) {
            std::cerr << "Impossible d'allouer les tableaux pour les ponts.\n";
            MPI_Abort(MPI_COMM_WORLD, MEMORY_ALLOCATION_ERROR);
        }

        for (int i = 0; i < nb_ponts; ++i) {
            ponts[i].rang_a = rang_par_ile[ponts[i].ile_a];
            ponts[i].rang_b = rang_par_ile[ponts[i].ile_b];

            ++nb_ponts_par_rang[ponts[i].rang_a];
            if (ponts[i].rang_b != ponts[i].rang_a) {
                ++nb_ponts_par_rang[ponts[i].rang_b];
            }
        }

        int position_pont = 0;
        for (int p = 0; p < nprocs; ++p) {
            prochaine_case[p] = position_pont;
            deplacements_ponts[p] = position_pont * sizeof(pont);
            tailles_ponts[p] = nb_ponts_par_rang[p] * sizeof(pont);
            position_pont += nb_ponts_par_rang[p];
        }

        envoi_ponts = (pont*) std::malloc(position_pont * sizeof(pont));
        if (position_pont > 0 && !envoi_ponts) {
            std::cerr << "Impossible d'allouer le tampon d'envoi des ponts.\n";
            MPI_Abort(MPI_COMM_WORLD, MEMORY_ALLOCATION_ERROR);
        }

        for (int i = 0; i < nb_ponts; ++i) {
            int rang_a = ponts[i].rang_a;
            int rang_b = ponts[i].rang_b;

            envoi_ponts[prochaine_case[rang_a]] = ponts[i];
            ++prochaine_case[rang_a];

            if (rang_b != rang_a) {
                envoi_ponts[prochaine_case[rang_b]] = ponts[i];
                ++prochaine_case[rang_b];
            }
        }

        std::free(nb_ponts_par_rang);
        std::free(prochaine_case);
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

    int taille_ponts_locale;
    MPI_Scatter(tailles_ponts, 1, MPI_INT,
                &taille_ponts_locale, 1, MPI_INT,
                root, MPI_COMM_WORLD);

    pont* mes_ponts = (pont*) std::malloc(taille_ponts_locale);
    // Un rang sans pont peut recevoir nullptr de malloc(0).
    if (taille_ponts_locale > 0 && mes_ponts == nullptr) {
        std::cerr << "Impossible d'allouer les ponts du rang " << rank << '\n';
        MPI_Abort(MPI_COMM_WORLD, MEMORY_ALLOCATION_ERROR);
    }

    MPI_Scatterv(envoi_ponts, tailles_ponts, deplacements_ponts, MPI_BYTE,
                 mes_ponts, taille_ponts_locale, MPI_BYTE,
                 root, MPI_COMM_WORLD);

    int nb_ponts_locaux = taille_ponts_locale / sizeof(pont);
    std::cout << "Rang " << rank << " : " << nb_ponts_locaux << " ponts\n";

    std::free(envoi);
    std::free(tailles);
    std::free(deplacement);
    std::free(ponts);
    std::free(envoi_ponts);
    std::free(tailles_ponts);
    std::free(deplacements_ponts);

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

    std::unordered_map<int, int> indice_par_ile;
    for (int i = 0; i < monde_local.nb_iles; ++i) {
        indice_par_ile[monde_local.liste_iles[i]->num] = i;
    }

    liste_voisins* listes = (liste_voisins*) std::calloc(
        monde_local.nb_iles, sizeof(liste_voisins));

    if (monde_local.nb_iles > 0 && listes == nullptr) {
        MPI_Abort(MPI_COMM_WORLD, MEMORY_ALLOCATION_ERROR);
    }

    for (int i = 0; i < nb_ponts_locaux; ++i) {
        const pont& p = mes_ponts[i];

        if (p.rang_a == rank) {
            ++listes[indice_par_ile[p.ile_a]].nombre;
        }
        if (p.rang_b == rank && p.ile_b != p.ile_a) {
            ++listes[indice_par_ile[p.ile_b]].nombre;
        }
    }

    for (int i = 0; i < monde_local.nb_iles; ++i) {
        int nombre = listes[i].nombre;
        listes[i].voisins = (voisin*) std::malloc(nombre * sizeof(voisin));
        if (nombre > 0 && listes[i].voisins == nullptr) {
            MPI_Abort(MPI_COMM_WORLD, MEMORY_ALLOCATION_ERROR);
        }
        listes[i].nombre = 0;
    }

    for (int i = 0; i < nb_ponts_locaux; ++i) {
        const pont& p = mes_ponts[i];

        if (p.rang_a == rank) {
            liste_voisins& liste = listes[indice_par_ile[p.ile_a]];
            liste.voisins[liste.nombre++] = {p.ile_b, p.rang_b};
        }
        if (p.rang_b == rank && p.ile_b != p.ile_a) {
            liste_voisins& liste = listes[indice_par_ile[p.ile_b]];
            liste.voisins[liste.nombre++] = {p.ile_a, p.rang_a};
        }
    }

    int capacite_ghosts = 0;
    for (int i = 0; i < monde_local.nb_iles; ++i) {
        for (int j = 0; j < listes[i].nombre; ++j) {
            if (listes[i].voisins[j].rang != rank) {
                ++capacite_ghosts;
            }
        }
    }

    ghost* ghosts = (ghost*) std::malloc(capacite_ghosts * sizeof(ghost));
    if (capacite_ghosts > 0 && ghosts == nullptr) {
        std::cerr << "Impossible d'allouer les ghosts du rang " << rank << '\n';
        MPI_Abort(MPI_COMM_WORLD, MEMORY_ALLOCATION_ERROR);
    }

    int nb_ghosts = 0;
    std::unordered_map<int, int> indice_par_ghost;

    for (int i = 0; i < monde_local.nb_iles; ++i) {
        for (int j = 0; j < listes[i].nombre; ++j) {
            const voisin& v = listes[i].voisins[j];

            if (v.rang == rank) {
                continue;
            }

            if (indice_par_ghost.emplace(v.num, nb_ghosts).second) {
                ghosts[nb_ghosts] = {v.num, v.rang, 0.0f, 0.0f};
                ++nb_ghosts;
            }
        }
    }

    for (int i = 0; i < monde_local.nb_iles; ++i) {
        liberer_ile(monde_local.liste_iles[i]);
    }
    std::free(monde_local.liste_iles);
    std::free(mes_ponts);

    for (int i = 0; i < monde_local.nb_iles; ++i) {
        std::free(listes[i].voisins);
    }
    std::free(listes);
    std::free(ghosts);

    MPI_Finalize();
    return EXIT_SUCCESS;
}
