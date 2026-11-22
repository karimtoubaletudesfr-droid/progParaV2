#include "ile.hpp"

#include <cstdlib>
#include <iostream>

ile* creer_ile(std::istream& fichier) {
    auto* nouvelle_ile = static_cast<ile*>(std::malloc(sizeof(ile)));
    fichier >> nouvelle_ile->num >> nouvelle_ile->nom >> nouvelle_ile->nb_hab;

    nouvelle_ile->liste_hab = static_cast<habitant**>(
        std::malloc(nouvelle_ile->nb_hab * sizeof(habitant*)));
    for (int i = 0; i < nouvelle_ile->nb_hab; ++i) {
        nouvelle_ile->liste_hab[i] = creation_habitant(nouvelle_ile->nom, i);
    }

    return nouvelle_ile;
}

void affichage_ile(struct ile* ile) {
    std::cout << ile->nom << "->" << ile->nb_hab << " habitants : ";
    for (int i=0; i<ile->nb_hab; i++) {
        affichage_habitant(ile->liste_hab[i]);
        std::cout << std::endl;
    }
}

void liberer_ile(ile* une_ile) {
    for (int i = 0; i < une_ile->nb_hab; ++i) {
        liberer_habitant(une_ile->liste_hab[i]);
    }
    std::free(une_ile->liste_hab);
    std::free(une_ile);
}
