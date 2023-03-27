/* See COPYRIGHT for copyright information. */

#ifndef JOS_KERN_SCHED_H
#define JOS_KERN_SCHED_H
#ifndef JOS_KERNEL
# error "This is a JOS kernel header; user programs should not #include it"
#endif

#ifndef SCHED_RR
#define SCHED_MLFQ 1
#endif

// This function does not return.
void sched_yield(void);
// void sched_yield(void) __attribute__((noreturn));

// MLFQ scheduler initializer
void sched_init(void);

#define SCHED_TIME_SLICE 1000000   // Amount of timer cycles that constitute a time slice
#define SCHED_MLFQ_QUEUES 5		 // Amount of priority queues
#define SCHED_MLFQ_BOOST_TIME 30 // Amount of time slices the scheduler runs for before boosting all envs to the highest priority queue

#define TIMER_INITIAL_COUNT 10000000

#define SCHED_HISTORY 20
#define MAX_USER_PROCESSES 	7867	// indicates the number of processes 
									// to collect for statistics

// Scheduler statistics
typedef struct env_stats {
	envid_t env_id;
	uint32_t env_runs;
	uint32_t env_ran_for;	
	uint32_t env_start_time;		// Scheduler total timer cycles elapsed at env start
	uint32_t env_completion_time;
} env_stats_t;

uint32_t sched_calls;		// Total amount of calls to the scheduler
struct env_stats process_statistics[MAX_USER_PROCESSES];
int collected_processes;


// MLFQ Scheduler data structures
typedef struct priority_queue {
	struct Env *envs[NENV];		// Static array for the queue
	int front;				// Index of the front element of the queue
	int rear;				// Index of the rear element of the queue
	uint32_t time_slices;		// The amount of time slices each env in the queue will run for
} priority_queue_t;


typedef struct mlfq {
	priority_queue_t env_queues[SCHED_MLFQ_QUEUES];	// MLFQ priority queues
    uint32_t partial_time;	// Partial amount of timer cycles the scheduler has ran for, resets on boost
	uint32_t total_time;	// Total amount of timer cycles the scheduler has ran for
} mlfq_t;


mlfq_t sched;	// Main scheduler

void queue_env(struct Env *env);
struct Env *dequeue_env(int priority);
uint32_t get_current_timer_count();

#endif	// !JOS_KERN_SCHED_H
