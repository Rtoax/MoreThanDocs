/*******************************************************************************
 ** 
 ** Copyright (c) 2006-2010 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: lte_sema.h
 ** Description: Header file for sema functions.
 **
 ** Old Version: 1.0
 ** Author: Jihua Zhou (jhzhou@ict.ac.cn)
 ** Date: 2006.01.06
 **
 ******************************************************************************/


#ifndef SEMA_H
#define SEMA_H

/* Dependencies ------------------------------------------------------------- */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include "lte_type.h"

/* Constants ---------------------------------------------------------------- */
#define SEM_FULL 1
#define SEM_EMPTY 0
#define WAIT_FOREVER -1
#define NO_WAIT 0
#define SEM_WAIT_POLL_TIME 1000

/* Types -------------------------------------------------------------------- */
typedef sem_t *SemaType;

/* Macros ------------------------------------------------------------------- */

/* Globals ------------------------------------------------------------------ */

/* Functions ---------------------------------------------------------------- */
SemaType create_semb(INT32 options);

SemaType create_semc(INT32 init_count);


INT32 delete_sem(SemaType sem_p);

INT32 take_sem(SemaType sem_p, INT32 timeout);

INT32 give_sem(SemaType sem_p);

INT32 get_sem_value(SemaType sem_p);
#endif

