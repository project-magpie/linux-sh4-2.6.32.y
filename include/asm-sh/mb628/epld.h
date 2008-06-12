/*
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Definitions applicable to the STMicroelectronics STx7141 Validation board.
 */

#ifndef __ASM_SH_MB671_EPLD_H
#define __ASM_SH_MB671_EPLD_H

#define EPLD_IDENT		0x010000
#define EPLD_TEST		0x020000
#define EPLD_RESET		0x030000
#define   EPLD_RESET_MII		(1<<5)
#define EPLD_AUDIO		0x040000
#define EPLD_FLASH		0x050000
#define   EPLD_FLASH_NOTWP		(1<<0)
#define   EPLD_FLASH_NOTRESET		(1<<1)
#define EPLD_IEEE		0x060000
#define EPLD_ENABLE		0x070000
#define   EPLD_ENABLE_HBEAT		(1<<2)
#define EPLD_CCARDCTRL		0x080000
#define EPLD_CCARDCTRL2		0x090000
#define EPLD_CCARDIMDIMODE	0x0A0000
#define EPLD_CCARDTS3INMODE	0x0B0000
#define EPLD_STATUS		0x0C0000

#endif
