/*
 * arch/sh/kernel/cpu/sh4/cpufreq-stx7200.c
 *
 * Cpufreq driver for the ST40 processors.
 * Version: 0.1 (7 Jan 2008)
 *
 * Copyright (C) 2008 STMicroelectronics
 * Author: Francesco M. Virlinzi <francesco.virlinzi@st.com>
 *
 * This program is under the terms of the
 * General Public License version 2 ONLY
 *
 */
#include <linux/types.h>
#include <linux/cpufreq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <linux/cpumask.h>
#include <linux/smp.h>
#include <linux/sched.h>	/* set_cpus_allowed() */
#include <linux/stm/pm.h>

#include <asm/processor.h>
#include <asm/system.h>
#include <asm/freq.h>
#include <asm/io.h>
#include <asm/clock.h>

#undef  dbg_print
#ifdef  CONFIG_CPU_FREQ_DEBUG
#define dbg_print(fmt, args...)  printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#define dbg_print(fmt, args...)
#endif

static struct clk *pll0_clk;
static struct clk *sh4_clk;
static struct clk *sh4_ic_clk;
static struct clk *module_clk;
static unsigned long clk_iomem;

static inline unsigned long _1_ms_lpj(void)
{
	return clk_get_rate(sh4_clk) / (1000 * 2);
}

static struct cpufreq_frequency_table *cpu_freqs;

#define CLKGNA_DIV_CFG		( clk_iomem + 0x10 )
#define CKGA_CLKOUT_SEL 	( clk_iomem + 0x18)
#define SH4_CLK_MASK		( 0x1ff << 1 )
/*
 *	value: 0  1  2  3  4  5  6     7
 *	ratio: 1, 2, 3, 4, 6, 8, 1024, 1
 */
static unsigned long sh4_ratio[] = {
/*	  cpu	   bus	    per */
	(0 << 1) | (1 << 4) | (3 << 7),	/* 1:1 - 1:2 - 1:4 */
	(1 << 1) | (3 << 4) | (3 << 7),	/* 1:2 - 1:4 - 1:4 */
	(3 << 1) | (5 << 4) | (5 << 7)	/* 1:4 - 1:8 - 1:8 */
};

static void st_cpufreq_update_clocks(unsigned int set, int propagate)
{
	static unsigned int sh_current_set;
	unsigned long clks_address = CLKGNA_DIV_CFG;
	unsigned long clks_value = ctrl_inl(clks_address);
	unsigned long flag;
	unsigned long l_p_j = _1_ms_lpj();

	l_p_j >>= 3;		/* l_p_j = 125 usec (for each HZ) */

	if (set > sh_current_set) {	/* down scaling... */
		l_p_j >>= 1;
		if ((set + sh_current_set) == 2)
			l_p_j >>= 1;
	} else {		/* up scaling... */
		l_p_j <<= 1;
		if ((set + sh_current_set) == 2)
			l_p_j <<= 1;
	}

	clks_value &= ~SH4_CLK_MASK;
	clks_value |= sh4_ratio[set];

	local_irq_save(flag);
	asm volatile (".balign	32	\n"
		      "mov.l	%1, @%0	\n"
		      "tst	%2, %2	\n"
		      "1:		\n"
		      "bf/s	1b	\n"
		      " dt	%2	\n"
		 ::"r" (clks_address),	// 0
		      "r"(clks_value),	// 1
		      "r"(l_p_j)	// 2
		      :"t", "memory");

	dbg_print("\n");
	sh_current_set = set;
	sh4_clk->rate = (cpu_freqs[set].frequency << 3) * 125;
	if (cpu_data->cut_major < 2){
		sh4_ic_clk->rate = (cpu_freqs[set].frequency << 2) * 125;
		module_clk->rate = clk_get_rate(pll0_clk) >> 3;
		if (set == 2)
			module_clk->rate >>= 1;
/* The module_clk propagation can create a race condition
 * on the tmu0 during the suspend/resume...
 * The race condition basically leaves the TMU0 enabled
 * with interrupt enabled and the system immediately resume
 * after a suspend
 */
		if (propagate)
			clk_set_rate(module_clk, module_clk->rate);	/* to propagate... */
	}
	local_irq_restore(flag);
}

void *__init st_cpufreq_platform_init(struct cpufreq_frequency_table
				      *_cpu_freqs)
{
	dbg_print("\n");

	if (!_cpu_freqs)
		return NULL;
	cpu_freqs = _cpu_freqs;

	pll0_clk = clk_get(NULL, "pll0_clk");
	sh4_clk = clk_get(NULL, "sh4_clk");
	clk_iomem = (unsigned long)clk_get_iomem();

	if (!pll0_clk) {
		printk(KERN_ERR "ERROR: on clk_get(pll0_clk)\n");
		return NULL;
	}
	if (!sh4_clk) {
		printk(KERN_ERR "ERROR: on clk_get(sh4_clk)\n");
		return NULL;
	}
	if (!clk_iomem)
		return NULL;
	if (cpu_data->cut_major < 2){
		sh4_ic_clk = clk_get(NULL, "sh4_ic_clk");
		module_clk = clk_get(NULL, "module_clk");
		if (!sh4_ic_clk) {
			printk(KERN_ERR "ERROR: on clk_get(sh4_ic_clk)\n");
			return NULL;
		}
		if (!module_clk) {
			printk(KERN_ERR "ERROR: on clk_get(module_clk)\n");
			return NULL;
		}
	}
	ctrl_outl(0xc, CKGA_CLKOUT_SEL);	/* sh4:2 routed on SYSCLK_OUT */

	return (void *)st_cpufreq_update_clocks;
}

MODULE_LICENSE("GPL");
