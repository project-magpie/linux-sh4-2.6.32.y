#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/seq_file.h>i
#include <linux/stm/coprocessor.h>
#include <linux/stm/sysconf.h>
#include <asm-generic/sections.h>
#include <asm/io.h>

#define N_COPROC	2
struct coproc_board_info coproc_info = {
	.name = "st231",
	.max_coprs = N_COPROC,
};

coproc_t coproc[N_COPROC];

static struct sysconf_field* copro_reset_out;

struct cpu_reg {
	struct sysconf_field* boot;
	struct sysconf_field* reset;
};
static struct cpu_reg cpu_regs[N_COPROC];

int coproc_cpu_open(coproc_t * cop)
{
	return (0);
}

int __init coproc_cpu_init(coproc_t * cop)
{
	unsigned int id = cop->pdev.id;

	BUG_ON(id >= coproc_info.max_coprs);
	if(!copro_reset_out)
	if(!(copro_reset_out=sysconf_claim(SYS_CFG, 9, 27, 27, NULL))){
		printk(KERN_ERR"Error on sysconf_claim SYS_CFG_9\n");
		return 1;
		}

	if(!cpu_regs[id].boot)
	if(!(cpu_regs[id].boot = sysconf_claim(SYS_CFG,26+id*2, 0, 31, NULL))){
		printk(KERN_ERR"Error on sysconf_claim SYS_CFG_%u\n",26+id*2);
		return 1;
		}

	if(!cpu_regs[id].reset)
	if(!(cpu_regs[id].reset = sysconf_claim(SYS_CFG, 27+id*2, 0,31, NULL))){
		printk(KERN_ERR"Error on sysconf_claim SYS_CFG_%u\n",27+id*2);
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
	DPRINTK(">>> platform: st231.%u start from 0x%x...\n",id, bootAddr);
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

