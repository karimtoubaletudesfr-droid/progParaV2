#ifndef ILE_PORTABLE_HPP
#define ILE_PORTABLE_HPP

#include <iosfwd>

struct ilePortable {
    int num;
    char nom[10];
    int nb_hab;
};
ilePortable creer_ile_portable(std::istream& fichier);
#endif
