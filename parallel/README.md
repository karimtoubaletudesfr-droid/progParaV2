# Version dans `parallel`

## Compilation et exécution

Depuis la racine du projet, avec `g++` :

```bash
cd parallel
g++ -std=c++11 -Wall -Wextra -pedantic -Iinclude src/*.cpp -o simulation
./simulation
```

## Libération de la mémoire

L’ancienne version libérait les îles en laissant leurs habitants et les tableaux `liste_hab` en mémoire. Désormais, `liberer_monde` appelle `liberer_ile` pour chaque île. Celle-ci appelle `liberer_habitant` pour chaque habitant, puis libère `liste_hab` et l’île elle-même. Le monde libère ensuite son tableau d’îles, sa matrice d’adjacence et sa propre mémoire.
