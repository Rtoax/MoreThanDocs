/*******************************************************************************
 ** 
 ** Copyright (c) 2006-2010 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: lte_lock.h
 ** Description: Header file for lte_lock functions.
 **
 ** Current Version: 1.0
 ** Author: WeiKun (weikun@ict.ac.cn)
 ** Date: 2011.08.15
 **
*******************************************************************************/
#ifndef	LTE_LOCK_H
#define	LTE_LOCK_H
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
#include <linux/spinlock.h>
#include "lte_type.h"
#include "lte_malloc.h"

/* Constants ---------------------------------------------------------------- */

/* Types -------------------------------------------------------------------- */
typedef	struct spinlock_t* LteLock;

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
LteLock create_lock();
 
/*******************************************************************************
 * Delete the locker.
 *
 * Input: lock_p: pointer to the lock to be locked.
 *        
 * Output: 
 ******************************************************************************/
INT32 delete_lock(LteLock lock);

/*******************************************************************************
 * Lock the locker so that other users can't use the resource.
 *
 * Input: lock_p: pointer to the lock to be locked.
 *        
 * Output: 
 ******************************************************************************/
INT32 lte_lock(LteLock lock);

/*******************************************************************************
 * Unlock the locker so that other users can use the resource.
 *
 * Input: lock_p: pointer to the lock to be locked.
 *        
 * Output: 
 ******************************************************************************/
INT32 lte_unlock(LteLock lock);

#else 	/*!__KERNEL__*/
/* Dependencies ------------------------------------------------------------- */
#include <pthread.h>
#include "lte_type.h"
#include "lte_malloc.h"

/* Constants ---------------------------------------------------------------- */

/* Types -------------------------------------------------------------------- */
typedef	pthread_mutex_t* LteLock;

/* Macros ------------------------------------------------------------------- */
#define MUTEX_DEFINE(m)    pthread_mutex_t m
#define mutex_init(m)      pthread_mutex_init(m, NULL)
#define mutex_destroy(m)   pthread_mutex_destroy(m)
#define mutex_lock(m)      pthread_mutex_lock(m)
#define mutex_trylock(m)   pthread_mutex_trylock(m)
#define mutex_unlock(m)    pthread_mutex_unlock(m)

#define RWLOCK_DEFINE(m)    pthread_rwlock_t m
#define rwlock_init(m)      pthread_rwlock_init(m, NULL)
#define rwlock_destroy(m)   pthread_rwlock_destroy(m)
#define rwlock_rdlock(m)    pthread_rwlock_rdlock(m)
#define rwlock_tryrdlock(m) pthread_rwlock_tryrdlock(m)
#define rwlock_wrlock(m)    pthread_rwlock_wrlock(m)
#define rwlock_trywrlock(m) pthread_rwlock_trywrlock(m)
#define rwlock_unlock(m)    pthread_rwlock_unlock(m)
/* Globals ------------------------------------------------------------------ */

/* Functions ---------------------------------------------------------------- */
/*******************************************************************************
 * Create the locker.
 *
 * Input: 
 *        None.
 * Output: 
 ******************************************************************************/
LteLock create_lock();
 
/*******************************************************************************
 * Delete the locker.
 *
 * Input: lock_p: pointer to the lock to be locked.
 *        
 * Output: 
 ******************************************************************************/
INT32 delete_lock(LteLock lock);

/*******************************************************************************
 * Lock the locker so that other users can't use the resource.
 *
 * Input: lock_p: pointer to the lock to be locked.
 *        
 * Output: 
 ******************************************************************************/
INT32 lte_lock(LteLock lock);

/*******************************************************************************
 * Unlock the locker so that other users can use the resource.
 *
 * Input: lock_p: pointer to the lock to be locked.
 *        
 * Output: 
 ******************************************************************************/
INT32 lte_unlock(LteLock lock);

#endif /*__KERNEL__*/
#elif defined(RTAI)
/* Dependencies ------------------------------------------------------------- */

/* Constants ---------------------------------------------------------------- */

/* Types -------------------------------------------------------------------- */

/* Macros ------------------------------------------------------------------- */

/* Globals ------------------------------------------------------------------ */

/* Functions ---------------------------------------------------------------- */


#endif	/*OS*/
#endif	/*LTE_LOCK_H*/
