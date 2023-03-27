#!/bin/bash
###########################
FS_MOUNT_POINT="prueba"
cd $FS_MOUNT_POINT
echo "----- Pruebas de hardlinks -----"
echo "123">h1.txt
###########################
nombreDelTest="test1 _ funcnm _ creación de hardlink: tienen mismo contenido."

link h1.txt h2.txt
actual=$(diff h1.txt h2.txt | wc -l)
expected=0

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: ${actual}."
fi
#######

nombreDelTest="test2 _ funcnm _ modificar el hardlink modifica al original."
echo "456">>h2.txt
sleep 1
#sleep 0.902 #4 #9 #2 # 2 ###0.94922;
expected=$(cat h2.txt)
actual=$(cat h1.txt)

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: ${actual}."
fi

########
## Tests 3 y 4 _ Ambos tienen la misma cantidad de hardlinks: 2.
nombreDelTest="test3 _ cant enlaces _ el original tiene 2 hardlinks"
actual=$(stat h1.txt | grep Enlaces | cut -d ":" -f 4)
expected=" 2"

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: ${actual}."
fi

###
nombreDelTest="test4 _ cant enlaces _ el link tiene 2 hardlinks"
actual=$(stat h2.txt | grep Enlaces | cut -d ":" -f 4)
expected=" 2"

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: ${actual}."
fi
####################

nombreDelTest="test5 _ rm _ luego de borrar el orig, puedo seguir leyendo el link"
rm h1.txt
actual=$(cat h2.txt)
expected="123"$'\n'"456"

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: ${actual}."
fi

###
nombreDelTest="test6 _ rm _ luego de borrar el orig, el link tiene solo 1 hardlink"
actual=$(stat h2.txt | grep Enlaces | cut -d ":" -f 4)
expected=" 1"

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: ${actual}."
fi

##########
nombreDelTest="test7 _ rm _ es posible borrar el último hardlink"

actual=$(rm h2.txt 2>&1 | wc -l)
expected="0"

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: ${actual}."
fi

###############

nombreDelTest="test8 _ rm name _ rm hardlink borra el de nombre correcto"

echo "123">h1.txt
link h1.txt h2.txt
rm h2.txt

actual=$(cat h1.txt 2>&1)
expected="123"

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: ${actual}."
fi

###############
nombreDelTest="test9 _ truncate _ luego de sobreescritura sigue teniendo nlink 2."

echo "123">h1.txt
link h1.txt h2.txt

echo "sobreescribo">h2.txt

actual=$(stat h1.txt | grep Enlaces | cut -d ":" -f 4)
expected=" 2"

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: ${actual}."
fi

###############
############################
