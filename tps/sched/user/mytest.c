//custom, hello
/*
  to add a new test, add the path in: kern/Makefrag
  in the second KERN_BINFILES.
  for example: "\ user/mytest"
*/
#include <inc/lib.h>
	
void
umain(int argc, char **argv)
{
	cprintf("\n");
	cprintf("HELLO FROM CUSTOM TEST\n");
	cprintf("i am environment %08x\n", thisenv->env_id);
	cprintf("i runned: %d times\n", thisenv->env_runs);
	cprintf("\n");
}
