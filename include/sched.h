/*
 * sched.h - Estructures i macros pel tractament de processos
 */

#ifndef __SCHED_H__
#define __SCHED_H__

#include <list.h>
#include <types.h>
#include <mm_address.h>
#include <stats.h>

#define NR_TASKS      10
#define KERNEL_STACK_SIZE	1024
#define INIT_QUANTUM      20

enum state_t { ST_RUN, ST_READY, ST_BLOCKED };

struct task_struct { //PCB
  int PID;			/* Process ID. This MUST be the first field of the struct. */
  page_table_entry * dir_pages_baseAddr;
  int kernel_espP;
  int * program_break;
  int quantum;
  int posPage;
  int count_read;
  struct stats est;
  struct list_head list;
  enum state_t state;
};

union task_union { //PCB + KERNEL STACK
  struct task_struct task;
  unsigned long stack[KERNEL_STACK_SIZE];    /* pila de sistema, per procés */
};

extern union task_union protected_tasks[NR_TASKS+2];
extern union task_union *task; /* Vector de tasques */
extern struct task_struct *idle_task;
extern struct list_head freequeue;
extern struct list_head readyqueue;
extern struct list_head keyboardqueue;

struct semaphore {
	int counter;
	int owner; //camp per identificar qui ha creat el semaforo
	struct list_head semaphorequeue;
};

struct semaphore array_semaphore[NR_TASKS*2];

int cpu_ticks;

int pages_in_use[NR_TASKS];

#define KERNEL_ESP(t)       	(DWord) &(t)->stack[KERNEL_STACK_SIZE]

#define INITIAL_ESP       	KERNEL_ESP(&task[1])

/* Inicialitza les dades del proces inicial */
void init_task1(void);

void init_idle(void);

void init_sched(void);

struct task_struct * current();

void task_switch(union task_union*t);

struct task_struct *list_head_to_task_struct(struct list_head *l);

int allocate_DIR(struct task_struct *t);

page_table_entry * get_PT (struct task_struct *t) ;

page_table_entry * get_DIR (struct task_struct *t) ;

/* Headers for the scheduling policy */
void sched_next_rr();
void update_process_state_rr(struct task_struct *t, struct list_head *dest);
int needs_sched_rr();
void update_sched_data_rr();

void block();
void unblock();

#endif  /* __SCHED_H__ */
