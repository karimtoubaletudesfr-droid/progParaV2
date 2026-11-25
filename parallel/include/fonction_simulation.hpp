#ifndef FONCTION_SIMULATION_HPP
#define FONCTION_SIMULATION_HPP

#include <random>

#include "ile.hpp"
#include "voisin.hpp"

double calculer_gini(const ile* une_ile);
float revenue_median(const ile* une_ile);
habitant** vieillir_tout_le_monde(ile* une_ile, int* nb_nouveaux_majeurs);
void donner_premier_revenu(habitant** nouveaux_majeurs, int nombre,
                           std::mt19937& generateur);
void indexer_revenus(ile* une_ile);
void hausse_n_pour_cent_pour_moins_de_t_ans(ile* une_ile, float n, int t);
void vie_et_mort(ile* une_ile, std::mt19937& generateur);
int lancer_de(std::mt19937& generateur);
int lancer_de_n(int nombre_lancers, std::mt19937& generateur);
// Une liste peut être nulle si son île est sur un autre rang.
void retirer_pont(int num_ile_a, int num_ile_b,
                  liste_voisins* voisins_a, liste_voisins* voisins_b);

#endif
