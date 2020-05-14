/*******************************************************************************
 ** 
 ** Copyright (c) 2006-2010 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: lte_bits.h
 ** Description: Header file for bits operation.
 **
 ** Current Version: 1.0
 ** Author: Jiangtao.Dong (dongjiangtao@ict.ac.cn)
 ** Date: 2007.05.15
 **
 ******************************************************************************/


#ifndef BITS_H
#define BITS_H

/* Dependencies ------------------------------------------------------------- */
#include "lte_type.h"

/* Constants ---------------------------------------------------------------- */

/*
 * The Bits operation type.
 */
typedef enum {
	BIT_PACKED,		/* Packed bits. */
	BIT_UNPACKED	/* Unpacked bits. */
} BitOptType;

/* Types -------------------------------------------------------------------- */

/*
 * Type of Bits for pack message operation.
 */
typedef struct {
	UINT8 *msg_p;		/* Point to the message whitch packed or unpacked */
	UINT32 bit_offset;		/* The bit offset from the start of msg_p. */
	UINT32 msg_len;			/* The total length of the message. */
} BitOpt;

#if 0
/*
 * Type of Bits for unpack message operation.
 */
typedef struct {
	const UINT8 *msg_p;		/* Point to the message whitch packed or unpacked */
	UINT32 msg_len;			/* The total length of the message. */
	UINT32 bit_offset;		/* The bit offset from the start of msg_p. */
	UINT8 bit_used;			/* Used bits in the last byte. */
} BitOpt;
#endif

/* Macros ------------------------------------------------------------------- */
#ifdef __LITTLE_ENDIAN__	
/* x86 */
#define SWAP_16(x) 		\
	((unsigned short int)(						\
	(((unsigned short int)(x) & 0x00ff)<<8)| 	\
	(((unsigned short int)(x) & 0xff00)>>8)))	
    
#define SWAP_32(x)		\
	((unsigned long int)(							\
	(((unsigned long int)(x) & 0x000000ff)<<24)|	\
	(((unsigned long int)(x) & 0x0000ff00)<<8)|		\
	(((unsigned long int)(x) & 0x00ff0000)>>8)|		\
	(((unsigned long int)(x) & 0xff000000)>>24)))
#else
/* big endian */
#define SWAP_16(x)
#define SWAP_32(x)
#endif

//by lzl 20140730
#define SWAB_BIT_4(x) 		\
		((unsigned long int)(							\
		(((unsigned long int)(x) & 0x01)<<3)|	\
		(((unsigned long int)(x) & 0x02)<<1)|		\
		(((unsigned long int)(x) & 0x04)>>1)|		\
		(((unsigned long int)(x) & 0x08)>>3)))

#define SWAP_BIT_8(x)		\
		((unsigned long int)(						\
		(((unsigned long int)(x) & 0x01)<<7)|		\
		(((unsigned long int)(x) & 0x02)<<5)|		\
		(((unsigned long int)(x) & 0x04)<<3)|		\
		(((unsigned long int)(x) & 0x08)<<1)|		\
		(((unsigned long int)(x) & 0x10)>>1)|		\
		(((unsigned long int)(x) & 0x20)>>3)|		\
		(((unsigned long int)(x) & 0x40)>>5)|		\
		(((unsigned long int)(x) & 0x80)>>7)))
/* Globals ------------------------------------------------------------------ */

/* Functions ---------------------------------------------------------------- */
/*******************************************************************************
 * Convert length of bits to length of bytes.
 *
 * Input: bit_len : The lenght of bits.
 *
 * Output: return the length of bytes.
 ******************************************************************************/
extern  UINT32 convert_bitlen_to_bytelen(UINT32 bit_len);

/*******************************************************************************
 * Get the length of message.
 *
 * Input: bit_p : The pointer to struct bit operation.
 *
 * Output: return the length of message, or -1 when filed.
 ******************************************************************************/
extern  INT32 get_bits_msg_len(BitOpt *bit_p, UINT32 opt_type);


/*******************************************************************************
 * Show every bits of memory.
 *
 * Input: msg_p: The start pointer of memory.
 * 		  msg_len: The length of memory.
 * 		  lable: The lable of the memory.
 *
 * Output: None.
 ******************************************************************************/
extern  void show_bits(UINT8 *msg_p, UINT32 msg_len, char *name);

/*******************************************************************************
 * Initial the struct of BitOpt.
 *
 * Input: bit_p : The pointer to struct bit operation.
 *        msg_p : Point to the message, that will be packed or unpacked.
 *        msg_len : The length of message.
 *        bit_offset : The bit offset from the start of msg_p;
 *
 * Output: return 0, or -1 when filed.
 ******************************************************************************/
extern  INT32 init_bits(BitOpt *bit_p, UINT8 *msg_p, UINT32 msg_len, UINT8 bit_offset);

/*******************************************************************************
 * Skip n bits in packed or unpacked operation. Pack operation, not insure the 
 * validity of the bits that ware sikped.
 *
 * Input: bit_p : The pointer to struct bit operation.
 *        n : The number of bits will be skiped.
 *
 * Output: return 0, or -1 when filed.
 ******************************************************************************/
extern  INT32 skip_bits(BitOpt *bit_p, UINT32 n, UINT8 opt_type);

/*******************************************************************************
 * Unpack n bits from message, not support 32 bits or more.
 *
 * Input: bit_p : The pointer to struct bit operation.
 *        n : The number of bits will be unpacked.
 *        msg_len : The length of message.
 *
 * Output: return the unpacked value, or -1 when error occur.
 ******************************************************************************/
extern  INT32 unpack_bits(BitOpt *bit_p, UINT32 n);

/*******************************************************************************
 * Pack the value into message with n bits, not support more than 32 bits.
 *
 * Input: bit_p : The pointer to struct bit operation.
 *        n : The number of bits will be packed.
 *        value : The value need pacded into message.
 *
 * Output: return the length of message after packed, or -1 when filed.
 ******************************************************************************/
extern  INT32 pack_bits(BitOpt *bit_p, UINT32 n, UINT32 value);

#endif	/* BITS_H */
