/*
 * sched.c - initializes struct for task 0 anda task 1
 */

#include <sched.h>
#include <mm.h>
#include <io.h>
#include <list.h>

#include <libc.h>///temp.


/**
 * Container for the Task array and 2 additional pages (the first and the last one)
 * to protect against out of bound accesses.
 */
union task_union protected_tasks[NR_TASKS+2]
  __attribute__((__section__(".data.task")));

union task_union *task = &protected_tasks[1]; /* == union task_union task[NR_TASKS] */
struct list_head freequeue;
struct list_head readyqueue;
struct list_head keyboardqueue;
struct task_struct * idle_task;

//#if 0
struct task_struct *list_head_to_task_struct(struct list_head *l)
{
  return list_entry( l, struct task_struct, list);
}
//#endif

extern struct list_head blocked;

void stats_system_to_ready() { //(c)
	current()->est.system_ticks += get_ticks() - (current()->est.elapsed_total_ticks);
	current()->est.elapsed_total_ticks = get_ticks();
	return;
}

void stats_ready_to_system(struct task_struct * cur) { //(d)
	cur->est.ready_ticks += get_ticks() - (cur->est.elapsed_total_ticks);
	cur->est.elapsed_total_ticks = get_ticks();
	++cur->est.total_trans;
	return;
}

void stats_system_to_blocked() { 
	current()->est.system_ticks += get_ticks() - (current()->est.elapsed_total_ticks);
	current()->est.elapsed_total_ticks = get_ticks();
	return;
}

void stats_blocked_to_system(struct task_struct * cur) { 
	cur->est.blocked_ticks += get_ticks() - (cur->est.elapsed_total_ticks);
	cur->est.elapsed_total_ticks = get_ticks();
	//++cur->est.total_trans;
	return;
}


/* get_DIR - Returns the Page Directory address for task 't' */
page_table_entry * get_DIR (struct task_struct *t) 
{
	return t->dir_pages_baseAddr;
}

/* get_PT - Returns the Page Table address for task 't' */
page_table_entry * get_PT (struct task_struct *t) 
{
	return (page_table_entry *)(((unsigned int)(t->dir_pages_baseAddr->bits.pbase_addr))<<12);
}


int allocate_DIR(struct task_struct *t) 
{
	int pos;
	
	if (t->posPage == -1) {
		while (pos < NR_TASKS && pages_in_use[pos] != 0) ++pos;
		pages_in_use[pos] = 1;
		t->posPage = pos;
	}
	else {
		pos = t->posPage;
		++pages_in_use[pos];
	}		

	t->dir_pages_baseAddr = (page_table_entry*) &dir_pages[pos]; 

	return 1;
}

void cpu_idle(void)
{
	__asm__ __volatile__("sti": : :"memory");

	while(1)
	{
	;
	}
}

void init_idle (void)
{
	union task_union * ts;
	struct list_head *h;
	h = list_first( &freequeue );
	ts = (union task_union *) list_head_to_task_struct(h);
	list_del(h);
	ts->task.PID = 0;
	ts->task.quantum = 0;
	ts->task.posPage = -1;
	allocate_DIR(&ts->task); //Store process address space
	ts->task.dir_pages_baseAddr = get_DIR(&ts->task); //Initialize the process address space
	idle_task = &(ts->task);
	((union task_union *) idle_task) -> stack[1023] = cpu_idle; //sin parentesis direccion rutina.
	((union task_union *) idle_task) -> stack[1022] = 0;
	ts->task.kernel_espP = &(((union task_union *) idle_task) -> stack[1022]);
	/*ts->task.est.user_ticks = 0;
	ts->task.est.system_ticks = 0;
	ts->task.est.blocked_ticks = 0;
	ts->task.est.ready_ticks = 0;
	ts->task.est.elapsed_total_ticks = get_ticks();
	ts->task.est.total_trans = 0;
	ts->task.est.remaining_ticks = 0;*/
}

void init_task1(void) //init process
{
	union task_union *tu_init;
 	struct list_head *h;
	h = list_first( &freequeue );
	tu_init = (union task_union *) list_head_to_task_struct(h);
	list_del(h);	
	tu_init->task.PID = 1;
	tu_init->task.program_break = HEAP_START;
	tu_init->task.quantum = INIT_QUANTUM;
	cpu_ticks = tu_init->task.quantum;
	tu_init->task.posPage = -1;
	allocate_DIR(tu_init);
	tu_init->task.dir_pages_baseAddr = get_DIR(tu_init);
	set_user_pages(tu_init);
	tss.esp0 = (tu_init->stack) + KERNEL_STACK_SIZE;
	set_cr3(tu_init->task.dir_pages_baseAddr);
	tu_init->task.est.user_ticks = 0;
	tu_init->task.est.system_ticks = 0;
	tu_init->task.est.blocked_ticks = 0;
	tu_init->task.est.ready_ticks = 0;
	tu_init->task.est.elapsed_total_ticks = get_ticks();
	tu_init->task.est.total_trans = 0;
	tu_init->task.est.remaining_ticks = INIT_QUANTUM;
}

void init_sched(){ //add all task_struct to freequeue and initialization ready and free queue
	INIT_LIST_HEAD(&freequeue);
	INIT_LIST_HEAD(&readyqueue);
	INIT_LIST_HEAD(&keyboardqueue);
	int i;
	for (i = 0; i < NR_TASKS; ++i) {
		list_add_tail( &(task[i].task.list), &freequeue );
	}
}

struct task_struct* current()
{
  int ret_value;
  
  __asm__ __volatile__(
  	"movl %%esp, %0"
	: "=g" (ret_value)
  );
  return (struct task_struct*)(ret_value&0xfffff000);
}

/*void task_switch(union task_union*t) {
	__asm__ __volatile__(
		"pushl %%edi\n\t"
		"pushl %%esi\n\t"
		"pushl %%ebx\n\t"
		"pushl %0\n\t"
		"call inner_task_switch\n\t"
		"popl %%ebx\n\t"
		"popl %%esi\n\t"
		"popl %%edi"
		: 
		: "r" (t));
}*/

void task_switch(union task_union*t) {
	__asm__ __volatile__(
		"pushl %%edi\n\t"
		"pushl %%esi\n\t"
		"pushl %%ebx\n\t"
		:
		: );
	inner_task_switch(t);
	__asm__ __volatile__(
		"popl %%ebx\n\t"
		"popl %%esi\n\t"
		"popl %%edi"
		:
		: );
}

void inner_task_switch(union task_union *new_task) {
	tss.esp0 = (*new_task).stack + KERNEL_STACK_SIZE;
	if ((*new_task).task.dir_pages_baseAddr != current()->dir_pages_baseAddr) {
		set_cr3((*new_task).task.dir_pages_baseAddr);
	}
	__asm__ __volatile__(
		"movl %%ebp, %%eax\n\t" //ebp -> current.kernel_espP
		"movl %%edx, %%esp\n\t" //new.kernel_espP -> esp
		"popl %%ebp\n\t"
		: "=a" (current()->kernel_espP)
		: "d" ((*new_task).task.kernel_espP));
		__asm__ __volatile__(
		"ret\n\t"
		: 
		: );	
		
}

int get_quantum(struct task_struct *t) {
	return t->quantum;
}

void set_quantum(struct task_struct *t, int new_quantum) {
	t->quantum = new_quantum;
}

void update_sched_data_rr(void) {
	--cpu_ticks;
	current()->est.remaining_ticks = cpu_ticks;
}

int needs_sched_rr(void) {
	if (cpu_ticks <= 0){
		++current()->est.total_trans; //se cree que estaba aqui el error pero sigue sin ir
		struct list_head * e = list_first( &readyqueue );
		if (list_empty(e) == 1) {
			cpu_ticks = get_quantum(current());
			current()->est.remaining_ticks = cpu_ticks;
			return 0;
		}
		return 1;
	}
	return 0;
}

void update_process_state_rr(struct task_struct *t, struct list_head *dest) {
	if (dest != NULL) {
		if (dest != &readyqueue) {
			t->state=ST_BLOCKED;
			stats_system_to_blocked();
		}
		else {
			if (t->state == ST_BLOCKED) stats_blocked_to_system(t);
			t->state=ST_READY;
			stats_system_to_ready();
		}
		list_add_tail( &(t->list), dest );
	}
}

void sched_next_rr(void) {
	union task_union * tu;
	struct list_head * e = list_first( &readyqueue );
	
	if (list_empty(e) == 1) {
		tu = idle_task;
		cpu_ticks = get_quantum((struct task_struct *) idle_task);
	}
	else {
		tu = list_head_to_task_struct(e);
		list_del(e);
		cpu_ticks = get_quantum(tu);
	}
	tu->task.state = ST_RUN;
	stats_ready_to_system(tu);
	task_switch(tu);
}

void schedule() {
	update_sched_data_rr();
	if(needs_sched_rr() == 1) {
		update_process_state_rr(current(),&readyqueue);
		sched_next_rr();
	}
	else update_process_state_rr(current(),NULL);
}

void block() {
	update_process_state_rr(current(), &keyboardqueue);
	sched_next_rr();
}

void unblock() {
	struct task_struct * t;
	struct list_head * e = list_first( &keyboardqueue );
	t = (union task_union *) list_head_to_task_struct(e);	
	list_del(e);
	stats_blocked_to_system(t);
	t->state=ST_READY;
	stats_system_to_ready();
	list_add( &(current()->list), &readyqueue );
	list_add( &(t->list), &readyqueue );
	sched_next_rr();
}

