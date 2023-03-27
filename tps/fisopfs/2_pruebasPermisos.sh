#!/bin/bash
###########################
TESTUSER=$(whoami) # user a usar para pruebas de chmod.
if [ $TESTUSER = "root" ]; then
  echo "Para que las pruebas de chown tengan sentido, correr esto sin ser root. Saliendo."
  exit
fi

FS_MOUNT_POINT="prueba"
cd $FS_MOUNT_POINT
echo "----- Pruebas de permisos -----"
echo "hola mundo">perm.txt
###########################
nombreDelTest="test1 _ puedo leer."

expected="hola mundo"
actual=$(cat perm.txt)

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: ${actual}."
fi

##########
nombreDelTest="test2 _ intento lectura da error si chmod -r."
chmod -r perm.txt

expected="cat: perm.txt: Permiso denegado"
actual=$(cat perm.txt 2>&1)

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: ${actual}."
fi

#############

nombreDelTest="test3 _ intento escritura da error si chmod -w."
chmod -w perm.txt

expected=" echo: error de escritura: Permiso denegado"
actual=$((echo "segunda línea">>perm.txt) 2>&1)
# (la línea completa $actual incluye el nombre de este script al inicio, me quedo solo con la siguiente parte)
actual=$(echo $actual | cut -d ":" -f -2 --complement)

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: $actual."
fi

###############
###
# Agregarle sudo cuando funcione como el chown de verdad.
nombreDelTest="test4 _ chown root perm.txt cambia el user y no el group. _ chown caso1."
chown root perm.txt

expected="root al"
owner=$(ls -la perm.txt | cut -d " " -f 3)
group=$(ls -la perm.txt | cut -d " " -f 4)
actual=$owner" "$group

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: $actual."
fi

###############

nombreDelTest="test5 _ puedo escribir y leer si chmod 077 y soy group."
# No soy root, soy user del grupo de user, así que debería poder hacer cosas con permisos de group.
chmod 077 perm.txt

(echo "algo más">>perm.txt) 2>&1
expected="algo más"
actual=$(tail --lines=1 perm.txt)

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: $actual."
fi

###############

nombreDelTest="test6 _ chown unUser:root perm.txt cambia el user y el group. _chown caso2."
chown $TESTUSER:root perm.txt

expected="al root"
owner=$(ls -la perm.txt | cut -d " " -f 3)
group=$(ls -la perm.txt | cut -d " " -f 4)
actual=$owner" "$group

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: $actual."
fi

###############

nombreDelTest="test7_ chown :unUser perm.txt cambia solo el group. _ chown caso4."
chown :$TESTUSER perm.txt

expected="al al"
owner=$(ls -la perm.txt | cut -d " " -f 3)
group=$(ls -la perm.txt | cut -d " " -f 4)
actual=$owner" "$group

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: $actual."
fi

###############

nombreDelTest="test8 _ chown unUser: perm.txt cambia el user y el group. _ chown caso3."
chown root: perm.txt

expected="root root"
owner=$(ls -la perm.txt | cut -d " " -f 3)
group=$(ls -la perm.txt | cut -d " " -f 4)
actual=$owner" "$group

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: $actual."
fi

###############

nombreDelTest="test9 _ chown : perm.txt no efectúa cambios. _ chown caso5."
owner=$(ls -la perm.txt | cut -d " " -f 3)
group=$(ls -la perm.txt | cut -d " " -f 4)
expected=$owner" "$group

chown : perm.txt

owner=$(ls -la perm.txt | cut -d " " -f 3)
group=$(ls -la perm.txt | cut -d " " -f 4)
actual=$owner" "$group

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: $actual."
fi

###############

nombreDelTest="test10 _ no puedo leer con chmod 007 un arch siendo user y grupo."

chown $TESTUSER:$TESTUSER perm.txt
chmod 007 perm.txt
expected="cat: perm.txt: Permiso denegado"
actual=$(cat perm.txt 2>&1)

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: $actual."
fi

###############

nombreDelTest="test11 _ no puedo escribir con chmod 007 un arch siendo user y grupo."

chown $TESTUSER:$TESTUSER perm.txt
chmod 007 perm.txt

expected=" echo: error de escritura: Permiso denegado"
actual=$((echo "segunda línea">>perm.txt) 2>&1)
# (la línea completa $actual incluye el nombre de este script al inicio, me quedo solo con la siguiente parte)
actual=$(echo $actual | cut -d ":" -f -2 --complement)

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: $actual."
fi

###############
rm perm.txt
###############################

nombreDelTest="test12 _ puedo hacer chmod a un dir y se setean ok los permisos."

mkdir directorio
chmod 704 directorio

expected="drwx---r--"
actual=$((ls -la | grep directorio | cut -d " " -f 1) 2>&1)

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: $actual."
fi
#####
rmdir directorio
###################

nombreDelTest="test13 _ no puedo hacer ls sobre dir sin permisos."
	   # También: puedo hacer chown sobre dir y funciona.

mkdir dirDeRoot
chown root:root dirDeRoot
chmod 700 dirDeRoot

expected="ls: no se puede abrir el directorio 'dirDeRoot': Permiso denegado"
actual=$(ls dirDeRoot 2>&1)

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: $actual."
fi

###################
nombreDelTest="test14 _ no puedo crear carpeta dentro de dir sin permisos."

expected="mkdir: no se puede crear el directorio «dirDeRoot/bypass»: Permiso denegado"
actual=$(mkdir dirDeRoot/bypass 2>&1)

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: $actual."
fi

###
nombreDelTest="test15 _ no puedo crear archivo dentro de dir sin permisos."

expected=" dirDeRoot/bypass.txt: Permiso denegado"
actual=$((echo "hola">dirDeRoot/bypass.txt) 2>&1)
# (la línea completa $actual incluye el nombre de este script al inicio, me quedo solo con la siguiente parte)
actual=$(echo $actual | cut -d ":" -f -2 --complement)

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: $actual."
fi
#####
rmdir dirDeRoot
###############
############################
