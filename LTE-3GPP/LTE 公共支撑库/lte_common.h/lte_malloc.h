/******************************************************************************
 ** Copyright (c) 2005-2008 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: test_mallc.h
 ** Description: Header file for test malloc functions.
 **
 ** Current Version: 1.0
 ** Author: Yuxin Yang (yangyuxin@ict.ac.cn)
 ** Date: 2006.10.29
 **
 ******************************************************************************/

#ifndef LTE_MALLOC_H
#define LTE_MALLOC_H

/* Dependencies ------------------------------------------------------------- */
#include <stdlib.h>
#include <pthread.h>
#include "lte_list.h"
/* Constants ---------------------------------------------------------------- */
#define DP_PDU_HEAD_OFFSET 100			/* user plane : rlc head/ pdcp head or gtpu head */
#define GTPU_MAC_PACKET_SIZE 10000		/* gptu max packet size */
#define EVERY_MEM_SIZE GTPU_MAC_PACKET_SIZE + DP_PDU_HEAD_OFFSET
#define MEM_BUFF_NUM 20000
/* Types -------------------------------------------------------------------- */
typedef struct {
	NodeType ln;  
	int size; 	 
	int total;
	int line;
	int malloc_count;
}MemoInfoNode;

typedef struct {
	NodeType ln;  
	int line;
}FreeNode;

typedef struct{
	UINT16	no;
	UINT16	len; // all len in the membuff, offset in the system
	UINT16 offset;
	UINT16 flag;
	UINT8 *msg_p;
}MemBuff;

/* Macros ------------------------------------------------------------------- */
#ifdef TEST_MALLOC
#define lte_malloc(size) test_malloc(size, __FILE__, __LINE__)
#define lte_free(mem_p) \
		do { \
			test_free(mem_p, __FILE__, __LINE__);\
			mem_p = NULL; \
		} while(0)
#else
#define lte_malloc(size) malloc(size)
#define lte_free(mem_p) \
		do {\
			free(mem_p);\
			mem_p = NULL;\
		} while(0)
#endif

/* Globals ------------------------------------------------------------------ */

/* Functions ---------------------------------------------------------------- */

/*******************************************************************************
 * Replace the function of kmalloc(size)
 *
 * Input: size: the size of the memory which user want to malloc
 *        __FILE__: sys Macro to record name of the file in which malloc() had 
 *                  been used
 *        __LINE__: sys Macro to record the line number on which malloc() had 
 *                  been used
 *                     
 * Output: return void*, the memory address that user had malloced
 *                   -1, error in malloc()
 ******************************************************************************/
void *test_malloc(int size, char *pFile, int line);

/*******************************************************************************
 * Replace the function of kfree(mem_p)
 *
 * Input: mem_P: pointer to the address which should be released
 *
 * Output: return void* 
 ******************************************************************************/
void test_free(void *mem_p, char *pFile, int line);

/*******************************************************************************
 * print out the info which reflects the malloc/free used 
 * the message has been written into a .txt doc
 *
 * Input: mem_p: 
 *
 * Output: return 0, successfully done
 * 		 -1, error in print_mem_info
 ******************************************************************************/
int print_mem_info(void);

/*******************************************************************************
 * init lte_malloc mod.
 *
 * Input: 
 *
 * Output: 
 ******************************************************************************/
INT32 malloc_init();

/*******************************************************************************
 * cleanup lte_malloc mod.
 *
 * Input: 
 *
 * Output: 
 ******************************************************************************/
INT32 malloc_cleanup();

INT32 init_mem_buff();

INT32 clean_mem_buff();

INT32 malloc_mem_buff(UINT32 msg_len, UINT32 offset, MemBuff *mem_buff_p) ;

INT32 copy_mem_buff(MemBuff *des_buff_p, MemBuff *source_buff_p, UINT32 msg_len, UINT32 offset) ;

INT32 free_mem_buff(MemBuff *mem_buff_p) ;

#endif
