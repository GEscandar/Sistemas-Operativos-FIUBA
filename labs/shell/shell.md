# Lab: shell

### Búsqueda en $PATH

¿cuáles son las diferencias entre la syscall execve(2) y la familia de wrappers proporcionados por la librería estándar de C (libc) exec(3)?

Los wrappers de la librería estándar de C son simplemente 'frontends' para la syscall `execve`. Es decir, lo único que hacen
es llamar a `execve`, procesando sus argumentos de distinta forma y/o realizando algún manejo de errores adicional. Según el manual, execve es la syscall
encargada de ejecutar el programa que reciba en su argumento pathname, reemplazando la imagen del proceso en ejecución por un nuevo programa, inicializando 
stack, heap y data.

Las funciones exec con una p en su nombre, cuando reciben un argumento pathname que no comienza con '/', buscan la ruta completa de pathname en la variable PATH, para 
luego pasarsela a execve.

Las funciones exec con una e al final de su nombre permiten pasar el parámetro envp, que representa el entorno que tendrá el nuevo programa. Todas las otras funciones
de la familia de exec (las que no terminan con una e) pasan a `execve` el entorno completo del proceso en ejecución, usando la variable `environ`.

Las funciones de exec con una l en su nombre, en vez de recibir un array que representa los argumentos del programa a ejecutar, reciben a todos estos argumentos como parámetro,
requiriendo pasar un parámetro como NULL para marcar el final de los argumentos a pasar. Luego, estas funciones deberán armar internamente el mencionado array para pasárselo a execve.

¿Puede la llamada a exec(3) fallar? ¿Cómo se comporta la implementación de la shell en ese caso?

Si que puede fallar, y todos los casos de error están documentados en la página del manual de execve(). 
En caso de error, la implementación de la shell llama a la syscall _exit() pera terminar el proceso hijo
de la shell inmediatamente.

### Comandos built-in

¿Entre cd y pwd, alguno de los dos se podría implementar sin necesidad de ser built-in? ¿Por qué? ¿Si la respuesta es sí, cuál es el motivo, entonces, de hacerlo como built-in? (para esta última pregunta pensar en los built-in como true y false)

El comando `cd` no podría ser implementado como un programa aparte, porque al ejecutarse desde la shell correría en un proceso nuevo y cambiaría el directorio en ese proceso y, al finalizar, la shell no habría
cambiado de directorio. El comando `pwd`, en cambio, podría ser implementado perfectamente sin necesidad de ser un built-in, porque lo único que hace es mostrar el directorio actual del proceso que lo llama; cuando
se llama a `fork`, el proceso hijo tiene el mismo working directory que el proceso padre, entonces la salida del comando no cambia, sea o no un comando built-in.

De hecho, en bash, además del comando interno `pwd` puede usarse el comando externo `/bin/pwd`, que es el comando descrito al ejecutar `man pwd`. Esto es así porque POSIX requiere que las *standard utilities* sean implementadas de forma tal que puedan ser accesibles por la familia de funciones de *exec*. Además, se especifica que algunas de estas *utilities* suelen implementarse como builtins por cuestiones de performance, o porque de esa forma logran implementar funcionalidad que sería más difícil de lograr en un entorno separado (https://pubs.opengroup.org/onlinepubs/009695399/utilities/xcu_chap01.html#tagtcjh_5).


### Variables de entorno adicionales

¿Por qué es necesario hacerlo luego de la llamada a fork(2)?

Porque si se setean las variables de entorno en el proceso de la shell, podríamos modificar erróneamente una variable de entorno usada por la shell. Además, como los procesos hijos heredan las variables
de entorno del proceso padre, todos los comandos a ejecutar en el futuro tendrán la variable seteada, aunque no lo hayan hecho.


En algunos de los wrappers de la familia de funciones de exec(3) (las que finalizan con la letra e), se les puede pasar un tercer argumento (o una lista de argumentos dependiendo del caso), con nuevas variables de entorno para la ejecución de ese proceso. Supongamos, entonces, que en vez de utilizar setenv(3) por cada una de las variables, se guardan en un array y se lo coloca en el tercer argumento de una de las funciones de exec(3).

¿El comportamiento resultante es el mismo que en el primer caso? Explicar qué sucede y por qué.
Describir brevemente (sin implementar) una posible implementación para que el comportamiento sea el mismo.

Si se usa una de las funciones de la familia de exec que finalizan con la letra e y se les pasa un array con variables de entorno, el comportamiento del programa no es el mismo que al usar el resto de los wrappers,
ya que en este caso el proceso en ejecución solamente contará con las variables de entorno que proporcionemos en el parámetro envp, y no podrá "ver" las variables de entorno de la shell. Para poder lograr esto último, será necesario pasar, además de las variables de entorno presentes en la línea ingresada por el usuario, las variables de entorno del proceso de la shell.

En POSIX, esto puede lograrse leyendo la variable global `environ`, que es un array dinámico de strings "ENVVAR=VALUE":
```
extern char **environ;
```

Entonces, bastaría con crear un array dinámico nuevo, y agregarle los contenidos de `environ` y los de las asignaciones realizadas por el usuario. Luego, se llamaría a execvpe, pasando dicho array en el parámetro envp.

### Procesos en segundo plano

Los procesos en segundo plano se implementaron usando waitpid con la opcion WNOHANG en las primeras líneas de la función
runcmd, para que se ejecute ni bien se parsea la línea ingresada por el usuario. De esta forma, se logra esperar oportunamente
a la finalización de cualquier proceso que aún siga corriendo en segundo plano. Luego, para poder mostrar por pantalla la información
del proceso que finaliza, fue necesario guardar los punteros a cmd pertenecientes a una ejecución en segundo plano en un
array fijo (para no complejizar la implementación), para luego reobtener el elemento guardado usando el resultado de waitpid,
que (si es que se ejecutó sin errores) corresponde al pid del proceso finalizado. 

### Flujo estándar

Investigar el significado de 2>&1, explicar cómo funciona su forma general y mostrar qué sucede con la salida de cat out.txt en el ejemplo. Luego repetirlo invertiendo el orden de las redirecciones. ¿Cambió algo?

La redirección 2>&1 se traduce a "redireccionar la salida estandar de error al mismo archivo en el cual se esté escribiendo la salida estandar". En su forma general, el operador > redirecciona la salida del file
descriptor asociado al numero que lo precede (o STDOUT, si no se provee ninguno) al mismo archivo en el cual se esté escribiendo la salida del file descriptor asociado al número que se escribe despues del caracter
'&'. En el ejemplo, primero se redirige la salida estandar al archivo out.txt y luego se redirige la salida estándar de error "al mismo archivo en el cual se esté escribiendo la salida estandar", es decir, a out.txt.

```
 (/home/german) 
$ ls -C /home /noexiste >out.txt 2>&1
        Program: [ls -C /home /noexiste >out.txt 2>&1] exited, status: 2 
 (/home/german) 
$ cat out.txt
ls: cannot access '/noexiste': No such file or directory
/home:
german
        Program: [cat out.txt] exited, status: 0
```

Si se invierte el orden de las redirecciones, lo que pasará es que con la redirección 2>&1, la salida de error se redigirá "al mismo archivo en el cual se esté escribiendo la salida estandar", es decir, a la terminal, y luego se redigirá la salida estándar al archivo out.txt.

```
 (/home/german) 
$ ls -C /home /noexiste 2>&1 >out.txt
ls: cannot access '/noexiste': No such file or directory
        Program: [ls -C /home /noexiste 2>&1 >out.txt] exited, status: 2 
 (/home/german) 
$ cat out.txt
/home:
german
```

### Tuberías simples (pipes)

Investigar qué ocurre con el exit code reportado por la shell si se ejecuta un pipe ¿Cambia en algo? ¿Qué ocurre si, en un pipe, alguno de los comandos falla? Mostrar evidencia (e.g. salidas de terminal) de este comportamiento usando bash. Comparar con la implementación del este lab.

Al ejecutar un pipe en bash, el status del comando completo es siempre el status del último comando del pipe. En este lab se intentó emular este comportamiento, agregando a la sección de PIPE de execcmd llamados
a la funcion print_status_info, que es la encargada de convertir el status recibido por waitpid al status real del proceso. Para comprobar esto, se muestran la salida de la terminal y de este lab con algunos 
comandos de prueba.

Salida de bash:
```
german@german-mint20:~/Desktop/Facultad/SisOp/sisop_2022b_escandar/shell$ ls /noexiste | grep Doc
ls: cannot access '/noexiste': No such file or directory
german@german-mint20:~/Desktop/Facultad/SisOp/sisop_2022b_escandar/shell$ echo $?
1
```

Salida de este lab:
```
 (/home/german) 
$ ls /noexiste | grep Doc
ls: cannot access '/noexiste': No such file or directory
        Program: [ls /noexiste ] exited, status: 2 
        Program: [grep Doc] exited, status: 1 
        Program: [] exited, status: 1 
 (/home/german) 
$ echo $?
1
        Program: [echo $?] exited, status: 0 
```

### Pseudo-variables

Investigar al menos otras tres variables mágicas estándar, y describir su propósito. Incluir un ejemplo de su uso en bash (u otra terminal similar).

- `$0 - $99` x-ésimo argumento proporcionado al script/función. $0 es siempre el nombre del script, incluso cuando se accede desde dentro de una función. $1+ funcionará en funciones.

- `$@` Todos los argumentos proporcionados al script/función (accesible dentro del script/función).

- `$#` Cantidad de argumentos proporcionados al script/función (accesible dentro del script/función).

- `$_` Último argumento del ultimo comando ejecutado.

```
german@german-mint20:~/Desktop/Facultad/SisOp/sisop_2022b_escandar/shell$ echo hola como estas
hola como estas
german@german-mint20:~/Desktop/Facultad/SisOp/sisop_2022b_escandar/shell$ echo $_
estas
```

- `$!` PID del último proceso ejecutado en segundo plano.

```
german@german-mint20:~/Desktop/Facultad/SisOp/sisop_2022b_escandar/shell$ ps &
[1] 16400
german@german-mint20:~/Desktop/Facultad/SisOp/sisop_2022b_escandar/shell$     PID TTY          TIME CMD
   2683 pts/0    00:00:00 bash
  16400 pts/0    00:00:00 ps

[1]+  Done                    ps
german@german-mint20:~/Desktop/Facultad/SisOp/sisop_2022b_escandar/shell$ echo $!
16400
```

- `$$` PID del proceso actual.

```
german@german-mint20:~/Desktop/Facultad/SisOp/sisop_2022b_escandar/shell$ ps
    PID TTY          TIME CMD
   2683 pts/0    00:00:00 bash
  15317 pts/0    00:00:00 ps
german@german-mint20:~/Desktop/Facultad/SisOp/sisop_2022b_escandar/shell$ echo $$
2683
german@german-mint20:~/Desktop/Facultad/SisOp/sisop_2022b_escandar/shell$ sh
$ ps
    PID TTY          TIME CMD
   2683 pts/0    00:00:00 bash
  17557 pts/0    00:00:00 sh
  17576 pts/0    00:00:00 ps
$ echo $$
17557
$ exit
```


### Desafío (historial)

¿Cuál es la función de los parámetros MIN y TIME del modo no canónico? ¿Qué se logra en el ejemplo dado al establecer a MIN en 1 y a TIME en 0?

El parámetro MIN establece la cantidad mínima de bytes que deben haberse escrito a la entrada para que una llamada a read vuelva, y TIME es el timeout de las llamadas a read, es decir, si despues del tiempo especificado, aún no se ha leído nada, la llamada a read termina. Lo que se logra al establecer MIN en 1 y TIME en 0 es que todas las llamadas a read terminen inmediatamente (es decir, que no sean bloqueantes), sin importar si se pudo leer algo o no.