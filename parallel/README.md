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
mpic++ -std=c++11 -Wall -Wextra -pedantic -Iinclude -Irepartion_de_base/round_robin src/mainPara.cpp src/ile.cpp src/ile_portable.cpp src/habitant.cpp src/fonction_simulation.cpp repartion_de_base/round_robin/round_robin.cpp -o monde_mpi
mpirun -np 3 ./monde_mpi ../fichierMondes/monde1.txt 1 10 42
```

- Le root lit le fichier et réparti les îles entre les processus en round robin. Il met les descriptions dans `envoi`, avec un bloc par rang.
- Pour les ponts, le root retrouve le rang des deux îles reliées. Si elles sont sur deux rangs différents, le pont est mis dans les deux blocs de `envoi_ponts`.
- Les tableaux de tailles disent combien d’octets chaque rang reçoit. Les déplacements disent ou commence son bloc. `prochaine_case` indique la prochaine place libre quand on range les ponts.
- `MPI_Scatter` envoie d’abord les tailles. Comme ça chaque rang sait combien de mémoire allouer. Ensuite `MPI_Scatterv` remplit `mes_descriptions` et `mes_ponts`.
- Chaque rang crée les habitants de ses propres îles. Les tableaux sont alloués avec `malloc` ou `calloc`, puis libérés avec `free` quand on en a plus besoin.

## Libération de la mémoire

L’ancienne version libérait les îles en laissant leurs habitants et les tableaux `liste_hab` en mémoire. Désormais, `liberer_monde` appelle `liberer_ile` pour chaque île. Celle-ci appelle `liberer_habitant` pour chaque habitant, puis libère `liste_hab` et l’île elle-même. Le monde libère ensuite son tableau d’îles, sa matrice d’adjacence et sa propre mémoire.
