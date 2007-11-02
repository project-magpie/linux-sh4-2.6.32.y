/*
 * linux/arch/sh/kernel/cpu/irq/st40_ilc_stx7200.c
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Interrupts routed through the Interrupt Level Controller (ILC3) on the STx7200
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/errno.h>
#include <linux/platform_device.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq-ilc.h>

#include "st40_ilc.h"

struct ilc_data {
	unsigned int priority;
	struct list_head list;
};

static struct ilc_data ilc_data[ILC_NR_IRQS] =
{
	[0 ... ILC_NR_IRQS-1 ] = { .priority = 7 }
};

static struct list_head intc_data[16];

static DEFINE_SPINLOCK(ilc_data_lock);

/*
 * Debug printk macro
 */

/* #define ILC_DEBUG */
/* #define ILC_DEBUG_DEMUX */

#ifdef ILC_DEBUG
#define DPRINTK(args...)   printk(args)
#else
#define DPRINTK(args...)
#endif

/*
 * Beware this one; the ASC has ILC ints too...
 */

#ifdef ILC_DEBUG_DEMUX
#define DPRINTK2(args...)   printk(args)
#else
#define DPRINTK2(args...)
#endif

/*
 * The interrupt demux function. Check if this was an ILC interrupt, and
 * of so which device generated the interrupt.
 */

void ilc_irq_demux(unsigned int irq, struct irq_desc *desc)
{
	unsigned int priority = 14 - irq;
	unsigned int irq_offset;
	struct ilc_data *this;
	int handled = 0;

	DPRINTK2("ilc demux got irq %d\n", irq);

	list_for_each_entry(this, &intc_data[priority], list) {

		irq_offset = this - ilc_data;

		if (ILC_GET_STATUS(irq_offset) && ILC_GET_ENABLE(irq_offset)) {
			struct irq_desc *desc = irq_desc + ILC_IRQ(irq_offset);

			DPRINTK2("ilc found ilc %d active\n", irq_offset);
			ILC_CLR_STATUS(irq_offset);
			desc->handle_irq(ILC_IRQ(irq_offset), desc);
			handled = 1;
		}
	}

	if (!handled)
		printk(KERN_INFO "ILC: spurious interrupt demux %d\n", irq);
}

static unsigned int startup_ilc_irq(unsigned int irq)
{
	struct ilc_data *this;
	unsigned int priority;
	int irq_offset = irq - ILC_FIRST_IRQ;
	unsigned long flags;

	DPRINTK("ilc startup irq %d\n", irq);

	if ((irq_offset < 0) || (irq_offset >= ILC_NR_IRQS))
		return -ENODEV;

	this = &ilc_data[irq_offset];
	priority = this->priority;

	spin_lock_irqsave(&ilc_data_lock, flags);
	list_add(&this->list, &intc_data[priority]);
	spin_unlock_irqrestore(&ilc_data_lock, flags);

	ILC_SET_PRI(irq_offset, priority);
	ILC_SET_TRIGMODE(irq_offset, ILC_TRIGGERMODE_HIGH);

	/* Gross hack for external Ethernet PHYs which are active low */
	/* FIXME: Move this into the BSP code */
	if ((irq_offset == 93)  ||  (irq_offset == 95)) {
		ILC_SET_TRIGMODE(irq_offset, ILC_TRIGGERMODE_LOW);
	}

	ILC_SET_ENABLE(irq_offset);

	return 0;
}

static void shutdown_ilc_irq(unsigned int irq)
{
	struct ilc_data *this;
	unsigned int priority;
	int irq_offset = irq - ILC_FIRST_IRQ;
	unsigned long flags;

	DPRINTK("ilc shutdown irq %d\n", irq);

	if ((irq_offset < 0) || (irq_offset >= ILC_NR_IRQS))
		return;

	this = &ilc_data[irq_offset];
	priority = this->priority;

	ILC_CLR_ENABLE(irq_offset);
	ILC_SET_PRI(irq_offset, 0);

	spin_lock_irqsave(&ilc_data_lock, flags);
	list_del(&this->list);
	spin_unlock_irqrestore(&ilc_data_lock, flags);
}

static void enable_ilc_irq(unsigned int irq)
{
	int irq_offset = irq - ILC_FIRST_IRQ;
DPRINTK2("%s: irq %d\n", __FUNCTION__, irq);
	ILC_SET_ENABLE(irq_offset);
}

static void disable_ilc_irq(unsigned int irq)
{
	int irq_offset = irq - ILC_FIRST_IRQ;
DPRINTK2("%s: irq %d\n", __FUNCTION__, irq);
	ILC_CLR_ENABLE(irq_offset);
}

static void mask_and_ack_ilc(unsigned int irq)
{
	int irq_offset = irq - ILC_FIRST_IRQ;
DPRINTK2("%s: irq %d\n", __FUNCTION__, irq);
	ILC_CLR_ENABLE(irq_offset);
	(void)ILC_GET_ENABLE(irq_offset); /* Defeat write posting */
}

static struct irq_chip ilc_chip = {
	.name		= "ILC3-IRQ",
	.startup	= startup_ilc_irq,
	.shutdown	= shutdown_ilc_irq,
	.mask		= disable_ilc_irq,
	.mask_ack	= mask_and_ack_ilc,
	.unmask		= enable_ilc_irq,
};

void __init ilc_stx7200_init(void)
{
	int irq;

	DPRINTK("STx7200: Initialising ILC\n");

	for (irq = ILC_FIRST_IRQ; irq < (ILC_FIRST_IRQ+ILC_NR_IRQS); irq++)
		/* SIM: Should we do the masking etc in ilc_irq_demux and
		 * then change this to handle_simple_irq? */
		set_irq_chip_and_handler_name(irq, &ilc_chip, handle_level_irq,
					      "ILC");

	for (irq = 0; irq < 16; irq++) {
		INIT_LIST_HEAD(&intc_data[irq]);
	}
}
