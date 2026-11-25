#include "habitant.hpp"

#include <cstdio>
#include <cstdlib>
#include <iostream>

habitant* creation_habitant(const char* nom, int suffixe, std::mt19937* generateur) {
    struct habitant* h = (struct habitant*) malloc(sizeof(struct habitant));
    std::snprintf(h->nom, sizeof(h->nom), "%d%s", suffixe, nom);
    // La version MPI utilise le générateur de l'île. L'entrée séquentielle utilise rand.
    if (generateur) {
        std::uniform_int_distribution<int> age(0, 49);
        std::uniform_real_distribution<float> revenu(revenu_min, revenu_max);
        h->age = age(*generateur);
        h->revenu = h->age < 18 ? 0.0f : revenu(*generateur);
    } else {
        h->age = rand()%50;
        h->revenu = revenu_min + ((float)rand()/(float)RAND_MAX)*(float)(revenu_max-revenu_min);
    }
    return h;
}

void affichage_habitant(struct habitant* h) {
    std::cout << h->nom << " \t" << h->age << " ans \t(" << h->revenu << "€)"; 
}

void liberer_habitant(habitant* h) {
    std::free(h);
}
