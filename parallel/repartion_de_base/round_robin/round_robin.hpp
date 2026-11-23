#ifndef ROUND_ROBIN_HPP
#define ROUND_ROBIN_HPP

#include <cstddef>

// Les indices commencent à 0. Le nombre de processus doit être positif.
int rang_round_robin(std::size_t indice_ile, int nombre_processus);

#endif
