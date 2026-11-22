#ifndef MONDE_HPP
#define MONDE_HPP

#include <stdlib.h>
#include <fstream>
#include <iostream>
#include <string.h>

using namespace std;

#define inflation_initiale 2.5
#define revenu_min 1.0
#define revenu_max 10.0

struct habitant {
    char nom[10];
    int age;
    float revenu;
};

struct ile {
    int num;
    char nom[10];
    int nb_hab;
    struct habitant** liste_hab;
    float gini;
    int surface;
    float inflation;
};

struct monde {
    struct ile** mes_iles;
    int nb_iles;
    int* mat_adj;
    float inflation;
};

struct habitant* creation_habitant(char* nom, int suffixe);

struct monde* generation(const char* filename);

void affichage_monde(struct monde* m);

void affichage_ile(struct ile* ile);

void affichage_habitant(struct habitant* h);

void liberer_monde(struct monde** m);

#endif