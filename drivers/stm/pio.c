/*
 * drivers/stm/pio.c
 *
 * (c) 2001 Stuart Menefy <stuart.menefy@st.com>
 * based on hd64465_gpio.c:
 * by Greg Banks <gbanks@pocketpenguins.com>
 * (c) 2000 PocketPenguins Inc
 *
 * PIO pin support for ST40 devices.
 *
 * History:
 * 	9/3/2006
 * 	Added stpio_enable_irq and stpio_disable_irq
 * 		Angelo Castello <angelo.castello@st.com>
 * 	13/3/2006
 * 	Added stpio_request_set_pin and /proc support
 * 	  	 Marek Skuczynski <mareksk@easymail.pl>
 *	13/3/2006
 *	Integrated patches above and tidied up STPIO_PIN_DETAILS
 *	macro to stop code duplication
 *		Carl Shaw <carl.shaw@st.com>
 *	26/3/2008
 *	Code cleanup, gpiolib integration
 *		Pawel Moll <pawel.moll@st.com>
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/bitops.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#ifdef CONFIG_HAVE_GPIO_LIB
#include <linux/gpio.h>
#endif
#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#include <linux/kallsyms.h>
#endif

#include <linux/stm/pio.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq-ilc.h>



#define STPIO_POUT_OFFSET	0x00
#define STPIO_PIN_OFFSET	0x10
#define STPIO_PC0_OFFSET	0x20
#define STPIO_PC1_OFFSET	0x30
#define STPIO_PC2_OFFSET	0x40
#define STPIO_PCOMP_OFFSET	0x50
#define STPIO_PMASK_OFFSET	0x60

#define STPIO_SET_OFFSET	0x4
#define STPIO_CLEAR_OFFSET	0x8

#define STPIO_STATUS_FREE	0
#define STPIO_STATUS_REQ_STPIO	1
#define STPIO_STATUS_REQ_GPIO	2

struct stpio_port;

struct stpio_pin {
	struct stpio_port *port;
	unsigned int no;
#ifdef CONFIG_PROC_FS
	int direction;
#endif
	const char *name;
	void (*func)(struct stpio_pin *pin, void *dev);
	void *dev;
};

struct stpio_port {
	void __iomem *base;
	struct stpio_pin pins[STPIO_PINS_IN_PORT];
#ifdef CONFIG_HAVE_GPIO_LIB
	struct gpio_chip gpio_chip;
#endif
};



static struct stpio_port stpio_ports[STPIO_MAX_PORTS];
static DEFINE_SPINLOCK(stpio_lock);



struct stpio_pin *__stpio_request_pin(unsigned int portno,
		unsigned int pinno, const char *name, int direction,
		int __set_value, unsigned int value)
{
	struct stpio_pin *pin = NULL;

	if (portno >= STPIO_MAX_PORTS || pinno >= STPIO_PINS_IN_PORT)
		return NULL;

	spin_lock(&stpio_lock);

#ifdef CONFIG_HAVE_GPIO_LIB
	if (gpio_request(stpio_to_gpio(portno, pinno), name) == 0) {
#else
	if (stpio_ports[portno].pins[pinno].name == NULL) {
#endif
		pin = &stpio_ports[portno].pins[pinno];
		if (__set_value)
			stpio_set_pin(pin, value);
		stpio_configure_pin(pin, direction);
		pin->name = name;
	}

	spin_unlock(&stpio_lock);

	return pin;
}
EXPORT_SYMBOL(__stpio_request_pin);

void stpio_free_pin(struct stpio_pin *pin)
{
	stpio_configure_pin(pin, STPIO_IN);
	pin->name = NULL;
	pin->func = 0;
	pin->dev  = 0;
#ifdef CONFIG_HAVE_GPIO_LIB
	gpio_free(stpio_to_gpio(pin->port - stpio_ports, pin->no));
#endif
}
EXPORT_SYMBOL(stpio_free_pin);

void stpio_configure_pin(struct stpio_pin *pin, int direction)
{
	writel(1 << pin->no, pin->port->base + STPIO_PC0_OFFSET +
			((direction & (1 << 0)) ? STPIO_SET_OFFSET :
			STPIO_CLEAR_OFFSET));
	writel(1 << pin->no, pin->port->base + STPIO_PC1_OFFSET +
			((direction & (1 << 1)) ? STPIO_SET_OFFSET :
			STPIO_CLEAR_OFFSET));
	writel(1 << pin->no, pin->port->base + STPIO_PC2_OFFSET +
			((direction & (1 << 2)) ? STPIO_SET_OFFSET :
			 STPIO_CLEAR_OFFSET));
#ifdef CONFIG_PROC_FS
	pin->direction = direction;
#endif
}
EXPORT_SYMBOL(stpio_configure_pin);

void stpio_set_pin(struct stpio_pin *pin, unsigned int value)
{
	writel(1 << pin->no, pin->port->base + STPIO_POUT_OFFSET +
			(value ? STPIO_SET_OFFSET : STPIO_CLEAR_OFFSET));
}
EXPORT_SYMBOL(stpio_set_pin);

unsigned int stpio_get_pin(struct stpio_pin *pin)
{
	return (readl(pin->port->base + STPIO_PIN_OFFSET) & (1 << pin->no))
			!= 0;
}
EXPORT_SYMBOL(stpio_get_pin);

static irqreturn_t stpio_interrupt(int irq, void *dev)
{
	const struct stpio_port *port = dev;
	unsigned int portno = port - stpio_ports;
    	unsigned long in, mask, comp;
	unsigned int pinno;

	in   = readl(port->base + STPIO_PIN_OFFSET);
	mask = readl(port->base + STPIO_PMASK_OFFSET);
	comp = readl(port->base + STPIO_PCOMP_OFFSET);

	mask &= in ^ comp;

	while ((pinno = ffs(mask)) != 0) {
		struct stpio_pin *pin;

		pinno--;
		pin = &stpio_ports[portno].pins[pinno];
		if (pin->func != 0)
			pin->func(pin, pin->dev);
		else
			printk(KERN_NOTICE "unexpected PIO interrupt, "
					"PIO%d[%d]\n", portno, pinno);
		mask &= ~(1 << pinno);
	}

	return IRQ_HANDLED;
}

void stpio_request_irq(struct stpio_pin *pin, int comp,
		       void (*handler)(struct stpio_pin *pin, void *dev),
		       void *dev)
{
	unsigned long flags;

	spin_lock_irqsave(&stpio_lock, flags);

	pin->func = handler;
	pin->dev = dev;

	stpio_enable_irq(pin, comp);

	spin_unlock_irqrestore(&stpio_lock, flags);
}
EXPORT_SYMBOL(stpio_request_irq);

void stpio_free_irq(struct stpio_pin *pin)
{
	unsigned long flags;

	spin_lock_irqsave(&stpio_lock, flags);

	stpio_disable_irq(pin);
	pin->func = 0;
	pin->dev = 0;

	spin_unlock_irqrestore(&stpio_lock, flags);
}
EXPORT_SYMBOL(stpio_free_irq);

void stpio_enable_irq(struct stpio_pin *pin, int comp)
{
	writel(1 << pin->no, pin->port->base + STPIO_PCOMP_OFFSET +
			(comp ? STPIO_SET_OFFSET : STPIO_CLEAR_OFFSET));
	writel(1 << pin->no, pin->port->base + STPIO_PMASK_OFFSET +
			STPIO_SET_OFFSET);
}
EXPORT_SYMBOL(stpio_enable_irq);

void stpio_disable_irq(struct stpio_pin *pin)
{
	writel(1 << pin->no, pin->port->base + STPIO_PMASK_OFFSET +
			STPIO_CLEAR_OFFSET);
}
EXPORT_SYMBOL(stpio_disable_irq);



#ifdef CONFIG_PROC_FS

static struct proc_dir_entry *proc_stpio;

static inline const char *stpio_get_direction_name(unsigned int direction)
{
	switch (direction) {
	case STPIO_NONPIO:	return "IN  (pull-up)      ";
	case STPIO_BIDIR:	return "BI  (open-drain)   ";
	case STPIO_OUT:		return "OUT (push-pull)    ";
	case STPIO_IN:		return "IN  (Hi-Z)         ";
	case STPIO_ALT_OUT:	return "Alt-OUT (push-pull)";
	case STPIO_ALT_BIDIR:	return "Alt-BI (open-drain)";
	default:		return "forbidden          ";
	}
};

static inline const char *stpio_get_handler_name(void *func)
{
	static char sym_name[KSYM_NAME_LEN];
	char *modname;
	unsigned long symbolsize, offset;
	const char *symb;

	if (func == NULL)
		return "";

	symb = kallsyms_lookup((unsigned long)func, &symbolsize, &offset,
			&modname, sym_name);

	return symb ? symb : "???";
}

static int stpio_read_proc(char *page, char **start, off_t off, int count,
		int *eof, void *data_unused)
{
	int len;
	int portno;
	off_t begin = 0;

	spin_lock(&stpio_lock);

	len = sprintf(page, "  port      name          direction\n");
	for (portno = 0; portno < STPIO_MAX_PORTS; portno++)
	{
		int pinno;

		for (pinno = 0; pinno < STPIO_PINS_IN_PORT; pinno++) {
			struct stpio_pin *pin =
					&stpio_ports[portno].pins[pinno];

			len += sprintf(page + len,
					"PIO %d.%d [%-10s] [%s] [%s]\n",
					portno, pinno,
					(pin->name ? pin->name : ""),
					stpio_get_direction_name(
					pin->direction),
					stpio_get_handler_name(pin->func));
			if (len + begin > off + count)
				goto done;
			if (len + begin < off) {
				begin += len;
				len = 0;
			}
		}
	}

	*eof = 1;

done:
	spin_unlock(&stpio_lock);
	if (off >= len + begin)
		return 0;
	*start = page + (off - begin);
	return ((count < begin + len - off) ? count : begin + len - off);
}

#endif /* CONFIG_PROC_FS */



#ifdef CONFIG_HAVE_GPIO_LIB

#define to_stpio_port(chip) container_of(chip, struct stpio_port, gpio_chip)

static const char *stpio_gpio_name = "gpiolib";

static int stpio_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	struct stpio_port *port = to_stpio_port(chip);
	struct stpio_pin *pin = &port->pins[offset];

	/* Already claimed by some other stpio user? */
	if (pin->name != NULL && strcmp(pin->name, stpio_gpio_name) != 0)
		return -EINVAL;

	/* Now it is forever claimed by gpiolib ;-) */
	if (!pin->name)
		pin->name = stpio_gpio_name;

	stpio_configure_pin(pin, STPIO_IN);

	return 0;
}

static int stpio_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct stpio_port *port = to_stpio_port(chip);
	struct stpio_pin *pin = &port->pins[offset];

	return (int)stpio_get_pin(pin);
}

static int stpio_gpio_direction_output(struct gpio_chip *chip, unsigned offset,
		int value)
{
	struct stpio_port *port = to_stpio_port(chip);
	struct stpio_pin *pin = &port->pins[offset];

	/* Already claimed by some other stpio user? */
	if (pin->name != NULL && strcmp(pin->name, stpio_gpio_name) != 0)
		return -EINVAL;

	/* Now it is forever claimed by gpiolib ;-) */
	if (!pin->name)
		pin->name = stpio_gpio_name;

	stpio_configure_pin(pin, STPIO_OUT);

	return 0;
}

static void stpio_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct stpio_port *port = to_stpio_port(chip);
	struct stpio_pin *pin = &port->pins[offset];

	stpio_set_pin(pin, (unsigned int)value);
}

#endif /* CONFIG_HAVE_GPIO_LIB */



static int stpio_init_port(struct platform_device *pdev, int early)
{
	int result;
	int portno = pdev->id;
	struct stpio_port *port = &stpio_ports[portno];
	struct resource *memory, *irq;
	int size;

	BUG_ON(portno >= STPIO_MAX_PORTS);

	memory = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!memory || !irq) {
		result = -EINVAL;
		goto error_get_resources;
	}
	size = memory->end - memory->start + 1;

	if (!early) {
		if (!request_mem_region(memory->start, size, pdev->name)) {
			result = -EBUSY;
			goto error_request_mem_region;
		}
	}

	if (early || !port->base) {
		int pinno;

		port->base = ioremap(memory->start, size);
		if (!port->base) {
			result = -ENOMEM;
			goto error_ioremap;
		}

		for (pinno = 0; pinno < STPIO_PINS_IN_PORT; pinno++) {
			port->pins[pinno].no = pinno;
			port->pins[pinno].port = port;
		}

#ifdef CONFIG_HAVE_GPIO_LIB
		port->gpio_chip.label = pdev->dev.bus_id;
		port->gpio_chip.direction_input = stpio_gpio_direction_input;
		port->gpio_chip.get = stpio_gpio_get;
		port->gpio_chip.direction_output = stpio_gpio_direction_output;
		port->gpio_chip.set = stpio_gpio_set;
		port->gpio_chip.dbg_show = NULL;
		port->gpio_chip.base = portno * STPIO_PINS_IN_PORT;
		port->gpio_chip.ngpio = STPIO_PINS_IN_PORT;
		port->gpio_chip.can_sleep = 0;
		result = gpiochip_add(&port->gpio_chip);
		if (result != 0)
			goto error_gpiochip_add;
#endif
	}

	if (!early) {
		result = request_irq(irq->start, stpio_interrupt, 0,
				pdev->name, port);
		if (result < 0)
			goto error_request_irq;
	}

	return 0;

error_request_irq:
	release_mem_region(memory->start, size);
#ifdef CONFIG_HAVE_GPIO_LIB
	if (gpiochip_remove(&port->gpio_chip) != 0)
		printk(KERN_ERR "stpio aaargh!\n");
error_gpiochip_add:
#endif
	iounmap(port->base);
error_ioremap:
	if (!early)
		release_mem_region(memory->start, size);
error_request_mem_region:
error_get_resources:
	return result;
}



/* This is called early to allow board start up code to use PIO
 * (in particular console devices). */
void __init stpio_early_init(struct platform_device *pdev, int num)
{
	int i;

	for (i = 0; i < num; i++)
		stpio_init_port(pdev++, 1);
}

static int __devinit stpio_probe(struct platform_device *pdev)
{
	return stpio_init_port(pdev, 0);
}

static int __devexit stpio_remove(struct platform_device *pdev)
{
	struct stpio_port *port = &stpio_ports[pdev->id];
	struct resource *resource;

	BUG_ON(pdev->id >= STPIO_MAX_PORTS);

#ifdef CONFIG_HAVE_GPIO_LIB
	if (gpiochip_remove(&port->gpio_chip) != 0)
		return -EBUSY;
#endif

	resource = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	BUG_ON(!resource);
	free_irq(resource->start, port);

	iounmap(port->base);

	resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	BUG_ON(!resource);
	release_mem_region(resource->start,
			resource->end - resource->start + 1);

	return 0;
}

static struct platform_driver stpio_driver = {
	.driver	= {
		.name	= "stpio",
		.owner	= THIS_MODULE,
	},
	.probe		= stpio_probe,
	.remove		= __devexit_p(stpio_remove),
};

static int __init stpio_init(void)
{
#ifdef CONFIG_PROC_FS
	proc_stpio = create_proc_entry("stpio", 0, NULL);
	if (proc_stpio)
		proc_stpio->read_proc = stpio_read_proc;
#endif

	return platform_driver_register(&stpio_driver);
}

static void __exit stpio_exit(void)
{
#ifdef CONFIG_PROC_FS
	if (proc_stpio)
		remove_proc_entry("stpio", NULL);
#endif

	platform_driver_unregister(&stpio_driver);
}

module_init(stpio_init);
module_exit(stpio_exit);

MODULE_AUTHOR("Stuart Menefy <stuart.menefy@st.com>");
MODULE_DESCRIPTION("STMicroelectronics PIO driver");
MODULE_LICENSE("GPL");
