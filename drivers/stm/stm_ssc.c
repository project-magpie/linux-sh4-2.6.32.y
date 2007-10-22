/*
   -------------------------------------------------------------------------
   stm_ssc.c
   -------------------------------------------------------------------------
   STMicroelectronics
   -------------------------------------------------------------------------
 *  Copyright (C) 2006  Virlinzi Francesco
 *                   <francesco.virlinzi@st.com>
 *
 * 23 August 2006 - Modified to support the 2.6.17 kernel version
 *      Virlinzi Francesco <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */

#include "stm_ssc.h"
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <asm/semaphore.h>
#include <asm/clock.h>
#include <linux/stm/soc.h>
#include <linux/wait.h>
#include <linux/module.h>
#include <linux/ioport.h>
#include <asm/io.h>

#undef dgb_print

#ifdef  CONFIG_STM_SSC_DEBUG
#define dgb_print(fmt, args...)  printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#define dgb_print(fmt, args...)
#endif

#if defined(CONFIG_CPU_SUBTYPE_STI5528) ||  defined(CONFIG_CPU_SUBTYPE_STM8000)

#define NR_SSC_BUSSES 2

#elif defined(CONFIG_CPU_SUBTYPE_STB7100) || defined(ARCH_ST200)

#define NR_SSC_BUSSES 3

#elif defined(CONFIG_CPU_SUBTYPE_STX7200)

#define NR_SSC_BUSSES 5

#else
#error Need to configure the right SSC number devices on this chip
#endif


/*
 *  Here we alloc the right number of busses
 */
static struct ssc_t ssc_device[NR_SSC_BUSSES];

static struct platform_device *ssc_device_data;

unsigned int ssc_device_available()
{
	dgb_print("\n");
	return ssc_device_data->num_resources / 2;
}

unsigned int ssc_get_clock()
{
	return clk_get_rate(clk_get(NULL, "comms_clk"));
}

struct ssc_t *ssc_device_request(unsigned int device_id)
{
	dgb_print("\n");
	if (device_id >= ssc_device_data->num_resources / 2)
		return NULL;

	return &(ssc_device[device_id]);
}

unsigned int ssc_capability(unsigned int ssc_id)
{
	struct plat_ssc_data *info;
	dgb_print("\n");

	if (ssc_id >= ssc_device_data->num_resources / 2)
		return 0;

	info = (struct plat_ssc_data *)
	    (ssc_device_data->dev.platform_data);
	return (info->capability >> (2 * ssc_id)) &
	    (SSC_I2C_CAPABILITY | SSC_SPI_CAPABILITY);
}

void ssc_request_bus(struct ssc_t *ssc_bus, void (*irq_function) (void *),
		     void *irq_data)
{
	dgb_print("\n");
	mutex_lock(&(ssc_bus->mutex_bus));
	ssc_bus->irq_function = irq_function;
	ssc_bus->irq_private_data = irq_data;
}

void ssc_release_bus(struct ssc_t *ssc_bus)
{
   dgb_print("\n");
	ssc_bus->irq_function = NULL;
	ssc_bus->irq_private_data = NULL;
	mutex_unlock(&(ssc_bus->mutex_bus));
}

static irqreturn_t ssc_handler(int this_irq, void *dev_id, struct pt_regs *regs)
{
	struct ssc_t *ssc_bus = (struct ssc *)dev_id;
	if (ssc_bus->irq_function != NULL)
		ssc_bus->irq_function(ssc_bus->irq_private_data);
	return IRQ_HANDLED;
}

static int __init stm_ssc_probe(struct device *dev)
{
   dgb_print("\n");
	ssc_device_data = to_platform_device(dev);

	if (!ssc_device_data->name ){
		printk(KERN_ERR
		       "Device probe failed.  Check your kernel SoC config!!\n");
	}
	return 0;
}

static int stm_ssc_remove(struct device *dev)
{
    dgb_print("\n");
    return 0;
}

static void stm_ssc_shutdown(struct device *dev)
{
    dgb_print("\n");
    return;
}

static struct device_driver ssc_driver = {
	.name = "ssc",
   .owner = THIS_MODULE,
	.bus = &platform_bus_type,
	.probe = stm_ssc_probe,
   .shutdown = stm_ssc_shutdown,
   .remove = stm_ssc_remove,
};

/*
 * Request the IO memory
 * Remap   the IO memory
 * Request the right PIO pins
 * Request the interrupt line.
 */
static int ssc_hw_resrc_init(struct ssc_t *ssc_data)
{
	struct resource *res;
	struct plat_ssc_data *info;
	struct plat_ssc_pio_t *pio_info;
	unsigned char pio_bank;
	unsigned char pio_line;

	dgb_print("\n");

	info = (struct plat_ssc_data *)
	    (ssc_device_data->dev.platform_data);
	pio_info = info->pio;
/*1.    IO Mem*/
	res =
	    platform_get_resource(ssc_device_data, IORESOURCE_MEM,
				  ssc_data->ssc_id);
	if (!res) {
		printk(KERN_ERR
		       "Error on platform_get_resource mem settings\n");
		return -ENODEV;
	}
	if (!request_mem_region(res->start, res->end - res->start, "ssc")) {
		printk(KERN_ERR "ERROR: ssc %d Request MEM Region NOT Done\n",
		       ssc_data->ssc_id);
		return -ENODEV;
	}
	dgb_print("ssc bus %d Request MEM Region Done\n", ssc_data->ssc_id);
	ssc_data->base = ioremap(res->start, res->end - res->start);

	dgb_print("ssc bus %d Request MEM Region Remapping Done\n",
		ssc_data->ssc_id);
/* 2.   Request of PIO pins */

/* 2.1  Pio clock */
	pio_bank = pio_info[ssc_data->ssc_id].sclbank;
	pio_line = pio_info[ssc_data->ssc_id].sclpin;

	ssc_data->pio_clk = stpio_request_pin(pio_bank, pio_line,
					      "ssc clock", STPIO_ALT_BIDIR);
	if (!ssc_data->pio_clk) {
		printk(KERN_ERR
		       "ERROR: ssc bus %d Request PIO clock pins not Done\n",
		       ssc_data->ssc_id);
		goto release_mem_region;
	}
	dgb_print("ssc bus %d Request Clock: Pin%d[%d] Done\n", ssc_data->ssc_id,
		pio_bank, pio_line);
/* 2.2  Pio Data out */
	pio_bank = pio_info[ssc_data->ssc_id].sdoutbank;
	pio_line = pio_info[ssc_data->ssc_id].sdoutpin;

	ssc_data->pio_data = stpio_request_pin(pio_bank, pio_line,
					       "ssc data", STPIO_ALT_BIDIR);
	if (!ssc_data->pio_data) {
		printk(KERN_ERR
		       "ERROR: ssc bus %d Request PIO Data pins not Done\n",
		       ssc_data->ssc_id);
		goto release_pio_clk;
	}
	dgb_print("ssc bus %d Request Data: Pin%d[%d] Done\n",
		ssc_data->ssc_id, pio_bank, pio_line);

/* 2.3 Pio Data in */
	ssc_data->pio_data_in = NULL;

	pio_bank = pio_info[ssc_data->ssc_id].sdinbank;
	pio_line = pio_info[ssc_data->ssc_id].sdinpin;

	if (pio_bank != 0xff) {
		ssc_data->pio_data_in =
		    stpio_request_pin(pio_bank,
				      pio_line, "ssc data in", STPIO_ALT_BIDIR);
		if (ssc_data->pio_data_in == NULL) {
			printk(KERN_ERR
			       "ERROR: ssc %d Request PIO DataIN pins not Done\n",
			       ssc_data->ssc_id);
			goto release_pio_data;
		}
		dgb_print("ssc bus %d Request DataIN Pin%d[%d] Done\n",
			ssc_data->ssc_id, pio_bank, pio_line);
	}
	/* 3.  Request of IRQ */
	res =
	    platform_get_resource(ssc_device_data, IORESOURCE_IRQ,
				  ssc_data->ssc_id);
	if (!res) {
		printk(KERN_ERR
		       "Error on platform_get_resource irq settings\n");
		goto release_pio_data_in;
	}
	if (request_irq(res->start, ssc_handler, SA_INTERRUPT, "ssc",
			ssc_data) < 0) {
		printk(KERN_ERR "ERROR: ssc bus %d Request IRQ NOT Done\n",
		       ssc_data->ssc_id);
		goto release_pio_data_in;
	}
	dgb_print("ssc bus %d Request IRQ %d Done\n",
		ssc_data->ssc_id, res->start);
        ssc_data->dev.driver = &ssc_driver;
	ssc_data->dev.parent = &platform_bus ;
        sprintf(ssc_data->dev.bus_id, "ssc-%d", ssc_data->ssc_id);
	if ( device_register(&ssc_data->dev)<0){
           printk(KERN_ERR "ERROR: Incapable to register ssc device\n");
           goto release_irq;
        }
	return 0;
      release_irq:
        free_irq(res->start, ssc_handler);
      release_pio_data_in:
	if (ssc_data->pio_data_in != NULL)
		stpio_free_pin(ssc_data->pio_data_in);
      release_pio_data:
	stpio_free_pin(ssc_data->pio_data);
      release_pio_clk:
	stpio_free_pin(ssc_data->pio_clk);
      release_mem_region:
	res =
	    platform_get_resource(ssc_device_data, IORESOURCE_MEM,
				  ssc_data->ssc_id);

	release_mem_region(res->start, res->end - res->start);

	return -ENODEV;
}

static void ssc_hw_release(struct ssc_t *ssc_data)
{
	struct resource *res;
	dgb_print("\n");
	res =
	    platform_get_resource(ssc_device_data, IORESOURCE_MEM,
				  ssc_data->ssc_id);

	release_mem_region(res->start, res->end - res->start);

	res =
	    platform_get_resource(ssc_device_data, IORESOURCE_IRQ,
				  ssc_data->ssc_id);
	free_irq(res->start, ssc_data);

	stpio_free_pin(ssc_data->pio_data);
	stpio_free_pin(ssc_data->pio_clk);
	if (!(ssc_data->pio_data_in))
		stpio_free_pin(ssc_data->pio_data_in);
}

static int __init ssc_bus_init(void)
{
	unsigned int index;
	struct ssc_t *pssc_bus;
        dgb_print("\n");
	driver_register(&ssc_driver);
	dgb_print("ssc driver registered\n");
	if (!ssc_device_data) {
		printk(KERN_ERR "Error on ssc platform settings\n");
		return -ENODEV;
	}
	for (index = 0; index < (ssc_device_data->num_resources) / 2; ++index) {
		pssc_bus = &(ssc_device[index]);
		pssc_bus->ssc_id = index;
		mutex_init(&(pssc_bus->mutex_bus));
		init_waitqueue_head(&(pssc_bus->wait_queue));
		ssc_hw_resrc_init(pssc_bus);
	}
	printk(KERN_INFO "stssc layer initialized\n");
	return 0;
}

static void __exit ssc_bus_exit(void)
{
	unsigned int index;

	dgb_print("\n");

	if (!ssc_device_data)
		return;
	for (index = 0; index < (ssc_device_data->num_resources) / 2; ++index) {
		ssc_hw_release(&(ssc_device[index]));
	}
}

/*
 * If modules is NOT defined when this file is compiled, then the MODULE_*
 * macros will resolve to nothing
 */

MODULE_AUTHOR("STMicroelectronics  <www.st.com>");
MODULE_DESCRIPTION("stssc bus adapter routines for ssc device");
MODULE_LICENSE("GPL");

/* Called when module is loaded or when kernel is initialized.
 * If MODULES is defined when this file is compiled, then this function will
 * resolve to init_module (the function called when insmod is invoked for a
 * module).  Otherwise, this function is called early in the boot, when the
 * kernel is intialized
 */

module_init(ssc_bus_init);

module_exit(ssc_bus_exit);
