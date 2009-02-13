/*
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#ifndef __LINUX_STM_SYSCONF_H
#define __LINUX_STM_SYSCONF_H

#include <linux/types.h>

struct sysconf_field;

/**
 * sysconf_claim - Claim ownership of a field of a sysconfig register
 * @regtype: SYS_STA or SYS_CFG
 * @regnum: the sysconfig register number
 * @lsb: the LSB of the register we are claiming
 * @msb: the MSB of the register we are claiming
 * @dev: device claiming the field
 *
 * This function claims ownership of a field from a sysconfig register.
 * The part of the sysconfig register being claimed is from bit @lsb
 * through to bit @msb inclusive. To claim the whole register, @lsb
 * should be 0, @msb 31 (or 63 for systems with 64 bit sysconfig registers).
 *
 * It returns a &struct sysconf_field which can be used in subsequent
 * operations on this field.
 */
struct sysconf_field* sysconf_claim(int regtype, int regnum, int lsb, int msb,
				    const char* dev);

/**
 * sysconf_release - Release ownership of a field of a sysconfig register
 * @field: the sysconfig field to write to
 *
 * Release ownership of a field from a sysconf register.
 * @field must have been claimed using sysconf_claim().
 */
void sysconf_release(struct sysconf_field *field);

/**
 * sysconf_write - Write a value into a field of a sysconfig register
 * @field: the sysconfig field to write to
 * @value: the value to write into the field
 *
 * This writes @value into the field of the sysconfig register @field.
 * @field must have been claimed using sysconf_claim().
 */
void sysconf_write(struct sysconf_field *field, u64 value);

/**
 * sysconf_read - Read a field of a sysconfig register
 * @field: the sysconfig field to read
 *
 * This reads a field of the sysconfig register @field.
 * @field must have been claimed using sysconf_claim().
 */
u64 sysconf_read(struct sysconf_field *field);

/*
 * sysconf_address: Return the address memory of sysconfig register
 * @field: the sysconfig field to return
 *
 * This returns the address memory of sysconfig register
 * @field must have been claimed using sysconf_claim().
 */
void *sysconf_address(struct sysconf_field *field);
/*
 * sysconf_mask: Return the bitmask of sysconfig register
 * @field: the sysconfig field to return
 *
 * This returns the bitmask of sysconfig register
 * @field must have been claimed using sysconf_claim().
 */
unsigned long sysconf_mask(struct sysconf_field *field);

#define SYS_DEV 0
#define SYS_STA 1
#define SYS_CFG 2

#endif
