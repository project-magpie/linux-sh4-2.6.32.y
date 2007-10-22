#define ILC_FIRST_IRQ	44
#define ILC_NR_IRQS	150
#define ILC_IRQ(x) (ILC_FIRST_IRQ + (x))

void __init ilc_early_init(struct platform_device* pdev);
void __init ilc_stx7200_init(void);
void ilc_irq_demux(unsigned int irq, struct irq_desc *desc);
