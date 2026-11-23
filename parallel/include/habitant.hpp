#ifndef HABITANT_HPP
#define HABITANT_HPP

#include <random>

#define revenu_min 1.0
#define revenu_max 10.0

struct habitant {
    char nom[10];
    int age;
    float revenu;
};

habitant* creation_habitant(const char* nom, int suffixe, std::mt19937* generateur = nullptr);
void affichage_habitant(struct habitant* h);
void liberer_habitant(habitant* h);

#endif
