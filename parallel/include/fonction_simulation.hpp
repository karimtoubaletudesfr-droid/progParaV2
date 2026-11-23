#ifndef FONCTION_SIMULATION_HPP
#define FONCTION_SIMULATION_HPP

#include <random>

#include "ile.hpp"

double calculer_gini(const ile* une_ile);
habitant** vieillir_tout_le_monde(ile* une_ile, int* nb_nouveaux_majeurs);
void donner_premier_revenu(habitant** nouveaux_majeurs, int nombre,
                           std::mt19937& generateur);
void indexer_revenus(ile* une_ile);
void vie_et_mort(ile* une_ile, std::mt19937& generateur);

#endif
