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
#include <linux/platform_device.h>

struct sysconf_field;

/**
 * sysconf_claim - Claim ownership of a field of a sysconfig register
 * @group: register group (ie. SYS_CFG, SYS_STA); SOC-specific, see below
 * @num: register number
 * @lsb: the LSB of the register we are claiming
 * @msb: the MSB of the register we are claiming
 * @devname: device claiming the field
 *
 * This function claims ownership of a field from a sysconfig register.
 * The part of the sysconfig register being claimed is from bit @lsb
 * through to bit @msb inclusive. To claim the whole register, @lsb
 * should be 0, @msb 31 (or 63 for systems with 64 bit sysconfig registers).
 *
 * It returns a &struct sysconf_field which can be used in subsequent
 * operations on this field.
 */
struct sysconf_field *sysconf_claim(int group, int num, int lsb, int msb,
		const char *devname);

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
void sysconf_write(struct sysconf_field *field, unsigned long value);

/**
 * sysconf_read - Read a field of a sysconfig register
 * @field: the sysconfig field to read
 *
 * This reads a field of the sysconfig register @field.
 * @field must have been claimed using sysconf_claim().
 */
unsigned long sysconf_read(struct sysconf_field *field);

/**
 * sysconf_address - Return the address memory of sysconfig register
 * @field: the sysconfig field to return
 *
 * This returns the address memory of sysconfig register
 * @field must have been claimed using sysconf_claim().
 */
void *sysconf_address(struct sysconf_field *field);

/**
 * sysconf_mask: Return the bitmask of sysconfig register
 * @field: the sysconfig field to return
 *
 * This returns the bitmask of sysconfig register
 * @field must have been claimed using sysconf_claim().
 */
unsigned long sysconf_mask(struct sysconf_field *field);

/**
 * sysconf_early_init - Used by board initialization code
 */

void sysconf_early_init(struct platform_device *pdevs, int pdevs_num);

/**
 * Available register types:
 */

#if defined(CONFIG_CPU_SUBTYPE_STX5197)

#define HS_CFG 			0
#define HD_CFG 			1

#define CFG_CTRL_A		HS_CFG, (0x00 / 4)
#define CFG_CTRL_B		HS_CFG, (0x04 / 4)

#define CFG_CTRL_C		HD_CFG, (0x00 / 4)
#define CFG_CTRL_D		HD_CFG, (0x04 / 4)
#define CFG_CTRL_E		HD_CFG, (0x08 / 4)
#define CFG_CTRL_F		HD_CFG, (0x0c / 4)
#define CFG_CTRL_G		HD_CFG, (0x10 / 4)
#define CFG_CTRL_H		HD_CFG, (0x14 / 4)
#define CFG_CTRL_I		HD_CFG, (0x18 / 4)
#define CFG_CTRL_J		HD_CFG, (0x1c / 4)

#define CFG_CTRL_K		HD_CFG, (0x40 / 4)
#define CFG_CTRL_L		HD_CFG, (0x44 / 4)
#define CFG_CTRL_M		HD_CFG, (0x48 / 4)
#define CFG_CTRL_N		HD_CFG, (0x4c / 4)
#define CFG_CTRL_O		HD_CFG, (0x50 / 4)
#define CFG_CTRL_P		HD_CFG, (0x54 / 4)
#define CFG_CTRL_Q		HD_CFG, (0x58 / 4)
#define CFG_CTRL_R		HD_CFG, (0x5c / 4)

#define CFG_MONITOR_A		HS_CFG, (0x08 / 4)
#define CFG_MONITOR_B		HS_CFG, (0x0c / 4)

#define CFG_MONITOR_C		HD_CFG, (0x20 / 4)
#define CFG_MONITOR_D		HD_CFG, (0x24 / 4)
#define CFG_MONITOR_E		HD_CFG, (0x28 / 4)
#define CFG_MONITOR_F		HD_CFG, (0x2c / 4)
#define CFG_MONITOR_G		HD_CFG, (0x30 / 4)
#define CFG_MONITOR_H		HD_CFG, (0x34 / 4)
#define CFG_MONITOR_I		HD_CFG, (0x38 / 4)
#define CFG_MONITOR_J		HD_CFG, (0x3c / 4)

#define CFG_MONITOR_K		HD_CFG, (0x60 / 4)
#define CFG_MONITOR_L		HD_CFG, (0x64 / 4)
#define CFG_MONITOR_M		HD_CFG, (0x68 / 4)
#define CFG_MONITOR_N		HD_CFG, (0x6c / 4)
#define CFG_MONITOR_O		HD_CFG, (0x70 / 4)
#define CFG_MONITOR_P		HD_CFG, (0x74 / 4)
#define CFG_MONITOR_Q		HD_CFG, (0x78 / 4)
#define CFG_MONITOR_R		HD_CFG, (0x7c / 4)

#elif defined(CONFIG_CPU_SUBTYPE_STX7108)

#define NE_SYS_STA		0
#define NE_SYS_CFG		1
#define SE_SYS_STA		2
#define SE_SYS_CFG		3
#define NW_SYS_STA		4
#define NW_SYS_CFG		5
#define SW_SYS_STA		6
#define SW_SYS_CFG		7

#else

#define SYS_DEV			0
#define SYS_STA			1
#define SYS_CFG			2

#endif

#endif
