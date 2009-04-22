/*
 * Synopsis : Error Correction Codes (ECC) Algorithms.
 *
 * Copyright (c) 2008 STMicroelectronics Limited.  All right reserved.
 *
 * See stm_nand_ecc.h for a description of this module.
 *
 *  May be copied or modified under the terms of the GNU General Public
 *  License Version 2.0 only.  See linux/COPYING for more information.
 *
 * Changelog:
 *    2009-02-25 Angus Clark <angus.clark@st.com>
 *
 *        - Renamed, formatted, and edited for linux compatibility
 *
 *        - Added clause in stm_ecc_correct() to hanlde inconsistency between
 *          data and expected ecc for freshly erased page.
 *
 */

#include <linux/types.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include "stm_nand_ecc.h"

static const uint8_t byte_parity_table[] =   /* Parity look up table */
{
  0x00, 0x2B, 0x2D, 0x06, 0x33, 0x18, 0x1E, 0x35,
  0x35, 0x1E, 0x18, 0x33, 0x06, 0x2D, 0x2B, 0x00,
  0x4B, 0x60, 0x66, 0x4D, 0x78, 0x53, 0x55, 0x7E,
  0x7E, 0x55, 0x53, 0x78, 0x4D, 0x66, 0x60, 0x4B,
  0x4D, 0x66, 0x60, 0x4B, 0x7E, 0x55, 0x53, 0x78,
  0x78, 0x53, 0x55, 0x7E, 0x4B, 0x60, 0x66, 0x4D,
  0x06, 0x2D, 0x2B, 0x00, 0x35, 0x1E, 0x18, 0x33,
  0x33, 0x18, 0x1E, 0x35, 0x00, 0x2B, 0x2D, 0x06,
  0x53, 0x78, 0x7E, 0x55, 0x60, 0x4B, 0x4D, 0x66,
  0x66, 0x4D, 0x4B, 0x60, 0x55, 0x7E, 0x78, 0x53,
  0x18, 0x33, 0x35, 0x1E, 0x2B, 0x00, 0x06, 0x2D,
  0x2D, 0x06, 0x00, 0x2B, 0x1E, 0x35, 0x33, 0x18,
  0x1E, 0x35, 0x33, 0x18, 0x2D, 0x06, 0x00, 0x2B,
  0x2B, 0x00, 0x06, 0x2D, 0x18, 0x33, 0x35, 0x1E,
  0x55, 0x7E, 0x78, 0x53, 0x66, 0x4D, 0x4B, 0x60,
  0x60, 0x4B, 0x4D, 0x66, 0x53, 0x78, 0x7E, 0x55,
  0x55, 0x7E, 0x78, 0x53, 0x66, 0x4D, 0x4B, 0x60,
  0x60, 0x4B, 0x4D, 0x66, 0x53, 0x78, 0x7E, 0x55,
  0x1E, 0x35, 0x33, 0x18, 0x2D, 0x06, 0x00, 0x2B,
  0x2B, 0x00, 0x06, 0x2D, 0x18, 0x33, 0x35, 0x1E,
  0x18, 0x33, 0x35, 0x1E, 0x2B, 0x00, 0x06, 0x2D,
  0x2D, 0x06, 0x00, 0x2B, 0x1E, 0x35, 0x33, 0x18,
  0x53, 0x78, 0x7E, 0x55, 0x60, 0x4B, 0x4D, 0x66,
  0x66, 0x4D, 0x4B, 0x60, 0x55, 0x7E, 0x78, 0x53,
  0x06, 0x2D, 0x2B, 0x00, 0x35, 0x1E, 0x18, 0x33,
  0x33, 0x18, 0x1E, 0x35, 0x00, 0x2B, 0x2D, 0x06,
  0x4D, 0x66, 0x60, 0x4B, 0x7E, 0x55, 0x53, 0x78,
  0x78, 0x53, 0x55, 0x7E, 0x4B, 0x60, 0x66, 0x4D,
  0x4B, 0x60, 0x66, 0x4D, 0x78, 0x53, 0x55, 0x7E,
  0x7E, 0x55, 0x53, 0x78, 0x4D, 0x66, 0x60, 0x4B,
  0x00, 0x2B, 0x2D, 0x06, 0x33, 0x18, 0x1E, 0x35,
  0x35, 0x1E, 0x18, 0x33, 0x06, 0x2D, 0x2B, 0x00
};

static const uint8_t  bit_count_table[] =   /* Parity look up table */
{
  0, 1, 1, 2, 1, 2, 2, 3,
  1, 2, 2, 3, 2, 3, 3, 4,
  1, 2, 2, 3, 2, 3, 3, 4,
  2, 3, 3, 4, 3, 4, 4, 5,
  1, 2, 2, 3, 2, 3, 3, 4,
  2, 3, 3, 4, 3, 4, 4, 5,
  2, 3, 3, 4, 3, 4, 4, 5,
  3, 4, 4, 5, 4, 5, 5, 6,
  1, 2, 2, 3, 2, 3, 3, 4,
  2, 3, 3, 4, 3, 4, 4, 5,
  2, 3, 3, 4, 3, 4, 4, 5,
  3, 4, 4, 5, 4, 5, 5, 6,
  2, 3, 3, 4, 3, 4, 4, 5,
  3, 4, 4, 5, 4, 5, 5, 6,
  3, 4, 4, 5, 4, 5, 5, 6,
  4, 5, 5, 6, 5, 6, 6, 7,
  1, 2, 2, 3, 2, 3, 3, 4,
  2, 3, 3, 4, 3, 4, 4, 5,
  2, 3, 3, 4, 3, 4, 4, 5,
  3, 4, 4, 5, 4, 5, 5, 6,
  2, 3, 3, 4, 3, 4, 4, 5,
  3, 4, 4, 5, 4, 5, 5, 6,
  3, 4, 4, 5, 4, 5, 5, 6,
  4, 5, 5, 6, 5, 6, 6, 7,
  2, 3, 3, 4, 3, 4, 4, 5,
  3, 4, 4, 5, 4, 5, 5, 6,
  3, 4, 4, 5, 4, 5, 5, 6,
  4, 5, 5, 6, 5, 6, 6, 7,
  3, 4, 4, 5, 4, 5, 5, 6,
  4, 5, 5, 6, 5, 6, 6, 7,
  4, 5, 5, 6, 5, 6, 6, 7,
  5, 6, 6, 7, 6, 7, 7, 8
};

/******************************************************************************/
#define COL_LOOP_STEP(c__f, c__e, c__o, c__t)		\
	do {						\
		c__o ^= (c__f ? c__t : 1);		\
		c__e ^= (c__f ? 1 : c__t);		\
	} while (0)

/* Generate 3 byte ECC code for ecc_size block p_data.
   "p_data" is a pointer to the date and must be 4-byte aligned.
   "size" gives length of "p_data" - one of enum ecc_size.
*/
void stm_ecc_gen(const uint8_t *p_data, uint8_t *ecc, enum ecc_size size)
{
	uint32_t *p_data_long = (uint32_t *)p_data;
	uint32_t parity_bits[18];  /* maximum number */
	uint32_t reg32;
	uint32_t temp;
	uint32_t int_cnt;
	uint32_t bit_cnt;

	unsigned int num_parity_bits;

	uint8_t *p_byt;
	uint8_t byte_reg;
	uint8_t byte_a;
	uint8_t byte_b;
	uint8_t byte_c;
	uint8_t byte_d;

	ecc[0] = 0;
	ecc[1] = 0;
	ecc[2] = 0;

	switch (size) {
	case ECC_128:
		num_parity_bits = 14;
		break;
	case ECC_256:
		num_parity_bits = 16;
		break;
	case ECC_512:
		num_parity_bits = 18;
		break;
	default:
		printk(KERN_ERR "Internal error in ecc_gen: unknown format\n");
		BUG();
		return;
	}

	/* Initialize variables */
	byte_reg = 0;
	reg32 = 0;

	for (bit_cnt = 0; bit_cnt < num_parity_bits; bit_cnt ++)
		parity_bits[bit_cnt] = 0;

	/* Build up column parity */
	for (int_cnt = 0; int_cnt < size/sizeof(uint32_t); int_cnt++) {
		temp = p_data_long[int_cnt];

		switch (size) {
		case ECC_512:
			COL_LOOP_STEP((int_cnt & 0x40), parity_bits[16],
				      parity_bits[17], temp);
			/* fall through */
		case ECC_256:
			COL_LOOP_STEP((int_cnt & 0x20), parity_bits[14],
				      parity_bits[15], temp);
			/* fall through */
		case ECC_128:
			COL_LOOP_STEP((int_cnt & 0x01), parity_bits[4],
				      parity_bits[5], temp);
			COL_LOOP_STEP((int_cnt & 0x02), parity_bits[6],
				      parity_bits[7], temp);
			COL_LOOP_STEP((int_cnt & 0x04), parity_bits[8],
				      parity_bits[9], temp);
			COL_LOOP_STEP((int_cnt & 0x08), parity_bits[10],
				      parity_bits[11], temp);
			COL_LOOP_STEP((int_cnt & 0x10), parity_bits[12],
				      parity_bits[13], temp);
		}
	}

	reg32 = parity_bits[12] ^ parity_bits[13];

	p_byt = (uint8_t *)&reg32;
#if __LITTLE_ENDIAN__
	byte_a = p_byt[3];
	byte_b = p_byt[2];
	byte_c = p_byt[1];
	byte_d = p_byt[0];
#else
	byte_a = p_byt[0];
	byte_b = p_byt[1];
	byte_c = p_byt[2];
	byte_d = p_byt[3];
#endif

	byte_reg = byte_a ^ byte_b ^ byte_c ^ byte_d;
	byte_reg = byte_parity_table[byte_reg] >> 1;

	/* Create line parity */
	parity_bits[0] = byte_d ^ byte_b;
	parity_bits[1] = byte_c ^ byte_a;
	parity_bits[2] = byte_d ^ byte_c;
	parity_bits[3] = byte_b ^ byte_a;

	for (bit_cnt = 4; bit_cnt < num_parity_bits; bit_cnt++) {
		p_byt = (uint8_t *)(parity_bits + bit_cnt);
		/* NB Only LS Byte of parity_bits used from now on */
		p_byt[0] ^= (p_byt[1] ^ p_byt[2] ^ p_byt[3]);
	}

	/* Calculate final ECC code */
	for (bit_cnt = 0; bit_cnt < 8; bit_cnt++)
		ecc[0] |=
			(byte_parity_table[(uint8_t)parity_bits[bit_cnt]] &
			 0x01) << bit_cnt;
	for (; bit_cnt < 16 && bit_cnt < num_parity_bits; bit_cnt++)
		ecc[1] |=
			(byte_parity_table[(uint8_t)parity_bits[bit_cnt]] &
			 0x01) << (bit_cnt - 8);
	for (; bit_cnt < num_parity_bits; bit_cnt++)
		ecc[2] |=
			(byte_parity_table[(uint8_t)parity_bits[bit_cnt]] &
			 0x01) << (bit_cnt - 16);

	ecc[2] = (uint8_t)(byte_reg << 2) | (ecc[2] & 0x03);
}
EXPORT_SYMBOL_GPL(stm_ecc_gen);

/******************************************************************************/
/* Detect and correct a 1 bit error in a 128, 256 or 512 byte block.
   "p_data" is a pointer to the data.
   "old_ecc" is the proper ECC for the data.
   "new_ecc" is the ECC generated from the (possibly) corrupted data.
   The size of the block is given in "size".

   Returns whether the data needed correcting, or was not correctable.
   If the result code is E_D1_CHK, then the data will have been modified.
 */
enum ecc_check stm_ecc_correct(uint8_t *p_data,
			       uint8_t *old_ecc,
			       uint8_t *new_ecc,
			       enum ecc_size size)
{
	uint8_t bit_cnt02;
	uint8_t bit_addr02;
	unsigned int byte_addr02;

	uint8_t ecc_xor[3];

	uint8_t error_bit_count;

	switch (size) {
	case ECC_128:
		error_bit_count = 10;
		break;
	case ECC_256:
		error_bit_count = 11;
		break;
	case ECC_512:
		error_bit_count = 12;
		break;
	default:
		printk(KERN_ERR "Internal error in ecc_gen: unknown format\n");
		BUG();
		return E_UN_CHK;
	}

	/* Basic Error Detection phase */
	ecc_xor[0] = new_ecc[0] ^ old_ecc[0];
	ecc_xor[1] = new_ecc[1] ^ old_ecc[1];
	ecc_xor[2] = new_ecc[2] ^ old_ecc[2];

	if ((ecc_xor[0] | ecc_xor[1] | ecc_xor[2]) == 0) {
		return E_NO_CHK;  /* No errors */
	}
	/* If we get here then there were errors */
	if (ecc_xor[0] == 0xff &&
	    ecc_xor[1] == 0xff &&
	    ecc_xor[2] == 0xff) {
		/* Probably a freshly erased page: All 0xff's, including OOB,
		   but expecting 0x00, 0x00, 0x00 for ECC data
		*/
		return E_NO_CHK;
	}
	if (size == ECC_512) {
		/* 512-byte error correction requires a little more than 128 or
		   256.  If there is a correctable error then the xor will have
		   12 bits set, but there can also be 12 bits set in some
		   uncorrectable errors.  This can be solved by xoring the odd
		   and even numbered bits.

		   0xAA = 10101010
		   0x55 = 01010101
		*/
		bit_cnt02  = bit_count_table[((ecc_xor[0] & 0xAA) >> 1) ^
					      (ecc_xor[0] & 0x55)];
		bit_cnt02 += bit_count_table[((ecc_xor[1] & 0xAA) >> 1) ^
					      (ecc_xor[1] & 0x55)];
		bit_cnt02 += bit_count_table[((ecc_xor[2] & 0xAA) >> 1) ^
					      (ecc_xor[2] & 0x55)];
	} else {
		/* Counts the number of bits set in ecc code */
		bit_cnt02  = bit_count_table[ecc_xor[0]];
		bit_cnt02 += bit_count_table[ecc_xor[1]];
		bit_cnt02 += bit_count_table[ecc_xor[2]];
	}

	if (bit_cnt02 == error_bit_count) {
		/* Set the bit address */
		bit_addr02 = ((ecc_xor[2] >> 3) & 0x01) |
			((ecc_xor[2] >> 4) & 0x02) |
			((ecc_xor[2] >> 5) & 0x04);

		/* Evaluate 2 LS bits of address */
		byte_addr02 = ((ecc_xor[0] >> 1) & 0x01) |
			((ecc_xor[0] >> 2) & 0x02);

		/* Add in remaining bits of address */
		switch (size) {
		case ECC_512:
			byte_addr02 |= (((unsigned int)ecc_xor[2]) << 7)
				& 0x100;
			/* Fall through */
		case ECC_256:
			byte_addr02 |= (ecc_xor[1] & 0x80);
			/* Fall through */
		case ECC_128:
			byte_addr02 |= ((ecc_xor[0] >> 3) & 0x04) |
				((ecc_xor[0] >> 4) & 0x08) |
				((ecc_xor[1] << 3) & 0x10) |
				((ecc_xor[1] << 2) & 0x20) |
				((ecc_xor[1] << 1) & 0x40);
		}
		printk(KERN_WARNING "%s: correcting bit "
		       "(ECC block offset %03d:%d)\n",
		       __FUNCTION__, byte_addr02, bit_addr02);
		/* Correct bit error in the data */
		p_data[byte_addr02] ^= (0x01 << bit_addr02);

		/* NB p_old_code is okay, p_new_code is corrupt */
		return E_D1_CHK;  /* Data had 1-bit error (now corrected) */
	} else {
		if (bit_cnt02 == 1) {
			printk(KERN_WARNING "%s: error in ECC, data ok\n",
			       __FUNCTION__);
			return E_C1_CHK;  /* ECC has 1-bit error, data is ok */
		} else {
			printk(KERN_ERR "%s: uncorrectable error\n",
			       __FUNCTION__);
			return E_UN_CHK;  /* Uncorrectable Error */
		}
	}
}
EXPORT_SYMBOL_GPL(stm_ecc_correct);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Angus Clark");
MODULE_DESCRIPTION("STMicroelectronics NAND ECC support");

