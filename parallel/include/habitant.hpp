#ifndef HABITANT_HPP
#define HABITANT_HPP

#define revenu_min 1.0
#define revenu_max 10.0

struct habitant {
    char nom[10];
    int age;
    float revenu;
};

struct habitant* creation_habitant(char* nom, int suffixe);
void affichage_habitant(struct habitant* h);
void liberer_habitant(habitant* h);

#endif
