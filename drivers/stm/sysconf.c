/*
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/stm/soc.h>
#include <linux/stm/sysconf.h>
#include <linux/list.h>
#include <asm/io.h>

#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif

#define DRIVER_NAME "sysconf"

struct sysconf_field {
	u16 offset;
	u8 lsb, msb;
	char* dev;
	struct list_head list;
};

static void __iomem *sysconf_base;
static int sysconf_offsets[3];
static DEFINE_SPINLOCK(sysconf_lock);
static LIST_HEAD(sysconf_fields);
static struct platform_device *sysconf_pdev;

/* We need a small stash of allocations before kmalloc becomes available */
#define NUM_EARLY_FIELDS 64
#define EARLY_BITS_MAPS_SIZE	DIV_ROUND_UP(NUM_EARLY_FIELDS, 32)
static struct sysconf_field early_fields[NUM_EARLY_FIELDS];
static unsigned long early_fields_map[EARLY_BITS_MAPS_SIZE];

static struct sysconf_field* field_alloc(void)
{
	int bank;
	int first_free;
	struct sysconf_field *tmp;

	for (bank = 0; bank < ARRAY_SIZE(early_fields_map); ++bank) {
		first_free = ffz(early_fields_map[bank]);
		if (first_free < 32)
			break;
	}
	if (first_free < 32) { /* found! */
		spin_lock(&sysconf_lock);
		early_fields_map[bank] |= (1 << (first_free)); /* set as used!*/
		spin_unlock(&sysconf_lock);
		tmp = &early_fields[first_free  + (bank * 32)];
		return tmp;
	}

	return kzalloc(sizeof(struct sysconf_field), GFP_KERNEL);
}

struct sysconf_field* sysconf_claim(int regtype, int regnum, int lsb, int msb,
				    const char *dev)
{
	struct sysconf_field *field, *pfield = NULL;
	struct list_head *node;
	int offset = sysconf_offsets[regtype] + (regnum * 4);

	field = field_alloc();
	if (!field)
		return NULL;

	list_for_each(node, &sysconf_fields) {
		pfield = container_of(node, struct sysconf_field, list);
		if (pfield->offset < offset)
			continue;
		if (pfield->offset > offset)
			break;
		if (pfield->lsb > msb)
			continue;
		if (pfield->msb < lsb)
			break;
	}

	field->offset = offset;
	field->lsb = lsb;
	field->msb = msb;
	field->dev = (char *)dev;

	spin_lock(&sysconf_lock);
	list_add_tail(&field->list, node);
	spin_unlock(&sysconf_lock);

	return field;
}
EXPORT_SYMBOL(sysconf_claim);

void sysconf_release(struct sysconf_field *field)
{
	if (field >= early_fields &&
	    field <= &early_fields[NUM_EARLY_FIELDS]){
		int bank, idx;
		bank = (&early_fields[32] > field ? 0 : 1);
		idx = ((unsigned long)field -(unsigned long)&early_fields[bank])
			% sizeof(struct sysconf_field);
		spin_lock(&sysconf_lock);
		early_fields_map[bank] &= ~(1<<idx); /* set as free */
		list_del(&field->list);
		spin_unlock(&sysconf_lock);
		return ;
	}
	spin_lock(&sysconf_lock);
	list_del(&field->list);
	spin_unlock(&sysconf_lock);
	kfree(field);
}
EXPORT_SYMBOL(sysconf_release);

void sysconf_write(struct sysconf_field *field, u64 value)
{
	void __iomem *reg;
	int field_bits;	/* Number of bits */

	reg = sysconf_base + field->offset;
	field_bits = field->msb - field->lsb + 1;

	if (field_bits == 32) {
		/* Operating on the whole register, nice and easy */
		writel(value, reg);
	} else {
		u32 reg_mask;
		u32 tmp;

		reg_mask = ~(((1 << field_bits) -1) << field->lsb);
		spin_lock(&sysconf_lock);
		tmp = readl(reg);
		tmp &= reg_mask;
		tmp |= value << field->lsb;
		writel(tmp, reg);
		spin_unlock(&sysconf_lock);
	}
}
EXPORT_SYMBOL(sysconf_write);

u64 sysconf_read(struct sysconf_field *field)
{
	void __iomem *reg;
	int field_bits;	/* Number of bits -1 */
	u32 tmp;

	reg = sysconf_base + field->offset;
	tmp = readl(reg);
	field_bits = field->msb - field->lsb + 1;

	if (field_bits != 32) {
		tmp >>= field->lsb;
		tmp &= (1 << field_bits) -1;
	}

	return (u64)tmp;
}
EXPORT_SYMBOL(sysconf_read);

void *sysconf_address(struct sysconf_field *field)
{
	return sysconf_base + field->offset;
}
EXPORT_SYMBOL(sysconf_address);

unsigned long sysconf_mask(struct sysconf_field *field)
{
	int field_bits = field->msb - field->lsb + 1;
	if (field_bits == 32)
		return 0xffffffff;
	return ((1 << field_bits) -1) << field->lsb;
}
EXPORT_SYMBOL(sysconf_mask);

#ifdef CONFIG_PM
int sysconf_pm_state(pm_message_t state)
{
	unsigned long size, i;
	static unsigned long prev_state = PM_EVENT_ON;
	static long *saved_data;

	size = sysconf_pdev->resource[0].end - sysconf_pdev->resource[0].start
		- sysconf_offsets[SYS_CFG]; /* how many bytes I need */

	switch (state.event) {
	case PM_EVENT_ON:
		if (prev_state == PM_EVENT_FREEZE && saved_data) {
			for (i = 0; i < size; i += sizeof(long))
				writel(saved_data[i/sizeof(long)],sysconf_base + i
					+ sysconf_offsets[SYS_CFG]);
			kfree(saved_data);
		}
	case PM_EVENT_SUSPEND:
		prev_state = state.event;
		break;
	case PM_EVENT_FREEZE:
		prev_state = state.event;
		saved_data = kmalloc(size, GFP_NOWAIT);
                if (!saved_data) {
                        printk(KERN_ERR "Unable to freeze the sysconf registers\n");
                        return -1;
                }
		for (i = 0; i < size; i += sizeof(long))
			saved_data[i/sizeof(long)] = readl(sysconf_base + i +
				sysconf_offsets[SYS_CFG]);
		break;
	}
	return 0;
}
#endif

/* This is called early to allow board start up code to use sysconf
 * registers (in particular console devices). */
void __init sysconf_early_init(struct platform_device* pdev)
{
	int size = pdev->resource[0].end - pdev->resource[0].start + 1;
	struct plat_sysconf_data *data = pdev->dev.platform_data;

#if 1
	sysconf_base = ioremap(pdev->resource[0].start, size);

	/* I don't like panicing here, but it we failed to ioremap, we
	 * probably don't have any way to report things have gone
	 * wrong. So a panic here at least gives some hope of being able to
	 * debug the problem.
	 */
	if (!sysconf_base)
		panic("Unable to ioremap sysconf registers");
#else
	set_fixmap_nocache(FIX_SYSCONF, pdev->resource[0].start);
	sysconf_base = fix_to_virt(FIX_SYSCONF);
#endif

	sysconf_offsets[SYS_DEV] = data->sys_device_offset;
	sysconf_offsets[SYS_STA] = data->sys_sta_offset;
	sysconf_offsets[SYS_CFG] = data->sys_cfg_offset;
}

#ifdef CONFIG_PROC_FS
static void *sysconf_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	struct list_head *tmp;
	union {
		loff_t value;
		long parts[2];
	} ltmp;

	ltmp.value = *pos;
	tmp = (struct list_head *)ltmp.parts[0];
	tmp = tmp->next;
	ltmp.parts[0] = (long)tmp;

	*pos = ltmp.value;

	if (tmp == &sysconf_fields)
		return NULL; /* No more to read */
	return pos;
}

void *sysconf_seq_start(struct seq_file *s, loff_t *pos)
{
	if (!*pos) { /* first call! */
		union {
			loff_t value;
			long parts[2];
		} ltmp;
		ltmp.parts[0] = (long) sysconf_fields.next;
		*pos = ltmp. value;
		seq_puts(s, "------System Registers-----\n");
		seq_puts(s, " Type Num Bits    Owner\n");
		seq_puts(s, "---------------------------\n");
		return pos;
	}
	--(*pos); /* to realign *pos value! */

	return sysconf_seq_next(s, NULL, pos);
}

static int sysconf_seq_show(struct seq_file *s, void *v)
{
	unsigned long *l = (unsigned long *)v;
	struct list_head *tmp = (struct list_head *)(*l);
	struct sysconf_field *field =
			container_of(tmp, struct sysconf_field, list);
	int type;

	seq_printf(s, "+ ");
	if (field->offset >= sysconf_offsets[SYS_CFG]) {
		seq_printf(s, "Cfg ");
		type = SYS_CFG;
	} else if (field->offset >= sysconf_offsets[SYS_STA]) {
		seq_printf(s, "Sta ");
		type = SYS_STA;
	 } else {
		seq_printf(s, "Dev ");
		type = SYS_DEV;
	}

	seq_printf(s, "%2d [%2d:%2d]",
		(field->offset - sysconf_offsets[type])/4,
		field->msb, field->lsb);

	if (field->dev)
		seq_printf(s, ": %s\n", field->dev);
	else
		seq_printf(s, "\n");

	return 0;
}

static void sysconf_seq_stop(struct seq_file *s, void *v)
{
}

static struct seq_operations sysconf_seq_ops = {
	.start = sysconf_seq_start,
	.next = sysconf_seq_next,
	.stop = sysconf_seq_stop,
	.show = sysconf_seq_show,
};

static int sysconf_proc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &sysconf_seq_ops);
}

static struct file_operations sysconf_proc_ops = {
	.owner = THIS_MODULE,
	.open = sysconf_proc_open,
	.read = seq_read,
	.llseek = seq_lseek,
	.release = seq_release,
};
#endif

static int __init sysconf_probe(struct platform_device *pdev)
{
	int size = pdev->resource[0].end - pdev->resource[0].start + 1;

	if (!request_mem_region(pdev->resource[0].start, size, pdev->name))
		return -EBUSY;

	sysconf_pdev = pdev;
	/* Have we already been set up through sysconf_init? */
	if (sysconf_base)
		return 0;

#if 1
	sysconf_early_init(pdev);
#else
	sysconf_base = ioremap(pdev->resource[0].start, size);
	if (!sysconf_base)
		return -ENOMEM;
#endif

	return 0;
}

static struct platform_driver sysconf_driver = {
	.probe		= sysconf_probe,
	.driver	= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init sysconf_init(void)
{
#ifdef CONFIG_PROC_FS
	struct proc_dir_entry *entry =
		create_proc_entry("sysconf", S_IRUGO, NULL);
	if (entry)
		entry->proc_fops = &sysconf_proc_ops;
#endif
	return platform_driver_register(&sysconf_driver);
}

arch_initcall(sysconf_init);
