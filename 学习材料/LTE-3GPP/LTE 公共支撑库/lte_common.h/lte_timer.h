/*******************************************************************************
 ** 
 ** Copyright (c) 2006-2010 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: td_timer.h
 ** Description: Header file for wrapping timer functions.
 **
 ** Current Version: 1.3
 ** $Revision: 1.1 $
 ** Author:  Huli(huli@ict.ac.cn)
 ** Date: 2009.07.16
 ** 
 ** Old Version: 1.2
 ** $Revision: 1.1 $
 ** Author: Jihua Zhou (jhzhou@ict.ac.cn)
 ** Date: 2007.04.12
 **
 ** Old Version: 1.1
 ** Author: Jihua Zhou (jhzhou@ict.ac.cn)
 ** Date: 2006.04.07
 **
 ** Old Version: 1.0
 ** Author: Jihua Zhou (jhzhou@ict.ac.cn)
 ** Date: 2006.01.09
 **
 ******************************************************************************/

#ifndef TIMER_H
#define TIMER_H
/* Dependencies ------------------------------------------------------------- */
#include "lte_type.h"
#include <sys/time.h>
#include <stdio.h>
#include <signal.h>
#include <time.h>
#include <pthread.h>
#include <string.h>

/* Types -------------------------------------------------------------------- */

#ifndef CLOCK_REALTIME
#define CLOCK_REALTIME	0
#endif
typedef union {
	void *arg_p;
	int arg;
} LteTimerArg;
typedef void (*TIMERFUNC)(LteTimerArg);	/* ptr to timer function */

typedef timer_t LteTimer;

enum {
	TIME_UNIT_US,
	TIME_UNIT_MS,
	TIME_UNIT_S
};
/* Macros ------------------------------------------------------------------- */

#ifdef MAC_SCHED_INFO_STATISTIC
/* calculate func elapsed time */
#define CALC_ELAPSED_TIME(func)			\
({										\
    struct timeval start, end;			\
	INT64 diff;							\
    gettimeofday(&start,0);				\
    func;								\
    gettimeofday(&end,0);				\
    diff = ( end.tv_sec*1000000L + end.tv_usec ) 		\
        - ( start.tv_sec*1000000L + start.tv_usec );	\
})
#else

#define CALC_ELAPSED_TIME(func)			\
({										\
    func;0;								\
})

#endif

#define TIMEVAL_TV_USEC_SIZE 4


#define create_timer_us(timerid_p, func, arg, expires, reload) \
		(create_timer_internal(timerid_p, TIME_UNIT_US, func, \
				(LteTimerArg)(arg), expires, reload))

#define create_timer_ms(timerid_p, func, arg, expires, reload) \
		(create_timer_internal(timerid_p, TIME_UNIT_MS, func, \
				(LteTimerArg)(arg), expires, reload))

#define create_timer_s(timerid_p, func, arg, expires, reload) \
		(create_timer_internal(timerid_p, TIME_UNIT_S, func, \
				(LteTimerArg)(arg), expires, reload))

#define create_timer(timerid_p, func, arg, expires, reload) \
		(create_timer_internal(timerid_p, TIME_UNIT_MS, func, \
				(LteTimerArg)(arg), expires, reload))
#define create_rlc_timer(timerid_p, func, arg, expires, reload) \
		(create_rlc_timer_internal(timerid_p, TIME_UNIT_MS, func, \
				(LteTimerArg)(arg), expires, reload))


#define set_timer_time_us(timerid, expires, reload) \
		set_timer_time_internal(timerid, TIME_UNIT_US, expires, reload)

#define set_timer_time_ms(timerid, expires, reload) \
		set_timer_time_internal(timerid, TIME_UNIT_MS, expires, reload)

#define set_timer_time_s(timerid, expires, reload) \
		set_timer_time_internal(timerid, TIME_UNIT_S, expires, reload)

#define set_timer_time(timerid, expires, reload) \
		set_timer_time_internal(timerid, TIME_UNIT_MS, expires, reload)


#define get_timer_time_us(timerid, expires, reload) \
		get_timer_time_internal(timerid, TIME_UNIT_US, expires, reload)

#define get_timer_time_ms(timerid, expires, reload) \
		get_timer_time_internal(timerid, TIME_UNIT_MS, expires, reload)

#define get_timer_time_s(timerid, expires, reload) \
		get_timer_time_internal(timerid, TIME_UNIT_S, expires, reload)

#define get_timer_time(timerid, expires, reload) \
		get_timer_time_internal(timerid, TIME_UNIT_MS, expires, reload)

/* Globals ------------------------------------------------------------------ */


/* Fuctions ------------------------------------------------------------------ */


/*******************************************************************************
 * Create a timer and add it to kernel timer list.
 *
 * Input: timerid_p: pointer to timerid to be created.
 *        time_unit: unit of expires/reload;
 *                   TIME_UNIT_US: in unit of microsecond,
 *                   TIME_UNIT_MS: in unit of millisecond,
 *                   TIME_UNIT_S:  in unit of second.
 *        func: user routine;
 *              func should be declared as:
 *                  void func(timer_t timer_id, LteTimerArg arg);
 *        arg: user argument;
 *        expires: expire time, relative to current time.
 *        reload: reload time, interval value.
 *
 * Output: return 0, if success.
 *                -1, if the call fails.
 ******************************************************************************/
extern INT32 create_timer_internal(LteTimer *timerid_p, INT32 time_unit,
                                		  TIMERFUNC func, LteTimerArg arg,
                                		  UINT32 expires, UINT32 reload);

/*******************************************************************************
 * Modify expiration and reload time of a timer.
 *
 * Input: timerid: ID of timer to be modified.
 *        time_unit: unit of expires/reload;
 *                   TIME_UNIT_US: in unit of microsecond,
 *                   TIME_UNIT_MS: in unit of millisecond,
 *                   TIME_UNIT_S:  in unit of second.
 *        expires: expire time, in unit of relative to current time.
 *                              0, don't modify expiration time.
 *        reload: reload time, interval value.
 *
 * Output: return 0, if success,
 *                -1, if the call fails.
 ******************************************************************************/
extern INT32 set_timer_time_internal(timer_t timerid, INT32 time_unit,
											UINT32 expires, UINT32 reload);

/*******************************************************************************
 * Get remaining expiration time and reload time of a timer.
 *
 * Input: timerid: ID of timer which time to be gotton.
 *        time_unit: unit of expires/reload;
 *                   TIME_UNIT_US: in unit of microsecond,
 *                   TIME_UNIT_MS: in unit of millisecond,
 *                   TIME_UNIT_S:  in unit of second.
 *
 * Output: expires: remaining expire time, relative to current time.
 *                  value 0, the timer has expired.
 *                  NULL, do not need this value.
 *         reload: reload time, interval value.
 *         			NULL, do not need this value.
 *         
 *         return 0, if success,
 *                -1, if the call fails.
 ******************************************************************************/
extern INT32 get_timer_time_internal(timer_t timerid, INT32 time_unit,
											UINT32 *expires, UINT32 *reload);

/*******************************************************************************
 * Cancel a timer.
 * This function stops a timer, but not delete it.
 * A canceled timer can be actived again by calling set_timer_time().
 *
 * Input: timerid: ID of timer to be canceled.
 *
 * Output: return 0, if success,
 *                -1, if the call fails.
 ******************************************************************************/
extern INT32 cancel_timer(timer_t timerid);

/*******************************************************************************
 * Delete a timer.
 * A deleted timer can not be used again.
 *
 * Input: timerid: ID of timer to be deleted.
 *
 * Output: return 0, if success,
 *                -1, if the call fails.
 ******************************************************************************/
extern INT32 delete_timer(timer_t timerid);
/*******************************************************************************
 * Get the argument of trigger functin of a timer.
 *
 * Input: timerid: ID of the timer.
 *
 * Output: arg_p: pointer to argument of trigger function.
 *         return 0, if success,
 *                -1, if the call fails.
 ******************************************************************************/
//extern INT32 get_timer_arg(timer_t timerid, INT32 *arg_p);

#endif	/*TIMER_H*/
