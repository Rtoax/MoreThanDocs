/*******************************************************************************
 ** 
 ** Copyright (c) 2005-2010 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: lte_log.h
 ** Description: A lot of useful debug utilities.
 **
 ** Current Version: 0.0
 ** Revision: 0.0.0.0
 ** Author: Jia Baolei jiabaolei@ict.ac.cn
 ** Date: 2010-12-20
 **
 ******************************************************************************/

#ifndef LTE_LOG_H
#define LTE_LOG_H

/* Dependencies ------------------------------------------------------------- */
#include <pthread.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>
#include "lte_type.h"
/* Constants ---------------------------------------------------------------- */


/* Types -------------------------------------------------------------------- */

/* describe the mode this module will be working in*/
enum {
	WRITE2STDOUT = 0, /* write the output to stdout*/
	WRITE2FILE,/* write the output to file*/
	WRITE2WIRESHARK /* write the output to wireshark through socket */
};

/* debug level*/
enum {
	LOG_ERR = 0,/* used when fatal error occurs*/
	LOG_WARNING, /* used when un-nomarl */
	LOG_SUMMARY,    /* summary info,Normal, but significant events */
    LOG_INFO  /* other debug information, mem buf and so on*/
};

enum {
	LOG_SIMPLE,
	LOG_COMPLETE
};

/* module names, users can output the specific module debug info*/
/* while omit debug info of other modules', by specify the module*/
/* name */
enum {
	COMMON,
	MAC_RX,
	MAC_TX,
	MAC_HARQ,
	MAC_RA,
	MAC_CONS,
	MAC_ULSCHED,
	MAC_DLSCHED,
	MAC_SCHEDULER,
	MAC_MGMT,
	RLC_RX,
	RLC_TX,
	RLC_MGMT,
	RLC_AM,
	RLC_UM,
	RLC_TM,
	PDCP,
	AS_SEC,
	RRC_CONN,
	RRC_REESTAB,
	RRC_RELEASE,
	RRC_RECFG,
	RRC_CORE,
	RRC_TX,
	COUNTER_CHECK,
	SI_MGMT,
	HENB_CORE,
	HENB_ASN,
	HENB_TNL,
	GTPU,
	S1AP,
	X2AP,
	RRM_SM,
	RRM,
	TRACE,
	L1API,
	MAIN,
	FSL_L1_IPC,
	LOG_MAX_MODULE_NAME
};

#ifdef PC9608_CL1
extern void post_debug_msg_to_wq(uint64_t lineno,
        const char *func,const char* format, ...);
#define log_msg(level,module, _format, ...) ({ \
    if (level <= get_log_level()) {\
        post_debug_msg_to_wq(__LINE__, __func__,_format, ##__VA_ARGS__);\
    }\
})

#else
/* Macros ------------------------------------------------------------------- */
#define log_msg(level,module, fmt, arg...) \
		log_msg_i(level,module, #module, __FILE__, __LINE__, __func__, fmt, ##arg)
#endif


#define  HENB_INIT(statement)  \
	do { \
		if (statement) {    \
			log_msg(LOG_ERR, RRM, #statement " failed\n"); \
			perror(#statement); \
			exit(-1);  \
		} else log_msg(LOG_SUMMARY, RRM, #statement " success\n"); \
	} while (0)


#define LTE_ASSERT(cond)    \
	if (!(cond)) { \
		log_msg(LOG_ERR, TRACE, "ASSERT condition (" #cond ") failed!\n");\
		assert(0); \
		*(char *)0 = 0; /*Forcefully cause a Crash*/\
	}
		

		
#define PRINT_BUFF(buf,size)	\
	do{							\
		UINT32 i = 0;			\
		log_msg(LOG_SUMMARY, TRACE , "The memory len:%d!\n", size); \
		for (; i < size; ++i)										\
			printf("%02x", (unsigned char) buf[i]);					\
		printf("\n");												\
	}while(0)


/* Globals ------------------------------------------------------------------ */

/* Functions ---------------------------------------------------------------- */

/*******************************************************************************
* init the log module 
* Input:
*		style: WRITE2FILE or WRITE2STDOUT
*		current_level: LOG_ERR, LOG_WARRNING or LOG_INFO, indicating the lowest
*					level that the module will excute
* Output:
*		None;
* Return:
*		0:success;
*		-1:failed
 ******************************************************************************/
INT32 log_init(UINT8 style, UINT8 format, UINT8 current_level, INT8 *log_ip,UINT32 log_port);
/*******************************************************************************
* cleanup the log module ,release the resource
* Input:
*		None
* Output:
*		None;
* Return:
*		0:success;
*		-1:failed
 ******************************************************************************/
INT32 log_cleanup();
/*******************************************************************************
* turn on the log print of one module 
* Input:
*		module :name of module
* Output:
*		None;
* Return:
*		None;
 ******************************************************************************/
void log_open(UINT8 module); 
/*******************************************************************************
* turn off the log print of one module 
* Input:
*		module :name of module
* Output:
*		None;
* Return:
*		None;
 ******************************************************************************/
void log_close(UINT8 module); 

/*******************************************************************************
* output the debug information, this function won't be used directly by users
* Input:
*		level: the output level
*		module: module name user belong
*		*fmt: varable argument
*		...: optional argument
* Output:
*		None;
* Return:
*		0:success;
*		-1:failed
 ******************************************************************************/
INT32 log_msg_i(UINT8 level, UINT8 module, const char *mdstr, const char *file, INT32 lineno,
					const char *func, const char *fmt,...);

#endif /* LTE_LOG_H */
