# Version dans `parallel`

Notes internes pour s'y retrouver dans le code et préparer les prochains essais.

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
mkdir -p LOGS
mpirun -np 3 ./monde_mpi ../fichierMondes/monde1.txt 1 10 42
```

Les arguments de `monde_mpi` sont le fichier du monde, le rang root, le nombre d'années et la graine facultative. Avec une graine fixée, une île démarre avec son propre générateur, initialisé avec la graine globale plus le numéro de l'île.

Le programme écrit ses fichiers dans `LOGS`, dans le répertoire depuis lequel on le lance.

## Mondes de benchmark

Les quinze fichiers sont dans [`monde/`](../monde/). Pour un niveau donné, les trois catégories ont les mêmes îles dans le même ordre et la même population dans chaque île. Le réseau `sparse` est inclus dans `balanced`, qui est lui-même inclus dans `dense`. Les trois réseaux sont connexes au départ.

| Charge | Îles | Population totale | Ponts sparse | Ponts balanced | Ponts dense |
| --- | ---: | ---: | ---: | ---: | ---: |
| tiny | 16 | 1 280 | [20](../monde/sparse/tiny.txt) | [48](../monde/balanced/tiny.txt) | [96](../monde/dense/tiny.txt) |
| small | 96 | 15 360 | [120](../monde/sparse/small.txt) | [576](../monde/balanced/small.txt) | [2 304](../monde/dense/small.txt) |
| medium | 512 | 131 072 | [640](../monde/sparse/medium.txt) | [3 072](../monde/balanced/medium.txt) | [16 384](../monde/dense/medium.txt) |
| large | 2 048 | 1 048 576 | [2 560](../monde/sparse/large.txt) | [12 288](../monde/balanced/large.txt) | [65 536](../monde/dense/large.txt) |
| extreme | 8 192 | 6 291 456 | [10 240](../monde/sparse/extreme.txt) | [49 152](../monde/balanced/extreme.txt) | [262 144](../monde/dense/extreme.txt) |

Les fichiers de monde `.txt` ont été générés par IA. Ils suivent le format de `monde1.txt`, avec le nombre d'îles, les lignes `numéro nom population`, le nombre de ponts, puis les paires de numéros d'îles.

Sur la machine de développement, un i5‑13420H à 8 cœurs physiques, il restait environ 9 Gio de mémoire disponible au moment du contrôle. Avec une compilation `-O2`, une graine de simulation de 42 et le monde `dense/extreme`, un essai de 10 ans sur 8 rangs a pris environ 8,1 s au chrono du programme. Le plus gros pic mémoire d'un rang était de 128 Mio et la somme des pics RSS individuels était de 796 Mio. Sur 1 rang pendant 1 an, le pic était de 611 Mio. La somme des pics donne un repère conservateur, puisque les rangs peuvent atteindre leur pic à des instants différents. Le fichier `LOGS/LOGS.TXT` occupe environ 151 Mio pour ce monde.

Exemple depuis `parallel/`, avec le dossier `LOGS` déjà présent.

```bash
mpirun -np 8 ./monde_mpi ../monde/dense/extreme.txt 0 10 42
```

## Script de benchmark

Depuis la racine du projet, lancer [`benchmark.sh`](../benchmark.sh).

```bash
./benchmark.sh
```

Le script crée `LOGS` si besoin et compile une fois le binaire MPI `benchmark` avec `-O3`. Il parcourt les catégories dans l'ordre `balanced`, `dense`, `sparse`, puis les fichiers de chaque catégorie par ordre alphabétique. Pour chaque monde, il lance la simulation de 1 à `MAXnprocs` processus, avec le rang 0 comme root.

Les réglages se modifient en haut du script. La configuration actuelle utilise `MAXnprocs=8`, `ANNEES=20` et `GRAINE=42` pour la simulation. Cela donne 120 lancements pour les 15 mondes.

- `LOGS/resultat.txt` conserve les tableaux des benchmarks successifs. Le script affiche aussi chaque ligne dans le terminal, avec le monde, le nombre de processus, le temps en secondes et le nombre d'itérations. Une ligne `==================` clôt chaque benchmark terminé.
- `LOGS/STATS.TXT` conserve les mesures brutes du simulateur, dont le script lit la dernière ligne après chaque lancement.
- `LOGS/LOGS.TXT` et `LOGS/resKmeans` contiennent respectivement la répartition initiale et les enquêtes K-means du dernier lancement.

## Ordre d'exécution MPI

1. **Démarrage.** Le root lit les descriptions d'îles et les range dans `envoi`, par bloc de destination. Un pont entre deux rangs figure dans leurs deux blocs de `envoi_ponts`. Les tailles et déplacements sont en octets. Pour les ponts, `prochaine_case` suit la prochaine position libre dans chaque bloc.
2. **Répartition.** `MPI_Bcast` partage la graine. Les appels à `MPI_Scatter` donnent la taille des tampons à allouer, puis `MPI_Scatterv` remplit `mes_descriptions` et `mes_ponts`. Chaque rang crée ensuite ses habitants, ses listes de voisins et un ghost par île distante reliée. Les rangs écrivent ensemble la répartition initiale dans `LOGS/LOGS.TXT`.
3. **Chaque année.** Les habitants vieillissent et les nouveaux majeurs reçoivent un premier revenu. Les rangs calculent puis partagent les Gini avec leurs voisins. Ils indexent ensuite les revenus, remplacent les morts par des nouveau-nés et accordent une seule hausse de 5 % aux moins de 25 ans si une île voisine a un Gini supérieur.
4. **Tous les cinq ans.** On calcule les médianes après les règles de l'année et on les échange entre voisins distants. On traite les conflits dans l'ordre d'origine des ponts. L'écart est mesuré par rapport à la médiane la plus haute. À partir de 5 % d'écart, l'île riche lance quatre dés et l'autre deux. L'île riche doit obtenir un meilleur lancer strictement supérieur pour fermer le pont. Les deux extrémités mettent leurs listes à jour, puis chaque rang enlève les ghosts devenus inutiles.
5. **Tous les dix ans.** Après les conflits de ponts, les rangs calculent le Gini, le revenu moyen et l'âge moyen de chaque île sur l'état de fin d'année. Ils normalisent les trois valeurs à l'échelle mondiale et font converger un K-means distribué. Les groupes sont classés par Gini décroissant et les catégories 1, 2 et 3 ajustent l'inflation de −0,5, 0 et +0,5 point. Le nouveau taux sert aux indexations suivantes.
6. **Fin.** Les rangs ferment `LOGS/resKmeans` et mesurent leur temps écoulé. Le root affiche le maximum et l'ajoute à `LOGS/STATS.TXT`. Chaque rang libère ses données avant `MPI_Finalize`.

## Catalogue des fonctions

### Démarrage et durée de vie des données

| Fonction | Fichier | Rôle |
| --- | --- | --- |
| `main` | `src/mainPara.cpp` | Orchestre le démarrage MPI, la répartition, la simulation et la sortie des résultats. |
| `creer_ile_portable` | `src/ile_portable.cpp` | Lit le numéro, le nom et l'effectif d'une île pour l'envoi par le root. |
| `rang_round_robin` | `repartion_de_base/round_robin/round_robin.cpp` | Choisit le rang à partir de la position de l'île dans le fichier. |
| `convertir_ile` | `src/ile.cpp` | Transmet la description reçue et le générateur de l'île à `creer_ile`. |
| `creer_ile(num, nom, nb_hab, inflation, generateur)` | `src/ile.cpp` | Alloue l'île et appelle `creation_habitant` pour remplir sa population. |
| `creation_habitant` | `src/habitant.cpp` | Crée le nom, tire l'âge et initialise le revenu d'un habitant. En MPI, le revenu des mineurs démarre à zéro. |
| `ecrire_repartition` | `src/mainPara.cpp` | Écrit les effectifs et le monde local de chaque rang juste après la répartition. |
| `ajouter_statistique` | `src/mainPara.cpp` | Ajoute le chemin du monde, le nombre de processus et le temps écoulé au fichier des mesures. |
| `liberer_ile` | `src/ile.cpp` | Libère les habitants avec `liberer_habitant`, puis leur tableau et l'île. |
| `liberer_habitant` | `src/habitant.cpp` | Libère un habitant. |

### Règles annuelles et conflits de ponts

| Fonction | Fichier | Rôle |
| --- | --- | --- |
| `vieillir_tout_le_monde` | `src/fonction_simulation.cpp` | Augmente les âges et renvoie le tableau temporaire des nouveaux majeurs. |
| `donner_premier_revenu` | `src/fonction_simulation.cpp` | Tire le premier revenu de chaque nouveau majeur avec le générateur de son île. |
| `calculer_gini` | `src/fonction_simulation.cpp` | Calcule le Gini d'une île en parcourant les couples de revenus en O(n²). Sert aussi à l'enquête des dix ans. |
| `calculer_gini_locaux` | `src/mainPara.cpp` | Met à jour les Gini des îles locales et produit un tableau à deux cases par île. |
| `remplir_ghosts` | `src/mainPara.cpp` | Envoie le numéro et la valeur de chaque île concernée une fois par rang voisin. Le tag MPI indique si la valeur reçue est un Gini ou une médiane. |
| `indexer_revenus` | `src/fonction_simulation.cpp` | Applique le taux d'inflation de l'île aux revenus des majeurs. |
| `vie_et_mort` | `src/fonction_simulation.cpp` | Tire un nombre de décès jusqu'à 5 % de la population et réutilise les emplacements pour les nouveau-nés. |
| `hausse_n_pour_cent_pour_moins_de_t_ans` | `src/fonction_simulation.cpp` | Augmente de `n` % les revenus des habitants d'âge strictement inférieur à `t`. La boucle annuelle lui passe 5 et 25. |
| `revenue_median` | `src/fonction_simulation.cpp` | Trie une copie des revenus pour calculer la médiane. |
| `comparer_revenus` | `src/fonction_simulation.cpp` | Compare deux revenus pour le tri `qsort` de la médiane. |
| `traiter_conflits_ponts` | `src/mainPara.cpp` | Compare les médianes, fait les lancers, échange les résultats des ponts distants et compacte le tableau des ponts conservés. |
| `lancer_de_n` | `src/fonction_simulation.cpp` | Appelle `lancer_de` n fois et conserve le maximum. |
| `lancer_de` | `src/fonction_simulation.cpp` | Tire un entier de 1 à 6 avec le générateur de l'île. |
| `retirer_pont` | `src/fonction_simulation.cpp` | Enlève une liaison dans chaque liste locale disponible. |
| `retirer_voisin` | `src/fonction_simulation.cpp` | Retire une entrée et décale les suivantes dans une liste. |
| `retirer_ghosts_sans_pont` | `src/mainPara.cpp` | Garde les ghosts encore utilisés par une île locale et recalcule leurs indices. |

### Enquête et résultats K-means

| Fonction | Fichier | Rôle |
| --- | --- | --- |
| `calculer_caracteristiques_locales` | `src/mainPara.cpp` | Prépare un tableau à trois cases par île avec le Gini, le revenu moyen et l'âge moyen. |
| `normaliser_caracteristiques` | `src/mainPara.cpp` | Échange les totaux globaux avec `MPI_Allreduce`, puis centre et réduit les valeurs sur chaque rang. |
| `initialiser_centres` | `src/mainPara.cpp` | Sélectionne les îles aux plus petits numéros avec `MPI_MINLOC` et diffuse leurs coordonnées. |
| `affecter_groupes_kmeans` | `src/mainPara.cpp` | Affecte les îles au centre le plus proche et partage les sommes par groupe. Arrête les itérations quand les affectations se stabilisent. Un groupe vide garde son centre précédent. |
| `appliquer_categories_kmeans` | `src/mainPara.cpp` | Classe les centres par Gini décroissant, transforme les numéros de groupe en catégories et ajuste l'inflation. |
| `ecrire_resultats_kmeans` | `src/mainPara.cpp` | Formate les lignes locales et les écrit collectivement dans `LOGS/resKmeans`. |
| `verifier_fichier_mpi` | `src/mainPara.cpp` | Affiche le chemin concerné et arrête le programme si une opération sur un fichier MPI échoue. |

### Entrée séquentielle dans `parallel`

| Fonction | Fichier | Rôle |
| --- | --- | --- |
| `main` | `src/main.cpp` | Lit, affiche et libère le monde de démonstration. |
| `generation` | `src/monde.cpp` | Lit les îles et construit la matrice d'adjacence. |
| `creer_ile(fichier)` | `src/ile.cpp` | Lit une description et appelle l'autre version de `creer_ile`. |
| `affichage_monde` | `src/monde.cpp` | Affiche les îles et leur matrice d'adjacence. |
| `affichage_ile` | `src/ile.cpp` | Affiche une île en appelant `affichage_habitant` pour chaque personne. |
| `affichage_habitant` | `src/habitant.cpp` | Affiche le nom, l'âge et le revenu d'un habitant. |
| `liberer_monde` | `src/monde.cpp` | Libère les îles, la matrice d'adjacence et le monde. |

La version d'origine dans [`Sequentiel/`](../Sequentiel/) regroupe les mêmes opérations de démonstration dans `monde.cpp` avec `generation`, `creation_habitant`, `affichage_monde`, `affichage_ile`, `affichage_habitant` et `liberer_monde`. Son `main.cpp` enchaîne lecture, affichage et libération.

## Fichier à lire pour les statistiques

`LOGS/resKmeans` est un CSV à trois colonnes, `annee,ile,categorie`. Il contient une ligne par île pour les années 10, 20, 30, etc. Les lignes d'une enquête sont écrites dans l'ordre des rangs. Pour comparer deux exécutions avec des nombres de rangs différents, utiliser le couple `(annee, ile)` comme identifiant.

Chaque lancement recrée le contenu du fichier, y compris pour une simulation de moins de dix ans où seul l'en-tête est écrit.

## Temps d'exécution et logs

Le programme réécrit `LOGS/LOGS.TXT` une seule fois par lancement, après la répartition. Le fichier contient un bloc par rang, avec le nombre d'îles, le nombre d'habitants et l'état initial de son monde local.

La ligne `Simulation parallèle (s)` donne le temps écoulé du rang le plus lent. La mesure va de la lecture du monde à la fermeture de `LOGS/resKmeans`, avec le temps passé à écrire `LOGS.TXT` soustrait.

Le root ajoute une ligne à `LOGS/STATS.TXT` à la fin de chaque lancement. Les trois colonnes sont séparées par des tabulations, dans cet ordre : chemin du monde passé en argument, nombre de processus, durée en secondes.
