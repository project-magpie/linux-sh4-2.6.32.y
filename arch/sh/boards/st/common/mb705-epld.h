/*
 * arch/sh/boards/st/common/mb705-epld.h
 *
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Register offsets into the EPLD memory space.
 */

#define EPLD_EMI_IDENT			0x000
#define EPLD_EMI_TEST			0x002
#define EPLD_EMI_SWITCH			0x004
#define EPLD_EMI_SWITCH_BOOTFROMNOR		(1<<8)
#define EPLD_EMI_MISC			0x00a
#define EPLD_EMI_MISC_NORFLASHVPPEN		(1<<2)
#define EPLD_EMI_MISC_NOTNANDFLASHWP		(1<<3)
#define EPLD_EMI_INT_STATUS		0x020
#define EPLD_EMI_INT_MASK		0x022
#define EPLD_EMI_INT_PRI(x)		(0x024+((x)*2))

#define EPLD_TS_DISPLAY_CTRL_REG	0x10c
#define EPLD_TS_DISPLAY0_BASE		0x140
#define EPLD_TS_DISPLAY1_BASE		0x180

