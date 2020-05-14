/**************************************************************************** 
 ** Copyright (c) 2005-2008 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: lte_malloc.c
 **
 ** Description: ...
 ** Replace the malloc, free, 
 ** and record the info about malloc&free has been used 
 **
 ** Current Version: 1.0
 **
 ** Author: Yuxin Yang (yangyuxin@ict.ac.cn)
 **
 ** Date: 06.10.29
*****************************************************************************/ 

/* Dependencies ------------------------------------------------------------- */
#include <memory.h>

#include "lte_malloc.h"
#include "lte_sema.h"
#include "lte_file.h"
#include "lte_type.h"
#include "lte_lock.h"

/* Constants ---------------------------------------------------------------- */

/* Types -------------------------------------------------------------------- */

/* Macros ------------------------------------------------------------------- */

/* Globals ------------------------------------------------------------------ */

ListType g_mem_alloc_free_info;
ListType g_mem_free_erro_info;
pthread_mutex_t g_malloc_mutexlock = PTHREAD_MUTEX_INITIALIZER;

int remain_count = 0;     /*  record the nodes still in the list after free   */
int free_remain = 0;
int my_malloc_used = 0;   /*  record the number that the malloc has been used */
int my_free_used = 0;     /*  record the number that the free has been used   */
static char *file_path = "mem_record.txt";

static MemBuff g_mem_buff[MEM_BUFF_NUM];
static SemaType g_mem_sema[MEM_BUFF_NUM];
static LteLock s_mem_lock;

/* Functions ---------------------------------------------------------------- */

/*******************************************************************************
 * Replace the function of malloc(size)
 *
 * Input:  size: the size of the memory which user want to malloc
 *        pFile: to record name of the file in which malloc() had 
 *                  been used
 *         line: to record the line number on which malloc() had 
 *                  been used
 *                     
 * Output: return void*, the memory address that user had malloced
 *                   -1, error in malloc()
 ******************************************************************************/
void *test_malloc(int size, char *pFile, int line)
{
	void *malloc_p = NULL; /*the address of memory malloc from system.*/
	int file_len = 1;	/* the length of the file-name */
	char* counter = pFile;/* temp counter to count the length of file-name */
    MemoInfoNode *MemoInfoNode_p = NULL;/* record the info of mem assigning */
    
    /*count the length of pFile.*/
	while (*counter != '\0') {
		counter++;
		file_len++;
	}
	pthread_mutex_lock(&g_malloc_mutexlock);
	
	/*malloc mem.*/
	malloc_p = malloc(sizeof(MemoInfoNode) + size + file_len);
	MemoInfoNode_p = (MemoInfoNode*)malloc_p;
//	memset(MemoInfoNode_p, 0, sizeof(MemoInfoNode));
	
	MemoInfoNode_p->total = sizeof(MemoInfoNode) + size + file_len;
	MemoInfoNode_p->size = size;
	MemoInfoNode_p->malloc_count++;
	
	MemoInfoNode_p->line = line;
	memcpy((UINT8 *)malloc_p + sizeof(MemoInfoNode) + size, pFile, file_len);
	
	add_list(&g_mem_alloc_free_info, (NodeType*)MemoInfoNode_p);
	my_malloc_used++;

	pthread_mutex_unlock(&g_malloc_mutexlock);
	return (malloc_p + sizeof(MemoInfoNode));
}

/*******************************************************************************
 * Replace the function of free(mem_p)
 *
 * Input: mem_P: pointer to the address which should be released
 *
 * Output: 
 ******************************************************************************/
void test_free(void *mem_p, char *pFile, int line)
{

	int file_len = 1; 	/* the length of the file-name */
	char* counter = NULL;	/* temp counter to count the length of file-name */
	FreeNode *free_node_p = NULL;
	void *free_p, *free_node_adress_p = NULL;
	MemoInfoNode *node_p = NULL;
	pthread_mutex_lock(&g_malloc_mutexlock);
	
	/*calculate actual pointer malloc from system.*/
	if (mem_p) {
		free_p = ((UINT8 *)mem_p - sizeof(MemoInfoNode));
		node_p = (MemoInfoNode *)free_p;
	}
	if ((!mem_p) || (node_p->malloc_count != 1)) {
		/*released twice.*/
		counter = pFile;
		while (*counter != '\0') {
			counter++;
			file_len++;
		}
		free_node_adress_p = malloc(sizeof(FreeNode) + file_len);
		free_node_p = (FreeNode*)free_node_adress_p;
//		memset(free_node_p, 0, sizeof(FreeNode));
		memcpy((UINT8 *)free_node_adress_p + sizeof(FreeNode), pFile, file_len);
		free_node_p->line = line;
		add_list(&g_mem_free_erro_info, (NodeType*)free_node_p);
	} else {
		/*release.*/
		delete_list((ListType *)&g_mem_alloc_free_info, 
								(NodeType *)node_p);
		node_p->malloc_count--;
		free(free_p);
	}
	my_free_used++;
	pthread_mutex_unlock(&g_malloc_mutexlock);
	return;
}
/*******************************************************************************
 * print out the info which reflects the malloc/free used 
 * the message has been written into a .txt doc
 *
 * Input: 
 *
 * Output: return 0, successfully done
 * 		 -1, error in print_mem_info
 ******************************************************************************/
int print_mem_info(void)
{
	MemoInfoNode *node_p = NULL;
	FreeNode *f_p = NULL;
	FILE *file_p = NULL;
	printf("malloc() used %d, free() used %d \n",
				my_malloc_used, my_free_used);
	pthread_mutex_lock(&g_malloc_mutexlock);
	
	if ((file_p = open_file(file_path,"w+")) == NULL) {
		printf("File open error! \n");
		return -1;
	}				
	
	/*print mem leaked infomation*/
	if (g_mem_alloc_free_info.count == 0) {
		printf("All the memory that has been assigned has been released!\n");
	} else {
		if (printf_file(file_p, "Malloc used %d, Free used %d \n", 
					my_malloc_used, my_free_used) < 0) {
			printf("Write in File error! \n");
			return -1;
		}

		remain_count = g_mem_alloc_free_info.count;
		node_p = (MemoInfoNode*)get_list(&g_mem_alloc_free_info);
		while (node_p) {
			if (printf_file(file_p,"Mem leaked in file %s, lines in %d\n",
			(char *)((char *)node_p + sizeof(MemoInfoNode) + node_p->size), 
					node_p->line) < 0) {
				printf("Write in File error! \n");
				return -1;
			}
			free(node_p);
			node_p = (MemoInfoNode*)get_list(&g_mem_alloc_free_info);
		}
	}
	
	/*print double free infomation.*/
	if (g_mem_free_erro_info.count == 0) {
		printf(
			"There is no mem which has been freed 2 times\n");
	} else {
		free_remain = g_mem_free_erro_info.count;
		f_p = (FreeNode*)get_list(&g_mem_free_erro_info);
		while (f_p) {        
			if (printf_file(file_p,
					"Mem freed 2 times in file %s, lines in %d\n", 
					(char *)((void *)f_p + sizeof(FreeNode)), f_p->line) < 0) {
				printf("Write in File error! \n");
				return -1;
			}
			free(f_p);
			f_p = (FreeNode*)get_list(&g_mem_free_erro_info);
		}
	}

	if (printf_file(file_p, "The number of node still in MeM list is %d \n", 
				remain_count) < 0) {
		printf("Remain_count writen in File error! \n");
		return -1;
	}
	if (printf_file(file_p, "The number of node freed 2times is %d \n", 
				free_remain) < 0) {
		printf("Remain_count writen in File error! \n");
		return -1;
	}
	if (close_file(file_p) < 0) {
		printf("Close File Operation error! \n");
		return -1;
	}
	printf("The number of mem not freed is %d \n", remain_count);
    printf("Number of node that has been freed 2 times is %d\n", free_remain);
	pthread_mutex_unlock(&g_malloc_mutexlock);
	return 0;
}
/*******************************************************************************
 * init lte_malloc mod.
 *
 * Input: 
 *
 * Output: 
 ******************************************************************************/
INT32 malloc_init()
{
	return 0;
}
/*******************************************************************************
 * cleanup lte_malloc mod.
 *
 * Input: 
 *
 * Output: 
 ******************************************************************************/
INT32 malloc_cleanup()
{
	return 0;
}

INT32 init_mem_buff(){
	INT32 i = 0;
#ifdef USE_MEM_BUFF
	for (i = 0; i < MEM_BUFF_NUM; i++) {
		g_mem_buff[i].msg_p = malloc(EVERY_MEM_SIZE);
		g_mem_buff[i].no = i;
		g_mem_buff[i].len = EVERY_MEM_SIZE;
        g_mem_buff[i].flag = 0;
		//g_mem_sema[i] = create_semb(0);
	}
#endif
	s_mem_lock = create_lock();
	return 0;
}

INT32 clean_mem_buff(){
	INT32 i = 0;

#ifdef USE_MEM_BUFF
	for (i = 0; i < MEM_BUFF_NUM; i++) {
		free(g_mem_buff[i].msg_p);
		//create_semb(g_mem_sema[i]);
	}
#endif
	return 0;
}

static UINT32 top = 0;

INT32 malloc_mem_buff(UINT32 msg_len, UINT32 offset, MemBuff *mem_buff_p) 
{
lte_lock(s_mem_lock);
	static INT32 start = 0;
	INT32 i = 0, value = 0;
	INT32 end = MEM_BUFF_NUM + start;
#if 0
	/*the start search location is the next of the prior malloc location.*/
	for (i = start; i < end; i++) {
		value = g_mem_buff[i % MEM_BUFF_NUM].flag;//get_sem_value(g_mem_sema[i % MEM_BUFF_NUM]);
		if (0 == value && g_mem_buff[i % MEM_BUFF_NUM].len >= (msg_len + offset))
			break;
	}

	if (i == end) {
		lte_unlock(s_mem_lock);
		return 1;
	}
	i = i % MEM_BUFF_NUM;

        g_mem_buff[i].flag++;
	mem_buff_p->no = g_mem_buff[i].no;
	mem_buff_p->len = msg_len;
	mem_buff_p->msg_p = g_mem_buff[i].msg_p + offset;
	mem_buff_p->offset = offset;
//	give_sem(g_mem_sema[i]);
	/*update the begin location.*/
	start = (i + 1) % MEM_BUFF_NUM;


#else
	if(g_mem_buff[top].len < (msg_len + offset) ){
		printf("required buff:%d larger than mem size:%d\n",msg_len + offset, g_mem_buff[top].len);
		lte_unlock(s_mem_lock);
		return -1;
	}
	if(g_mem_buff[top].flag != 0){
//		printf("mem id:%d have used,flag:%d \n", top, g_mem_buff[top].flag);
//		lte_unlock(s_mem_lock);
//	return -1;
	}
        g_mem_buff[top].flag = 1;
        mem_buff_p->no = g_mem_buff[top].no;
        mem_buff_p->len = msg_len;
        mem_buff_p->msg_p = g_mem_buff[top].msg_p + offset;
        mem_buff_p->offset = offset;	
	top = (top + 1) % MEM_BUFF_NUM;
#endif
lte_unlock(s_mem_lock);
	return 0;
}

INT32 copy_mem_buff(MemBuff *des_buff_p, MemBuff *source_buff_p, UINT32 msg_len, UINT32 offset)
{
lte_lock(s_mem_lock);

	if (offset + msg_len > source_buff_p->len) {
		lte_unlock(s_mem_lock);
		return 1;
	}

	des_buff_p->msg_p = source_buff_p->msg_p + offset;
	des_buff_p->len = msg_len;
	des_buff_p->no = source_buff_p->no;
	des_buff_p->offset = source_buff_p->offset + offset;
    g_mem_buff[source_buff_p->no].flag++;
//	give_sem(g_mem_sema[source_buff_p->no]);

lte_unlock(s_mem_lock);
	return 0;
}

INT32 free_mem_buff(MemBuff *mem_buff_p) 
{
lte_lock(s_mem_lock);
	if (NULL == mem_buff_p || mem_buff_p->no >= MEM_BUFF_NUM || NULL == mem_buff_p->msg_p){
		printf("msg buff is null!!, mem_buff_p=%p, mem_buff_p->no=%d, mem_buff_p->msg_p=%p\n",
				mem_buff_p, mem_buff_p->no, mem_buff_p->msg_p);
		lte_unlock(s_mem_lock);
		return 1;
	}

	if (g_mem_buff[mem_buff_p->no].flag <= 0){
//		printf("the buffer is null, can not be freed,no:%d,flag:%d, top:%d\n",mem_buff_p->no,g_mem_buff[mem_buff_p->no].flag, top);
		lte_unlock(s_mem_lock);
		return 1;
	}else{
//		printf("free membuf ,no:%d,flag:%d, top:%d\n",mem_buff_p->no,g_mem_buff[mem_buff_p->no].flag, top);
        	g_mem_buff[mem_buff_p->no].flag--;
	}
    
//	take_sem(g_mem_sema[mem_buff_p->no], NO_WAIT);

	mem_buff_p->msg_p = NULL;
lte_unlock(s_mem_lock);
	return 0;
}
