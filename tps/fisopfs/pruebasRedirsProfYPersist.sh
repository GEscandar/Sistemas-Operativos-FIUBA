#!/bin/bash
###########################
FS_MOUNT_POINT="prueba"
cd $FS_MOUNT_POINT
###########################
nombreDelTest="test1 _ redirs _ Se escribe usando '>', y se lee con cat correctamente."
echo "algo">arch1.txt

expected="algo"
actual=$(cat arch1.txt)

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: ${actual}."
fi

rm arch1.txt

############################

nombreDelTest="test2 _ redirs _ Se escribe, se appendea, y se lee con cat correctamente."
test_string1="primera línea"
test_string2="una segunda línea"
echo $test_string1>arch1.txt
echo $test_string2>>arch1.txt

expected=$test_string1$'\n'$test_string2
actual=$(cat arch1.txt)

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: ${actual}."
fi

rm arch1.txt

#############################

nombreDelTest="test3 _ redirs _ Se escribe, se sobreescribe con '>', y se lee con cat correctamente."
test_string1="primera línea"
test_string2="piso lo existente con esto"
echo $test_string1>arch1.txt
echo $test_string2>arch1.txt

expected=$test_string2
actual=$(cat arch1.txt)

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: ${actual}."
fi

rm arch1.txt

##############################

nombreDelTest="test4 _ prof requerida _ Se escribe ok en archivo dentro de dir dentro de '/'."
test_string="hola"

dir="unDirectorio"
mkdir $dir
pathDelArchivo="$dir/unArchivo.txt"
echo $test_string>$pathDelArchivo

expected=$test_string
actual=$(cat $pathDelArchivo)

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: ${actual}."
fi

rm $pathDelArchivo
rmdir $dir
##############################

nombreDelTest="test5 _ prof extra _ Se escribe ok en archivo dentro de múltiples dirs anidados."
test_string="hola"

dir="dir1"
dir2="dir2"
dir3="dir3"

mkdir -p "$dir/$dir2/$dir3"
pathDelArchivo="$dir/$dir2/$dir3/unArchivo.txt"

echo $test_string>$pathDelArchivo

expected=$test_string
actual=$(cat $pathDelArchivo)

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: ${actual}."
fi

#echo "tree de debug:"
#tree

rm $pathDelArchivo
rmdir "$dir/$dir2/$dir3"
rmdir "$dir/$dir2"
rmdir $dir
##############################

### IMPORTANTE: Si este test6 llegara a fallar, asegurarse de 'rm image.fisop' antes de montar el fs antes de ejecutar estas pruebas.
nombreDelTest="test6 _ persistencia _ Se lee ok luego de desmontar y volver a montar."
test_string="una string que debería seguir existiendo"
dir="probandoPersistencia"

mkdir "$dir"
pathDelArchivo="$dir/unArchivoAPersistir.txt"
echo $test_string>$pathDelArchivo

#echo "--- debug: tree par ver lo creado:"
#tree

cd ..
sudo umount "prueba/"
#echo "--- debug: tree"
#tree

# ----------- #
# Esperando a que se monte de nuevo, manualmente, el fs
echo "El fs se ha desmontado para probar persistencia. En otra terminal, volvé a montarlo, y luego presioná cualquier letra para continuar:"
char=$(read -t 40 -n 1)
# -------- #
# Continúa el test

cd "prueba/"
#echo "--- debug: mount | grep fisop"
#mount | grep "fisop"
#echo "--- debug: pwd me da $(pwd)"
echo "--- debug: tree habiendo vuelto:"
tree

expected=$test_string
actual=$(cat $pathDelArchivo)

if [[ $expected = $actual ]]; then
    echo "[OK] ${nombreDelTest}"
else
    echo "[ERROR] ${nombreDelTest} expected: ${expected} but got: ${actual}."
    echo "TROUBLESHOOTING: En la otra terminal, ejecutar: 'rm image.fisop' y volver a montar;"
    echo "                 Entonces volver a ejecutar estas pruebas."
fi

## Limpieza: borrar los archivos y directorios:
rm $pathDelArchivo
rmdir $dir
##############################
