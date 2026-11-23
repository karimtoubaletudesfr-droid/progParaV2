#include "fonction_simulation.hpp"

#include <cmath>
#include <cstdlib>

double calculer_gini(const ile* une_ile) {
    int n = une_ile->nb_hab;
    double somme_revenus = 0.0;

    for (int i = 0; i < n; ++i) {
        somme_revenus += une_ile->liste_hab[i]->revenu;
    }

    if (somme_revenus == 0.0) {
        return 0.0;
    }

    double somme_ecarts = 0.0;
    for (int i = 0; i < n; ++i) {
        double revenu_i = une_ile->liste_hab[i]->revenu;
        for (int j = 0; j < n; ++j) {
            double revenu_j = une_ile->liste_hab[j]->revenu;
            somme_ecarts += std::abs(revenu_i - revenu_j);
        }
    }

    return somme_ecarts / (2.0 * n * somme_revenus);
}

habitant** vieillir_tout_le_monde(ile* une_ile, int* nb_nouveaux_majeurs) {
    *nb_nouveaux_majeurs = 0;

    for (int i = 0; i < une_ile->nb_hab; ++i) {
        habitant* h = une_ile->liste_hab[i];
        ++h->age;
        if (h->age == 18) {
            ++(*nb_nouveaux_majeurs);
        }
    }

    if (*nb_nouveaux_majeurs == 0) {
        return nullptr;
    }

    habitant** nouveaux_majeurs = (habitant**) std::malloc(
        *nb_nouveaux_majeurs * sizeof(habitant*));
    if (nouveaux_majeurs == nullptr) {
        return nullptr;
    }

    int position = 0;
    for (int i = 0; i < une_ile->nb_hab; ++i) {
        habitant* h = une_ile->liste_hab[i];
        if (h->age == 18) {
            nouveaux_majeurs[position] = h;
            ++position;
        }
    }

    return nouveaux_majeurs;
}

void donner_premier_revenu(habitant** nouveaux_majeurs, int nombre,
                           std::mt19937& generateur) {
    std::uniform_real_distribution<float> revenu(revenu_min, revenu_max);
    for (int i = 0; i < nombre; ++i) {
        nouveaux_majeurs[i]->revenu = revenu(generateur);
    }
}

void indexer_revenus(ile* une_ile) {
    float facteur = 1.0f + une_ile->inflation / 100.0f;
    for (int i = 0; i < une_ile->nb_hab; ++i) {
        habitant* h = une_ile->liste_hab[i];
        if (h->age >= 18) {
            h->revenu *= facteur;
        }
    }
}

void vie_et_mort(ile* une_ile, std::mt19937& generateur) {
    int maximum_deces = une_ile->nb_hab / 20;
    std::uniform_int_distribution<int> nombre_deces(0, maximum_deces);
    int deces_restants = nombre_deces(generateur);

    for (int i = 0; i < une_ile->nb_hab && deces_restants > 0; ++i) {
        int habitants_restants = une_ile->nb_hab - i;
        std::uniform_int_distribution<int> tirage(1, habitants_restants);

        if (tirage(generateur) <= deces_restants) {
            habitant* nouveau_ne = une_ile->liste_hab[i];
            nouveau_ne->age = 0;
            nouveau_ne->revenu = 0.0f;
            --deces_restants;
        }
    }
}
