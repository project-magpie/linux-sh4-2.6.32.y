#define ILC_FIRST_IRQ	44
#define ILC_NR_IRQS	150
#define ILC_IRQ(x) (ILC_FIRST_IRQ + (x))

void __init init_IRQ_ilc(void);
void ilc_irq_demux(int irq, struct irq_desc *desc);
