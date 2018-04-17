#include <libc.h>
#include <stats.h>

char buff[24];

int __attribute__ ((__section__(".text.main")))
  main(void)
{
    /* Next line, tries to move value 0 to CR3 register. This register is a privileged one, and so it will raise an exception */
     /* __asm__ __volatile__ ("mov %0, %%cr3"::"r" (0) ); */
	runjp();
	/*int x =	sbrk(10);
	itoa (x, buff);
	write(1,buff,24);
	write (1, " ", 1);

	x = sbrk(-5);
	char buff2[24];
	itoa (x, buff2);
	write(1,buff2,7);

/*
	write (1, " ", 1);
	x = sbrk(0);
	char buff3[24];
	itoa (x, buff3);
	write(1,buff3,7);*/
	/*sem_init(1,0);
	int pid = fork();	
	if (pid == 0)	{
		sem_wait(1);
		char buff1[10];		
		read(1, buff1, 10);
		write(1,buff1,10);
	}
	else {
		char buff2[10];
		read(1,buff2,10);
		write(1,buff2,10);
		sem_signal(1);
	}
	
	//perror();
	//write(1,buff,10);*/
  while(1) { }
}
