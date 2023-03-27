# Fisopfs

Sistema de archivos tipo FUSE.


# Diseño de la Solución

## Contexto

El diseño de este filesystem se llevó a cabo tomando como inspiración el modelo de inodos de los filesystems principales de sistemas operativos unix-like (ext3, ext4, etc.). Cuenta con *10240* bloques de datos de *4096 Bytes*. Dicho valor surge del tamaño de las páginas de memoria en sistemas de 32 bits.

Esta implementación de filesystem, sin embargo, no usa una estructura del super bloque para guardar meta datos sobre el filesystem. Como dicha información ocupa mucho menos espacio que el que hay en un bloque, se decidió usar variables y directivas del procesador para guardar la información que le correspondería.

La estructura del inodo, que guarda información sobre archivos y directorios, es la siguiente:

```c
    typedef struct inode {
        uint32_t ino;       // inode number
        uint32_t mode;      // inode (file or directory) mode flags:
        size_t file_size;   // real size of the file (or directory) in bytes
        uint8_t nblocks;    // size of the file (or directory) in blocks
        time_t d_changed;   // last time metadata related to the file changed
        time_t d_modified;  // last time the file's contents were modified
        time_t d_accessed;  // last access date
        uint32_t data_blocks[NDATA_BLOCKS_PER_INODE];  // direct data block pointers
        uint32_t nentries;  // if type==DIR, this is the number of directory entries of this inode
        uint32_t parent;  // inode number of the parent directory of this inode
    } inode_t;  
```

Donde:
- `ino` es el número único que identifica al inodo
- `mode` representa el tipo de inodo y las operaciones permitidas sobre el mismo (lectura, escritura, ejecución).
- `file_size` representa el tamaño del contenido del inodo, ya sea de un archivo o directorio. El `file_size` de un directorio es siempre igual a la suma de los `file_size` de sus entradas.
- `nblocks` es la cantidad de bloques usadas actualmente por el inodo para guardar siu contenido.
- `d_changed` es la fecha de la última modificación del inodo.
- `d_modified` es la fecha de la última modificación de los datos del archivo o directorio.
- `d_accessed` es la fecha del último acceso (lectura) del inodo.
- `data_blocks` es un array de referencias 'directas' a bloques de datos. Como en nuestro caso los bloques de datos están guardados en memoria como un array también llamado *'data_blocks'*, lo que se guarda en este caso son los números de bloque. Se guardan tantos elementos como sea necesario para que el tamaño total de la estructura sume *256 Bytes*.
- `nentries` es, si el inodo fuera de tipo directorio, la cantidad de entradas del mismo. Si el inodo fuera de tipo archivo, siempre vale cero.
- `parent` es el número de inodo del directorio padre que tiene como entrada a este inodo.

Con esta estructura, el tamaño máximo soportado de un archivo es igual a NDATA_BLOCKS_PER_INODE * BLOCK_SIZE.

Para controlar si un inodo o un bloque de datos está ocupado, se definió también un tipo de dato para representar a un bitmap:

```c
typedef uint32_t word_t;
typedef struct bitmap {
	word_t *words;
	uint32_t nwords;
} bitmap_t;
```

Donde:
- `words` es un puntero a un array de 'palabras', que son simplemente enteros sin signo donde cada bit de los mismos se usa para representar un elemento del bitmap.
- `nwords` es la cantidad de enteros del array referenciado por `words`.

Además, se implementaron las funciones necesarias para el uso de esta estructura:
- `set_bit`: pone en 1 un bit del bitmap
- `clear_bit`: pone en 0 un bit del bitmap
- `is_bit_on`: se fija si un bit tiene valor igual a 1
- `get_free_bit`: devuelve el primer bit del bitmap cuyo valor es 0.

Los arrays de palabras de los bitmaps de inodos y de datos (que son lo que se persiste al desmontar el filesystem) ocupan cada uno 1 bloque en memoria, por lo que pueden guardar un total de BLOCK_SIZE * nbits_por_byte = 4096 * 8 = 32768 bits.

Luego, para representar una entrada de directorio se usa la siguiente estructura:
```c
    typedef struct dirent {
        uint32_t ino;                // inode number
        char name[DIRENT_NAME_LEN];  // entry name
    } dirent;
```

Donde:
- `ino` es el número del inodo asociado a la entrada
- `name` es el nombre de la entrada. Se ajustó su longitud máxima para que el tamaño total de la estructura sumara *256 Bytes*.

Un detalle a destacar es que los inodos de tipo 'directorio' guardan en sus bloques de datos estas mismas estructuras, por lo que el número máximo de entradas por directorio puede calcularse como (BLOCK_SIZE / sizeof(dirent)) * NDATA_BLOCKS_PER_INODE = (4096 / 256) * 50 = *800 entradas*. 

Además, el número de inodo de una entrada puede corresponder a un inodo de directorio, lo que nos permite tener muchos niveles en nuestro 'árbol' de directorios. El número exacto dependerá de la cantidad de bloques que esten siendo usados para guardar archivos o entradas de otros directorios en el momento, pues son bloques que ya estarán ocupados y por lo tanto limitarán cada vez más la profundidad de nuestro filesystem mientras más se usen. En resumen: mientras menos bloques estén siendo usados por el filesystem, mayor será la cantidad de bloques disponibles para guardar entradas de directorio y, como consecuencia, mayor será la posible profundidad de nuestro filesystem.


Finalmente, las estructuras nombradas hasta el momento se declaran de la siguiente forma dentro del filesystem:

```c
// inode bitmap, to check wether an inode is in use or not
word_t __inode_bitmap[NWORDS_PER_BLOCK] = { 0 };
bitmap_t inode_bitmap = { .words = __inode_bitmap,
	                  .nwords = sizeof(__inode_bitmap) / sizeof(word_t) };

// data bitmap, to check wether a data block is in use or not
word_t __data_bitmap[NWORDS_PER_BLOCK] = { 0 };
bitmap_t data_bitmap = { .words = __data_bitmap,
	                 .nwords = sizeof(__data_bitmap) / sizeof(word_t) };

// blocks containing inodes: file metadata
inode_t inode_table[NTOTAL_INODES];

// blocks containing file data
uint32_t data_blocks[NDATA_BLOCKS * (BLOCK_SIZE / sizeof(uint32_t))];
```


## Accesos

Para acceder a un inodo se necesita el número de inodo, su ID (*inumber*) y 
la dirección de comienzo a la tabla de inodos.

Para acceder a un bloque, se usa su número de orden, y la dirección de comienzo
de la data region; este último dato es accesible desde cualquier parte del código.

## Búsqueda de un Path

Para traducir una ruta del filesystem (un path) al inodo de interés, se usa la 
función *find_inode*, que busca cada uno de los nombres de la ruta en el árbol
de directorios, bajando un nivel en el mismo cuando hay una coincidencia de 
nombres entre una entrada de directorio y uno de los nombres de la ruta; 
Por ejemplo, si se buscara la ruta /dir1/dir2/archivo, primero se recorren todas 
las entradas del directorio raíz del filesystem y se busca una cuyo nombre sea 
'dir1'. Si la entrada es un directorio, entonces se procede a hacer la búsqueda 
de las entradas del inodo de 'dir1' (obtenido usando el número de inodo de la
entrada) por una cuyo nombre sea 'dir2'. Al encontrarse y cumplirse la misma 
condición previamente mencionada, se hace lo mismo para encontrar la entrada 
que corresponde a 'archivo', y una vez encontrada esta se devuelve un puntero al 
inodo asociado. En caso de falla (no se encuentra una entrada que coincida con un 
nombre de la ruta, o el inodo asociado a la entrada es de tipo incorrecto) se 
devuelve NULL.

A continuación, el código que lleva a cabo todo esto:
```c
inode_t *
find_inode(const char *path, uint32_t type_flags)
{
	if (strcmp(path, ROOT_INODE_NAME) == 0)
		return root;

	char **tokens =
	        malloc((strlen(path) / 2) *
	               sizeof(char *));  // puede haber como máximo strlen(path)/2
	                                 // strings divididos por el delimitador
	int split_size = strsplit(path, tokens, '/');
	char *last_token = tokens[split_size - 1];

	// Search the filesystem directory structure to find the inode
	inode_t *inode = NULL;
	uint32_t nentries = root->nentries;
	dirent *entries = (dirent *) GET_DATA_BLOCK(root->data_blocks[0]);
	bool found = false;

	int i = 0, j = 0;
	while (!found && j < nentries && i < split_size) {
		dirent *entry = &entries[j % NENTRIES_PER_BLOCK];
		printf("[debug] entry name: %s, token: %s, j: %d\n",
		       entry->name,
		       tokens[i],
		       j);
		int comp = strcmp(entry->name, tokens[i]);
		if (comp == 0) {
			inode = GET_INODE(entry->ino);
			printf("[debug] found match for token %s\n", tokens[i]);
			print_inode(inode);
			if (tokens[i] == last_token) {
				if ((inode->mode & type_flags) != 0)
					found = true;
				else
					break;  // inode is of incorrect type
			}
			if (S_ISDIR(inode->mode)) {
				j = 0;
				nentries = inode->nentries;
				entries = (dirent *) GET_DATA_BLOCK(
				        inode->data_blocks[0]);
				free(tokens[i]);
				i++;
			}
		}
		if (comp != 0 || (comp == 0 && !S_ISDIR(inode->mode))) {
			j++;
			if (j % NENTRIES_PER_BLOCK == 0 && inode != NULL) {
				printf("[debug] more than 16 entries, "
				       "switching to next block\n");
				inode_t *parent =
				        inode != root ? GET_INODE(inode->parent)
				                      : root;
				entries = (dirent *) GET_DATA_BLOCK(
				        parent->data_blocks[j / NENTRIES_PER_BLOCK]);
			}
		}
	}

	for (; i < split_size; i++)
		free(tokens[i]);
	free(tokens);

	if (!found)
		return NULL;

	return inode;
}
```

## Algunas de las operaciones implementadas

#### Escritura (write)
Al escribir se debe efectuar una serie de operaciones.
En primer lugar hay que obtener el inodo del archivo en cuestión.
Luego habrá que efectivamente escribir lo deseado en el file system, esto comprende varios pasos.
 - obtener/acceder a los bloques de datos del inodo para almacenar en ellos lo recibido por write.
 - actualizar el bitmap de datos en caso de que la cantidad de bloques de datos existentes hasta el momento no sea suficiente para realizar la escritura.
 - actualizar el bitmap de inodos con el nuevo size, y con el nuevo bloque de datos en caso de aberse allocado uno.

Algunas consideraciones
Algo a tener en cuenta es que la función write 'intenta' escribir la cantidad de bytes solicitados, por lo que la actualización de file_size con el tamaño solicitado no se hacen en una única vez (tampoco la escritura), sino que se va escribiendo y actualizando file_size en cada vuelta de un loop luego de allocar un nuevo bloque de ser necesario.

Luego de calcular la dirección de memoria en la que se encuentra el inicio del bloque de datos analizado, se escribe directamente a dicha dirección utilizando memset.

#### Lectura (read)
Al leer, el procedimiento es el siguiente.
En primer lugar, al igual que para escribir, hay que obtener el inodo del archivo en cuestión.
Luego procederemos a leer los datos, para ello:
 - al igual que en el caso de la escritura, obtenemos los bloques de datos del archivo, que pueden ser hallados a través de la metadata del mismo, es decir, utilizando el inodo.
 - cargamos los datos requeridos al buffer de destino provisto por la librería fuse.
 - actualizamos la fecha de último acceso en su metadata.

En este caso no se requiere actualizar bitmaps ya que solamente se está accediendo a los datos sin modificarlos, por lo que esta operación no generará nuevos bloques ni eliminará existentes.

Iteramos entonces sobre los bloques de datos utilizados por el archivo (teniendo en cuenta el offset desde el cual se solicita leer), leyendo la cantidad de bytes requerida, y copiándola al buffer proporcionado por la librería fuse.


## Sobre persistencia
Al momento de montar el sistema de archivos, se ejecuta la función fisopfs_init, en ella leemos del archivo image.fisopfs y con su contenido cargamos nuestras variables del filesystem.
Posteriormente, el sistema se persiste a disco al momento de recibirse una solicitud de flush para un archivo, cuya implementación se encuentra en fisopfs_flush, en la cual se lleva a cabo el proceso inverso, escribiéndose nuestras variables al archivo (por simplicidad, se optó por persistir el sistema completo a disco en cada flush).
Por último, al desmontar el fs se ejecuta fisopfs_destroy, que llama a fisopfs_flush para toda la jerarquía de directorios desde "/" que corresponde al punto de montaje.


Notar el concepto de punto de montaje para aclarar la siguiente situación de otra forma paradójica.
El kernel redirige las solicitudes a read/write/etc a nuestro proceso de user que incluye el header de la librería FUSE. Estamos implementando las funciones que se ejecutarán al llamarse desde cualquier proceso a las syscalls write/read/etc, y la implementación dice que todo el FS se maneja en memoria. Sabemos que quien tiene la potestad de efectivamente realizar una operación de write es el kernel, por lo cual es indistinto si llamamos a la syscall o a algún wrapper suyo, en algún momento se llegará a una syscall write.
Es clave entonces el concepto de punto de montaje, para entender por qué funciona la persistencia a disco y su lectura, las únicas peticiones que se redirigen a nuestro proceso son las que están dentro de la carpeta utilizada como punto de montaje. Es por eso que es posible persistir la información a disco, las escrituras por fuera del punto de montaje no nos llegan a nuestra implementación.

## Serialización

Para visualizar el estado del filesystem existen *4* variables principales, estas son:
* __inode_bitmap: indica si un nodo está libre.
* __data_bitmap: indica si un bloque está libre.
* inode_table: guarda la tabla de inodos.
* data_blocks: guarda los bloques de datos, del filesystem.

Con estas variables es posible reconstruir el sistema, mediante ``fisopfs_flush``
se guardan los datos, abstrayéndolos como Bytes:

```
  static int
  fisopfs_flush(const char *path, struct fuse_file_info *fi)
  {
      printf("[debug] fisopfs_flush(%s)\n", path);
      inode_t *inode = find_inode(path, __S_IFREG);
      if (!inode)
          return -ENOENT;
  
      // save entire filesystem image
      size_t items_written;
      fseek(image, 0, SEEK_SET);
      items_written = fwrite(__inode_bitmap, sizeof(__inode_bitmap), 1, image);
      assert(items_written == 1);
      items_written = fwrite(__data_bitmap, sizeof(__data_bitmap), 1, image);
      assert(items_written == 1);
      items_written = fwrite(inode_table, sizeof(inode_table), 1, image);
      assert(items_written == 1);
      items_written = fwrite(data_blocks, sizeof(data_blocks), 1, image);
      assert(items_written == 1);
  
      fflush(image);
      return 0;
  }

```

Luego, si existe una imagen serializada del sistema, en ``fisopfs_init``
se recupera el estado de las estructuras principales, abstrayéndolos nuevamente como
Bytes, pero tomando sentido con cada tipo de datos:

```   
      items_read = fread(__inode_bitmap, sizeof(__inode_bitmap), 1, image);
	  printf("[debug] items_read: %lu\n", items_read);
	  printf("[debug] inode_bitmap[0]: %u\n", __inode_bitmap[0]);
	  
	  items_read = fread(__data_bitmap, sizeof(__data_bitmap), 1, image);
	  printf("[debug] items_read: %lu\n", items_read);
	  printf("[debug] __data_bitmap[0]: %u\n", __data_bitmap[0]);
	  
	  items_read = fread(inode_table, sizeof(inode_table), 1, image);
	  printf("[debug] items_read: %lu\n", items_read);
	  print_inode(root);
	  
	  items_read = fread(data_blocks, sizeof(data_blocks), 1, image);
	  printf("[debug] items_read: %lu\n", items_read);
	  perror("[debug] Error reading data blocks");

```

## Funcionamiento

Para verificar el correcto funcionamiento del filesystem, se sugieren una serie de pruebas, mostrando tambien el resultado esperado de las mismas:

- Probar la creación de un archivo vacío, y la correcta configuracion de las fechas de acceso y modificacion:
```
> touch archivo
> stat archivo
File: tst/archivo
  Size: 0         	Blocks: 0          IO Block: 4096   regular empty file
Device: 39h/57d	Inode: 2           Links: 1
Access: (0664/-rw-rw-r--)  Uid: ( 1000/  german)   Gid: ( 1000/  german)
Access: 2022-12-08 11:35:55.000000000 -0300
Modify: 2022-12-08 11:35:55.000000000 -0300
Change: 2022-12-08 11:35:55.000000000 -0300
 Birth: -
```

- Prueba de escritura de tamaño pequeño a un archivo:
```
> echo "Hello World!" > archivo
> cat archivo
Hello World!
```

- Prueba de escritura de tamaño moderado a un archivo:
```
> seq 5000 | xargs echo > archivo
> cat archivo
1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 ... 5000
```

- Prueba de concatenación de caracteres a un archivo (en archivos de tamaño pequeño y moderado):
```
> echo "Hello" > archivo
> echo "World!" >> archivo
> cat archivo
Hello
World!
```

- Prueba de truncamiento de archivo (en archivos de tamaño pequeño y moderado):
```
> echo "Hello" > archivo
> echo "World!" > archivo
> cat archivo
World!
```

- Prueba de borrado de archivo (en archivos de tamaño pequeño y moderado):
```
> echo "Hello" > archivo
> cat archivo
Hello
> rm archivo
> cat archivo
cat: archivo: No such file or directory
```

- Prueba apertura y listado de directorio (también con muchas entradas):
```
> echo "Hello" > archivo
> ls -la
total 5
drwxr-xr-x 1 german german    0 Dec  8 01:48 .
drwxrwxr-x 8 german german 4096 Dec  8 02:09 ..
-rw-rw-r-- 1 german german    0 Dec 31  1985 archivo
```

- Prueba de creación de directorio (también con muchas entradas):
```
> echo "Hello" > archivo
> ls -la
total 5
drwxr-xr-x 1 german german   10 Dec  8 11:26 .
drwxrwxr-x 8 german german 4096 Dec  8 11:26 ..
-rw-rw-r-- 1 german german    0 Dec  8 11:35 archivo
> mkdir directorio
> ls -la
drwxr-xr-x 1 german german   10 Dec  8 11:26 .
drwxrwxr-x 8 german german 4096 Dec  8 11:26 ..
-rw-rw-r-- 1 german german    0 Dec  8 11:35 archivo
drwxrwxr-x 1 german german    0 Dec  8 11:37 directorio
```

- Reproducir las pruebas de archivos dentro del directorio creado

- Prueba de eliminación de directorio (debe estar vacío):
```
> mkdir directorio
> ls -la
drwxr-xr-x 1 german german   10 Dec  8 11:26 .
drwxrwxr-x 8 german german 4096 Dec  8 11:26 ..
-rw-rw-r-- 1 german german    0 Dec  8 11:35 archivo
drwxrwxr-x 1 german german    0 Dec  8 11:37 directorio
> rmdir  directorio
> ls -la
drwxr-xr-x 1 german german   10 Dec  8 11:26 .
drwxrwxr-x 8 german german 4096 Dec  8 11:26 ..
-rw-rw-r-- 1 german german    0 Dec  8 11:35 archivo
```


## Testing

Para correr todas estas pruebas de forma automatizada, se realizó un script en python:
`test_fisopfs.py`. Para correrlo es necesario tener instalado python3 y
el módulo `sh`, que puede ser instalado con pip.
Los comandos necesarios para correr los test son:

```
  #para isntalar pip3: sudo apt install python3-pip -y
  pip3 install sh            
  chmod +x ./test_fisop.py
  make
  ./test_fisopfs.py
```

Al finalizar, el script le pedirá al usuario que ingrese su contraseña para desmontar el filesystem. Este paso puede omitirse usando la variable de entorno SUDO_PASSWORD de la siguiente forma:
```
export SUDO_PASSWORD=mypassword
python test_fisopfs.py
```


Se implementaron además algunas pruebas automatizadas en bash, para tener más control sobre redirecciones de la shell. El archivo es `pruebasRedirsProfYPersist.sh`, se puede ejecutar desde una shell bash en este mismo directorio luego de haber montado el filesystem en el directorio `prueba/`.

A su vez, en el archivo `PRUEBAS.md` se muestra la salida de una serie de pruebas también en bash.
