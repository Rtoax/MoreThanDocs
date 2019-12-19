/*******************************************************************************
 **
 **
 ** Copyright (c) 2011-2015 ICT/CAS.
 ** All rights reserved.
 **
 ** File name:lte_muti_core.c
 ** Description: Source file for muti core thread.
 **
 ** Current Version: 1.2
 ** $Revision: 4.1 $
 ** Author: Wang Guoqiang (wangguoqiang@ict.ac.cn)
 ** Date: 2014.01.06
 ******************************************************************************/

/* Dependencies ------------------------------------------------------------- */
#include <unistd.h>
#include <sched.h>
#include <pthread.h>
#include "lte_muti_core.h"
#include "lte_log.h"

/* Constants ---------------------------------------------------------------- */

/* Types -------------------------------------------------------------------- */

/* Macros ------------------------------------------------------------------- */

/* Globals ------------------------------------------------------------------ */

static INT32 g_core_num = 0;

/*The thread node queue of every core. */
static MutiCoreMsgList *g_msg_list_p;

/*count the time of the running node.*/
static INT32 *extr_time;

/* the time that every thread node may be cost.*/
static INT32 g_init_time_cost[MAX_MSG_NUM] =
{
		500/*tti*/, 100/*mac rx*/, 50/*no ue*/,100/*ul sched*/, 300/*dl sched*/,300/*rx s1*/, 300/*rx x2*/,
		80/*rlc rx*/, 200/*rx rrc*/, 10/*rx gtp*/, 5/*exprire*/
};

NodeTime g_node_time[MAX_MSG_NUM];

/*if the same node is conflicted, 0 is no , 1 is rnti , 2 is rnti and lcid.*/
static INT32 g_msg_parallel[MAX_MSG_NUM] =
{
RNTI_CONFLICT, NO_CONFLICT, RNTI_CONFLICT, NO_CONFLICT, RNTI_CONFLICT,
NO_CONFLICT,NO_CONFLICT,LCID_CONFLICT, LCID_CONFLICT,LCID_CONFLICT,
NO_CONFLICT
};

LteLock	sched_lock_t;
UINT8 sched_num = 0;
UINT8	ue_sched_flag = 0;
#ifdef LTE_MUTI_CORE
/* Functions ---------------------------------------------------------------- */
/**********************************************************
 * init the muti core thread.
 *
 * Input:
 * 	core_num : the number of the core.
 *
 * Output:
 * 	none
 *
 * Return:
 * 	0 : 	success
 * 	other: fail
 ************************************************************/
INT32 init_muti_core(INT32 core_num)
{
	INT32 i = 0;

	if (core_num < 1 || core_num >  MAX_MSG_NUM) {
		log_msg(LOG_ERR, COMMON, "core number error, number=%d\n", core_num);
		return -1;
	}

	g_core_num = core_num;

	g_msg_list_p = (MutiCoreMsgList *)malloc(core_num * sizeof(MutiCoreMsgList));
	if (NULL == g_msg_list_p) return -1;
	memset(g_msg_list_p, 0, core_num * sizeof(MutiCoreMsgList));

	for (i = 0; i < core_num; i++) {
		g_msg_list_p[i].lock_t = create_lock();
		g_msg_list_p[i].semb = create_semb(SEM_EMPTY);
		memset(g_msg_list_p[i].need_time, 0, (MAX_MSG_NUM + 1) * sizeof(INT32));
	}

	for (i = 0; i < MAX_MSG_NUM; i++){
		g_node_time[i].time_cost = g_init_time_cost[i];
		g_node_time[i].flag = 0;
		g_node_time[i].lock_t = create_lock();
	}

	extr_time = (INT32 *)lte_malloc(core_num);

	sched_lock_t = create_lock();
	return 0;
}

/**********************************************************
 * cleanup the resource of muti core.
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
INT32 clean_muti_core()
{
	INT32 i = 0;

	for (i = 0; i < g_core_num; i++) {
		free_list(&g_msg_list_p[i].lh);
		delete_lock(g_msg_list_p[i].lock_t);
		delete_sem(g_msg_list_p[i].semb);
	}

	free(g_msg_list_p);

	for (i = 0; i < MAX_MSG_NUM; i++){
		delete_lock(g_node_time[i].lock_t);
	}

	lte_free(extr_time);

	delete_lock(sched_lock_t);
	return 0;
}
/**********************************************************
 * set the core init time.
 *
 * Input:
 * 	core_num : the number of the core.
 * 	time: the init time before the core runs
 *
 * Output:
 * 	none
 *
 * Return:
 * 	0 : 	success
 * 	other: fail
 ************************************************************/
INT32 set_core_init_time(INT32 core_index, INT32 time)
{
	INT32 i = 0;

	if (core_index < 0 || core_index >  MAX_MSG_NUM) {
		log_msg(LOG_ERR, COMMON, "core number error, number=%d\n", core_index);
		return -1;
	}

	for (i = 0; i <= MAX_MSG_NUM; i++) {
		g_msg_list_p[core_index].need_time[i] += time;
	}


	return 0;
}
/**********************************************************
 * find if the thread node is conflict with the thread nodes in the thread list.
 *
 * Input:
 * 	node_p : the thread node.
 * 	core_index : the index of the core
 *
 * Output:
 * 	none
 *
 * Return:
 * 	0 : no conflict
 * 	1 : conflict
 ************************************************************/
static inline INT32 is_conflict(MutiCoreMsgNode *node_p, INT32 core_index)
{
	MutiCoreMsgNode *p = NULL;

	if (NO_CONFLICT == g_msg_parallel[node_p->msg_type]) {
		return 0;
	} else if (RNTI_CONFLICT == g_msg_parallel[node_p->msg_type]){
		lte_lock(g_msg_list_p[core_index].lock_t);
		p = (MutiCoreMsgNode *)first_list((ListType *)&g_msg_list_p[core_index]);

		while (NULL != p) {
			/*the reason of conflict*/
			if (p->msg_type == node_p->msg_type && p->rnti == node_p->rnti)	{
				lte_unlock(g_msg_list_p[core_index].lock_t);
				return 1;
			}
			p = (MutiCoreMsgNode *)next_list((NodeType *)p);
		}
		lte_unlock(g_msg_list_p[core_index].lock_t);
	} else if (LCID_CONFLICT == g_msg_parallel[node_p->msg_type]) {
		lte_lock(g_msg_list_p[core_index].lock_t);
		p = (MutiCoreMsgNode *)first_list((ListType *)&g_msg_list_p[core_index]);

		while (NULL != p) {
			/*the reason of conflict*/
			if (p->msg_type == node_p->msg_type && p->rnti == node_p->rnti && p->lc_id == node_p->lc_id){
				lte_unlock(g_msg_list_p[core_index].lock_t);
				return 1;
			}
			p = (MutiCoreMsgNode *)next_list((NodeType *)p);
		}
		lte_unlock(g_msg_list_p[core_index].lock_t);
	} else {
		log_msg(LOG_ERR, COMMON, "node_p->msg_type error, value is %d\n", node_p->msg_type);
		return 1;
	}

	return 0;
}
/**********************************************************
 * find the thread list according to the thread node..
 *
 * Input:
 * 	node_p : the thread node.
 * 	no_core_index : the index of the core which should not execute the node(-1 is not exits)
 *
 * Output:
 * 	core_index : the index of the core, the value is 0 ~ core_num - 1
 *
 * Return:
 * 	core_index : the index of the core, the value is 0 ~ core_num - 1
 ************************************************************/
static INT32 find_thread_list(MutiCoreMsgNode *node_p, INT32 no_core_index)
{
	INT32 i = 0, core_index = -1;
	INT32 min_type_value = 0, min_all_value = 0;
	struct timeval	now_time;

	if (g_core_num < 1 || g_core_num >  MAX_MSG_NUM) {
		log_msg(LOG_ERR, COMMON, "core number error, number=%d\n", g_core_num);
		return -1;
	}

	/*find the core that the cost time before execute this node is less than others.*/
	gettimeofday(&now_time, 0);
	for (i = 0; i < g_core_num; i++){
		if (0 != g_msg_list_p[i].curr_run_node_time) {
			extr_time[i] = (now_time.tv_sec - g_msg_list_p[i].run_time.tv_sec) * 1000000 +
					now_time.tv_usec - g_msg_list_p[i].run_time.tv_usec;
		} else {
			extr_time[i]  = 0;
		}
	}
	core_index = -1;
	for (i = 0; i < g_core_num; i++){
		if (i == no_core_index) continue;

		/*find if exits the conflict thread node, if exits, put the node to the thread list.*/
		if (1 == is_conflict(node_p, i))	return i;

		if (-1 == core_index) {
			core_index = i;
			min_type_value = g_msg_list_p[i].need_time[node_p->msg_type] + extr_time[i];
		}else if (min_type_value > (g_msg_list_p[i].need_time[node_p->msg_type] + extr_time[i])) {
			min_type_value = g_msg_list_p[i].need_time[node_p->msg_type] + extr_time[i];
			core_index = i;
		}
	}

	/*find the all cost time is least in the core that the cost time before execute this node is less than others.*/
	min_all_value = g_msg_list_p[core_index].need_time[MAX_MSG_NUM];
	for (i = 0; i < g_core_num; i++){
		if (i == no_core_index || min_type_value != (g_msg_list_p[i].need_time[node_p->msg_type] + extr_time[i]))
			continue;

		if (min_all_value > (g_msg_list_p[i].need_time[MAX_MSG_NUM] + extr_time[i])) {
			min_all_value = g_msg_list_p[i].need_time[MAX_MSG_NUM] + extr_time[i];
			core_index = i;
		}
	}

	return core_index;
}
/**********************************************************
 * add the thread node to the thread list.
 *
 * Input:
 * 	node_p : the thread node.
 * 	no_core_index : the index of the core which should not execute the node(-1 is not exits)
 *
 * Output:
 * 	none
 *
 * Return:
 * 	0 : 	success
 * 	other: fail
 ************************************************************/
#include "fs_lowmac_ctrl.h"
extern LowmacInfoDebug_t g_mac_info_statistic;
INT32 add_thread_node(MutiCoreMsgNode *node_p, INT32 no_core_index, INT32 set_core_index)
{

	INT32 i = 0;
	INT32 core_index = -1;
	MutiCoreMsgNode *msg_node_p = NULL, *pre_msg_node_p = NULL;
	UINT32 cost_time = CALC_ELAPSED_TIME(
	if (NULL == node_p){
		log_msg(LOG_ERR, COMMON, "node_p is null \n");
		return -1;
	}

	if (node_p->msg_type < 0 || node_p->msg_type >= MAX_MSG_NUM){
		log_msg(LOG_ERR, COMMON, "node type is error, type=%d\n", node_p->msg_type);
		return -1;
	}

	node_p->cost_time = g_node_time[node_p->msg_type].time_cost;
	if (set_core_index > g_core_num - 1 || set_core_index < 0)
		core_index = find_thread_list(node_p, no_core_index);
	else
		core_index  = set_core_index;
//	if (UE_DL_SCHED == node_p->msg_type){
//		printf("core_index=%d\n", core_index);
//	}
//	if (UE_UL_SCHED == node_p->msg_type)
//		printf("UL sched core idnex=%d\n", core_index);
	if (core_index < 0 || core_index >=  MAX_MSG_NUM) {
		log_msg(LOG_ERR, COMMON, "Can not find a thread list%d\n");
		return -1;
	}

	lte_lock(g_msg_list_p[core_index].lock_t);
	msg_node_p = (MutiCoreMsgNode *)first_list((ListType *)&g_msg_list_p[core_index].lh);

	/*find the last node which type is not more than the new node.*/
	while(NULL != msg_node_p && msg_node_p->msg_type <= node_p->msg_type) {
		pre_msg_node_p = msg_node_p;
		msg_node_p = (MutiCoreMsgNode *)next_list((NodeType *)msg_node_p);
	}
	insert_list((ListType *)&g_msg_list_p[core_index].lh, pre_msg_node_p, node_p);

	/*add the time cost*/
	for (i = node_p->msg_type; i <= MAX_MSG_NUM; i++) {
		g_msg_list_p[core_index].need_time[i] += node_p->cost_time;
	}
	lte_unlock(g_msg_list_p[core_index].lock_t);

	give_sem(g_msg_list_p[core_index].semb);
	);
	update_minmax_avg_timing(&g_mac_info_statistic.sctp_rx_time,cost_time);

	return 0;
}

/**********************************************************
 * get the thread node from the thread list.
 *
 * Input:
 * 	core_index : the thread list of the core
 *
 * Output:
 * 	none
 *
 * Return:
 * 	node_p : the thread node.
 ************************************************************/
MutiCoreMsgNode* get_thread_node(MutiCoreMsgList *msg_list_p)
{
	INT32 i = 0;
	MutiCoreMsgNode * msg_node_p = NULL;

	if (NULL == msg_list_p) {
		log_msg(LOG_ERR, COMMON, "Msg list is NULL\n");
		return NULL;
	}

	lte_lock(msg_list_p->lock_t);
	msg_node_p = (MutiCoreMsgNode *)get_list((ListType *)msg_list_p);

	/*clean the time cost*/
	for(i = msg_node_p->msg_type; i <= MAX_MSG_NUM; i++) {
		msg_list_p->need_time[i] -= msg_node_p->cost_time;
	}
	lte_unlock(msg_list_p->lock_t);
	gettimeofday(&msg_list_p->run_time, 0);
	msg_list_p->curr_run_node_time = msg_node_p->cost_time;

	return msg_node_p;
}

/**********************************************************
 * Check which core is running is thread,.
 *
 * Input:
 * 	none.
 *
 * Output:
 * 	none
 *
 * Return:
 * 	0 : 	success
 * 	other: fail
 ************************************************************/
INT32 check_process()
{
	INT32 i = 0;
	cpu_set_t get;

	CPU_ZERO(&get);

	if (pthread_getaffinity_np(pthread_self(), sizeof(get), &get) < 0)
		perror("can not get thread affinity!\n");

	for (i = 0; i < g_core_num; i++){
		if(CPU_ISSET(i, &get)) {
			log_msg(LOG_WARNING, COMMON, "The thread is running in the process %d, list0 node count=%d,list1 node count=%d,  tti time=%d, dl_sched_time=%d, ul_sched=%d, "
					"list 0need_time=%d, list1 need_time=%d\n",
					i, count_list((ListType *)&g_msg_list_p[0]), count_list((ListType *)&g_msg_list_p[1]),g_node_time[TTI_SCHED].time_cost, g_node_time[UE_DL_SCHED].time_cost, g_node_time[UE_UL_SCHED].time_cost
					, g_msg_list_p[0].need_time[MAX_MSG_NUM], g_msg_list_p[1].need_time[MAX_MSG_NUM]);
			return i;
		}
	}
	log_msg(LOG_SUMMARY, COMMON, "Can not find which core is running in the thread\n");
	return -1;
}

/**********************************************************
 * get the core index which core is running is thread,.
 *
 * Input:
 * 	none.
 *
 * Output:
 * 	none
 *
 * Return:
 * 	core_index
 ************************************************************/
INT32 get_process_id()
{
	INT32 i = 0;
	cpu_set_t get;

	CPU_ZERO(&get);

	if (pthread_getaffinity_np(pthread_self(), sizeof(get), &get) < 0)
		perror("can not get thread affinity!\n");

	for (i = 0; i < g_core_num; i++){
		if(CPU_ISSET(i, &get)) {
			return i;
		}
	}
	log_msg(LOG_SUMMARY, COMMON, "Can not find which core is running in the thread\n");
	return -1;
}
INT32 proc_count_num(){
	INT32 i = 0;
	for (i = 0; i < g_core_num; i++){
		printf("core :%d, node number:%d\n",i, count_list((ListType *)&g_msg_list_p[i]));
	}
	check_process();
	return 0;
}
/**********************************************************
 * The thread that every core will execute.
 *
 * Input:
 * 	arg_p : the index of the core, the value is 0 ~ core_num - 1
 *
 * Output:
 * 	none
 *
 * Return:
 * 	0 : 	success
 * 	other: fail
 ************************************************************/
INT32 execute_thread(void *arg_p)
{
	MutiCoreMsgList	*msg_list_p = NULL;
	MutiCoreMsgNode	* msg_node_p = NULL;
	struct timeval begin, end;
	INT32 time_cost = 0;
	INT32 count = 0;
	INT32 core_index = 0;
	cpu_set_t mask;

	core_index = (INT32)arg_p;
	if (core_index < 0 || core_index >= g_core_num) {
		log_msg(LOG_ERR, COMMON, "core index error, value is %d\n", core_index);
		return -1;
	}

	CPU_ZERO(&mask);

	CPU_SET(core_index,&mask);


	if(pthread_setaffinity_np(pthread_self(), sizeof(mask), &mask) < 0)
		perror("can not set thread affinity!\n");

	msg_list_p = &g_msg_list_p[core_index];

	log_msg(LOG_SUMMARY, COMMON, "Success create thread core_index=%d\n", core_index);
	check_process();
	while(1) {
		take_sem(msg_list_p->semb, WAIT_FOREVER);
		msg_node_p = get_thread_node(msg_list_p);
//		if (msg_node_p->msg_type == RX_GTP_PDU)
//			printf("count list=%d\n", count_list((ListType *)msg_list_p));
//		printf("########################begin begin#############################\n");
		if (NULL == msg_node_p) {
			log_msg(LOG_ERR, COMMON, "Thread execute error. core_index=%d\n", core_index);
		}


		gettimeofday(&begin, 0);
		msg_node_p->fun(msg_node_p->arg);
		gettimeofday(&end, 0);

		/*clean the run node info*/
		msg_list_p->curr_run_node_time = 0;
		/*update the cost time of this node.*/
		time_cost = (end.tv_sec - begin.tv_sec) * 1000000 + end.tv_usec - begin.tv_usec;
		/*update time*/
		//lte_lock(g_node_time[msg_node_p->msg_type].lock_t);
		g_node_time[msg_node_p->msg_type].time_value_count += time_cost;
		g_node_time[msg_node_p->msg_type].time_num_count++;
		if (0 == g_node_time[msg_node_p->msg_type].flag) {
			g_node_time[msg_node_p->msg_type].time_cost = g_node_time[msg_node_p->msg_type].time_value_count;
			g_node_time[msg_node_p->msg_type].flag = 1;
			g_node_time[msg_node_p->msg_type].time_value_count = 0;
			g_node_time[msg_node_p->msg_type].time_num_count = 0;
		} else if (TIME_PERIOD_COUNT == g_node_time[msg_node_p->msg_type].time_num_count) {
			g_node_time[msg_node_p->msg_type].time_cost = g_node_time[msg_node_p->msg_type].time_value_count / TIME_PERIOD_COUNT;
			if (g_node_time[msg_node_p->msg_type].time_cost  > g_init_time_cost[msg_node_p->msg_type])
				g_node_time[msg_node_p->msg_type].time_cost = g_init_time_cost[msg_node_p->msg_type];
			g_node_time[msg_node_p->msg_type].time_value_count = 0;
			g_node_time[msg_node_p->msg_type].time_num_count = 0;
		}
		//lte_unlock(g_node_time[msg_node_p->msg_type].lock_t);
//		log_msg(LOG_SUMMARY - core_index, COMMON, "arg1=%d, arg2=%d.\n", msg_node_p->arg[0].value, msg_node_p->arg[1].value);
//		check_process();
		lte_free(msg_node_p);
//		printf("############################end end#############################\n");
	}

	return 0;
}
#endif
