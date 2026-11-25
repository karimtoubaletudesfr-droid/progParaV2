#include <mpi.h>

#include <climits>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "fonction_simulation.hpp"
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
const int NB_CARACTERISTIQUES = 3;
const int NB_GROUPES = 3;

// Tags MPI utilisés dans remplir_ghosts pour distinguer les Gini des médianes.
const int GINI_GHOST = 100;
const int REVENU_MEDIAN_GHOST = 101;

// Tag MPI utilisé par traiter_conflits_ponts pour échanger les meilleurs lancers.
const int LANCER_PONT = 102;

double* calculer_gini_locaux(const pseudoMonde* monde, int* taille) {
    *taille = 2 * monde->nb_iles;
    double* ginis = (double*) std::malloc(*taille * sizeof(double));
    if (*taille > 0 && ginis == nullptr) {
        MPI_Abort(MPI_COMM_WORLD, MEMORY_ALLOCATION_ERROR);
    }

    // Chaque île occupe deux cases : son numéro, puis son Gini.
    // La taille compte les cases du tableau : deux cases par île.
    for (int i = 0; i < monde->nb_iles; ++i) {
        ile* une_ile = monde->liste_iles[i];
        une_ile->gini = calculer_gini(une_ile);
        ginis[2 * i] = une_ile->num;
        ginis[2 * i + 1] = une_ile->gini;
    }
    return ginis;
}

void remplir_ghosts(
    const double* donnees_locales, int taille_donnees,
    const liste_voisins* listes, int nb_ponts_locaux,
    ghost* ghosts, int nb_ghosts,
    const std::unordered_map<int, int>& indice_par_ghost,
    int nprocs, int tag_statistique) {
    if (nb_ghosts == 0) {
        return;
    }

    int* tailles_reception = (int*) std::calloc(nprocs, sizeof(int));
    double* reception = (double*) std::malloc(2 * nb_ghosts * sizeof(double));
    double* envoi = (double*) std::malloc(2 * nb_ponts_locaux * sizeof(double));
    MPI_Request* requetes = (MPI_Request*) std::malloc(
        2 * nprocs * sizeof(MPI_Request));

    if (tailles_reception == nullptr || reception == nullptr ||
        envoi == nullptr || requetes == nullptr) {
        MPI_Abort(MPI_COMM_WORLD, MEMORY_ALLOCATION_ERROR);
    }

    // Chaque ghost correspond à un numéro d'île et à une valeur à recevoir.
    for (int i = 0; i < nb_ghosts; ++i) {
        tailles_reception[ghosts[i].rang] += 2;
    }

    int nb_requetes = 0;
    int position_reception = 0;

    // Réserver une tranche du tampon par rang voisin et poster les réceptions d'abord.
    for (int rang = 0; rang < nprocs; ++rang) {
        int taille = tailles_reception[rang];
        if (taille > 0) {
            MPI_Irecv(reception + position_reception, taille, MPI_DOUBLE,
                      rang, tag_statistique, MPI_COMM_WORLD,
                      &requetes[nb_requetes++]);
            position_reception += taille;
        }
    }

    // Prochaine case libre dans le tableau d'envoi commun à tous les rangs.
    int position_envoi = 0;

    for (int rang = 0; rang < nprocs; ++rang) {
        // Une réception prévue indique qu'au moins un pont relie les deux rangs.
        if (tailles_reception[rang] > 0) {
            // Le message de ce rang commence à cette position.
            int debut_envoi = position_envoi;

            // Chaque île locale occupe deux cases dans le tableau.
            for (int i = 0; i < taille_donnees / 2; ++i) {
                // Chercher le premier voisin de cette île situé sur ce rang.
                int j = 0;
                while (j < listes[i].nombre &&
                       listes[i].voisins[j].rang != rang) {
                    ++j;
                }

                // Ajouter l'île une seule fois, même si elle a plusieurs
                // ponts vers ce rang.
                if (j < listes[i].nombre) {
                    envoi[position_envoi++] = donnees_locales[2 * i];
                    envoi[position_envoi++] = donnees_locales[2 * i + 1];
                }
            }

            // La différence donne le nombre de cases à envoyer à ce rang.
            // MPI_Waitall utilisera la requête après la boucle.
            MPI_Isend(envoi + debut_envoi, position_envoi - debut_envoi,
                      MPI_DOUBLE, rang, tag_statistique, MPI_COMM_WORLD,
                      &requetes[nb_requetes++]);
        }
    }

    // Les deux tampons doivent rester en place jusqu'à la fin de tous les échanges.
    MPI_Waitall(nb_requetes, requetes, MPI_STATUSES_IGNORE);

    // Retrouver chaque ghost par son numéro, indépendamment de l'ordre des messages.
    for (int i = 0; i < position_reception; i += 2) {
        int num = (int) reception[i];
        ghost* voisin_distant = &ghosts[indice_par_ghost.at(num)];
        float valeur = (float) reception[i + 1];

        if (tag_statistique == GINI_GHOST) {
            voisin_distant->gini = valeur;
        } else {
            voisin_distant->revenu_median = valeur;
        }
    }

    std::free(requetes);
    std::free(envoi);
    std::free(reception);
    std::free(tailles_reception);
}

int traiter_conflits_ponts(
    pont* ponts, int nb_ponts, int rang, const pseudoMonde* monde,
    liste_voisins* listes, std::mt19937* generateurs, const ghost* ghosts,
    const std::unordered_map<int, int>& indice_par_ile,
    const std::unordered_map<int, int>& indice_par_ghost) {
    int nb_ponts_conserves = 0;

    // Les médianes ont déjà été échangées. Les deux extrémités testent donc le même écart.
    for (int i = 0; i < nb_ponts; ++i) {
        const pont p = ponts[i];
        int indice_a = p.rang_a == rang ? indice_par_ile.at(p.ile_a) : -1;
        int indice_b = p.rang_b == rang ? indice_par_ile.at(p.ile_b) : -1;
        double mediane_a = indice_a >= 0
            ? monde->liste_iles[indice_a]->revenu_median
            : ghosts[indice_par_ghost.at(p.ile_a)].revenu_median;
        double mediane_b = indice_b >= 0
            ? monde->liste_iles[indice_b]->revenu_median
            : ghosts[indice_par_ghost.at(p.ile_b)].revenu_median;

        bool fermer = false;
        bool a_plus_riche = mediane_a > mediane_b;
        double plus_riche = a_plus_riche ? mediane_a : mediane_b;
        double moins_riche = a_plus_riche ? mediane_b : mediane_a;

        // L'écart est mesuré par rapport à la médiane de l'île la plus riche.
        if (mediane_a != mediane_b &&
            100.0 * (plus_riche - moins_riche) >= 5.0 * plus_riche) {
            int lancer_a = 0;
            int lancer_b = 0;
            if (indice_a >= 0) {
                lancer_a = lancer_de_n(a_plus_riche ? 4 : 2, generateurs[indice_a]);
            }
            if (indice_b >= 0) {
                lancer_b = lancer_de_n(a_plus_riche ? 2 : 4, generateurs[indice_b]);
            }

            // L'ordre commun des ponts permet d'apparier les MPI_Sendrecv successifs.
            // Pour un pont distant, chacun échange son lancer et prend la même décision.
            if (indice_a < 0) {
                MPI_Sendrecv(&lancer_b, 1, MPI_INT, p.rang_a, LANCER_PONT,
                             &lancer_a, 1, MPI_INT, p.rang_a, LANCER_PONT,
                             MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            } else if (indice_b < 0) {
                MPI_Sendrecv(&lancer_a, 1, MPI_INT, p.rang_b, LANCER_PONT,
                             &lancer_b, 1, MPI_INT, p.rang_b, LANCER_PONT,
                             MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }

            // L'île riche doit gagner strictement. En cas d'égalité, le pont reste.
            fermer = a_plus_riche ? lancer_a > lancer_b : lancer_b > lancer_a;
        }

        if (fermer) {
            retirer_pont(p.ile_a, p.ile_b,
                         indice_a >= 0 ? &listes[indice_a] : nullptr,
                         indice_b >= 0 ? &listes[indice_b] : nullptr);
        } else {
            // Garder l'ordre des ponts pour les prochains échanges.
            ponts[nb_ponts_conserves++] = p;
        }
    }
    return nb_ponts_conserves;
}

int retirer_ghosts_sans_pont(
    ghost* ghosts, int nb_ghosts, const liste_voisins* listes,
    int nb_iles, int rang, std::unordered_map<int, int>& indice_par_ghost) {
    if (nb_ghosts == 0) {
        return 0;
    }

    bool* encore_voisin = (bool*) std::calloc(nb_ghosts, sizeof(bool));
    if (encore_voisin == nullptr) {
        MPI_Abort(MPI_COMM_WORLD, MEMORY_ALLOCATION_ERROR);
    }

    // Un ghost reste utile si au moins une île locale a encore un pont vers lui.
    for (int i = 0; i < nb_iles; ++i) {
        for (int j = 0; j < listes[i].nombre; ++j) {
            const voisin& v = listes[i].voisins[j];
            if (v.rang != rang) {
                encore_voisin[indice_par_ghost.at(v.num)] = true;
            }
        }
    }

    int nb_conserves = 0;
    // Compacter le tableau et mettre à jour les indices des ghosts gardés.
    for (int i = 0; i < nb_ghosts; ++i) {
        int num = ghosts[i].num;
        if (encore_voisin[i]) {
            ghosts[nb_conserves] = ghosts[i];
            indice_par_ghost.at(num) = nb_conserves;
            ++nb_conserves;
        } else {
            indice_par_ghost.erase(num);
        }
    }
    std::free(encore_voisin);
    return nb_conserves;
}

double* calculer_caracteristiques_locales(const pseudoMonde* monde) {
    double* donnees = (double*) std::malloc(
        monde->nb_iles * NB_CARACTERISTIQUES * sizeof(double));

    if (monde->nb_iles > 0 && donnees == nullptr) {
        MPI_Abort(MPI_COMM_WORLD, MEMORY_ALLOCATION_ERROR);
    }

    // Préparer les trois caractéristiques de chaque île du rang.
    for (int i = 0; i < monde->nb_iles; ++i) {
        const ile* une_ile = monde->liste_iles[i];
        double somme_revenus = 0.0;
        double somme_ages = 0.0;

        // Additionner les revenus et les âges pour calculer les deux moyennes.
        for (int j = 0; j < une_ile->nb_hab; ++j) {
            const habitant* h = une_ile->liste_hab[j];
            somme_revenus += h->revenu;
            somme_ages += h->age;
        }

        // Chaque île occupe trois cases : Gini, revenu moyen, âge moyen.
        // Le Gini est recalculé avec les revenus observés à la fin de l'année.
        int debut = i * NB_CARACTERISTIQUES;
        donnees[debut] = calculer_gini(une_ile);
        donnees[debut + 1] = 0.0;
        donnees[debut + 2] = 0.0;

        if (une_ile->nb_hab > 0) {
            donnees[debut + 1] = somme_revenus / une_ile->nb_hab;
            donnees[debut + 2] = somme_ages / une_ile->nb_hab;
        }
    }

    return donnees;
}

void normaliser_caracteristiques(double* donnees, int nb_iles_locales) {
    const int case_effectif = 2 * NB_CARACTERISTIQUES;
    const int taille_totaux = case_effectif + 1;

    // Cases 0 à 2 : sommes. Cases 3 à 5 : sommes des carrés.
    // Dernière case : nombre d'îles.
    double totaux_locaux[taille_totaux] = {};
    double totaux_mondiaux[taille_totaux];
    totaux_locaux[case_effectif] = nb_iles_locales;

    // Parcourir les îles locales pour préparer leur contribution aux totaux mondiaux.
    for (int i = 0; i < nb_iles_locales; ++i) {
        // Accumuler séparément les sommes et les carrés de chaque colonne.
        for (int c = 0; c < NB_CARACTERISTIQUES; ++c) {
            double valeur = donnees[i * NB_CARACTERISTIQUES + c];
            totaux_locaux[c] += valeur;
            totaux_locaux[NB_CARACTERISTIQUES + c] += valeur * valeur;
        }
    }

    // Même un rang sans île participe avec des totaux locaux à zéro.
    MPI_Allreduce(totaux_locaux, totaux_mondiaux, taille_totaux,
                  MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

    double nb_iles = totaux_mondiaux[case_effectif];
    if (nb_iles == 0.0) {
        return;
    }

    // Calculer une moyenne et un écart-type mondiaux pour chaque colonne.
    for (int c = 0; c < NB_CARACTERISTIQUES; ++c) {
        double moyenne = totaux_mondiaux[c] / nb_iles;
        double moyenne_carres =
            totaux_mondiaux[NB_CARACTERISTIQUES + c] / nb_iles;
        double variance = moyenne_carres - moyenne * moyenne;

        // Une variance nulle ou négative après arrondi donne une colonne à zéro.
        double ecart_type = variance > 0.0 ? std::sqrt(variance) : 0.0;

        // Centrer puis réduire la valeur de chaque île locale dans cette colonne.
        for (int i = 0; i < nb_iles_locales; ++i) {
            int case_valeur = i * NB_CARACTERISTIQUES + c;
            if (ecart_type > 0.0) {
                donnees[case_valeur] =
                    (donnees[case_valeur] - moyenne) / ecart_type;
            } else {
                donnees[case_valeur] = 0.0;
            }
        }
    }
}

int initialiser_centres(const pseudoMonde* monde,
                        const double* caracteristiques,
                        double* centres, int rang) {
    int dernier_num = 0;

    // Choisir les centres un par un, par numéro d'île croissant.
    for (int groupe = 0; groupe < NB_GROUPES; ++groupe) {
        int candidat_local[2] = {INT_MAX, INT_MAX};
        int elu[2];
        int indice_local = -1;

        // Sur ce rang, chercher le plus petit numéro encore disponible.
        for (int i = 0; i < monde->nb_iles; ++i) {
            int num = monde->liste_iles[i]->num;
            if ((groupe == 0 || num > dernier_num) &&
                (indice_local == -1 || num < candidat_local[0])) {
                candidat_local[0] = num;
                candidat_local[1] = rang;
                indice_local = i;
            }
        }

        // Le premier entier est le numéro de l'île, le second est son rang.
        MPI_Allreduce(candidat_local, elu, 1, MPI_2INT, MPI_MINLOC,
                      MPI_COMM_WORLD);

        // Un monde de moins de trois îles fournit moins de trois centres.
        if (elu[1] == INT_MAX) {
            return groupe;
        }

        double* centre = centres + groupe * NB_CARACTERISTIQUES;
        if (rang == elu[1]) {
            // Le rang élu copie le Gini, le revenu moyen et l'âge moyen.
            for (int c = 0; c < NB_CARACTERISTIQUES; ++c) {
                centre[c] =
                    caracteristiques[indice_local * NB_CARACTERISTIQUES + c];
            }
        }

        // Tous les rangs auront les mêmes coordonnées pour ce centre.
        MPI_Bcast(centre, NB_CARACTERISTIQUES, MPI_DOUBLE, elu[1],
                  MPI_COMM_WORLD);
        dernier_num = elu[0];
    }

    return NB_GROUPES;
}

int* affecter_groupes_kmeans(const double* donnees, int nb_iles, double* centres,
                            int nb_centres) {
    int* groupes = (int*) std::malloc(nb_iles * sizeof(int));
    if (nb_iles > 0 && groupes == nullptr) {
        MPI_Abort(MPI_COMM_WORLD, MEMORY_ALLOCATION_ERROR);
    }
    if (nb_centres == 0) {
        return groupes;
    }

    // Au premier tour, les îles n'ont encore aucun groupe.
    for (int i = 0; i < nb_iles; ++i) {
        groupes[i] = -1;
    }

    // Chaque groupe a trois sommes et un effectif. La dernière case compte les changements.
    const int taille_groupe = NB_CARACTERISTIQUES + 1;
    const int case_changements = NB_GROUPES * taille_groupe;
    const int taille_totaux = case_changements + 1;
    double totaux_mondiaux[taille_totaux];

    do {
        double totaux_locaux[taille_totaux] = {};

        // Affecter les îles locales au centre le plus proche et préparer leurs sommes.
        for (int i = 0; i < nb_iles; ++i) {
            int groupe_proche = 0;
            double distance_min = HUGE_VAL;

            // Comparer les centres avec la distance euclidienne au carré.
            for (int groupe = 0; groupe < nb_centres; ++groupe) {
                double distance = 0.0;

                // Chaque caractéristique contribue par le carré de son écart au centre.
                for (int c = 0; c < NB_CARACTERISTIQUES; ++c) {
                    double ecart = donnees[i * NB_CARACTERISTIQUES + c]
                                 - centres[groupe * NB_CARACTERISTIQUES + c];
                    distance += ecart * ecart;
                }

                // À distance égale, garder le groupe précédent s'il est parmi les plus proches.
                if (distance < distance_min ||
                    (distance == distance_min && groupe == groupes[i])) {
                    distance_min = distance;
                    groupe_proche = groupe;
                }
            }

            if (groupe_proche != groupes[i]) {
                ++totaux_locaux[case_changements];
                groupes[i] = groupe_proche;
            }

            int debut = groupe_proche * taille_groupe;
            // Additionner les trois valeurs de l'île dans son groupe.
            for (int c = 0; c < NB_CARACTERISTIQUES; ++c) {
                totaux_locaux[debut + c] += donnees[i * NB_CARACTERISTIQUES + c];
            }
            ++totaux_locaux[debut + NB_CARACTERISTIQUES];
        }

        MPI_Allreduce(totaux_locaux, totaux_mondiaux, taille_totaux,
                      MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);

        // Tous les rangs recalculent les mêmes centres. Un groupe vide garde son centre.
        for (int groupe = 0; groupe < nb_centres; ++groupe) {
            int debut = groupe * taille_groupe;
            double effectif = totaux_mondiaux[debut + NB_CARACTERISTIQUES];
            if (effectif > 0.0) {
                // Diviser chaque somme par l'effectif mondial du groupe.
                for (int c = 0; c < NB_CARACTERISTIQUES; ++c) {
                    centres[groupe * NB_CARACTERISTIQUES + c] =
                        totaux_mondiaux[debut + c] / effectif;
                }
            }
        }
    } while (totaux_mondiaux[case_changements] > 0.0);

    return groupes;
}

void appliquer_categories_kmeans(const pseudoMonde* monde, int* groupes,
                                 const double* centres, int nb_centres) {
    int categories[NB_GROUPES] = {};

    // Classer les centres par Gini décroissant. Un Gini égal départage par numéro de groupe.
    for (int groupe = 0; groupe < nb_centres; ++groupe) {
        int position = 0;
        double gini = centres[groupe * NB_CARACTERISTIQUES];

        // Compter les centres classés devant celui de ce groupe.
        for (int autre = 0; autre < nb_centres; ++autre) {
            double autre_gini = centres[autre * NB_CARACTERISTIQUES];
            if (autre_gini > gini || (autre_gini == gini && autre < groupe)) {
                ++position;
            }
        }

        // Avec un seul centre, conserver l'inflation. Avec deux, utiliser les catégories extrêmes.
        if (nb_centres == 1) {
            categories[groupe] = 2;
        } else if (nb_centres == 2) {
            categories[groupe] = position == 0 ? 1 : 3;
        } else {
            categories[groupe] = position + 1;
        }
    }

    const float variations[NB_GROUPES] = {-0.5f, 0.0f, 0.5f};
    // Remplacer le numéro interne du groupe par sa catégorie et ajuster l'inflation.
    for (int i = 0; i < monde->nb_iles; ++i) {
        int categorie = categories[groupes[i]];
        groupes[i] = categorie;
        monde->liste_iles[i]->inflation += variations[categorie - 1];
    }
}

void verifier_fichier_kmeans(int code, int rang) {
    if (code != MPI_SUCCESS) {
        char message[MPI_MAX_ERROR_STRING];
        int longueur;
        MPI_Error_string(code, message, &longueur);
        std::cerr << "Erreur pour resKmeans sur le rang " << rang << " : "
                  << message << '\n';
        MPI_Abort(MPI_COMM_WORLD, code);
    }
}

void ecrire_resultats_kmeans(MPI_File fichier, int annee, const pseudoMonde* monde,
                             const int* categories, int rang) {
    const int taille_ligne = 32;
    if (monde->nb_iles > (INT_MAX - 1) / taille_ligne) {
        MPI_Abort(MPI_COMM_WORLD, MEMORY_ALLOCATION_ERROR);
    }
    char* lignes = (char*) std::malloc(monde->nb_iles * taille_ligne + 1);
    if (lignes == nullptr) {
        MPI_Abort(MPI_COMM_WORLD, MEMORY_ALLOCATION_ERROR);
    }

    int longueur = 0;
    // Préparer une ligne CSV pour chaque île locale avant l'écriture collective.
    for (int i = 0; i < monde->nb_iles; ++i) {
        longueur += std::snprintf(lignes + longueur, taille_ligne, "%d,%d,%d\n",
                                  annee, monde->liste_iles[i]->num, categories[i]);
    }

    // MPI écrit les lignes rang par rang au même emplacement partagé dans le fichier.
    verifier_fichier_kmeans(MPI_File_write_ordered(fichier, lignes, longueur,
                               MPI_CHAR, MPI_STATUS_IGNORE), rang);
    std::free(lignes);
}

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


    // Seul le root lit le fichier. Il prépare un bloc d'îles et un bloc de ponts par rang.
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
        // Le round robin donne une île de plus aux premiers rangs s'il y a un reste.
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
        // Ranger chaque description dans le bloc de destination et noter son rang par numéro.
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

        // Compter chaque pont sur les rangs de ses deux extrémités, une fois par rang.
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

        // Remplir les blocs dans l'ordre du fichier, conservé lors des conflits.
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
    // Les Scatter donnent les tailles en octets avant l'allocation des tampons de réception.
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

    // Après les Scatterv, chaque rang travaille avec ses descriptions et ses ponts reçus.
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
    std::vector<std::mt19937> generateurs(monde_local.nb_iles);
    // Garder le même générateur pour créer les habitants et faire évoluer l'île ensuite.
    for (int i = 0; i < monde_local.nb_iles; ++i) {
        unsigned int num = mes_descriptions[i].num;
        generateurs[i].seed(seed_globale + num);

        monde_local.liste_iles[i] = convertir_ile(
            &mes_descriptions[i], inflation_initiale, generateurs[i]);
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

    // Le premier passage compte les voisins et fixe la taille des listes.
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
        listes[i].nombre = 0;  // Sert maintenant d'indice de remplissage.
    }

    // Le deuxième passage ajoute chaque voisin à la liste de son île locale.
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
    // Une entrée par voisin distant donne une capacité suffisante avant dédoublonnage.
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

    // Plusieurs îles du rang peuvent partager le même ghost.
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

    MPI_File fichier_kmeans;
    const char nom_resultats[] = "resKmeans";
    verifier_fichier_kmeans(MPI_File_open(MPI_COMM_WORLD, nom_resultats,
                               MPI_MODE_CREATE | MPI_MODE_WRONLY,
                               MPI_INFO_NULL, &fichier_kmeans), rank);
    // Une nouvelle simulation repart d'un fichier vide et écrit un seul en-tête.
    verifier_fichier_kmeans(MPI_File_set_size(fichier_kmeans, 0), rank);
    char entete[] = "annee,ile,categorie\n";
    int taille_entete = rank == root ? sizeof(entete) - 1 : 0;
    verifier_fichier_kmeans(MPI_File_write_ordered(fichier_kmeans, entete,
                               taille_entete, MPI_CHAR, MPI_STATUS_IGNORE), rank);

    for (int annee = 0; annee < annees; ++annee) {
        // Niveau 1. Vieillir les habitants et donner un premier revenu aux nouveaux majeurs.
        for (int i = 0; i < monde_local.nb_iles; ++i) {
            ile* une_ile = monde_local.liste_iles[i];
            int nb_nouveaux_majeurs;
            habitant** nouveaux_majeurs =
                vieillir_tout_le_monde(une_ile, &nb_nouveaux_majeurs);

            if (nb_nouveaux_majeurs > 0 && nouveaux_majeurs == nullptr) {
                MPI_Abort(MPI_COMM_WORLD, MEMORY_ALLOCATION_ERROR);
            }

            donner_premier_revenu(nouveaux_majeurs, nb_nouveaux_majeurs,
                                  generateurs[i]);
            std::free(nouveaux_majeurs);
        }

        // Le niveau 2 compare ces Gini avant l'indexation et les remplacements.
        int taille_donnees;
        double* donnees_locales = calculer_gini_locaux(&monde_local, &taille_donnees);
        remplir_ghosts(donnees_locales, taille_donnees, listes, nb_ponts_locaux,
                       ghosts, nb_ghosts, indice_par_ghost, nprocs, GINI_GHOST);

        // Terminer les règles locales, puis appliquer la hausse liée aux voisins.
        for (int i = 0; i < monde_local.nb_iles; ++i) {
            ile* une_ile = monde_local.liste_iles[i];
            indexer_revenus(une_ile);
            vie_et_mort(une_ile, generateurs[i]);

            bool voisin_plus_inegal = false;
            int j = 0;
            // Un seul voisin avec un Gini supérieur suffit pour déclencher la hausse.
            while (j < listes[i].nombre && !voisin_plus_inegal) {
                const voisin& v = listes[i].voisins[j];
                float gini_voisin;

                // Les Gini distants viennent des ghosts remplis pour cette année.
                if (v.rang == rank) {
                    gini_voisin = monde_local.liste_iles[indice_par_ile.at(v.num)]->gini;
                } else {
                    gini_voisin = ghosts[indice_par_ghost.at(v.num)].gini;
                }

                voisin_plus_inegal = gini_voisin > une_ile->gini;
                ++j;
            }

            if (voisin_plus_inegal) {
                hausse_n_pour_cent_pour_moins_de_t_ans(une_ile, 5.0f, 25);
            }
        }

        // Tous les cinq ans, on prend les revenus après les hausses de l'année.
        if ((annee + 1) % 5 == 0) {
            // Calculer une médiane par île avant de les échanger avec les voisins distants.
            for (int i = 0; i < monde_local.nb_iles; ++i) {
                ile* une_ile = monde_local.liste_iles[i];
                une_ile->revenu_median = revenue_median(une_ile);

                // Le numéro reste dans la première case, la médiane remplace le Gini.
                donnees_locales[2 * i + 1] = une_ile->revenu_median;
            }
            remplir_ghosts(donnees_locales, taille_donnees, listes, nb_ponts_locaux,
                           ghosts, nb_ghosts, indice_par_ghost, nprocs,
                           REVENU_MEDIAN_GHOST);

            int nb_ponts_conserves = traiter_conflits_ponts(
                mes_ponts, nb_ponts_locaux, rank, &monde_local, listes,
                generateurs.data(), ghosts, indice_par_ile, indice_par_ghost);

            // L'échange suivant doit attendre uniquement les îles encore reliées.
            if (nb_ponts_conserves < nb_ponts_locaux) {
                nb_ghosts = retirer_ghosts_sans_pont(
                    ghosts, nb_ghosts, listes, monde_local.nb_iles, rank, indice_par_ghost);
            }
            nb_ponts_locaux = nb_ponts_conserves;
        }
        std::free(donnees_locales);

        // L'enquête a lieu à la fin des années 10, 20, 30, etc.
        if ((annee + 1) % 10 == 0) {
            double* caracteristiques = calculer_caracteristiques_locales(&monde_local);
            normaliser_caracteristiques(caracteristiques, monde_local.nb_iles);

            double centres[NB_GROUPES * NB_CARACTERISTIQUES];
            int nb_centres = initialiser_centres(
                &monde_local, caracteristiques, centres, rank);
            int* groupes = affecter_groupes_kmeans(
                caracteristiques, monde_local.nb_iles, centres, nb_centres);
            appliquer_categories_kmeans(&monde_local, groupes, centres, nb_centres);
            ecrire_resultats_kmeans(fichier_kmeans, annee + 1, &monde_local,
                                    groupes, rank);
            std::free(groupes);
            std::free(caracteristiques);
        }
    }

    verifier_fichier_kmeans(MPI_File_close(&fichier_kmeans), rank);

    // Chaque rang libère les habitants, les îles puis ses structures de voisinage.
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
