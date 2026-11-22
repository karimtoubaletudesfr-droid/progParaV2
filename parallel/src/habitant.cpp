#include "habitant.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>

struct habitant* creation_habitant(char* nom, int suffixe) {
    struct habitant* h = (struct habitant*) malloc(sizeof(struct habitant));
    h->age = rand()%50;
    sprintf(h->nom, "%d", suffixe);
    strcat(h->nom,nom);
    h->revenu = revenu_min + ((float)rand()/(float)RAND_MAX)*(float)(revenu_max-revenu_min);
    return h;
}

void affichage_habitant(struct habitant* h) {
    std::cout << h->nom << " \t" << h->age << " ans \t(" << h->revenu << "€)"; 
}

void liberer_habitant(habitant* h) {
    std::free(h);
}
