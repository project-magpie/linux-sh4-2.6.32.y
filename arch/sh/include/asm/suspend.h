/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright 2009 (c) STMicroelectronics
 *
 */
#ifndef __ASM_SH_SUSPEND_H
#define __ASM_SH_SUSPEND_H

extern const unsigned long __nosave_begin, __nosave_end;
static inline int arch_prepare_suspend(void)
{
	return 0;
}

extern void (*arch_swsusp_processor_state)(int suspend);
#endif /* __ASM_SH_SUSPEND_H */
