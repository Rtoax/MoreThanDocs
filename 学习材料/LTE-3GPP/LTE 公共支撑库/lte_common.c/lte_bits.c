/*******************************************************************************
 ** 
 ** Copyright (c) 2006-20010 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: mac_bits.c
 ** Description: Source file for bits operation.
 **
 ** Current Version: 1.0
 ** $Revision: 1.1 $
 ** Author: Jiangtao.Dong (dongjiangtao@ict.ac.cn)
 ** Date: 2007.05.16
 **
 ******************************************************************************/


/* Dependencies ------------------------------------------------------------- */
#include <stdio.h>
#include "lte_bits.h"

/* Constants ---------------------------------------------------------------- */

/* Types -------------------------------------------------------------------- */

/* Macros ------------------------------------------------------------------- */

/* Globals ------------------------------------------------------------------ */

UINT8 g_bits_mask[9] = {0x00, 0x01, 0x03, 0x07, 0x0f, 0x1f, 0x3f, 0x7f, 0xff};

/* Functions ---------------------------------------------------------------- */
/*******************************************************************************
 * 
 * Input: 	x: a signed integer
 *		y: a unsigned integer
 * Output: return  x of the y-th power
 ******************************************************************************/
static inline UINT32 bits_pow(INT32 x, UINT32 y)
{
	UINT32 ret = 1;
	
	while (y-- > 0) {
		ret *= x;
	}

	return ret;
}

/*******************************************************************************
 * Convert length of bits to length of bytes.
 *
 * Input: bit_len : The lenght of bits.
 *
 * Output: return the length of bytes.
 ******************************************************************************/
UINT32 convert_bitlen_to_bytelen(UINT32 bit_len)
{
	return ((bit_len & 7) == 0) ? (bit_len >> 3) : ((bit_len >> 3) + 1);
}


/*******************************************************************************
 * Get the length of message.
 *
 * Input: bit_p : The pointer to struct bit operation.
 *
 * Output: return the length of message, or -1 when filed.
 ******************************************************************************/
INT32 get_bits_msg_len(BitOpt *bit_p, UINT32 opt_type)
{
	if (bit_p == NULL) {
		return -1;
	} else {
		if (opt_type == BIT_PACKED) {
			return bit_p->msg_len;
		} else if (opt_type == BIT_UNPACKED) {
			return (bit_p->msg_len - (bit_p->bit_offset / 8));
		} else {
			printf("Not support the Bit operation type!\n");
			return -1;
		}
	}
}

/*******************************************************************************
 * Show every bits of memory.
 *
 * Input: msg_p: The start pointer of memory.
 * 		  msg_len: The length of memory.
 * 		  lable: The lable of the memory.
 *
 * Output: None.
 ******************************************************************************/
void show_bits(UINT8 *msg_p, UINT32 msg_len, char *name)
{
	int byte;
	UINT8 bit, mask;

    if (msg_p == NULL) {
        printf("The pointer is NULL in show_bits()\n");
        return;
    }

    if (name != NULL) {
        printf("The bits of memory %s is:\n", name);
    } else {
        printf("The bits of memory is:\n");
    }

    for (byte = 0; byte < msg_len; byte++) {
		for (bit = 0, mask = 0x80; bit < 8; bit++) {
			if (*(msg_p + byte) & (mask >> bit)) {
				printf("1");
			} else {
				printf("0");
			}
		}
		printf(" ");
			
        if ((byte + 1) % 8 == 0) {
            printf("\n");
        }
    }
    printf("\n");
}

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
INT32 init_bits(BitOpt *bit_p, UINT8 *msg_p, UINT32 msg_len, UINT8 bit_offset)
{
	if ((msg_p == NULL) || (bit_p == NULL)) {
		return -1;
	}
	
	bit_p->msg_p = msg_p;
	bit_p->bit_offset = bit_offset;
	bit_p->msg_len = msg_len;
		
	return 0;
}

/*******************************************************************************
 * Skip n bits in packed or unpacked operation. Pack operation, not insure the 
 * validity of the bits that ware sikped.
 *
 * Input: bit_p : The pointer to struct bit operation.
 *        n : The number of bits will be skiped.
 *
 * Output: return 0, or -1 when filed.
 ******************************************************************************/
INT32 skip_bits(BitOpt *bit_p, UINT32 n, UINT8 opt_type)
{
	if (bit_p == NULL) {
		return -1;
	}

	if (opt_type == BIT_PACKED) {	/* Skip bits when packing bits. */
		bit_p->bit_offset += n;
		bit_p->msg_len = convert_bitlen_to_bytelen(bit_p->bit_offset);
	} else if (opt_type == BIT_UNPACKED) {	/* Skip bits when unpacking bits */
		bit_p->bit_offset += n;
		if (bit_p->msg_len < convert_bitlen_to_bytelen(bit_p->bit_offset)) {
			printf("skip_bits() err! -- %d -- is too large\n", n);
			printf("bit->msg_len is %d, bit->bit_offset is %d, n is %d\n",bit_p->msg_len,bit_p->bit_offset,n);
			bit_p->bit_offset -= n;
			
			return -1;
		}
	} else {
		printf("skip_bits() err! Unknon bits operation type! \n");
		return -1;
	}
	
	return 0;
}

/*******************************************************************************
 * Unpack n bits from message, not support 32 bits or more.
 *
 * Input: bit_p : The pointer to struct bit operation.
 *        n : The number of bits will be unpacked.
 *        msg_len : The length of message.
 *
 * Output: return the unpacked value, or -1 when error occur.
 ******************************************************************************/
INT32 unpack_bits(BitOpt *bit_p, UINT32 n)
{
	UINT8 bit_used, bit_left;
	UINT8 *current_byte_p;
	UINT32 ret;

	if (bit_p == NULL) {
		printf("Pointer is NULL in unpack_bits()\n");
		return -1;
	}
	
	if (n > 31) {
		printf("system not support bit unpacked more than 31 bits !\n");
		return -1;
	}
	
	if (bit_p->bit_offset + n > (bit_p->msg_len << 3)) {
		printf("Unpack bits out boundary!!\n");
		printf("bit_p->bit_offset is %d, bit_p->msg_len is %d, n is %d\n",bit_p->bit_offset,bit_p->msg_len,n);
		return -1;
	}
	
	current_byte_p = bit_p->msg_p + (bit_p->bit_offset >> 3);
	bit_used = bit_p->bit_offset & 7;
	bit_left = 8 - bit_used;

	bit_p->bit_offset += n;
#if 0
printf("current_byte_p: %x, bit_used: %d, bit_left: %d, bit_offset: %d, n: %d\n", current_byte_p, bit_used, bit_left, bit_p->bit_offset, n);
#endif

	if (n <= bit_left) {
		/*
		 * All bits within current byte.
		 */
		ret = (*current_byte_p >> (bit_left - n)) & g_bits_mask[n];
	} else {
		/*
		 * Bits wanted in this and at least some of next byte.
         * Get left bits left in current byte 1st- all high bits wanted.
		 */
		ret = *current_byte_p++ & g_bits_mask[bit_left];
		n -= bit_left;

		/* Get whole bytes. */
		while (n >= 8) {
			ret = (ret << 8) + *current_byte_p++;
			n -= 8;
		}
		
        /* Get remaining n bits (n < 8), masking out unwanted high bits. */
		if (n > 0) {
			ret = (ret << n) + ((*current_byte_p >> (8-n)) & g_bits_mask[n]);
		}
		
	}

	return ret;
}

/*******************************************************************************
 * Pack the value into message with n bits, not support more than 32 bits.
 *
 * Input: bit_p : The pointer to struct bit operation.
 *        n : The number of bits will be packed.
 *        value : The value need pacded into message.
 *
 * Output: return the length of message after packed, or -1 when filed.
 ******************************************************************************/
INT32 pack_bits(BitOpt *bit_p, UINT32 n, UINT32 value)
{
	UINT8 bit_used, bit_left;
	UINT8 *current_byte_p;

	if ((bit_p == NULL) || (n > 31)) {
		printf("Bits pack error!\n");
		return -1;
	}

	if (value >= bits_pow(2, n)) {
		printf("Value out flow your bits!!!\n");
		return -1;
	}
	
	current_byte_p = bit_p->msg_p + (bit_p->bit_offset / 8);
	bit_used = bit_p->bit_offset % 8;
	bit_left = 8 - bit_used;

	bit_p->bit_offset += n;
	bit_p->msg_len = convert_bitlen_to_bytelen(bit_p->bit_offset);

#if 0
printf("value: %x, bit_used: %d, bit_left: %d, bit_offset: %d, n: %d\n", value, bit_used, bit_left, bit_p->bit_offset, n);
#endif
	if (n <= bit_left) {
		/*
		 * All bits can packed into current byte.
		 */
		/**current_byte_p += (value >> bit_used) & g_bits_mask[n];*/
		*current_byte_p += (value & g_bits_mask[n]) << (bit_left - n);
	} else {
		/*
		 * value can't packed into left bits in current byte,   
         * pack the the bit_left high bits in to bit_left low bits of current
		 * byte, and increment byte pointer afterwards.
		 */
		*current_byte_p++ += (value >> (n - bit_left)) & g_bits_mask[bit_left];
		n -= bit_left;

		while (n >= 8) {
			*current_byte_p++ = (value >> (n - 8)) & g_bits_mask[8];
			n -= 8;
		}

		/* Pack remaining n bits (n < 8) into current byte. */
		if (n > 0) {
			*current_byte_p = (value & g_bits_mask[n]) << (8 - n);
		}
	}
	
	return 0;
}



