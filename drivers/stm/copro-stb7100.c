#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/mm.h>
#include <linux/seq_file.h>
#include <linux/delay.h>
#include <linux/stm/coprocessor.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/mach/coproc.h>
#include <asm/sections.h>
#include <asm/addrspace.h>

struct coproc_board_info coproc_info = {
	.name = "st231",
	.max_coprs = N_COPROC,
};

coproc_t coproc[N_COPROC];

int coproc_cpu_open(coproc_t * cop)
{
	return (0);
}

int coproc_cpu_init(coproc_t * cop)
{
	BUG_ON(cop->id >= N_COPROC);

	/*
	 * define for the STb7100 the ST231 view of the LMI base address
	 */
	return (0);
}

int coproc_cpu_grant(coproc_t * cop, unsigned long arg)
{
	u_long bootAddr;
	u_long cpu = cop->id;

	BUG_ON(cpu >= N_COPROC);

	if (arg == 0)
		bootAddr = COPR_ADDR(cop, 0);
	else
		bootAddr = arg;

	/* Now set the less meaningful bit to trigger the ST231 start */
	bootAddr |= 1;
	DPRINTK(">>> %s: ST231-%ld start from 0x%lx...\n",
		xstring(PLATFORM), cpu, bootAddr);

	/* stick it into the System configuration and... good luck! */
	writel((readl(SYSCFG_09) | 0x08000000), SYSCFG_09);
	writel(bootAddr, SYSCFG_BOOT_REG(cpu));
	writel((readl(SYSCFG_RESET_REG(cpu)) | 0x1), SYSCFG_RESET_REG(cpu));
	writel((readl(SYSCFG_RESET_REG(cpu)) & ~0x1), SYSCFG_RESET_REG(cpu));

	msleep(10);

	writel((readl(SYSCFG_09) & ~0x18000000), SYSCFG_09);

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
	u_long cpu = cop->id;

	/* Enable the ST231 CPUs to be resetted */
	writel((readl(SYSCFG_09) | 0x08000000), SYSCFG_09);

	writel((readl(SYSCFG_RESET_REG(cpu)) | 0x1), SYSCFG_RESET_REG(cpu));
	writel((readl(SYSCFG_RESET_REG(cpu)) & ~0x1), SYSCFG_RESET_REG(cpu));

	msleep(10);

	/* Disable the ST231 CPUs to be resetted */
	writel((readl(SYSCFG_09) & ~0x18000000), SYSCFG_09);

	return 0;
}

void coproc_proc_other_info(coproc_t * cop_dump, struct seq_file *s_file)
{
	return;			/* Do nothing, doesn't delete it */
}

int coproc_check_area(u_long addr, u_long size, int i, coproc_t * coproc)
{
#if 0
        if (((addr >= CONFIG_MEMORY_START) && (addr < PHYSADDR(_end))) || \
                (((addr + size) > CONFIG_MEMORY_START) && \
		(addr < CONFIG_MEMORY_START)))
        {
                coproc[i].ram_offset = coproc[i].ram_size = 0;
                return 1;
        }
#endif
        return 0;
}

