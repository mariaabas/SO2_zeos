/*
 * sys.c - Syscalls implementation
 */
#include <devices.h>

#include <utils.h>

#include <io.h>

#include <mm.h>

#include <mm_address.h>

#include <sched.h>

#include <errno.h>

#include <segment.h>

#include <interrupt.h>

#define LECTURA 0
#define ESCRIPTURA 1
#define SIZEK 512

int PIDS = 1;

void stats_user_to_system() { //(a)
	current()->est.user_ticks += get_ticks() - (current()->est.elapsed_total_ticks);
	current()->est.elapsed_total_ticks = get_ticks();
	return;
}

int stats_system_to_user(int ret) { //(b)
	current()->est.system_ticks += get_ticks() - (current()->est.elapsed_total_ticks);
	current()->est.elapsed_total_ticks = get_ticks();
	return ret;
}

int check_fd(int fd, int permissions)
{
  if (fd!=1) return -EBADF; /*9*/
  if (permissions!=ESCRIPTURA) return -EACCES; /*13*/
  return 0;
}

int sys_ni_syscall()
{
	stats_user_to_system();
	return stats_system_to_user(-ENOSYS); /*38*/
}

int sys_getpid()
{
	stats_user_to_system();
	return stats_system_to_user(current()->PID);
}

int ret_from_fork() {
	return 0;
}

int sys_fork()
{
	stats_user_to_system();
  	int PID=-1;

	/*INICIALIZACIONES*/
	union task_union * tu_child;
	union task_union * tu_daddy = (union task_union *) current();
	struct list_head * e = list_first( &freequeue );
	if (list_empty(e) == 1) return stats_system_to_user(-EAGAIN); /*11*/
	tu_child = (union task_union *) list_head_to_task_struct(e);
	list_del(e);
	copy_data(tu_daddy,tu_child,4*KERNEL_STACK_SIZE);
	tu_child->task.posPage = -1;	
	allocate_DIR(&tu_child->task);
	tu_child->task.dir_pages_baseAddr = get_DIR(&tu_child->task);

	/*COPY USER CODE*/
	page_table_entry * ptc = get_PT(tu_child); //PT Child
	page_table_entry * ptd = get_PT(tu_daddy); //PT Daddy
	int i;	
	for (i = PAG_LOG_INIT_CODE; i < PAG_LOG_INIT_CODE + NUM_PAG_CODE; i++) {
		ptc[i] = ptd[i];
	}

	int d = tu_daddy->task.program_break;
	d = (d-HEAP_START)/PAGE_SIZE + (d%PAGE_SIZE != 0);
	/*INITIALIZE CHILD USER DATA+STACK*/
	int frames[NUM_PAG_DATA];
	i = 0;
	int x = 1;
	while (i < NUM_PAG_DATA + d && x > 0) {
		x = alloc_frame();
		frames[i] = x;
		i++;	
	}
	if (x < 0) {
		while (i >= 0) {
			free_frame(frames[i]);
			i--;
		}
		list_add_tail( &(tu_child->task.list), &freequeue );
		return stats_system_to_user(-ENOMEM); /*12*/
	}
	for (i = 0; i < NUM_PAG_DATA+d; i++) {
		set_ss_pag(ptc, PAG_LOG_INIT_DATA+i, frames[i]);
	}

	/*"AMPLIAR" ESPACIO PADRE Y COPIAR DATA+STACK*/
	for (i = 0; i < NUM_PAG_DATA+d; i++) {
		set_ss_pag(ptd, PAG_LOG_INIT_DATA+NUM_PAG_DATA+d+i, frames[i]);
	}
	copy_data(PAG_LOG_INIT_DATA << 12, (PAG_LOG_INIT_DATA+NUM_PAG_DATA+d) << 12, (NUM_PAG_DATA+d)*PAGE_SIZE);
	for (i = 0; i < NUM_PAG_DATA+d; i++) {
		del_ss_pag(ptd, PAG_LOG_INIT_DATA+NUM_PAG_DATA+d+i); 
	}
 	set_cr3((*tu_daddy).task.dir_pages_baseAddr);

	/*ASIGNAR PID HIJO Y RESET STATS */
	++PIDS;	
	PID = PIDS;
	tu_child->task.PID = PID;
	tu_child->task.est.user_ticks = 0;
	tu_child->task.est.system_ticks = 0;
	tu_child->task.est.blocked_ticks = 0;
	tu_child->task.est.ready_ticks = 0;
	tu_child->task.est.elapsed_total_ticks = 0;
	tu_child->task.est.total_trans = 0;
	tu_child->task.est.remaining_ticks = 0;

	/*RET HIJO*/
	long y;
	__asm__ __volatile__(
		"movl %%ebp, %%eax\n\t"
		: "=a" (y)
		: );
	long z = &(tu_daddy->stack[KERNEL_STACK_SIZE]);
	tu_child->stack[KERNEL_STACK_SIZE-((z-y)/4)] = ret_from_fork;
	tu_child->stack[KERNEL_STACK_SIZE-((z-y)/4)-1] = 0;
	tu_child->task.kernel_espP = &(tu_child->stack[KERNEL_STACK_SIZE-((z-y)/4)-1]);
	list_add_tail( &(tu_child->task.list), &readyqueue );
	return stats_system_to_user(PID);
}

int sys_write(int fd, char *buffer, int size) {
	stats_user_to_system();
	int j = check_fd(fd, ESCRIPTURA);
	if (j < 0) return stats_system_to_user(j);
	else if (buffer == NULL) return stats_system_to_user(-EFAULT); /*14*/
	else if (size < 0)  return stats_system_to_user(-EINVAL); /*22*/
	else if (access_ok(ESCRIPTURA, buffer, size) == 0) return stats_system_to_user(-EFAULT); /*14*/
	else {
		char kernel_buf[SIZEK];
		int i = 0;
		int x = 0;
		int error = 0;
		while (size-i >= SIZEK) {
			error = copy_from_user(buffer+i, kernel_buf, SIZEK);
			if (error < 0) return stats_system_to_user(error); //Error de Kernel (?)
			error = sys_write_console(kernel_buf, SIZEK);
			if (error < 0) return stats_system_to_user(error); //Error de Consola (?)
			x += error;		
			i +=SIZEK;
		}
		error = copy_from_user(buffer+i, kernel_buf, size-i);
		if (error < 0) return stats_system_to_user(error);
		error = sys_write_console(kernel_buf, size-i);
		if (error < 0) return stats_system_to_user(error);		
		x += error;	
		return stats_system_to_user(x);

	} 
}

int sys_gettime() {
	stats_user_to_system();
	return stats_system_to_user(zeos_ticks);
}

int sys_getstats(int pid, struct stats *st) {
	stats_user_to_system();
	if (st == NULL) return stats_system_to_user(-EFAULT); /*14*/
	if (access_ok(ESCRIPTURA, st, sizeof(struct stats)) == 0) return stats_system_to_user(-EFAULT); /*14*/
	if (pid <= 0) return stats_system_to_user(-EINVAL); /*22*/
	int i = 0;
	int x = 0;
	struct task_struct* ts;	
	while (i < NR_TASKS && x == 0) {
		if (task[i].task.PID == pid) {
			x = 1;
			ts = &(task[i].task);
		}
		i++;
	}
	if (i == NR_TASKS) return stats_system_to_user(-ESRCH); /*3*/
	copy_to_user(&(ts->est),st, sizeof(struct stats));
	return stats_system_to_user(0);
}

void ret_from_clone() {
	__asm__ __volatile__(
		"movl %%ebp, %%esp\n\t"
		"popl %%ebp\n\t"
		"movl %0, %%edx\n\t"
      		"movl %%edx, %%ds\n\t"
     		"movl %%edx, %%es\n\t"
		"iret"
		:
		: "g" (__USER_DS));

}

int sys_clone(void (*function)(void), void *stack)
{
	stats_user_to_system();
  	int PID=-1;

	if (access_ok(ESCRIPTURA, stack, sizeof(stack)) == 0) return stats_system_to_user(-EFAULT); /*14*/
	if (access_ok(LECTURA, function, sizeof(function)) == 0) return stats_system_to_user(-EFAULT); /*14*/

	/*INICIALIZACIONES*/
	union task_union * tu_clon;
	union task_union * tu_daddy = (union task_union *) current();
	struct list_head * e = list_first( &freequeue );
	if (list_empty(e) == 1) return stats_system_to_user(-EAGAIN); /*11*/
	tu_clon = (union task_union *) list_head_to_task_struct(e);
	list_del(e);
	copy_data(tu_daddy,tu_clon,4*KERNEL_STACK_SIZE);
	
	allocate_DIR(&tu_clon->task);
	tu_clon->task.dir_pages_baseAddr = get_DIR(&tu_clon->task);

	/*COPY SHARED DATA*/
	page_table_entry * ptc = get_PT(tu_clon); //PT Clon
	page_table_entry * ptd = get_PT(tu_daddy); //PT Daddy
	int d = tu_daddy->task.program_break;
	d = (d-HEAP_START)/PAGE_SIZE;
	int i;	
	for (i = PAG_LOG_INIT_CODE; i < PAG_LOG_INIT_CODE + NUM_PAG_CODE+NUM_PAG_DATA + d; i++) {
		ptc[i] = ptd[i];
	}

	/*ASIGNAR PID CLON Y RESET STATS */
	++PIDS;	
	PID = PIDS;
	tu_clon->task.PID = PID;
	tu_clon->task.est.user_ticks = 0;
	tu_clon->task.est.system_ticks = 0;
	tu_clon->task.est.blocked_ticks = 0;
	tu_clon->task.est.ready_ticks = 0;
	tu_clon->task.est.elapsed_total_ticks = 0;
	tu_clon->task.est.total_trans = 0;
	tu_clon->task.est.remaining_ticks = 0;

	/*RET CLON*/
	tu_clon->stack[KERNEL_STACK_SIZE-6]   = ret_from_clone;
	tu_clon->stack[KERNEL_STACK_SIZE-5] = function;			//eip
	tu_clon->stack[KERNEL_STACK_SIZE-2] = stack; 			//esp
	tu_clon->stack[KERNEL_STACK_SIZE-7] = 0;
	tu_clon->task.kernel_espP = &(tu_clon->stack[KERNEL_STACK_SIZE-7]);
	list_add_tail( &(tu_clon->task.list), &readyqueue );
	return stats_system_to_user(PID);
}

int sys_sem_init(int n_sem, unsigned int value) {
	stats_user_to_system();
	if (n_sem < 0 || n_sem >= NR_TASKS*2) return stats_system_to_user(-EINVAL);
	if (array_semaphore[n_sem].owner > 0) return stats_system_to_user(-EBUSY);
	array_semaphore[n_sem].counter = value;
	array_semaphore[n_sem].owner = current()->PID;
	INIT_LIST_HEAD(&(array_semaphore[n_sem].semaphorequeue));
	return stats_system_to_user(0);
}

int sys_sem_wait(int n_sem) {
	stats_user_to_system();
	if (n_sem < 0 || n_sem >= NR_TASKS*2 || array_semaphore[n_sem].owner <= 0) return stats_system_to_user(-EINVAL);
	if (array_semaphore[n_sem].counter <= 0) {
		update_process_state_rr(current(),&(array_semaphore[n_sem].semaphorequeue));
		sched_next_rr();
	}
	else --array_semaphore[n_sem].counter;
	if (array_semaphore[n_sem].owner <= 0) return stats_system_to_user(-EINVAL);
	return stats_system_to_user(0);	
}

int sys_sem_signal(int n_sem) {
	stats_user_to_system();
	if (n_sem < 0 || n_sem >= NR_TASKS*2 || array_semaphore[n_sem].owner <= 0) return stats_system_to_user(-EINVAL);
	struct task_struct * t;
	struct list_head * e = list_first( &(array_semaphore[n_sem].semaphorequeue) );
	if (list_empty(e) == 1) ++array_semaphore[n_sem].counter;
	else {
		t = (union task_union *) list_head_to_task_struct(e);
		list_del(e);
		update_process_state_rr(t,&readyqueue);
	}
	return stats_system_to_user(0);
}

int sys_sem_destroy(int n_sem) {
	stats_user_to_system();
	if (n_sem < 0 || n_sem >= NR_TASKS*2 || array_semaphore[n_sem].owner <= 0) return stats_system_to_user(-EINVAL);
	if (array_semaphore[n_sem].owner != current()->PID) return stats_system_to_user(-EPERM);
	array_semaphore[n_sem].owner = -1;
	array_semaphore[n_sem].counter = -1;
	struct list_head * e = list_first( &(array_semaphore[n_sem].semaphorequeue) );
	while (list_empty(e) != 1) {
		struct task_struct * t;
		t = (union task_union *) list_head_to_task_struct(e);
		list_del(e);
		update_process_state_rr(t,&readyqueue);
		e = list_first( &(array_semaphore[n_sem].semaphorequeue) );
	}
	return stats_system_to_user(0);
}

int sys_read(int fd, char *buf, int count) {
	stats_user_to_system();
	if (fd != 0) return stats_system_to_user(-EBADF);
	else if (buf == NULL) return stats_system_to_user(-EFAULT); /*14*/
	else if (count < 0)  return stats_system_to_user(-EINVAL); /*22*/
	else if (access_ok(LECTURA, buf, count) == 0) return stats_system_to_user(-EFAULT); /*14*/
	else {
		current()->count_read = count;
		char buffersito[SIZEK];
		int i = 0;
		int x = 0;
		int error = 0;
		while (count-i >= SIZEK) {
			error = sys_read_keyboard(&buffersito, SIZEK);
			if (error < 0) return stats_system_to_user(error); //Error de Read (?)
			x += error;		
			copy_to_user(buffersito, buf+i, SIZEK);
			i += SIZEK;
		}
		error = sys_read_keyboard(&buffersito, count-i);
		copy_to_user(buffersito, buf+i, count-i);
		x += error;	
		return stats_system_to_user(x);
	}

}

void *sys_sbrk(int increment) {
	stats_user_to_system();
	union task_union * tu = (union task_union *) current();
	int heapsize = ((int)tu->task.program_break-HEAP_START); //HEAP_START+PROGRAM_BREAK/PAGESIZE??¿?
	int d = (int)tu->task.program_break;
	page_table_entry * pt = get_PT(tu);
	int i = 0;
	int x = 1;
 	if (increment == 0) return (void *) d;
	else if (increment < 0) { //FREE
		int inici = (d+increment);
		if (inici < HEAP_START) increment = HEAP_START - d;
		if (heapsize%PAGE_SIZE == 0) { 
			/*INITIALIZE NEW MEMORY SPACE*/
			while (i < (((-1)*increment)/PAGE_SIZE + 1)) {
				x = get_frame(pt, (d/PAGE_SIZE) -i);
				del_ss_pag(pt, (d/PAGE_SIZE) - i);
				free_frame(x);					
				++i;
			}
			d += increment;
		}		
		else {
			int z = heapsize%PAGE_SIZE;
			if (z < increment) d += increment;
			else {
				increment -= z;
				d +=z;
				while (i < (((-1)*increment)/PAGE_SIZE + 1)) {
					x = get_frame(pt, (d/PAGE_SIZE) -i);
					del_ss_pag(pt, (d/PAGE_SIZE)-i);
					free_frame(x);					
					++i;
				}
				d += increment;
			}
		}	
		
	}
	else if (increment > 0) { //ALLOCATE
		int final = (d+increment)/PAGE_SIZE;
		if (final > 357) return (void *) stats_system_to_user(-ENOMEM);
		int frames[356]; //TOTAL_PAGES - NUM_PAG_KERNEL - (NUM_PAG_CODE + NUM_PAG_DATA)
		if (heapsize%PAGE_SIZE == 0) { 
			/*INITIALIZE NEW MEMORY SPACE*/
			while (i < (increment/PAGE_SIZE + 1) && x > 0) {
				x = alloc_frame();
				frames[i] = x;
				i++;	
			}
			if (x < 0) {
				while (i >= 0) {
					free_frame(frames[i]);
					i--;
				}
				return (void*) stats_system_to_user(-ENOMEM); /*12*/
			}
			
			for (i = 0; i < (increment/PAGE_SIZE+1); i++) {
				set_ss_pag(pt, (d/PAGE_SIZE)+i, frames[i]);
				
			}
			d+=increment;
		}
		else {
			int y = PAGE_SIZE-(heapsize%PAGE_SIZE);
			if (y > increment) d += increment;
			else {
				increment -= y;
				d +=y;
				/*INITIALIZE NEW MEMORY SPACE*/
				while (i < (increment/PAGE_SIZE + 1) && x > 0) {
					x = alloc_frame();
					frames[i] = x;
					i++;	
				}
				if (x < 0) {
					while (i >= 0) {
						free_frame(frames[i]);
						i--;
					}
					return (void*) stats_system_to_user(-ENOMEM); /*12*/
				}
				for (i = 0; i < (increment/PAGE_SIZE+1); i++) {
					set_ss_pag(pt, (d/PAGE_SIZE)+i, frames[i]);
				}
				d+=increment;
			}
		}
			
	}
	int previous = tu->task.program_break;
	tu->task.program_break = d;
	set_cr3(get_DIR(&(tu->task)));
	return (void*) stats_system_to_user(previous);
} 

void sys_exit()
{ 
	stats_user_to_system();
	struct task_struct * ts = current();
	int i;
	for (i = 0; i < 2*NR_TASKS; ++i) {
		if (array_semaphore[i].owner == ts->PID) sys_sem_destroy(i);
	}
	//int d = (int) ts->program_break;
	//d = HEAP_START - d;
	//sys_sbrk(d);
	int pos = ts->posPage;
	if (pages_in_use[pos] == 1) free_user_pages(ts);
	--pages_in_use[pos];
	ts->PID = -1;
	update_process_state_rr(current(),&freequeue);
	sched_next_rr();	 
}

