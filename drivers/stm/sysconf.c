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

#include <asm/io.h>

#define DRIVER_NAME "sysconf"

struct sysconf_field {
	u16 offset;
	u8 lsb, msb;
};

static void __iomem *sysconf_base;
static int sysconf_offsets[3];
static DEFINE_SPINLOCK(sysconf_lock);

/* We need a small stash of allocations before kmalloc becomes available */
#define NUM_EARLY_FIELDS 10
static struct sysconf_field early_fields[NUM_EARLY_FIELDS];
static int next_early_field = 0;

static struct sysconf_field* field_alloc(void)
{
	if (next_early_field < NUM_EARLY_FIELDS)
		return &early_fields[next_early_field++];

	return kzalloc(sizeof(struct sysconf_field), GFP_KERNEL);
}

struct sysconf_field* sysconf_claim(int regtype, int regnum, int lsb, int msb,
				    const char *dev)
{
	struct sysconf_field *field;

	field = field_alloc();
	if (!field)
		return NULL;

	field->offset = sysconf_offsets[regtype] + (regnum * 4);
	field->lsb = lsb;
	field->msb = msb;

	return field;
}

void sysconf_write(struct sysconf_field *field, u64 value)
{
	void __iomem *reg;
	int field_bits;	/* Actually number of bits -1 */

	reg = sysconf_base + field->offset;
	field_bits = field->msb - field->lsb;

	if (field_bits == 31) {
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

u64 sysconf_read(struct sysconf_field *field)
{
	void __iomem *reg;
	int field_bits;	/* Actually number of bits -1 */
	u32 tmp;

	reg = sysconf_base + field->offset;
	tmp = readl(reg);
	field_bits = field->msb - field->lsb;

	if (field_bits != 31) {
		tmp >>= field->lsb;
		tmp &= (1 << field_bits) -1;
	}

	return (u64)tmp;
}

/* This is called early to allow board start up code to use sysconf
 * registers (in particular console devices). */
void __init sysconf_early_init(struct platform_device* pdev)
{
	int size = pdev->resource[0].end - pdev->resource[0].start + 1;
	struct plat_sysconf_data *data = pdev->dev.platform_data;

	sysconf_base = ioremap(pdev->resource[0].start, size);

	/* I don't like panicing here, but it we failed to ioremap, we
	 * probably don't have any way to report things have gone
	 * wrong. So a panic here at least gives some hope of being able to
	 * debug the problem.
	 */
	if (!sysconf_base)
		panic("Unable to ioremap sysconf registers");

	sysconf_offsets[SYS_DEV] = data->sys_device_offset;
	sysconf_offsets[SYS_STA] = data->sys_sta_offset;
	sysconf_offsets[SYS_CFG] = data->sys_cfg_offset;
}

static int __init sysconf_probe(struct platform_device *pdev)
{
	int size = pdev->resource[0].end - pdev->resource[0].start + 1;

	if (!request_mem_region(pdev->resource[0].start, size, pdev->name))
		return -EBUSY;

	/* Have we already been set up through sysconf_init? */
	if (sysconf_base)
		return 0;

	sysconf_early_init(pdev);

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
	return platform_driver_register(&sysconf_driver);
}

arch_initcall(sysconf_init);
