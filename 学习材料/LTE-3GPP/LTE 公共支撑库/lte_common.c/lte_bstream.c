/*******************************************************************************
 ** 
 ** Copyright (c) 2008-2012 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: lte_bstream.c
 ** Description: This file contains all functions needed to use bstreams
 **
 ** Current Version: 0.1
 ** $Revision:   $
 ** Author: yxn
 ** Date: 2011.12.15
 **
*******************************************************************************/

/* -------------------------------------------------------------------------- */
/* Dependencies */
/* -------------------------------------------------------------------------- */
#include "lte_tlv.h" 
#include "lte_malloc.h"

/* -------------------------------------------------------------------------- */
/* Constants */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* Macros */
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Type Defs */
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Globals */
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/* Functions */
/* -------------------------------------------------------------------------- */
INT32 lte_bstream_init(ByteStream_t *bs, UINT8 *data, INT32 len)
{
	bs->data = data;
	bs->len = len;
	bs->offset = 0;
	return 0;
}

UINT32 lte_bstream_empty(ByteStream_t *bs)
{
	return bs->len - bs->offset;
}

UINT32 lte_bstream_curpos(ByteStream_t *bs)
{
	return bs->offset;
}

UINT32 lte_bstream_setpos(ByteStream_t *bs, UINT32 off)
{

	if (off > bs->len)
		return -1;

	bs->offset = off;

	return off;
}

void lte_bstream_rewind(ByteStream_t *bs)
{

	lte_bstream_setpos(bs, 0);

	return;
}

INT32 lte_bstream_advance(ByteStream_t *bs, INT32 n)
{

	if (lte_bstream_empty(bs) < n)
		return -1; /* XXX throw an exception */

	bs->offset += n;

	return n;
}

UINT8 lte_bs_get8(ByteStream_t *bs)
{
	
	if (lte_bstream_empty(bs) < 1)
		return -1; /* XXX throw an exception */
	
	bs->offset++;
	
	return BS_GET8(bs->data + bs->offset - 1);
}

UINT16 lte_bs_get16(ByteStream_t *bs)
{
	
	if (lte_bstream_empty(bs) < 2)
		return -1; /* XXX throw an exception */
	
	bs->offset += 2;
	
	return BS_GET16(bs->data + bs->offset - 2);
}

UINT32 lte_bs_get32(ByteStream_t *bs)
{

	if (lte_bstream_empty(bs) < 4)
		return -1; /* XXX throw an exception */
	
	bs->offset += 4;
	
	return BS_GET32(bs->data + bs->offset - 4);
}



INT32 lte_bs_put8(ByteStream_t *bs, UINT8 v)
{

	if (lte_bstream_empty(bs) < 1)
		return -1; /* XXX throw an exception */

	bs->offset += BS_PUT8(bs->data + bs->offset, v);

	return 0;
}

INT32 lte_bs_put16(ByteStream_t *bs, UINT16 v)
{

	if (lte_bstream_empty(bs) < 2)
		return -1; /* XXX throw an exception */

	bs->offset += BS_PUT16(bs->data + bs->offset, v);

	return 0;
}

INT32 lte_bs_put32(ByteStream_t *bs, UINT32 v)
{

	if (lte_bstream_empty(bs) < 4)
		return -1; /* XXX throw an exception */

	bs->offset += BS_PUT32(bs->data + bs->offset, v);

	return 0;
}

INT32 lte_bs_getrawbuf(ByteStream_t *bs, UINT8 *buf, INT32 len)
{

	if (lte_bstream_empty(bs) < len)
		return -1;

	memcpy(buf, bs->data + bs->offset, len);
	bs->offset += len;

	return len;
}

UINT8 *lte_bs_getraw(ByteStream_t *bs, INT32 len)
{
	UINT8 *ob;

	if (!(ob = lte_malloc(len)))
		return NULL;

	if (lte_bs_getrawbuf(bs, ob, len) < len) {
		lte_free(ob);
		return NULL;
	}

	return ob;
}

INT8 *lte_bs_getstr(ByteStream_t *bs, INT32 len)
{
	INT8 *ob;

	if (!(ob = lte_malloc(len+1)))
		return NULL;

	if (lte_bs_getrawbuf(bs, (UINT8 *)ob, len) < len) {
		lte_free(ob);
		return NULL;
	}

	ob[len] = '\0';

	return ob;
}

INT32 lte_bs_putraw(ByteStream_t *bs, const UINT8 *v, INT32 len)
{

	if (lte_bstream_empty(bs) < len)
		return -1; /* XXX throw an exception */
	if (NULL == v)
		return -1;

	memcpy(bs->data + bs->offset, v, len);
	bs->offset += len;

	return len;
}

INT32 lte_bs_putbs(ByteStream_t *bs, ByteStream_t *srcbs, INT32 len)
{

	if (lte_bstream_empty(srcbs) < len)
		return -1; /* XXX throw exception (underrun) */

	if (lte_bstream_empty(bs) < len)
		return -1; /* XXX throw exception (overflow) */

	memcpy(bs->data + bs->offset, srcbs->data + srcbs->offset, len);
	bs->offset += len;
	srcbs->offset += len;

	return len;
}
