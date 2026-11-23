#ifndef ILE_HPP
#define ILE_HPP

#include <iosfwd>
#include <random>

#include"ile_portable.hpp"

#include "habitant.hpp"
#define inflation_initiale 2.5


struct ile {
    int num;
    char nom[10];
    int nb_hab;
    struct habitant** liste_hab;
    float gini;
    int surface;
    float inflation;
};

ile* creer_ile(std::istream& fichier);
ile* creer_ile(int num, const char* nom, int nb_hab, float inflation,
               std::mt19937* generateur = nullptr);
ile* convertir_ile(const ilePortable* description, float inflation, unsigned int seed_globale);
void affichage_ile(struct ile* ile);
void liberer_ile(ile* une_ile);

#endif
