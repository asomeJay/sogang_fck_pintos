#include "userprog/process.h"
#include <debug.h>
#include <inttypes.h>
#include <round.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "userprog/gdt.h"
#include "userprog/pagedir.h"
#include "userprog/tss.h"
#include "filesys/directory.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "threads/flags.h"
#include "threads/init.h"
#include "threads/interrupt.h"
#include "threads/palloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
//int ffllaagg = 0;
static thread_func start_process NO_RETURN;
static bool load (const char *cmdline, void (**eip) (void), void **esp);

/* Starts a new thread running a user program loaded from
   FILENAME.  The new thread may be scheduled (and may even exit)
   before process_execute() returns.  Returns the new process's
   thread id, or TID_ERROR if the thread cannot be created. */
tid_t
process_execute (const char *file_name) 
{
  char *fn_copy;
  int i = 0, count = 0;
  tid_t tid;
  char* command_name;
  for ( i = 0; file_name[i] != ' ' && file_name[i] != '\0'; i++);
  command_name = ((char *)malloc(sizeof(char) * (i + 1)));
  strlcpy(command_name, file_name, i+1);
  command_name[i] = '\0';
//	printf("PID : %d\n", thread_current()->tid);
// printf("%s\n", command_name);
  //printf("=====---%s\n",file_name);
  /* Make a copy of FILE_NAME.
     Otherwise there's a race between the caller and load(). */
  fn_copy = palloc_get_page (0);
  if (fn_copy == NULL)
    return TID_ERROR;
  strlcpy (fn_copy, file_name, PGSIZE);
 // printf("\n---- second %s\n", fn_copy);
  /* Create a new thread to execute FILE_NAME. */
	if(filesys_open(command_name) == NULL){
		return -1;
	}
  tid = thread_create (command_name, PRI_DEFAULT, start_process, fn_copy);
  free(command_name);
  sema_down(&thread_current()->exe_child);
  if (tid == TID_ERROR)
    palloc_free_page (fn_copy); 
  //printf("\n ---process_execute pid : %d---\n", tid);
	
  struct list_elem * ele = NULL;
  struct thread * thr = NULL;
  for ( ele = list_begin(&thread_current()->child_thread); ele != list_end(&thread_current()->child_thread); ele = list_next(ele)){
  thr = list_entry(ele, struct thread, child_thread_elem);
//	printf("FUCKINGSHI\n");
  if(thr->flag == 1){
//	printf("HIHIHIHIHIHI\n");
//	printf("return process waiting\n");
	return process_wait(tid);
  }
}
//printf("asdasdad");
  return tid;
}


// !!!!
void esp_stack(char *file_name, void **esp){
	char ** argv;
	char ** arg_addr;
	int argc = 0;
	int data_stack_len=0;
	int i;
	int word_align;
	char temp[128];
	char *one_arg;
	char *next_ptr;
	int blanking = 0;

	for(i=0; i<(int)strlen(file_name); i++){
		// 전체 argument 개수구하기 for malloc
		if(file_name[i]==' '&&file_name[i]!='\0' && blanking==0){
			argc +=1;
			blanking = 1;
		}else{
			blanking = 0;
		}
	}
	// 1. arg 개수 구하기
	// 파일이름 변수 변수이면 띄어쓰기 두개, 변수 두개만 나오므로 끝에 더하
	argc +=1;

	// 2. arg 변수별로 자르기 
	argv = (char **) malloc(sizeof(char *) * argc);
	arg_addr = (char **) malloc(sizeof(char *) * argc);
	// 원하는 길이만큼 copy
	strlcpy(temp, file_name, strlen(file_name) + 1);
	one_arg = strtok_r(temp, " ",&next_ptr);
	for(i=0; i< argc;i++){
		// ""단위로 자르고 next_ptr로 이
		argv[i] = one_arg;
		one_arg = strtok_r(NULL, " ", &next_ptr);
	}
	//printf("\n--%d------%s  --- %s",argc,argv[1],file_name);
	// 여기까지 OK

	//3. 자른거 주소값 집어넣기(거꾸로 집어넣어야 한다) 
	for(i = argc-1; i>=0; i--){
		*esp -= strlen(argv[i]) +1;
		data_stack_len = data_stack_len + strlen(argv[i])+ 1;
		//printf("\n---%s---\n",argv[i]);
		// 복사하기
		strlcpy(*esp, argv[i], strlen(argv[i]) + 1);
		arg_addr[i] = *esp;
	}

	//4. WORD ALIGN 계산하기
	if(data_stack_len%4!=0){
		word_align = 4-(data_stack_len %4);
	}else{
		word_align = 0;
	}
	*esp = *esp - word_align;

	//5. NULL 집어넣기
	*esp -=4;
	// 주소값을 통째로 집어넣는거니까
	**(uint32_t **) esp = 0;

	// 6. 그 주소값 집어넣기
	for(i = argc-1; i>=0; i--){
		*esp = *esp-4;
		// 주소값을 통째로 집어넣는 거니까
		**(uint32_t **) esp = arg_addr[i];
	}

//	printf("%p --- %p", arg_addr[0], arg_addr[1]);
	
	// 7. argv 주소 집어넣기
	*esp -= 4;
	**(uint32_t **)esp = *esp + 4;
//	printf("esp : %p",esp);
	// 8. argc 집어넣기
	*esp = *esp -4;
	**(uint32_t **)esp = argc;

	free(argv);
	free(arg_addr);
	
	// 9. return address 넣기
	*esp = *esp-4;
	**(uint32_t **)esp = 0;
	//offset, buffer, size, ascii
//	hex_dump(*esp,*esp,100,1);
//	free(argv);
//	free(ard_addr);
//	free(temp);	
}
// @@@

/* A thread function that loads a user process and starts it
   running. */
static void
start_process (void *file_name_)
{
  char *file_name = file_name_;
  char real_file_name[128]; // 4kb도 있는데 이걸로 해도 될듯. 
  struct intr_frame if_;
  bool success;

  // !!!! filname은 run 뒤에 모든 변수 다 들어온다.
  //printf("------%s----\n", file_name);
  int idx=0;
  // 끝도 아니고 띄어쓰기 앞까지
  while(file_name[idx] != ' ' && file_name[idx]!= '\0')
  {  real_file_name[idx] = file_name[idx];
	 idx++;
  }
  // 끝맺음도 해줘야지
  real_file_name[idx]='\0';	
  // @@@@@

  /* Initialize interrupt frame and load executable. */
  memset (&if_, 0, sizeof if_);
  if_.gs = if_.fs = if_.es = if_.ds = if_.ss = SEL_UDSEG;
  if_.cs = SEL_UCSEG;
  if_.eflags = FLAG_IF | FLAG_MBS;
  success = load (real_file_name, &if_.eip, &if_.esp);
//  free(real_file_name);
  sema_up(&thread_current()->parent->exe_child);	
  // !!!!
  if(success){
	// setup이 완료되었다면
	 esp_stack(file_name, &if_.esp);
  }
  // @@@@
  /* If load failed, quit. */
  palloc_free_page (file_name);
//printf("PIDPIDUP : %d\n", thread_current()->tid);
//  sema_up(&thread_current()->parent->exe_child);
//printf("PIDPIDDOWN: %d\n", thread_current()->tid);
  if (!success) {
//	ffllaagg = 1;
	thread_current()->flag = 1;
//	thread_exit();
	exit(-1);
}
//    thread_exit ();

  /* Start the user process by simulating a return from an
     interrupt, implemented by intr_exit (in
     threads/intr-stubs.S).  Because intr_exit takes all of its
     arguments on the stack in the form of a `struct intr_frame',
     we just point the stack pointer (%esp) to our stack frame
     and jump to it. */
  asm volatile ("movl %0, %%esp; jmp intr_exit" : : "g" (&if_) : "memory");
  NOT_REACHED ();
}

/* Waits for thread TID to die and returns its exit status.  If
   it was terminated by the kernel (i.e. killed due to an
   exception), returns -1.  If TID is invalid or if it was not a
   child of the calling process, or if process_wait() has already
   been successfully called for the given TID, returns -1
   immediately, without waiting.

   This function will be implemented in problem 2-2.  For now, it
   does nothing. */
int
process_wait (tid_t child_tid ) 
{
	int exit_status;
	struct thread* cur = thread_current();
	struct thread* cur_thread = NULL;
	struct list_elem* temp = NULL;
	//printf("enter,,,\n");
	for (temp = list_begin(&cur->child_thread); temp != list_end(&cur->child_thread); 
		temp = list_next(temp)) {
		cur_thread = list_entry(temp, struct thread, child_thread_elem);
//		printf("at process_wait... child_tid : %d cur_thread->tid : %d\n", child_tid, cur_thread->tid);
		if (child_tid == cur_thread->tid) {
//			cur_thread->already_wait = 1;
			sema_down(&(cur_thread->memory_preserve));

			exit_status = cur_thread->child_exit_status;
			list_remove(&(cur_thread->child_thread_elem));
			sema_up(&(cur_thread->child_thread_lock));
			return exit_status;
		}
	}
	return -1;
}

/* Free the current process's resources. */
void
process_exit (void)
{
  struct thread *cur = thread_current ();
  uint32_t *pd;

  /* Destroy the current process's page directory and switch back
     to the kernel-only page directory. */
  pd = cur->pagedir;
  if (pd != NULL) 
    {
      /* Correct ordering here is crucial.  We must set
         cur->pagedir to NULL before switching page directories,
         so that a timer interrupt can't switch back to the
         process page directory.  We must activate the base page
         directory before destroying the process's page
         directory, or our active page directory will be one
         that's been freed (and cleared). */
      cur->pagedir = NULL;
      pagedir_activate (NULL);
      pagedir_destroy (pd);
    }
  sema_up(&(cur->memory_preserve)); 
  sema_down(&(cur->child_thread_lock)); 
}

/* Sets up the CPU for running user code in the current
   thread.
   This function is called on every context switch. */
void
process_activate (void)
{
  struct thread *t = thread_current ();

  /* Activate thread's page tables. */
  pagedir_activate (t->pagedir);

  /* Set thread's kernel stack for use in processing
     interrupts. */
  tss_update ();
}

/* We load ELF binaries.  The following definitions are taken
   from the ELF specification, [ELF1], more-or-less verbatim.  */

/* ELF types.  See [ELF1] 1-2. */
typedef uint32_t Elf32_Word, Elf32_Addr, Elf32_Off;
typedef uint16_t Elf32_Half;

/* For use with ELF types in printf(). */
#define PE32Wx PRIx32   /* Print Elf32_Word in hexadecimal. */
#define PE32Ax PRIx32   /* Print Elf32_Addr in hexadecimal. */
#define PE32Ox PRIx32   /* Print Elf32_Off in hexadecimal. */
#define PE32Hx PRIx16   /* Print Elf32_Half in hexadecimal. */

/* Executable header.  See [ELF1] 1-4 to 1-8.
   This appears at the very beginning of an ELF binary. */
struct Elf32_Ehdr
  {
    unsigned char e_ident[16];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
  };

/* Program header.  See [ELF1] 2-2 to 2-4.
   There are e_phnum of these, starting at file offset e_phoff
   (see [ELF1] 1-6). */
struct Elf32_Phdr
  {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
  };

/* Values for p_type.  See [ELF1] 2-3. */
#define PT_NULL    0            /* Ignore. */
#define PT_LOAD    1            /* Loadable segment. */
#define PT_DYNAMIC 2            /* Dynamic linking info. */
#define PT_INTERP  3            /* Name of dynamic loader. */
#define PT_NOTE    4            /* Auxiliary info. */
#define PT_SHLIB   5            /* Reserved. */
#define PT_PHDR    6            /* Program header table. */
#define PT_STACK   0x6474e551   /* Stack segment. */

/* Flags for p_flags.  See [ELF3] 2-3 and 2-4. */
#define PF_X 1          /* Executable. */
#define PF_W 2          /* Writable. */
#define PF_R 4          /* Readable. */

static bool setup_stack (void **esp);
static bool validate_segment (const struct Elf32_Phdr *, struct file *);
static bool load_segment (struct file *file, off_t ofs, uint8_t *upage,
                          uint32_t read_bytes, uint32_t zero_bytes,
                          bool writable);



/* Loads an ELF executable from FILE_NAME into the current thread.
   Stores the executable's entry point into *EIP
   and its initial stack pointer into *ESP.
   Returns true if successful, false otherwise. */
bool
load (const char *file_name, void (**eip) (void), void **esp) 
{
  struct thread *t = thread_current ();
  struct Elf32_Ehdr ehdr;
  struct file *file = NULL;
  off_t file_ofset;
  bool success = false;
  int i;

  /* Allocate and activate page directory. */
  t->pagedir = pagedir_create ();
  if (t->pagedir == NULL) 
    goto done;
  process_activate ();

  /* Open executable file. */
  file = filesys_open (file_name);
  if (file == NULL) 
    {
      printf ("load: %s: open failed\n", file_name);
      goto done; 
    }

  /* Read and verify executable header. */
  if (file_read (file, &ehdr, sizeof ehdr) != sizeof ehdr
      || memcmp (ehdr.e_ident, "\177ELF\1\1\1", 7)
      || ehdr.e_type != 2
      || ehdr.e_machine != 3
      || ehdr.e_version != 1
      || ehdr.e_phentsize != sizeof (struct Elf32_Phdr)
      || ehdr.e_phnum > 1024) 
    {
      printf ("load: %s: error loading executable\n", file_name);
      goto done; 
    }

  /* Read program headers. */
  file_ofset = ehdr.e_phoff;
  for (i = 0; i < ehdr.e_phnum; i++) 
    {
      struct Elf32_Phdr phdr;

      if (file_ofset < 0 || file_ofset > file_length (file))
        goto done;
      file_seek (file, file_ofset);

      if (file_read (file, &phdr, sizeof phdr) != sizeof phdr)
        goto done;
      file_ofset += sizeof phdr;
      switch (phdr.p_type) 
        {
        case PT_NULL:
        case PT_NOTE:
        case PT_PHDR:
        case PT_STACK:
        default:
          /* Ignore this segment. */
          break;
        case PT_DYNAMIC:
        case PT_INTERP:
        case PT_SHLIB:
          goto done;
        case PT_LOAD:
          if (validate_segment (&phdr, file)) 
            {
              bool writable = (phdr.p_flags & PF_W) != 0;
              uint32_t file_page = phdr.p_offset & ~PGMASK;
              uint32_t mem_page = phdr.p_vaddr & ~PGMASK;
              uint32_t page_offset = phdr.p_vaddr & PGMASK;
              uint32_t read_bytes, zero_bytes;
              if (phdr.p_filesz > 0)
                {
                  /* Normal segment.
                     Read initial part from disk and zero the rest. */
                  read_bytes = page_offset + phdr.p_filesz;
                  zero_bytes = (ROUND_UP (page_offset + phdr.p_memsz, PGSIZE)
                                - read_bytes);
                }
              else 
                {
                  /* Entirely zero.
                     Don't read anything from disk. */
                  read_bytes = 0;
                  zero_bytes = ROUND_UP (page_offset + phdr.p_memsz, PGSIZE);
                }
              if (!load_segment (file, file_page, (void *) mem_page,
                                 read_bytes, zero_bytes, writable))
                goto done;
            }
          else
            goto done;
          break;
        }
    }

  /* Set up stack. */
  if (!setup_stack (esp))
    goto done;
  /* Start address. */
  *eip = (void (*) (void)) ehdr.e_entry;

  success = true;

 done:
  /* We arrive here whether the load is successful or not. */
  file_close (file);
  return success;
}


/* load() helpers. */

static bool install_page (void *upage, void *kpage, bool writable);

/* Checks whether PHDR describes a valid, loadable segment in
   FILE and returns true if so, false otherwise. */
static bool
validate_segment (const struct Elf32_Phdr *phdr, struct file *file) 
{
  /* p_offset and p_vaddr must have the same page offset. */
  if ((phdr->p_offset & PGMASK) != (phdr->p_vaddr & PGMASK)) 
    return false; 

  /* p_offset must point within FILE. */
  if (phdr->p_offset > (Elf32_Off) file_length (file)) 
    return false;

  /* p_memsz must be at least as big as p_filesz. */
  if (phdr->p_memsz < phdr->p_filesz) 
    return false; 

  /* The segment must not be empty. */
  if (phdr->p_memsz == 0)
    return false;
  
  /* The virtual memory region must both start and end within the
     user address space range. */
  if (!is_user_vaddr ((void *) phdr->p_vaddr))
    return false;
  if (!is_user_vaddr ((void *) (phdr->p_vaddr + phdr->p_memsz)))
    return false;

  /* The region cannot "wrap around" across the kernel virtual
     address space. */
  if (phdr->p_vaddr + phdr->p_memsz < phdr->p_vaddr)
    return false;

  /* Disallow mapping page 0.
     Not only is it a bad idea to map page 0, but if we allowed
     it then user code that passed a null pointer to system calls
     could quite likely panic the kernel by way of null pointer
     assertions in memcpy(), etc. */
  if (phdr->p_vaddr < PGSIZE)
    return false;

  /* It's okay. */
  return true;
}

/* Loads a segment starting at offset OFS in FILE at address
   UPAGE.  In total, READ_BYTES + ZERO_BYTES bytes of virtual
   memory are initialized, as follows:

        - READ_BYTES bytes at UPAGE must be read from FILE
          starting at offset OFS.

        - ZERO_BYTES bytes at UPAGE + READ_BYTES must be zeroed.

   The pages initialized by this function must be writable by the
   user process if WRITABLE is true, read-only otherwise.

   Return true if successful, false if a memory allocation error
   or disk read error occurs. */
static bool
load_segment (struct file *file, off_t ofs, uint8_t *upage,
              uint32_t read_bytes, uint32_t zero_bytes, bool writable) 
{
  ASSERT ((read_bytes + zero_bytes) % PGSIZE == 0);
  ASSERT (pg_ofs (upage) == 0);
  ASSERT (ofs % PGSIZE == 0);

  file_seek (file, ofs);
  while (read_bytes > 0 || zero_bytes > 0) 
    {
      /* Calculate how to fill this page.
         We will read PAGE_READ_BYTES bytes from FILE
         and zero the final PAGE_ZERO_BYTES bytes. */
      size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
      size_t page_zero_bytes = PGSIZE - page_read_bytes;

      /* Get a page of memory. */
      uint8_t *knpage = palloc_get_page (PAL_USER);
      if (knpage == NULL)
        return false;

      /* Load this page. */
      if (file_read (file, knpage, page_read_bytes) != (int) page_read_bytes)
        {
          palloc_free_page (knpage);
          return false; 
        }
      memset (knpage + page_read_bytes, 0, page_zero_bytes);

      /* Add the page to the process's address space. */
      if (!install_page (upage, knpage, writable)) 
        {
          palloc_free_page (knpage);
          return false; 
        }

      /* Advance. */
      read_bytes -= page_read_bytes;
      zero_bytes -= page_zero_bytes;
      upage += PGSIZE;
    }
  return true;
}

/* Create a minimal stack by mapping a zeroed page at the top of
   user virtual memory. */
static bool
setup_stack (void **esp) 
{
  uint8_t *kpage;
  bool success = false;
  kpage = palloc_get_page (PAL_USER | PAL_ZERO);
  if (kpage != NULL) 
    {
      success = install_page (((uint8_t *) PHYS_BASE) - PGSIZE, kpage, true);
      if (success)
        *esp = PHYS_BASE;
      else
        palloc_free_page (kpage);
    }
  return success;
}

/* Adds a mapping from user virtual address UPAGE to kernel
   virtual address KPAGE to the page table.
   If WRITABLE is true, the user process may modify the page;
   otherwise, it is read-only.
   UPAGE must not already be mapped.
   KPAGE should probably be a page obtained from the user pool
   with palloc_get_page().
   Returns true on success, false if UPAGE is already mapped or
   if memory allocation fails. */
static bool
install_page (void *upage, void *kpage, bool writable)
{
  struct thread *th = thread_current ();

  /* Verify that there's not already a page at that virtual
     address, then map our page there. */
  return (pagedir_get_page (th->pagedir, upage) == NULL
          && pagedir_set_page (th->pagedir, upage, kpage, writable));
}
