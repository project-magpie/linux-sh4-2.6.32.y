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
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/errno.h>
#include <linux/platform_device.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq-ilc.h>

#include "st40_ilc.h"

struct ilc_data {
#define ilc_get_priority(_ilc)		((_ilc)->priority)
#define ilc_set_priority(_ilc, _prio)	((_ilc)->priority = (_prio))
	unsigned char priority;
#define ILC_STATE_USED			0x1
#define ilc_set_used(_ilc)		((_ilc)->state |= ILC_STATE_USED)
#define ilc_set_unused(_ilc)		((_ilc)->state &= ~(ILC_STATE_USED))
	unsigned char state;
};

static struct ilc_data ilc_data[ILC_NR_IRQS] =
{
	[0 ... ILC_NR_IRQS-1 ] = { .priority = 7 }
};

static DEFINE_SPINLOCK(ilc_data_lock);


#define ILC_PRIORITY_MASK_SIZE		DIV_ROUND_UP(ILC_NR_IRQS, 32)

struct pr_mask {
	/* Each priority mask needs ILC_NR_IRQS bits */
       unsigned long mask[ILC_PRIORITY_MASK_SIZE];
};

static struct pr_mask priority_mask[16];

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
 * if so which device generated the interrupt.
 */
void ilc_irq_demux(unsigned int irq, struct irq_desc *desc)
{
	unsigned int priority = 14 - irq;
	unsigned int irq_offset;
	int handled = 0;
	int idx;
	unsigned long status;

	DPRINTK2("ilc demux got irq %d\n", irq);

	for (idx = 0; idx < ILC_PRIORITY_MASK_SIZE; ++idx) {
		struct irq_desc *desc;

		status = ioread32(ilc_base + ILC_BASE_STATUS + (idx<<2)) &
			ioread32(ilc_base + ILC_BASE_ENABLE + (idx<<2)) &
			priority_mask[priority].mask[idx] ;
		if (!status)
			continue;

		irq_offset = (idx*32)+ffs(status)-1;
		desc = irq_desc + ILC_IRQ(irq_offset);
		desc->handle_irq(ILC_IRQ(irq_offset), desc);
		handled = 1;
		ILC_CLR_STATUS(irq_offset);
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
	ilc_set_used(this);
	priority_mask[priority].mask[_BANK(irq_offset)] |=
		_BIT(irq_offset);
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
	ilc_set_unused(this);
	priority_mask[priority].mask[_BANK(irq_offset)] &=
		~(_BIT(irq_offset));
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

}
