/*******************************************************************************
 ** 
 ** Copyright (c) 2006-2010 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: lte_msgq.h
 ** Description: Header file for wrapping message queue functions.
 **
 ** Current Version: 1.2
 ** $Revision: 4.1 $
 ** Author: Jihua Zhou (jhzhou@ict.ac.cn)
 ** Date: 2006.10.10
 **
 ** Old Version: 1.1
 ** Author: Jihua Zhou (jhzhou@ict.ac.cn)
 ** Date: 2006.04.06
 **
 ** Old Version: 1.0
 ** Author: Jihua Zhou (jhzhou@ict.ac.cn)
 ** Date: 2006.01.08
 **
 ******************************************************************************/
#ifndef MSGQ_H
#define MSGQ_H
/*******************************************************************************
 ** 
 Declaration for all **
 ******************************************************************************/

/* Dependencies ------------------------------------------------------------- */
#include "lte_type.h"
/* Constants ---------------------------------------------------------------- */
#define MSGQ_STATUS_DELETED -2
/* Types -------------------------------------------------------------------- */
/* Macros ------------------------------------------------------------------- */
/* Globals ------------------------------------------------------------------ */
/* Functions ---------------------------------------------------------------- */



/*******************************************************************************
 ** 
 Declaration for VxWorks **
 ******************************************************************************/

#ifdef VXWORKS
/* Dependencies ------------------------------------------------------------- */
#include <msgQLib.h>
/* Constants ---------------------------------------------------------------- */
#define MSG_QUEUE_OPTION MSG_Q_FIFO /* Max. number of messages in pipe */

/* Types -------------------------------------------------------------------- */
typedef MSG_Q_ID MsgqType;

/* Macros ------------------------------------------------------------------- */
/* Globals ------------------------------------------------------------------ */
/* Functions ---------------------------------------------------------------- */
/**
 * 消息队列是不是就这几个API
 *   不知道，再看看吧
 */
MsgqType create_msgq(INT32 , INT32);
INT32 delete_msgq(MsgqType);
INT32 send_msgq(MsgqType, char *, UINT32,INT32,);
INT32 receive_msgq(MsgqType, char *, UINT32, INT32);
INT32 get_msgq_num(MsgqType);


#elif defined(LINUX)

/*******************************************************************************
 ** 
 Declaration for Linux(both kernel & non-kernel) **
 ******************************************************************************/
/* __KERNEL && !__KERNEL */
/* Dependencies ------------------------------------------------------------- */
/* Constants ---------------------------------------------------------------- */
#define MSG_QUEUE_NUM   128 /* Limit max. number of message queues to 128 */
#define NO_WAIT 0
#define WAIT_FOREVER -1
#define MSG_PRI_NORMAL 0
#define MSG_PRI_URGENT (MQ_PRIO_MAX - 1)

/* Types -------------------------------------------------------------------- */
/* Macros ------------------------------------------------------------------- */
/* Globals ------------------------------------------------------------------ */
/* Functions ---------------------------------------------------------------- */

/*******************************************************************************
 ** 
 Declaration for Linux kernel only **
 ******************************************************************************/
/* __KERNEL__ */
#ifdef __KERNEL__
/* Dependencies ------------------------------------------------------------- */
#include <linux/sched.h>
#include <linux/wait.h>
#include <linux/errno.h>
#include "lte_malloc.h"
#include "lte_list.h"





/* Constants ---------------------------------------------------------------- */
#define MQ_PRIO_MAX 32
#define MSGQ_STATUS_NORMAL 0
#define MSGQ_STATUS_DELETE -1
#define MSGQ_STATUS_FULL -2
#define MSGQ_STATUS_EMPTY -3
#define MSGQ_MSG_SIZE 4

/* Types -------------------------------------------------------------------- */
typedef struct {
	ListType lh;
	INT32 max_num;
	INT32 msg_size;
	INT32 status;
	INT32 rx_used;
	INT32 tx_used;
	INT32 free_flag;
	wait_queue_head_t waitq;
} MsgqLst;
typedef struct {
	NodeType ln;
	char *msg_p;
} MsgqNode;

typedef MsgqLst* MsgqType;

/* Macros ------------------------------------------------------------------- */

/* Globals ------------------------------------------------------------------ */
extern MsgqType g_msgq_id[MSG_QUEUE_NUM + 1];

/* Functions ---------------------------------------------------------------- */
MsgqType create_msgq(INT32, INT32);
INT32 delete_msgq(MsgqType );
INT32 send_msgq(MsgqType , char *, UINT32 ,INT32 , INT32 );
INT32 receive_msgq(MsgqType, char *,UINT32, INT32 );
INT32 get_msgq_num(MsgqType);
INT32 cleanup_msgqs(void);



/*******************************************************************************
 ** 
 Declaration for Linux non-kernel only **
 ******************************************************************************/
/* !__KERNEL__ */
#else
/* Dependencies ------------------------------------------------------------- */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <mqueue.h>


/* Constants ---------------------------------------------------------------- */
#define MSGQ_STATUS_NORMAL 0
#define MSGQ_STATUS_DELETE -1
#define MSGQ_STATUS_FULL -2
#define MSGQ_STATUS_EMPTY -3
#define MSGQ_MSG_SIZE 10

#define MSG_QUEUE_FLAG O_RDWR | O_CREAT
#define MSG_QUEUE_MODE S_IRUSR | S_IWUSR
#define MSGQ_HNAME_LEN  8   /* Length of head part of msg queue name */

#define MSGQ_ERROR      -1

typedef mqd_t MsgqType;


/* Macros ------------------------------------------------------------------- */

/* Globals ------------------------------------------------------------------ */

/* Functions ---------------------------------------------------------------- */
MsgqType open_msgq(char *msgq_name, INT32 max_msg_num, INT32 max_msg_len);
INT32 close_msgq(char *msgq_name, MsgqType msgq_id);
INT32 send_msgq(MsgqType , char *, UINT32 ,INT32 , INT32 );
INT32 receive_msgq(MsgqType, char *,UINT32, INT32 );
INT32 get_msgq_num(MsgqType);
INT32 cleanup_msgqs(void);


#endif


#elif defined(RTAI)

/*******************************************************************************
 ** 
 Declaration for Linux(both kernel & non-kernel) **
 ******************************************************************************/
/* __KERNEL && !__KERNEL */
/* Dependencies ------------------------------------------------------------- */
/* Constants ---------------------------------------------------------------- */
/* Constants ---------------------------------------------------------------- */
#define MSG_QUEUE_NUM   128 /* Limit max. number of message queues to 128 */
#define NO_WAIT 0
#define WAIT_FOREVER -1
#define MSG_PRI_NORMAL 0
#define MSG_PRI_URGENT (MQ_PRIO_MAX - 1)


/* Types -------------------------------------------------------------------- */
/* Macros ------------------------------------------------------------------- */
/* Globals ------------------------------------------------------------------ */
/* Functions ---------------------------------------------------------------- */

/*******************************************************************************
 ** 
 Declaration for Linux kernel only **
 ******************************************************************************/
/* __KERNEL__ */
#ifdef __KERNEL__
/* Dependencies ------------------------------------------------------------- */

#include "lte_malloc.h"
#include "lte_list.h"
#include "lte_sema.h"

/* Constants ---------------------------------------------------------------- */
#define MQ_PRIO_MAX 32
#define MSGQ_STATUS_NORMAL 0
#define MSGQ_STATUS_DELETE -1
#define MSGQ_STATUS_FULL -2
#define MSGQ_STATUS_EMPTY -3
#define MSGQ_MSG_SIZE 4

/* Types -------------------------------------------------------------------- */
typedef struct {
	ListType lh;
	INT32 max_num;
	INT32 msg_size;
	INT32 status;
	INT32 free_flag;
	
	SemaType  send_sem;
	SemaType  receive_sem;
	SemaType  empty_sem;
	SemaType  full_sem;
	
} MsgqLst;


typedef struct {
	NodeType ln;
	char *msg_p;
} MsgqNode;

typedef MsgqLst* MsgqType;

/* Macros ------------------------------------------------------------------- */

/* Globals ------------------------------------------------------------------ */
extern MsgqType g_msgq_id[MSG_QUEUE_NUM + 1];

/* Functions ---------------------------------------------------------------- */
MsgqType create_msgq(INT32, INT32);
INT32 delete_msgq(MsgqType );
INT32 send_msgq(MsgqType , char *, UINT32 ,INT32 , INT32 );
INT32 receive_msgq(MsgqType, char *,UINT32, INT32 );
INT32 get_msgq_num(MsgqType);
INT32 cleanup_msgqs(void);
#else
/* Dependencies ------------------------------------------------------------- */


#include "lte_list.h"
#include "lte_sema.h"
#include "lte_malloc.h"


/* Constants ---------------------------------------------------------------- */
#define MQ_PRIO_MAX 32
#define MSGQ_STATUS_NORMAL 0
#define MSGQ_STATUS_DELETE -1
#define MSGQ_STATUS_FULL -2
#define MSGQ_STATUS_EMPTY -3
#define MSGQ_MSG_SIZE 4

/* Types -------------------------------------------------------------------- */
typedef struct {
	ListType lh;
	INT32 max_num;
	INT32 msg_size;
	INT32 status;
	INT32 free_flag;
	
	SemaType send_sem;
	SemaType receive_sem;
	SemaType empty_sem;
	SemaType full_sem;
	
} MsgqLst;


typedef struct {
	NodeType ln;
	char *msg_p;
} MsgqNode;

typedef MsgqLst* MsgqType;

/* Macros ------------------------------------------------------------------- */

/* Globals ------------------------------------------------------------------ */
extern MsgqType g_msgq_id[MSG_QUEUE_NUM + 1];


/* Functions ---------------------------------------------------------------- */
MsgqType create_msgq(INT32 max_msg_num, INT32 max_msg_len);
INT32 delete_msgq(MsgqType );
INT32 send_msgq(MsgqType , char *, UINT32 ,INT32 , INT32 );
INT32 receive_msgq(MsgqType, char *,UINT32, INT32 );
INT32 get_msgq_num(MsgqType);
INT32 cleanup_msgqs(void);




#endif
#endif
#endif	/* MSGQ_H */
