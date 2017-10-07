#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

static void syscall_handler (struct intr_frame *);

/************************ System Calls ************************/
static void sys_halt (void);
static void sys_exit (int status);
static pid_t sys_exec (const char* cmd_line);
static int sys_wait (pid_t);
static bool sys_create (const char* file, unsigned initial_size);
static bool sys_remove (const char* file);
static int sys_open (const char* file);
static int sys_filesize (int fd);
static int sys_read (int fd, void* buffer, unsigned length);
static int sys_write (int fd, const void* buffer, unsigned length);
static void sys_seek (int fd, unsigned position);
static unsigned sys_tell (int fd);
static void sys_close (int fd);

/************************ Memory Access Functions ************************/
static void mem_read(void* dest_addr, void* uaddr, size_t size);
static int mem_read_byte (const uint8_t *uaddr);
static bool mem_write_byte (uint8_t *dest, uint8_t byte);
static void invalid_user_access(void);

/*
 * void syscall_init (void)
 * Description: system call initialization.
 */
void syscall_init (void) {
    intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

/*
 * void syscall_handler (struct intr_frame* f UNUSED)
 *     - Parameters:
 *         - f: interrupt stack frame pointer.
 * Description:
 */
static void syscall_handler (struct intr_frame* f UNUSED) {
    // copy the stack pointer
    void* sp = f->esp;
    // get syscall number
    uint32_t syscall_num;
    mem_read(&syscall_num, sp, sizeof(syscall_num));
    // excute syscall according to syscall number
    switch (syscall_num) {
        /* Halt the operating system. */
        case SYS_HALT:
            // syscall, no return
            sys_halt();
            break;
        /* Terminate this process. */
        case SYS_EXIT:
            // increment stack pointer
            sp = (uint32_t *) sp + 1;
            // get status from stack
            int status;
            mem_read(&status, sp, sizeof(status));
            // syscall, no return
            sys_exit(status);
            break;
        /* Start another process. */
        case SYS_EXEC:
            // increment stack pointer
            sp = (uint32_t *) sp + 1;
            // syscall, return pid_t
            f->eax = sys_exec((char*) *sp);
            break;
        /* Wait for a child process to die. */
        case SYS_WAIT:
            // increment stack pointer
            sp = (uint32_t *) sp + 1;
            // syscall, return int
            f->eax = sys_wait((pid_t) *sp);
            break;
        /* Create a file. */
        case SYS_CREATE:
            // increment stack pointer
            sp = (uint32_t *) sp + 1;
            // syscall, return bool
            f->eax = sys_create (const char* file, unsigned initial_size);

            break;
        /* Delete a file. */
        case SYS_REMOVE:
            // increment stack pointer
            sp = (uint32_t *) sp + 1;
            // syscall, return bool
            f->eax = sys_remove (const char* file);

            
            break;
        /* Open a file. */
        case SYS_OPEN:
            // increment stack pointer
            sp = (uint32_t *) sp + 1;
            // syscall, return int
            f->eax = sys_open (const char* file);
            break;
        /* Obtain a file's size. */
        case SYS_FILESIZE:
            // increment stack pointer
            sp = (uint32_t *) sp + 1;
            // syscall, return int
            f->eax = sys_filesize (int fd);

            break;
        /* Read from a file. */
        case SYS_READ:
            // increment stack pointer
            sp = (uint32_t *) sp + 1;
            // syscall, return int
            f->eax = sys_read (int fd, void* buffer, unsigned length);

            break;
        /* Write to a file. */
        case SYS_WRITE:
            // get fd
            sp = (uint32_t *) sp + 1;
            int fd;
            mem_read(&fd, sp, sizeof(fd));
            // get buffer address
            sp = (uint32_t *) sp + 1;
            void *buffer;
            mem_read(&buffer, sp, sizeof(buffer));
            // get size
            sp = (void **) sp + 1;
            unsigned size;
            mem_read(&size, sp, sizeof(size));
            // syscall, return int
            f->eax = sys_write(fd, buffer, size);
            break;
        /* Change position in a file. */
        case SYS_SEEK:
            // increment stack pointer
            
            // syscall, no return
            sys_seek (int fd, unsigned position);
            break;
        /* Report current position in a file. */
        case SYS_TELL:
            // increment stack pointer
            sp = (uint32_t *) sp + 1;
            // syscall, return
            sys_tell (int fd);

            break;
        /* Close a file. */
        case SYS_CLOSE:
            // increment stack pointer
            sp = (uint32_t *) sp + 1;
            // get fd
            int fd;
            mem_read(&fd, sp, sizeof(fd));
            // syscall, no return
            sys_close (fd);
            break;
            
            
            
        /* Map a file into memory. */
        case SYS_MMAP:
            break;
        /* Remove a memory mapping. */
        case SYS_MUNMAP:
            break;
            
            
        
        /* Change the current directory. */
        case SYS_CHDIR:
            break;
        /* Create a directory. */
        case SYS_MKDIR:
            break;
        /* Reads a directory entry. */
        case SYS_READDIR:
            break;
        /* Tests if a fd represents a directory. */
        case SYS_ISDIR:
            break;
        /* Returns the inode number for a fd. */
        case SYS_INUMBER:
            break;
    }

}

/************************ Memory Access Functions Implementation ************************/

/*
 * void mem_read(void* dest_addr, void* uaddr, size_t size)
 *     - Parameters:
 *         - dest_addr: destination address to save the result of memory read.
 *         - uaddr: starting memory location to be read from.
 *         - size: number of bytes to be read.
 * Description: As part of a system call, the kernel must often access memory through
 *     pointers provided by a user program. The kernel must be very careful about doing so,
 *     because the user can pass a null pointer, a pointer to unmapped virtual memory, or a
 *     pointer to kernel virtual address space (above PHYS_BASE).
 */
static void mem_read(void* dest_addr, void* uaddr, size_t size) {
    // uaddr must be below PHYS_BASE and must not be NULL pointer
    if (uaddr == NULL || !is_user_vaddr(uaddr)) invalid_user_access();
    // read
    for (unsigned int i = 0; i < size; i++) {
        // read a byte from memory
        int byte_data = mem_read_byte(uaddr + i) ;
        // if byte_data = -1, the last memory read was a segment fault
        if (byte_data == -1) invalid_user_access();
        // save this byte of data to destination address
        *(uint8_t*)(dest_addr + i) = byte_data & 0xFF;
    }
}

/*
 * int mem_read_byte (const uint8_t* uaddr)
 *     - Parameters:
 *         - uaddr: address in user space to be read.
 *     - Return: the byte value if successful, -1 if a segfault occurred.
 * Description: Reads a byte at user virtual address uaddr.
 *     uaddr must be below PHYS_BASE.
 *     uaddr points to a byte of memory.
 */
static int mem_read_byte (const uint8_t* uaddr) {
    int result;
    asm ("movl $1f, %0; movzbl %1, %0; 1:"
         : "=&a" (result) : "m" (*uaddr));
    return result;
}

/*
 * bool mem_write_byte (uint8_t* dest, uint8_t byte)
 *     - Parameters: 
 *         - dest_addr: destination address for writing.
 *         - byte: byte of data to be written.
 *     - Return: true if successful, false if a segfault occurred.
 * Description: writes a byte to user address dest_addr.
 *     dest_addr must be below PHYS_BASE.
 */
static bool mem_write_byte (uint8_t* dest_addr, uint8_t byte) {
    int error_code;
    asm ("movl $1f, %0; movb %b2, %1; 1:"
         : "=&a" (error_code), "=m" (*dest_addr) : "q" (byte));
    return error_code != -1;
}

/*
 * void invalid_user_access()
 * Description: for now just exits with status -1
 *     how to free memory and release lock?
 */
static void invalid_user_access() {
    sys_exit(-1);
}


/************************ System Call Implementation ************************/
    
/*
 * void sys_halt (void)
 * Description: terminates Pintos by calling shutdown_power_off() 
 *     (declared in threads/init.h). This should be seldom used, 
 *     because you lose some information about possible deadlock 
 *     situations, etc.
 */
void sys_halt (void) { shutdown_power_off(); }

/*
 * void sys_exit (int status)
 * Discription: terminates the current user program, returning status 
 *     to the kernel. If the process's parent waits for it (see below), 
 *     this is the status that will be returned. Conventionally, a status 
 *     of 0 indicates success and nonzero values indicate errors.
 */
void sys_exit (int status) {
    //
    printf("%s: exit(%d)\n", thread_current()->name, status);
    // assign status
    
    //
    thread_exit();
}

/*
 * pid_t sys_exec (const char* cmd_line)
 *     - Parameters:
 *         - command: command to be excuted, followed by its arguments.
 *     - Return: the new process's program id (pid). Must return pid -1,
 *           which otherwise should not be a valid pid, if the program
 *           cannot load or run for any reason.
 * Discription: runs the executable whose name is given in command, passing
 *     any given arguments. The parent process cannot return from the exec
 *     until it knows whether the child process successfully loaded its executable.
 *     You must use appropriate synchronization to ensure this.
 */
pid_t sys_exec (const char* cmd_line) {
    return 0;
}

/*
 * int sys_wait(pid_t pid)
 *     - Parameters:
 *         - pid: the pid of a child process to be waiting for.
 *     - Return: the status that pid passed to exit. If pid did not call exit(),
 *           but was terminated by the kernel (e.g. killed due to an exception), wait(pid)
 *           must return -1.
 * Description: Waits for a child process pid and retrieves the child's exit status.
 * Function Calls:
 *     - [int process_wait(tid_t child_tid)] in process.h
 */
int sys_wait(pid_t pid) { return process_wait(pid); }

/*
 * bool sys_create (const char* file, unsigned initial_size)
 *     - Parameters:         
 *         - file: file name for the file to be created.
 *         - initial_size: size in bytes for the file to be created.
 *     - Return: true if successful, false otherwise.
 * Description: creates a new file called file initially initial_size bytes in size.
 */
bool sys_create (const char* file, unsigned initial_size) {

}

/*
 * bool sys_remove (const char* file)
 *     - Parameters:
 *         - file: file name of the file to be deleted.
 *     - Return: true if successful, false otherwise.
 * Description: deletes the file called file. A file may be removed regardless of whether
 *     it is open or closed, and removing an open file does not close it.
 */
bool sys_remove (const char* file) {
    
}

/*
 * int sys_open (const char *file)
 *     - Parameters:
 *         - file:
 *     - Return: a nonnegative integer handle called a "file descriptor" (fd),
 *           or -1 if the file could not be opened.
 * Description: opens the file called file.
 */
int sys_open (const char* file) {

}

/*
 * int sys_filesize (int fd)
 *     - Parameters:
 *         - fd: file descriptor of the file to be checked.
 *     - Return: size of file in bytes.
 * Description: returns the size, in bytes, of the file open as fd.
 */
int sys_filesize (int fd) {

}

/*
 * int sys_read (int fd, void *buffer, unsigned size)
 *     - Parameters:
 *         - fd: file descriptor to the file to be read.
 *         - buffer: buffer for data read.
 *         - size: size, in bytes, to be read.
 *     - Return: the number of bytes actually read (0 at end of file), or -1 if
 *           the file could not be read (due to a condition other than end of file).
 * Description: reads size bytes from the file open as fd into buffer. Fd 0 reads from
 *     the keyboard using input_getc().
 */
int sys_read (int fd, void* buffer, unsigned length) {
    
}

/*
 * int sys_write (int fd, const void* buffer, unsigned size)
 *     - Parameters:
 *         - fd: file descriptor for the file to be writen.
 *         - buffer: buffer for data to be writen.
 *         - size: size of data to be written.
 *     - Return: the number of bytes actually written, which may be less than size
 *           if some bytes could not be written.
 * Description: writes size bytes from buffer to the open file fd. Fd 1 writes to the console.
 *     Your code to write to the console should write all of buffer in one call to putbuf(),
 *     at least as long as size is not bigger than a few hundred bytes.
 */
int sys_write (int fd, const void* buffer, unsigned length) {

}

/*
 * void sys_seek (int fd, unsigned position)
 *     - Parameters:
 *         - fd: file descriptor of file to be read or written.
 *         - position:
 * Description: changes the next byte to be read or written in open file fd to position, expressed
 *     in bytes from the beginning of the file. (Thus, a position of 0 is the file's start.)
 */
void sys_seek (int fd, unsigned position) {
}

/*
 * unsigned sys_tell (int fd)
 *     - Parameters:
 *         - fd: file descriptor of file to be read or written.
 *     - Return: position of the next byte to be read or written.
 * Description: returns the position of the next byte to be read or written in open file fd, expressed
 *     in bytes from the beginning of the file.
 */
unsigned sys_tell (int fd) {

}

/*
 * void sys_close (int fd)
 *     - Parameters: file descriptor to be closed.
 * Description: closes file descriptor fd.
 */
void sys_close (int fd) {

}

