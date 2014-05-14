/*
 * drivers/stm/gpio.c
 *
 * (c) 2010 STMicroelectronics Limited
 *
 * Authors: Pawel Moll <pawel.moll@st.com>
 *          Stuart Menefy <stuart.menefy@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/bootmem.h>
#include <linux/module.h>
#include <linux/gpio.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/bitops.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/debugfs.h>
#include <linux/seq_file.h>
#include <linux/stm/platform.h>
#include <linux/stm/pad.h>
#include <linux/stm/pio.h>
#include <linux/stm/pm_sys.h>
#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#include <linux/kallsyms.h>
#endif
#include "reg_pio.h"



struct stpio_pin {
#ifdef CONFIG_STPIO
	const char *pin_name;
	void (*func)(struct stpio_pin *pin, void *dev);
	void* dev;
	unsigned short port_no, pin_no;
#endif
};

struct stm_gpio_pin {
	unsigned char flags;
#define PIN_FAKE_EDGE			4
#define PIN_IGNORE_EDGE_FLAG	2
#define PIN_IGNORE_EDGE_VAL	1
#define PIN_IGNORE_RISING_EDGE	(PIN_IGNORE_EDGE_FLAG | 0)
#define PIN_IGNORE_FALLING_EDGE	(PIN_IGNORE_EDGE_FLAG | 1)
#define PIN_IGNORE_EDGE_MASK	(PIN_IGNORE_EDGE_FLAG | PIN_IGNORE_EDGE_VAL)

//#ifdef CONFIG_HIBERNATION
	unsigned char pm_saved_data;
//#endif
	struct stpio_pin stpio;
};

#ifdef CONFIG_HIBERNATION
#define GPIO_DIR_MASK  0x7f
#define GPIO_VAL_MASK  0x80
static inline void gpio_pm_set_direction(struct stm_gpio_pin *pin,
	unsigned int dir)
{
	pin->pm_saved_data &= ~GPIO_DIR_MASK;
	pin->pm_saved_data |= dir & GPIO_DIR_MASK;
}

static inline unsigned char gpio_pm_get_direction(struct stm_gpio_pin *pin)
{
	return pin->pm_saved_data & GPIO_DIR_MASK;
}

static inline void gpio_pm_set_value(struct stm_gpio_pin *pin, int val)
{
	pin->pm_saved_data &= ~GPIO_VAL_MASK;
	pin->pm_saved_data |= (val ? GPIO_VAL_MASK : 0);
}

static inline int gpio_pm_get_value(struct stm_gpio_pin *pin)
{
	return (pin->pm_saved_data & GPIO_VAL_MASK) ? 1 : 0;
}
#else
static inline void gpio_pm_set_direction(struct stm_gpio_pin *pin,
	unsigned int dir)
{
	return;
}

static inline void gpio_pm_set_value(struct stm_gpio_pin *pin, int val)
{
	return;
}

#endif

#define to_stm_gpio_port(chip) \
		container_of(chip, struct stm_gpio_port, gpio_chip)

#define sysdev_to_stm_gpio(dev) \
		container_of((dev), struct stm_gpio_port, sysdev)

struct stm_gpio_port {
	struct gpio_chip gpio_chip;
	void *base;
	unsigned long irq_level_mask;
	struct stm_gpio_pin pins[STM_GPIO_PINS_PER_PORT];
};

struct stm_gpio_irqmux {
	void *base;
	int port_first;
};



int stm_gpio_num; /* Number of available internal PIOs (pins) */
EXPORT_SYMBOL(stm_gpio_num);

static unsigned int stm_gpio_irq_base; /* First irq number used by PIO "chip" */
static struct stm_gpio_port *stm_gpio_ports; /* PIO port descriptions */

/* PIO port base addresses copy, used by optimized gpio_get_value()
 * and gpio_set_value() in include/linux/stm/gpio.h */
void __iomem **stm_gpio_bases;
EXPORT_SYMBOL(stm_gpio_bases);



/*** PIO interrupt chained-handler implementation ***/

static void __stm_gpio_irq_handler(const struct stm_gpio_port *port)
{
	int port_no = port - stm_gpio_ports;
	int pin_no;
	unsigned long port_in, port_mask, port_comp, port_active;
	unsigned long port_level_mask = port->irq_level_mask;

	/* We don't want to mask the INTC2/ILC first level interrupt here,
	 * and as these are both level based, there is no need to ack. */

	port_in = get__PIO_PIN(port->base);
	port_comp = get__PIO_PCOMP(port->base);
	port_mask = get__PIO_PMASK(port->base);

	port_active = (port_in ^ port_comp) & port_mask;

	pr_debug("level_mask = 0x%08lx\n", port_level_mask);

	/* Level sensitive interrupts we can mask for the duration */
	set__PIO_CLR_PMASK(port->base, port_level_mask);

	/* Edge sensitive we want to know about if they change */
	set__PIO_CLR_PCOMP(port->base,
			~port_level_mask & port_active & port_comp);
	set__PIO_SET_PCOMP(port->base,
			~port_level_mask & port_active & ~port_comp);

	while ((pin_no = ffs(port_active)) != 0) {
		unsigned gpio;
		struct stm_gpio_pin *pin;
		unsigned int pin_irq;
		struct irq_desc *pin_irq_desc;
		unsigned long pin_mask;

		pin_no--;

		pr_debug("active = %ld  pinno = %d\n", port_active, pin_no);

		gpio = stm_gpio(port_no, pin_no);

		pin_irq = gpio_to_irq(gpio);
		pin_irq_desc = &irq_desc[pin_irq];
		pin = get_irq_chip_data(pin_irq);
		pin_mask = 1 << pin_no;

		port_active &= ~pin_mask;

		if (pin->flags & PIN_FAKE_EDGE) {
			int value = gpio_get_value(gpio);

			pr_debug("pinno %d PIN_FAKE_EDGE val %d\n",
					pin_no, value);
			if (value)
				set__PIO_SET_PCOMP(port->base, pin_mask);
			else
				set__PIO_CLR_PCOMP(port->base, pin_mask);

			if ((pin->flags & PIN_IGNORE_EDGE_MASK) ==
					(PIN_IGNORE_EDGE_FLAG | (value ^ 1)))
				continue;
		}

		if (unlikely(pin_irq_desc->status &
				(IRQ_INPROGRESS | IRQ_DISABLED))) {
			set__PIO_CLR_PMASK(port->base, pin_mask);
			/* The unmasking will be done by enable_irq in
			 * case it is disabled or after returning from
			 * the handler if it's already running.
			 */
			if (pin_irq_desc->status & IRQ_INPROGRESS) {
				/* Level triggered interrupts won't
				 * ever be reentered
				 */
				BUG_ON(port_level_mask & pin_mask);
				pin_irq_desc->status |= IRQ_PENDING;
			}
			continue;
		} else {
			pin_irq_desc->handle_irq(pin_irq, pin_irq_desc);

			/* If our handler has disabled interrupts,
			 * then don't re-enable them */
			if (pin_irq_desc->status & IRQ_DISABLED) {
				pr_debug("handler has disabled interrupts!\n");
				port_mask &= ~pin_mask;
			}
		}

		if (unlikely((pin_irq_desc->status &
				(IRQ_PENDING | IRQ_DISABLED)) == IRQ_PENDING)) {
			pin_irq_desc->status &= ~IRQ_PENDING;
			set__PIO_SET_PMASK(port->base, pin_mask);
		}

	}

	/* Re-enable level */
	set__PIO_SET_PMASK(port->base, port_level_mask & port_mask);

	/* Do we need a software level as well, to cope with interrupts
	 * which get disabled during the handler execution? */

	pr_debug("exiting\n");
}

static void stm_gpio_irq_handler(unsigned int irq, struct irq_desc *desc)
{
	struct stm_gpio_port *port = get_irq_data(irq);

	__stm_gpio_irq_handler(port);
}

static void stm_gpio_irqmux_handler(unsigned int irq, struct irq_desc *desc)
{
	struct stm_gpio_irqmux *irqmux = get_irq_data(irq);
	unsigned long status;
	int bit;

	status = readl(irqmux->base);
	while ((bit = ffs(status)) != 0) {
		struct stm_gpio_port *port;

		bit--;
		port = &stm_gpio_ports[irqmux->port_first + bit];
		__stm_gpio_irq_handler(port);
		status &= ~(1 << bit);
	}
}

static void stm_gpio_irq_chip_disable(unsigned int pin_irq)
{
	unsigned gpio = irq_to_gpio(pin_irq);
	int port_no = stm_gpio_port(gpio);
	int pin_no = stm_gpio_pin(gpio);

	pr_debug("disabling pin %d\n", pin_no);

	set__PIO_CLR_PMASK__CLR_PMASK__CLEAR(stm_gpio_bases[port_no], pin_no);
}

static void stm_gpio_irq_chip_enable(unsigned int pin_irq)
{
	unsigned gpio = irq_to_gpio(pin_irq);
	int port_no = stm_gpio_port(gpio);
	int pin_no = stm_gpio_pin(gpio);

	pr_debug("enabling pin %d\n", pin_no);

	set__PIO_SET_PMASK__SET_PMASK__SET(stm_gpio_bases[port_no], pin_no);
}

static int stm_gpio_irq_chip_type(unsigned int pin_irq, unsigned type)
{
	unsigned gpio = irq_to_gpio(pin_irq);
	int port_no = stm_gpio_port(gpio);
	int pin_no = stm_gpio_pin(gpio);
	struct stm_gpio_port *port = &stm_gpio_ports[port_no];
	struct stm_gpio_pin *pin = &port->pins[pin_no];
	int comp;

	pr_debug("setting pin %d to type %d\n", pin_no, type);

	switch (type) {
	case IRQ_TYPE_EDGE_RISING:
		pin->flags = PIN_FAKE_EDGE | PIN_IGNORE_FALLING_EDGE;
		comp = 1;
		port->irq_level_mask &= ~(1 << pin_no);
		break;
	case IRQ_TYPE_LEVEL_HIGH:
		pin->flags = 0;
		comp = 0;
		port->irq_level_mask |= (1 << pin_no);
		break;
	case IRQ_TYPE_EDGE_FALLING:
		pin->flags = PIN_FAKE_EDGE | PIN_IGNORE_RISING_EDGE;
		comp = 0;
		port->irq_level_mask &= ~(1 << pin_no);
		break;
	case IRQ_TYPE_LEVEL_LOW:
		pin->flags = 0;
		comp = 1;
		port->irq_level_mask |= (1 << pin_no);
		break;
	case IRQ_TYPE_EDGE_BOTH:
		pin->flags = PIN_FAKE_EDGE;
		comp = gpio_get_value(gpio);
		port->irq_level_mask &= ~(1 << pin_no);
		break;
	default:
		return -EINVAL;
	}

	set__PIO_PCOMP__PCOMP(port->base, pin_no, comp);

	return 0;
}

static int stm_gpio_irq_chip_wake(unsigned int irq, unsigned int on)
{
	return 0;
}

static struct irq_chip stm_gpio_irq_chip = {
	.name		= "stm_gpio_irq",
	.disable	= stm_gpio_irq_chip_disable,
	.mask		= stm_gpio_irq_chip_disable,
	.mask_ack	= stm_gpio_irq_chip_disable,
	.unmask		= stm_gpio_irq_chip_enable,
	.set_type	= stm_gpio_irq_chip_type,
	.set_wake	= stm_gpio_irq_chip_wake,
};

static int stm_gpio_irq_init(int port_no)
{
	struct stm_gpio_pin *pin;
	unsigned int pin_irq;
	int pin_no;

	pin = stm_gpio_ports[port_no].pins;
	pin_irq = stm_gpio_irq_base + (port_no * STM_GPIO_PINS_PER_PORT);
	for (pin_no = 0; pin_no < STM_GPIO_PINS_PER_PORT; pin_no++) {
		set_irq_chip_and_handler_name(pin_irq, &stm_gpio_irq_chip,
				handle_simple_irq, "stm_gpio");
		set_irq_chip_data(pin_irq, pin);
		stm_gpio_irq_chip_type(pin_irq, IRQ_TYPE_LEVEL_HIGH);
		pin++;
		pin_irq++;
	}

	return 0;
}



/*** Low level hardware manipulation code for gpio/gpiolib and stpio ***/

static inline int __stm_gpio_get(struct stm_gpio_port *port, unsigned offset)
{
	return get__PIO_PIN__PIN(port->base, offset);
}

static inline void __stm_gpio_set(struct stm_gpio_port *port, unsigned offset,
		int value)
{
	gpio_pm_set_value(&port->pins[offset], value);
	if (value)
		set__PIO_SET_POUT__SET_POUT__SET(port->base, offset);
	else
		set__PIO_CLR_POUT__CLR_POUT__CLEAR(port->base, offset);
}

static inline void __stm_gpio_direction(struct stm_gpio_port *port,
		unsigned offset, unsigned int direction)
{
	WARN_ON(direction != STM_GPIO_DIRECTION_BIDIR &&
			direction != STM_GPIO_DIRECTION_OUT &&
			direction != STM_GPIO_DIRECTION_IN &&
			direction != STM_GPIO_DIRECTION_ALT_OUT &&
			direction != STM_GPIO_DIRECTION_ALT_BIDIR);

	gpio_pm_set_direction(&port->pins[offset], direction);
	set__PIO_PCx(port->base, offset, direction);

	if (!port->pins[offset].stpio.pin_name)
		port->pins[offset].stpio.pin_name = "-----";
}



/*** Generic gpio & gpiolib interface implementation ***/

static int stm_gpio_request(struct gpio_chip *chip, unsigned offset)
{
	return stm_pad_claim_gpio(chip->base + offset);
}

static void stm_gpio_free(struct gpio_chip *chip, unsigned offset)
{
	stm_pad_release_gpio(chip->base + offset);
}

static int stm_gpio_get(struct gpio_chip *chip, unsigned offset)
{
	struct stm_gpio_port *port = to_stm_gpio_port(chip);

	return __stm_gpio_get(port, offset);
}

static void stm_gpio_set(struct gpio_chip *chip, unsigned offset, int value)
{
	struct stm_gpio_port *port = to_stm_gpio_port(chip);

	__stm_gpio_set(port, offset, value);
}

static int stm_gpio_direction_input(struct gpio_chip *chip, unsigned offset)
{
	struct stm_gpio_port *port = to_stm_gpio_port(chip);

	stm_pad_configure_gpio(chip->base + offset, STM_GPIO_DIRECTION_IN);

	if (!port->pins[offset].stpio.pin_name)
		port->pins[offset].stpio.pin_name = "-----";

	return 0;
}

static int stm_gpio_direction_output(struct gpio_chip *chip, unsigned offset,
		int value)
{
	struct stm_gpio_port *port = to_stm_gpio_port(chip);

	__stm_gpio_set(port, offset, value);

	stm_pad_configure_gpio(chip->base + offset, STM_GPIO_DIRECTION_OUT);

	if (!port->pins[offset].stpio.pin_name)
		port->pins[offset].stpio.pin_name = "-----";

	return 0;
}

static int stm_gpio_to_irq(struct gpio_chip *chip, unsigned offset)
{
	return stm_gpio_irq_base + chip->base + offset;
}

/* gpiolib doesn't support irq_to_gpio() call... */
int irq_to_gpio(unsigned int irq)
{
	if (irq < stm_gpio_irq_base || irq >= stm_gpio_irq_base + stm_gpio_num)
		return -EINVAL;

	return irq - stm_gpio_irq_base;
}
EXPORT_SYMBOL(irq_to_gpio);

int stm_gpio_direction(unsigned int gpio, unsigned int direction)
{
	int port_no = stm_gpio_port(gpio);
	int pin_no = stm_gpio_pin(gpio);

	BUG_ON(gpio >= stm_gpio_num);

	__stm_gpio_direction(&stm_gpio_ports[port_no], pin_no, direction);

	return 0;
}



/*** Deprecated stpio_... interface */

#ifdef CONFIG_STPIO

static inline int stpio_pin_to_irq(struct stpio_pin *pin)
{
	return gpio_to_irq(stm_gpio(pin->port_no, pin->pin_no));
}

struct stpio_pin *__stpio_request_pin(unsigned int port_no,
		unsigned int pin_no, const char *name, int direction,
		int __set_value, unsigned int value)
{
	struct stm_gpio_port *port;
	struct stm_gpio_pin *gpio_pin;
	int num_ports = stm_gpio_num / STM_GPIO_PINS_PER_PORT;

	if (port_no >= num_ports || pin_no >= STM_GPIO_PINS_PER_PORT)
		return NULL;

	port = &stm_gpio_ports[port_no];
	gpio_pin = &port->pins[pin_no];

	if (stm_pad_claim_gpio(stm_gpio(port_no, pin_no)) != 0)
		return NULL;

	if (__set_value)
		__stm_gpio_set(port, pin_no, value);

	__stm_gpio_direction(port, pin_no, direction);

	gpio_pin->stpio.port_no = port_no;
	gpio_pin->stpio.pin_no = pin_no;
	gpio_pin->stpio.pin_name = (name==NULL)?"-----":name;
	return &gpio_pin->stpio;
}
EXPORT_SYMBOL(__stpio_request_pin);

void stpio_free_pin(struct stpio_pin *pin)
{
	stpio_configure_pin(pin, STPIO_IN);
	pin->pin_name = NULL;
	pin->func = 0;
	pin->dev = 0;
	stm_pad_release_gpio(stm_gpio(pin->port_no, pin->pin_no));
}
EXPORT_SYMBOL(stpio_free_pin);

void stpio_configure_pin(struct stpio_pin *pin, int direction)
{
	struct stm_gpio_port *port = &stm_gpio_ports[pin->port_no];
	int pin_no = pin->pin_no;

	__stm_gpio_direction(port, pin_no, direction);
}
EXPORT_SYMBOL(stpio_configure_pin);

void stpio_set_pin(struct stpio_pin *pin, unsigned int value)
{
	struct stm_gpio_port *port = &stm_gpio_ports[pin->port_no];
	int pin_no = pin->pin_no;

	__stm_gpio_set(port, pin_no, value);
}
EXPORT_SYMBOL(stpio_set_pin);

unsigned int stpio_get_pin(struct stpio_pin *pin)
{
	struct stm_gpio_port *port = &stm_gpio_ports[pin->port_no];
	int pin_no = pin->pin_no;

	return __stm_gpio_get(port, pin_no);
}
EXPORT_SYMBOL(stpio_get_pin);

static irqreturn_t stpio_irq_wrapper(int irq, void *dev_id)
{
	struct stpio_pin *pin = dev_id;

	pin->func(pin, pin->dev);
	return IRQ_HANDLED;
}

int stpio_flagged_request_irq(struct stpio_pin *pin, int comp,
		       void (*handler)(struct stpio_pin *pin, void *dev),
		       void *dev, unsigned long flags)
{
	int irq;
	const char *owner;
	int result;

	/* stpio style interrupt handling doesn't allow sharing. */
	BUG_ON(pin->func);

	irq = stpio_pin_to_irq(pin);
	pin->func = handler;
	pin->dev = dev;

	owner = stm_pad_get_gpio_owner(stm_gpio(pin->port_no, pin->pin_no));
	set_irq_type(irq, comp ? IRQ_TYPE_LEVEL_LOW : IRQ_TYPE_LEVEL_HIGH);
	result = request_irq(irq, stpio_irq_wrapper, 0, owner, pin);
	BUG_ON(result);

	if (flags & IRQ_DISABLED) {
		/* This is a race condition waiting to happen... */
		disable_irq(irq);
	}

	return 0;
}
EXPORT_SYMBOL(stpio_flagged_request_irq);

void stpio_free_irq(struct stpio_pin *pin)
{
	int irq = stpio_pin_to_irq(pin);

	free_irq(irq, pin);

	pin->func = 0;
	pin->dev = 0;
}
EXPORT_SYMBOL(stpio_free_irq);

void stpio_enable_irq(struct stpio_pin *pin, int comp)
{
	int irq = stpio_pin_to_irq(pin);

	set_irq_type(irq, comp ? IRQ_TYPE_LEVEL_LOW : IRQ_TYPE_LEVEL_HIGH);
	enable_irq(irq);
}
EXPORT_SYMBOL(stpio_enable_irq);

/* This function is safe to call in an IRQ UNLESS it is called in */
/* the PIO interrupt callback function                            */
void stpio_disable_irq(struct stpio_pin *pin)
{
	int irq = stpio_pin_to_irq(pin);

	disable_irq(irq);
}
EXPORT_SYMBOL(stpio_disable_irq);

/* This is safe to call in IRQ context */
void stpio_disable_irq_nosync(struct stpio_pin *pin)
{
	int irq = stpio_pin_to_irq(pin);

	disable_irq_nosync(irq);
}
EXPORT_SYMBOL(stpio_disable_irq_nosync);

void stpio_set_irq_type(struct stpio_pin* pin, int triggertype)
{
	int irq = stpio_pin_to_irq(pin);

	set_irq_type(irq, triggertype);
}
EXPORT_SYMBOL(stpio_set_irq_type);

#ifdef CONFIG_PROC_FS

static struct proc_dir_entry *proc_stpio;

static inline const char *stpio_get_direction_name(unsigned int direction)
{
	switch (direction) {
	case STPIO_NONPIO:		return "IN  (pull-up)      ";
	case STPIO_BIDIR:		return "BI  (open-drain)   ";
	case STPIO_OUT:			return "OUT (push-pull)    ";
	case STPIO_IN:			return "IN  (Hi-Z)         ";
	case STPIO_ALT_OUT:		return "Alt-OUT (push-pull)";
	case STPIO_ALT_BIDIR:	return "Alt-BI (open-drain)";
	default:				return "forbidden          ";
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
	int port_no, pin_no;
	off_t begin = 0;
	int num_ports = stm_gpio_num / STM_GPIO_PINS_PER_PORT;
	struct stm_gpio_port *port;
	struct stm_gpio_pin *pin;

	len = sprintf(page, "  port      name          direction\n");
	for (port_no = 0; port_no < num_ports; port_no++)
	{
		for (pin_no = 0; pin_no < STM_GPIO_PINS_PER_PORT; pin_no++) {

			port = &stm_gpio_ports[port_no];
			if(!port) continue;

			pin = &port->pins[pin_no];
			if(!pin) continue;

			char *name = pin->stpio.pin_name ? pin->stpio.pin_name : "";
			len += sprintf(page + len,
					"PIO %d.%d [%-10s] [%s] [%s]\n",
					port_no, pin_no, name,
					stpio_get_direction_name(pin->pm_saved_data),
					stpio_get_handler_name(pin->stpio.func));

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
	if (off >= len + begin)
		return 0;
	*start = page + (off - begin);
	return ((count < begin + len - off) ? count : begin + len - off);
}

#endif /* CONFIG_PROC_FS */

#endif /* CONFIG_STPIO */



#ifdef CONFIG_DEBUG_FS
static void stm_gpio_dbg_show(struct seq_file *s, struct gpio_chip *chip)
{
	struct stm_gpio_port *port = to_stm_gpio_port(chip);
	int port_no = chip->base / STM_GPIO_PINS_PER_PORT;
	int pin_no;

	for (pin_no = 0; pin_no < STM_GPIO_PINS_PER_PORT; pin_no++) {
		unsigned gpio = stm_gpio(port_no, pin_no);
		const char *owner, *direction;

		seq_printf(s, " %-3d: PIO%d.%d: ", gpio, port_no, pin_no);

		switch (get__PIO_PCx(port->base, pin_no)) {
		case value__PIO_PCx__INPUT_WEAK_PULL_UP():
			direction = "input (weak pull up)";
			break;
		case value__PIO_PCx__BIDIR_OPEN_DRAIN():
		case value__PIO_PCx__BIDIR_OPEN_DRAIN__alt():
			direction = "bidirectional (open drain)";
			break;
		case value__PIO_PCx__OUTPUT_PUSH_PULL():
			direction = "output (push-pull)";
			break;
		case value__PIO_PCx__INPUT_HIGH_IMPEDANCE():
		case value__PIO_PCx__INPUT_HIGH_IMPEDANCE__alt():
			direction = "input (high impedance)";
			break;
		case value__PIO_PCx__ALTERNATIVE_OUTPUT_PUSH_PULL():
			direction = "alternative function output "
					"(push-pull)";
			break;
		case value__PIO_PCx__ALTERNATIVE_BIDIR_OPEN_DRAIN():
			direction = "alternative function bidirectional "
					"(open drain)";
			break;
		default:
			/* Should never get here... */
			__WARN();
			direction = "unknown configuration";
			break;
		}

		seq_printf(s, "%s, ", direction);

		owner = gpiochip_is_requested(chip, pin_no);
		if (owner) {
			unsigned irq = gpio_to_irq(gpio);
			struct irq_desc	*desc = irq_desc + irq;

			seq_printf(s, "allocated by GPIO to '%s'", owner);

			/* This races with request_irq(), set_irq_type(),
			 * and set_irq_wake() ... but those are "rare".
			 *
			 * More significantly, trigger type flags aren't
			 * currently maintained by genirq. */
			if (desc->action) {
				char *trigger;

				switch (desc->status & IRQ_TYPE_SENSE_MASK) {
				case IRQ_TYPE_NONE:
					trigger = "default";
					break;
				case IRQ_TYPE_EDGE_FALLING:
					trigger = "edge-falling";
					break;
				case IRQ_TYPE_EDGE_RISING:
					trigger = "edge-rising";
					break;
				case IRQ_TYPE_EDGE_BOTH:
					trigger = "edge-both";
					break;
				case IRQ_TYPE_LEVEL_HIGH:
					trigger = "level-high";
					break;
				case IRQ_TYPE_LEVEL_LOW:
					trigger = "level-low";
					break;
				default:
					__WARN();
					trigger = "unknown";
					break;
				}

				seq_printf(s, " and IRQ %d (%s trigger%s)",
					irq, trigger,
					(desc->status & IRQ_WAKEUP)
						? " wakeup" : "");
			}

			seq_printf(s, "\n");
		} else {
			owner = stm_pad_get_gpio_owner(stm_gpio(port_no,
						pin_no));
			if (owner) {
				seq_printf(s, "allocated by pad manager "
						"to '%s'\n", owner);
			} else {
				seq_printf(s, "unused\n");
			}
		}
	}
}
#endif



/*** Early initialization ***/

/* This is called early to allow board start up code to use PIO
 * (in particular console devices). */
void __init stm_gpio_early_init(struct platform_device pdevs[], int num,
		int irq_base)
{
	int port_no;

	stm_gpio_num = num * STM_GPIO_PINS_PER_PORT;
	stm_gpio_irq_base = irq_base;

	stm_gpio_ports = alloc_bootmem(sizeof(*stm_gpio_ports) * num);
	stm_gpio_bases = alloc_bootmem(sizeof(*stm_gpio_bases) * num);
	if (!stm_gpio_ports || !stm_gpio_bases)
		panic("stm_gpio: Can't get bootmem!\n");

	for (port_no = 0; port_no < num; port_no++) {
		struct platform_device *pdev = &pdevs[port_no];
		struct resource *memory;
		struct stm_gpio_port *port = &stm_gpio_ports[port_no];

		/* Skip non existing ports */
		if (!pdev->name)
			continue;

		memory = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (!memory)
			panic("stm_gpio: Can't find memory resource!\n");

		port->base = ioremap(memory->start,
				memory->end - memory->start + 1);
		if (!port->base)
			panic("stm_gpio: Can't get IO memory mapping!\n");
		port->gpio_chip.request = stm_gpio_request;
		port->gpio_chip.free = stm_gpio_free;
		port->gpio_chip.get = stm_gpio_get;
		port->gpio_chip.set = stm_gpio_set;
		port->gpio_chip.direction_input = stm_gpio_direction_input;
		port->gpio_chip.direction_output = stm_gpio_direction_output;
		port->gpio_chip.to_irq = stm_gpio_to_irq;
#ifdef CONFIG_DEBUG_FS
		port->gpio_chip.dbg_show = stm_gpio_dbg_show;
#endif
		port->gpio_chip.base = port_no * STM_GPIO_PINS_PER_PORT;
		port->gpio_chip.ngpio = STM_GPIO_PINS_PER_PORT;

		stm_gpio_bases[port_no] = port->base;

		if (gpiochip_add(&port->gpio_chip) != 0)
			panic("stm_gpio: Failed to add gpiolib chip!\n");
	}
}



/*** PIO bank platform device driver ***/

static int __devinit stm_gpio_probe(struct platform_device *pdev)
{
	int port_no = pdev->id;
	struct stm_gpio_port *port = &stm_gpio_ports[port_no];
	struct resource *memory;
	int irq;

	BUG_ON(port_no < 0);
	BUG_ON(port_no >= stm_gpio_num);

	memory = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!memory)
		return -EINVAL;

	if (!request_mem_region(memory->start,
			memory->end - memory->start + 1, pdev->name))
		return -EBUSY;

	irq = platform_get_irq(pdev, 0);
	if (irq >= 0) {
		set_irq_chained_handler(irq, stm_gpio_irq_handler);
		set_irq_data(irq, &stm_gpio_ports[port_no]);

		if (stm_gpio_irq_init(port_no) != 0) {
			printk(KERN_ERR "stm_gpio: Failed to init gpio "
					"interrupt!\n");
			return -EINVAL;
		}
	}

	port->gpio_chip.label = dev_name(&pdev->dev);
	dev_set_drvdata(&pdev->dev, port);

	return 0;

	/* This is a good time to check consistency of linux/stm/gpio.h
	 * declarations with the proper source... */
	BUG_ON(STM_GPIO_REG_SET_POUT != offset__PIO_SET_POUT());
	BUG_ON(STM_GPIO_REG_CLR_POUT != offset__PIO_CLR_POUT());
	BUG_ON(STM_GPIO_REG_PIN != offset__PIO_PIN());
	BUG_ON(STM_GPIO_DIRECTION_BIDIR != value__PIO_PCx__BIDIR_OPEN_DRAIN());
	BUG_ON(STM_GPIO_DIRECTION_OUT != value__PIO_PCx__OUTPUT_PUSH_PULL());
	BUG_ON(STM_GPIO_DIRECTION_IN != value__PIO_PCx__INPUT_HIGH_IMPEDANCE());
	BUG_ON(STM_GPIO_DIRECTION_ALT_OUT !=
			value__PIO_PCx__ALTERNATIVE_OUTPUT_PUSH_PULL());
	BUG_ON(STM_GPIO_DIRECTION_ALT_BIDIR !=
			value__PIO_PCx__ALTERNATIVE_BIDIR_OPEN_DRAIN());

	return 0;
}

static struct platform_driver stm_gpio_driver = {
	.driver	= {
		.name = "stm-gpio",
		.owner = THIS_MODULE,
	},
	.probe = stm_gpio_probe,
};



/*** PIO IRQ status register platform device driver ***/

static int __devinit stm_gpio_irqmux_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct stm_plat_pio_irqmux_data *plat_data = dev->platform_data;
	struct stm_gpio_irqmux *irqmux;
	struct resource *memory;
	int irq;
	int port_no;

	BUG_ON(!plat_data);

	irqmux = devm_kzalloc(dev, sizeof(*irqmux), GFP_KERNEL);
	if (!irqmux)
		return -ENOMEM;

	memory = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	irq = platform_get_irq(pdev, 0);
	if (!memory || irq < 0)
		return -EINVAL;

	if (!devm_request_mem_region(dev, memory->start,
			memory->end - memory->start + 1, pdev->name))
		return -EBUSY;

	irqmux->base = devm_ioremap_nocache(dev, memory->start,
			memory->end - memory->start + 1);
	if (!irqmux->base)
		return -ENOMEM;

	irqmux->port_first = plat_data->port_first;

	set_irq_chained_handler(irq, stm_gpio_irqmux_handler);
	set_irq_data(irq, irqmux);

	for (port_no = irqmux->port_first;
			port_no < irqmux->port_first + plat_data->ports_num;
			port_no++) {
		BUG_ON(port_no >= stm_gpio_num);

		if (stm_gpio_irq_init(port_no) != 0) {
			printk(KERN_ERR "stm_gpio: Failed to init gpio "
					"interrupt for port %d!\n", port_no);
			return -EINVAL;
		}
	}

	return 0;
}

static struct platform_driver stm_gpio_irqmux_driver = {
	.driver	= {
		.name = "stm-gpio-irqmux",
		.owner = THIS_MODULE,
	},
	.probe = stm_gpio_irqmux_probe,
};



/*** Drivers initialization ***/

#ifdef CONFIG_PM
#ifdef CONFIG_HIBERNATION
/*
 * platform_allow_pm_gpio
 * Every platform implementation of this function has to check if
 * a specific gpio_pin can be managed or not in the PM core code
 */
int __weak platform_allow_pm_gpio(int gpio, int freezing)
{
	return 1;
}

static int stm_gpio_port_restore(struct stm_gpio_port *port)
{
	int pin_no;

	for (pin_no = 0; pin_no < port->gpio_chip.ngpio; ++pin_no) {
		struct stm_gpio_pin *pin = &port->pins[pin_no];

		if (!platform_allow_pm_gpio(
			stm_gpio(port - stm_gpio_ports, pin_no), 0))
			continue;
		/*
		 * Direction can not be zero! Zero means 'un-claimed'
		 */
		if (!gpio_pm_get_direction(pin)) {
			/*
			 * On some chip the reset value ins't DIRECTION_IN...
			 */
			__stm_gpio_direction(port, pin_no,
					STM_GPIO_DIRECTION_IN);
			/* reset again to say 'un-claimed' as it was */
			gpio_pm_set_direction(pin, 0);
			continue;
		}

		/*
		 * In case of Direction_Out set the Out value
		 */
		if (STM_GPIO_DIRECTION_OUT == gpio_pm_get_direction(pin))
			__stm_gpio_set(port, pin_no, gpio_pm_get_value(pin));

		__stm_gpio_direction(port, pin_no, gpio_pm_get_direction(pin));

	}
	return 0;
}

static int stm_gpio_restore(void)
{
	int port_no, ret = 0;
	int port_num = stm_gpio_num / STM_GPIO_PINS_PER_PORT;

	for (port_no = 0; port_no < port_num; ++port_no)
		ret |= stm_gpio_port_restore(&stm_gpio_ports[port_no]);

	return ret;
}
#else
#define stm_gpio_restore	NULL
#endif

static int stm_gpio_port_suspend(struct stm_gpio_port *port)
{
	int port_no = port - stm_gpio_ports;
	int pin_no;

	/* Enable the wakeup pin IRQ if required */
	for (pin_no = 0; pin_no < port->gpio_chip.ngpio; ++pin_no) {
		int irq = gpio_to_irq(stm_gpio(port_no, pin_no));
		struct irq_desc *desc = irq_to_desc(irq);

		if (IRQ_WAKEUP & desc->status)
			stm_gpio_irq_chip_enable(irq);
		else
			stm_gpio_irq_chip_disable(irq);
	}

	return 0;
}

static int stm_gpio_suspend(void)
{
	int port_no, ret = 0;
	int port_num = stm_gpio_num / STM_GPIO_PINS_PER_PORT;

	for (port_no = 0; port_no < port_num; ++port_no)
		ret |= stm_gpio_port_suspend(&stm_gpio_ports[port_no]);

	return ret;
}


static struct stm_system stm_gpio_system = {
	.name = "gpio",
	.priority = stm_gpio_pr,
	.suspend = stm_gpio_suspend,
	.freeze = stm_gpio_suspend,
	.restore = stm_gpio_restore,
};

static int stm_gpio_subsystem_init(void)
{
	return stm_register_system(&stm_gpio_system);
}

module_init(stm_gpio_subsystem_init);
#endif

static int __init stm_gpio_init(void)
{
	int ret;

#ifdef CONFIG_PROC_FS
	proc_stpio = create_proc_entry("stpio", 0, NULL);
	if (proc_stpio)
		proc_stpio->read_proc = stpio_read_proc;
#endif

	ret = platform_driver_register(&stm_gpio_driver);
	if (ret)
		return ret;

	ret = platform_driver_register(&stm_gpio_irqmux_driver);
	if (ret)
		return ret;

	return ret;
}
postcore_initcall(stm_gpio_init);
