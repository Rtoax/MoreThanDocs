/*******************************************************************************
 ** 
 ** Copyright (c) 2006-2010 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: lte_lock.c
 ** Description: Source file for lte_lock functions.
 **
 ** Current Version: 1.0
 ** Author: WeiKun (weikun@ict.ac.cn)
 ** Date: 2011.08.15
 **
*******************************************************************************/
#ifdef VXWORKS
/* Dependencies ------------------------------------------------------------- */

/* Constants ---------------------------------------------------------------- */

/* Types -------------------------------------------------------------------- */

/* Macros ------------------------------------------------------------------- */

/* Globals ------------------------------------------------------------------ */

/* Functions ---------------------------------------------------------------- */

#elif defined(LINUX)
#ifdef __KERNEL__
/* Dependencies ------------------------------------------------------------- */
#include "lte_lock.h"

/* Constants ---------------------------------------------------------------- */

/* Types -------------------------------------------------------------------- */

/* Macros ------------------------------------------------------------------- */

/* Globals ------------------------------------------------------------------ */

/* Functions ---------------------------------------------------------------- */
/*******************************************************************************
 * Create the locker.
 *
 * Input: 
 *        None.
 * Output: 
 ******************************************************************************/
LteLock create_lock()
{
	spinlock_t *spinlock_p = malloc(sizeof(spinlock_t));
	if (lock) {
		return -1;
	}
	return spinlock_p;
}
/*******************************************************************************
 * Delete the locker.
 *
 * Input: lock_p: pointer to the lock to be locked.
 *        
 * Output: 
 ******************************************************************************/
INT32 delete_lock(LteLock lock)
{
	if (!lock) {
		return -1;
	}
	free(lock);
	return 0;
}
/*******************************************************************************
 * Lock the locker so that other users can't use the resource.
 *
 * Input: lock_p: pointer to the lock to be locked.
 *        
 * Output: 
 ******************************************************************************/
INT32 lte_lock(LteLock lock)
{
	if (!lock) {
		return -1;
	}
	spin_lock(lock);
	return 0;
}

/*******************************************************************************
 * Unlock the locker so that other users can use the resource.
 *
 * Input: lock_p: pointer to the lock to be locked.
 *        
 * Output: 
 ******************************************************************************/
INT32 lte_unlock(LteLock lock)
{
	if (!lock) {
		return -1;
	}
	spin_unlock(lock);
	return 0;
}

#else 	/*not KERNEL*/
/* Dependencies ------------------------------------------------------------- */
#include "lte_lock.h"

/* Constants ---------------------------------------------------------------- */
pthread_mutex_t g_mutexlock = PTHREAD_MUTEX_INITIALIZER;

/* Types -------------------------------------------------------------------- */

/* Macros ------------------------------------------------------------------- */

/* Globals ------------------------------------------------------------------ */

/* Functions ---------------------------------------------------------------- */
/*******************************************************************************
 * Create the locker.
 *
 * Input: 
 *        None.
 * Output: 
 ******************************************************************************/
LteLock create_lock()
{
	pthread_mutex_t *mutexlock_p = NULL;
	/*pthread_mutexattr_t mutexattr;   init_type 2*/
	mutexlock_p = (pthread_mutex_t *)lte_malloc(sizeof(pthread_mutex_t));
	(*mutexlock_p) = g_mutexlock;
	/*
		pthread_mutexattr_init(&attr);
		pthread_mutexattr_settype(&attr,PTHREAD_MUTEX_RECURSIVE_NP);
		pthread_mutex_init(&mutex,&attr);  
		init_type 2*/
	/*pthread_mutex_init(&mutexlock_p,NULL);	init_type 3*/
	return mutexlock_p;
}
/*******************************************************************************
 * Delete the locker.
 *
 * Input: lock_p: pointer to the lock to be locked.
 *        
 * Output: 
 ******************************************************************************/
INT32 delete_lock(LteLock lock)
{
	if (!lock) {
		return -1;
	}
	pthread_mutex_destroy(lock);
	lte_free(lock);
	return 0;
}
/*******************************************************************************
 * Lock the locker so that other users can't use the resource.
 *
 * Input: lock_p: pointer to the lock to be locked.
 *        
 * Output: 
 ******************************************************************************/
INT32 lte_lock(LteLock lock)
{
	if (!lock) {
		return -1;
	}
	pthread_mutex_lock(lock);
	return 0;
}

/*******************************************************************************
 * Unlock the locker so that other users can use the resource.
 *
 * Input: lock_p: pointer to the lock to be locked.
 *        
 * Output: 
 ******************************************************************************/
INT32 lte_unlock(LteLock lock)
{
	if (!lock) {
		return -1;
	}
	pthread_mutex_unlock(lock);
	return 0;
}

#endif /*__KERNEL__*/
#elif defined(RTAI)
/* Dependencies ------------------------------------------------------------- */

/* Constants ---------------------------------------------------------------- */

/* Types -------------------------------------------------------------------- */

/* Macros ------------------------------------------------------------------- */

/* Globals ------------------------------------------------------------------ */

/* Functions ---------------------------------------------------------------- */


#endif	/*OS*/
