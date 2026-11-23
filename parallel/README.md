# Version dans `parallel`

## Compilation et exécution

Depuis la racine du projet, avec `g++` :

```bash
cd parallel
g++ -std=c++11 -Wall -Wextra -pedantic -Iinclude src/main.cpp src/monde.cpp src/ile.cpp src/habitant.cpp -o simulation
./simulation
```

### Démarrage MPI

Depuis `parallel` :

```bash
mpic++ -std=c++11 -Wall -Wextra -pedantic -Iinclude -Irepartion_de_base/round_robin src/mainPara.cpp src/ile.cpp src/ile_portable.cpp src/habitant.cpp repartion_de_base/round_robin/round_robin.cpp -o monde_mpi
mpirun -np 3 ./monde_mpi ../fichierMondes/monde1.txt 1 10 42
```

Cet exemple utilise le rang 1 comme racine. La version actuelle répartit les descriptions des îles et crée les habitants localement. La durée est validée au lancement pour la future boucle de simulation.

## Libération de la mémoire

L’ancienne version libérait les îles en laissant leurs habitants et les tableaux `liste_hab` en mémoire. Désormais, `liberer_monde` appelle `liberer_ile` pour chaque île. Celle-ci appelle `liberer_habitant` pour chaque habitant, puis libère `liste_hab` et l’île elle-même. Le monde libère ensuite son tableau d’îles, sa matrice d’adjacence et sa propre mémoire.
