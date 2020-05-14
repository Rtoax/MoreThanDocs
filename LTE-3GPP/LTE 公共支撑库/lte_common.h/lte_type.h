/*******************************************************************************
 ** 
 ** Copyright (c) 2006-2010 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: lte_type.h
 ** Description: Header file for types definition.
 **
 ** Current Version: 1.0
 ** Author: Jihua Zhou (jhzhou@ict.ac.cn)
 ** Date: 2006.01.04
 **
 ******************************************************************************/

#ifndef LTE_TYPE_H
#define LTE_TYPE_H
#include <stdio.h>
#include <stdint.h>


/* Dependencies ------------------------------------------------------------- */
#define	ERR_NO_MEM		-2000
#define	ERR_FUNC_PARAM	-1999
#define	ERR_SYS_PARAM	-1998
#define	ERR_BIT_PACK	-1997

#define	NUM_ZERO		0
#define	NUM_ONE			1

/* Constants ---------------------------------------------------------------- */
#define FALSE 0
#define TRUE 1


/* Types -------------------------------------------------------------------- */
typedef int8_t    INT8;
typedef int16_t   INT16;
typedef int32_t   INT32;
typedef int64_t   INT64;
typedef uint8_t   UINT8;
typedef uint16_t  UINT16;
typedef uint32_t  UINT32;
typedef uint64_t  UINT64;
typedef UINT8     BOOL;

typedef void *(* FUNCPTR)(void *args);

/* Macros ------------------------------------------------------------------- */

/* Globals ------------------------------------------------------------------ */

/* Functions ---------------------------------------------------------------- */

static inline void show_memory(UINT8 *msg_p, INT32 len, char *name)
{
	int i;

	if (msg_p == NULL) {
        printf("The pointer is NULL in show_memory()\n");
		return;
	}
	
	if (name != NULL) {
        printf("The memory \"%s\" len is: %d\n", name, len);
	} else {
        printf("The memory len is %d:\n", len);
	}

	for (i = 0; i < len; i++) {
        printf("%02x ", *(UINT8 *)(msg_p + i));
		if ((i + 1) % 24 == 0) {
            printf("\n");
		}
	}
    printf("\n");
}


#endif  /* LTE_TYPE_H */
