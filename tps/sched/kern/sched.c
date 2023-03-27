#include <inc/assert.h>
#include <inc/x86.h>
#include <kern/spinlock.h>
#include <kern/env.h>
#include <kern/pmap.h>
#include <kern/monitor.h>
#include <kern/sched.h>

void sched_halt(void);

char *status[5] = {
	"ENV_FREE", "ENV_DYING", "ENV_RUNNABLE", "ENV_RUNNING", "ENV_NOT_RUNNABLE"
};
uint32_t curenv_sched_timer_count = 0;
extern volatile uint32_t *lapic;

void
print_statistics()
{
	cprintf("\n------------Scheduler statistics------------\n");
	cprintf("Total scheduler calls: %d\n", sched_calls);
	cprintf("Total scheduler time slices: %d\n",
	        sched.total_time / SCHED_TIME_SLICE);

	uint32_t i = 0;

	for (int i = 0; i < collected_processes; ++i) {
		if (i == 0)
			cprintf("\n   Statistics per env:\n\n");
		cprintf("   PID : [%08x]   RUNS: %ld   FIRST_SCHEDULED_AT: %u "
		        "timer cycles  "
		        " DESTROYED_AT: %u timer cycles  "
				" RAN_FOR: %u timer cycles\n",
		        process_statistics[i].env_id,
		        process_statistics[i].env_runs,
		        process_statistics[i].env_start_time,
		        process_statistics[i].env_completion_time,
				process_statistics[i].env_ran_for);
	}
	cprintf("\n");
}

void
initialize_statistics()
{
	sched_calls = 0;
	for (int i = 0; i < MAX_USER_PROCESSES; ++i) {
		process_statistics[i].env_id = 0;
		process_statistics[i].env_runs = 0;
		process_statistics[i].env_start_time = 0;
		process_statistics[i].env_completion_time = 0;
	}
}


uint32_t
get_elapsed_time(uint32_t since_time)
{
	uint32_t current_timer_count = get_current_timer_count();
	uint32_t diff = since_time > current_timer_count
	                        ? since_time - current_timer_count
	                        : (since_time + TIMER_INITIAL_COUNT -
	                           current_timer_count);

	return diff;
}


void
push_statistics(struct Env *env)
{
	if (env->env_type != ENV_TYPE_USER)
		return;
	env_stats_t *stats = &process_statistics[collected_processes];

	if (collected_processes < MAX_USER_PROCESSES - 1) {
		stats->env_id = env->env_id;
		stats->env_runs = env->env_runs;
		stats->env_start_time = sched.total_time;
		stats->env_completion_time = stats->env_start_time;
		stats->env_ran_for = env->env_ran_for;

		collected_processes++;
	}
}


void
update_statistics(struct Env *env, uint32_t diff)
{
	for (int i = 0; i < collected_processes; i++) {
		env_stats_t *stats = &process_statistics[i];
		if (env->env_id == stats->env_id &&
		    env->env_start_time == stats->env_start_time) {
			stats->env_runs = env->env_runs;
			if (diff == 0)
				diff = get_elapsed_time(stats->env_completion_time);
			stats->env_completion_time += diff;
			stats->env_ran_for = env->env_ran_for;
		}
	}
}

void
sched_init()
{
	// Statistics
	initialize_statistics();

	// MLFQ
	sched.total_time = 0;
	sched.partial_time = 0;
	for (int i = 0; i < SCHED_MLFQ_QUEUES; i++) {
		if (i == 0)
			sched.env_queues[i].time_slices = 1;
		else
			sched.env_queues[i].time_slices =
			        sched.env_queues[i - 1].time_slices * 2;
		sched.env_queues[i].front = -1;
		sched.env_queues[i].rear = -1;
		for (int j = 0; j < NENV; j++)
			sched.env_queues[i].envs[j] = NULL;
	}
}

void
queue_env(struct Env *env)
{
	priority_queue_t *queue = &sched.env_queues[env->env_priority];

	if (queue->rear < 0 || queue->front < 0) {
		queue->front = 0;
		queue->rear = 0;
	} else if (queue->rear + 1 == NENV) {
		queue->rear = 0;
	} else {
		queue->rear++;
	}

	queue->envs[queue->rear] = env;
}

struct Env *
dequeue_env(int priority)
{
	priority_queue_t *queue = &sched.env_queues[priority];
	struct Env *env = queue->front >= 0 ? queue->envs[queue->front] : NULL;

	if (queue->front == queue->rear) {
		queue->front = -1;
		queue->rear = -1;
	} else if (queue->front + 1 == NENV)
		queue->front = 0;
	else {
		queue->front++;
	}

	return env;
}

uint32_t
get_current_timer_count()
{
	return lapic[0x0390 / 4];
}

void
sched_mlfq()
{
	if (curenv) {
		uint32_t diff = get_elapsed_time(curenv_sched_timer_count);
		sched.total_time += diff;
		sched.partial_time += diff;
		curenv->env_ran_for += diff;

		update_statistics(curenv, diff);

		if ((curenv->env_ran_for / SCHED_TIME_SLICE) >=
		    sched.env_queues[curenv->env_priority].time_slices) {
			// Lower env priority if posible
			if (curenv->env_priority < SCHED_MLFQ_QUEUES - 1)
				curenv->env_priority++;
			curenv->env_ran_for = 0;
		}

		int this_cpu = cpunum();
		// only queue curenv if we are in the cpu it's running in
		if (curenv->env_status == ENV_RUNNABLE ||
		    (curenv->env_status == ENV_RUNNING &&
		     this_cpu == curenv->env_cpunum))
			queue_env(curenv);
	}

	uint32_t time_slices = sched.partial_time / SCHED_TIME_SLICE;
	if (time_slices > 0 && time_slices / SCHED_MLFQ_BOOST_TIME == 1) {
		for (int i = 1; i < SCHED_MLFQ_QUEUES; i++) {
			while (sched.env_queues[i].front >= 0) {
				struct Env *env = dequeue_env(i);
				env->env_priority = 0;
				queue_env(env);
			}
		}
		sched.partial_time = 0;
	}
	for (int i = 0; i < SCHED_MLFQ_QUEUES; i++) {
		struct Env *env = dequeue_env(i);
		if (env) {
			curenv_sched_timer_count = get_current_timer_count();
			if (env->env_runs == 0) {
				env->env_start_time = sched.total_time;
				push_statistics(env);
			}
			env_run(env);
		}
	}
}

// Choose a user environment to run and run it.
void
sched_yield(void)
{
	struct Env *idle;

	// Implement simple round-robin scheduling.
	//
	// Search through 'envs' for an ENV_RUNNABLE environment in
	// circular fashion starting just after the env this CPU was
	// last running.  Switch to the first such environment found.
	//
	// If no envs are runnable, but the environment previously
	// running on this CPU is still ENV_RUNNING, it's okay to
	// choose that environment.
	//
	// Never choose an environment that's currently running on
	// another CPU (env_status == ENV_RUNNING). If there are
	// no runnable environments, simply drop through to the code
	// below to halt the cpu.

	// Your code here
	// cprintf("Scheduling env...\n");
	sched_calls++;

#ifdef SCHED_RR
	size_t env_index = 0;
	if (curenv) {
		env_index = ENVX(curenv->env_id);
		assert(curenv->env_id == envs[env_index].env_id);
	}

	for (int i = env_index; i < NENV; i++) {
		if (envs[i].env_status == ENV_RUNNABLE) {
			env_run(envs + i);
		}
	}
	for (int i = 0; i < env_index; i++) {
		if (envs[i].env_status == ENV_RUNNABLE) {
			env_run(envs + i);
		}
	}

	// If there are no other runnable envs and curenv
	// has not finished, keep running it
	if (curenv && curenv->env_status == ENV_RUNNING) {
		env_run(curenv);
	}
#endif

#ifdef SCHED_MLFQ
	sched_mlfq();
#endif

	// sched_halt never returns
	sched_halt();
}


// Halt this CPU when there is nothing to do. Wait until the
// timer interrupt wakes it up. This function never returns.
//
void
sched_halt(void)
{
	/*
	 *	Historial de procesos ejecutados/seleccionados	OK
	 *	Número de llamadas al scheduler OK Número de ejecuciones por
	 *cada proceso 			OK Inicio y fin de cada proceso
	 *ejecutado 			...
	 */

	int i;

	// For debugging and testing purposes, if there are no runnable
	// environments in the system, then drop into the kernel monitor.
	for (i = 0; i < NENV; i++) {
		if ((envs[i].env_status == ENV_RUNNABLE ||
		     envs[i].env_status == ENV_RUNNING ||
		     envs[i].env_status == ENV_DYING)) {
			break;
		}
	}
	if (i == NENV) {
		print_statistics();
		cprintf("No runnable environments in the system!\n");
		while (1)
			monitor(NULL);
	}

	// Mark that no environment is running on this CPU
	curenv = NULL;
	lcr3(PADDR(kern_pgdir));

	// Mark that this CPU is in the HALT state, so that when
	// timer interupts come in, we know we should re-acquire the
	// big kernel lock
	xchg(&thiscpu->cpu_status, CPU_HALTED);

	// Release the big kernel lock as if we were "leaving" the kernel
	unlock_kernel();

	// Once the scheduler has finishied it's work, print statistics on
	// performance. Your code here

	// Reset stack pointer, enable interrupts and then halt.
	asm volatile("movl $0, %%ebp\n"
	             "movl %0, %%esp\n"
	             "pushl $0\n"
	             "pushl $0\n"
	             "sti\n"
	             "1:\n"
	             "hlt\n"
	             "jmp 1b\n"
	             :
	             : "a"(thiscpu->cpu_ts.ts_esp0));
}
