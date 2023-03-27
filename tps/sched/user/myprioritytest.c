//custom, priority test

#include <inc/lib.h>

void
umain(int argc, char **argv){

  int priority;
  int expected_priority = 1;

  for(int i = 0; i < 1500000; i++); // make sure we run for more than one time slice
  sys_yield();  // update env priority

  priority = thisenv->env_priority;
  if(priority != expected_priority){
      cprintf("   Diffent priority ERROR! env priority: %d, expected: %d, thisenv timer cycles: %d\n", 
        priority, expected_priority, thisenv->env_ran_for);
      return;
    }
  
  cprintf("After running for more than one time slice, the priority increases by one\n");
}