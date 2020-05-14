/*******************************************************************************
 ** 
 ** Copyright (c) 2008-2012 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: lte_tlv.c
 ** Description: define tlv operat.
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

static LteTlv_t *createtlv(UINT16 type, UINT16 length, UINT8 *value)
{
	LteTlv_t *ret;

	if (!(ret = (LteTlv_t *)lte_malloc(sizeof(LteTlv_t))))
		return NULL;
	ret->type = type;
	ret->length = length;
	ret->value = value;

	return ret;
}

static void lte_freetlv(LteTlv_t **oldtlv)
{

	if (!oldtlv || !*oldtlv)
		return;
	
	lte_free((*oldtlv)->value);
	lte_free(*oldtlv);
	*oldtlv = NULL;

	return;
}

/**
 * Read a TLV chain from a buffer.
 *
 * Reads and parses a series of TLV patterns from a data buffer; the
 * returned structure is manipulatable with the rest of the TLV
 * routines.  When done with a TLV chain, lte_tlvlist_lte_free() should
 * be called to lte_free the dynamic substructures.
 *
 * XXX There should be a flag setable here to have the tlvlist contain
 * bstream references, so that at least the ->value portion of each 
 * element doesn't need to be lte_malloc/memcpy'd.  This could prove to be
 * just as efficient as the in-place TLV parsing used in a couple places
 * in libfaim.
 *
 * @param bs Input bstream
 * @return Return the TLV chain read
 */
  LteTlvList_t *lte_tlvlist_read(ByteStream_t *bs)
{
	LteTlvList_t *list = NULL, *cur;
	
	while (lte_bstream_empty(bs) > 0) {
		UINT16 type, length;

		type = lte_bs_get8(bs);
		length = lte_bs_get8(bs);

		if (length > lte_bstream_empty(bs)) {
			lte_tlvlist_free(&list);
			return NULL;
		}

		cur = (LteTlvList_t *)lte_malloc(sizeof(LteTlvList_t));
		if (!cur) {
			lte_tlvlist_free(&list);
			return NULL;
		}

		memset(cur, 0, sizeof(LteTlvList_t));

		cur->tlv = createtlv(type, length, NULL);
		if (!cur->tlv) {
			lte_free(cur);
			lte_tlvlist_free(&list);
			return NULL;
		}
		if (cur->tlv->length > 0) {
			cur->tlv->value = lte_bs_getraw(bs, length);	
			if (!cur->tlv->value) {
				lte_freetlv(&cur->tlv);
				lte_free(cur);
				lte_tlvlist_free(&list);
				return NULL;
			}
		}

		cur->next = list;
		list = cur;
		
	}

	return list;
}

/**
 * Read a TLV chain from a buffer.
 *
 * Reads and parses a series of TLV patterns from a data buffer; the
 * returned structure is manipulatable with the rest of the TLV
 * routines.  When done with a TLV chain, lte_tlvlist_lte_free() should
 * be called to lte_free the dynamic substructures.
 *
 * XXX There should be a flag setable here to have the tlvlist contain
 * bstream references, so that at least the ->value portion of each 
 * element doesn't need to be lte_malloc/memcpy'd.  This could prove to be
 * just as efficient as the in-place TLV parsing used in a couple places
 * in libfaim.
 *
 * @param bs Input bstream
 * @param num The max number of TLVs that will be read, or -1 if unlimited.  
 *        There are a number of places where you want to read in a tlvchain, 
 *        but the chain is not at the end of the SNAC, and the chain is 
 *        preceded by the number of TLVs.  So you can limit that with this.
 * @return Return the TLV chain read
 */
  LteTlvList_t *lte_tlvlist_readnum(ByteStream_t *bs, UINT16 num)
{
	LteTlvList_t *list = NULL, *cur;

	while ((lte_bstream_empty(bs) > 0) && (num != 0)) {
		UINT16 type, length;

		type = lte_bs_get8(bs);
		length = lte_bs_get8(bs);

		if (length > lte_bstream_empty(bs)) {
			lte_tlvlist_free(&list);
			return NULL;
		}

		cur = (LteTlvList_t *)lte_malloc(sizeof(LteTlvList_t));
		if (!cur) {
			lte_tlvlist_free(&list);
			return NULL;
		}

		memset(cur, 0, sizeof(LteTlvList_t));

		cur->tlv = createtlv(type, length, NULL);
		if (!cur->tlv) {
			lte_free(cur);
			lte_tlvlist_free(&list);
			return NULL;
		}
		if (cur->tlv->length > 0) {
			cur->tlv->value = lte_bs_getraw(bs, length);
			if (!cur->tlv->value) {
				lte_freetlv(&cur->tlv);
				lte_free(cur);
				lte_tlvlist_free(&list);
				return NULL;
			}
		}

		if (num > 0)
			num--;
		cur->next = list;
		list = cur;
	}

	return list;
}

/**
 * Read a TLV chain from a buffer.
 *
 * Reads and parses a series of TLV patterns from a data buffer; the
 * returned structure is manipulatable with the rest of the TLV
 * routines.  When done with a TLV chain, lte_tlvlist_lte_free() should
 * be called to lte_free the dynamic substructures.
 *
 * XXX There should be a flag setable here to have the tlvlist contain
 * bstream references, so that at least the ->value portion of each 
 * element doesn't need to be lte_malloc/memcpy'd.  This could prove to be
 * just as efficient as the in-place TLV parsing used in a couple places
 * in libfaim.
 *
 * @param bs Input bstream
 * @param len The max length in bytes that will be read.
 *        There are a number of places where you want to read in a tlvchain, 
 *        but the chain is not at the end of the SNAC, and the chain is 
 *        preceded by the length of the TLVs.  So you can limit that with this.
 * @return Return the TLV chain read
 */
  LteTlvList_t *lte_tlvlist_readlen(ByteStream_t *bs, UINT16 len)
{
	LteTlvList_t *list = NULL, *cur;

	while ((lte_bstream_empty(bs) > 0) && (len > 0)) {
		UINT16 type, length;

		type = lte_bs_get8(bs);
		length = lte_bs_get8(bs);

		if (length > lte_bstream_empty(bs)) {
			lte_tlvlist_free(&list);
			return NULL;
		}

		cur = (LteTlvList_t *)lte_malloc(sizeof(LteTlvList_t));
		if (!cur) {
			lte_tlvlist_free(&list);
			return NULL;
		}

		memset(cur, 0, sizeof(LteTlvList_t));

		cur->tlv = createtlv(type, length, NULL);
		if (!cur->tlv) {
			lte_free(cur);
			lte_tlvlist_free(&list);
			return NULL;
		}
		if (cur->tlv->length > 0) {
			cur->tlv->value = lte_bs_getraw(bs, length);
			if (!cur->tlv->value) {
				lte_freetlv(&cur->tlv);
				lte_free(cur);
				lte_tlvlist_free(&list);
				return NULL;
			}
		}

		len -= lte_tlvlist_size(&cur);
		cur->next = list;
		list = cur;
	}

	return list;
}

/**
 * Duplicate a TLV chain.
 * This is pretty self explanatory.
 *
 * @param orig The TLV chain you want to make a copy of.
 * @return A newly allocated TLV chain.
 */
  LteTlvList_t *lte_tlvlist_copy(LteTlvList_t *orig)
{
	LteTlvList_t *new = NULL;

	while (orig) {
		lte_tlvlist_add_raw(&new, orig->tlv->type, orig->tlv->length, orig->tlv->value);
		orig = orig->next;
	}

	return new;
}

/*
 * Compare two TLV lists for equality.  This probably is not the most 
 * efficient way to do this.
 *
 * @param one One of the TLV chains to compare.
 * @param two The other TLV chain to compare.
 * @return Return 0 if the lists are the same, return 1 if they are different.
 */
  INT32 lte_tlvlist_cmp(LteTlvList_t *one, LteTlvList_t *two)
{
	ByteStream_t bs1, bs2;

	if (lte_tlvlist_size(&one) != lte_tlvlist_size(&two))
		return 1;

	lte_bstream_init(&bs1, ((UINT8 *)lte_malloc(lte_tlvlist_size(&one)*sizeof(UINT8))), lte_tlvlist_size(&one));
	lte_bstream_init(&bs2, ((UINT8 *)lte_malloc(lte_tlvlist_size(&two)*sizeof(UINT8))), lte_tlvlist_size(&two));

	lte_tlvlist_write(&bs1, &one);
	lte_tlvlist_write(&bs2, &two);

	if (memcmp(bs1.data, bs2.data, bs1.len)) {
		lte_free(bs1.data);
		lte_free(bs2.data);
		return 1;
	}

	lte_free(bs1.data);
	lte_free(bs2.data);

	return 0;
}

/**
 * Free a TLV chain structure
 *
 * Walks the list of TLVs in the passed TLV chain and
 * lte_frees each one. Note that any references to this data
 * should be removed before calling this.
 *
 * @param list Chain to be lte_freed
 */
  void lte_tlvlist_free(LteTlvList_t **list)
{
	LteTlvList_t *cur;

	if (!list || !*list)
		return;

	for (cur = *list; cur; ) {
		LteTlvList_t *tmp;
		
		lte_freetlv(&cur->tlv);

		tmp = cur->next;
		lte_free(cur);
		cur = tmp;
	}

	list = NULL;

	return;
}

/**
 * Count the number of TLVs in a chain.
 *
 * @param list Chain to be counted.
 * @return The number of TLVs stored in the passed chain.
 */
  INT32 lte_tlvlist_count(LteTlvList_t **list)
{
	LteTlvList_t *cur;
	INT32 count;

	if (!list || !*list)
		return -1;

	for (cur = *list, count = 0; cur; cur = cur->next)
		count++;

	return count;
}

/**
 * Count the number of bytes in a TLV chain.
 *
 * @param list Chain to be sized
 * @return The number of bytes that would be needed to 
 *         write the passed TLV chain to a data buffer.
 */
  INT32 lte_tlvlist_size(LteTlvList_t **list)
{
	LteTlvList_t *cur;
	INT32 size;

	if (!list || !*list)
		return -1;

	for (cur = *list, size = 0; cur; cur = cur->next)
		size += (4 + cur->tlv->length);

	return size;
}

/**
 * Adds the passed string as a TLV element of the passed type
 * to the TLV chain.
 *
 * @param list Desination chain (%NULL pointer if empty).
 * @param type TLV type.
 * @param length Length of string to add (not including %NULL).
 * @param value String to add.
 * @return The size of the value added.
 */
  INT32 lte_tlvlist_add_raw(LteTlvList_t **list, const UINT16 type, const UINT16 length, const UINT8 *value)
{
	LteTlvList_t *newtlv, *cur;

	if (list == NULL)
		return -1;

	if (!(newtlv = (LteTlvList_t *)lte_malloc(sizeof(LteTlvList_t))))
		return -1;
	memset(newtlv, 0x00, sizeof(LteTlvList_t));

	if (!(newtlv->tlv = createtlv(type, length, NULL))) {
		lte_free(newtlv);
		return -1;
	}
	if (newtlv->tlv->length > 0) {
		newtlv->tlv->value = (UINT8 *)lte_malloc(newtlv->tlv->length);
		memcpy(newtlv->tlv->value, value, newtlv->tlv->length);
	}

	if (!*list)
		*list = newtlv;
	else {
		for(cur = *list; cur->next; cur = cur->next)
			;
		cur->next = newtlv;
	}

	return newtlv->tlv->length;
}

/**
 * Add a one byte integer to a TLV chain.
 *
 * @param list Destination chain.
 * @param type TLV type to add.
 * @param value Value to add.
 * @return The size of the value added.
 */
  INT32 lte_tlvlist_add_8(LteTlvList_t **list, const UINT16 type, const UINT8 value)
{
	UINT8 v8[1];

	BS_PUT8(v8, value);

	return lte_tlvlist_add_raw(list, type, 1, v8);
}

/**
 * Add a two byte integer to a TLV chain.
 *
 * @param list Destination chain.
 * @param type TLV type to add.
 * @param value Value to add.
 * @return The size of the value added.
 */
  INT32 lte_tlvlist_add_16(LteTlvList_t **list, const UINT16 type, const UINT16 value)
{
	UINT8 v16[2];

	BS_PUT16(v16, value);

	return lte_tlvlist_add_raw(list, type, 2, v16);
}

/**
 * Add a four byte integer to a TLV chain.
 *
 * @param list Destination chain.
 * @param type TLV type to add.
 * @param value Value to add.
 * @return The size of the value added.
 */
  INT32 lte_tlvlist_add_32(LteTlvList_t **list, const UINT16 type, const UINT32 value)
{
	UINT8 v32[4];

	BS_PUT32(v32, value);

	return lte_tlvlist_add_raw(list, type, 4, v32);
}

/**
 * Adds a block of capability blocks to a TLV chain. The bitfield
 * passed in should be a bitwise %OR of any of the %AIM_CAPS constants:
 *
 *     %AIM_CAPS_BUDDYICON   Supports Buddy Icons
 *     %AIM_CAPS_TALK        Supports Voice Chat
 *     %AIM_CAPS_IMIMAGE     Supports DirectIM/IMImage
 *     %AIM_CAPS_CHAT        Supports Chat
 *     %AIM_CAPS_GETFILE     Supports Get File functions
 *     %AIM_CAPS_SENDFILE    Supports Send File functions
 *
 * @param list Destination chain
 * @param type TLV type to add
 * @param caps Bitfield of capability flags to send
 * @return The size of the value added.
 */
  INT32 lte_tlvlist_add_caps(LteTlvList_t **list, const UINT16 type, const UINT32 caps)
{
	UINT8 buf[16*16]; /* XXX icky fixed length buffer */
	ByteStream_t bs;

	if (!caps)
		return -1; /* nothing there anyway */

	lte_bstream_init(&bs, buf, sizeof(buf));

//	lte_putcap(&bs, caps);

	return lte_tlvlist_add_raw(list, type, lte_bstream_curpos(&bs), buf);
}


/**
 * Adds a TLV with a zero length to a TLV chain.
 *
 * @param list Destination chain.
 * @param type TLV type to add.
 * @return The size of the value added.
 */
  INT32 lte_tlvlist_add_noval(LteTlvList_t **list, const UINT16 type)
{
	return lte_tlvlist_add_raw(list, type, 0, NULL);
}

/*
 * Note that the inner TLV chain will not be modifiable as a tlvchain once
 * it is written using this.  Or rather, it can be, but updates won't be
 * made to this.
 *
 * XXX should probably support sublists for real.
 * 
 * This is so neat.
 *
 * @param list Destination chain.
 * @param type TLV type to add.
 * @param t1 The TLV chain you want to write.
 * @return The number of bytes written to the destination TLV chain.
 *         0 is returned if there was an error or if the destination
 *         TLV chain has length 0.
 */
  INT32 lte_tlvlist_add_frozentlvlist(LteTlvList_t **list, UINT16 type, LteTlvList_t **tl)
{
	UINT8 *buf;
	INT32 buflen;
	ByteStream_t bs;

	buflen = lte_tlvlist_size(tl);

	if (buflen <= 0)
		return -1;

	if (!(buf = lte_malloc(buflen)))
		return -1;

	lte_bstream_init(&bs, buf, buflen);

	lte_tlvlist_write(&bs, tl);

	lte_tlvlist_add_raw(list, type, lte_bstream_curpos(&bs), buf);

	lte_free(buf);

	return buflen;
}

/**
 * Substitute a TLV of a given type with a new TLV of the same type.  If 
 * you attempt to replace a TLV that does not exist, this function will 
 * just add a new TLV as if you called lte_tlvlist_add_raw().
 *
 * @param list Desination chain (%NULL pointer if empty).
 * @param type TLV type.
 * @param length Length of string to add (not including %NULL).
 * @param value String to add.
 * @return The length of the TLV.
 */
  INT32 lte_tlvlist_replace_raw(LteTlvList_t **list, const UINT16 type, const UINT16 length, const UINT8 *value)
{
	LteTlvList_t *cur;

	if (list == NULL)
		return -1;

	for (cur = *list; ((cur != NULL) && (cur->tlv->type != type)); cur = cur->next);
	if (cur == NULL)
		return lte_tlvlist_add_raw(list, type, length, value);

	lte_free(cur->tlv->value);
	cur->tlv->length = length;
	if (cur->tlv->length > 0) {
		cur->tlv->value = (UINT8 *)lte_malloc(cur->tlv->length);
		memcpy(cur->tlv->value, value, cur->tlv->length);
	} else
		cur->tlv->value = NULL;

	return cur->tlv->length;
}

/**
 * Substitute a TLV of a given type with a new TLV of the same type.  If 
 * you attempt to replace a TLV that does not exist, this function will 
 * just add a new TLV as if you called lte_tlvlist_add_raw().
 *
 * @param list Desination chain (%NULL pointer if empty).
 * @param type TLV type.
 * @return The length of the TLV.
 */
  INT32 lte_tlvlist_replace_noval(LteTlvList_t **list, const UINT16 type)
{
	return lte_tlvlist_replace_raw(list, type, 0, NULL);
}

/**
 * Substitute a TLV of a given type with a new TLV of the same type.  If 
 * you attempt to replace a TLV that does not exist, this function will 
 * just add a new TLV as if you called lte_tlvlist_add_raw().
 *
 * @param list Desination chain (%NULL pointer if empty).
 * @param type TLV type.
 * @param value 8 bit value to add.
 * @return The length of the TLV.
 */
  INT32 lte_tlvlist_replace_8(LteTlvList_t **list, const UINT16 type, const UINT8 value)
{
	UINT8 v8[1];

	BS_PUT8(v8, value);

	return lte_tlvlist_replace_raw(list, type, 1, v8);
}

/**
 * Substitute a TLV of a given type with a new TLV of the same type.  If 
 * you attempt to replace a TLV that does not exist, this function will 
 * just add a new TLV as if you called lte_tlvlist_add_raw().
 *
 * @param list Desination chain (%NULL pointer if empty).
 * @param type TLV type.
 * @param value 32 bit value to add.
 * @return The length of the TLV.
 */
  INT32 lte_tlvlist_replace_32(LteTlvList_t **list, const UINT16 type, const UINT32 value)
{
	UINT8 v32[4];

	BS_PUT32(v32, value);

	return lte_tlvlist_replace_raw(list, type, 4, v32);
}

/**
 * Remove a TLV of a given type.  If you attempt to remove a TLV that 
 * does not exist, nothing happens.
 *
 * @param list Desination chain (%NULL pointer if empty).
 * @param type TLV type.
 */
  void lte_tlvlist_remove(LteTlvList_t **list, const UINT16 type)
{
	LteTlvList_t *del;

	if (!list || !(*list))
		return;

	/* Remove the item from the list */
	if ((*list)->tlv->type == type) {
		del = *list;
		*list = (*list)->next;
	} else {
		LteTlvList_t *cur;
		for (cur=*list; (cur->next && (cur->next->tlv->type!=type)); cur=cur->next);
		if (!cur->next)
			return;
		del = cur->next;
		cur->next = del->next;
	}

	/* Free the removed item */
	lte_free(del->tlv->value);
	lte_free(del->tlv);
	lte_free(del);
}

/**
 * Write a TLV chain into a data buffer.
 *
 * Copies a TLV chain into a raw data buffer, writing only the number
 * of bytes specified. This operation does not lte_free the chain; 
 * lte_tlvlist_lte_free() must still be called to lte_free up the memory used
 * by the chain structures.
 *
 * XXX clean this up, make better use of bstreams 
 *
 * @param bs Input bstream
 * @param list Source TLV chain
 * @return Return 0 if the destination bstream is too small.
 */
  INT32 lte_tlvlist_write(ByteStream_t *bs, LteTlvList_t **list)
{
	INT32 goodbuflen;
	LteTlvList_t *cur;

	/* do an initial run to test total length */
	goodbuflen = lte_tlvlist_size(list);

	if (goodbuflen > lte_bstream_empty(bs))
		return -1; /* not enough buffer */

	/* do the real write-out */
	for (cur = *list; cur; cur = cur->next) {
		lte_bs_put8(bs, cur->tlv->type);
		lte_bs_put8(bs, cur->tlv->length);
		if (cur->tlv->length)
			lte_bs_putraw(bs, cur->tlv->value, cur->tlv->length);
	}

	return 0; /* XXX this is a nonsensical return */
}


/**
 * Grab the Nth TLV of type type in the TLV list list.
 *
 * Returns a pointer to an LteTlv_t of the specified type; 
 * %NULL on error.  The @nth parameter is specified starting at %1.
 * In most cases, there will be no more than one TLV of any type
 * in a chain.
 *
 * @param list Source chain.
 * @param type Requested TLV type.
 * @param nth Index of TLV of type to get.
 * @return The TLV you were looking for, or NULL if one could not be found.
 */
  LteTlv_t *lte_tlv_gettlv(LteTlvList_t *list, const UINT16 type, const INT32 nth)
{
	LteTlvList_t *cur;
	INT32 i;

	for (cur = list, i = 0; cur; cur = cur->next) {
		if (cur && cur->tlv) {
			if (cur->tlv->type == type)
				i++;
			if (i >= nth)
				return cur->tlv;
		}
	}

	return NULL;
}


  LteTlv_t *lte_tlv_get_nth(LteTlvList_t *list, const INT32 nth)
{
	LteTlvList_t *cur;
	INT32 i;

	for (cur = list, i = 0; cur; cur = cur->next) {
		if (cur && cur->tlv) {
			if (i == nth){
				return cur->tlv;
			}else {
                i++;
			}
		}
	}

	return NULL;
}

/**
 * Retrieve the data from the nth TLV in the given TLV chain as a string.
 *
 * @param list Source TLV chain.
 * @param type TLV type to search for.
 * @param nth Index of TLV to return.
 * @return The value of the TLV you were looking for, or NULL if one could 
 *         not be found.  This is a dynamic buffer and must be lte_freed by the 
 *         caller.
 */
  INT8 *lte_tlv_getstr(LteTlvList_t *list, const UINT16 type, const INT32 nth)
{
	LteTlv_t *tlv;
	INT8 *newstr;

	if (!(tlv = lte_tlv_gettlv(list, type, nth)))
		return NULL;

	newstr = (INT8 *) lte_malloc(tlv->length + 1);
	memcpy(newstr, tlv->value, tlv->length);
	newstr[tlv->length] = '\0';

	return newstr;
}

/**
 * Retrieve the data from the nth TLV in the given TLV chain as an 8bit 
 * integer.
 *
 * @param list Source TLV chain.
 * @param type TLV type to search for.
 * @param nth Index of TLV to return.
 * @return The value the TLV you were looking for, or 0 if one could 
 *         not be found.
 */
  UINT8 lte_tlv_get8(LteTlvList_t *list, const UINT16 type, const INT32 nth)
{
	LteTlv_t *tlv;

	if (!(tlv = lte_tlv_gettlv(list, type, nth)))
		return -1; /* erm */
	return BS_GET8(tlv->value);
}

/**
 * Retrieve the data from the nth TLV in the given TLV chain as a 16bit 
 * integer.
 *
 * @param list Source TLV chain.
 * @param type TLV type to search for.
 * @param nth Index of TLV to return.
 * @return The value the TLV you were looking for, or 0 if one could 
 *         not be found.
 */
  UINT16 lte_tlv_get16(LteTlvList_t *list, const UINT16 type, const INT32 nth)
{
	LteTlv_t *tlv;

	if (!(tlv = lte_tlv_gettlv(list, type, nth)))
		return -1; /* erm */
	return BS_GET16(tlv->value);
}


/**
 * Retrieve the data from the nth TLV in the given TLV chain as a 32bit 
 * integer.
 *
 * @param list Source TLV chain.
 * @param type TLV type to search for.
 * @param nth Index of TLV to return.
 * @return The value the TLV you were looking for, or 0 if one could 
 *         not be found.
 */
UINT32 lte_tlv_get32(LteTlvList_t *list, const UINT16 type, const INT32 nth)
{
	LteTlv_t *tlv;

	if (!(tlv = lte_tlv_gettlv(list, type, nth)))
		return -1; /* erm */
	return BS_GET32(tlv->value);
}


UINT32 lte_tlv_get32_nth(LteTlvList_t *list, const INT32 nth)
  {
      LteTlv_t *tlv;
  
      if (!(tlv = lte_tlv_get_nth(list,nth)))
          return -1; /* erm */
      return BS_GET32(tlv->value);
  }

UINT16 lte_tlv_get16_nth(LteTlvList_t *list, const INT32 nth)
{
	LteTlv_t *tlv;

	if (!(tlv = lte_tlv_get_nth(list,nth)))
		return -1; /* erm */
	return BS_GET16(tlv->value);
}

UINT8 lte_tlv_get8_nth(LteTlvList_t *list, const INT32 nth)
{
	LteTlv_t *tlv;

	if (!(tlv = lte_tlv_get_nth(list,nth)))
		return -1; /* erm */
	return BS_GET8(tlv->value);
}

