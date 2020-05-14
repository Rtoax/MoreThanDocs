/******************************************************************************
 ** 
 ** Copyright (c) 2006-2010 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: lte_task.h
 ** Description: Header file for wrapping task functions.
 **
 ** Current Version: 1.3
 ** $Revision: 1.1.1.1 $
 ** Author: Jihua Zhou (jhzhou@ict.ac.cn)
 ** Date: 2007.07.16
 ** Revision: Support creating task in non-block context, such as timer.
 **
 ** Old Version: 1.2
 ** Author: Jihua Zhou (jhzhou@ict.ac.cn)
 ** Date: 2006.08.20
 **
 ** Old Version: 1.1
 ** Author: Jihua Zhou (jhzhou@ict.ac.cn)
 ** Date: 2006.04.09
 **
 ** Old Version: 1.0
 ** Author: Jihua Zhou (jhzhou@ict.ac.cn)
 ** Date: 2006.01.05
 **
 ******************************************************************************/
#ifndef TASK_H
#define TASK_H
/* Dependencies ------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include "lte_type.h"

/* Constants ---------------------------------------------------------------- */
#define TASK_JOIN 1		/* Set the new created task to jionable state */
#define TASK_DETACH 0	/* Set the new created task to detached state */


/* Types -------------------------------------------------------------------- */
typedef pthread_t TaskType;

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
									INT32 state, FUNCPTR fn, void *args_p);

/*******************************************************************************
 * Whether the task should be stopped or not.
 *
 * Input: None.
 *
 * Output: return 1, if the task should be stopped;
 *                0, if the task should continue running.
 ******************************************************************************/
INT32 task_should_stop(void);
/*******************************************************************************
 * Stop a task.
 *
 * Input: task_p: pointer to task to be stopped.
 *
 * Output: return the result the task's fn.
 ******************************************************************************/
INT32 stop_task(TaskType task_p);

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
INT32 delete_task(TaskType task_p);
/*******************************************************************************
 * Exit a task.
 *
 * Input: None.
 * 
 * Output: None.
 ******************************************************************************/
void exit_task(void);
/*******************************************************************************
 * Wait on task completion.
 *
 * Input: taskid: identifier of task.
 *
 * Output: return 0, or -1 if error occurs.
 ******************************************************************************/
INT32 wait_task(TaskType taskid);
/*******************************************************************************
 * Delay task for a while.
 *
 * Input: ticks: number of OS ticks to delay task for.
 * 
 * Output: None.
 ******************************************************************************/
void delay_task(UINT32 ticks);
/*******************************************************************************
 * Get task id.
 *
 * Input: None.
 * 
 * Output: task id.
 ******************************************************************************/
TaskType get_taskid(void);
TaskType create_pthread(INT32 prior, INT32 policy,INT32 state, FUNCPTR fn, void *args_p);
void disp_pthread_attr(pthread_t t_id, pthread_attr_t attr );
#endif	/* TASK_H */

