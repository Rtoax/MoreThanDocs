/*******************************************************************************
 ** 
 ** Copyright (c) 2006-2010 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: lte_sema.c
 ** Description: Source file for semaphore functions.
 **
 ** Current Version: 1.2
 ** $Revision: 1.1.1.1 $
 ** Author:Zhang zidi (zhangzidi@ict.ac.cn)
 ** Date: 2009.07.17
 **
 ******************************************************************************/
#include "lte_sema.h"
#include "lte_malloc.h"


/* Dependencies ------------------------------------------------------------- */

/* Constants ---------------------------------------------------------------- */

/* Types -------------------------------------------------------------------- */

/* Macros ------------------------------------------------------------------- */

/* Globals ------------------------------------------------------------------ */

/* Functions ---------------------------------------------------------------- */

/*******************************************************************************
 * Initialize binary semaphore.
 *
 * Input: options: SEM_FULL, or SEM_EMPTY.
 *
 * Output: return semaphore created,
 *                NULL, if create failed.
 ******************************************************************************/
SemaType create_semb(INT32 options)
{

    SemaType sem_p;


    if (!(sem_p = (SemaType)lte_malloc(sizeof(sem_t)))) {
        return NULL;
    }
    if (sem_init(sem_p, 0, options)) {
        lte_free(sem_p);
        return NULL;
    }
    return sem_p;
}

/*******************************************************************************
 * Initialize count semaphore.
 *
 * Input: init_count: initial count.
 *
 * Output: return semaphore created.
 *                NULL, if create failed.
 ******************************************************************************/

SemaType create_semc(INT32 init_count)
{

    SemaType sem_p;

    if (!(sem_p = (SemaType)lte_malloc(sizeof(sem_t)))) {
        return NULL;
    }
    if (sem_init(sem_p, 0, init_count)) {
        lte_free(sem_p);
        return NULL;
    }


    return sem_p;
}

/*******************************************************************************
 * Delete semaphore.
 *
 * Input: sem_p: pointer to semaphore to be deleted.
 *
 * Output: return 0, or -1 if error occurs.
 ******************************************************************************/

INT32 delete_sem(SemaType sem_p)
{
    INT32 ret = 0;

    ret = sem_destroy(sem_p);
    lte_free(sem_p);
    return ret;
}

/*******************************************************************************
 * Wait on semaphore.
 *
 * Input: sem_p: pointer to semaphore.
 *        timeout: waiting time value, may be WAIT_FOREVER, NO_WAIT,
 *                 or other values in unit of usec.
 *                 
 * Output: return 0, or -1 if error occurs, or -2 if interruptible return.
 ******************************************************************************/
INT32 take_sem(SemaType sem_p, INT32 timeout)
{

    INT32 poll_time;

    switch (timeout) {
        case WAIT_FOREVER:
            return sem_wait(sem_p);
        case NO_WAIT:
            return sem_trywait(sem_p);
        default:
            if (timeout < 0) {
                return -1;
            }

            while (sem_trywait(sem_p) !=0) {
                if (timeout < 0) {
                    return -1;
                }
                if (timeout > SEM_WAIT_POLL_TIME) {
                    poll_time = SEM_WAIT_POLL_TIME;
                    timeout -= SEM_WAIT_POLL_TIME;
                } else {
                    poll_time = timeout;
                    timeout = -1;
                }
                usleep(poll_time);
            }
            return 0;
	}
}

/*******************************************************************************
 * Signal a semaphore.
 *
 * Input: sem_p: pointer to semaphore.
 * 
 * Output: return 0, or -1 if error occurs.
 ******************************************************************************/
INT32 give_sem(SemaType sem_p)
{
	return sem_post(sem_p);
}
/*******************************************************************************
 * Get the value of the semaphore.
 *
 * Input: sem_p: pointer to semaphore.
 *
 * Output:
 * 	int: the number
 ******************************************************************************/
INT32 get_sem_value(SemaType sem_p) {
	INT32 value = 0;
	sem_getvalue(sem_p, &value);
	return value;
}
