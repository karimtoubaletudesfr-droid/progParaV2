
#include "ile_portable.hpp"
#include <cstring>
#include <iostream>

ilePortable creer_ile_portable(std::istream& fichier) {
    int num;
    char nom[10];
    int nb_hab;

    fichier >> num >> nom >> nb_hab;

    // Le rang propriétaire créera ses habitants à partir de cette description.
    ilePortable nouvelle_ile = {};
    nouvelle_ile.num = num;
    std::strcpy(nouvelle_ile.nom, nom);
    nouvelle_ile.nb_hab = nb_hab;

    return nouvelle_ile;

}
