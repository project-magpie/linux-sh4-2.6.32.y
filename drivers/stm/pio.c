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
 *	20/05/2008
 *	Fix enable/disable calling - chip_irq structure needs
 *	  .enable and .disable defined (contrary to header file!)
 *	Add stpio_flagged_request_irq() - can now set initial
 *	  state to IRQ_DISABLED
 *	Deprecate stpio_request_irq()
 *	Add stpio_set_irq_type()
 *	  wrapper for set_irq_type()
 *	Fix stpio_irq_chip_handler():
 *		- stop infinite looping
 *		- keep pin disabled if user handler disables it
 *		- fix sense of edge-triggering
 *	Fix stpio_irq_chip_type() - make sure level_mask is
 *	  correctly set for level triggered interrupts
 *
 *		Carl Shaw <carl.shaw@st.com>
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
#ifdef CONFIG_GPIOLIB
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

/* Debug Macros */
/* #define DEBUG */
#undef DEBUG

#ifdef DEBUG
#define DPRINTK(fmt, args...) printk(KERN_DEBUG "%s: " fmt, __FUNCTION__ , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

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
	unsigned char type;
	unsigned char flags;
#define PIN_FAKE_EDGE		4
#define PIN_IGNORE_EDGE_FLAG	2
#define PIN_IGNORE_EDGE_VAL	1
#define PIN_IGNORE_RISING_EDGE	(PIN_IGNORE_EDGE_FLAG | 0)
#define PIN_IGNORE_FALLING_EDGE	(PIN_IGNORE_EDGE_FLAG | 1)
#define PIN_IGNORE_EDGE_MASK	(PIN_IGNORE_EDGE_FLAG | PIN_IGNORE_EDGE_VAL)
};

struct stpio_port {
	void __iomem *base;
	struct stpio_pin pins[STPIO_PINS_IN_PORT];
#ifdef CONFIG_GPIOLIB
	struct gpio_chip gpio_chip;
#endif
	unsigned int level_mask;
};


static struct stpio_port stpio_ports[STPIO_MAX_PORTS];
static DEFINE_SPINLOCK(stpio_lock);
static int stpio_irq_base;


struct stpio_pin *__stpio_request_pin(unsigned int portno,
		unsigned int pinno, const char *name, int direction,
		int __set_value, unsigned int value)
{
	struct stpio_pin *pin = NULL;

	if (portno >= STPIO_MAX_PORTS || pinno >= STPIO_PINS_IN_PORT)
		return NULL;

	spin_lock(&stpio_lock);

#ifdef CONFIG_GPIOLIB
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
#ifdef CONFIG_GPIOLIB
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

static void stpio_irq_chip_handler(unsigned int irq, struct irq_desc *desc)
{
	const struct stpio_port *port = get_irq_data(irq);
	unsigned int portno = port - stpio_ports;
	unsigned long in, mask, comp, active;
	unsigned int pinno;
	unsigned int level_mask = port->level_mask;

	DPRINTK("called\n");

	/*
	 * We don't want to mask the INTC2/ILC first level interrupt here,
	 * and as these are both level based, there is no need to ack.
	 */

	in   = readl(port->base + STPIO_PIN_OFFSET);
	mask = readl(port->base + STPIO_PMASK_OFFSET);
	comp = readl(port->base + STPIO_PCOMP_OFFSET);

	active = (in ^ comp) & mask;

	DPRINTK("levelmask = 0x%08x\n", level_mask);

	/* Level sensitive interrupts we can mask for the duration */
	writel(level_mask,
	       port->base + STPIO_PMASK_OFFSET + STPIO_CLEAR_OFFSET);

	/* Edge sensitive we want to know about if they change */
	writel(~level_mask & active & comp,
	       port->base + STPIO_PCOMP_OFFSET + STPIO_CLEAR_OFFSET);
	writel(~level_mask & active & ~comp,
	       port->base + STPIO_PCOMP_OFFSET + STPIO_SET_OFFSET);

	while ((pinno = ffs(active)) != 0) {
		struct irq_desc *desc;
		struct stpio_pin *pin;
		unsigned long pinmask;

		DPRINTK("active = %d  pinno = %d\n", active, pinno);

		pinno--;
		irq = stpio_irq_base + (portno*STPIO_PINS_IN_PORT) + pinno;
		desc = irq_desc + irq;
		pin = get_irq_chip_data(irq);
		pinmask = 1 << pinno;

		active &= ~pinmask;

		if (pin->flags & PIN_FAKE_EDGE) {
			int val = stpio_get_pin(pin);
			DPRINTK("pinno %d PIN_FAKE_EDGE val %d\n", pinno, val);
			writel(pinmask, port->base + STPIO_PCOMP_OFFSET +
			       (val ? STPIO_SET_OFFSET : STPIO_CLEAR_OFFSET));
			if ((pin->flags & PIN_IGNORE_EDGE_MASK) ==
			    (PIN_IGNORE_EDGE_FLAG | (val^1)))
				continue;
		}

		if (unlikely(desc->status & (IRQ_INPROGRESS | IRQ_DISABLED))) {
			writel(pinmask, port->base +
			       STPIO_PMASK_OFFSET + STPIO_CLEAR_OFFSET);
			/* The unmasking will be done by enable_irq in
			 * case it is disabled or after returning from
			 * the handler if it's already running.
			 */
			if (desc->status & IRQ_INPROGRESS) {
				/* Level triggered interrupts won't
				 * ever be reentered
				 */
				BUG_ON(level_mask & pinmask);
				desc->status |= IRQ_PENDING;
			}
			continue;
		} else {
			desc->handle_irq(irq, desc);

			/* If our handler has disabled interrupts, then don't */
			/* re-enable them                                     */
			if (desc->status & IRQ_DISABLED){
				DPRINTK("handler has disabled interrupts!\n");
				mask &= ~pinmask;
			}
		}

		if (unlikely((desc->status & (IRQ_PENDING | IRQ_DISABLED)) ==
			     IRQ_PENDING)) {
			desc->status &= ~IRQ_PENDING;
			writel(pinmask, port->base +
			       STPIO_PMASK_OFFSET + STPIO_SET_OFFSET);
		}

	}

	/* Re-enable level */
	writel(level_mask & mask,
	       port->base + STPIO_PMASK_OFFSET + STPIO_SET_OFFSET);

	/* Do we need a software level as well, to cope with interrupts
	 * which get disabled during the handler execution? */

	DPRINTK("exiting\n");
}

/*
 * Currently gpio_to_irq and irq_to_gpio don't go through the gpiolib
 * layer. Hopefully this will change one day...
 */
int gpio_to_irq(unsigned gpio)
{
	return gpio + stpio_irq_base;
}
EXPORT_SYMBOL(gpio_to_irq);

int irq_to_gpio(unsigned irq)
{
	return irq - stpio_irq_base;
}
EXPORT_SYMBOL(irq_to_gpio);

static inline int pin_to_irq(struct stpio_pin *pin)
{
	return gpio_to_irq(stpio_to_gpio(pin->port - stpio_ports, pin->no));
}

static irqreturn_t stpio_irq_wrapper(int irq, void *dev_id)
{
	struct stpio_pin *pin = dev_id;
	DPRINTK("calling pin handler\n");
	pin->func(pin, pin->dev);
	return IRQ_HANDLED;
}


void stpio_flagged_request_irq(struct stpio_pin *pin, int comp,
		       void (*handler)(struct stpio_pin *pin, void *dev),
		       void *dev, unsigned long flags)
{
	int irq = pin_to_irq(pin);
	int ret;

	DPRINTK("called\n");

	/* stpio style interrupt handling doesn't allow sharing. */
	BUG_ON(pin->func);

	pin->func = handler;
	pin->dev = dev;

	set_irq_type(irq, comp ? IRQ_TYPE_LEVEL_LOW : IRQ_TYPE_LEVEL_HIGH);
	ret = request_irq(irq, stpio_irq_wrapper, 0, pin->name, pin);
	BUG_ON(ret);

	if (flags & IRQ_DISABLED)
		disable_irq(irq);
}
EXPORT_SYMBOL(stpio_flagged_request_irq);

void stpio_free_irq(struct stpio_pin *pin)
{
	int irq = pin_to_irq(pin);

	DPRINTK("calling free_irq\n");
	free_irq(irq, pin);

	pin->func = 0;
	pin->dev = 0;
}
EXPORT_SYMBOL(stpio_free_irq);

void stpio_enable_irq(struct stpio_pin *pin, int comp)
{
	int irq = pin_to_irq(pin);
	set_irq_type(irq, comp ? IRQ_TYPE_LEVEL_LOW : IRQ_TYPE_LEVEL_HIGH);
	DPRINTK("calling enable_irq for pin %s\n", pin->name);
	enable_irq(irq);
}
EXPORT_SYMBOL(stpio_enable_irq);

/* This function is safe to call in an IRQ UNLESS it is called in */
/* the PIO interrupt callback function                            */
void stpio_disable_irq(struct stpio_pin *pin)
{
	int irq = pin_to_irq(pin);
	DPRINTK("calling disable_irq for irq %d\n", irq);
	disable_irq(irq);
}
EXPORT_SYMBOL(stpio_disable_irq);

/* This is safe to call in IRQ context */
void stpio_disable_irq_nosync(struct stpio_pin *pin)
{
	int irq = pin_to_irq(pin);
	DPRINTK("calling disable_irq_nosync for irq %d\n", irq);
	disable_irq_nosync(irq);
}
EXPORT_SYMBOL(stpio_disable_irq_nosync);

void stpio_set_irq_type(struct stpio_pin* pin, int triggertype)
{
	int irq = pin_to_irq(pin);
	DPRINTK("setting pin %s to type %d\n", pin->name, triggertype);
	set_irq_type(irq, triggertype);
}
EXPORT_SYMBOL(stpio_set_irq_type);

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



#ifdef CONFIG_GPIOLIB

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

#endif /* CONFIG_GPIOLIB */

static void stpio_irq_chip_disable(unsigned int irq)
{
	struct stpio_pin *pin = get_irq_chip_data(irq);
	struct stpio_port *port = pin->port;
	int pinno = pin->no;

	DPRINTK("disabling pin %d\n", pinno);
	writel(1<<pinno, port->base + STPIO_PMASK_OFFSET + STPIO_CLEAR_OFFSET);
}

static void stpio_irq_chip_enable(unsigned int irq)
{
	struct stpio_pin *pin = get_irq_chip_data(irq);
	struct stpio_port *port = pin->port;
	int pinno = pin->no;

	DPRINTK("enabling pin %d\n", pinno);
	writel(1<<pinno, port->base + STPIO_PMASK_OFFSET + STPIO_SET_OFFSET);
}

static int stpio_irq_chip_type(unsigned int irq, unsigned type)
{
	struct stpio_pin *pin = get_irq_chip_data(irq);
	struct stpio_port *port = pin->port;
	int pinno = pin->no;
	int comp;

	DPRINTK("setting pin %d to type %d\n", pinno, type);
	pin->type = type;

	switch (type) {
	case IRQ_TYPE_EDGE_RISING:
		pin->flags = PIN_FAKE_EDGE | PIN_IGNORE_FALLING_EDGE;
		comp = 1;
		port->level_mask &= ~(1<<pinno);
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		pin->flags = 0;
		comp = 0;
		port->level_mask |= (1<<pinno);
		break;
	case IRQ_TYPE_EDGE_FALLING:
		pin->flags = PIN_FAKE_EDGE | PIN_IGNORE_RISING_EDGE;
		comp = 0;
		port->level_mask &= ~(1<<pinno);
		break;
	case IRQ_TYPE_LEVEL_LOW:
		pin->flags = 0;
		comp = 1;
		port->level_mask |= (1<<pinno);
		break;
	case IRQ_TYPE_EDGE_BOTH:
		pin->flags = PIN_FAKE_EDGE;
		comp = stpio_get_pin(pin);
		port->level_mask &= ~(1<<pinno);
		break;
	default:
		return -EINVAL;
	}

	writel(1<<pinno, port->base + STPIO_PCOMP_OFFSET +
	       (comp ? STPIO_SET_OFFSET : STPIO_CLEAR_OFFSET));

	return 0;
}

static struct irq_chip stpio_irq_chip = {
	.name		= "PIO-IRQ",
	.mask		= stpio_irq_chip_disable,
	.mask_ack	= stpio_irq_chip_disable,
	.unmask		= stpio_irq_chip_enable,
	.set_type	= stpio_irq_chip_type,
};

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

#ifdef CONFIG_GPIOLIB
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
		int irq;
		struct stpio_pin *pin;
		int i;

		irq = pdev->resource[1].start;
		if (irq == -1)
			goto no_irq;

		set_irq_chained_handler(irq, stpio_irq_chip_handler);
		set_irq_data(irq, port);

		irq = stpio_irq_base + (portno * STPIO_PINS_IN_PORT);
		for (i = 0; i < STPIO_PINS_IN_PORT; i++) {
			pin = &port->pins[i];
			set_irq_chip_and_handler_name(irq,
						      &stpio_irq_chip,
						      handle_simple_irq,
						      "STPIO");
			set_irq_chip_data(irq, pin);
			stpio_irq_chip_type(irq, IRQ_TYPE_LEVEL_HIGH);
			irq++;
			pin++;
		}
	}
no_irq:

	return 0;

#ifdef CONFIG_GPIOLIB
error_gpiochip_add:
	iounmap(port->base);
#endif
error_ioremap:
	if (!early)
		release_mem_region(memory->start, size);
error_request_mem_region:
error_get_resources:
	return result;
}



/* This is called early to allow board start up code to use PIO
 * (in particular console devices). */
void __init stpio_early_init(struct platform_device *pdev, int num, int irq)
{
	int i;

	for (i = 0; i < num; i++)
		stpio_init_port(pdev++, 1);

	stpio_irq_base = irq;
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

#ifdef CONFIG_GPIOLIB
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
subsys_initcall(stpio_init);

MODULE_AUTHOR("Stuart Menefy <stuart.menefy@st.com>");
MODULE_DESCRIPTION("STMicroelectronics PIO driver");
MODULE_LICENSE("GPL");
