/*******************************************************************************
 ** 
 ** Copyright (c) 2006-2010 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: lte_msgq.c
 ** Description: Source file for wrapping message queue functions.
 **
 ** Current Version:1.3
 ** Author: ZHAGN Zidi (zhangzidi@ict.ac.cn)
 ** Date: 2009.07.17
 ** 
 ** Old Version: 1.0s
 ** $Revision: 4.1 $
 ** Author: Jihua Zhou (jhzhou@ict.ac.cn)
 ** Date: 2006.10.28
 **
 ******************************************************************************/

#include "lte_msgq.h"
#include "lte_log.h"
#include <unistd.h>
#include <pwd.h>

#ifndef SEM_ERR
#define SEM_ERR 0xffff
#endif

#ifndef SEM_TIMEOUT
#define SEM_TIMEOUT 0xfffe
#endif
/*******************************************************************************
 ** 
 Functions for VxWorks


5 functions:
MsgqType create_msgq(INT32 , INT32);
INT32 delete_msgq(MsgqType);
INT32 send_msgq(MsgqType, char *, UINT32,INT32,);
INT32 receive_msgq(MsgqType, char *, UINT32, INT32);
INT32 get_msgq_num(MsgqType);
 **
 ******************************************************************************/
#ifdef VXWORKS

/* Dependencies ------------------------------------------------------------- */

/* Constants ---------------------------------------------------------------- */

/* Types -------------------------------------------------------------------- */

/* Macros ------------------------------------------------------------------- */

/* Globals ------------------------------------------------------------------ */

/* Functions ---------------------------------------------------------------- */

/*******************************************************************************
 * Create and initialize a message queue.
 *
 * Input: max_msg_num: Max messages that can be queued.
 *        max_msg_len: Max bytes in a message.
 *
 * Output: return message queue identifier,
 *                NULL, if error.
 ******************************************************************************/
MsgqType create_msgq(INT32 max_msg_num, INT32 max_msg_len)
{
    return msgQCreate(max_msg_num, max_msg_len, MSG_QUEUE_OPTION);
}


/*******************************************************************************
 * Delete a message queue.
 *
 * Input: msgq_id: identifier of message queue to be deleted.
 *
 * Output: return 0, if success.
 *                -1, if deletion failed.
 ******************************************************************************/
INT32 delete_msgq(MsgqType msgq_id)
{
    if (msgQDelete(msgq_id) == ERROR) {
        return -1;
    }
    return 0;
}

/*******************************************************************************
 * Send a message to a message queue.
 *
 * Input: msgq_id: identifier of message queue on which to send.
 *        buffer_p: pointer to message to be sent.
 *        msg_len: length of message in bytes.
 *        time_out: ticks to wait.
 *                  this argument can also be NO_WAIT or WAIT_FOREVER.
 *        prio: priority of message to be sent.
 *              in VXWORKS: prio can only be MSG_PRI_NORMAL or MSG_PRI_URGENT,
 *                          other values are as MSG_PRI_URGENT. *
 * Output: return 0, if success.
 *                -1, if send failed.
 ******************************************************************************/
INT32 send_msgq(MsgqType msgq_id, char *buffer_p, UINT32 msg_len,
				INT32 time_out, INT32 prio)
{

    if (!buffer_p) 
	{
        return -1;
    }
    if (prio != MSG_PRI_NORMAL) 
	{
       prio = MSG_PRI_URGENT;
    }
    if (msgQSend(msgq_id, buffer_p, msg_len, time_out, prio) == ERROR) 
	{
        return -1;
    }
    return 0;

}

/*******************************************************************************
 * Receive a message from a message queue.
 *
 * Input: msgq_id: identifier of message queue from which to receive.
 *        buffer_p: pointer to buffer to receive message.
 *        msg_len: length of buffer which can hold received message.
 *        time_out: ticks to wait.
 *                  this argument can also be NO_WAIT or WAIT_FOREVER.
 *
 * Output: return 0, if success.
 *                -1, if receive failed.
 ******************************************************************************/
INT32 receive_msgq(MsgqType msgq_id, char *buffer_p, 
								 UINT32 msg_len, INT32 time_out)
{
    if (!buffer_p) {
        return -1;
    }
    if (msgQReceive(msgq_id, buffer_p, msg_len, time_out) == ERROR) {
        return -1;
    }
    return 0;
}


/*******************************************************************************
 * Get the number of messages queued to a message queue.
 *
 * Input: msgq_id: identifier of message queue to examine.
 *
 * Output: return the number of message queued, if success.
 *                -1, if error.
 ******************************************************************************/
INT32 get_msgq_num(MsgqType msgq_id)
{
	return msgQNumMsgs(msgq_id);
}


#elif defined(LINUX)
#ifdef __KERNEL__


/*******************************************************************************
 ** 
 Functions for Linux Kernel


6 functions:
MsgqType create_msgq(INT32, INT32);
INT32 delete_msgq(MsgqType );
INT32 send_msgq(MsgqType , char *, UINT32 ,INT32 , INT32 );
INT32 receive_msgq(MsgqType, char *,UINT32, INT32 );
INT32 get_msgq_num(MsgqType);
INT32 cleanup_msgqs(void);
 **
 ******************************************************************************/


/* Dependencies ------------------------------------------------------------- */

/*#include "lte_msgq.h"*/

/* Constants ---------------------------------------------------------------- */

/* Types -------------------------------------------------------------------- */

/* Macros ------------------------------------------------------------------- */

/* Globals ------------------------------------------------------------------ */

MsgqType g_msgq_id[MSG_QUEUE_NUM + 1];

/* Functions ---------------------------------------------------------------- */


/*******************************************************************************
 * Create and initialize a message queue.
 *
 * Input: max_msg_num: Max messages that can be queued.
 *        max_msg_len: Max bytes in a message.
 *                     0: transmit integer in this msg queue, this is a trick.
 *                     4: transmit pointer in this msg queue.
 *                     Others: not supported now.
 *
 * Output: return message queue identifier,
 *                NULL, if error.
 ******************************************************************************/



MsgqType create_msgq(INT32 max_msg_num, INT32 max_msg_len)
{
	MsgqType msgq_head_p;

	if ((INT32)g_msgq_id[0] > MSG_QUEUE_NUM) {
		/* Excess max. num of msg queues */
		return NULL;
	}
	if (((max_msg_len != MSGQ_MSG_SIZE) && (max_msg_len != 0))
			|| (max_msg_num == 0)) {
		return NULL;
	}
	if (!(msgq_head_p = lte_malloc(sizeof(MsgqLst)))) {
		return NULL;
	}
	init_list((ListType *)msgq_head_p);
	msgq_head_p->max_num = max_msg_num;
	if (max_msg_len == 0) {
		msgq_head_p->msg_size = 4;
		msgq_head_p->free_flag = 0;
	} else {
		msgq_head_p->msg_size = max_msg_len;
		msgq_head_p->free_flag = 1;
	}
	msgq_head_p->status = MSGQ_STATUS_EMPTY;
	msgq_head_p->rx_used = 0;
	msgq_head_p->tx_used = 0;
	init_waitqueue_head(&msgq_head_p->waitq);
	g_msgq_id[0] = (MsgqType)((INT32)g_msgq_id[0] + 1);	/* A message queue was successfully created */
	g_msgq_id[(INT32)g_msgq_id[0]] = msgq_head_p;	/* Recording msgq ID */

	return msgq_head_p;
}

/*******************************************************************************
 * Delete a message queue.
 *
 * Input: msgq_id: identifier of message queue to be deleted.
 *
 * Output: return 0, if success.
 *                -1, if deletion failed.
 ******************************************************************************/
INT32 delete_msgq(MsgqType msgq_id)
{
		MsgqNode *node_p;
		UINT32 count = 0;
		wait_queue_head_t my_queue;
		
		if (!msgq_id) {
			return -1;
		}
		if (msgq_id->status == MSGQ_STATUS_DELETE) {
			return -1;
		}
		if (msgq_id->status == MSGQ_STATUS_EMPTY) {
			msgq_id->status = MSGQ_STATUS_DELETE;
			wake_up_interruptible(&msgq_id->waitq);
		} else {
			msgq_id->status = MSGQ_STATUS_DELETE;
		}
		while (msgq_id->tx_used || msgq_id->rx_used) {
			if (count++ > 50) {
				return -1;
			}
			init_waitqueue_head(&my_queue);
			wait_event_interruptible_timeout(my_queue, 0, 10);
		}
		while (count_list((ListType *)msgq_id) > 0) {
			if (!(node_p = (MsgqNode *)get_list((ListType *)msgq_id))) {
				break;
			}
			if (msgq_id->free_flag) {
				/* 
				 * Free memory allocated by sender.
				 * In some cases, this operation will cause some problem,
				 * i.e. the memory has been freed by sender.
				 */
				if (node_p->msg_p) {
					lte_free(node_p->msg_p);
				}
			}		
			lte_free(node_p);
		}
		if (count_list((ListType *)msgq_id)) {
			return -1;
		}
	
		lte_free(msgq_id);
		
		return 0;
}
/*******************************************************************************
 * Send a message to a message queue.
 *
 * Input: msgq_id: identifier of message queue on which to send.
 *        buffer_p: pointer to message to be sent.
 *        msg_len: length of message in bytes.
 *        time_out: ticks to wait.
 *                  this argument can also be NO_WAIT or WAIT_FOREVER.
 *                  Any value other than NO_WAIT is as WAIT_FOREVER.
 *                  
 *        prio: no meaning.
 *
 * Output: return 0, if success.
 *                -1, if send failed.
 ******************************************************************************/
INT32 send_msgq(MsgqType msgq_id, char *buffer_p, UINT32 msg_len,
                				INT32 time_out, INT32 prio)
{
	MsgqNode *node_p;

    if (!buffer_p) {
        return -1;
    }
	if (!msgq_id || (msg_len != MSGQ_MSG_SIZE) ||
		(msgq_id->status == MSGQ_STATUS_DELETE) || (msgq_id->tx_used == 1)) {
		return -1;
	}
	msgq_id->tx_used = 1;
	if (msgq_id->status == MSGQ_STATUS_FULL) {
		/* Msgq is full */
		if (time_out == NO_WAIT) {
			msgq_id->tx_used = 0;
			return -1;
		}
	}
	/* 
	 * Msgq is not full or WAIT_FOREVER was set.
	 */
	if (!(node_p = (MsgqNode *)lte_malloc(sizeof(MsgqNode)))) {
		return -1;
	}
	node_p->msg_p = (char *)(*(UINT32 *)buffer_p);
	add_list((ListType *)msgq_id, (NodeType *)node_p);
	if (msgq_id->status == MSGQ_STATUS_EMPTY) {
		msgq_id->status = MSGQ_STATUS_NORMAL;
		wake_up_interruptible(&msgq_id->waitq);
	}
	if (count_list((ListType *)msgq_id) >= msgq_id->max_num) {
		msgq_id->status = MSGQ_STATUS_FULL;
	} else {
		msgq_id->status = MSGQ_STATUS_NORMAL;
	}
	msgq_id->tx_used = 0;
	
	return 0;
}

/*******************************************************************************
 * Receive a message from a message queue.
 *
 * Input: msgq_id: identifier of message queue from which to receive.
 *        buffer_p: pointer to buffer to receive message.
 *        msg_len: length of buffer which can hold received message.
 *        time_out: ticks to wait.
 *                  this argument can also be NO_WAIT or WAIT_FOREVER.
 *                  Any value other than NO_WAIT is as WAIT_FOREVER.
 *
 * Output: return 0, if success.
 *                -1, if receive failed.
 ******************************************************************************/
INT32 receive_msgq(MsgqType msgq_id, char *buffer_p,
								UINT32 msg_len, INT32 time_out)
{
	MsgqNode *node_p;
	
    if (!buffer_p) {
        return -1;
    }
	if (!msgq_id || (msg_len < msgq_id->msg_size) ||
			(msgq_id->status == MSGQ_STATUS_DELETE)/* ||
			(msgq_id->rx_used == 1)*/) {
		return -1;
	}
	/*
	 * An ugly way to protect the MSGQ from being recieved
	 * by more than one thread.
	 * ZJH: Fix me.
	 */
	msgq_id->rx_used = 1;
	
	if (msgq_id->status == MSGQ_STATUS_EMPTY) {
		/* Msgq is empty */
		if (time_out == NO_WAIT) {
			msgq_id->rx_used = 0;
			return -1;
		}
		/* Wait until Msgq is not empty */
		if (wait_event_interruptible(msgq_id->waitq,
				msgq_id->status != MSGQ_STATUS_EMPTY)) {
			flush_signals(current);
			msgq_id->rx_used = 0;
			return -1;
		}
		if (msgq_id->status == MSGQ_STATUS_DELETE) {
			msgq_id->rx_used = 0;
			return MSGQ_STATUS_DELETED;
		}
	}
	/*
	 * Msgq is not empty.
	 */
	node_p = (MsgqNode *)get_list((ListType *)msgq_id);
	if (!node_p) {
		msgq_id->rx_used = 0;
		return -1;
	}
	*(UINT32 *)buffer_p = (UINT32)(node_p->msg_p);
	lte_free(node_p);
	if (count_list((ListType *)msgq_id) < 1) {
		msgq_id->status = MSGQ_STATUS_EMPTY;
	}
	msgq_id->rx_used = 0;
	
	return 0;
}

/*******************************************************************************
 * Get the number of messages queued to a message queue.
 *
 * Input: msgq_id: identifier of message queue to examine.
 *
 * Output: return the number of message queued, if success.
 *                -1, if error.
 ******************************************************************************/
INT32 get_msgq_num(MsgqType msgq_id)
{
	INT32 num;

	num = count_list((ListType *)msgq_id);
	
	return (msgq_id->max_num < num) ? msgq_id->max_num: num;	
}

/*******************************************************************************
 * Clean up message queues created, freeing memory used by them.
 *
 * Input: None.
 *
 * Output: return 0, if success.
 *                -1, if error.
 ******************************************************************************/
INT32 cleanup_msgqs(void)
{
	INT32 i;

	for (i = 1; i <= (INT32)g_msgq_id[0]; i++) {
		if (g_msgq_id[i]) {
			delete_msgq(g_msgq_id[i]);
			lte_free(g_msgq_id[i]);
		}
	}
	g_msgq_id[0] = 0;
	
	return 0; 
}

#else	/* !__KERNEL__ */

/*******************************************************************************
 ** 
 Functions for Linux non-Kernel


5 functions:
MsgqType create_msgq(INT32 , INT32);
INT32 delete_msgq(MsgqType);
INT32 send_msgq(MsgqType, char *, UINT32,INT32,);
INT32 receive_msgq(MsgqType, char *, UINT32, INT32);
INT32 get_msgq_num(MsgqType);

 **
 ******************************************************************************/

/* Dependencies ------------------------------------------------------------- */

/* Constants ---------------------------------------------------------------- */

/* Types -------------------------------------------------------------------- */

/* Macros ------------------------------------------------------------------- */
#define MAX_MSGQ_NAME_LEN         256
/* Globals ------------------------------------------------------------------ */

/* Functions ---------------------------------------------------------------- */

/******************************************************************************
 * Open a message queue with a given name.
 *
 * Input: msgq_name: name of message queue to be opened, i.e. /mymsgqname
 *        max_msg_num: Max messages that can be queued.
 *        max_msg_len: Max bytes in a message.
 * 
 * Output: return message queue identifier,
 *                NULL, if error.
 ******************************************************************************/
MsgqType open_msgq(char *msgq_name, INT32 max_msg_num, 
								INT32 max_msg_len)
{
    MsgqType msgq_id;
    struct mq_attr attr;
    char mqname[MAX_MSGQ_NAME_LEN];
    struct passwd *pwd = NULL;

    if (!msgq_name) {
        return MSGQ_ERROR;
    }

    if ((pwd = getpwuid(geteuid())) == NULL) {
	    return MSGQ_ERROR;
    }

    snprintf(mqname, MAX_MSGQ_NAME_LEN, "%s_%s", msgq_name, pwd->pw_name);

    attr.mq_maxmsg = max_msg_num;
    attr.mq_msgsize = max_msg_len;

    /* Need to be deleted if exists , or may exceed system limit */
    mq_unlink(mqname);
    /* Open a message queue */
    msgq_id = mq_open(mqname, MSG_QUEUE_FLAG, MSG_QUEUE_MODE, &attr);

    if (msgq_id == (mqd_t)-1) {
    	log_msg(LOG_ERR, 0, "open msg_queue err:%s\n", msgq_name);
        return MSGQ_ERROR;
    }
    endpwent();

    return msgq_id;
}

/*******************************************************************************
 * Close and delete a message queue.
 * 
 * Input: msgq_name: name of message queue to be deleted,
 *                   if NULL, do not delete it.
 *        msgq_id: identifier of message queue to be closed.
 *    
 * Output: return 0, if success.
 *               -1, if close or deletion failed.
 ******************************************************************************/
INT32 close_msgq(char *msgq_name, MsgqType msgq_id)
{
    INT32 ret;
	char mqname[MAX_MSGQ_NAME_LEN];
	struct passwd *pwd = NULL;

    /* Close */
    ret = mq_close(msgq_id);

    if (!msgq_name) {
        return MSGQ_ERROR;
    }

    if ((pwd = getpwuid(geteuid())) == NULL) {
        return MSGQ_ERROR;
    }

    snprintf(mqname, MAX_MSGQ_NAME_LEN, "%s_%s", msgq_name, pwd->pw_name);
    /* Delete */
    if (mqname) {
        mq_unlink(mqname);
    }

    return ret;
}


/*******************************************************************************
 * Send a message to a message queue.
 *
 * Input: msgq_id: identifier of message queue on which to send.
 *        buffer_p: pointer to message to be sent.
 *        msg_len: length of message in bytes.
 *        time_out: ticks to wait.
 *                  this argument can also be NO_WAIT or WAIT_FOREVER.
 *        prio: priority of message to be sent.
 *              in VXWORKS: prio can only be MSG_PRI_NORMAL or MSG_PRI_URGENT,
 *                          other values are as MSG_PRI_URGENT.
 *              in LINUX: prio can be MSG_PRI_NORMAL, MSG_PRI_URGENT,
 *                        or an integer from 1 to MQ_PRIO_MAX. 
 *
 * Output: return 0, if success.
 *                -1, if send failed.
 ******************************************************************************/
INT32 send_msgq(MsgqType msgq_id, char *buffer_p, UINT32 msg_len,
				INT32 time_out, INT32 prio)
{
    struct mq_attr attr;

    if (!buffer_p) {
        return -1;
    }
    if (time_out == NO_WAIT) {
        attr.mq_flags = O_NONBLOCK;
        mq_setattr(msgq_id, &attr, NULL);

        return mq_send(msgq_id, buffer_p, msg_len, prio);
    } else if(time_out == WAIT_FOREVER) {
        attr.mq_flags = 0;
        mq_setattr(msgq_id, &attr, NULL);

        return mq_send(msgq_id, buffer_p, msg_len, prio);
    } else {
        struct timespec tms;

        tms.tv_sec = time_out / CLOCKS_PER_SEC;
        tms.tv_nsec = (time_out % CLOCKS_PER_SEC) *
                        (1000000000 / CLOCKS_PER_SEC);
        return mq_timedsend(msgq_id, buffer_p, msg_len, prio, &tms);
    }
}
/*******************************************************************************
 * Receive a message from a message queue.
 *
 * Input: msgq_id: identifier of message queue from which to receive.
 *        buffer_p: pointer to buffer to receive message.
 *        msg_len: length of buffer which can hold received message.
 *        time_out: ticks to wait.
 *                  this argument can also be NO_WAIT or WAIT_FOREVER.
 *
 * Output: return 0, if success.
 *                -1, if receive failed.
 ******************************************************************************/
INT32 receive_msgq(MsgqType msgq_id, char *buffer_p, 
								 UINT32 msg_len, INT32 time_out)
{
    struct mq_attr attr;

    if (!buffer_p) {
        return -1;
    }
    if (time_out == NO_WAIT) {
        attr.mq_flags = O_NONBLOCK;
        mq_setattr(msgq_id, &attr, NULL);

        return mq_receive(msgq_id, buffer_p, msg_len, NULL);
    } else if(time_out == WAIT_FOREVER) {
        attr.mq_flags = 0;
        mq_setattr(msgq_id, &attr, NULL);

        return mq_receive(msgq_id, buffer_p, msg_len, NULL);
    } else {
        struct timespec tms;

        tms.tv_sec = time_out / CLOCKS_PER_SEC;
        tms.tv_nsec = (time_out % CLOCKS_PER_SEC) *
                        (1000000000 / CLOCKS_PER_SEC);

        return mq_timedreceive(msgq_id, buffer_p, msg_len, NULL, &tms);
    }

}
/*******************************************************************************
 * Get the number of messages queued to a message queue.
 *
 * Input: msgq_id: identifier of message queue to examine.
 *
 * Output: return the number of message queued, if success.
 *                -1, if error.
 ******************************************************************************/
INT32 get_msgq_num(MsgqType msgq_id)
{
	struct mq_attr attr;
    if (mq_getattr(msgq_id, &attr) < 0) {
        return -1;
    }
    return attr.mq_curmsgs;
}


#endif	/* __KERNEL__ */

#elif defined(RTAI)
#ifdef __KERNEL__
/*******************************************************************************
 ** 
 Functions for Linux Kernel


6 functions:
MsgqType create_msgq(INT32, INT32);
INT32 delete_msgq(MsgqType );
INT32 send_msgq(MsgqType , char *, UINT32 ,INT32 , INT32 );
INT32 receive_msgq(MsgqType, char *,UINT32, INT32 );
INT32 get_msgq_num(MsgqType);
INT32 cleanup_msgqs(void);
 **
 ******************************************************************************/


/* Dependencies ------------------------------------------------------------- */

#include "lte_print.h"

/* Constants ---------------------------------------------------------------- */

/* Types -------------------------------------------------------------------- */

/* Macros ------------------------------------------------------------------- */

#define MBX_TYPE  RES_Q


/* Globals ------------------------------------------------------------------ */

MsgqType g_msgq_id[MSG_QUEUE_NUM + 1];

/* Functions ---------------------------------------------------------------- */


/*******************************************************************************
 * Create and initialize a message queue.
 *
 * Input: max_msg_num: Max messages that can be queued.
 *        max_msg_len: Max bytes in a message.
 *                     0: transmit integer in this msg queue, this is a trick.
 *                     4: transmit pointer in this msg queue.
 *                     Others: not supported now.
 *
 * Output: return message queue identifier,
 *                NULL, if error.
 ******************************************************************************/
MsgqType create_msgq(INT32 max_msg_num, INT32 max_msg_len)
{

	MsgqType msgq_head_p;
	if ((INT32)g_msgq_id[0] > MSG_QUEUE_NUM) {
		/* Excess max. num of msg queues */
		return NULL;
	}
	if (((max_msg_len != MSGQ_MSG_SIZE) && (max_msg_len != 0))
			|| (max_msg_num == 0)) {
		return NULL;
	}
	if (!(msgq_head_p = lte_malloc(sizeof(MsgqLst)))) {
		return NULL;
	}
	init_list((ListType *)msgq_head_p);
	msgq_head_p->max_num = max_msg_num;
	if (max_msg_len == 0) {
		msgq_head_p->msg_size = 4;
		msgq_head_p->free_flag = 0;
	} else {
		msgq_head_p->msg_size = max_msg_len;
		msgq_head_p->free_flag = 1;
	}
	msgq_head_p->status = MSGQ_STATUS_EMPTY;
	//msgq_head_p->rx_used = 0;
	//msgq_head_p->tx_used = 0;

	msgq_head_p->empty_sem = create_semb(SEM_EMPTY);
	msgq_head_p->full_sem = create_semb(SEM_EMPTY);
	msgq_head_p->send_sem =  create_semb(SEM_FULL);
	msgq_head_p->receive_sem	= create_semb(SEM_FULL);


	g_msgq_id[0] = (MsgqType)((INT32)g_msgq_id[0] + 1);	/* A message queue was successfully created */
	g_msgq_id[(INT32)g_msgq_id[0]] = msgq_head_p;	/* Recording msgq ID */

	return msgq_head_p;
}

/*******************************************************************************
 * Delete a message queue.
 *
 * Input: msgq_id: identifier of message queue to be deleted.
 *
 * Output: return 0, if success.
 *                -1, if deletion failed.
 ******************************************************************************/
INT32 delete_msgq(MsgqType msgq_id)
{
	MsgqNode *node_p;

	if (!msgq_id) {
		return -1;
	}
	
	/*If the msgq has been deleted before, just return 0*/
	if (msgq_id->status == MSGQ_STATUS_DELETE) {
		return 0;
	}
	
	if(take_sem(msgq_id->send_sem, NO_WAIT)<0){
		return -1;
	}
	/*********skip it for test
	if(take_sem(msgq_id->receive_sem, NO_WAIT)<0){
		return -3;
	}*/

	msgq_id->status = MSGQ_STATUS_DELETE;
	/*	
	if (msgq_id->status == MSGQ_STATUS_EMPTY) {
		msgq_id->status = MSGQ_STATUS_DELETE;
		
	} else {
		msgq_id->status = MSGQ_STATUS_DELETE;
	}
	*/
	
	while (count_list((ListType *)msgq_id) > 0) {
		if (!(node_p = (MsgqNode *)get_list((ListType *)msgq_id))) {
			break;
		}
		if (msgq_id->free_flag) {
			/* 
			 * Free memory allocated by sender.
			 * In some cases, this operation will cause some problem,
			 * i.e. the memory has been freed by sender.
			 */
			if (node_p->msg_p) {
				lte_free(node_p->msg_p);
			}
		}		
		lte_free(node_p);
	}
	if (count_list((ListType *)msgq_id)) {
		return -1;
	}
	if(delete_sem(msgq_id->send_sem) || 
		delete_sem(msgq_id->receive_sem) ||
			delete_sem(msgq_id->empty_sem) ||
				delete_sem(msgq_id->full_sem)){
		return -1;
	}

	lte_free(msgq_id);
	
	return 0;
}


/*******************************************************************************
 * Send a message to a message queue.
 *
 * Input: msgq_id: identifier of message queue on which to send.
 *        buffer_p: pointer to message to be sent.
 *        msg_len: length of message in bytes.
 *        time_out: ticks to wait.
 *                  this argument can also be NO_WAIT or WAIT_FOREVER.
 *                  Any value other than NO_WAIT is as WAIT_FOREVER.
 *                  
 *        prio: no meaning.
 *
 * Output: return 0, if success.
 *                -1, if send failed.
 ******************************************************************************/
INT32 send_msgq(MsgqType msgq_id, char *buffer_p, UINT32 msg_len,
                				INT32 time_out, INT32 prio)
{
	MsgqNode *node_p; 
	/*valid agument?*/
	if (!buffer_p) {
        		return -1;
    	}
	if (!msgq_id || (msg_len != MSGQ_MSG_SIZE) ||
		(msgq_id->status == MSGQ_STATUS_DELETE) ){
		return -1;
	}

	if(take_sem(msgq_id->send_sem, time_out) < 0) return -1;

	if(msgq_id->status == MSGQ_STATUS_NORMAL ||msgq_id->status == MSGQ_STATUS_EMPTY){
		take_sem(msgq_id->full_sem, NO_WAIT);
	}	

	/*msgq is full*/
	if (msgq_id->status == MSGQ_STATUS_FULL) {
		if(take_sem(msgq_id->full_sem,time_out) < 0) {	
			give_sem(msgq_id->send_sem);
			return -1;
		}
	}
	/* 
	 * Msgq is not full or WAIT_FOREVER was set.
	 */
	if (!(node_p = (MsgqNode *)lte_malloc(sizeof(MsgqNode)))) {
		give_sem(msgq_id->send_sem);
		return -1;
	}

	//node_p->msg_p = (char *)buffer_p;
	node_p->msg_p = (char *)(*(UINT32 *)buffer_p);

	add_list((ListType *)msgq_id, (NodeType *)node_p);

	if (msgq_id->status == MSGQ_STATUS_EMPTY) {
		msgq_id->status = MSGQ_STATUS_NORMAL;
		//printf("status normal...\n");
		//if(msgq_id->empty_sem->owndby != 0){
			give_sem(msgq_id->empty_sem);
			//printf("give the empty sem!\n");
		//}
	}
	
	if (count_list((ListType *)msgq_id) >= msgq_id->max_num){
		msgq_id->status = MSGQ_STATUS_FULL;
	} else {
		msgq_id->status = MSGQ_STATUS_NORMAL;
	}

	give_sem(msgq_id->send_sem);

	return 0;

}

/*******************************************************************************
 * Receive a message from a message queue.
 *
 * Input: msgq_id: identifier of message queue from which to receive.
 *        buffer_p: pointer to buffer to receive message.
 *        msg_len: length of buffer which can hold received message.
 *        time_out: ticks to wait.
 *                  this argument can also be NO_WAIT or WAIT_FOREVER.
 *                  Any value other than NO_WAIT is as WAIT_FOREVER.
 *
 * Output: return 0, if success.
 *                -1, if receive failed.
 ******************************************************************************/
INT32 receive_msgq(MsgqType msgq_id, char *buffer_p,
								UINT32 msg_len, INT32 time_out)
{
	MsgqNode *node_p;

	/* valid argument? */
    	if (!buffer_p) {
        		return -1;
    	}

	if (msgq_id->status == MSGQ_STATUS_DELETE) {
		return MSGQ_STATUS_DELETED;
	}

	if (!msgq_id || (msg_len < msgq_id->msg_size)) {
		return -1;
	}

	if(take_sem(msgq_id->receive_sem, time_out) < 0) return -1;

	if(msgq_id->status == MSGQ_STATUS_NORMAL ||msgq_id->status == MSGQ_STATUS_FULL){
		take_sem(msgq_id->empty_sem, NO_WAIT);
	}
		
	/* Msgq is empty */	
	if (msgq_id->status == MSGQ_STATUS_EMPTY) {
		//printf("receiving empty...\n");
		if(take_sem(msgq_id->empty_sem,time_out) < 0){
			give_sem(msgq_id->receive_sem);
			return -1;
		}

	}
	//printf("received...\n");
	

	node_p = (MsgqNode *)get_list((ListType *)msgq_id);
	if (!node_p) {
		give_sem(msgq_id->receive_sem);
		return -1;
	}
	
	*(UINT32 *)buffer_p = (UINT32)(node_p->msg_p);

	lte_free(node_p);

	if (msgq_id->status == MSGQ_STATUS_FULL) {
		msgq_id->status = MSGQ_STATUS_NORMAL;
		//if(msgq_id->full_sem->owndby != 0){
			give_sem(msgq_id->full_sem);
		//}
	}
	
	if (count_list((ListType *)msgq_id) < 1) {
		msgq_id->status = MSGQ_STATUS_EMPTY;
	}
	
	give_sem(msgq_id->receive_sem);
	
	return 0;
		
}



INT32 get_msgq_num(MsgqType msgq_id)
{
	INT32 num;

	num = count_list((ListType *)msgq_id);
	
	return (msgq_id->max_num < num) ? msgq_id->max_num: num;	
}



/*******************************************************************************
 * Clean up message queues created, freeing memory used by them.
 *
 * Input: None.
 *
 * Output: return 0, if success.
 *                -1, if error.
 ******************************************************************************/
INT32 cleanup_msgqs(void)
{
	INT32 i;
	//rt_printk("Cleanup_msgqs...\n");
	for (i = 1; i <= (INT32)g_msgq_id[0]; i++) {
		if (g_msgq_id[i]) {
			delete_msgq(g_msgq_id[i]);
			//free(g_msgq_id[i]);
		}
	}
	g_msgq_id[0] = 0;
	
	return 0; 
}

#else /*!__KERNEL__*/
/* Macros ------------------------------------------------------------------- */

#define MBX_TYPE  RES_Q


/* Globals ------------------------------------------------------------------ */

MsgqType g_msgq_id[MSG_QUEUE_NUM + 1];

/*******************************************************************************
 * Create and initialize a message queue.
 *
 * Input: max_msg_num: Max messages that can be queued.
 *        max_msg_len: Max bytes in a message.
 *                     0: transmit integer in this msg queue, this is a trick.
 *                     4: transmit pointer in this msg queue.
 *                     Others: not supported now.
 *
 * Output: return message queue identifier,
 *                NULL, if error.
 ******************************************************************************/
MsgqType create_msgq(INT32 max_msg_num, INT32 max_msg_len)
{

	MsgqType msgq_head_p;
	if ((INT32)g_msgq_id[0] > MSG_QUEUE_NUM) {
		/* Excess max. num of msg queues */
		return NULL;
	}
	if (((max_msg_len != MSGQ_MSG_SIZE) && (max_msg_len != 0))
			|| (max_msg_num == 0)) {
		return NULL;
	}
	if (!(msgq_head_p = lte_malloc(sizeof(MsgqLst)))) {
		return NULL;
	}
	
	init_list((ListType *)msgq_head_p);
	msgq_head_p->max_num = max_msg_num;
	if (max_msg_len == 0) {
		msgq_head_p->msg_size = 4;
		msgq_head_p->free_flag = 0;
	} else {
		msgq_head_p->msg_size = max_msg_len;
		msgq_head_p->free_flag = 1;
	}
	msgq_head_p->status = MSGQ_STATUS_EMPTY;

	msgq_head_p->empty_sem = create_semb(SEM_EMPTY);
	msgq_head_p->full_sem = create_semb(SEM_EMPTY);
	msgq_head_p->send_sem =  create_semb(SEM_FULL);
	msgq_head_p->receive_sem	= create_semb(SEM_FULL);
	
	g_msgq_id[0] = (MsgqType)((INT32)g_msgq_id[0] + 1);	/* A message queue was successfully created */
	g_msgq_id[(INT32)g_msgq_id[0]] = msgq_head_p;	/* Recording msgq ID */
	return msgq_head_p;
}

/*******************************************************************************
 * Delete a message queue.
 *
 * Input: msgq_id: identifier of message queue to be deleted.
 *
 * Output: return 0, if success.
 *                -1, if deletion failed.
 ******************************************************************************/
INT32 delete_msgq(MsgqType msgq_id)
{
	MsgqNode *node_p;

	if (!msgq_id) {
		return -1;
	}
	if(take_sem(msgq_id->send_sem, NO_WAIT)<0){
	log_msg(LOG_INFO, 0, "delete failed -- send not done\n");//for test;
		return -2;
	}
	if(take_sem(msgq_id->receive_sem, NO_WAIT)<0){
		log_msg(LOG_INFO, 0, "delete failed --- receive not done\n");//for test;
		return -3;
	}
	
	/*If the msgq has been deleted before, just return 0*/
	if (msgq_id->status == MSGQ_STATUS_DELETE) {
		return 0;
	}	
	msgq_id->status = MSGQ_STATUS_DELETE;
	/*	
	if (msgq_id->status == MSGQ_STATUS_EMPTY) {
		msgq_id->status = MSGQ_STATUS_DELETE;
		
	} else {
		msgq_id->status = MSGQ_STATUS_DELETE;
	}
	*/
	
	while (count_list((ListType *)msgq_id) > 0) {
		if (!(node_p = (MsgqNode *)get_list((ListType *)msgq_id))) {
			break;
		}
		if (msgq_id->free_flag) {
			/* 
			 * Free memory allocated by sender.
			 * In some cases, this operation will cause some problem,
			 * i.e. the memory has been freed by sender.
			 */
			if (node_p->msg_p) {
				lte_free(node_p->msg_p);
			}
		}		
		lte_free(node_p);
	}	// end of while

	if (count_list((ListType *)msgq_id)) {
		log_msg(LOG_INFO, 0, "The msgq isn't null; erorr!\n");//for test;
		return -4;
	}
	if(delete_sem(msgq_id->send_sem) || 
		delete_sem(msgq_id->receive_sem) ||
			delete_sem(msgq_id->empty_sem) ||
				delete_sem(msgq_id->full_sem)){
        log_msg(LOG_INFO, 0, "delete failed -- delete sem fail!\n");//for test;
 
		return -5;
	}
	lte_free(msgq_id);
	return 0;
}
/*******************************************************************************
 * Send a message to a message queue.
 *
 * Input: msgq_id: identifier of message queue on which to send.
 *        buffer_p: pointer to message to be sent.
 *        msg_len: length of message in bytes.
 *        time_out: ticks to wait.
 *                  this argument can also be NO_WAIT or WAIT_FOREVER.
 *                  Any value other than NO_WAIT is as WAIT_FOREVER.
 *                  
 *        prio: no meaning.
 *
 * Output: return 0, if success.
 *                -1, if send failed.
 ******************************************************************************/
INT32 send_msgq(MsgqType msgq_id, char *buffer_p, UINT32 msg_len,
                				INT32 time_out, INT32 prio)
{
	MsgqNode *node_p; 
	/*valid agument?*/
	if (!buffer_p) {
        		return -1;
    	}
	if (!msgq_id || (msg_len != MSGQ_MSG_SIZE) ||(msgq_id->status == MSGQ_STATUS_DELETE) ){
		return -1;
	}

	if(take_sem(msgq_id->send_sem, time_out) < 0) return -1;
	
	if(msgq_id->status == MSGQ_STATUS_NORMAL ||msgq_id->status == MSGQ_STATUS_EMPTY){
		take_sem(msgq_id->full_sem, NO_WAIT);
	}	

	/*msgq is full*/
	if (msgq_id->status == MSGQ_STATUS_FULL) {
		if(take_sem(msgq_id->full_sem,time_out) < 0) {	
			give_sem(msgq_id->send_sem);
			return -1;
		}
	}
	
	/* 
	 * Msgq is not full or WAIT_FOREVER was set.
	 */
	if (!(node_p = (MsgqNode *)lte_malloc(sizeof(MsgqNode)))) {
		give_sem(msgq_id->send_sem);
		return -1;
	}

	node_p->msg_p = (char *)(*(UINT32*)buffer_p);

	add_list((ListType *)msgq_id, (NodeType *)node_p);

	if (msgq_id->status == MSGQ_STATUS_EMPTY) {
		msgq_id->status = MSGQ_STATUS_NORMAL;
		give_sem(msgq_id->empty_sem);
	}
	
	if (count_list((ListType *)msgq_id) >= msgq_id->max_num){
		msgq_id->status = MSGQ_STATUS_FULL;
	} else {
		msgq_id->status = MSGQ_STATUS_NORMAL;
	}
	give_sem(msgq_id->send_sem);
	return 0;
}
/*******************************************************************************
 * Receive a message from a message queue.
 *
 * Input: msgq_id: identifier of message queue from which to receive.
 *        buffer_p: pointer to buffer to receive message.
 *        msg_len: length of buffer which can hold received message.
 *        time_out: ticks to wait.
 *                  this argument can also be NO_WAIT or WAIT_FOREVER.
 *                  Any value other than NO_WAIT is as WAIT_FOREVER.
 *
 * Output: return 0, if success.
 *                -1, if receive failed.
 ******************************************************************************/
INT32 receive_msgq(MsgqType msgq_id, char *buffer_p,
								UINT32 msg_len, INT32 time_out)
{
	MsgqNode *node_p;

	/* valid argument? */
    	if (!buffer_p) {
        		return -1;
    	}

	if (msgq_id->status == MSGQ_STATUS_DELETE) {
		return MSGQ_STATUS_DELETED;
	}

	if (!msgq_id || (msg_len < msgq_id->msg_size)) {
		return -1;
	}
	
	if(take_sem(msgq_id->receive_sem, time_out) < 0) return -1;

	if(msgq_id->status == MSGQ_STATUS_NORMAL ||msgq_id->status == MSGQ_STATUS_FULL){
		take_sem(msgq_id->empty_sem, NO_WAIT);
	}
	
	/* Msgq is empty */	
	if (msgq_id->status == MSGQ_STATUS_EMPTY) {
		if(take_sem(msgq_id->empty_sem,time_out) < 0){
			give_sem(msgq_id->receive_sem);
			return -1;
		}
	}

	node_p = (MsgqNode *)get_list((ListType *)msgq_id);
	if (!node_p) {
		give_sem(msgq_id->receive_sem);
		return -1;
	}

	*(UINT32 *)buffer_p = (UINT32)(node_p->msg_p);

	lte_free(node_p);

	if (msgq_id->status == MSGQ_STATUS_FULL) {
		msgq_id->status = MSGQ_STATUS_NORMAL;
			give_sem(msgq_id->full_sem);
	}
	
	if (count_list((ListType *)msgq_id) < 1) {
		msgq_id->status = MSGQ_STATUS_EMPTY;
	}
	give_sem(msgq_id->receive_sem);
	return 0;
		
}

INT32 get_msgq_num(MsgqType msgq_id)
{
	INT32 num;

	num = count_list((ListType *)msgq_id);
	
	return (msgq_id->max_num < num) ? msgq_id->max_num: num;	
}



/*******************************************************************************
 * Clean up message queues created, freeing memory used by them.
 *
 * Input: None.
 *
 * Output: return 0, if success.
 *                -1, if error.
 ******************************************************************************/
INT32 cleanup_msgqs(void)
{
	INT32 i;
	//rt_printk("Cleanup_msgqs...\n");
	for (i = 1; i <= (INT32)g_msgq_id[0]; i++) {
		if (g_msgq_id[i]) {
			delete_msgq(g_msgq_id[i]);
			//free(g_msgq_id[i]);
		}
	}
	g_msgq_id[0] = 0;
	
	return 0; 
}
#endif
#endif
