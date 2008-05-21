#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/seq_file.h>
#include <linux/stm/coprocessor.h>
#include <linux/stm/sysconf.h>
#include <asm-generic/sections.h>
#include <asm/io.h>

struct coproc_board_info coproc_info = {
	.name = "st231",
	.max_coprs = CONFIG_STM_NUM_COPROCESSOR,
};

coproc_t coproc[CONFIG_STM_NUM_COPROCESSOR];

static struct sysconf_field* copro_reset_out;

struct cpu_reg {
	struct sysconf_field* boot;
	struct sysconf_field* reset;
};
static struct cpu_reg cpu_regs[CONFIG_STM_NUM_COPROCESSOR];

int coproc_cpu_open(coproc_t * cop)
{
	return (0);
}

int __init coproc_cpu_init(coproc_t * cop)
{
	unsigned int id = cop->pdev.id;

#if defined CONFIG_CPU_SUBTYPE_STB7100
	const unsigned int boot_lookup[] =   { 28, 26 };
	const unsigned int reset_lookup[]  = { 29, 27 };
#elif defined CONFIG_CPU_SUBTYPE_STX7200
	const unsigned int boot_lookup[] =   { 28, 36, 26, 34 };
	const unsigned int reset_lookup[]  = { 29, 37, 27, 35 };
#else
#error Need to define the sysconf configuration for this CPU subtype
#endif

	BUG_ON(id >= ARRAY_SIZE(boot_lookup));
	BUG_ON(id >= coproc_info.max_coprs);

	if(!copro_reset_out)
	if(!(copro_reset_out=sysconf_claim(SYS_CFG, 9, 27, 27, NULL))){
		printk(KERN_ERR"Error on sysconf_claim SYS_CFG_9\n");
		return 1;
		}

	if(!cpu_regs[id].boot)
	if(!(cpu_regs[id].boot = sysconf_claim(SYS_CFG, boot_lookup[id], 0, 31, NULL))){
		printk(KERN_ERR"Error on sysconf_claim SYS_CFG_%u\n", boot_lookup[id]);
		return 1;
		}

	if(!cpu_regs[id].reset)
	if(!(cpu_regs[id].reset = sysconf_claim(SYS_CFG, reset_lookup[id], 0,31, NULL))){
		printk(KERN_ERR"Error on sysconf_claim SYS_CFG_%u\n", reset_lookup[id]);
		return 1;
		}

	return 0;
}

int coproc_cpu_grant(coproc_t * cop, unsigned long arg)
{
	u_long bootAddr;
	int id = cop->pdev.id;

	BUG_ON(id >= coproc_info.max_coprs);

	if (arg == 0)
		bootAddr = COPR_ADDR(cop, 0);
	else
		bootAddr = arg;
	/* Now set the less meaningful bit to trigger the ST231 start */
	bootAddr |= 1;
	DPRINTK(">>> platform: st231.%u start from 0x%x...\n",
					id, (unsigned int)bootAddr);
	/* bypass the st40 to reset only the coprocessor */
	sysconf_write(copro_reset_out, 1);

	sysconf_write(cpu_regs[id].boot, bootAddr);

	sysconf_write(cpu_regs[id].reset, sysconf_read(cpu_regs[id].reset) | 1) ;
	msleep(5);
	sysconf_write(cpu_regs[id].reset, sysconf_read(cpu_regs[id].reset) & ~1);

	msleep(10);
	/* remove the st40 bypass */
	sysconf_write(copro_reset_out, 0);
	cop->control |= COPROC_RUNNING;
	return (0);
}

int coproc_cpu_release(coproc_t * cop)
{
	/* do nothing! */
	return (0);
}

int coproc_cpu_reset(coproc_t * cop)
{
 	int id = cop->pdev.id;

 	DPRINTK("\n");
 	/* bypass the st40 to reset only the coprocessor */
 	sysconf_write(copro_reset_out,  1);
 	msleep(5);
 	sysconf_write(cpu_regs[id].reset, sysconf_read(cpu_regs[id].reset) | 1);
 	msleep(5);
 	sysconf_write(cpu_regs[id].reset, sysconf_read(cpu_regs[id].reset) & ~1);
 	msleep(10);

 	/* remove the st40 bypass */
 	sysconf_write(copro_reset_out, 0);

 	return 0;
}

void coproc_proc_other_info(coproc_t * cop_dump, struct seq_file *s_file)
{
	return;			/* Do nothing, doesn't delete it */
}

int coproc_check_area(u_long addr, u_long size, int i, coproc_t * coproc)
{
       if (((addr >= CONFIG_MEMORY_START) && (addr < __pa(_end))) || \
           (((addr + size) > CONFIG_MEMORY_START) && \
            (addr < CONFIG_MEMORY_START)))
       {
           coproc[i].ram_offset = coproc[i].ram_size = 0;
           return 1;
       }
       return 0;
}

