#include "ile.hpp"

#include <cstdlib>
#include <cstring>
#include <iostream>

ile* creer_ile(std::istream& fichier) {
    int num;
    char nom[10];
    int nb_hab;
    fichier >> num >> nom >> nb_hab;
    return creer_ile(num, nom, nb_hab, 0.0f);
}

ile* creer_ile(int num, const char* nom, int nb_hab, float inflation,
               std::mt19937* generateur) {
    ile* nouvelle_ile = (ile*) std::malloc(sizeof(ile));
    *nouvelle_ile = {};
    nouvelle_ile->num = num;
    std::strcpy(nouvelle_ile->nom, nom);
    nouvelle_ile->nb_hab = nb_hab;
    nouvelle_ile->inflation = inflation;
    nouvelle_ile->liste_hab = (habitant**) std::malloc(nb_hab * sizeof(habitant*));
    for (int i = 0; i < nouvelle_ile->nb_hab; ++i) {
        nouvelle_ile->liste_hab[i] = creation_habitant(nouvelle_ile->nom, i, generateur);
    }

    return nouvelle_ile;
}

ile* convertir_ile(const ilePortable* description, float inflation,
                   std::mt19937& generateur) {
    // Construire la population sur le rang qui a reçu la description.
    return creer_ile(description->num, description->nom, description->nb_hab,
                     inflation, &generateur);
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
