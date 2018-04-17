#include <io.h>
#include <utils.h>
#include <list.h>
#include <sched.h>
#include <interrupt.h>


// Queue for blocked processes in I/O 
struct list_head blocked;

int sys_write_console(char *buffer,int size)
{
  int i;
  
  for (i=0; i<size; i++)
    printc(buffer[i]);
  
  return size;
}

int sys_read_keyboard(char* buffersito, int size) 
{
	struct list_head * e = list_first( &keyboardqueue );
	if (list_empty(e) == 0) block();
	int i;
	for (i=0; i<size; i++) {
		if (puntero_read == puntero_write) block();
		buffersito[i] = buffersircular[puntero_read];
		buffersircular[puntero_read] = NULL;
		++puntero_read;
	}
	return size;
}
