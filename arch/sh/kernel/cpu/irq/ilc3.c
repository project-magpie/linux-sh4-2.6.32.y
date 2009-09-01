/*
 * linux/arch/sh/kernel/cpu/irq/ilc3.c
 *
 * Copyright (C) 2009 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 * Author: Francesco Virlinzi <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Interrupts routed through the Interrupt Level Controller (ILC3)
 */


#include <linux/kernel.h>
#include <linux/sysdev.h>
#include <linux/cpu.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/delay.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/spinlock.h>
#include <linux/string.h>
#include <linux/stm/platform.h>
#include <linux/io.h>

#include <asm/hw_irq.h>
#include <asm/system.h>
#include <asm/irq-ilc.h>

#include "ilc3.h"

#define ilc_base			(data->iomem)

#define DRIVER_NAME "ilc3"

struct ilc_irq {
#define ilc_get_priority(_ilc)		((_ilc)->priority)
#define ilc_set_priority(_ilc, _prio)	((_ilc)->priority = (_prio))
	unsigned char priority;
#define ILC_STATE_USED			0x1
#define ILC_WAKEUP_ENABLED		0x2
#define ILC_ENABLED			0x4

#define ilc_is_used(_ilc)		(((_ilc)->state & ILC_STATE_USED) != 0)
#define ilc_set_used(_ilc)		((_ilc)->state |= ILC_STATE_USED)
#define ilc_set_unused(_ilc)		((_ilc)->state &= ~(ILC_STATE_USED))

#define ilc_set_wakeup(_ilc)		((_ilc)->state |= ILC_WAKEUP_ENABLED)
#define ilc_reset_wakeup(_ilc)		((_ilc)->state &= ~ILC_WAKEUP_ENABLED)
#define ilc_wakeup_enabled(_ilc)  (((_ilc)->state & ILC_WAKEUP_ENABLED) != 0)

#define ilc_set_enabled(_ilc)		((_ilc)->state |= ILC_ENABLED)
#define ilc_set_disabled(_ilc)		((_ilc)->state &= ~ILC_ENABLED)
#define ilc_is_enabled(_ilc)		(((_ilc)->state & ILC_ENABLED) != 0)

	unsigned char state;
/*
 * trigger_mode is used to restore the right mode
 * after a resume from hibernation
 */
	unsigned char trigger_mode;
};

struct drv_ilc_data {
	void *iomem;			/* ILC base address */
	spinlock_t lock;		/* a lock */
	struct ilc_irq *irq_data;	/* the input states descriptor */
	struct platform_device *pdev;
	unsigned long **priority;
	struct irq_chip chip;		/* the real chip */
	struct irq_chip	dummy;		/* a dummy chip to catch the event */
};
/*
 * Debug printk macro
 */

/* #define ILC_DEBUG */
/* #define ILC_DEBUG_DEMUX */

#ifdef ILC_DEBUG
#define DPRINTK(fmt, args...)		\
	printk(KERN_INFO"%s: " fmt, __func__, ## args)
#else
#define DPRINTK(args...)
#endif

/*
 * Beware this one; the ASC has ILC ints too...
 */

#ifdef ILC_DEBUG_DEMUX
#define DPRINTK2(args...)   		\
	printk(KERN_INFO"%s: " fmt, __func__, ## args)
#else
#define DPRINTK2(args...)
#endif

#define ilc_base		(data->iomem)

/*
 * From evt2irq to ilc2irq
 */
int ilc2irq(unsigned int evtcode)
{
	int idx, irq = evt2irq(evtcode);
	struct irq_chip *chip = irq_desc[irq].chip;
	struct drv_ilc_data *data = (struct drv_ilc_data *)
		container_of(chip, struct drv_ilc_data, dummy);
	struct stm_plat_ilc3_data *pdata = (struct stm_plat_ilc3_data *)
		data->pdev->dev.platform_data;
#if	defined(CONFIG_CPU_SUBTYPE_STX7111) || \
	defined(CONFIG_CPU_SUBTYPE_STX7141)
	unsigned int priority = 7;
#elif	defined(CONFIG_CPU_SUBTYPE_STX5197) || \
	defined(CONFIG_CPU_SUBTYPE_STX7105) || \
	defined(CONFIG_CPU_SUBTYPE_STX7200)
	unsigned int priority = 14 - evt2irq(evtcode);
#endif
	unsigned long status;
	for (idx = 0, status = 0;
	     idx < DIV_ROUND_UP(pdata->num_output, 32) && !status;
	     ++idx)
		status = readl(data->iomem + ILC_BASE_STATUS + (idx << 2)) &
			readl(data->iomem + ILC_BASE_ENABLE + (idx << 2)) &
			data->priority[priority][idx];

	return pdata->first_irq + (idx * 32) + ffs(status) - 1;
}
/*
 * The interrupt demux function. Check if this was an ILC interrupt, and
 * if so which device generated the interrupt.
 */
void ilc_irq_demux(unsigned int irq, struct irq_desc *desc)
{
	struct irq_chip *chip = irq_desc[irq].chip;
	struct drv_ilc_data *data = (struct drv_ilc_data *)
		container_of(chip, struct drv_ilc_data, dummy);
	struct stm_plat_ilc3_data *pdata = (struct stm_plat_ilc3_data *)
		data->pdev->dev.platform_data;
#if	defined(CONFIG_CPU_SUBTYPE_STX7111) || \
	defined(CONFIG_CPU_SUBTYPE_STX7141)
	unsigned int priority = 7;
#elif	defined(CONFIG_CPU_SUBTYPE_STX5197) || \
	defined(CONFIG_CPU_SUBTYPE_STX7105) || \
	defined(CONFIG_CPU_SUBTYPE_STX7200)
	unsigned int priority = 14 - irq;
#endif
	int handled = 0;
	int idx;

	DPRINTK2("%s: irq %d\n", __func__, irq);

	for (idx = 0; idx < DIV_ROUND_UP(pdata->num_input, 32); ++idx) {
		unsigned long status;
		unsigned int irq_offset;
		struct irq_desc *desc;

		status = readl(data->iomem + ILC_BASE_STATUS + (idx << 2)) &
			readl(data->iomem + ILC_BASE_ENABLE + (idx << 2)) &
			data->priority[priority][idx];
		if (!status)
			continue;

	irq_offset = (idx * 32) + ffs(status) - 1;
		desc = irq_desc + irq_offset + pdata->first_irq;
		desc->handle_irq(pdata->first_irq + irq_offset, desc);
		handled = 1;
		ILC_CLR_STATUS(irq_offset);
	}

	if (likely(handled))
		return;

	atomic_inc(&irq_err_count);

	printk(KERN_INFO "ILC: spurious interrupt demux %d\n", irq);

	printk(KERN_DEBUG "ILC:  inputs   status  enabled    used\n");

	for (idx = 0; idx < DIV_ROUND_UP(pdata->num_input, 32); ++idx) {
		unsigned long status, enabled, used;

		status = readl(data->iomem + ILC_BASE_STATUS + (idx << 2));
		enabled = readl(data->iomem + ILC_BASE_ENABLE + (idx << 2));
		used = 0;
		for (priority = 0; priority < pdata->num_output; ++priority)
			used |= data->priority[priority][idx];

		printk(KERN_DEBUG "ILC: %3d-%3d: %08lx %08lx %08lx"
				"\n", idx * 32, (idx * 32) + 31,
				status, enabled, used);
	}
}

static unsigned int startup_ilc_irq(unsigned int irq)
{
	struct irq_chip *chip = irq_desc[irq].chip;
	struct drv_ilc_data *data = (struct drv_ilc_data *)
		container_of(chip, struct drv_ilc_data, chip);
	struct stm_plat_ilc3_data *pdata = (struct stm_plat_ilc3_data *)
		data->pdev->dev.platform_data;
	unsigned int priority;
	unsigned long flags;
	int irq_offset = irq - pdata->first_irq;
	struct ilc_irq *this;

	DPRINTK("%s: irq %d\n", __func__, irq);

	if ((irq_offset < 0) || (irq_offset >= pdata->num_input))
		return -ENODEV;

	this = &data->irq_data[irq_offset];
	priority = this->priority;

	spin_lock_irqsave(&data->lock, flags);
	ilc_set_used(this);
	ilc_set_enabled(this);
	data->priority[priority][_BANK(irq_offset)] |= _BIT(irq_offset);
	spin_unlock_irqrestore(&data->lock, flags);

#if	defined(CONFIG_CPU_SUBTYPE_STX7111)
	/* ILC_EXT_OUT[4] -> IRL[0] (default priority 13 = irq  2) */
	/* ILC_EXT_OUT[5] -> IRL[1] (default priority 10 = irq  5) */
	/* ILC_EXT_OUT[6] -> IRL[2] (default priority  7 = irq  8) */
	/* ILC_EXT_OUT[7] -> IRL[3] (default priority  4 = irq 11) */
	ILC_SET_PRI(irq_offset, 0x8007);
#elif	defined(CONFIG_CPU_SUBTYPE_STX5197) || \
	defined(CONFIG_CPU_SUBTYPE_STX7105) || \
	defined(CONFIG_CPU_SUBTYPE_STX7200)
	ILC_SET_PRI(irq_offset, priority);
#elif	defined(CONFIG_CPU_SUBTYPE_STX7141)
	ILC_SET_PRI(irq_offset, 0x0);
#endif

	ILC_SET_ENABLE(irq_offset);

	return 0;
}

static void shutdown_ilc_irq(unsigned int irq)
{
	struct irq_chip *chip = irq_desc[irq].chip;
	struct drv_ilc_data *data = (struct drv_ilc_data *)
		container_of(chip, struct drv_ilc_data, chip);
	struct stm_plat_ilc3_data *pdata = (struct stm_plat_ilc3_data *)
		data->pdev->dev.platform_data;
	struct ilc_irq *this;
	unsigned int priority;
	unsigned long flags;
	int irq_offset = irq - pdata->first_irq;

	DPRINTK("%s: irq %d\n", __func__, irq);

	WARN_ON(!ilc_is_used(&data->irq_data[irq_offset]));

	if ((irq_offset < 0) || (irq_offset >= pdata->num_input))
		return;

	this = &data->irq_data[irq_offset];
	priority = this->priority;

	ILC_CLR_ENABLE(irq_offset);
	ILC_SET_PRI(irq_offset, 0);

	spin_lock_irqsave(&data->lock, flags);
	ilc_set_disabled(this);
	ilc_set_unused(this);
	data->priority[priority][_BANK(irq_offset)] &= ~(_BIT(irq_offset));
	spin_unlock_irqrestore(&data->lock, flags);
}

static void unmask_ilc_irq(unsigned int irq)
{
	struct irq_chip *chip = irq_desc[irq].chip;
	struct drv_ilc_data *data = (struct drv_ilc_data *)
		container_of(chip, struct drv_ilc_data, chip);
	struct stm_plat_ilc3_data *pdata = (struct stm_plat_ilc3_data *)
		data->pdev->dev.platform_data;
	int irq_offset = irq - pdata->first_irq;
	struct ilc_irq *this = &data->irq_data[irq_offset];

	DPRINTK2("%s: irq %d\n", __func__, irq);

	ILC_SET_ENABLE(irq_offset);
	ilc_set_enabled(this);
}

static void mask_ilc_irq(unsigned int irq)
{
	struct irq_chip *chip = irq_desc[irq].chip;
	struct drv_ilc_data *data = (struct drv_ilc_data *)
		container_of(chip, struct drv_ilc_data, chip);
	struct stm_plat_ilc3_data *pdata = (struct stm_plat_ilc3_data *)
		data->pdev->dev.platform_data;
	int irq_offset = irq - pdata->first_irq;
	struct ilc_irq *this = &data->irq_data[irq_offset];

	DPRINTK2("%s: irq %d\n", __func__, irq);

	ILC_CLR_ENABLE(irq_offset);
	ilc_set_disabled(this);
}

static void mask_and_ack_ilc(unsigned int irq)
{
	struct irq_chip *chip = irq_desc[irq].chip;
	struct drv_ilc_data *data = (struct drv_ilc_data *)
		container_of(chip, struct drv_ilc_data, chip);
	struct stm_plat_ilc3_data *pdata = (struct stm_plat_ilc3_data *)
		data->pdev->dev.platform_data;
	int irq_offset = irq - pdata->first_irq;
	struct ilc_irq *this = &data->irq_data[irq_offset];

	DPRINTK2("%s: irq %d\n", __func__, irq);

	ILC_CLR_ENABLE(irq_offset);
	(void)ILC_GET_ENABLE(irq_offset); /* Defeat write posting */
	ilc_set_disabled(this);
}

static int set_type_ilc_irq(unsigned int irq, unsigned int flow_type)
{
	struct irq_chip *chip = irq_desc[irq].chip;
	struct drv_ilc_data *data = (struct drv_ilc_data *)
		container_of(chip, struct drv_ilc_data, chip);
	struct stm_plat_ilc3_data *pdata = (struct stm_plat_ilc3_data *)
		data->pdev->dev.platform_data;
	int irq_offset = irq - pdata->first_irq;
	int mode;

	switch (flow_type) {
	case IRQ_TYPE_EDGE_RISING:
		mode = ILC_TRIGGERMODE_RISING;
		break;
	case IRQ_TYPE_EDGE_FALLING:
		mode = ILC_TRIGGERMODE_FALLING;
		break;
	case IRQ_TYPE_EDGE_BOTH:
		mode = ILC_TRIGGERMODE_ANY;
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		mode = ILC_TRIGGERMODE_HIGH;
		break;
	case IRQ_TYPE_LEVEL_LOW:
		mode = ILC_TRIGGERMODE_LOW;
		break;
	default:
		return -EINVAL;
	}

	ILC_SET_TRIGMODE(irq_offset, mode);
	data->irq_data[irq_offset].trigger_mode = (unsigned char)mode;

	return 0;
}

static int set_wake_ilc_irq(unsigned int irq, unsigned int on)
{
	struct irq_chip *chip = irq_desc[irq].chip;
	struct drv_ilc_data *data = (struct drv_ilc_data *)
		container_of(chip, struct drv_ilc_data, chip);
	struct stm_plat_ilc3_data *pdata = (struct stm_plat_ilc3_data *)
		data->pdev->dev.platform_data;
	int irq_offset;
	struct ilc_irq *this;

	if (irq < pdata->first_irq ||
	    irq > (pdata->num_input + pdata->first_irq))
		/* this interrupt can not be on ILC3 */
		return -1;
	irq_offset = irq - pdata->first_irq;
	this = &data->irq_data[irq_offset];
	if (on) {
		ilc_set_wakeup(this);
		ILC_WAKEUP_ENABLE(irq_offset);
		ILC_WAKEUP(irq_offset, 1);
	} else {
		ilc_reset_wakeup(this);
		ILC_WAKEUP_DISABLE(irq_offset);
	}
	return 0;
}

static struct irq_chip ilc_chip_template = {
	.name		= "ILC3",
	.startup	= startup_ilc_irq,
	.shutdown	= shutdown_ilc_irq,
	.mask		= mask_ilc_irq,
	.mask_ack	= mask_and_ack_ilc,
	.unmask		= unmask_ilc_irq,
	.set_type	= set_type_ilc_irq,
	.set_wake	= set_wake_ilc_irq,
};

static void __init ilc_demux_init(struct platform_device *pdev)
{
	int irq, irq_offset;
	struct drv_ilc_data *data = (struct drv_ilc_data *)
		pdev->dev.driver_data;
	struct stm_plat_ilc3_data *pdata = (struct stm_plat_ilc3_data *)
		pdev->dev.platform_data;

	/* Default all interrupts to active high. */
	for (irq_offset = 0; irq_offset < pdata->num_input; irq_offset++)
		ILC_SET_TRIGMODE(irq_offset, ILC_TRIGGERMODE_HIGH);

	for (irq = pdata->first_irq; irq < (pdata->first_irq+pdata->num_input);
		irq++)
		/* SIM: Should we do the masking etc in ilc_irq_demux and
		 * then change this to handle_simple_irq? */
		set_irq_chip_and_handler_name(irq, &data->chip,
			handle_level_irq, pdev->name);
	if (!pdata->cpu_irq)
		return ;
	/* standard demux algo */
	for (irq = 0; pdata->cpu_irq[irq] != -1; ++irq)
		set_irq_chip_and_handler(pdata->cpu_irq[irq],
			&data->dummy, ilc_irq_demux);
	return ;
}

static int __init ilc_probe(struct platform_device *pdev)
{
	struct stm_plat_ilc3_data *plt_data = (struct stm_plat_ilc3_data *)
		pdev->dev.platform_data;
	int i, size = pdev->resource[0].end - pdev->resource[0].start + 1;
	struct drv_ilc_data *drv_data;

	if (!request_mem_region(pdev->resource[0].start, size, pdev->name))
		return -EBUSY;

	if (pdev->dev.driver_data)
		return 0;

	drv_data = kzalloc(sizeof(struct drv_ilc_data), GFP_KERNEL);

	if (!drv_data)
		return -ENOMEM;

	pdev->dev.driver_data = drv_data;
	drv_data->pdev = pdev;
	memcpy(&drv_data->chip, &ilc_chip_template, sizeof(ilc_chip_template));
	spin_lock_init(&drv_data->lock);
	drv_data->iomem = ioremap(pdev->resource[0].start, size);
	drv_data->irq_data = kzalloc(sizeof(struct ilc_irq) *
		plt_data->num_input, GFP_KERNEL);

	for (i = 0; i < plt_data->num_input; ++i) {
		drv_data->irq_data[i].priority = 7;
		drv_data->irq_data[i].trigger_mode = ILC_TRIGGERMODE_HIGH;
	}

	drv_data->priority = kzalloc(plt_data->num_output * sizeof(long),
			GFP_KERNEL);
	for (i = 0; i < plt_data->num_output; ++i)
		drv_data->priority[i] = kmalloc(sizeof(long) *
			DIV_ROUND_UP(plt_data->num_input, 32), GFP_KERNEL);

	ilc_demux_init(pdev);
	return 0;
}

static struct platform_driver ilc_driver = {
	.probe		= ilc_probe,
	.driver	= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
};

#if defined(CONFIG_PROC_FS)
#include "../../../../../drivers/base/base.h"
static void *ilc_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	struct list_head *node;
	union {
		loff_t value;
		long parts[2];
		} ltmp;
	ltmp.value = *pos;
	node = (struct list_head *)ltmp.parts[0];
	node = node->next;
	if (node == &ilc_driver.driver.p->klist_devices.k_list)
		return NULL; /* no more device */
	ltmp.parts[1] = 0;
	ltmp.parts[0] = (long) node;
	*pos = ltmp.value;
	return pos;
}

static void *ilc_seq_start(struct seq_file *s, loff_t *pos)
{
	union {
		loff_t value;
		long parts[2];
		} ltmp;
	struct list_head *node;
	struct device *ilc;
	struct device_private *ilc_priv;

	if (!*pos) { /* first call! */
		node = ilc_driver.driver.p->klist_devices.k_list.next;
		if (node == &ilc_driver.driver.p->klist_devices.k_list)
			return NULL;	/* no devices */
		ilc_priv = container_of(node, struct device_private,
				knode_driver.n_node);
		ilc = ilc_priv->device;
		ltmp.parts[1] = 0;
		ltmp.parts[0] = (long)node;
		*pos = ltmp.value;
		seq_printf(s,
		"input irq status enabled used priority mode wakeup\n");
		return pos;
	}
	--(*pos); /* to realign *pos value! */

	return ilc_seq_next(s, NULL, pos);
}

static void ilc_seq_stop(struct seq_file *s, void *v)
{
}

static int ilc_seq_show(struct seq_file *s, void *v)
{
	unsigned long *l = (unsigned long *)v;
	struct list_head *tmp = (struct list_head *)(*l);
	struct device_private *dev_priv = (struct device_private *)
		container_of(tmp, struct device_private,
			knode_driver.n_node);
	struct device *ilc = dev_priv->device;

	struct drv_ilc_data *data = (struct drv_ilc_data *)
			ilc->driver_data;
	struct stm_plat_ilc3_data *pdata = ilc->platform_data;
	int i, status, enabled, used, wakeup;

	seq_printf(s, "ILC: %s\n", dev_name(ilc));
	for (i = 0; i < pdata->num_input; ++i) {
		status = (ILC_GET_STATUS(i) != 0);
		enabled = (ILC_GET_ENABLE(i) != 0);
		used = ilc_is_used(&data->irq_data[i]);
		wakeup = ilc_wakeup_enabled(&data->irq_data[i]);
		seq_printf(s, "%3d %3d %d %d %d %d %d %d",
			i, i + pdata->first_irq,
			status, enabled, used, readl(ILC_PRIORITY_REG(i)),
			readl(ILC_TRIGMODE_REG(i)), wakeup);
		if (enabled && !used)
			seq_printf(s, " !!!");
		seq_printf(s, "\n");
	}

	return 0;
}

static const struct seq_operations ilc_seq_ops = {
	.start = ilc_seq_start,
	.next = ilc_seq_next,
	.stop = ilc_seq_stop,
	.show = ilc_seq_show,
};

static int ilc_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &ilc_seq_ops);
}

static const struct file_operations ilc_proc_ops = {
	.owner = THIS_MODULE,
	.open = ilc_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};

/* Called from late in the kernel initialisation sequence, once the
 * normal memory allocator is available. */
static int __init ilc_proc_init(void)
{
	struct proc_dir_entry *entry = create_proc_entry("ilc", S_IRUGO, NULL);

	if (entry)
		entry->proc_fops = &ilc_proc_ops;

	return 0;
}
module_init(ilc_proc_init);

#endif /* CONFIG_PROC_FS */

#ifdef CONFIG_PM
static int ilc_resume_from_hibernation(struct device *dev, void *_data)
{
	struct drv_ilc_data *data = (struct drv_ilc_data *)dev->driver_data;
	struct stm_plat_ilc3_data *pdata = (struct stm_plat_ilc3_data *)
		dev->platform_data;
	unsigned long flag;
	int i, irq;
	local_irq_save(flag);
	for (i = 0; i < pdata->num_input; ++i) {
		irq = i + pdata->first_irq;
		ILC_SET_PRI(i, data->irq_data[i].priority);
		ILC_SET_TRIGMODE(i, data->irq_data[i].trigger_mode);
		if (ilc_is_used(&data->irq_data[i])) {
			startup_ilc_irq(irq);
			if (ilc_is_enabled(&data->irq_data[i]))
				unmask_ilc_irq(irq);
			else
				mask_ilc_irq(irq);
			}
		}
	local_irq_restore(flag);
	return 0;
}

static int ilc_sysdev_suspend(struct sys_device *dev, pm_message_t state)
{
	static pm_message_t prev_state;
	int ret = 0;
	if (state.event == PM_EVENT_ON &&
	    prev_state.event == PM_EVENT_FREEZE)
		ret = driver_for_each_device(&ilc_driver.driver, NULL, NULL,
			ilc_resume_from_hibernation);

	prev_state = state;
	return ret;
}

static int ilc_sysdev_resume(struct sys_device *dev)
{
	return ilc_sysdev_suspend(dev, PMSG_ON);
}

static struct sysdev_driver ilc_sysdev_driver = {
	.suspend = ilc_sysdev_suspend,
	.resume = ilc_sysdev_resume,
};

static int __init ilc_sysdev_init(void)
{
	return sysdev_driver_register(&cpu_sysdev_class, &ilc_sysdev_driver);
}
#else
#define ilc_sysdev_init()			(0)
#endif

static int __init ilc_init(void)
{
	int ret;
	ret = platform_driver_register(&ilc_driver);
	if (!ret)
		ilc_sysdev_init();
	return ret;
}

arch_initcall(ilc_init);
