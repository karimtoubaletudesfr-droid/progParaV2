#ifndef ILE_HPP
#define ILE_HPP

#include <iosfwd>

#include "habitant.hpp"

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

void affichage_ile(struct ile* ile);
void liberer_ile(ile* une_ile);

#endif
