/*******************************************************************************
 ** 
 ** Copyright (c) 2006-2010 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: mac_timer.c
 ** Description: Source file for wrapping timer functions.
 **
 ** Current Version: 1.0
 ** $Revision: 1.1 $
 ** Author: Jihua Zhou (jhzhou@ict.ac.cn)
 ** Date: 2006.01.09
 **
 ******************************************************************************/
 
 
/* Dependencies ------------------------------------------------------------- */
#include "lte_timer.h"
#include "lte_log.h"


/* Types -------------------------------------------------------------------- */


/* Macros ------------------------------------------------------------------- */
#define GLIBC_TIMER_BUG_FIXED
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
 *                  void func(timer_t timer_id, INT32 arg);
 *        arg: user argument;
 *        expires: expire time, relative to current time.
 *        reload: reload time, interval value.
 *
 * Output: return 0, if success.
 *                -1, if the call fails.
 ******************************************************************************/
INT32 create_timer_internal(LteTimer *timerid_p, INT32 time_unit,
                                		  TIMERFUNC func, LteTimerArg arg,
                                		  UINT32 expires, UINT32 reload)
{
	struct sigevent se;
 	struct itimerspec ts;
	UINT32 exp_s, exp_ns, rel_s, rel_ns;
	

	memset(&se, 0, sizeof (struct sigevent));
	memset(&ts, 0, sizeof (struct itimerspec));	
	se.sigev_notify = SIGEV_THREAD;
	se.sigev_notify_function = (void (*)(union sigval))func;
	se.sigev_value.sival_int = arg.arg;
	se.sigev_value.sival_ptr = arg.arg_p;

	if (timer_create(CLOCK_REALTIME, &se, timerid_p) < 0) {
		perror ("timer_creat  failure");
		return -1;
	}

	if ((time_unit < TIME_UNIT_US) || (time_unit > TIME_UNIT_S)) {
		time_unit = TIME_UNIT_US;
	}
	if (time_unit == TIME_UNIT_US) {
		exp_s = expires / 1000000;	/* in unit of second */
		exp_ns = (expires % 1000000) * 1000; /* in unit of ns */
		rel_s = reload / 1000000;
		rel_ns = (reload % 1000000) * 1000;
	} else if (time_unit == TIME_UNIT_MS) {
		exp_s = expires / 1000;
		exp_ns = (expires % 1000) * 1000000;
		rel_s = reload / 1000;
		rel_ns = (reload % 1000) * 1000000;
	} else {
		exp_s = expires;
		rel_s = reload;
	}

	ts.it_value.tv_sec = exp_s;
   	ts.it_value.tv_nsec =  exp_ns;
	ts.it_interval.tv_sec = rel_s;
	ts.it_interval.tv_nsec = rel_ns;
	if (timer_settime (*timerid_p, TIMER_ABSTIME, &ts, NULL) < 0) {
		perror ("timer_settime");
		return -1;
	}
	return 0;
}

INT32 create_rlc_timer_internal(LteTimer *timerid_p, INT32 time_unit,
                                		  TIMERFUNC func, LteTimerArg arg,
                                		  UINT32 expires, UINT32 reload)
{
	struct sigevent se;
 	struct itimerspec ts;
	UINT32 exp_s, exp_ns, rel_s, rel_ns;


	memset(&se, 0, sizeof (struct sigevent));
	memset(&ts, 0, sizeof (struct itimerspec));
	se.sigev_notify = SIGEV_THREAD;
	se.sigev_notify_function = (void (*)(union sigval))func;
	se.sigev_value.sival_int = arg.arg;
	se.sigev_value.sival_ptr = arg.arg_p;

	if (timer_create(CLOCK_REALTIME, &se, timerid_p) < 0) {
		perror ("timer_creat  failure");
		return -1;
	}

	if ((time_unit < TIME_UNIT_US) || (time_unit > TIME_UNIT_S)) {
		time_unit = TIME_UNIT_US;
	}
	if (time_unit == TIME_UNIT_US) {
		exp_s = expires / 1000000;	/* in unit of second */
		exp_ns = (expires % 1000000) * 1000; /* in unit of ns */
		rel_s = reload / 1000000;
		rel_ns = (reload % 1000000) * 1000;
	} else if (time_unit == TIME_UNIT_MS) {
		exp_s = expires / 1000;
		exp_ns = (expires % 1000) * 1000000;
		rel_s = reload / 1000;
		rel_ns = (reload % 1000) * 1000000;
	} else {
		exp_s = expires;
		rel_s = reload;
	}

	ts.it_value.tv_sec = exp_s;
   	ts.it_value.tv_nsec =  exp_ns;
	ts.it_interval.tv_sec = rel_s;
	ts.it_interval.tv_nsec = rel_ns;
//	if (timer_settime (*timerid_p, TIMER_ABSTIME, &ts, NULL) < 0) {
//		perror ("timer_settime");
//		return -1;
//	}
	return 0;
}
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
INT32 set_timer_time_internal(timer_t timerid, INT32 time_unit, 
											UINT32 expires, UINT32 reload)
{
	struct itimerspec  timer_spec;
	UINT32 exp_s, exp_ns, rel_s,  rel_ns;
	if (!timerid) {
		return -1;
	}
	if ((time_unit < TIME_UNIT_US) || (time_unit > TIME_UNIT_S)) {
		time_unit = TIME_UNIT_US;
	}
	if (time_unit == TIME_UNIT_US) {
		exp_s = expires / 1000000;	/* in unit of second */
		exp_ns = (expires % 1000000) * 1000; /* in unit of ns */
		rel_s = reload / 1000000;
		rel_ns = (reload % 1000000) * 1000;
	} else if (time_unit == TIME_UNIT_MS) {
		exp_s = expires / 1000;
		exp_ns = (expires % 1000) * 1000000;
		rel_s = reload / 1000;
		rel_ns = (reload % 1000) * 1000000;
	} else {
		exp_s = expires; 
		rel_s = reload;
	}

    timer_spec.it_value.tv_sec = exp_s;
	timer_spec.it_value.tv_nsec = exp_ns;
	timer_spec.it_interval.tv_sec = rel_s;
	timer_spec.it_interval.tv_nsec = rel_ns;

	 if (timer_settime (timerid, 0, &timer_spec, NULL) < 0)
       {
           perror ("timer_settime");
           return -1;
       }
	return 0;
	
}
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
INT32 get_timer_time_internal(timer_t timerid, INT32 time_unit,
											UINT32 *expires, UINT32 *reload)
{

	UINT32 exp, rel;
	struct itimerspec timer_spec;
	
	if (!timerid) {
		return -1;
	}
	if ((time_unit < TIME_UNIT_US) || (time_unit > TIME_UNIT_S)) {
		time_unit = TIME_UNIT_US;
	}
		
	if (timer_gettime(timerid, &timer_spec) ) {
		return -1;
	}

	if (expires)
	{
	if (time_unit == TIME_UNIT_US) {
			exp = timer_spec.it_value.tv_sec * 1000000 
				  + timer_spec.it_value.tv_nsec / 1000;
		} else if(time_unit == TIME_UNIT_MS) {
			exp = timer_spec.it_value.tv_sec * 1000 
				+ timer_spec.it_value.tv_nsec / 1000000;
		} else {
			exp = timer_spec.it_value.tv_sec 
				  + timer_spec.it_value.tv_nsec / 1000000000;
		}
		*expires = (UINT32)exp;
	}

	if (reload) {
		if (time_unit == TIME_UNIT_US) {
			rel = timer_spec.it_interval.tv_sec * 1000000
				  + timer_spec.it_interval.tv_nsec / 1000; 
		} else if(time_unit == TIME_UNIT_MS) {
			rel = timer_spec.it_interval.tv_sec * 1000
					+ timer_spec.it_interval.tv_nsec / 1000000;
		} else {
			rel = timer_spec.it_interval.tv_sec 
				  + timer_spec.it_interval.tv_nsec / 1000000000;
		}
		*reload = rel;
	}
	return 0;
}
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
INT32 cancel_timer(timer_t timerid)
{
	struct itimerspec  timer_spec;
	if (!timerid) {
		return -1;
	}
	memset(&timer_spec,0,sizeof(struct itimerspec));

	if (timer_settime (timerid, 0, &timer_spec, NULL) < 0) {
		perror ("cancel timer failure");
		return -1;
	}
	return 0;
}
/*******************************************************************************
 * Delete a timer.
 * A deleted timer can not be used again.
 *
 * Input: timerid: ID of timer to be deleted.
 *
 * Output: return 0, if success,
 *                -1, if the call fails.
 ******************************************************************************/
INT32 delete_timer(timer_t timerid)
{
	if (!timerid) {
		return -1;
	}
	/* Stop timer before delete it, prevent Segment fault under */
	/* some conditions, ex. rlc_delete_entity */
	cancel_timer(timerid);
#ifdef GLIBC_TIMER_BUG_FIXED
	return timer_delete(timerid);
#else
	/* Due to GLIBC bug, do not call timer_delete to delete timer temprarily */
	return 0;             
#endif
}


