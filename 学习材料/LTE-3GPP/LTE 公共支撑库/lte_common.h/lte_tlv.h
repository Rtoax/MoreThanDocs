/*******************************************************************************
 ** 
 ** Copyright (c) 2011-2015 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: lte_tlv.h
 ** Description: this file is the header of the tlv type def
 **
 ** Current Version: 0.1
 ** $Revision:  
 ** Author: yxn
 ** Date: 2011.11.30
 **
*******************************************************************************/

#ifndef LTE_TLV_H
#define LTE_TLV_H

/* -------------------------------------------------------------------------- */
/* Dependencies */
/* -------------------------------------------------------------------------- */
#include "lte_type.h"
#include "lte_bstream.h"
#include "lte_malloc.h"

#include <string.h>		/* memcpy */
	

/* -------------------------------------------------------------------------- */
/* Constants */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* Macros */
/* -------------------------------------------------------------------------- */

#ifndef __LITTLE_ENDIAN__

/* byte stream encode Big-endian versions */
#define BS_PUT8(buf, data) ((*(UINT8*)(buf) = (UINT8)(data)&0xff),1)
#define BS_GET8(buf) ((*(buf))&0xff)
#define BS_PUT16(buf, data) ( \
		(*(UINT8*)(buf) = (UINT8)((data)>>8)&0xff), \
		(*(UINT8*)((buf)+1) = (UINT8)(data)&0xff),  \
		2)
#define BS_GET16(buf) ((((*(buf))<<8)&0xff00) + ((*((buf)+1)) & 0xff))
#define BS_PUT32(buf, data) ( \
		(*((UINT8*)(buf)) = (UINT8)((data)>>24)&0xff), \
		(*(UINT8*)((buf)+1) = (UINT8)((data)>>16)&0xff), \
		(*(UINT8*)((buf)+2) = (UINT8)((data)>>8)&0xff), \
		(*(UINT8*)((buf)+3) = (UINT8)(data)&0xff), \
		4)
#define BS_GET32(buf) ((((*(buf))<<24)&0xff000000) + \
		(((*((buf)+1))<<16)&0x00ff0000) + \
		(((*((buf)+2))<< 8)&0x0000ff00) + \
		(((*((buf)+3))&0x000000ff)))
#else
/* byte stream encode Little-endian versions */
#define BS_PUT8(buf, data) ( \
		(*(UINT8*)(buf) = (UINT8)(data) & 0xff), \
		1)
#define BS_GET8(buf) ( \
		(*(buf)) & 0xff \
		)
#define BS_PUT16(buf, data) ( \
		(*(UINT8*)((buf)+0) = (UINT8)((data) >> 0) & 0xff),  \
		(*(UINT8*)((buf)+1) = (UINT8)((data) >> 8) & 0xff), \
		2)
#define BS_GET16(buf) ( \
		(((*((buf)+0)) << 0) & 0x00ff) + \
		(((*((buf)+1)) << 8) & 0xff00) \
		)
#define BS_PUT32(buf, data) ( \
		(*(UINT8*)((buf)+0) = (UINT8)((data) >>  0) & 0xff), \
		(*(UINT8*)((buf)+1) = (UINT8)((data) >>  8) & 0xff), \
		(*(UINT8*)((buf)+2) = (UINT8)((data) >> 16) & 0xff), \
		(*(UINT8*)((buf)+3) = (UINT8)((data) >> 24) & 0xff), \
		4)
#define BS_GET32(buf) ( \
		(((*((buf)+0)) <<  0) & 0x000000ff) + \
		(((*((buf)+1)) <<  8) & 0x0000ff00) + \
		(((*((buf)+2)) << 16) & 0x00ff0000) + \
		(((*((buf)+3)) << 24) & 0xff000000))

#endif




/* -------------------------------------------------------------------------- */
/* Type Defs */
/* -------------------------------------------------------------------------- */

/* TLV structure */
typedef struct LteTlv_s {
	UINT16 type;
	UINT16 length;
	UINT8 *value;
} LteTlv_t;

/* TLV List structure */
typedef struct LteTlvList_s {
	LteTlv_t *tlv;
	struct LteTlvList_s *next;
} LteTlvList_t;


/* -------------------------------------------------------------------------- */
/* Globals */
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Functions */
/* -------------------------------------------------------------------------- */

/* TLV handling functions */
LteTlv_t *lte_tlv_gettlv(LteTlvList_t *list, UINT16 type, const int nth);
INT8 *lte_tlv_getstr(LteTlvList_t *list, const UINT16 type, const int nth);
UINT8 lte_tlv_get8(LteTlvList_t *list, const UINT16 type, const int nth);
UINT16 lte_tlv_get16(LteTlvList_t *list, const UINT16 type, const int nth);
UINT32 lte_tlv_get32(LteTlvList_t *list, const UINT16 type, const int nth);

/* no type */
LteTlv_t *lte_tlv_get_nth(LteTlvList_t *list, const INT32 nth);
UINT8 lte_tlv_get8_nth(LteTlvList_t *list, const int nth);
UINT16 lte_tlv_get16_nth(LteTlvList_t *list, const int nth);
UINT32 lte_tlv_get32_nth(LteTlvList_t *list, const int nth);


/* TLV list handling functions */
LteTlvList_t *lte_tlvlist_read(ByteStream_t *bs);
LteTlvList_t *lte_tlvlist_readnum(ByteStream_t *bs, UINT16 num);
LteTlvList_t *lte_tlvlist_readlen(ByteStream_t *bs, UINT16 len);
LteTlvList_t *lte_tlvlist_copy(LteTlvList_t *orig);


INT32 lte_tlvlist_count(LteTlvList_t **list);
INT32 lte_tlvlist_size(LteTlvList_t **list);
INT32 lte_tlvlist_cmp(LteTlvList_t *one, LteTlvList_t *two);
INT32 lte_tlvlist_write(ByteStream_t *bs, LteTlvList_t **list);
void lte_tlvlist_free(LteTlvList_t **list);

INT32 lte_tlvlist_add_raw(LteTlvList_t **list, const UINT16 type, const UINT16 length, const UINT8 *value);
INT32 lte_tlvlist_add_noval(LteTlvList_t **list, const UINT16 type);
INT32 lte_tlvlist_add_8(LteTlvList_t **list, const UINT16 type, const UINT8 value);
INT32 lte_tlvlist_add_16(LteTlvList_t **list, const UINT16 type, const UINT16 value);
INT32 lte_tlvlist_add_32(LteTlvList_t **list, const UINT16 type, const UINT32 value);
INT32 lte_tlvlist_add_caps(LteTlvList_t **list, const UINT16 type, const UINT32 caps);
INT32 lte_tlvlist_add_frozentlvlist(LteTlvList_t **list, UINT16 type, LteTlvList_t **tl);

INT32 lte_tlvlist_replace_raw(LteTlvList_t **list, const UINT16 type, const UINT16 lenth, const UINT8 *value);
INT32 lte_tlvlist_replace_noval(LteTlvList_t **list, const UINT16 type);
INT32 lte_tlvlist_replace_8(LteTlvList_t **list, const UINT16 type, const UINT8 value);
INT32 lte_tlvlist_replace_16(LteTlvList_t **list, const UINT16 type, const UINT16 value);
INT32 lte_tlvlist_replace_32(LteTlvList_t **list, const UINT16 type, const UINT32 value);

void lte_tlvlist_remove(LteTlvList_t **list, const UINT16 type);


#endif /* LTE_TLV_H */

