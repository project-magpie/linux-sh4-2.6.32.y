/*
 * Copyright (C) 2005 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Definitions applicable to the STMicroelectronics STb7100 Validation board.
 */

#define epld_out(val,addr) ctrl_outb(val,addr)
#define epld_in(addr)      ctrl_inb(addr)

#define EPLD_BASE		0xa3000000
#define EPLD_EPLDVER		(EPLD_BASE + 0x000000)
#define EPLD_PCBVER		(EPLD_BASE + 0x020000)
#define EPLD_STEM		(EPLD_BASE + 0x040000)
#define EPLD_DRIVER		(EPLD_BASE + 0x060000)
#define EPLD_RESET		(EPLD_BASE + 0x080000)
#define EPLD_INTSTAT0		(EPLD_BASE + 0x0a0000)
#define EPLD_INTSTAT1		(EPLD_BASE + 0x0c0000)
#define EPLD_INTMASK0		(EPLD_BASE + 0x0e0000)
#define EPLD_INTMASK0SET	(EPLD_BASE + 0x100000)
#define EPLD_INTMASK0CLR	(EPLD_BASE + 0x120000)
#define EPLD_INTMASK1		(EPLD_BASE + 0x140000)
#define EPLD_INTMASK1SET	(EPLD_BASE + 0x160000)
#define EPLD_INTMASK1CLR	(EPLD_BASE + 0x180000)
#define EPLD_TEST		(EPLD_BASE + 0x1e0000)

/* Some registers are also available in the POD EPLD */
#define EPLD_POD_BASE		0xa2100000
#define EPLD_POD_REVID		(EPLD_POD_BASE + 0x00)
#define EPLD_POD_LED		(EPLD_POD_BASE + 0x10)
#define EPLD_POD_DEVID		(EPLD_POD_BASE + 0x1c)

#define EPLD_LED_ON     1
#define EPLD_LED_OFF    0

#ifndef __ASSEMBLY__
extern inline int harp_has_intmask_setclr(void)
{
        return 1;
}

#if 0
extern inline void harp_set_vpp_on(void)
{
	epld_out(3, EPLD_FLASH);	/* bits: 0 = VPP ON; 1 = RESET	*/
}

extern inline void harp_set_vpp_off(void)
{
	epld_out(2, EPLD_FLASH);	/* Leave ON only RESET		*/
}

#endif
void harp_init_irq(void);
#endif /* !__ASSEMBLY__ */
