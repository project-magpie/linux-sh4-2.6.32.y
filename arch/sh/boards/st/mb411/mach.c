/*
 * arch/sh/boards/st/mb411/mach.c
 *
 * Copyright (C) 2005 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Machine vector for the STMicroelectronics STb7100 Validation board.
 */

#include <linux/init.h>
#include <linux/irq.h>
#include <asm/system.h>
#include <asm/io.h>
#include <asm/machvec.h>
#include <asm/led.h>
#include <asm/machvec_init.h>
#include <asm/mb411/harp.h>

static void __iomem *mb411_ioport_map(unsigned long port, unsigned int size)
{
	/* However picking somewhere safe isn't as easy as you might think.
	 * I used to use external ROM, but that can cause problems if you are
	 * in the middle of updating Flash. So I'm now using the processor core
	 * version register, which is guaranted to be available, and non-writable.
	 */
	return CCN_PVR;
}

static void __init mb411_init_irq(void)
{
	/* Set the ILC to route external interrupts to the the INTC */
	/* Outputs 0-3 are the interrupt pins, 4-7 are routed to the INTC */
	ilc_route_external(ILC_EXT_IRQ0, 4, 0);
	ilc_route_external(ILC_EXT_IRQ1, 5, 0);
	ilc_route_external(ILC_EXT_IRQ2, 6, 0);
	ilc_route_external(ILC_EXT_IRQ3, 7, 0);

	/* ...where they are hadled as normal HARP style (encoded) interrpts */
	harp_init_irq();
}

struct sh_machine_vector mv_mediaref __initmv = {
	.mv_nr_irqs		= NR_IRQS,
	.mv_init_irq		= mb411_init_irq,
	.mv_ioport_map		= mb411_ioport_map,

#ifdef CONFIG_HEARTBEAT
	.mv_heartbeat		= heartbeat_heart,
#endif
};
ALIAS_MV(mediaref)
