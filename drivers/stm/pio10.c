/*
 * drivers/stm/pio10.c
 *
 * (c) 2009 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * Support for the "Standalone 10 banks PIO" block. See ADCS 8073738
 * for details.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/gpio.h>
#include <linux/stm/platform.h>
#include "gpio_i.h"

#define STM_PIO10_STATUS_BANK	0xf000
#define STM_PIO10_STATUS_OFFSET	0x80

struct stm_pio10_port {
	void __iomem *base;
	struct stm_gpio_port *ports[0];
};

static void stm_pio10_irq_chip_handler(unsigned int irq, struct irq_desc *desc)
{
	const struct stm_pio10_port *port10 = get_irq_data(irq);
	unsigned long status;
	int portno;

	status = readl(port10->base + STM_PIO10_STATUS_OFFSET);
	while ((portno = ffs(status)) != 0) {
		portno--;
		stm_gpio_irq_handler(port10->ports[portno]);
		status &= ~(1<<portno);
	}
}

static int __devinit stm_pio10_probe(struct platform_device *pdev)
{
	struct stm_plat_pio10_data *data = pdev->dev.platform_data;
	int port10_size;
	struct stm_pio10_port *port10;
	struct device *dev = &pdev->dev;
	struct resource *memory, *irq;
	int j;

	port10_size = sizeof(*port10);
	port10_size += data->num_pio*sizeof(struct stpio_port *);
	port10 = devm_kzalloc(dev, port10_size, GFP_KERNEL);
	if (!port10)
		return -ENOMEM;

	memory = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!memory || !irq)
		return -EINVAL;

	if (!devm_request_mem_region(dev, memory->start,
			memory->end - memory->start + 1, pdev->name))
		return -EBUSY;

	port10->base = devm_ioremap_nocache(dev,
		memory->start + STM_PIO10_STATUS_BANK, 0x100);
	if (!port10->base)
		return -ENOMEM;

	for (j = 0; j < data->num_pio; j++) {
		struct stm_gpio_port *port;
		port = stm_gpio_irq_init(data->start_pio+j,
					 dev_name(&pdev->dev));
		if (IS_ERR(port))
		    return PTR_ERR(port);
		port10->ports[j] = port;
	}

	set_irq_chained_handler(irq->start, stm_pio10_irq_chip_handler);
	set_irq_data(irq->start, port10);

	platform_set_drvdata(pdev, port10);

	return 0;
}

static int __devexit stm_pio10_remove(struct platform_device *pdev)
{
	struct stm_pio10_port *port10 = platform_get_drvdata(pdev);
	struct resource *resource;

	resource = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	BUG_ON(!resource);
	free_irq(resource->start, port10);

	return 0;
}

static struct platform_driver stm_pio10_driver = {
	.driver	= {
		.name	= "stm-pio10",
		.owner	= THIS_MODULE,
	},
	.probe		= stm_pio10_probe,
	.remove		= __devexit_p(stm_pio10_remove),
};

static int __init stm_pio10_init(void)
{
	return platform_driver_register(&stm_pio10_driver);
}
subsys_initcall(stm_pio10_init);

MODULE_AUTHOR("Stuart Menefy <stuart.menefy@st.com>");
MODULE_DESCRIPTION("STMicroelectronics PIO10 driver");
MODULE_LICENSE("GPL");
