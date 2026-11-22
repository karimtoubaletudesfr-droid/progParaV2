#include <iostream>

#include "monde.hpp"

int main(void) {

    struct monde* mm = generation("monde1.txt");
    
    affichage_monde(mm);

    liberer_monde(&mm);

    return EXIT_SUCCESS;
}