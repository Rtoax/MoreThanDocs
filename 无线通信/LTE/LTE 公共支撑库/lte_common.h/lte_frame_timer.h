/*******************************************************************************
 **
 ** Copyright (c) 2011-2015 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: lte_frame_timer.h
 ** Description: Header file for Timer.
 **
 ** Current Version: 1.2
 ** $Revision: 4.1 $
 ** Author: Wang Guoqiang (wangguoqiang@ict.ac.cn)
 ** Date: 2014.01.06
 ******************************************************************************/

#ifndef LTE_FRAME_TIMER_H_
#define LTE_FRAME_TIMER_H_

/*******************************************************************************
 **
 Declaration for all
 **
 ******************************************************************************/

/* Dependencies ------------------------------------------------------------- */
#include <errno.h>
#include <stdio.h>
#include "lte_list.h"
#include "lte_type.h"
#include "lte_lock.h"
/* Constants ---------------------------------------------------------------- */

#define EXPIRE_TIMER_NUM 1024

/* Types -------------------------------------------------------------------- */

typedef union{
		void			*msg_p;
		INT32		value;
}ArgValue;

typedef INT32 (*EXPIRE_FUN)(ArgValue arg);

typedef struct{
		ListType		lh;
		LteLock 		lock_t;
}TimerList;

typedef struct{
		NodeType		ln;
		EXPIRE_FUN		expire_fun;
		ArgValue		arg;
		INT32		count;
		INT32		wait_time;
		INT32		location;
}TimerNode;

/* Macros ------------------------------------------------------------------- */

/* Globals ------------------------------------------------------------------ */

/* Functions ---------------------------------------------------------------- */
/**********************************************************
 * init the frame timer.
 *
 * Input:
 * 	none
 *
 * Output:
 * 	none
 *
 * Return:
 * 	0 : 	success
 * 	other: fail
 ************************************************************/
INT32 init_frame_timer();

/**********************************************************
 * clean the frame timer.
 *
 * Input:
 * 	none
 *
 * Output:
 * 	none
 *
 * Return:
 * 	0 : 	success
 * 	other: fail
 ************************************************************/
INT32 clean_frame_timer();

/**********************************************************
 * create the timer node, but not start the node
 *
 * Input:
 * 	expire_fun : the function will be executed when the timer node expires
 * 	arg : the argument of the expire function
 * 	wait_time : the waiting time that the timer node need to be wait
 *
 * Output:
 * 	timer_node: the detail of the timer node.
 *
 * Return:
 * 	timer_node: the detail of the timer node.
 ************************************************************/
TimerNode* create_frame_timer(EXPIRE_FUN expire_fun, ArgValue arg, INT32 wait_time);

/**********************************************************
 * get the timer node that has been created.
 *
 * Input:
 * 	timer_p : the detail of the timer node.
 * 	expire_p : the pointer of the expire time
 *
 * Output:
 *
 *
 * Return:
 * 	0 : 	success
 * 	other: fail
 ************************************************************/
int get_frame_timer(TimerNode *timer_p, INT32 *expire_p);

/**********************************************************
 * start the timer node that has been created.
 *
 * Input:
 * 	timer_p : the detail of the timer node.
 * 	wait_time : the waiting time that the timer node need to be wait
 *
 * Output:
 * 	the timer node will be added in the according timer list.
 *
 * Return:
 * 	0 : 	success
 * 	other: fail
 ************************************************************/
int start_frame_timer(TimerNode *timer_p, INT32 wait_time);

/**********************************************************
 * stop the timer node that has being starting
 *
 * Input:
 * 	timer_p : the detail of the timer node.
 *
 * Output:
 * 	the timer node will be deleted in the according timer list.
 *
 * Return:
 * 	0 : 	success
 * 	other: fail
 ************************************************************/
int stop_frame_timer(TimerNode *timer_p);

/**********************************************************
 * delete the timer node that has being created.
 *
 * Input:
 * 	timer_p : the detail of the timer node.
 *
 * Output:
 * 	the timer node will be free.
 *
 * Return:
 * 	0 : 	success
 * 	other: fail
 ************************************************************/
int delete_frame_timer(TimerNode *timer_p);

/**********************************************************
 * update the time.
 *
 * Input:
 * 	time : the current time
 *
 * Output:
 * 	none
 *
 * Return:
 * 	0 : 	success
 * 	other: fail
 ************************************************************/
INT32 update_frame_time(INT32 time);

/**********************************************************
 * timer is expire.
 *
 * Input:
 * 	timer_list_p : the expired timer list.
 *
 * Output:
 * 	none
 *
 * Return:
 * 	0 : 	success
 * 	other: fail
 ************************************************************/
INT32 expire_frame_timer(TimerList *timer_list_p);

#endif /* LTE_FRAME_TIMER_H_ */
