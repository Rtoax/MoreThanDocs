/*******************************************************************************
 ** 
 ** Copyright (c) 2006-2010 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: lte_list.
 ** Description: source file for list functions.
 **
 ** Old Version: 1.0
 ** Author: Jihua Zhou (jhzhou@ict.ac.cn)
 ** Date: 2006.01.06
 **
 ******************************************************************************/
#include <memory.h>
#include "lte_malloc.h"
#include "lte_list.h"
/*******************************************************************************
 * Wether the node is in a list or not.
 *
 * Input: node_p: pointer to node to be checked.
 *
 * Output: return 0, in list;
 *                -1, not in list.
 ******************************************************************************/
INT32 check_node(NodeType *node_p) {
	if (node_p->previous) {
		if (node_p->previous->next != node_p) {
			return -1; /* The node is not in the list */
		}
	}
    if (node_p->next) {
        if (node_p->next->previous != node_p) {
            return -1; /* The node is not in the list */
        }
    }

	return 0;	/* Maybe the node is in a list, but we can not assure */
}

/*******************************************************************************
 * Initialize a list.
 *
 * Input: list_p: pointer to list header to be initialized.
 *
 * Output: None.
 ******************************************************************************/
 void init_list(ListType *list_p)
{
	if (!list_p) {
		return;
	}
	memset(list_p, 0, sizeof(ListType));
}

/*******************************************************************************
 * Add a node to the end of a list.
 *
 * Input: list_p: pointer to list head.
 *        node_p: pointer to node to be added.
 *
 * Output: None.
 ******************************************************************************/
 void add_list(ListType *list_p, NodeType *node_p)
{
 
	NodeType *tail_p;
	
	if (!list_p || !node_p) {
		return;
	}
	tail_p = ((NodeType *)list_p)->previous;	/* Seek tail */
	if (!tail_p) {
		tail_p = (NodeType *)list_p;
		node_p->previous = NULL;/* First node's previous is NULL */
	} else {
		node_p->previous = tail_p;
	}
	tail_p->next = node_p;		/* Link the node to tail */
	node_p->next = NULL;
	((NodeType *)list_p)->previous = node_p;	/* Set new tail */
	list_p->count++;			/* Increcement node count */
 
}

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
  void concat_list(ListType *dst_list_p, ListType *add_list_p)
{
 	NodeType *first_p, *last_p;
 	
	if (!dst_list_p || !add_list_p) {
		return;
	}

 	first_p = ((NodeType *)add_list_p)->next;
	if (!first_p) {
		return;
	}
	last_p = ((NodeType *)dst_list_p)->previous;
	if (!last_p) {
		last_p = (NodeType *)dst_list_p;
	} else {
		first_p->previous = last_p;
	}
	last_p->next = first_p;
	((NodeType *)dst_list_p)->previous = ((NodeType *)add_list_p)->previous;
	dst_list_p->count += add_list_p->count;
	memset(add_list_p, 0, sizeof(ListType));
 }

/*******************************************************************************
 * Report the number of nodes in a list.
 *
 * Input: list_p: pointer to list head.
 *
 * Output: return the number of nodes in the list.
 ******************************************************************************/
 INT32 count_list(ListType *list_p)
{
	if (!list_p) {
		return -1;
	}

 	return list_p->count;
}

/*******************************************************************************
 * Delete a node from list.
 *
 * Input: list_p: pointer to list head.
 *        node_p: pointer to node to be deleted.
 *
 * Output: None.
 ******************************************************************************/
 void delete_list(ListType *list_p, NodeType *node_p)
{
	NodeType *prev_p, *next_p;
	
	if (!list_p || !node_p) {
		return;
	}

 	if (list_p->count < 1) {
		return;		/* List is NULL */
	}
	/* Check the node */
	if (check_node(node_p) < 0) {
		/* Node is not in the list */
		return;
	}
	
	list_p->count--;
	prev_p = node_p->previous;
	next_p = node_p->next;
	if (prev_p) {
		prev_p->next = next_p;
	} else {
		((NodeType *)list_p)->next = next_p;
	}
	if (next_p) {
		next_p->previous = prev_p;
	} else {
		((NodeType *)list_p)->previous = prev_p;
	}
	node_p->next = NULL;
	node_p->previous = NULL;
 }

/*******************************************************************************
 * find first node in list.
 *
 * Input: list_p: pointer to list head.
 * 
 * Output: return pointer to the first node in a list,
 *                NULL, if the list is empty.              
 ******************************************************************************/
  NodeType *first_list(ListType *list_p)
{
	if (!list_p) {
		return NULL;
	}

 	if (list_p->count < 1) {
		return NULL;
	}
	return ((NodeType *)list_p)->next;
 }

/*******************************************************************************
 * Delete and return the first node from list.
 *
 * Input: list_p: pointer to list head.
 * 
 * Output: return pointer to the first node gotten,
 *                NULL, if the list is NULL.
 ******************************************************************************/
 NodeType *get_list(ListType *list_p)
{
 	NodeType *first_p;
 	
	if (!list_p) {
		return NULL;
	}
 	if (list_p->count < 1) {
		return NULL;
	}
	first_p = ((NodeType *)list_p)->next;
	if (!first_p) {
		return NULL;
	}
	((NodeType *)list_p)->next = first_p->next;
	if (first_p->next) {
		first_p->next->previous = NULL;	/* First node's previous is NULL */
	} else {
		((NodeType *)list_p)->previous = NULL;	/* List is NULL */
	}
	list_p->count--;
	first_p->next = NULL;	/* Remove the first node from the list */
	return first_p;
 }

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
 void insert_list(ListType *list_p, NodeType *prev_p, NodeType *node_p)
{
 	NodeType *next_p;
 	
	if (!list_p || !node_p) {
		return;
	}
 	if (!prev_p) {	/* Insert at the head of the list */
		next_p = ((NodeType *)list_p)->next;
		((NodeType *)list_p)->next = node_p;
	} else {		/* Insert after the specified node */
		/* Check the node */
		if (check_node(prev_p) < 0) {
			/* Node is not in the list */
			return;
		}
		next_p = prev_p->next;
		prev_p->next = node_p;
	}
	node_p->next = next_p;
	node_p->previous = prev_p;
	
	if (!next_p) {
		((NodeType *)list_p)->previous = node_p;
	} else {
		next_p->previous = node_p;
	}
	list_p->count++;
 }

/*******************************************************************************
 * Find the last node in a list.
 *
 * Input: list_p: pointer to list head.
 *        prev_p: pointer to node after which to insert.
 * 
 * Output: return pointer to the last node in the list,
 *                NULL, if the list is empty.
 ******************************************************************************/
NodeType *last_list(ListType *list_p)
{
	if (!list_p) {
		return NULL;
	}
 	return ((NodeType *)list_p)->previous;	

}

/*******************************************************************************
 * Find the next node in a list.
 *
 * Input: node_p: pointer to node whose successor is to be found.
 * 
 * Output: return pointer to the next node in the list,
 *                NULL, if there is no next node.
 ******************************************************************************/
 NodeType *next_list(NodeType *node_p)
{
	if (!node_p) {
		return NULL;
	}

 	/* Check the node */
	if (check_node(node_p) < 0) {
		/* Node is not in the list */
		return NULL;
	}
	
	return node_p->next;
 }

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
 NodeType *nth_list(ListType *list_p, INT32 node_num)
{
 	INT32 count, i;
	NodeType *node_p;
 
	if (!list_p) {
		return NULL;
	}
 	count = list_p->count;
	if ((node_num < 1) || (node_num > count)) {
		return NULL;
	}
	if (node_num > (count / 2)) {	
		node_p = ((NodeType *)list_p)->previous;	/* Search from tail */
		node_num = count - node_num + 1;
		for (i = 1; i < node_num; i++) {
			node_p = node_p->previous;
		}
	} else {
		node_p = ((NodeType *)list_p)->next;		/* Search from head */
		for (i = 1; i < node_num; i++) {
			node_p = node_p->next;
		}
	}
	return node_p;
 }

/*******************************************************************************
 * Find the previous node in a list.
 *
 * Input: node_p: pointer to node whose predecessor is to be found.
 * 
 * Output: return pointer to the previous node in the list,
 *                NULL, if there is no previous node.
 ******************************************************************************/
 NodeType *previous_list(NodeType *node_p)
{
	if (!node_p) {
		return NULL;
	}

 	/* Check the node */
	if (check_node(node_p) < 0) {
		/* Node is not in the list */
		return NULL;
	}
	
	return node_p->previous;
}

/*******************************************************************************
 * Free up a list.
 *
 * Input: list_p: pointer to list head for which to free all nodes.
 * 
 * Output: None.
 ******************************************************************************/
 void free_list(ListType *list_p)
{
 	NodeType *node_p, *next_p;
 	
	if (!list_p) {
		return;
	}
    if (!list_p) {
		return;
	}
 	if (list_p->count < 1) {
		memset(list_p, 0, sizeof(ListType));
		return;
	}
	node_p = ((NodeType *)list_p)->next;
	while (node_p) {
		next_p = node_p->next;
		lte_free(node_p);
		node_p = next_p;
	}
	memset(list_p, 0, sizeof(ListType));
 }

/*******************************************************************************
 * Delete and return the first n nodes from list.
 * If the list count is less than number of nodes to be gotton, all nodes in the
 * list will be returned.
 *
 * Input: list_p: pointer to list head.
 *        node_num: number of nodes to be gotton.
 *
 * Output: return pointer to the first node gotten,
 *                NULL, if the list is NULL.
 ******************************************************************************/
 NodeType *get_list_n(ListType *list_p, INT32 node_num)
{
	NodeType *first_p, *next_p;
	INT32 count;
	
	if (!list_p || (node_num <= 0)) {
		return NULL;
	}
	if (list_p->count < 1) {
		return NULL;
	}
	count = node_num;
	first_p = ((NodeType *)list_p)->next;
	if (node_num >= list_p->count) {
		memset(list_p, 0, sizeof(ListType));
	} else {
		next_p = first_p;
		while (--node_num) {
			next_p = next_p->next;
		}
		/* Now, next_p pointed to the Nth node */
		next_p->next->previous = NULL;	/* The (N+1)th node is new first node */
		/* Remove the gotton nodes from list */
		((NodeType *)list_p)->next = next_p->next;
		next_p->next = NULL;
		list_p->count -= count;
	}
		
	return first_p;
}

/*******************************************************************************
 * Add n nodes to the end of a list.
 * If node count of the node list is less than number of nodes to be added, 
 * all nodes in the node list will be added, and the added count is less than
 * expected.
 *
 * Input: list_p: pointer to list head.
 *        node_p: pointer to first node to be added.
 *        node_num: number of nodes to be added.
 *
 * Output: return number of nodes added, if success;
 *                -1, if failure.
 ******************************************************************************/
INT32 add_list_n(ListType *list_p, NodeType *node_p, INT32 node_num)
{
	NodeType *last_p, *next_p;
	INT32 count = 1;

	if (!list_p || !node_p || (node_num < 0)) {
		return -1;
	}
	if (node_num == 0) {
		return 0;
	}
	next_p = node_p;
	while (--node_num) {
		if (!next_p->next) {
			break;
		}
		next_p = next_p->next;
		count++;
	}
	if (next_p->next) {
		next_p->next->previous = NULL;
		next_p->next = NULL;
	}
	last_p = ((NodeType *)list_p)->previous;
	if (!last_p) {
		last_p = (NodeType *)list_p;
		node_p->previous = NULL;
	} else {
		node_p->previous = last_p;
	}
	last_p->next = node_p;	/* Add the nodes to the list */
	((NodeType *)list_p)->previous = next_p;	/* Set list tail */
	list_p->count += count;

	return count;
}

/*******************************************************************************
 * Insert n nodes in a list after a specified node.
 *
 * Input: list_p: pointer to list head.
 *        prev_p: pointer to node after which to insert.
 *                if prev_p is NULL, n nodes are inserted at the head of list.       
 *        node_p: pointer to the first node of n nodes to be inserted.
 *        node_num: number of nodes to be inserted.
 *                  if the node count of node_p is less than node_num,
 *                  all nodes are inserted.
 * 
 * Output: remain_p: pointer to the first node of remain nodes 
 *                   which are not inserted.
 *                   NULL, if all nodes are inserted.
 *         return number of nodes inserted, if success;
 *                -1, if insertion failed.
 ******************************************************************************/
INT32 insert_list_n(ListType *list_p, NodeType *prev_p,
						NodeType *node_p, INT32 node_num, NodeType **remain_p)
{
	NodeType *next_p;
	INT32 node_count = 1;
	
	if (!list_p || !node_p || (node_num < 0)) {
		return -1;
	}
	if (node_num == 0) {
		if (remain_p) {
			*remain_p = node_p;
		}
		return 0;
	}
	if (!prev_p) {	/* Insert at the head of the list */
		next_p = ((NodeType *)list_p)->next;
		((NodeType *)list_p)->next = node_p;
	} else {		/* Insert after the specified node */
		/* Check the node */
		if (check_node(prev_p) < 0) {
			/* Node is not in the list */
			return -1;
		}
		next_p = prev_p->next;
		prev_p->next = node_p;
	}
	node_p->previous = prev_p;
	while (node_p->next) {
		if ((node_count++) == node_num) {
			break;
		}
		node_p = node_p->next;
	}
	if (remain_p) {
		*remain_p = node_p->next;
	}
	node_p->next = next_p;
	
	if (!next_p) {
		((NodeType *)list_p)->previous = node_p;
	} else {
		next_p->previous = node_p;
	}
	list_p->count += node_count;

	return node_count;
}


