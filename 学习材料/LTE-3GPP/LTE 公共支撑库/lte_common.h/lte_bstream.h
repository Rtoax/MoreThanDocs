/*******************************************************************************
 ** 
 ** Copyright (c) 2011-2015 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: lte_bstream.h
 ** Description: this file is the header of the byte stream type def
 **
 ** Current Version: 0.1
 ** $Revision:  
 ** Author: yxn
 ** Date: 2011.11.30
 **
*******************************************************************************/

#ifndef LTE_BSTREAM_H
#define LTE_BSTREAM_H

/* -------------------------------------------------------------------------- */
/* Dependencies */
/* -------------------------------------------------------------------------- */
#include "lte_type.h"

/* -------------------------------------------------------------------------- */
/* Constants */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* Macros */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* Type Defs */
/* -------------------------------------------------------------------------- */
typedef struct ByteStream_s {
	UINT8 *data;
	UINT32 len;
	UINT32 offset;
} ByteStream_t;


/* -------------------------------------------------------------------------- */
/* Functions */
/* -------------------------------------------------------------------------- */


/* byte stream op*/
INT32 lte_bstream_init(ByteStream_t *bs, UINT8 *data, INT32 len);
UINT32 lte_bstream_empty(ByteStream_t *bs);
UINT32 lte_bstream_curpos(ByteStream_t *bs);
UINT32 lte_bstream_setpos(ByteStream_t *bs, UINT32 off);
void lte_bstream_rewind(ByteStream_t *bs);
INT32 lte_bstream_advance(ByteStream_t *bs, INT32 n);

UINT8 lte_bs_get8(ByteStream_t *bs);
UINT16 lte_bs_get16(ByteStream_t *bs);
UINT32 lte_bs_get32(ByteStream_t *bs);

INT32 lte_bs_put8(ByteStream_t *bs, UINT8 v);
INT32 lte_bs_put16(ByteStream_t *bs, UINT16 v);
INT32 lte_bs_put32(ByteStream_t *bs, UINT32 v);

INT32 lte_bs_getrawbuf(ByteStream_t *bs, UINT8 *buf, INT32 len);
UINT8 *lte_bs_getraw(ByteStream_t *bs, INT32 len);
INT8 *lte_bs_getstr(ByteStream_t *bs, INT32 len);
INT32 lte_bs_putraw(ByteStream_t *bs, const UINT8 *v, INT32 len);
INT32 lte_bs_putbs(ByteStream_t *bs, ByteStream_t *srcbs, INT32 len);


#endif /* LTE_TLV_H */

