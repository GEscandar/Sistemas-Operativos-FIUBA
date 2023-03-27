# sched.md

Lugar para respuestas en prosa, seguimientos con GDB y documentacion del TP

# Seguimiento con gdb: context_switch, kernel mode --> user mode.
En la función context_switch, se envía a correr a un proceso. Esto involucra volcar el contenido de sus registros almacenados en envs[0].env_tf (en este ejemplo es el primer proceso) y cambiar el nivel de privilegio (pasar de ring0 a ring3).

```
make gdb
gdb -q -s obj/kern/kernel -ex 'target remote 127.0.0.1:26000' -n -x .gdbinit
Leyendo símbolos desde obj/kern/kernel...hecho.
Remote debugging using 127.0.0.1:26000
aviso: No executable has been specified and target does not support
determining executable automatically.  Try using the "file" command.
0x0000fff0 in ?? ()
(gdb) b context_switch 
Punto de interrupción 1 at 0xf010435d: file kern/switch.S, line 14.
(gdb) c
Continuando.
Se asume que la arquitectura objetivo es i386
=> 0xf010435d <context_switch>:	mov    0x4(%esp),%esp

Breakpoint 1, context_switch () at kern/switch.S:14
14		movl 0x4(%esp),%esp
```

Primero vemos el estado del stack y de los registros al momento de llegar a la función context_switch:

```
(gdb) p envs[0].env_tf
$1 = {tf_regs = {reg_edi = 0, reg_esi = 0, reg_ebp = 0, reg_oesp = 0, 
    reg_ebx = 0, reg_edx = 0, reg_ecx = 0, reg_eax = 0}, tf_es = 35, 
  tf_padding1 = 0, tf_ds = 35, tf_padding2 = 0, tf_trapno = 0, tf_err = 0, 
  tf_eip = 8388640, tf_cs = 27, tf_padding3 = 0, tf_eflags = 512, 
  tf_esp = 4005552128, tf_ss = 35, tf_padding4 = 0}
(gdb) p/x envs[0].env_tf
$2 = {tf_regs = {reg_edi = 0x0, reg_esi = 0x0, reg_ebp = 0x0, reg_oesp = 0x0, 
    reg_ebx = 0x0, reg_edx = 0x0, reg_ecx = 0x0, reg_eax = 0x0}, tf_es = 0x23, 
  tf_padding1 = 0x0, tf_ds = 0x23, tf_padding2 = 0x0, tf_trapno = 0x0, 
  tf_err = 0x0, tf_eip = 0x800020, tf_cs = 0x1b, tf_padding3 = 0x0, 
  tf_eflags = 0x200, tf_esp = 0xeebfe000, tf_ss = 0x23, tf_padding4 = 0x0}

(gdb) x/2x $esp
0xf011ffbc:	0xf0103626	0xf02c9000
^ La primera de estas dos, debería ser la dirección de retorno que carga call al stack,
la segunda nuestro puntero al struct trapframe

(gdb) p *(struct Trapframe *) 0xf02c9000
$3 = {tf_regs = {reg_edi = 0, reg_esi = 0, reg_ebp = 0, reg_oesp = 0, 
    reg_ebx = 0, reg_edx = 0, reg_ecx = 0, reg_eax = 0}, tf_es = 35, 
  tf_padding1 = 0, tf_ds = 35, tf_padding2 = 0, tf_trapno = 0, tf_err = 0, 
  tf_eip = 8388640, tf_cs = 27, tf_padding3 = 0, tf_eflags = 512, 
  tf_esp = 4005552128, tf_ss = 35, tf_padding4 = 0}
(gdb) p/x *(struct Trapframe *) 0xf02c9000
$4 = {tf_regs = {reg_edi = 0x0, reg_esi = 0x0, reg_ebp = 0x0, reg_oesp = 0x0, 
    reg_ebx = 0x0, reg_edx = 0x0, reg_ecx = 0x0, reg_eax = 0x0}, tf_es = 0x23, 
  tf_padding1 = 0x0, tf_ds = 0x23, tf_padding2 = 0x0, tf_trapno = 0x0, 
  tf_err = 0x0, tf_eip = 0x800020, tf_cs = 0x1b, tf_padding3 = 0x0, 
  tf_eflags = 0x200, tf_esp = 0xeebfe000, tf_ss = 0x23, tf_padding4 = 0x0}

  "Vemos que el contenido de esa dir, es efectivamente lo que esperábamos, coincide con el print de envs[0].env_tf".
  "Ahora vemos el estado de los registros (que todavía no tienen lo que queremos que tengan, que es el contenido de los registros de nuestro proceso de user):"
  
(gdb) info registers
eax            0x0	0
ecx            0xf01213c0	-267250752
edx            0xef803000	-276811776
ebx            0xf02c9000	-265515008
esp            0xf011ffbc	0xf011ffbc
ebp            0xf011ffd8	0xf011ffd8
esi            0x0	0
edi            0x289000	2658304
eip            0xf010435d	0xf010435d <context_switch>
eflags         0x46	[ PF ZF ]
cs             0x8	8
ss             0x10	16
ds             0x10	16
es             0x10	16
fs             0x23	35
gs             0x23	35
```

Vemos que el cs = 0x8, 8 en decimal, 1000 en binario, sus últimos dos bits son 00, correspondientes al ring0, estamos en modo kernel.


Avanzamos en la función, para ver cómo quedó el contenido de los registros luego de llamar a iret:

```
(gdb) n
=> 0xf0104361 <context_switch+4>:	popa   
context_switch () at kern/switch.S:18
18		popal			// pop all general purpose registers except esp (eax, ebx, ecx, edx, ebp, esi, edi)
(gdb) n
=> 0xf0104362 <context_switch+5>:	pop    %es
context_switch () at kern/switch.S:19
19		popl %es		// pop extra segment
(gdb) n
=> 0xf0104363 <context_switch+6>:	pop    %ds
20		popl %ds		// pop data segment
(gdb) n
=> 0xf0104364 <context_switch+7>:	add    $0x8,%esp
21		addl $0x8,%esp	// move the stack pointer 8 bytes: 4 for tf_trapno and 4 for tf_err (to ignore them)
(gdb) n
=> 0xf0104367 <context_switch+10>:	iret   
22		iret			// load ss,cs,eflags and eip of the new process
(gdb) n
=> 0x800020:	cmp    $0xeebfe000,%esp
0x00800020 in ?? ()

(gdb) info registers
eax            0x0	0
ecx            0x0	0
edx            0x0	0
ebx            0x0	0
esp            0xeebfe000	0xeebfe000
ebp            0x0	0x0
esi            0x0	0
edi            0x0	0
eip            0x800020	0x800020
eflags         0x202	[ IF ]
cs             0x1b	27
ss             0x23	35
ds             0x23	35
es             0x23	35
fs             0x23	35
gs             0x23	35
```

Coincide con el print inicial de envs[0].env_tf. Vemos que se cargaron exitosamente los contenidos de nuestros registros del proceso de modo usuario a los registros del cpu.


Tambien notamos que el cs vale 27 en decimal, lo cual termina en 11 en binario, estamos ahora en ring3, modo user.

---

# Seguimiento gdb: _alltraps, user mode --> kernel mode
Continuando el seguimiento con gdb, llegamos a la función _alltraps.

```
(gdb) b _alltraps 
Punto de interrupción 2 at 0xf0104354: file kern/trapentry.S, line 85.
(gdb) c
Continuando.
=> 0xf0104354 <_alltraps>:	push   %ds

Breakpoint 2, _alltraps () at kern/trapentry.S:85
85		pushl %ds
(gdb) disas
Dump of assembler code for function _alltraps:
=> 0xf0104354 <+0>:	push   %ds
   0xf0104355 <+1>:	push   %es
   0xf0104356 <+2>:	pusha  
   0xf0104357 <+3>:	push   %esp
   0xf0104358 <+4>:	call   0xf010418c <trap>
End of assembler dump.
(gdb) n
=> 0xf0104355 <_alltraps+1>:	push   %es
_alltraps () at kern/trapentry.S:86
86		pushl %es
(gdb) n
=> 0xf0104356 <_alltraps+2>:	pusha  
_alltraps () at kern/trapentry.S:87
87		pushal
(gdb) n
=> 0xf0104357 <_alltraps+3>:	push   %esp
_alltraps () at kern/trapentry.S:90
90		pushl %esp
(gdb) n
=> 0xf0104358 <_alltraps+4>:	call   0xf010418c <trap>
_alltraps () at kern/trapentry.S:92
92		call trap
```

Analizamos lo que quedó en el stack, lo comparamos con los registros del cpu:

```
(gdb) x/3x $esp
0xefffffb8:	0xefffffbc	0x00000000	0x00000000
"^ De izquierda a derecha, esperamos: nuestro puntero a struct Trapframe, el %es, y el %ds.
Esperamos poder leer correctamente el último elemento del stack (es decir 0xefffffbc) como el puntero a
struct Trapframe, porque se fueron pusheando todos sus componentes en el orden apropiado, siendo el último
push necesario para conformarlo el de los registros de propósito general (pushal)".

(gdb) p/x *(struct Trapframe *) 0xefffffbc
$5 = {tf_regs = {reg_edi = 0x0, reg_esi = 0x0, reg_ebp = 0xeebfdfc0, reg_oesp = 0xefffffdc, reg_ebx = 0x0, 
    reg_edx = 0x0, reg_ecx = 0x0, reg_eax = 0x2}, tf_es = 0x23, tf_padding1 = 0x0, tf_ds = 0x23, 
  tf_padding2 = 0x0, tf_trapno = 0x30, tf_err = 0x0, tf_eip = 0x800a0f, tf_cs = 0x1b, tf_padding3 = 0x0, 
  tf_eflags = 0x292, tf_esp = 0xeebfdf98, tf_ss = 0x23, tf_padding4 = 0x0}
(gdb) info registers
eax            0x2	2
ecx            0x0	0
edx            0x0	0
ebx            0x0	0
esp            0xefffffb8	0xefffffb8
ebp            0xeebfdfc0	0xeebfdfc0
esi            0x0	0
edi            0x0	0
eip            0xf0104358	0xf0104358 <_alltraps+4>
eflags         0x92	[ AF SF ]
cs             0x8	8
ss             0x10	16
ds             0x23	35
es             0x23	35
fs             0x23	35
gs             0x23	35
```

Vemos que edi esi ebp ebx edx ecx eax pusheados, es decir, los que leemos de reg_eax, reg_ebx, etc, coinciden con los valores de los registros de propósito general que están en nuestro cpu; lo mismo ocurre para %es y %ds. Hemos casteado a puntero a struct trapframe la dirección de memoria que quedó en el stack luego de hacer los push, el hecho de que se haya interpretado correctamente los valores de cada campo (que coincidan con los del cpu) quiere decir que hemos construido correctamente el struct con este orden de instrucciones push.

Vemos además que eip cs eflags esp ss no coinciden, pero no los estamos pusheando en esta función desde nuestros registros del cpu sino que dichos valores se cargan antes, ya los recibimos cargados y nuestra tarea es terminar de cargar %ds %es y los de propósito general.
(Vemos que tf_trapno es 0x30 es decir 48, que es de las user defined según intel y es destinada a las syscalls según vemos en trap.c, y la macro llamada pushea un 0 que es el que vemos en el print casteado en tf_err).

Notamos también que los últimos dos bits del %cs volvieron a cambiar para indicar el cambio de nivel de privilegios: vale cs = 0x8 nuevamente, 8 en decimal, 1000 en binario, estamos de nuevo en modo kernel. (La función _alltraps está implementada en kern/trapentry.S, en cualquier instrucción de ella ya nos encontramos en modo kernel).

Obs: en este caso la comparación no es contra lo almacenado en envs[0], de hecho los registros no coinciden con envs[0]. Esto es porque luego del context switch hacia el proceso de user, corre el proceso en modo usuario utilizando los registros del cpu (es decir, no lo almacenado sino los registros de cpu en hardware) con lo que lo almacenado en envs[0] queda desactualizado hasta que se vuelvan a guardar en este context switch hacia modo kernel. (Vemos en el código que la función trap hace 'curenv->env_tf = *tf;', donde el *tf es el parámetro recibido y sabemos que la convención de pasaje de parámetros en C es por stack, con lo cual está guardando estos registros que le estamos pusheando en _alltraps).

---
# Scheduling round robin
Para implementar scheduling round-robin, simplemente iteramos sobre el vector estático que contiene todos los procesos del sistema (partiendo desde el índice del elemento correspondiente a ```curenv```) y ejecutamos el primero que tenga estado ENV_RUNNABLE:
```c
size_t env_index = 0;
if (curenv){
  env_index = ENVX(curenv->env_id);
  assert(curenv->env_id == envs[env_index].env_id);
}

for (int i = env_index; i < NENV; i++){
  if (envs[i].env_status == ENV_RUNNABLE) {
    env_run(envs+i);
  }
}
for (int i = 0; i < env_index; i++){
  if (envs[i].env_status == ENV_RUNNABLE) {
    env_run(envs+i);
  }
}
```
Luego, si el único proceso para ejecutar en el sistema es ```curenv```, se lo continúa ejecutando:
```c
// If there are no other runnable envs and curenv
// has not finished, keep running it
if (curenv && curenv->env_status == ENV_RUNNING) {
  env_run(curenv);
}
```


# Scheduling con prioridades
Se optó por implementar MLFQ como algoritmo de scheduling, usando 5 niveles de prioridad y arrays estáticos para las colas. La prioridad va entonces de 0 a 4, siendo 0 la mejor y 4 la peor. El principal desafío de este desarrollo fue encontrar una forma de contar el tiempo de ejecución de un proceso para determinar cuando cambiar su bajar su prioridad; una vez hecho esto, el resto de la implementación se guía por las reglas de MLFQ. 

Para realizar la medida mencionada, usamos el contador del timer del procesador, que cuenta para abajo desde un valor constante y se resetea periodicamente. Esto se ve mejor en los siguientes snippets de codigo:

- Constantes
```c
#define SCHED_TIME_SLICE 1000000   // Amount of timer cycles that constitute a time slice
#define TIMER_INITIAL_COUNT 10000000 // Timer cycles initial count (from lapic.c)
```

- Dentro de cada proceso
```c
struct Env {
  ...
  uint32_t env_priority;		// Scheduling priority of the environment
  uint32_t env_ran_for;		// The amount of timer cycles this process ran for
  ...
}
```

- Guardado del valor actual del contador antes de ejecutar el proceso:
```c
curenv_sched_timer_count = get_current_timer_count();
env_run(env);
```

- Cálculo de la diferencia entre el valor guardado y el actual en la **proxima llamada al scheduler**.
```c
uint32_t current_timer_count = get_current_timer_count();
		uint32_t diff = curenv_sched_timer_count > current_timer_count
			? curenv_sched_timer_count - current_timer_count // Env scheduling timer count - current timer count
			: (curenv_sched_timer_count + TIMER_INITIAL_COUNT - current_timer_count);
		sched.total_time += diff;
		sched.partial_time += diff;
		curenv->env_ran_for += diff;
```

Para las colas usamos vectores estáticos, dado que el numero máximo de procesos en el sistema es la constante **NENV**, que usamos para dimensionar las colas. Esto simplifica bastante la implementación de las mismas, pero ocupa mucha más memoria de la que ocuparía si se usara memoria dinámica:
```c
typedef struct priority_queue {
	struct Env *envs[NENV];		// Static array for the queue
	int front;				// Index of the front element of the queue
	int rear;				// Index of the rear element of the queue
	uint32_t time_slices;		// The amount of time slices each env in the queue will run for
} priority_queue_t;
```

Luego, la estructura principal del scheduler, simplemente tenemos las colas de prioridades y un par de contadores para determinar el tiempo total de ejecución de procesos y para determinar si es necesario realizar un **boost**, como lo requiere MLFQ:
```c
typedef struct mlfq {
	priority_queue_t env_queues[SCHED_MLFQ_QUEUES];	// MLFQ priority queues
    uint32_t partial_time;	// Partial amount of timer cycles the scheduler has ran for, resets on boost
	uint32_t total_time;	// Total amount of timer cycles the scheduler has ran for
} mlfq_t;
```

## Funcionamiento

El primer paso es actualizar el tiempo de ejecución de ```curenv``` (el proceso en ejecución) y de los contadores del scheduler: El primero es necesario para ddeterminar si hay que bajarle la prioridad al proceso, y el segundo es necesario para determinar si hay que realizar un **boost** de los procesos a la prioridad 0. Esto se muestra en el snippet 'Cálculo de la diferencia entre el valor guardado...' mostrado más arriba. Luego de esto, procedemos a actualizar la prioridad de ```curenv``` de ser necesario.

Para ello, primero nos fijamos si se ejecutó por el tiempo que señala su prioridad, y si es así, la bajamos:
```c
if ((curenv->env_ran_for / SCHED_TIME_SLICE) >= sched.env_queues[curenv->env_priority].time_slices){
			// Lower env priority if posible
			if (curenv->env_priority < SCHED_MLFQ_QUEUES-1)
				curenv->env_priority++;
			curenv->env_ran_for = 0;
		}
```

Y luego de esto, encolamos ```curenv``` de ser posible, pues en env_run se lo desalojará y se cambiara su estado a ENV_RUNNABLE:
```c
int this_cpu = cpunum();
// only queue curenv if we are in the cpu it's running in
if ((curenv->env_status == ENV_RUNNING || 
  curenv->env_status == ENV_RUNNABLE ||
  curenv->env_status == ENV_DYING) && 
  this_cpu == curenv->env_cpunum)
  queue_env(curenv);
```

Una vez hecho esto, nos fijamos si es necesario realizar un **boost** de los procesos, y si es así reseteamos el contador correspondiente:
```c
uint32_t time_slices = sched.partial_time / SCHED_TIME_SLICE;
	if (time_slices > 0 && time_slices / SCHED_MLFQ_BOOST_TIME == 1){
		for (int i = 1; i < SCHED_MLFQ_QUEUES; i++) {
			while (sched.env_queues[i].front >= 0) {
				struct Env *env = dequeue_env(i);
				env->env_priority = 0;
				queue_env(env);
			}
		}
		sched.partial_time = 0;
	}
```

Al final del paso anterior, ya estamos listos para ejecutar el primer proceso que logremos desencolar, iterando las colas de mejor a peor prioridad. Esto nos garantiza que los procesos de peor prioridad solo se ejecutarán si las colas de prioridades mejores están vacías:
```c
for (int i = 0; i < SCHED_MLFQ_QUEUES; i++) {
		struct Env *env = dequeue_env(i);
		if (env){
			curenv_sched_timer_count = get_current_timer_count();
			env_run(env);
		}
	}
```
