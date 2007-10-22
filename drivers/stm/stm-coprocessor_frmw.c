/*
 * Copyright (C) 2006 STMicroelectronics
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * Interfaces (where required) the co-processors on ST platforms based
 * on multiprocessor architecture, for embedded products like Set-top-Box
 * DVD, etc...
 *
 */

#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/bootmem.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/device.h>
#include <linux/firmware.h>

#include <linux/delay.h>
#include <linux/mm.h>

#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif

#include <linux/stm/coprocessor.h>
#include <asm/types.h>
#include <asm/sections.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/irq.h>

#undef dbg_print

#ifdef CONFIG_COPROCESSOR_DEBUG
#define dbg_print(fmt, args...)  printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#define dbg_print(fmt, args...)
#endif

/* ---------------------------------------------------------------------------
 *     Local (declared out of order) functions
 * ------------------------------------------------------------------------ */

static int __init parse_coproc_mem(char *from);
static int __init proc_st_coproc_init(void);

/* ---------------------------------------------------------------------------
 *    Exported and Imported
 * ------------------------------------------------------------------------ */

extern unsigned long memory_start, memory_end;

/* ---------------------------------------------------------------------------
 * 		Co-processor: Hardware dependent support
 * This includes:
 *    - per platform device and memory addresses
 *    - platform dependent macros
 *    - HW dependent actions required by the generic APIs: Init,
 *      Open, Release, Ioctl (to reset, trigger the start (grant),
 *      peek and poke, etc...) functions
 * ------------------------------------------------------------------------ */

extern struct coproc_board_info coproc_info;

/* ---------------------------------------------------------------------------
 *    Local data structure
 * ------------------------------------------------------------------------ */

extern coproc_t coproc[];	/* The maximum number of coprocessors
				 * depends on platform type  */

/* ---------------------------------------------------------------------------
 * 			Co-processor driver APIs
 * ---------------------------------------------------------------------------
 */
#define minor_2_firmware(min)  ( (min) & 0x3f      )
#define minor_2_device(min)    ( ((min) >> 6) & 0x3)

static int st_coproc_open(struct inode *inode, struct file *file)
{
	/*
	 ** use minor number (ID) to access the current coproc. descriptor
	 */
	char firm_file[FIRMWARE_NAME_MAX];
	char number[10];
	unsigned long minor = MINOR((file)->f_dentry->d_inode->i_rdev);
	unsigned long id_device = minor_2_device(minor);
	unsigned long id_firmware = minor_2_firmware(minor);
	struct firmware *fw = NULL;
	int res;

	coproc_t *cop = &coproc[id_device];

	if (cop->control & COPROC_IN_USE)
		return -EBUSY;
	/* Now call the platform dependent open stage */
	coproc_cpu_open(cop);
/* Build the firmware file name.
 * We use the standard name: "st_firmware_XX_XX.elf"
 * to specify the device number and the firmware number
 */

	strcpy(firm_file, "st_firmware_");
	sprintf(number, "%ld", id_device);
	strcat(firm_file, number);
	strcat(firm_file, "_");
	sprintf(number, "%ld", id_firmware);
	strcat(firm_file, number);
	strcat(firm_file, ".elf");

	dbg_print("Asking the file %s for %s\n", firm_file, cop->dev.bus_id);
	if (request_firmware(&fw, firm_file, &(cop->dev)) == 0) {
		unsigned long boot_address;

		cop->control |= COPROC_IN_USE;
		/* move the firmware in the coprocessor memory */
		dbg_print("Received firmware size %d bytes\n", fw->size - 4);
		dbg_print("cop->ram_size    = 0x%x\n", cop->ram_size);
		dbg_print("cop->ram_offset  = 0x%x\n", cop->ram_offset);
		/*
		 * The last 4 bytes in the fw->data buffer
		 * aren't code.
		 * They are the boot vma (relocated) address!
		 */
		memcpy(&boot_address, (fw->data) + (fw->size - 4), 4);
		dbg_print("boot address     = 0x%x\n", boot_address);
		memcpy(cop->vma_address, fw->data, fw->size - 4);
		release_firmware(fw);
		dbg_print("Run the Firmware code\n");
		coproc_cpu_grant(cop, boot_address);	//7100 only...
		res = 0;
	} else {
		dbg_print("Error on Firmware Download\n");
		res = -EINVAL;
	}
	return res;
}

static int st_coproc_release(struct inode *inode, struct file *file)
{
	coproc_t *cop = FILE_2_COP(coproc, file);

	coproc_cpu_release(cop);
	cop->control &= ~COPROC_IN_USE;

	return 0;
}

static struct file_operations coproc_fops = {
      open:st_coproc_open,
      release:st_coproc_release
};

/* Start: ST-Coprocessor Device Attribute on SysFs*/
static ssize_t st_copro_show_running(struct device *dev, char *buf)
{
	coproc_t *cop = container_of(dev, coproc_t, dev);
	return sprintf(buf, "%d", cop->control & COPROC_IN_USE);
}

static DEVICE_ATTR(running, S_IRUGO, st_copro_show_running, NULL);

static ssize_t st_copro_show_mem_size(struct device *dev, char *buf)
{
	coproc_t *cop = container_of(dev, coproc_t, dev);
	return sprintf(buf, "0x%x", cop->ram_size);
}

static DEVICE_ATTR(mem_size, S_IRUGO, st_copro_show_mem_size, NULL);

static ssize_t st_copro_show_mem_base(struct device *dev, char *buf)
{
	coproc_t *cop = container_of(dev, coproc_t, dev);
	return sprintf(buf, "0x%x", (int)COPR_ADDR(cop, 0));
}

static DEVICE_ATTR(mem_base, S_IRUGO, st_copro_show_mem_base, NULL);
/* End: ST-Coprocessor Device Attribute SysFs*/

static int st_coproc_driver_probe(struct device *dev)
{
	if (!strncmp("st2", dev->bus_id, 3))
		return 1;
	return 0;
}
static struct device_driver st_coproc_driver = {
	.name = "st-copro",
	.owner = THIS_MODULE,
	.bus = &platform_bus_type,
	.probe = st_coproc_driver_probe,
};

static int __init st_coproc_init(void)
{
	int i;
	coproc_t *cop;
	struct device *dev;

	printk("STMicroelectronics - Coprocessors %s Init\n", coproc_info.name);

	if (driver_register(&st_coproc_driver)) {
		printk(KERN_ERR
		       "Error on ST-Coprocessor device driver registration\n");
		return (-EAGAIN);
	}

	if (register_chrdev(COPROCESSOR_MAJOR, coproc_info.name, &coproc_fops)) {
		printk("Can't allocate major %d for ST Coprocessor Devices\n",
		       COPROCESSOR_MAJOR);
		driver_unregister(&st_coproc_driver);
		return (-EAGAIN);
	}

	for (cop = &coproc[0], i = 0; i < coproc_info.max_coprs; i++, cop++) {
       /**
        ** Nodes:
        **    STm8000/ST220Eval: /dev/st220-0    c   63   0
        **                       /dev/st220-1    c   63   1
        **                       /dev/st220-2    c   63   2
        **    STb7100          : /dev/st231-0    c   63   0
        **                       /dev/st231-1    c   63   1
        **/
		cop->id = i;
		if (!cop->ram_offset) {
			printk("st-coprocessor-%d: No RAM reserved\n", cop->id);
			cop->control &= ~COPROC_SPACE_ALLOCATE;
		} else {
			cop->control |= COPROC_SPACE_ALLOCATE;
			cop->vma_address =
			    (int)ioremap_nocache(cop->ram_offset, cop->ram_size);
		}
		/*
		 * Setup and Add the device entries in the SysFS
		 */
		dev = &(cop->dev);
		memset(dev, 0, sizeof(struct device));
		sprintf(cop->dev.bus_id, "%s-%d", coproc_info.name, i);
		dev->driver = &st_coproc_driver;
		dev->parent = &platform_bus;
		dev->bus = &platform_bus_type;
		if (device_register(dev))
			printk(KERN_ERR
			       "Error on ST-Coprocessor device registration\n");
		else {
			/* Add the attributes on the device */
			device_create_file(dev, &dev_attr_mem_base);
			device_create_file(dev, &dev_attr_mem_size);
			device_create_file(dev, &dev_attr_running);
		}

		/* Now complete with the platform dependent init stage */
		if (coproc_cpu_init(cop)) {
			printk(KERN_ERR
			       "CPU %d : HW dep. initialization failed!\n", i);
			return (1);
		}
	}

	proc_st_coproc_init();

	return (0);
}

static void __exit st_coproc_exit(void)
{
	dbg_print("Release coprocessor module...\n");
}

/*
 * Parse the optional kernel argument:
 *
 * ... coprocessor_mem=size_0@phis_address_0, size_1@phis_address_1
 *
 * It seems to be reasonable to assume that in a "staically partitioned
 * RAM layout", the regions of RAM assigned to each slave processor are
 * not scattered in memory!
 */
static int __init parse_coproc_mem(char *from)
{
	char *cmdl = (from);    /* start scan from '=' char */
	u_long size, addr;
	int i = 0;
	char *error_msg;
	static char size_error[] __initdata =
		KERN_ERR "st-coprocessor: Error parsing size\n";
	static char addr_error[] __initdata =
		KERN_ERR "st-coprocessor: Error parsing address\n";
	static char too_many_warn[] __initdata =
		KERN_WARNING "st-coprocessor: More regions than coprocessors\n";
	static char alloc_error[] __initdata =
		KERN_ERR "st-coprocessor: Failed to reserve memoryat 0x%08x\n";

	while (*cmdl && (i < coproc_info.max_coprs)) {
		size = memparse(cmdl, &cmdl);
		if (*cmdl != '@') {
			error_msg = size_error;
			goto args_error;
		}
		addr = memparse(cmdl+1, &cmdl);
		if (*cmdl) {
			if (*cmdl++ != ',') {
				error_msg = addr_error;
				goto args_error;
			}
		}
		coproc[i].ram_offset = addr;
		coproc[i].ram_size = size;
		++i;
	}

	if (*cmdl) {
		printk(too_many_warn);
	}

	for (i = 0; i < coproc_info.max_coprs; ++i) {
		if (coproc[i].ram_size) {
			void* mem;
			addr = coproc[i].ram_offset;
			size = coproc[i].ram_size;
			/* Switch to __alloc_bootmem_nopanic or
			 * __alloc_bootmem_core when we update thekernel. */
			mem = __alloc_bootmem(size, PAGE_SIZE, addr);
			if (mem != __va(addr)) {
				if (mem) {
					free_bootmem(virt_to_phys(mem), size);
				}
				/* At this point, if addr overlaps kernel
				 * memory, coprocessor won't be allocated.
                                 */
				if (coproc_check_area(addr, size, i, coproc))
                                        printk(alloc_error, addr);
                        }
		}
	}

	return 1;

args_error:
	printk(error_msg);
	return 1;
}

__setup("coprocessor_mem=", parse_coproc_mem);

MODULE_DESCRIPTION("Co-processor manager for multi-core devices");
MODULE_AUTHOR("STMicroelectronics Limited");
MODULE_VERSION("0.3");
MODULE_LICENSE("GPL");

module_init(st_coproc_init);
module_exit(st_coproc_exit);

#if CONFIG_PROC_FS

static int show_st_coproc(struct seq_file *m, void *v)
{
	int i;
	coproc_t *cop;

	seq_printf(m, "Coprocessors: %d  %s\n", coproc_info.max_coprs,
		   coproc_info.name);
	seq_printf(m,
		   "  CPU (dev)        Host addr.     Copr. addr.     Size\n");
	seq_printf(m,
		   "  -----------------------------------------------------------		--------\n");
	for (i = 0, cop = &coproc[0]; i < coproc_info.max_coprs; i++, cop++) {
		seq_printf(m, "  /dev/%-8s    ", cop->dev.bus_id);
		if (cop->ram_size == 0)
			seq_printf(m, "not allocated!\n");
		else
			seq_printf(m,
				   "0x%08lx     0x%08lx      0x%08x (%2d Mb)\n",
				   (HOST_ADDR(cop, 0)), COPR_ADDR(cop, 0),
				   cop->ram_size, (cop->ram_size / MEGA));
	}
	seq_printf(m, "\n");

	coproc_proc_other_info(cop, m);
	return (0);
}

static void *st_coproc_seq_start(struct seq_file *m, loff_t * pos)
{
	return (void *)(*pos == 0);
}

static void *st_coproc_seq_next(struct seq_file *m, void *v, loff_t * pos)
{
	return NULL;
}

static void st_coproc_seq_stop(struct seq_file *m, void *v)
{
}

static struct seq_operations proc_st_coproc_op = {
      start:st_coproc_seq_start,
      next:st_coproc_seq_next,
      stop:st_coproc_seq_stop,
      show:show_st_coproc,
};

static int proc_st_coproc_open(struct inode *inode, struct file *file)
{
	return seq_open(file, &proc_st_coproc_op);
}

static struct file_operations proc_st_coproc_operations = {
      open:proc_st_coproc_open,
      read:seq_read,
      llseek:seq_lseek,
      release:seq_release,
};

static int __init proc_st_coproc_init(void)
{
	struct proc_dir_entry *entry;
	entry = create_proc_entry("coprocessor", 0, NULL);
	if (entry != NULL) {
		entry->proc_fops = &proc_st_coproc_operations;
	}

	return 0;
}

#endif				/* CONFIG_PROC_FS */
