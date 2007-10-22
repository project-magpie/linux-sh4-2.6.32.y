/*
 * Copyright (C) 2000 David J. Mckay (david.mckay@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Looks after interrupts on the HARP board.
 *
 * Bases on the IPR irq system
 */

#include <linux/init.h>
#include <linux/irq.h>

#include <asm/system.h>
#include <asm/io.h>

#include <asm/mach/harp.h>

#define NUM_EXTERNAL_IRQS 16

static void enable_harp_irq(unsigned int irq);
static void disable_harp_irq(unsigned int irq);

/* shutdown is same as "disable" */
#define shutdown_harp_irq disable_harp_irq

static void mask_and_ack_harp(unsigned int);
static void end_harp_irq(unsigned int irq);

static unsigned int startup_harp_irq(unsigned int irq)
{
	enable_harp_irq(irq);
	return 0;		/* never anything pending */
}

static struct hw_interrupt_type harp_irq_type = {
	.typename = "Harp-IRQ",
	.startup = startup_harp_irq,
	.shutdown = shutdown_harp_irq,
	.enable = enable_harp_irq,
	.disable = disable_harp_irq,
	.ack = mask_and_ack_harp,
	.end = end_harp_irq
};

static void disable_harp_irq(unsigned int irq)
{
	unsigned maskReg;
	unsigned mask;
	int pri;

	if (irq < 0 || irq >= NUM_EXTERNAL_IRQS)
		return;

	pri = 15 - irq;

	if (pri < 8) {
		maskReg = EPLD_INTMASK0CLR;
	} else {
		maskReg = EPLD_INTMASK1CLR;
		pri -= 8;
	}
	mask=1<<pri;

	ctrl_outl(mask, maskReg);

	/* Read back the value we just wrote to flush any write posting */
	epld_in(maskReg);
}

static void enable_harp_irq(unsigned int irq)
{
	unsigned maskReg;
	unsigned mask;
	int pri;

	if (irq < 0 || irq >= NUM_EXTERNAL_IRQS)
		return;

	pri = 15 - irq;

	if (pri < 8) {
		maskReg = EPLD_INTMASK0SET;
	} else {
		maskReg = EPLD_INTMASK1SET;
		pri -= 8;
	}
	mask=1<<pri;

	ctrl_outl(mask, maskReg);
}

/* This functions sets the desired irq handler to be an overdrive type */
static void __init make_harp_irq(unsigned int irq)
{
	disable_irq_nosync(irq);
	irq_desc[irq].handler = &harp_irq_type;
	disable_harp_irq(irq);
}

static void mask_and_ack_harp(unsigned int irq)
{
	disable_harp_irq(irq);
}

static void end_harp_irq(unsigned int irq)
{
	if (!(irq_desc[irq].status & (IRQ_DISABLED|IRQ_INPROGRESS)))
		enable_harp_irq(irq);
}

static void __init disable_all_interrupts(void)
{
	ctrl_outl(0x00, EPLD_INTMASK0);
	ctrl_outl(0x00, EPLD_INTMASK1);
}

void __init harp_init_irq(void)
{
	int i;

	disable_all_interrupts();

	if (! harp_has_intmask_setclr()) {
		printk(KERN_ERR "HARP does not have interrupt set/clr registers\n");
	}

	for (i = 0; i < NUM_EXTERNAL_IRQS; i++) {
		make_harp_irq(i);
	}
}
