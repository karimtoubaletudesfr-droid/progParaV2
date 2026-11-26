#!/bin/bash
set -e

MAXnprocs=8
ANNEES=20
GRAINE=42
RESULTATS=LOGS/resultat.txt

mkdir -p LOGS

echo "Compilation du benchmark..." >&2
mpic++ -O3 -std=c++11 \
    -Iparallel/include \
    -Iparallel/repartion_de_base/round_robin \
    parallel/src/mainPara.cpp parallel/src/ile.cpp parallel/src/ile_portable.cpp \
    parallel/src/habitant.cpp parallel/src/fonction_simulation.cpp \
    parallel/repartion_de_base/round_robin/round_robin.cpp \
    -o benchmark

echo "Monde | Nproc | Temps (s) | Nb itérations" | tee -a "$RESULTATS"

for monde in monde/{balanced,dense,sparse}/*.txt
do
    for ((i=1; i<=MAXnprocs; i++))
    do
        printf 'En cours : %s, %d proc, %d ans\n' "$monde" "$i" "$ANNEES" >&2
        mpirun -np "$i" ./benchmark "$monde" 0 "$ANNEES" "$GRAINE" > /dev/null
        temps=$(tail -n 1 LOGS/STATS.TXT | cut -f 3)
        echo "$monde | $i | $temps | $ANNEES" | tee -a "$RESULTATS"
    done
done

echo "==================" | tee -a "$RESULTATS"
