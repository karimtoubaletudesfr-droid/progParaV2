#include "fonction_simulation.hpp"

#include <algorithm>
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

    // La formule de Gini compte les écarts pour tous les couples ordonnés d'habitants.
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

int comparer_revenus(const void* premier, const void* second) {
    float a = *(const float*) premier;
    float b = *(const float*) second;
    return (a > b) - (a < b);
}

float revenue_median(const ile* une_ile) {
    int nombre = une_ile->nb_hab;
    if (nombre == 0) {
        return 0.0f;
    }

    float* revenus = (float*) std::malloc(nombre * sizeof(float));
    if (revenus == nullptr) {
        std::abort();
    }

    // Trier une copie des revenus pour conserver l'ordre des habitants.
    for (int i = 0; i < nombre; ++i) {
        revenus[i] = une_ile->liste_hab[i]->revenu;
    }
    std::qsort(revenus, nombre, sizeof(float), comparer_revenus);

    int milieu = nombre / 2;
    float mediane = revenus[milieu];
    // Avec un effectif pair, la médiane est la moyenne des deux revenus centraux.
    if (nombre % 2 == 0) {
        mediane = ((double) revenus[milieu - 1] + revenus[milieu]) / 2.0;
    }

    std::free(revenus);
    return mediane;
}

habitant** vieillir_tout_le_monde(ile* une_ile, int* nb_nouveaux_majeurs) {
    *nb_nouveaux_majeurs = 0;

    // Compter les nouveaux majeurs pendant le vieillissement pour allouer la bonne taille.
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

    // Référencer les habitants existants. L'appelant libère seulement le tableau de pointeurs.
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

void hausse_n_pour_cent_pour_moins_de_t_ans(ile* une_ile, float n, int t) {
    float facteur = 1.0f + n / 100.0f;
    for (int i = 0; i < une_ile->nb_hab; ++i) {
        habitant* h = une_ile->liste_hab[i];
        if (h->age < t) {
            h->revenu *= facteur;
        }
    }
}

void vie_et_mort(ile* une_ile, std::mt19937& generateur) {
    // La division entière fixe le maximum à 5 % de la population, arrondi à l'inférieur.
    int maximum_deces = une_ile->nb_hab / 20;
    std::uniform_int_distribution<int> nombre_deces(0, maximum_deces);
    int deces_restants = nombre_deces(generateur);

    // Choisir les personnes à remplacer sans repasser sur un habitant déjà traité.
    for (int i = 0; i < une_ile->nb_hab && deces_restants > 0; ++i) {
        int habitants_restants = une_ile->nb_hab - i;
        std::uniform_int_distribution<int> tirage(1, habitants_restants);

        if (tirage(generateur) <= deces_restants) {
            // Le nouveau-né reprend l'emplacement de la personne remplacée.
            habitant* nouveau_ne = une_ile->liste_hab[i];
            nouveau_ne->age = 0;
            nouveau_ne->revenu = 0.0f;
            --deces_restants;
        }
    }
}

int lancer_de(std::mt19937& generateur) {
    std::uniform_int_distribution<int> de(1, 6);
    return de(generateur);
}

int lancer_de_n(int nombre_lancers, std::mt19937& generateur) {
    int meilleur_lancer = 0;
    for (int i = 0; i < nombre_lancers; ++i) {
        meilleur_lancer = std::max(meilleur_lancer, lancer_de(generateur));
    }
    return meilleur_lancer;
}

void retirer_voisin(liste_voisins* liste, int numero) {
    if (liste == nullptr) {
        return;
    }

    int i = 0;
    while (i < liste->nombre && liste->voisins[i].num != numero) {
        ++i;
    }

    if (i < liste->nombre) {
        // Décaler les voisins suivants pour garder la liste compacte et dans le même ordre.
        --liste->nombre;
        for (; i < liste->nombre; ++i) {
            liste->voisins[i] = liste->voisins[i + 1];
        }
    }
}

void retirer_pont(int num_ile_a, int num_ile_b,
                  liste_voisins* voisins_a, liste_voisins* voisins_b) {
    retirer_voisin(voisins_a, num_ile_b);

    // Un pont d'une île vers elle-même n'a qu'une entrée dans sa liste.
    if (voisins_b != voisins_a) {
        retirer_voisin(voisins_b, num_ile_a);
    }
}
