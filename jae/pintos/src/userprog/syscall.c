#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);
void halt(void);
void exit(int status);
pid_t exec(const char* cmd_lines);
int wait(pid_t pid);
bool create(const char* file, unsigned initial_size);
bool remove(const char* file);
int open(const char* file);
int filesize(int fd);
int read(int fd, void* buffer, unsigned size);
int write(int fd, const void* buffer, unsigned size);
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);

#define WORD sizeof(uint32_t)

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

//####
static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  printf ("system call!\n");

  switch (*(uint32_t*)(f->esp)) {
  case SYS_HALT:                   /* Halt the operating system. */
	  halt();
	  break;
  case SYS_EXIT:					/* Terminate this process. */
	  exit((int) *(uint32_t *) (f->esp + WORD);
	  break;
  case SYS_EXEC:                   /* Start another process. */
	  
	  break;
  case SYS_WAIT:                   /* Wait for a child process to die. */
	  break;
  case SYS_CREATE:                 /* Create a file. */
	  break;
  case SYS_REMOVE:                 /* Delete a file. */
	  break;
  case SYS_OPEN:                   /* Open a file. */
	  break;
  case SYS_FILESIZE:               /* Obtain a file's size. */
	  break;
  case SYS_READ:                   /* Read from a file. */
	  break;
  case SYS_WRITE:                  /* Write to a file. */
	  break;
  case SYS_SEEK:                   /* Change position in a file. */
	  break;
  case SYS_TELL:                   /* Report current position in a file. */
	  break;
  case SYS_CLOSE:                  /* Close a file. */
	  break;
	  //####
  case SYS_FIBBO :
	  break;
  case SYS_SUM:
	  break;
	  //$$$$
  default : 
	  printf("userprog/Syscall.c/Function System_Handler Error breaks out \n");
  }
  thread_exit ();
}

void halt(void) {
	printf("userprog/syscall.c/halt start\n");
	shutdown_power_off();
	printf("userprog/syscall.c/halt end\n");
}

void exit(int status) {
	printf("userprog/syscall.c/exit start\n");
	thread_exit();
	printf("userprog/syscall.c/exit end\n");
}

pid_t exec(const char* cmd_lines) {
	return process_execute(cmd_lines) - 1;
}

int wait(pid_t pid) {
	printf("userprog/syscall.c/wait start\n");
	return process_wait((tid_t)pid);
	printf("userprog/syscall.c/wait end\n");

}
bool create(const char* file, unsigned initial_size);
bool remove(const char* file);
int open(const char* file);
int filesize(int fd);
int read(int fd, void* buffer, unsigned size);
int write(int fd, const void* buffer, unsigned size) {

}
void seek(int fd, unsigned position);
unsigned tell(int fd);
void close(int fd);
//$$$$