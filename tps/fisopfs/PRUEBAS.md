## Pruebas

Como se dijo en la práctica, se van a ejecutar algunos comandos y se mostrará lo que devuelven

En todas las pruebas se montará el Filesystem en el directorio "prueba".

### ls inicial luego de montar el FS

```bash
(/prueba) $ ls
(/prueba) $ 
```

### Crear un directorio luego de montar el FS

```bash
(/prueba) $ mkdir dir1
(/prueba) $ ls -lsa
total 5
1 drwxr-xr-x 1 user user    0 Dec  9 13:38 .
4 drwxr-xr-x 3 user user 4096 Dec  9 14:06 ..
0 drwxr-xr-x 1 user user    0 Dec  9 14:05 dir1
```

### touch para crear un archivo dentro de dir1

```bash
(/prueba) $ touch dir1/test.txt
(/prueba) $ ls dir1 -lsa
total 1
1 drwxr-xr-x 1 user user 0 Dec  9 14:05 .
1 drwxr-xr-x 1 user user 0 Dec  9 13:38 ..
0 -rw-r--r-- 1 user user 0 Dec  9 14:11 test.txt
```

### Se escribe el archivo dentro de dir1: test.txt

```bash
(/prueba) $ echo "primera linea" > dir1/test.txt
(/prueba) $ cd dir1
(/prueba/dir1) $ cat test.txt
primera linea
```

### Appendeamos una linea a test.txt

```bash
(/prueba/dir1) $ echo "segunda linea" >> test.txt
(/prueba/dir1) $ cat test.txt
primera linea
segunda linea
```

### Sobreescribimos el archivo test.txt

```bash
(/prueba/dir1) $ echo "sobreescritura del archivo" > test.txt
(/prueba/dir1) $ cat test.txt
sobreescritura del archivo
```

### Ejecutamos stat para ver las estadísticas de test.txt

```bash
(/prueba/dir1) $ stat test.txt
File: test.txt
  Size: 28        	Blocks: 1          IO Block: 4096   regular file
Device: 28h/40d	Inode: 3           Links: 1
Access: (0644/-rw-r--r--)  Uid: ( 1000/    user)   Gid: ( 1000/    user)
Access: 2022-12-09 14:29:59.000000000 -0300
Modify: 2022-12-09 14:25:18.000000000 -0300
Change: 2022-12-09 14:29:59.000000000 -0300
 Birth: -
```

### Creamos otro directorio dir2 en prueba

```bash
(/prueba) $ mkdir dir2
(/prueba) $ ls -lash
total 5.0K
 512 drwxr-xr-x 1 user user   27 Dec  9 14:24 .
4.0K drwxr-xr-x 3 user user 4.0K Dec  9 14:41 ..
 512 drwxr-xr-x 1 user user   27 Dec  9 14:24 dir1
   0 drwxr-xr-x 1 user user    0 Dec  9 14:41 dir2
```

### touch para crear un archivo dentro de dir2 y le appendeamos contenido

```bash
(/prueba) $ touch dir2/file.txt
(/prueba) $ echo "Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum" >> dir2/file.txt
(/prueba) $ cat dir2/file.txt
Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum
```

### Ejecutamos less en dir2/file.txt

```bash
(/prueba) $ less dir2/file.txt
Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum
dir2/file.txt (END)
```

### Ejecutamos more en dir2/file.txt

```bash
(/prueba) $ more dir2/file.txt
Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incidid
unt ut labore et dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitati
on ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure dolor in r
eprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur
 sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim 
id est laborum
```

### Ejecutamos stat para ver las estadísticas de file.txt

```bash
(/prueba) $ stat dir2/file.txt
File: dir2/file.txt
  Size: 445       	Blocks: 1          IO Block: 4096   regular file
Device: 28h/40d	Inode: 5           Links: 1
Access: (0644/-rw-r--r--)  Uid: ( 1000/    user)   Gid: ( 1000/    user)
Access: 2022-12-09 14:50:19.000000000 -0300
Modify: 2022-12-09 14:45:05.000000000 -0300
Change: 2022-12-09 14:50:19.000000000 -0300
 Birth: -
```

### Borramos el archivo file.txt

```bash
(/prueba) $ rm dir2/file.txt
(/prueba) $ ls dir2
(/prueba) $
```

### Borramos el dir1 que contiene test.txt

```bash
(/prueba) $ rm -rf dir1
(/prueba) $ ls
dir2
```

### Borramos el dir2 con rmdir

```bash
(/prueba) $ rmdir dir2
(/prueba) $ ls
(/prueba) $
```