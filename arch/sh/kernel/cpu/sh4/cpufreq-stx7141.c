/*
 * arch/sh/kernel/cpu/sh4/cpufreq-stx7141.c
 *
 * Cpufreq driver for the ST40 processors.
 * Version: 0.1 (Aug 26 2008)
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
#include <linux/delay.h>	/* loops_per_jiffy */
#include <linux/cpumask.h>
#include <linux/smp.h>
#include <linux/sched.h>	/* set_cpus_allowed() */
#include <linux/stm/pm.h>

#include <asm/processor.h>
#include <asm/freq.h>
#include <asm/io.h>
#include <asm/clock.h>

#undef  dbg_print
#ifdef  CONFIG_CPU_FREQ_DEBUG
#include <linux/stm/sysconf.h>
#include <linux/stm/pio.h>
#define dbg_print(fmt, args...)  printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#define dbg_print(fmt, args...)
#endif

#define clk_iomem		0xfe213000      /* Clockgen A */
#define CKGA_PLL0LS_DIV_CFG(x)	(0xa10+(((x)-4)*4))
#define ST40_CLK 		(clk_iomem + CKGA_PLL0LS_DIV_CFG(4))

static struct clk *sh4_clk;
static struct cpufreq_frequency_table *cpu_freqs;
/*				1:1,	  1:2,	    1:4		*/
unsigned long st40_ratios[] = { 0x10000, 0x10001, 0x10003 };

static inline unsigned long _1_ms_lpj(void)
{
	return clk_get_rate(sh4_clk) / (1000 * 2);
}

static void st_cpufreq_update_clocks(unsigned int set,
				     int not_used_on_this_platform)
{
	static unsigned int current_set = 0;
	unsigned long flag;
	unsigned long st40_clk = ST40_CLK;
	unsigned long l_p_j = _1_ms_lpj();

	dbg_print("\n");
	l_p_j >>= 3;		/* l_p_j = 125 usec (for each HZ) */

	local_irq_save(flag);

	if (set > current_set) {	/* down scaling... */
		/* it scales l_p_j based on the new frequency */
		l_p_j >>= 1;	// 450 -> 225 or 225 -> 112.5
		if ((set + current_set) == 2)
			l_p_j >>= 1;	// 450 -> 112.5
	} else {
		/* it scales l_p_j based on the new frequency */
		l_p_j <<= 1;	// 225   -> 450 or 112.5 -> 225
		if ((set + current_set) == 2)
			l_p_j <<= 1;	// 112.5 -> 450
	}

	asm volatile (".balign  32      \n"
			      "mov.l    %1, @%0\n"      // sets the st40 clock
			      "tst      %2, %2  \n"
			      "2:               \n"
			      "bf/s     2b      \n"
			      " dt      %2      \n"
			::    "r" (st40_clk),           // 0
			      "r" (st40_ratios[set]),   // 1
			      "r" (l_p_j)               // 2
			:     "memory", "t");

	current_set = set;
	sh4_clk->rate = (cpu_freqs[set].frequency << 3) * 125;
	local_irq_restore(flag);
}

void *__init st_cpufreq_platform_init(struct cpufreq_frequency_table
				      *_cpu_freqs)
{
	dbg_print("\n");

	if (!_cpu_freqs)
		return NULL;
	cpu_freqs = _cpu_freqs;

	sh4_clk = clk_get(NULL, "sh4_clk");

	if (!sh4_clk) {
		printk(KERN_ERR "ERROR: on clk_get(sh4_clk)\n");
		return NULL;
	}

#ifdef CONFIG_CPU_FREQ_DEBUG
	{
	struct sysconf_field *sc;
	/* route the sh4/2  clock frequency */
	iowrite32(0xc ,clk_iomem+CKGA_CLKOBS_MUX1_CFG);
	stpio_request_set_pin(3,2,"clkA dbg", STPIO_ALT_OUT, 1);
	sc = sysconf_claim(SYS_CFG,19,22,23,"clkA dbg");
        sysconf_write(sc, 11);
	}
#endif
	return (void *)st_cpufreq_update_clocks;
}

MODULE_LICENSE("GPL");
