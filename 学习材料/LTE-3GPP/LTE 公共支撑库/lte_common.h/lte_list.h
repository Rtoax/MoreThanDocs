/*******************************************************************************
 ** 
 ** Copyright (c) 2006-2010 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: lte_list.h
 ** Description: Header file for list functions.
 **
 ** Old Version: 1.0
 ** Author: Jihua Zhou (jhzhou@ict.ac.cn)
 ** Date: 2006.01.06
 **
 ******************************************************************************/


#ifndef LIST_H
#define LIST_H

/* Dependencies ------------------------------------------------------------- */
#include <stdlib.h>
#include "lte_type.h"

/* Constants ---------------------------------------------------------------- */

/* Types -------------------------------------------------------------------- */
typedef struct nodetype{
	struct nodetype *next;
	struct nodetype *previous;
}NodeType;
typedef struct listtype{
	NodeType ln;
	UINT32 count;
}ListType;

/* Macros ------------------------------------------------------------------- */

/* Globals ------------------------------------------------------------------ */

/* Functions ---------------------------------------------------------------- */

/*******************************************************************************
 * Initialize a list.
 *
 * Input: list_p: pointer to list header to be initialized.
 *
 * Output: None.
 ******************************************************************************/
void init_list(ListType *list_p);

/*******************************************************************************
 * Add a node to the end of a list.
 *
 * Input: list_p: pointer to list head.
 *        node_p: pointer to node to be added.
 *
 * Output: None.
 ******************************************************************************/
 void add_list(ListType *list_p, NodeType *node_p);

/*******************************************************************************
 * Concatenate two lists.
 * Concatenate the second list to the end of the first list, and the second
 * list is left empty, but the list head is not freed.
 *
 * Input: dst_list_p: pointer to destination list head.
 *        add_list_p: pointer to list head to be added.
 *
 * Output: None.
 ******************************************************************************/
 void concat_list(ListType *dst_list_p, ListType *add_list_p);

/*******************************************************************************
 * Report the number of nodes in a list.
 *
 * Input: list_p: pointer to list head.
 *
 * Output: return the number of nodes in the list.
 ******************************************************************************/
INT32 count_list(ListType *list_p);

/*******************************************************************************
 * Delete a node from list.
 *
 * Input: list_p: pointer to list head.
 *        node_p: pointer to node to be deleted.
 *
 * Output: None.
 ******************************************************************************/
 void delete_list(ListType *list_p, NodeType *node_p);

/*******************************************************************************
 * find first node in list.
 *
 * Input: list_p: pointer to list head.
 * 
 * Output: return pointer to the first node in a list,
 *                NULL, if the list is empty.              
 ******************************************************************************/
 NodeType *first_list(ListType *list_p);

/*******************************************************************************
 * Delete and return the first node from list.
 *
 * Input: list_p: pointer to list head.
 * 
 * Output: return pointer to the first node gotten,
 *                NULL, if the list is NULL.
 ******************************************************************************/
 NodeType *get_list(ListType *list_p);

/*******************************************************************************
 * Insert a node in a list after a specified node.
 *
 * Input: list_p: pointer to list head.
 *        prev_p: pointer to node after which to insert.
 *                if prev_p is NULL, the node is inserted at the head of list.
 *        node_p: pointer to node to be inserted.
 * 
 * Output: None.
 ******************************************************************************/
 void insert_list(ListType *list_p, NodeType *prev_p, NodeType *node_p);

/*******************************************************************************
 * Find the last node in a list.
 *
 * Input: list_p: pointer to list head.
 *        prev_p: pointer to node after which to insert.
 * 
 * Output: return pointer to the last node in the list,
 *                NULL, if the list is empty.
 ******************************************************************************/
 NodeType *last_list(ListType *list_p);

/*******************************************************************************
 * Find the next node in a list.
 *
 * Input: node_p: pointer to node whose successor is to be found.
 * 
 * Output: return pointer to the next node in the list,
 *                NULL, if there is no next node.
 ******************************************************************************/
 NodeType *next_list(NodeType *node_p);

/*******************************************************************************
 * Find the Nth node in a list.
 *
 * Input: list_p: pointer to list head.
 *        node_num: number of node to be found,
 *                  the first node in the list is numbered 1.
 * 
 * Output: return pointer to the Nth node in the list,
 *                NULL, if there is no Nth node.
 ******************************************************************************/
 NodeType *nth_list(ListType *list_p, INT32 node_num);

/*******************************************************************************
 * Find the previous node in a list.
 *
 * Input: node_p: pointer to node whose predecessor is to be found.
 * 
 * Output: return pointer to the previous node in the list,
 *                NULL, if there is no previous node.
 ******************************************************************************/
 NodeType *previous_list(NodeType *node_p);

/*******************************************************************************
 * Free up a list.
 *
 * Input: list_p: pointer to list head for which to free all nodes.
 * 
 * Output: None.
 ******************************************************************************/
 void free_list(ListType *list_p);

#endif	/* LIST_H */
