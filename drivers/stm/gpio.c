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
#include "reg_pio.h"
#include "gpio_i.h"

#define DPRINTK(...)

struct stpio_pin {
#ifdef CONFIG_STPIO
	void (*func)(struct stpio_pin *pin, void *dev);
	void* dev;
	unsigned short port_no, pin_no;
#endif
};

struct stm_gpio_pin {
	unsigned char flags;
#define PIN_FAKE_EDGE		4
#define PIN_IGNORE_EDGE_FLAG	2
#define PIN_IGNORE_EDGE_VAL	1
#define PIN_IGNORE_RISING_EDGE	(PIN_IGNORE_EDGE_FLAG | 0)
#define PIN_IGNORE_FALLING_EDGE	(PIN_IGNORE_EDGE_FLAG | 1)
#define PIN_IGNORE_EDGE_MASK	(PIN_IGNORE_EDGE_FLAG | PIN_IGNORE_EDGE_VAL)
	struct stm_pad_config *pad_config;
	struct stm_pad_state *pad_state;
	struct stpio_pin stpio;
};

#define to_stm_gpio_port(chip) \
		container_of(chip, struct stm_gpio_port, gpio_chip)

struct stm_gpio_port {
	struct gpio_chip gpio_chip;
	void *base;
	unsigned long irq_level_mask;
	struct stm_gpio_pin pins[STM_GPIO_PINS_PER_PORT];
};




int stm_gpio_num; /* Number of available internal PIOs (pins) */
static unsigned int stm_gpio_irq_base; /* First irq number used by PIO "chip" */
static struct stm_gpio_port *stm_gpio_ports; /* PIO port descriptions */

/* PIO port base addresses copy, used by optimized gpio_get_value()
 * and gpio_set_value() in include/linux/stm/gpio.h */
void __iomem **stm_gpio_bases;



/*** PIO interrupt chained-handler implementation ***/

void stm_gpio_irq_handler(const struct stm_gpio_port *port)
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

	DPRINTK("level_mask = 0x%08lx\n", port_level_mask);

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

		DPRINTK("active = %ld  pinno = %d\n", port_active, pin_no);

		gpio = stm_gpio(port_no, pin_no);

		pin_irq = gpio_to_irq(gpio);
		pin_irq_desc = &irq_desc[pin_irq];
		pin = get_irq_chip_data(pin_irq);
		pin_mask = 1 << pin_no;

		port_active &= ~pin_mask;

		if (pin->flags & PIN_FAKE_EDGE) {
			int value = gpio_get_value(gpio);

			DPRINTK("pinno %d PIN_FAKE_EDGE val %d\n", pin_no, value);
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
				DPRINTK("handler has disabled interrupts!\n");
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

	DPRINTK("exiting\n");
}

static void stm_gpio_irq_chip_handler(unsigned int port_irq,
		struct irq_desc *port_irq_desc)
{
	struct stm_gpio_port *port = get_irq_data(port_irq);

	stm_gpio_irq_handler(port);
}

static void stm_gpio_irq_chip_disable(unsigned int pin_irq)
{
	unsigned gpio = irq_to_gpio(pin_irq);
	int port_no = stm_gpio_port(gpio);
	int pin_no = stm_gpio_pin(gpio);

	DPRINTK("disabling pin %d\n", pin_no);

	set__PIO_CLR_PMASK__CLR_PMASK__CLEAR(stm_gpio_bases[port_no], pin_no);
}

static void stm_gpio_irq_chip_enable(unsigned int pin_irq)
{
	unsigned gpio = irq_to_gpio(pin_irq);
	int port_no = stm_gpio_port(gpio);
	int pin_no = stm_gpio_pin(gpio);

	DPRINTK("enabling pin %d\n", pin_no);

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

	DPRINTK("setting pin %d to type %d\n", pin_no, type);

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

static struct irq_chip stm_gpio_irq_chip = {
	.name		= "stm_gpio_irq",
	.mask		= stm_gpio_irq_chip_disable,
	.mask_ack	= stm_gpio_irq_chip_disable,
	.unmask		= stm_gpio_irq_chip_enable,
	.set_type	= stm_gpio_irq_chip_type,
};

struct stm_gpio_port *stm_gpio_irq_init(int port_no, const char* name)
{
	struct stm_gpio_port *port = &stm_gpio_ports[port_no];
	struct stm_gpio_pin *pin;
	unsigned int pin_irq;
	int pin_no;

	/* Overwrite the "EARLY" label used before platform device init */
	port->gpio_chip.label = name;

	pin = &port->pins[0];
	pin_irq = stm_gpio_irq_base + (port_no * STM_GPIO_PINS_PER_PORT);
	for (pin_no = 0; pin_no < STM_GPIO_PINS_PER_PORT; pin_no++) {
		set_irq_chip_and_handler_name(pin_irq, &stm_gpio_irq_chip,
				handle_simple_irq, "stm_gpio");
		set_irq_chip_data(pin_irq, pin);
		stm_gpio_irq_chip_type(pin_irq, IRQ_TYPE_LEVEL_HIGH);
		pin++;
		pin_irq++;
	}

	return port;
}



/*** Internal utility functions ***/

static const char* stm_gpio_owner(int port_no, int pin_no)
{
	char pad_label[] = "PIOxx.x";

	snprintf(pad_label, sizeof(pad_label), "PIO%d.%d",
		 port_no, pin_no);
	return stm_pad_owner(pad_label);
}



/*** Low level hardware manipulation code for gpio/gpiolib and stpio ***/

static inline int __stm_gpio_get(struct stm_gpio_port *port, unsigned offset)
{
	return get__PIO_PIN__PIN(port->base, offset);
}

static inline void __stm_gpio_set(struct stm_gpio_port *port, unsigned offset,
		int value)
{
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

	set__PIO_PCx(port->base, offset, direction);
}

static inline int __stm_gpio_mux(struct stm_gpio_port *port,
		unsigned offset, int mux)
{
	struct stm_gpio_pin *gpio_pin;

	gpio_pin = &port->pins[offset];

	return stm_pad_mux(gpio_pin->pad_state, gpio_pin->pad_config, mux);
}



/*** generic gpio & gpiolib interface implementation ***/

/* Currently gpio_to_irq and irq_to_gpio don't go through the gpiolib
 * layer. Hopefully this will change one day... */
int gpio_to_irq(unsigned gpio)
{
	if (gpio >= stm_gpio_num)
		return -EINVAL;

	return gpio + stm_gpio_irq_base;
}
EXPORT_SYMBOL(gpio_to_irq);

int irq_to_gpio(unsigned int irq)
{
	if (irq < stm_gpio_irq_base || irq >= stm_gpio_irq_base + stm_gpio_num)
		return -EINVAL;

	return irq - stm_gpio_irq_base;
}
EXPORT_SYMBOL(irq_to_gpio);

static int stm_gpio_request(struct gpio_chip *chip, unsigned offset)
{
	struct stm_gpio_port *port = to_stm_gpio_port(chip);
	struct stm_pad_state *pad_state;

	pad_state = stm_pad_claim_exec(port->pins[offset].pad_config,
				       chip->label, 0);
	if (IS_ERR(pad_state))
		return PTR_ERR(pad_state);

	port->pins[offset].pad_state = pad_state;
	return 0;
}

static void stm_gpio_free(struct gpio_chip *chip, unsigned offset)
{
	struct stm_gpio_port *port = to_stm_gpio_port(chip);

	stm_pad_release(port->pins[offset].pad_state);
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

	set__PIO_PCx__INPUT_HIGH_IMPEDANCE(port->base, offset);
	__stm_gpio_mux(port, offset, -1);

	return 0;
}

static int stm_gpio_direction_output(struct gpio_chip *chip, unsigned offset,
		int value)
{
	struct stm_gpio_port *port = to_stm_gpio_port(chip);

	__stm_gpio_set(port, offset, value);

	set__PIO_PCx__OUTPUT_PUSH_PULL(port->base, offset);
	__stm_gpio_mux(port, offset, -1);

	return 0;
}

int stm_gpio_direction(unsigned int gpio, unsigned int direction)
{
	int port_no = stm_gpio_port(gpio);
	int pin_no = stm_gpio_pin(gpio);

	BUG_ON(gpio >= stm_gpio_num);

	__stm_gpio_direction(&stm_gpio_ports[port_no], pin_no, direction);

	return 0;
}

int stm_gpio_mux(unsigned int gpio, int mux)
{
	int port_no = stm_gpio_port(gpio);
	int pin_no = stm_gpio_pin(gpio);

	BUG_ON(gpio >= stm_gpio_num);

	return __stm_gpio_mux(&stm_gpio_ports[port_no], pin_no, mux);
}



#ifdef CONFIG_STPIO

/*** Legacy stpio_... interface */

#define stpio_pin_to_stm_gpio_pin(pin) \
		container_of(pin, struct stm_gpio_pin, stpio)

static inline int stpio_pin_to_irq(struct stpio_pin *pin)
{
	return gpio_to_irq(stm_gpio(pin->port_no, pin->pin_no));
}

struct stpio_pin *__stpio_request_pin(unsigned int portno,
		unsigned int pinno, const char *name, int direction,
		int __set_value, unsigned int value)
{
	struct stm_gpio_port *port;
	struct stm_gpio_pin *gpio_pin;
	struct stm_pad_state *pad_state;
	int num_ports = stm_gpio_num / STM_GPIO_PINS_PER_PORT;

	if (portno >= num_ports || pinno >= STM_GPIO_PINS_PER_PORT)
		return NULL;

	port = &stm_gpio_ports[portno];
	gpio_pin = &port->pins[pinno];

	pad_state = stm_pad_claim(gpio_pin->pad_config, name);
	if (IS_ERR(pad_state))
		return NULL;

	if (__set_value)
		__stm_gpio_set(port, pinno, value);

	__stm_gpio_direction(port, pinno, direction);

	gpio_pin->pad_state = pad_state;
	gpio_pin->stpio.port_no = portno;
	gpio_pin->stpio.pin_no = pinno;

	return &gpio_pin->stpio;
}
EXPORT_SYMBOL(__stpio_request_pin);

void stpio_free_pin(struct stpio_pin *pin)
{
	struct stm_gpio_pin *gpio_pin = stpio_pin_to_stm_gpio_pin(pin);

	stm_pad_release(gpio_pin->pad_state);
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
	int ret;

	/* stpio style interrupt handling doesn't allow sharing. */
	BUG_ON(pin->func);

	irq = stpio_pin_to_irq(pin);
	pin->func = handler;
	pin->dev = dev;

	set_irq_type(irq, comp ? IRQ_TYPE_LEVEL_LOW : IRQ_TYPE_LEVEL_HIGH);
	ret = request_irq(irq, stpio_irq_wrapper, 0,
			  stm_gpio_owner(pin->port_no, pin->pin_no), pin);
	BUG_ON(ret);

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

#endif /* CONFIG_STPIO */

#ifdef CONFIG_DEBUG_FS
static void stm_gpio_dbg_show(struct seq_file *s, struct gpio_chip *chip)
{
	struct stm_gpio_port *port = to_stm_gpio_port(chip);
	int port_no = port - stm_gpio_ports;
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
			owner = stm_gpio_owner(port_no, pin_no);
			if (owner) {
				seq_printf(s, "allocated by pad mgr "
						"to '%s'\n", owner);
			} else {
				seq_printf(s, "unused\n");
			}
		}
	}
}
#endif



/*** Early initialization ***/

static void stm_gpio_early_init_port(struct stm_gpio_port *port,
	int port_no, void* base, struct stm_pad_config *pad_config)
{
	int pin_no;

	port->base = base;
	port->gpio_chip.label = "EARLY";
	port->gpio_chip.request = stm_gpio_request;
	port->gpio_chip.free = stm_gpio_free;
	port->gpio_chip.get = stm_gpio_get;
	port->gpio_chip.set = stm_gpio_set;
	port->gpio_chip.direction_input = stm_gpio_direction_input;
	port->gpio_chip.direction_output = stm_gpio_direction_output;
#ifdef CONFIG_DEBUG_FS
	port->gpio_chip.dbg_show = stm_gpio_dbg_show;
#endif
	port->gpio_chip.base = port_no * STM_GPIO_PINS_PER_PORT;
	port->gpio_chip.ngpio = STM_GPIO_PINS_PER_PORT;

	stm_gpio_bases[port_no] = port->base;

	for (pin_no = 0; pin_no < STM_GPIO_PINS_PER_PORT; pin_no++)
		port->pins[pin_no].pad_config = &pad_config[pin_no];

	if (gpiochip_add(&port->gpio_chip) != 0)
		panic("stm_gpio: Failed to add gpiolib chip!\n");
}

/* This is called early to allow board start up code to use PIO
 * (in particular console devices). */
void __init stm_gpio_early_init(struct platform_device pdevs[], int num,
		int irq_base)
{
	int pdev_no;
	int num_ports;

	num_ports = 0;
	for (pdev_no = 0; pdev_no < num; pdev_no++) {
		struct platform_device *pdev = &pdevs[pdev_no];
		int last_port_no;

		if (strcmp(pdev->name, "stm-pio10") == 0) {
			struct stm_plat_pio10_data *data = pdev->dev.platform_data;
			last_port_no = data->start_pio + data->num_pio - 1;
		} else
			last_port_no = pdev->id;
		num_ports = max(num_ports, last_port_no+1);
	}

	stm_gpio_num = num_ports * STM_GPIO_PINS_PER_PORT;
	stm_gpio_irq_base = irq_base;

	stm_gpio_ports = alloc_bootmem(sizeof(*stm_gpio_ports) * num_ports);
	stm_gpio_bases = alloc_bootmem(sizeof(*stm_gpio_bases) * num_ports);
	if (!stm_gpio_ports || !stm_gpio_bases)
		panic("stm_gpio: Can't get bootmem!\n");

	for (pdev_no = 0; pdev_no < num; pdev_no++) {
		struct platform_device *pdev = &pdevs[pdev_no];
		struct resource *memory;
		void* base;

		memory = platform_get_resource(pdev, IORESOURCE_MEM, 0);
		if (!memory)
			panic("stm_gpio: Can't find memory resource!\n");

		base = ioremap(memory->start,
				memory->end - memory->start + 1);
		if (!base)
			panic("stm_gpio: Can't get IO memory mapping!\n");

		if (strcmp(pdev->name, "stm-pio10") == 0) {
			struct stm_plat_pio10_data *data = pdev->dev.platform_data;
			int port_no = data->start_pio;
			struct stm_gpio_port *port = &stm_gpio_ports[port_no];
			int i;

			for (i=0; i<data->num_pio; i++) {
				stm_gpio_early_init_port(port, port_no, base,
					data->port_data[i].pad_configs);
				port++;
				port_no++;
				base += 0x1000;
			}
		} else {
			struct stm_plat_pio_data *plat_data = pdev->dev.platform_data;
			int port_no = pdev->id;
			struct stm_gpio_port *port = &stm_gpio_ports[port_no];

			stm_gpio_early_init_port(port, port_no, base,
						 plat_data->pad_configs);
			port_no++;
		}
	}
}

/*** Platform device driver ***/

static int __devinit stm_gpio_probe(struct platform_device *pdev)
{
	struct resource *memory, *irq;

	BUG_ON(pdev->id >= stm_gpio_num);

	memory = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!memory)
		return -EINVAL;

	if (!request_mem_region(memory->start,
			memory->end - memory->start + 1, pdev->name))
		return -EBUSY;

	irq = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (irq) {
		struct stm_gpio_port *port;
		port = stm_gpio_irq_init(pdev->id, dev_name(&pdev->dev));
		if (IS_ERR(port)) {
			dev_err(&pdev->dev, "Failed to init gpio interrupt\n");
			return PTR_ERR(port);
		}

		set_irq_chained_handler(irq->start, stm_gpio_irq_chip_handler);
		set_irq_data(irq->start, &stm_gpio_ports[pdev->id]);
	}

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

static int __devexit stm_gpio_remove(struct platform_device *pdev)
{
	struct resource *resource;

	BUG_ON(pdev->id >= stm_gpio_num);

	resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	BUG_ON(!resource);
	release_mem_region(resource->start,
			resource->end - resource->start + 1);

	return 0;
}

static struct platform_driver stm_gpio_driver = {
	.driver	= {
		.name	= "stm-gpio",
		.owner	= THIS_MODULE,
	},
	.probe		= stm_gpio_probe,
	.remove		= __devexit_p(stm_gpio_remove),
};

static int __init stm_gpio_init(void)
{
	return platform_driver_register(&stm_gpio_driver);
}
subsys_initcall(stm_gpio_init);

MODULE_AUTHOR("Pawel Moll <pawel.moll@st.com>");
MODULE_LICENSE("GPL");
