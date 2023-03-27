#include <inc/lib.h>

#define LOOP 1000

void
umain(int argc, char **argv){

  envid_t pid = fork();
  int parent_priority, child_priority;

  // Interactive processes will have more I/O calls and less computation time, 
  // so they should have a higher priority

  if (pid == 0){
    // Child, interactive process
    // Waits for a value and prints it
    uint32_t value = ipc_recv(&pid, 0, 0);
    for (int i = 0; i < LOOP/100; i++);
    cprintf("Value: %u\n", value);
    cprintf("On interactive process, priority is %d\n", sys_env_get_priority(sys_getenvid()));
  }
  else {
    // Parent, non-interactive process
    int j = 1;
    for (int i = 1; i <= LOOP; i++)
        j *= i; // some computation
    ipc_send(pid, j, 0, 0);
    sys_yield(); // finish the child process

    parent_priority = sys_env_get_priority(sys_getenvid());
    child_priority = sys_env_get_priority(pid);

    cprintf("On non-interactive process, priority is %d\n", parent_priority);
    assert(parent_priority > child_priority);
  }
}