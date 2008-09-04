/*
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#ifndef __ASM_SH_IRQ_ILC_H
#define __ASM_SH_IRQ_ILC_H

#include <linux/platform_device.h>

#if defined(CONFIG_CPU_SUBTYPE_STX7105)
#define ILC_FIRST_IRQ	176
#define ILC_NR_IRQS	(64+35)
#define ILC_INT_IRQ(x)	(ILC_FIRST_IRQ + (x))
#define ILC_EXT_IRQ(x)	(ILC_FIRST_IRQ + 64 + (x))
#define ILC_IRQ(x)	ILC_INT_IRQ(x)
#elif	defined(CONFIG_CPU_SUBTYPE_STX7111)
#define ILC_FIRST_IRQ	176
#define ILC_NR_IRQS	(64+33)
#define ILC_INT_IRQ(x)	(ILC_FIRST_IRQ + (x))
#define ILC_EXT_IRQ(x)	(ILC_FIRST_IRQ + 64 + (x))
#define ILC_IRQ(x)	ILC_INT_IRQ(x)
#elif	defined(CONFIG_CPU_SUBTYPE_STX7141)
/* set this to 65 to allow 64 (INTEVT 0xa00) to demux */
#define ILC_FIRST_IRQ	65
#define ILC_NR_IRQS	180
#define ILC_IRQ(x)	(ILC_FIRST_IRQ + (x))
#elif defined(CONFIG_CPU_SUBTYPE_STX7200)
#define ILC_FIRST_IRQ	44
#define ILC_NR_IRQS	150
#define ILC_IRQ(x) (ILC_FIRST_IRQ + (x))
#endif

void __init ilc_early_init(struct platform_device* pdev);
void __init ilc_demux_init(void);
void ilc_irq_demux(unsigned int irq, struct irq_desc *desc);

#endif
