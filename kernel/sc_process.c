/**
 * kernel/sc_process.c
 *
 * Part of P-OS kernel.
 *
 * Contains process related syscalls
 *
 * Written by Peter Bosch <peterbosc@gmail.com>
 *
 * Changelog:
 * 07-04-2014 - Created
 */
#include <sys/errno.h>
#include <string.h>
#include "kernel/heapmm.h"
#include "kernel/physmm.h"
#include "kernel/paging.h"
#include "kernel/process.h"
#include "kernel/scheduler.h"
#include "kernel/permissions.h"
#include "kernel/synch.h"
#include "kernel/syscall.h"
#include "kernel/streams.h"

/**
 * @brief Syscall implementation: fork
 * Create a copy of a 
 */
uint32_t sys_fork(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e, uint32_t f)
{


	/* Call the schedulur to actually do the fork */
	return (uint32_t) scheduler_fork();
	
}

/**
 * @brief Syscall implementation: kill
 * Sends a signal to a process.
 */
uint32_t sys_kill(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e, uint32_t f)
{
	struct siginfo info;
	process_info_t *process;
	
	memset( &info, 0, sizeof( struct siginfo ) );

	/* The process to signal */
	int pid = (int) a;
	
	/* The signal to send */
	int sig = (int) b;

	/* Do a range check on sid */	
	if (sig < 0 || sig > 31) {
		syscall_errno = EINVAL;
		return (uint32_t)-1;		
	} 

	/* Fill info */
	info.si_code = SI_USER;
	info.si_pid  = scheduler_current_task->pid;
	info.si_uid  = scheduler_current_task->uid;
	
	/* Handle various cases for pid */
	if (pid == 0) {		   
	
		/* pid = 0  -> Send signal to current process group */
		syscall_errno = ESRCH;
		if (process_signal_pgroup(
			scheduler_current_task->pgid, sig, info ) == 0) {
			return (uint32_t) -1;
		} 
		syscall_errno = 0;
		return 0;
		
	} else if (pid == -1) {
		/* pid = -1 -> Send signal to all processes except init and things we
		   aren't allowed to signal. */
		//TODO: Implement kill all system processes
		syscall_errno = EINVAL;
		return (uint32_t)-1;
		
	} else if (pid < -1) {
		/* pid < -1 -> Send signal to process group -pgid */
	
		syscall_errno = ESRCH;
		if (process_signal_pgroup((pid_t) -pid, sig, info) == 0) {
			return (uint32_t) -1;
		} 
		syscall_errno = 0;
		return 0;
		
	} else {
		/* Signal process with id pid */
		
		process = process_get((pid_t) pid);
		if (process == NULL) {
			syscall_errno = ESRCH;
			return (uint32_t) -1;			
		}
		if (get_perm_class(process->uid, process->gid) != PERM_CLASS_OWNER){
			syscall_errno = EPERM;
			return (uint32_t) -1;
		}	
		process_send_signal(process, sig, info);
		return 0;
	}		
}

uint32_t sys_getpid(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e, uint32_t f)
{
	return (pid_t) scheduler_current_task->pid;
}

uint32_t sys_getppid(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e, uint32_t f)
{
	return (pid_t) scheduler_current_task->parent_pid;
}

uint32_t sys_getpgrp(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e, uint32_t f)
{
	return (pid_t) scheduler_current_task->pgid;
}

uint32_t sys_getsid(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e, uint32_t f)
{
	return (pid_t) scheduler_current_task->sid;
}

uint32_t sys_setsid(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e, uint32_t f)
{
	if (scheduler_current_task->pgid == scheduler_current_task->pid) {
		syscall_errno = EPERM;
		return (uint32_t) -1;	
	}
	scheduler_current_task->pgid = scheduler_current_task->pid;
	scheduler_current_task->sid = scheduler_current_task->pid;
	scheduler_current_task->ctty = 0;
	return (pid_t) scheduler_current_task->sid;
}

uint32_t sys_setpgrp(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e, uint32_t f)
{
	if (scheduler_current_task->pgid == scheduler_current_task->pid) {
		syscall_errno = EPERM;
		return (uint32_t) -1;	
	}
	scheduler_current_task->pgid = scheduler_current_task->pid;
	return 0;
}

uint32_t sys_setpgid(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e, uint32_t f)
{
	process_info_t *task;
	pid_t pid = (pid_t) a;//TODO: FIX PERMS
	pid_t pgid = (pid_t) b;
	if (pid == 0)
		pid = scheduler_current_task->pid;
	if (pgid == 0)
		pgid = pid;
	if (pgid < 0) {
		syscall_errno = EINVAL;
		return (uint32_t) -1;
	}
	task = process_get(pid);
	if (!task) {
		syscall_errno = ESRCH;
		return (uint32_t) -1;	
	}
	task->pgid = pgid;
	return 0;
}

uint32_t sys_exit(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e, uint32_t f)
{
	scheduler_current_task->exit_status = a;
	scheduler_current_task->state = PROCESS_KILLED;
	process_child_event(scheduler_current_task, PROCESS_CHILD_KILLED);
	stream_do_close_all (scheduler_current_task);
	procvmm_clear_mmaps();
	schedule();
	return 0; // NEVER REACHED
}

uint32_t sys_yield(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e, uint32_t f)
{
	schedule();
	return 0;
}

#define WNOHANG 1
#define WUNTRACED 2

//pid_t pid, int *status, int options
uint32_t sys_waitpid(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e, uint32_t f)
{
	int status, options;
	pid_t pid;
	process_child_event_t *ev_info;
	process_info_t *chld;
	if (!copy_user_to_kern((void *)b, &status, sizeof(int))) {
		syscall_errno = EFAULT;
		return -1;
	}
	pid = (pid_t) a;
	options = (int) c;

	if (pid == 0)
		pid = -(scheduler_current_task->pid);
	if (pid < -1) {
		//TODO: Determine whether pgroup exists
		if (options & WNOHANG) {
			while (  (!(ev_info = process_get_event_pg(-pid))) || 
				 ((ev_info->event == PROCESS_CHILD_STOPPED) && !(options & WUNTRACED)) || 
				 (ev_info->event == PROCESS_CHILD_CONTD)) {

				if (!ev_info)	
					return 0;	

				process_absorb_event(ev_info);
			}
			
		} else {
			while (  (!(ev_info = process_get_event_pg(-pid))) || 
				 ((ev_info->event == PROCESS_CHILD_STOPPED) && !(options & WUNTRACED)) || 
				 (ev_info->event == PROCESS_CHILD_CONTD)) {

				if (ev_info)		
					process_absorb_event(ev_info);

				if (semaphore_idown(scheduler_current_task->child_sema)) {
					syscall_errno = EINTR;
					return -1;
				}
			}
		}
	} else if (pid == -1) {
		if (options & WNOHANG) {
			while (  
				(!(ev_info = (process_child_event_t *) 
					llist_get_last(scheduler_current_task->child_events))) ||
				 ((ev_info->event == PROCESS_CHILD_STOPPED) && !(options & WUNTRACED)) || 
				 (ev_info->event == PROCESS_CHILD_CONTD)) {

				if (ev_info == NULL)	
					return 0;	

				process_absorb_event(ev_info);
			}
		} else {
			while (  (!(ev_info = (process_child_event_t *) llist_get_last(scheduler_current_task->child_events)))  ||
				 ((ev_info->event == PROCESS_CHILD_STOPPED) && !(options & WUNTRACED)) || 
				 (ev_info->event == PROCESS_CHILD_CONTD)) {

				if (ev_info != NULL)	
					process_absorb_event(ev_info);

				if (semaphore_idown(scheduler_current_task->child_sema)) {
					syscall_errno = EINTR;
					return -1;
				}
			}
		}
	} else if (pid > 0) {
		chld = process_get(pid);
		if (!chld) {
			syscall_errno = ECHILD;
			return -1;
		}
		if (options & WNOHANG) {
			while (  (!(ev_info = process_get_event(pid))) || 
				 ((ev_info->event == PROCESS_CHILD_STOPPED) && !(options & WUNTRACED)) || 
				 (ev_info->event == PROCESS_CHILD_CONTD)) {
				if (!ev_info)	
					return 0;	
				process_absorb_event(ev_info);
			}
			
		} else {
			while (  (!(ev_info = process_get_event(pid))) || 
				 ((ev_info->event == PROCESS_CHILD_STOPPED) && !(options & WUNTRACED)) || 
				 (ev_info->event == PROCESS_CHILD_CONTD)) {
				if (ev_info)		
					process_absorb_event(ev_info);

				if (semaphore_idown(scheduler_current_task->child_sema)) {
					syscall_errno = EINTR;
					return -1;
				}
			}
		}
	}
	chld = process_get(ev_info->child_pid);
	pid = ev_info->child_pid;
	switch (ev_info->event) {
		case PROCESS_CHILD_KILLED:
			switch (chld->term_cause) {
				case PROCESS_TERM_SIGNAL:
					status = chld->last_signal;
					break;
				case PROCESS_TERM_EXIT:
					status = ((chld->exit_status) << 8) & 0xFF00;
					break;
			}
			process_reap(chld);
			break;
		case PROCESS_CHILD_STOPPED:
			if(options & WUNTRACED) 
				status = (((chld->last_signal) << 8) & 0xFF00) | 0x7f;
			else
				return 0;
			break;
		case PROCESS_CHILD_CONTD://TODO: Implement this in libc, linux only hehehe
			//if(options & WCONTINUED) 
			//	status = (((chld->last_signal) << 8) & 0xFF00) | 0x7f;
			//else
				return 0;
			//break;
	}
	process_absorb_event(ev_info);
	if (!copy_kern_to_user(&status, (void *)b, sizeof(int))) {
		syscall_errno = EFAULT;
		return -1;
	}
	return pid;	
}

uint32_t sys_sbrk(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e, uint32_t f)
{
	uintptr_t old_brk   = (uintptr_t) scheduler_current_task->heap_end;
	uintptr_t heap_max = (uintptr_t) scheduler_current_task->heap_max;
	uintptr_t base = (uintptr_t) scheduler_current_task->heap_start;
	uintptr_t old_size = old_brk - base;
	int incr = a;
	if (incr > 0) {	
		if (incr & PHYSMM_PAGE_ADDRESS_MASK)
			incr = (incr & ~PHYSMM_PAGE_ADDRESS_MASK) + PHYSMM_PAGE_SIZE;
		if ((old_brk + incr) >= heap_max) {
			syscall_errno = ENOMEM;
			return (uint32_t) -1;
		}
		if (old_size != 0) {
			if (procvmm_resize_map((void *)base, old_size + incr)) {
				syscall_errno = ENOMEM;
				return (uint32_t) -1;
			}
		} else {
			if (procvmm_mmap_anon((void *)base, incr, PROCESS_MMAP_FLAG_HEAP | PROCESS_MMAP_FLAG_WRITE, "(heap)")) {
				syscall_errno = ENOMEM;
				return (uint32_t) -1;
			}
	
		}
		scheduler_current_task->heap_end = (void *) (old_brk + incr);
		return (uint32_t) old_brk;
	} else if (incr == 0) {
		return (uint32_t) old_brk;
	} else {
		//TODO: Handle decreasing heap size
		syscall_errno = ENOMEM;
		return (uint32_t) -1;
	}
}

int strlistlen(char **list);

//int execve(char *path, char **argv, char **envp);
uint32_t sys_execve(uint32_t a,uint32_t b,uint32_t c,uint32_t d,uint32_t e, uint32_t f)
{
	int pl,argvc, envpc;
	const char  *path;
	const char **argv;
	const char **envp;
	ssize_t status;
	path = ( const char * ) a;
	argv = ( const char ** ) b;
	envp = ( const char ** ) c;
	
	pl = procvmm_check_string( path, CONFIG_FILE_MAX_NAME_LENGTH );
	if ( pl < 0 ) {
		syscall_errno = EFAULT;
		return (uint32_t) -1;
	}
	
	argvc = procvmm_check_stringlist( argv, CONFIG_MAX_ARG_COUNT,
						CONFIG_MAX_ARG_LENGTH );
	if ( argvc < 0 ) {
		syscall_errno = EFAULT;
		return (uint32_t) -1;
	}
	
	envpc = procvmm_check_stringlist( envp, CONFIG_MAX_ENV_COUNT,
						CONFIG_MAX_ENV_LENGTH );
	if ( envpc < 0 ) {
		syscall_errno = EFAULT;
		return (uint32_t) -1;
	}
	
	status = process_exec( ( char * ) path, ( char ** ) argv, ( char ** ) envp);
	if ( status != 0 ) {
		syscall_errno = status;
		status = -1;
	}
	
	return (uint32_t) status;
}

//void *_sys_mmap(void *addr, size_t len, int prot, int flags, int fd, off_t offset);

uint32_t sys_mmap(	uint32_t a,
			uint32_t b,
			uint32_t c,
			uint32_t d,
			uint32_t e,
			uint32_t f)
{
	return (uint32_t) _sys_mmap(	(void *) a, 
					(size_t) b, 
					(int)    c, 
					(int)    d,
					(int)    e,
					(int)    f);
}


