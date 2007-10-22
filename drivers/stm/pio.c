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
 *
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/sched.h>
#include <linux/ioport.h>
#include <linux/bitops.h>
#include <linux/interrupt.h>
#include <linux/stm/pio.h>

#include <asm/system.h>
#include <asm/io.h>

#ifdef CONFIG_PROC_FS
#include <linux/proc_fs.h>
#include <linux/kallsyms.h>
#endif

#if defined(CONFIG_CPU_SUBTYPE_ST40STB1)
#define STPIO_NPORTS 3
#elif defined(CONFIG_CPU_SUBTYPE_STB7100)
#define STPIO_NPORTS 6
#elif defined(CONFIG_CPU_SUBTYPE_STX7200)
#define STPIO_NPORTS 8
#else
#error Unknown CPU
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

#define MODNAME "stpio"

struct stpio_pin {
	const char* name;
	void (*func)(struct stpio_pin *pin, void *dev);
	void *dev;
#ifdef CONFIG_PROC_FS
	int  direction;
#endif
};

struct stpio_port {
	unsigned long base;
	unsigned int irq;
};

static struct stpio_pin stpio_pin_conf[STPIO_NPORTS*8];

static const struct stpio_port stpio_port_conf[STPIO_NPORTS] = {
#if defined(CONFIG_CPU_SUBTYPE_ST40STB1)
	{0xbb010000, 80 },			/* PIO0 */
	{0xbb020000, 84 },			/* PIO1 */
	{0xbb030000, 88 },			/* PIO2 */
#elif defined(CONFIG_CPU_SUBTYPE_STB7100)
	{0xb8020000, 80 },
	{0xb8021000, 84 },
	{0xb8022000, 88 },
	{0xb8023000, 115 },
	{0xb8024000, 114 },
	{0xb8025000, 113 },
#elif defined(CONFIG_CPU_SUBTYPE_STX7200)
	{0xfd020000, 96+MUXED_IRQ_BASE },
	{0xfd021000, 97+MUXED_IRQ_BASE },
	{0xfd022000, 98+MUXED_IRQ_BASE },
	{0xfd023000, 99+MUXED_IRQ_BASE },
	{0xfd024000, 100+MUXED_IRQ_BASE },
	{0xfd025000, 101+MUXED_IRQ_BASE },
	{0xfd026000, 102+MUXED_IRQ_BASE },
	{0xfd027000, 103+MUXED_IRQ_BASE },
#else
#error Unknown CPU
#endif
};

static spinlock_t stpio_lock = SPIN_LOCK_UNLOCKED;

#define STPIO_PIN_DETAILS(pin, port, pinno)		\
	unsigned int pinno;				\
	const struct stpio_port *port;			\
	do {						\
		unsigned offset = pin - stpio_pin_conf;	\
		port = &stpio_port_conf[offset >> 3];	\
		pinno = offset & 7;			\
	} while (0)

void stpio_configure_pin(struct stpio_pin* pin, int direction)
{
	STPIO_PIN_DETAILS(pin, port, pinno);

#ifdef CONFIG_PROC_FS
	pin->direction = direction;
#endif

	ctrl_outl(1<<pinno, port->base + STPIO_PC0_OFFSET +
		  ((direction & (1<<0)) ? STPIO_SET_OFFSET : STPIO_CLEAR_OFFSET));
	ctrl_outl(1<<pinno, port->base + STPIO_PC1_OFFSET +
		  ((direction & (1<<1)) ? STPIO_SET_OFFSET : STPIO_CLEAR_OFFSET));
	ctrl_outl(1<<pinno, port->base + STPIO_PC2_OFFSET +
		  ((direction & (1<<2)) ? STPIO_SET_OFFSET : STPIO_CLEAR_OFFSET));
}

struct stpio_pin* stpio_request_pin(unsigned portno, unsigned pinno,
				    const char* name, int direction)
{
	struct stpio_pin *pin = NULL;

	spin_lock(&stpio_lock);

	if ((portno < STPIO_NPORTS) && (pinno < 8) &&
	    (stpio_pin_conf[portno*8 + pinno].name == NULL)) {
		pin = &stpio_pin_conf[portno*8 + pinno];
		stpio_configure_pin(pin, direction);
		pin->name = name;
	}

	spin_unlock(&stpio_lock);

	return pin;
}

struct stpio_pin* stpio_request_set_pin(unsigned portno, unsigned pinno,
				    const char* name, int direction, unsigned int value)
{
	struct stpio_pin *pin = NULL;

	spin_lock(&stpio_lock);

	if ((portno < STPIO_NPORTS) && (pinno < 8)) {
		pin = &stpio_pin_conf[portno*8 + pinno];
		if( (pin->name == NULL) ) {
		    stpio_set_pin(pin, value);
		    stpio_configure_pin(pin, direction);
		    pin->name = name;
		} else {
		    pin = NULL;
		}
	}

	spin_unlock(&stpio_lock);

	return pin;
}

void stpio_free_pin(struct stpio_pin* pin)
{
	stpio_configure_pin(pin, STPIO_IN);
	pin->name = NULL;
	pin->func = 0;
	pin->dev  = 0;
}

void stpio_set_pin(struct stpio_pin* pin, unsigned int value)
{
	STPIO_PIN_DETAILS(pin, port, pinno);

	ctrl_outl(1<<pinno, port->base + STPIO_POUT_OFFSET +
		  (value ? STPIO_SET_OFFSET : STPIO_CLEAR_OFFSET));
}

unsigned int stpio_get_pin(struct stpio_pin* pin)
{
	STPIO_PIN_DETAILS(pin, port, pinno);

	return (ctrl_inl(port->base + STPIO_PIN_OFFSET) & (1<<pinno)) ? 1 : 0;
}

static irqreturn_t stpio_interrupt(int irq, void *dev)
{
	const struct stpio_port *port = dev;
	unsigned portno = port - stpio_port_conf;
    	unsigned long in, mask, comp;
	unsigned int pinno;

	in   = ctrl_inl(port->base + STPIO_PIN_OFFSET);
	mask = ctrl_inl(port->base + STPIO_PMASK_OFFSET);
	comp = ctrl_inl(port->base + STPIO_PCOMP_OFFSET);

	mask &= in ^ comp;

	while ((pinno = ffs(mask)) != 0) {
		struct stpio_pin *pin;
		pinno--;
		pin = &stpio_pin_conf[portno*8 + pinno];
		if (pin->func != 0)
			pin->func(pin, pin->dev);
		else
			printk(KERN_NOTICE "unexpected PIO interrupt, PIO%d[%d]\n",
			       portno, pinno);
		mask &= ~(1<<pinno);
	}

	return IRQ_HANDLED;
}

void stpio_enable_irq(struct stpio_pin* pin, int comp)
{
    STPIO_PIN_DETAILS(pin, port, pinno);

    ctrl_outl(1<<pinno, port->base + STPIO_PCOMP_OFFSET +
              (comp ? STPIO_SET_OFFSET : STPIO_CLEAR_OFFSET));
    ctrl_outl(1<<pinno, port->base + STPIO_PMASK_OFFSET + STPIO_SET_OFFSET);
}

void stpio_disable_irq(struct stpio_pin* pin)
{
    STPIO_PIN_DETAILS(pin, port, pinno);

    ctrl_outl(1<<pinno, port->base + STPIO_PMASK_OFFSET + STPIO_CLEAR_OFFSET);
}

void stpio_request_irq(struct stpio_pin* pin, int comp,
		       void (*handler)(struct stpio_pin *pin, void *dev),
		       void *dev)
{
	STPIO_PIN_DETAILS(pin, port, pinno);

	disable_irq(port->irq);

	pin->func = handler;
	pin->dev = dev;

        stpio_enable_irq(pin, comp);

	enable_irq(port->irq);
}

void stpio_free_irq(struct stpio_pin* pin)
{
	STPIO_PIN_DETAILS(pin, port, pinno);

	disable_irq(port->irq);

    	stpio_disable_irq(pin);
	pin->func = 0;
	pin->dev = 0;

	enable_irq(port->irq);
}

#ifdef CONFIG_PROC_FS
static struct proc_dir_entry *proc_stpio;

static const char *stpio_dir_name[] =
{
    "IN  (pull-up)      ",
    "BI  (open-drain)   ",
    "OUT (push-pull)    ",
    "forbidden          ",
    "IN  (Hi-Z)         ",
    "forbidden          ",
    "Alt-OUT (push-pull)",
    "Alt-BI (open-drain)"
};

static const char *stpio_get_handler_name(unsigned long addr)
{
    static char sym_name[KSYM_NAME_LEN+1];
    char *modname;
    unsigned long symbolsize, offset;
    const char *symb;

    symb = kallsyms_lookup(addr, &symbolsize, &offset, &modname, sym_name);
    return ( symb ? symb : "");
}

static inline int stpio_proc_info (char *buf, int port, int pin)
{
/*
    PIO port.pin name mode handler_name mask
*/
    struct stpio_pin *pin_ptr = &stpio_pin_conf[port*8 + pin];

    return sprintf(buf, "PIO %d.%d [%-10s] [%s] [%s]\n",
	port, pin,
	(pin_ptr->name ? pin_ptr->name : "     "),
	stpio_dir_name[pin_ptr->direction & 0x7],
	(pin_ptr->func ? stpio_get_handler_name((unsigned long)pin_ptr->func) : "")
	);
}

static int stpio_read_proc (char *page, char **start, off_t off, int count,
			  int *eof, void *data_unused)
{
	int len, l, i, j;
        off_t   begin = 0;

	spin_lock(&stpio_lock);

	len = sprintf(page, "  port      name          direction\n");
        for (i=0; i< STPIO_NPORTS; i++)
	{
	    for(j=0; j < 8; j++ )
	    {
                l = stpio_proc_info(page + len, i, j);
                len += l;
                if (len+begin > off+count)
                        goto done;
                if (len+begin < off) {
                        begin += len;
                        len = 0;
                }
	    }
        }

        *eof = 1;

done:
	spin_unlock(&stpio_lock);
        if (off >= len+begin)
                return 0;
        *start = page + (off-begin);
        return ((count < begin+len-off) ? count : begin+len-off);
}

#endif /* CONFIG_PROC_FS */

static int __init stpio_init(void)
{
	int portno;

    	/* TODO: check return values */

	for (portno=0; portno<STPIO_NPORTS; portno++) {
		const struct stpio_port *port = &stpio_port_conf[portno];
		request_mem_region(port->base, 0x1000, MODNAME);
		request_irq(port->irq, stpio_interrupt,
			    SA_INTERRUPT, MODNAME, (void*)port);
	}

#ifdef CONFIG_PROC_FS
	if ((proc_stpio = create_proc_entry( "stpio", 0, NULL )))
		proc_stpio->read_proc = stpio_read_proc;
#endif

    	printk("STPIO layer initialised\n");
	return 0;
}

static void __exit stpio_exit(void)
{
	int portno;

    	/* TODO: check return values */

#ifdef CONFIG_PROC_FS
	if (proc_stpio)
		remove_proc_entry( "stpio", NULL );
#endif

	for (portno=0; portno<STPIO_NPORTS; portno++) {
		const struct stpio_port *port = &stpio_port_conf[portno];
		release_mem_region(port->base, 0x1000);
		free_irq(port->irq, (void*)port);
	}
}

module_init(stpio_init);
module_exit(stpio_exit);

EXPORT_SYMBOL(stpio_configure_pin);
EXPORT_SYMBOL(stpio_request_pin);
EXPORT_SYMBOL(stpio_request_set_pin);
EXPORT_SYMBOL(stpio_free_pin);
EXPORT_SYMBOL(stpio_get_pin);
EXPORT_SYMBOL(stpio_set_pin);
EXPORT_SYMBOL(stpio_request_irq);
EXPORT_SYMBOL(stpio_free_irq);
EXPORT_SYMBOL(stpio_disable_irq);
EXPORT_SYMBOL(stpio_enable_irq);
