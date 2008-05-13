/*
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#ifndef __ASM_SH_MB680_STEM_H
#define __ASM_SH_MB680_STEM_H

#include <asm/irq-ilc.h>

/* STEM CS0 = BANK2 */
/* Need to set J14A to 1-2 (notStemCS(0) <= notEMICSC) and
 * J4 to 1-2 and fit J2A (notStemIntr(0) <= SysIRQ2) if mb680 used
 * standalone. */
#define STEM_CS0_BANK 2
#define STEM_CS0_OFFSET 0

/* STEM CS1 = BANK3 */
/* Need to set J14B to 1-2 (notStemCS(1) <= notEMICSD) and
 * fit J2B (notStemIntr(1) <= SysIRQ1) if mb680 used
 * standalone. */
#define STEM_CS1_BANK 3
#define STEM_CS1_OFFSET 0

#define STEM_INTR0_IRQ ILC_EXT_IRQ(2)
#define STEM_INTR1_IRQ ILC_EXT_IRQ(1)

#endif
