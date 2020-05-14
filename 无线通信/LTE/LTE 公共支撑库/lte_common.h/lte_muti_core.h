/*******************************************************************************
 **
 ** Copyright (c) 2011-2015 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: lte_muti_core.h
 ** Description: Header file for muti core thread.
 **
 ** Current Version: 1.2
 ** $Revision: 4.1 $
 ** Author: Wang Guoqiang (wangguoqiang@ict.ac.cn)
 ** Date: 2014.01.06
 ******************************************************************************/
#ifndef LTE_MUTI_CORE_H_
#define LTE_MUTI_CORE_H_
/*******************************************************************************
 **
 Declaration for all
 **
 ******************************************************************************/

/* Dependencies ------------------------------------------------------------- */
#include <sys/time.h>
#include "lte_type.h"
#include "lte_lock.h"
#include "lte_list.h"
#include "lte_sema.h"
/* Constants ---------------------------------------------------------------- */

/*The period of the cost time of thread node will be modified*/
#define TIME_PERIOD_COUNT 100

/*The number of the argument of the thread node function.*/
#define MAX_THREAD_ARG 6

/* Types -------------------------------------------------------------------- */
typedef enum{
	NO_CONFLICT = 0,
	RNTI_CONFLICT = 1,
	LCID_CONFLICT = 2,
}MutiCoreMsgConflict;

typedef union{
		void			*msg_p;
		INT32		value;
}ThreadArgValue;

typedef INT32 (*THREAD_FUN)(ThreadArgValue *arg);

/* The priority of the thread node.*/
typedef enum{
		TTI_SCHED = 0,
		TEST_RX_MAC_PDU,
		NO_UE_SCHED,
		UE_UL_SCHED,
		UE_DL_SCHED,
		RX_S1_PDU,
		RX_X2_PDU,
		RX_RLC_PDU,
		RX_RRC_PDU,
		RX_GTP_PDU,
		TIME_EXPIRE,
		MAX_MSG_NUM,
} MutiCoreMsgType;

typedef struct {
	/* the time that every thread node may be cost.*/
	INT32		time_cost;
	/*The counter time of every thread node.*/
	INT32		time_value_count;
	/*The counter time of every thread node that has been counted.*/
	INT32		time_num_count;

	INT32		flag;
	LteLock	lock_t;
}NodeTime;

typedef struct{
		ListType		lh;

		/*the value of the core that run the i/o thread is not zero, others are zero.*/
		INT32			need_time[MAX_MSG_NUM + 1];

		struct timeval	run_time;

		INT32			curr_run_node_time;

		LteLock		lock_t;

		/*When the list is null£¬using semb can avoid using of while.*/
		SemaType	semb;
}MutiCoreMsgList;

typedef struct{
		NodeType		ln;
		MutiCoreMsgType			msg_type;
		UINT16			rnti;
		UINT8				lc_id;
		UINT8				reseved;
		INT32				cost_time;
		THREAD_FUN	fun;  /*the thread node function*/
		ThreadArgValue			arg[MAX_THREAD_ARG]; /*the argument of the thread function.*/
}MutiCoreMsgNode;

/* Macros ------------------------------------------------------------------- */

/* Globals ------------------------------------------------------------------ */
extern LteLock	sched_lock_t;
extern UINT8 sched_num;
extern UINT8	ue_sched_flag;
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
INT32 init_muti_core(INT32 core_num);

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
INT32 clean_muti_core();

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
INT32 set_core_init_time(INT32 core_index, INT32 time);

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
INT32 add_thread_node(MutiCoreMsgNode *node_p, INT32 no_core_index, INT32 set_core_index);

/**********************************************************
 * get the thread node from the thread list.
 *
 * Input:
 * 	msg_list_p : the thread list of the core
 *
 * Output:
 * 	none
 *
 * Return:
 * 	node_p : the thread node.
 ************************************************************/
MutiCoreMsgNode* get_thread_node(MutiCoreMsgList *msg_list_p);

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
INT32 execute_thread(void *arg_p);
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
INT32 check_process();
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
INT32 get_process_id();

#endif /* LTE_MUTI_CORE_H_ */
