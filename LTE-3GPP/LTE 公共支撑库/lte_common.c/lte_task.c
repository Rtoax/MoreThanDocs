/*******************************************************************************
 ** 
 ** Copyright (c) 2006-2010 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: mac_task.c
 ** Description: Source file for wrapping message queue functions.
 **
 ** Current Version: 1.0
 ** $Revision: 1.1.1.1 $
 ** Author: Jihua Zhou (jhzhou@ict.ac.cn)
 ** Date: 2007.07.16
 **
 ******************************************************************************/

/* Dependencies ------------------------------------------------------------- */

#include "lte_task.h"
#include <errno.h>

/* Constants ---------------------------------------------------------------- */

/* Types -------------------------------------------------------------------- */

/* Macros ------------------------------------------------------------------- */

/* Globals ------------------------------------------------------------------ */


/* Functions ---------------------------------------------------------------- */
/*******************************************************************************
 * Creates a task.
 *
 * Input: name_p: name of task.
 *        prior: priority of task, no meaning to __KERNEL__.
 *        stack_size: stack size of task, no meaning to LINUX.
 *        state: set the new created task to joinable or detached state,
 *               no meaning to VXWORKS and __KERNEL__.
 *               TASK_JOIN: joinable state,
 *               TASK_DETACH: detached state.
 *        fn: entry point of task.
 *        args_p: arguments of task entry function.
 *
 * #ifdef USE_TASK_WAIT
 * Output: return pointer to task created.
 *                NULL, if create failed.
 * #else
 * Output: return identifier of created task.
 *                0, if create failed.
 * #endif
 ******************************************************************************/
TaskType create_task(char *name_p, INT32 prior, INT32 stack_size, 
								INT32 state, FUNCPTR fn, void *args_p)
{

   	pthread_attr_t attr;
   	struct sched_param param;
    	TaskType taskid;

   	 /* Set attributes for new thread */
 //  	prior /= 3;         /* Prior must be in [1, 99] when RR scheding and FIFO */
    	param.sched_priority = prior;   /* Set task's priority */
    	pthread_attr_init(&attr);
    	pthread_attr_setschedpolicy(&attr, SCHED_RR);
    	pthread_attr_setschedparam(&attr, &param);
    	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
		
   	if (state != TASK_JOIN) {
        		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    	}
	if (pthread_create(&taskid, &attr, (void *)fn, args_p) != 0) {
        		return 0;
    	}
	pthread_attr_destroy(&attr);

	return taskid;
}

/*******************************************************************************
 * Creates a SCHED_FIFO task.
 ******************************************************************************/
TaskType create_fifo_task(char *name_p, INT32 prior, INT32 stack_size, 
								INT32 state, FUNCPTR fn, void *args_p)
{

   	pthread_attr_t attr;
   	struct sched_param param;
    	TaskType taskid;

   	 /* Set attributes for new thread */
	param.sched_priority = prior;   /* Set task's priority */
	pthread_attr_init(&attr);
	pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
//	pthread_attr_setstacksize(&attr, stack_size);
	pthread_attr_setschedparam(&attr, &param);
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
	pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
   	if (state != TASK_JOIN) {
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    }
	if (pthread_create(&taskid, &attr, (void *)fn, args_p) != 0) {
        return 0;
    }
	pthread_attr_destroy(&attr);

	return taskid;
}


/* get thread attr  */
void disp_pthread_attr(pthread_t t_id, pthread_attr_t attr )
{
	INT32 policy;
	INT32 inherit;
	INT32 detach_state;
	UINT32 guard_size;
	INT32 scope;
	struct sched_param param;

	void * stack_addr;
	size_t stack_size;


	if(getuid() == 0){
		printf("The current user is root\n");
	}else {
		printf("The current user is not root\n");
	}

	/* get sched policy and prior */
	pthread_getschedparam(t_id, &policy, &param);
	
	if(policy == SCHED_OTHER) {
		printf("thread sched policy:\t\tSCHED_OTHER\n");
	}else if(policy == SCHED_RR){
		printf("thread sched policy:\t\tSCHED_RR\n");
	}else if(policy == SCHED_FIFO) {
		printf("thread sched policy:\t\tSCHED_FIFO\n");
	}
	printf("thread sched priority:\t\t%d\n", param.sched_priority);

	/* thread inherit*/
	pthread_attr_getinheritsched(&attr, &inherit); 
	printf("thread inherit:\t\t\t%s\n", (inherit == PTHREAD_EXPLICIT_SCHED)
		?"PTHREAD_EXPLICIT_SCHED":"PTHREAD_INHERIT_SCHED");

	/* detachstate*/
	pthread_attr_getdetachstate(&attr,&detach_state);
	printf("thread detachstate:\t\t%s\n", (detach_state == PTHREAD_CREATE_DETACHED)
		?"PTHREAD_CREATE_DETACHED":"PTHREAD_CREATE_JOINABLE");

	/* scope */
	pthread_attr_getscope(&attr,&scope);
	printf("thread scope:\t\t\t%s\n", (scope == PTHREAD_SCOPE_SYSTEM)
		?"PTHREAD_SCOPE_SYSTEM":"PTHREAD_SCOPE_PROCESS");

	/* guard size */
	pthread_attr_getguardsize(&attr,&guard_size);
	printf("thread guard size:\t\t%d\n", guard_size);

	/* pthread_getattr_np() is a non-standard GNU extension that 
		retrieves the attributes of the thread specified in its 
		first argument */
	pthread_getattr_np(t_id, &attr);
    if( 0 == pthread_attr_getstack(&attr,(void*)&stack_addr,&stack_size) )
    {
		printf("thread stackSize:\t\t0x%x\n", stack_size);
		printf("thread stackAddr:\t\t0x%x\n", (UINT32)stack_addr);
    }
}

/*******************************************************************************
 * Creates a  thread .
 * Prior must be in [1, 99] when RR scheding and FIFO 
 * SCHED_RR
 * output >=0: task id
 		  -1: error
 ******************************************************************************/
TaskType create_pthread(INT32 prior, INT32 policy, 
	INT32 state, FUNCPTR fn, void *args_p)
{
   	pthread_attr_t attr;
   	struct sched_param param;
   	TaskType taskid;		/* unsigned long int */
	INT32 ret;

	/* Set attributes for new thread */
	pthread_attr_init(&attr);

	/* set sheduler policy: FIFO/RR */
	pthread_attr_setschedpolicy(&attr, policy);
	
	/* Set task's priority */
	param.sched_priority = prior;   
	pthread_attr_setschedparam(&attr, &param);

	/* PTHREAD_SCOPE_SYSTEM: kernel level thread 
	 * PTHREAD_SCOPE_PROCESS: user level thread */
	pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);

	/*PTHREAD_CREATE_JOINABLE non detachstate thread
	 *PTHREAD_CREATE_DETACHED : detachstate thread */	
   	if (state != TASK_JOIN) {
		pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	}
	
	/* PTHREAD_INHERIT_SCHED: inherit father thread's attr 
	 * PTHREAD_EXPLICIT_SCHED: use pthread_create() attr */
	pthread_attr_setinheritsched(&attr,PTHREAD_EXPLICIT_SCHED);

	if (ret = pthread_create(&taskid, &attr, (void *)fn, args_p) != 0) {
		printf("fail to create pthread:ret:%d, error:%s \n", ret, strerror(errno));
		if(getuid() != 0){
			printf("Please run app with root user\n");
		}
		return -1;
	}else{
//		printf("=====Create thread :%s ok, taskid:%lu=====\n", (char *)args_p,taskid );
		/* show thread attr */
//		disp_pthread_attr(taskid, attr);
		
		/* release pthread_attr_t */
		pthread_attr_destroy(&attr);
		
		return taskid;
	}
}



/*******************************************************************************
 * Whether the task should be stopped or not.
 *
 * Input: None.
 *
 * Output: return 1, if the task should be stopped;
 *                0, if the task should continue running.
 ******************************************************************************/
INT32 task_should_stop(void)
{
    INT32 ret = 0;

	/* To be defined */
    return ret;
}
/*******************************************************************************
 * Stop a task.
 *
 * Input: task_p: pointer to task to be stopped.
 *
 * Output: return the result the task's fn.
 ******************************************************************************/
INT32 stop_task(TaskType task_p)
{
    INT32 ret = 0;

	/* To be defined */

    return ret;
}
/*******************************************************************************
 * Delete a task.
 *
 * #ifdef USE_TASK_WAIT
 * Input: task_p: pointer to task to be deleted.
 * #else
 * Input: taskid: identifier of task to be deleted.
 * #endif
 *
 * Output: return 0, or -1 if error occurs.
 ******************************************************************************/
INT32 delete_task(TaskType task_p)
{
	INT32 ret = 0;
	if (pthread_cancel(task_p)) {
    		ret =  -1;
    	}
	return ret;
}
/*******************************************************************************
 * Exit a task.
 *
 * Input: None.
 * 
 * Output: None.
 ******************************************************************************/
void exit_task(void)
{
    	pthread_exit(0);
}
/*******************************************************************************
 * Wait on task completion.
 *
 * Input: taskid: identifier of task.
 *
 * Output: return 0, or -1 if error occurs.
 ******************************************************************************/
INT32 wait_task(TaskType taskid)
{
   	 if (pthread_join(taskid, NULL)) {
        		return -1;
   	 }
   	 return 0;
}
/*******************************************************************************
 * Delay task for a while.
 *
 * Input: ticks: number of OS ticks to delay task for.
 * 
 * Output: None.
 ******************************************************************************/
void delay_task(UINT32 ticks)
{
    UINT64 tick_ms;

    tick_ms = ticks;
    tick_ms = (tick_ms * 1000000) / CLOCKS_PER_SEC;
    usleep(tick_ms);
}
/*******************************************************************************
 * Get task id.
 *
 * Input: None.
 * 
 * Output: task id.
 ******************************************************************************/
TaskType get_taskid(void)
{
    	return pthread_self();
}


