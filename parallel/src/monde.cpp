#include "monde.hpp"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <ile.hpp>

struct monde* generation(const char* filename) {

    struct monde* m = (struct monde*)malloc(sizeof(struct monde));

    std::ifstream f(filename);

    f >> m->nb_iles;
    int nb = m->nb_iles;
    m->mes_iles = (struct ile**)malloc(m->nb_iles*sizeof(struct ile*));

    for (int i = 0; i < nb; ++i) {
        m->mes_iles[i] = creer_ile(f);
    }

    m->mat_adj = (int*)calloc(nb*nb,sizeof(int));

    int nb_arete;
    f>>nb_arete;

    int a, b;
    for (int i=0; i<nb_arete; i++) {
        f>>a>>b;
        m->mat_adj[a*nb+b] = 1;
        m->mat_adj[b*nb+a] = 1;
    }

    m->inflation = inflation_initiale;

    return m;
}

void affichage_monde(struct monde* m) {
    std::cout << m->nb_iles << " iles dans ce monde :" << std::endl;
    for (int i=0; i<m->nb_iles; i++)
        affichage_ile(m->mes_iles[i]);
    std::cout << "connectivité : " << std::endl;
    for (int i=0; i<m->nb_iles; i++) {
        for (int j=0; j<m->nb_iles; j++)
            std::cout << m->mat_adj[i*m->nb_iles+j] << " ";
        std::cout << std::endl;
    }
}

void liberer_monde(struct monde** m) {
    for (int i = 0; i < (*m)->nb_iles; ++i) {
        liberer_ile((*m)->mes_iles[i]);
    }
    std::free((*m)->mes_iles);
    std::free((*m)->mat_adj);
    std::free(*m);
    *m = nullptr;
}
