/*******************************************************************************
 **
 **
 ** Copyright (c) 2011-2015 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: lte_frame_timer.c
 ** Description: Source file for Timer.
 **
 ** Current Version: 1.2
 ** $Revision: 4.1 $
 ** Author: Wang Guoqiang (wangguoqiang@ict.ac.cn)
 ** Date: 2014.01.06
 ******************************************************************************/

/* Dependencies ------------------------------------------------------------- */
#include "lte_frame_timer.h"
#include "lte_log.h"
/* Constants ---------------------------------------------------------------- */

/* Types -------------------------------------------------------------------- */

/* Macros ------------------------------------------------------------------- */

/* Globals ------------------------------------------------------------------ */

/*store the timer node.*/
static TimerList *expire_list[EXPIRE_TIMER_NUM];

static INT32 current_time = -1;
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
INT32 init_frame_timer()
{
	INT32 i = 0;

	for (i = 0; i < EXPIRE_TIMER_NUM; i++){
		expire_list[i] = (TimerList *)malloc(sizeof(TimerList));

		if (NULL == expire_list[i]) return -1;

		memset(expire_list[i], 0, sizeof(TimerList));
		expire_list[i]->lock_t = create_lock();
	}

	return 0;
}

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
INT32 clean_frame_timer()
{
	INT32 i = 0;

	for (i = 0; i < EXPIRE_TIMER_NUM; i++){
		if (NULL == expire_list[i]) return -1;

		delete_lock(expire_list[i]->lock_t);
		free(expire_list[i]);
	}

	return 0;
}
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
TimerNode* create_frame_timer(EXPIRE_FUN expire_fun, ArgValue arg, INT32 wait_time)
{
	TimerNode *node_p = NULL;

	if (NULL == expire_fun || wait_time < 0 || wait_time > EXPIRE_TIMER_NUM){
		log_msg(LOG_ERR, COMMON, "The argument of create timer is wrong, wait_time=%d\n", wait_time);
		return NULL;
	}

	node_p = (TimerNode *)malloc(sizeof(TimerNode));
	if (NULL == node_p) {
		perror("malloc error in create timer:");
		return NULL;
	}

	node_p->arg = arg;
	node_p->expire_fun = expire_fun;
	node_p->wait_time = wait_time;
	node_p->location = 0;

	return node_p;
}

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
int get_frame_timer(TimerNode *timer_p, INT32 *expire_p)
{
	TimerList *list_p = NULL;
	TimerNode *node_p = NULL;

	if (NULL == timer_p || NULL == expire_p) {
		log_msg(LOG_ERR, COMMON, "The argument of start timer is wrong\n");
		return -1;
	}
	list_p = expire_list[timer_p->location % EXPIRE_TIMER_NUM];

	lte_lock(list_p->lock_t);
	node_p = (TimerNode *) first_list((ListType *) list_p);
	while (NULL != node_p) {
		if (timer_p == node_p){
			*expire_p = (node_p->location + EXPIRE_TIMER_NUM - current_time) % EXPIRE_TIMER_NUM;
			delete_list((ListType *)list_p, (NodeType *)node_p);
			lte_unlock(list_p->lock_t);
			return 0;
		}
		node_p = (TimerNode *)next_list((NodeType *)node_p);
	}
	lte_unlock(list_p->lock_t);
	return -1;
}

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
int start_frame_timer(TimerNode *timer_p, INT32 wait_time)
{
	TimerList *list_p = NULL;
	INT32 expire_time = 0;

	if (NULL == timer_p || wait_time <= 0) {
		log_msg(LOG_ERR, COMMON, "The argument of start timer is wrong, wait_time=%d\n", wait_time);
		return -1;
	}

	timer_p->wait_time = wait_time;
	expire_time = (current_time + wait_time) % EXPIRE_TIMER_NUM;
	timer_p->location = expire_time;
	timer_p->count = wait_time / EXPIRE_TIMER_NUM;

	/*add the node into the expire list.*/
	list_p = expire_list[expire_time];
	lte_lock(list_p->lock_t);
	add_list((ListType *)list_p, (NodeType *)timer_p);
	lte_unlock(list_p->lock_t);

	return 0;
}

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
int stop_frame_timer(TimerNode *timer_p)
{
	TimerList *list_p = NULL;
	TimerNode *node_p;

	if (NULL == timer_p) {
		log_msg(LOG_ERR, COMMON, "The argument of stop timer is wrong\n");
		return -1;
	}

	list_p = expire_list[timer_p->location % EXPIRE_TIMER_NUM];

	lte_lock(list_p->lock_t);
	node_p = (TimerNode *) first_list((ListType *) list_p);
	while (NULL != node_p) {
		if (timer_p == node_p){
			delete_list((ListType *)list_p, (NodeType *)node_p);
			lte_unlock(list_p->lock_t);
			return 0;
		}
		node_p = (TimerNode *)next_list((NodeType *)node_p);
	}
	lte_unlock(list_p->lock_t);
	return -1;
}

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
int delete_frame_timer(TimerNode *timer_p)
{
	if (NULL == timer_p) {
		log_msg(LOG_ERR, COMMON, "The argument of stop timer is wrong\n");
		return -1;
	}

	free(timer_p);

	return 0;
}

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
INT32 update_frame_time(INT32 time)
{
	INT32 ret = 0;

	current_time = (current_time + 1) % EXPIRE_TIMER_NUM;

	if (current_time != time % EXPIRE_TIMER_NUM){
		/*the lost timer can be expired*/
		log_msg(LOG_ERR, COMMON, "timer node is lost some time, current-%d, time=%d\n", current_time, time% EXPIRE_TIMER_NUM);

		while(current_time != time % EXPIRE_TIMER_NUM) {
			ret = expire_frame_timer(expire_list[current_time]);

			if (0 != ret) 	break;
			current_time = (current_time + 1) % EXPIRE_TIMER_NUM;
		}
	}

	if (0 == ret)
		ret = expire_frame_timer(expire_list[current_time]);

	return ret;
}

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
INT32 expire_frame_timer(TimerList *timer_list_p)
{
	TimerNode *node_p = NULL, *temp_p = NULL;

	lte_lock(timer_list_p->lock_t);

	node_p = (TimerNode *)first_list((ListType *)timer_list_p);
	lte_unlock(timer_list_p->lock_t);
	while (NULL != node_p ){
		/*timer node expire*/

		if(node_p->count == 0) {
			node_p->expire_fun(node_p->arg);
			lte_lock(timer_list_p->lock_t);
			temp_p = node_p;
			node_p = (NodeType *)next_list((NodeType *)node_p);
			delete_list((ListType *)timer_list_p, (NodeType *)temp_p);
			lte_unlock(timer_list_p->lock_t);
		} else {
			node_p->count--;
			lte_lock(timer_list_p->lock_t);
			node_p = (NodeType *)next_list((NodeType *)node_p);
			lte_unlock(timer_list_p->lock_t);
		}
	}


	return 0;
}
