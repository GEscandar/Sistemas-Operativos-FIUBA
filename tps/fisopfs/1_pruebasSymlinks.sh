#!/bin/bash
###########################
FS_MOUNT_POINT="prueba"
cd $FS_MOUNT_POINT
echo "----- Pruebas de symlinks -----"
echo "123">h1s.txt
###########################
nombreDelTest="test1 _ funcnm _ creación de symlink: tienen mismo contenido."

ln -s h1s.txt h2s.txt
actual=$(diff h1s.txt h2s.txt | wc -l)
expected=0

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: ${actual}."
fi
#######

nombreDelTest="test2 _ funcnm _ modificar el symlink modifica al original."
echo "456">>h2s.txt
sleep 1
#sleep 0.902 #4 #9 #2 # 2 ###0.94922;
expected=$(cat h2s.txt)
actual=$(cat h1s.txt)

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: ${actual}."
fi


####################

nombreDelTest="test3 _ rm _ luego de borrar el symlink, puedo seguir leyendo el orig"
rm h2s.txt
actual=$(cat h1s.txt 2>&1)
expected="123"$'\n'"456"

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: ${actual}."
fi

####################

nombreDelTest="test4 _ rm _ luego de borrar el orig, no puedo seguir leyendo el link"
ln -s h1s.txt h2s.txt
rm h1s.txt
actual=$(cat h2s.txt 2>&1)
expected="cat: h2s.txt: No such file or directory"

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: ${actual}."
fi
