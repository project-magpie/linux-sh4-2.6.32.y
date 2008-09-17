/*
 * arch/sh/kernel/cpu/sh4/cpufreq-stm.c
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
#define dbg_print(fmt, args...)  printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#define dbg_print(fmt, args...)
#endif

static struct clk *sh4_clk;
static struct cpufreq_frequency_table cpu_freqs[] = {
	{.index = 0,},		/* Really initialised during the boot ... */
	{.index = 1,},
	{.index = 2,},
	{.frequency = CPUFREQ_TABLE_END},
};

static void (*st_cpufreq_update_clocks) (unsigned int set, int propagate);

/*
 * Here we notify other drivers of the proposed change and the final change.
 */
static int st_cpufreq_setstate(unsigned int cpu, unsigned int set)
{
	cpumask_t cpus_allowed;
	struct cpufreq_freqs freqs = {
		.cpu = cpu,
		.old = clk_get_rate(sh4_clk) / 1000,
		.new = cpu_freqs[set].frequency,
		.flags = 0,	/* with Zero the system will updates loops_per_j. automatically */
	};

	dbg_print("\n");

	if (!cpu_online(cpu)) {
		dbg_print("cpu not online\n");
		return -ENODEV;
	}
	cpus_allowed = current->cpus_allowed;
	set_cpus_allowed(current, cpumask_of_cpu(cpu));
	BUG_ON(smp_processor_id() != cpu);

	cpufreq_notify_transition(&freqs, CPUFREQ_PRECHANGE);

	st_cpufreq_update_clocks(set, 1);

	set_cpus_allowed(current, cpus_allowed);

	cpufreq_notify_transition(&freqs, CPUFREQ_POSTCHANGE);	/* updates the loops_per_jiffies */

	return 0;
}

static int st_cpufreq_init(struct cpufreq_policy *policy)
{
	if (!cpu_online(policy->cpu))
		return -ENODEV;

	dbg_print("\n");
	/* cpuinfo and default policy values */
	policy->governor = CPUFREQ_DEFAULT_GOVERNOR;
	policy->cur = clk_get_rate(sh4_clk) / 1000;
	policy->cpuinfo.transition_latency = 10;

	return cpufreq_frequency_table_cpuinfo(policy, cpu_freqs);
}

static int st_cpufreq_verify(struct cpufreq_policy *policy)
{
	int ret = cpufreq_frequency_table_verify(policy, cpu_freqs);
	dbg_print("ret %d\n", ret);
	return ret;
}

static int st_cpufreq_target(struct cpufreq_policy *policy,
			     unsigned int target_freq, unsigned int relation)
{
	unsigned int idx = 0;
	dbg_print("\n");
	if (cpufreq_frequency_table_target(policy,
					   &cpu_freqs[0], target_freq, relation,
					   &idx))
		return -EINVAL;

	st_cpufreq_setstate(policy->cpu, idx);
	return 0;
}

static unsigned int st_cpufreq_get(unsigned int cpu)
{
	dbg_print("\n");
	return clk_get_rate(sh4_clk) / 1000;
}

#ifdef CONFIG_PM
static unsigned long pm_old_freq;
static int st_cpufreq_suspend(struct cpufreq_policy *policy, pm_message_t pmsg)
{
	dbg_print("\n");
	pm_old_freq = st_cpufreq_get(0);	/* save current frequency               */
	st_cpufreq_update_clocks(0, 0);	/* switch to the highest frequency    */
	return 0;
}

static int st_cpufreq_resume(struct cpufreq_policy *policy)
{
	int i;
	dbg_print("\n");
	for (i = 0; cpu_freqs[i].frequency != CPUFREQ_TABLE_END; ++i)
		if (cpu_freqs[i].frequency == pm_old_freq)
			break;
	st_cpufreq_update_clocks(i, 0);	/* restore the previous frequency */
	return 0;
}
#else
#define st_cpufreq_suspend      NULL
#define st_cpufreq_resume       NULL
#endif

static struct cpufreq_driver st_cpufreq_driver = {
	.owner = THIS_MODULE,
	.name = "st40-cpufreq",
	.init = st_cpufreq_init,
	.verify = st_cpufreq_verify,
	.get = st_cpufreq_get,
	.target = st_cpufreq_target,
	.suspend = st_cpufreq_suspend,
	.resume = st_cpufreq_resume,
	.flags = CPUFREQ_PM_NO_WARN,
};

void *st_cpufreq_platform_init(struct cpufreq_frequency_table *cpu_freqs);

static int __init st_cpufreq_module_init(void)
{
	int idx;
	dbg_print("\n");

	st_cpufreq_update_clocks = st_cpufreq_platform_init(cpu_freqs);

	if (!st_cpufreq_update_clocks)
		return -1;

	sh4_clk = clk_get(NULL, "sh4_clk");

	if (!sh4_clk) {
		printk(KERN_ERR "ERROR: on clk_get(sh4_clk)\n");
		return -ENODEV;
	}

	for (idx = 0; idx < (ARRAY_SIZE(cpu_freqs)) - 1; ++idx) {
		cpu_freqs[idx].frequency =
		    (clk_get_rate(sh4_clk) / 1000) >> idx;
		dbg_print("Initialize idx %u @ %u\n", idx,
			  cpu_freqs[idx].frequency);
	}

	if (cpufreq_register_driver(&st_cpufreq_driver))
		return -EINVAL;

	printk(KERN_INFO "st40 cpu frequency registered\n");

	return 0;
}

static void __exit st_cpufreq_module_exit(void)
{
	dbg_print("\n");
	cpufreq_unregister_driver(&st_cpufreq_driver);
	st_cpufreq_update_clocks(0, 1);	/* switch to the highest frequency */
}

late_initcall(st_cpufreq_module_init);
module_exit(st_cpufreq_module_exit);

MODULE_DESCRIPTION("cpufreq driver for ST40 Micro");
MODULE_LICENSE("GPL");
