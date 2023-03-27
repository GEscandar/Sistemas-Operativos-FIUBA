# TP: malloc

## Motivación

La motivación de este TP es la de implementar funciones de la familia **malloc**
con el fin de optimizarlas, tener más control sobre la memoria y reducir tanto, 
la **fragmentación externa como interna**.

## Estructuras elegidas

Las estructuras usadas se encuentran definidas en "mem.h". En un principio, la estructura 
principal usada como header de las regiones era la siguiente:

```c
    typedef struct region {
        bool free;              // Flag de liberación de memoria, usado para evitar double free   
        size_t size;            // Tamaño real de la región devuelta por malloc
        size_t block_size;      // Tamaño del bloque al que pertenece la región
        void *block;            // Puntero al bloque al que pertenece la región
        struct region *next;    // Puntero al nodo siguiente en la lista de regiones libres
    } region_t;
```

Para distinguir regiones pertenecientes a distintos bloques bastaba con comparar si los atributos 'block'
de cada región eran iguales. Sin embargo, surge un problema en el coalescing: se dificulta unir una región
recién liberada sin conocer bien si una región adyacente a ella también fue liberada. Para resolver este
problema, una posible solución podría haber sido ordenar la lista de regiones libres ascendentemente por
su virtual address. De esta forma, solo sería necesario verificar si los nodos anterior y siguiente al liberado
son regiones adyacentes a él. A pesar de esto, se optó por una solución distinta:

```c
    typedef struct region {
        bool free;              // Flag de liberación de memoria, usado para evitar double free  
        bool has_next;          // Flag de presencia de región adyacente siguiente
        bool has_prev;          // Flag de presencia de región adyacente anterior
        bool in_use;            // Flag de asignación de memoria, usado para verificar si la región está siendo usada por el usuario
        size_t size;            // Tamaño real de la región devuelta por malloc
        size_t prev_size;       // Tamaño de la region adyacente anterior
        struct region *next;    // Puntero al nodo siguiente en la lista de regiones libres
    } region_t;
```

De esta forma, al liberar una región, siempre es posible conocer si tiene una región adyacente, y obtener dicha 
región es tan sencillo como realizar aritmética de punteros sobre la región liberada, usando ```prev_size``` para
obtener la región adyacente anterior y ```size``` para obtener la región adyacente siguiente. La principal desventaja
de este enfoque es que agrega la complejidad de tener que actualizar más información tanto al momento de asignar 
una región nueva como al momento de liberarla.

Luego de varios refactors, la estructura final usada para los headers es:

```c
    typedef struct region {
        size_t size;
        struct region *next;
        bool free;
        bool has_next;
        bool has_prev;
        size_t prev_size;
    } region_t;
```


## Primera parte

Desde el principio se diseñó la solución teniendo en mente múltiples bloques. Con la estructura de región mencionada
en la última parte del segmento anterior, la separación de bloques se logra utilizando los flags has_next y has_prev:
```
Bloque 1
---------------------------------------------------
        r0      |        r1      |        r2      |
has_next: true  |has_next: true  |has_next: false |
has_prev: false |has_prev: true  |has_prev: true  |

Bloque 2
----------------------------------
        r0      |        r1      |
has_next: true  |has_next: false |
has_prev: false |has_prev: true  |

Bloque 3
-----------------
        r0      |
has_next: false |
has_prev: false |
```
Notar que has_next no guarda relacin con region->next, sino que es un campo que se utiliza para el coalescing.
En grow_heap, al reservar un nuevo bloque, estos campos booleanos se inicializan con false, y posteriormente se van actualizando para que el coalescing funcione de manera adecuada.

### Construcción
#### Validación de parámetros
Lo primero que se hizo fue definir la región mínima de memoria que manejaría malloc, esta es:
```c
    #define MIN_REGION_SIZE 256     //  Se encuentra definida en mem.h
```

Luego se manejaron los errores por parámetros inválidos de malloc:
```c
    if (size == 0)
	    return NULL;
		
    if (size < MIN_REGION_SIZE)
	    size = MIN_REGION_SIZE;
```

Se escribió una macro MEMERROR_AND_RETURN para setear el errno y salir, en caso de falla, ya que malloc() realloc() y calloc() fallan de la misma forma. Se optó por una macro y no una función por simplicidad ya que es muy mínima en cantidad de líneas de código y hace ella un return NULL.

#### Alineamiento

En esta etapa también se hace un alineamiento del tamaño pedido, ya que se asigna 
una variable cada 4 bytes, y por ende puede necesitarse un tamaño físico mayor en memoria 
para guardar variables.

#### Primer llamado

Luego de validar los parámetros, se hace una búsqueda de regiones libres, durante el primer 
llamado no hay una región en la que buscar, entonces, se mapea un nuevo bloque de memoria:

```c
    region_t next = find_free_region(size);
    
    if (!next){
        next = grow_heap(size);
    }
    
    if (!next) {        //  Falla cuando no hay memoria disponible
	errno = ENOMEM;
	return NULL;
    }
```

#### Memoria dinámica

La memoria dinámica se pide con:

```c
    region_t *curr = (region_t *) mmap(NULL,
	                 sizeof(region_t) + size,
	                 PROT_READ | PROT_WRITE,
	                 MAP_PRIVATE | MAP_ANONYMOUS,
	                 -1,
	                  0);
```
En donde:

```bash
    NULL : indica que la primera direción de memoria la decide el kernel
    sizeof(region_t) + size : indica el tamaño real de memoria necesaria (recordar que size está alineado)
    PROT_READ | PROT_WRITE : indican que en la región de memoria se autorizan operaciones de lectura y escritura
    MAP_PRIVATE | MAP_ANONYMUS : MAP_PRIVATE indica que los cambios que se hagan en la región de memoria son privados, por lo tanto sólo será visible por los procesos que la llamen.
                                 MAP_ANONYMOUS indica que los contenidos de la región se inicializan en "0", además la región devueta por mmap(), no se respaldan con ningún archivo
    -1 : el parámetro se usa para indicar fds, en este caso "-1" es un requisito de MAP_ANONYMOUS
    0 : indica, en bytes, las porciones de memoria que representarán un elemento (el tamaño del tipo de dato), debe ser "0" por requisitos de MAP_ANONYMOUS
```

Una vez reservado el bloque, que es para nuestro tp una región libre a insertar con tamaño igual al tamao de todo el bloque, se inicializa el header de la misma, y se devuelve un puntero a su header.

### Splitting

El nuevo bloque se coloca al principio de la lista de regiones libres solo si el tamaño solicitado por
el usuario permite que se haga un split del mismo, es decir, si el tamaño restante de realizar el split 
sería suficiente para satisfacer una solicitud mínima de memoria. Si se cumple esta condición, se lleva
a cabo el split, que consiste simplemente en operar con arítmetica de punteros usando la región libre,
separando de ella una región nueva de tamaño suficiente para satisfacer la solicitud recibida.

```c
    size_t excess = next->size - (sizeof(region_t) + size);  // allocated - requested
    
    region_t *splitted = REGIONOFFSET(next, excess);
    splitted->size = size;
    splitted->next = NULL;
    splitted->free = false;
    splitted->has_next = next->has_next;
    splitted->has_prev = true;
    splitted->prev_size = excess;
    splitted->in_use = true;
```

Donde ```REGIONOFFSET``` es una macro que tiene la siguiente 
definicion:

```c
#define REGIONOFFSET(ptr, offset)                                              \
	((region_t *) ((char *) REGION2PTR(ptr) + offset))
```

Por último se devuelve un puntero a la región (la primera dirección luego del header) 
que es la que se usará como memoria dinámica.


Como se puede observar en el código, nuestra implementacin funciona de la siguiente manera: si nos solicitan con malloc un tamaño menor al que tiene nuestro nodo encontrado luego de buscarlo en la lista, la función hará un split en el cual quedará el tamaño solicitado por malloc 'al final' del nodo encontrado, y el nodo encontrado quedará con el 'exceso'.

Notar que esta función crea un nuevo nodo (en nuestra implementacin el 'splitted') *dentro* de la lista de libres. Esto es, luego de actualizar el tamaño de nuestra regin ya existente, no alcanza con definir una nueva variable como region_t y setearle los atributos apropiados, sino que dicha nueva región debe tener una direccin de memoria perteneciente a la regin asignada previamente por mmap o de lo contrario no persistiría luego del final de esta función y su posterior uso resultaría en un segmentation fault. Para lograr esto, la macro mencionada se encarga de sumar el size de la regin ya existente a su inicio, aritmética de punteros mediante, ya que allí vivir este nuevo nodo.

Es decir, se actualiza el tamaño actual del nodo a ser splitteado, se crea uno nuevo que tendrá otro tamaño (en nuestra implementacin, el nodo recibido

**Con esta técnica se reduce la fragmentación interna**.

## Free y Coalescing

#### Casos borde de free

Si se intenta hacer un ```free``` de NULL, o de un puntero que no haya sido asignado 
por malloc, calloc, o realloc, o si se intenta hacer free 2 veces al  mismo puntero,
son todos casos que se manejan en esta línea. 

```c
    if (ptr == NULL || !is_valid_pointer(ptr))
		return;
```

Donde la función ```is_valid_pointer``` usa información del header para validar la 
operación:
```c
bool
is_valid_pointer(void *ptr)
{
    region_t *region = PTR2REGION(ptr);
    return region->in_use && region->size >= MIN_REGION_SIZE &&
        !region->free && (region->has_next || region->has_prev);
}
```

Luego, liberar una región consiste en setear los siguientes flags, e insertar la 
región libre en la lista de regiones libres:

```c
    reg->free = true;
    reg->in_use = false;
```

Si luego de realizar el coalescing (descrito más abajo), la región resultante 
representa un bloque entero, se desmapea la memoria del bloque, pues ya no se 
está usando ninguna de sus regiones:

```c
if (!curr->has_prev && !curr->has_next) {
    // this region represents the entire free block
    logfmt("Unmapping block...\n");
    unlink_region(curr);
    munmap(curr, curr->size + sizeof(region_t));
    return;
}
```

#### Coalescing

Se busca unir la región liberada con alguna región adyacente a ella que también
esté libre, para no llenar la lista de nodos con poca capacidad, lo cual empeoraría
la búsqueda de una región libre.
</br></br>
Primero se inserta la región liberada en la lista de regiones libres.

```c
    append_region(reg);
    region_t *curr = coalesce(reg);
```

Luego, en la función ```coalesce```, se procede a hacer la unión de la región liberada
con alguna de sus regiones adyacentes, si se pudiera: Si la región adyacente 
siguiente también está libre, se une con la liberada de la siguiente forma:
```c
unlink_region(next_region);             // quitar region de la lista de regiones libres
curr->has_next = next_region->has_next; // actualizar atributo has_next de la región liberada
curr->size += next_region->size + sizeof(region_t); // actualizar tamaño de la región liberada

if (curr->has_next) {
    next_region = REGIONOFFSET(curr, curr->size);
    next_region->prev_size = curr->size; // actualizar atributo prev_size de la nueva región adyacente siguiente 
}
```

Para unir con la región anterior en caso de ser necesario, el método es similar:
```c
prev_region->has_next = curr->has_next; // actualizar atributo has_next de la región adyacente anterior
prev_region->size += curr->size + sizeof(region_t); // actualizar tamaño de la región adyacente anterior
curr = prev_region;

// update neighbour
if (curr->has_next) {
    next_region = REGIONOFFSET(curr, curr->size);
    next_region->prev_size = curr->size; // actualizar atributo prev_size de la nueva región adyacente siguiente 
}
```
**Esta técnica evita la fragmentación externa**

### Búsqueda de regiones libres

Para la primera parte se implementó first-fit:

```c
    region_t *next = region_free_list;
    size_t target_size = sizeof(region_t) + size;
	
    while (next && next->size < target_size) {
	    next = next->next;
    }
```

## Segunda parte

<!-- Para la segunda parte se extendió el soporte para poder administrar varios bloques de memoria, 
en donde cada bloque administra sus regiones de memoria, para esto se cambió la estructura del header a:

```c
    typedef struct region {
        bool free;
        bool has_next;
        bool has_prev;
        bool in_use;
        size_t size;
        size_t prev_size;
        size_t block_size;
        void *block;
        struct region *next;    
    } region_t;
```

Agregando las nuevas variables:

```c
    size_t block_size;      // Tamaño del bloque al que pertenece la región
    void *block;            // Puntero al bloque al que pertenece la región
```

Para administrar los bloques se usó la misma lógica que para las regiones, cada bloque tiene 
un puntero al siguiente bloque y se sabe cuál es su tamaño. -->

### Bloques de distinto tamaño

Los tamaños de los bloques están definidos en "mem.h", estos son:

```c
    enum block_size {
        BLOCK_SMALL = 16*1024,
        BLOCK_MEDIUM = 1024*1024,
        BLOCK_BIG = 32*BLOCK_MEDIUM
    };
```

La aplicación para crear los distintos tamaños fue fácil, de crear un bloque de esta manera:

```c
    region_t next = find_free_region(size);
    
    if (!next){
        next = grow_heap(size);
    }
```

Se pasó a la de los bloques:

```c
    if (size < BLOCK_SMALL)
        next = grow_heap(BLOCK_SMALL);
    else if (size < BLOCK_MEDIUM)
        next = grow_heap(BLOCK_MEDIUM);
    else if (size <= BLOCK_BIG)
        next = grow_heap(BLOCK_BIG);
```

### Split y Coalescing

Para esta segunda parte, no se implementaron cambios.

## Tercera parte

### Mejora de la búsqueda de regiones libres
En la tercera parte se agregó el método de búsqueda de regiones libres, agregando la posibilidad de implementar 
**Best-Fit**:

```c
    region_t *next = region_free_list;
    size_t target_size = sizeof(region_t) + size;
    region_t *best_fit = NULL;
        
    while (next) {
	    if (next->size >= target_size &&
		   (!best_fit || next->size < best_fit->size))
		        best_fit = next;
	    next = next->next;
    }
	
    if (best_fit)
	    next = best_fit;
```

En donde, ya no se devuelve la primer región libre que se encuentra, sino que, se devuelve la del 
tamaño más ajustable a la región pedida, "que mejor encaja".

### Calloc

La implementación de calloc no fue difícil, ya que la mayoría de lo necesario, ya estaba implementado en ```malloc()```
Al igual que antes lo primero en hacerse, fue la validación de parámetros:

```c
    if ((nmemb == 0) || (size == 0))
	    return NULL;    
```

Luego se agregó la condición para evitar un **int overflow**:

```c
    size_t regionSize = nmemb * size;
	
    if (regionSize >= INT_MAX) {
    	errno = ENOMEM;
    	return NULL;
    }
```

En donde ```nmemb``` es la cantidad de posiciones que tendrá el vector pedido, y ```size``` es 
la cantidad de bytes que tendrá cada posición del vector, o dicho de otra forma, el tamaño de su tipo de datos.

La asignación de memoria se hace llamando a malloc:

```c
    void *initial = malloc(regionSize);
    if (!initial)
	    return NULL;
```
Por último la inicialización del contenido se realiza así:

```c
    for (size_t i = 0; i < nmemb; ++i) {
        initialize_cell(initial + i, size);
    }
```

## Cuarta Parte

### Realloc

Nuevamente, lo primero en realizarse fue la validación de parámetros:

```c
    if (!ptr)
	    return malloc(size);    //  Acá se demuestra que, realloc(NULL, size) = malloc(size) 
    else if (size == 0) {           //  Si size es "0" -> realloc(ptr, 0) = free(ptr)
	    free(ptr);
	    return NULL;
    }
```

Se obtiene el header de la región pasada y se valida que sea menor al pasado por parámetro, de lo contrario
se devuelve la región indicada, sin modificarla:

```c
    region_t *reg = PTR2REGION(ptr);
    if (reg->size >= size)
	    return ptr;

    void *newptr = malloc(size);  
```
Se guarda la nueva región en un nuevo puntero, para no comprometer a la región actual;
Por último se copia el contenido y se libera la vieja región:

```c
    memcpy(newptr, ptr, reg->size);
    free(ptr);
```

Para determinar que la región pasada pertenece al bloque administrado por ```malloc()```,
se utilizó la (previamente mencionada) función ```is_valid_pointer```, que realiza todas las
verificaciónes necesarias sobre el header de la región.

## Extras

Para monitorear el comportamiento de la librería y su administración con la memoria, se usaron tanto
la función indicada ```prinfmt()```, como creada: ```logfmt()```, que se encuentra en "printfmt.c", 
esta función tiene el objetivo de volcar ciertas acciones que realiza malloc en un archivo .log, que se
crea, o reutiliza en la carpeta actual.






