/*
 * -------------------------------------------------------------------------
 * <linux_root>/arch/sh/kernel/cpu/sh4/swsusp-st_socs.c
 * -------------------------------------------------------------------------
 * Copyright (C) 2009  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * ------------------------------------------------------------------------- */
#include <linux/suspend.h>
#include <linux/pm.h>
#include <linux/stm/pm.h>
#include <asm/pm.h>

#undef  dbg_print

#ifdef CONFIG_PM_DEBUG
#define dbg_print(fmt, args...)		\
		printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
#else
#define dbg_print(fmt, args...)
#endif

static void st_socs_processor_state(int state)
{
	pm_message_t pm = {.event = state, };
	if (state == PM_EVENT_FREEZE) {
		ilc_pm_state(pm);
		emi_pm_state(pm);
		clk_pm_state(pm);
		sysconf_pm_state(pm);
	} else {
		sysconf_pm_state(pm);
		clk_pm_state(pm);
		emi_pm_state(pm);
		ilc_pm_state(pm);
	}
	return;
}

static int __init init_swsusp_st_socs(void)
{
	arch_swsusp_processor_state = st_socs_processor_state;
	printk(KERN_INFO "sh4 hibernation support registered\n");
	return 0;
}

late_initcall(init_swsusp_st_socs);
