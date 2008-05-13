#ifndef __ASM_SH_IRQ_ILC_H
#define __ASM_SH_IRQ_ILC_H

#if defined(CONFIG_CPU_SUBTYPE_STX7105)
#define ILC_FIRST_IRQ	176
#define ILC_NR_IRQS	(64+16)
#define ILC_INT_IRQ(x)	(ILC_FIRST_IRQ + (x))
#define ILC_EXT_IRQ(x)	(ILC_FIRST_IRQ + 64 + (x))
#define ILC_IRQ(x)	ILC_INT_IRQ(x)
#elif	defined(CONFIG_CPU_SUBTYPE_STX7111)
/* In an attempt to stick within NR_IRQS (256), and not complicate the
 * mapping between ILC interrupt number and Linux IRQ number, we cap
 * the number of external interrupts at 16. This will probably
 * break one day, at which point we have the option of increasing
 * NR_IRQS or modifying the ILC code to support an offset (effectivly
 * ignoring the internal interrupts).
 */
#define ILC_FIRST_IRQ	176
#define ILC_NR_IRQS	(64+16)
#define ILC_INT_IRQ(x)	(ILC_FIRST_IRQ + (x))
#define ILC_EXT_IRQ(x)	(ILC_FIRST_IRQ + 64 + (x))
#define ILC_IRQ(x)	ILC_INT_IRQ(x)
#elif defined(CONFIG_CPU_SUBTYPE_STX7200)
#define ILC_FIRST_IRQ	44
#define ILC_NR_IRQS	150
#define ILC_IRQ(x) (ILC_FIRST_IRQ + (x))
#endif

void __init ilc_early_init(struct platform_device* pdev);
void __init ilc_demux_init(void);
void ilc_irq_demux(unsigned int irq, struct irq_desc *desc);

#endif
