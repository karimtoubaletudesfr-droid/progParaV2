#include "round_robin.hpp"

int rang_round_robin(std::size_t indice_ile, int nombre_processus) {
    return indice_ile % nombre_processus;
}
