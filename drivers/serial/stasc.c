/*
 *  drivers/serial/stasc.c
 *  Asynchronous serial controller (ASC) driver
 */

#if defined(CONFIG_SERIAL_ST_ASC_CONSOLE) && defined(CONFIG_MAGIC_SYSRQ)
#define SUPPORT_SYSRQ
#endif

#include <linux/module.h>
#include <linux/tty.h>
#include <linux/tty_flip.h>
#include <linux/sysrq.h>
#include <linux/serial.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/console.h>
#include <linux/stm/pio.h>
#include <linux/generic_serial.h>
#include <linux/spinlock.h>
#include <linux/platform_device.h>
#include <linux/stm/soc.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <asm/clock.h>

#ifdef CONFIG_SH_KGDB
#include <asm/kgdb.h>
#endif

#ifdef CONFIG_SH_STANDARD_BIOS
#include <asm/sh_bios.h>
#endif

#include "stasc.h"

#define DRIVER_NAME "stasc"

#ifdef CONFIG_SERIAL_ST_ASC_CONSOLE
/* This is used as a system console, set by serial_console_setup */
static struct console asc_console;
#endif

struct asc_port asc_ports[ASC_MAX_PORTS];

/*---- Forward function declarations---------------------------*/
static int  asc_request_irq(struct uart_port *);
static void asc_free_irq(struct uart_port *);
static void asc_transmit_chars(struct uart_port *);
static int asc_remap_port(struct asc_port *ascport, int req);
void        asc_set_termios_cflag (struct asc_port *, int ,int);
static inline void asc_receive_chars(struct uart_port *);

#ifdef CONFIG_SERIAL_ST_ASC_CONSOLE
static void asc_console_write (struct console *, const char *,
				  unsigned );
static int __init asc_console_setup (struct console *, char *);
#endif

#ifdef CONFIG_SH_KGDB
int kgdb_asc_setup(void);
static void kgdb_put_char(struct uart_port *port, char c);
static struct asc_port *kgdb_asc_port;
#ifdef CONFIG_SH_KGDB_CONSOLE
static struct console kgdb_console;
static struct tty_driver *kgdb_console_device(struct console *, int *);
static int __init kgdb_console_setup(struct console *, char *);
#endif
#endif

/*---- Inline function definitions ---------------------------*/

/* Some simple utility functions to enable and disable interrupts.
 * Note that these need to be called with interrupts disabled.
 */
static inline void asc_disable_tx_interrupts(struct uart_port *port)
{
	unsigned long intenable;

	/* Clear TE (Transmitter empty) interrupt enable in INTEN */
	intenable = asc_in(port, INTEN);
	intenable &= ~ASC_INTEN_THE;
	asc_out(port, INTEN, intenable);
}

static inline void asc_enable_tx_interrupts(struct uart_port *port)
{
	unsigned long intenable;

	/* Set TE (Transmitter empty) interrupt enable in INTEN */
	intenable = asc_in(port, INTEN);
	intenable |= ASC_INTEN_THE;
	asc_out(port, INTEN, intenable);
}

static inline void asc_disable_rx_interrupts(struct uart_port *port)
{
	unsigned long intenable;

	/* Clear RBE (Receive Buffer Full Interrupt Enable) bit in INTEN */
	intenable = asc_in(port, INTEN);
	intenable &= ~ASC_INTEN_RBE;
	asc_out(port, INTEN, intenable);
}


static inline void asc_enable_rx_interrupts(struct uart_port *port)
{
	unsigned long intenable;

	/* Set RBE (Receive Buffer Full Interrupt Enable) bit in INTEN */
	intenable = asc_in(port, INTEN);
	intenable |= ASC_INTEN_RBE;
	asc_out(port, INTEN, intenable);
}

/*----------------------------------------------------------------------*/

/*
 * UART Functions
 */

static unsigned int asc_tx_empty(struct uart_port *port)
{
	unsigned long status;

	status = asc_in(port, STA);
	if (status & ASC_STA_TE)
		return TIOCSER_TEMT;
	return 0;
}

static void asc_set_mctrl(struct uart_port *port, unsigned int mctrl)
{
	/* This routine is used for seting signals of: DTR, DCD, CTS/RTS
	 * We use ASC's hardware for CTS/RTS, so don't need any for that.
	 * Some boards have DTR and DCD implemented using PIO pins,
	 * code to do this should be hooked in here.
	 */
}

static unsigned int asc_get_mctrl(struct uart_port *port)
{
	/* This routine is used for geting signals of: DTR, DCD, DSR, RI,
	   and CTS/RTS */
	return TIOCM_CAR | TIOCM_DSR | TIOCM_CTS;
}

/*
 * There are probably characters waiting to be transmitted.
 * Start doing so.
 * The port lock is held and interrupts are disabled.
 */
static void asc_start_tx(struct uart_port *port)
{
	if (asc_dma_enabled(port))
		asc_fdma_start_tx(port);
	else
		asc_transmit_chars(port);
}

/*
 * Transmit stop - interrupts disabled on entry
 */
static void asc_stop_tx(struct uart_port *port)
{
	if (asc_dma_enabled(port))
		asc_fdma_stop_tx(port);
	else
		asc_disable_tx_interrupts(port);
}

/*
 * Receive stop - interrupts still enabled on entry
 */
static void asc_stop_rx(struct uart_port *port)
{
	if (asc_dma_enabled(port))
		asc_fdma_stop_rx(port);
	else
		asc_disable_rx_interrupts(port);
}

/*
 * Force modem status interrupts on - no-op for us
 */
static void asc_enable_ms(struct uart_port *port)
{
	/* Nothing here yet .. */
}

/*
 * Handle breaks - ignored by us
 */
static void asc_break_ctl(struct uart_port *port, int break_state)
{
	/* Nothing here yet .. */
}

/*
 * Enable port for reception.
 * port_sem held and interrupts disabled
 */
static int asc_startup(struct uart_port *port)
{
	asc_request_irq(port);
	asc_transmit_chars(port);
	asc_enable_rx_interrupts(port);

	return 0;
}

static void asc_shutdown(struct uart_port *port)
{
	if (asc_dma_enabled(port))
		asc_disable_fdma(port);
	asc_disable_tx_interrupts(port);
	asc_disable_rx_interrupts(port);
	asc_free_irq(port);
}

static void asc_set_termios(struct uart_port *port, struct ktermios *termios,
			    struct ktermios *old)
{
	struct asc_port *ascport = container_of(port, struct asc_port, port);
	unsigned int baud;

	baud = uart_get_baud_rate(port, termios, old, 0,
				  port->uartclk/16);

	asc_set_termios_cflag(ascport, termios->c_cflag, baud);
}
static const char *asc_type(struct uart_port *port)
{
	struct platform_device *pdev = to_platform_device(port->dev);
	return pdev->name;
}

static void asc_release_port(struct uart_port *port)
{
	struct platform_device *pdev = to_platform_device(port->dev);
	int size = pdev->resource[0].end - pdev->resource[0].start + 1;

	release_mem_region(port->mapbase, size);

	if (port->flags & UPF_IOREMAP) {
		iounmap(port->membase);
		port->membase = NULL;
	}
}

static int asc_request_port(struct uart_port *port)
{
	struct asc_port *ascport = container_of(port, struct asc_port, port);

	return asc_remap_port(ascport, 1);
}

/* Called when the port is opened, and UPF_BOOT_AUTOCONF flag is set */
/* Set type field if successful */
static void asc_config_port(struct uart_port *port, int flags)
{
	if (flags & UART_CONFIG_TYPE) {
		port->type = PORT_ASC;
		asc_request_port(port);
	}
}

static int
asc_verify_port(struct uart_port *port, struct serial_struct *ser)
{
	/* No user changeable parameters */
	return -EINVAL;
}

/*---------------------------------------------------------------------*/

static struct uart_ops asc_uart_ops = {
	.tx_empty	= asc_tx_empty,
	.set_mctrl	= asc_set_mctrl,
	.get_mctrl	= asc_get_mctrl,
	.start_tx	= asc_start_tx,
	.stop_tx	= asc_stop_tx,
	.stop_rx	= asc_stop_rx,
	.enable_ms	= asc_enable_ms,
	.break_ctl	= asc_break_ctl,
	.startup	= asc_startup,
	.shutdown	= asc_shutdown,
	.set_termios	= asc_set_termios,
	.type		= asc_type,
	.release_port	= asc_release_port,
	.request_port	= asc_request_port,
	.config_port	= asc_config_port,
	.verify_port	= asc_verify_port,
};

static void __devinit asc_init_port(struct asc_port *ascport,
				    struct platform_device *pdev)
{
	struct uart_port *port = &ascport->port;
	struct stasc_uart_data *data = pdev->dev.platform_data;
	struct clk *clk;
	unsigned long rate;
	int i;

	port->iotype	= UPIO_MEM;
	port->flags	= UPF_BOOT_AUTOCONF;
	port->ops	= &asc_uart_ops,
	port->fifosize	= FIFO_SIZE;
	port->line	= pdev->id;
	port->dev	= &pdev->dev;

	port->mapbase	= pdev->resource[0].start;
	port->irq	= pdev->resource[1].start;

	/* Assume that we can always use ioremap */
	port->flags	|= UPF_IOREMAP;
	port->membase	= NULL;

	clk = clk_get(NULL, "comms_clk");
	if (IS_ERR(clk)) clk = clk_get(NULL, "bus_clk");
	rate = clk_get_rate(clk);
	clk_put(clk);

	ascport->port.uartclk = rate;

	ascport->pio_port = data->pio_port;
	for (i=0; i<4; i++)
		ascport->pio_pin[i] = data->pio_pin[i];
}

static struct uart_driver asc_uart_driver = {
	.owner		= THIS_MODULE,
	.driver_name	= DRIVER_NAME,
	.dev_name	= "ttyAS",
	.major		= ASC_MAJOR,
	.minor		= ASC_MINOR_START,
	.nr		= ASC_MAX_PORTS,
	.cons		= &asc_console,
};

#ifdef CONFIG_SERIAL_ST_ASC_CONSOLE
static struct console asc_console = {
	.name		= "ttyAS",
	.device		= uart_console_device,
	.write		= asc_console_write,
	.setup		= asc_console_setup,
	.flags		= CON_PRINTBUFFER,
	.index		= -1,
	.data		= &asc_uart_driver,
};

/*
 * Early console initialization.
 */
static int __init asc_console_init(void)
{
	if (asc_default_console_device) {
		add_preferred_console("ttyAS", asc_default_console_device->id,
				      NULL);
		asc_init_port(&asc_ports[asc_default_console_device->id],
			      asc_default_console_device);
		register_console(&asc_console);
        }

        return 0;
}
console_initcall(asc_console_init);

/*
 * Late console initialization.
 */
static int __init asc_late_console_init(void)
{
	if (asc_default_console_device && !(asc_console.flags & CON_ENABLED))
		register_console(&asc_console);

        return 0;
}
core_initcall(asc_late_console_init);
#endif

static int __devinit asc_serial_probe(struct platform_device *pdev)
{
	int ret;
	struct asc_port *ascport = &asc_ports[pdev->id];

	asc_init_port(ascport, pdev);

	ret = uart_add_one_port(&asc_uart_driver, &ascport->port);
	if (ret == 0) {
		platform_set_drvdata(pdev, &ascport->port);
        }

	return ret;
}

static int __devexit asc_serial_remove(struct platform_device *pdev)
{
	struct uart_port *port = platform_get_drvdata(pdev);

        platform_set_drvdata(pdev, NULL);
	return uart_remove_one_port(&asc_uart_driver, port);
}

static struct platform_driver asc_serial_driver = {
	.probe		= asc_serial_probe,
	.remove		= __devexit_p(asc_serial_remove),
	.driver	= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
};

static int __init asc_init(void)
{
	int ret;
	static char banner[] __initdata =
		KERN_INFO "STMicroelectronics ASC driver initialized\n";

	printk(banner);

	asc_fdma_setreq();

	ret = uart_register_driver(&asc_uart_driver);
	if (ret)
		return ret;

	ret = platform_driver_register(&asc_serial_driver);
	if (ret)
		uart_unregister_driver(&asc_uart_driver);

	return ret;
}

static void __exit asc_exit(void)
{
	platform_driver_unregister(&asc_serial_driver);
	uart_unregister_driver(&asc_uart_driver);
}

module_init(asc_init);
module_exit(asc_exit);

MODULE_AUTHOR("Stuart Menefy <stuart.menefy@st.com>");
MODULE_DESCRIPTION("STMicroelectronics ASC serial port driver");
MODULE_LICENSE("GPL");

/*----------------------------------------------------------------------*/

/* This sections contains code to support the use of the ASC as a
 * generic serial port.
 */

static int asc_remap_port(struct asc_port *ascport, int req)
{
	struct uart_port *port = &ascport->port;
	struct platform_device *pdev = to_platform_device(port->dev);
	int size = pdev->resource[0].end - pdev->resource[0].start + 1;
	int i;
	static int pio_dirs[4] = {
		STPIO_ALT_OUT,	/* Tx */
		STPIO_IN,	/* Rx */
		STPIO_IN,	/* CTS */
		STPIO_ALT_OUT	/* RTS */
	};

	if (req && !request_mem_region(port->mapbase, size, pdev->name))
		return -EBUSY;

	/* We have already been remapped for the console */
	if (port->membase)
		return 0;

	if (port->flags & UPF_IOREMAP) {
		port->membase = ioremap(port->mapbase, size);
		if (port->membase == NULL) {
			release_mem_region(port->mapbase, size);
			return -ENOMEM;
		}
	}

	for (i=0; i<4; i++) {
		ascport->pios[i] = stpio_request_pin(ascport->pio_port,
			ascport->pio_pin[0], DRIVER_NAME, pio_dirs[i]);
	}

	return 0;
}

static int asc_set_baud (struct uart_port *port, int baud)
{
	unsigned int t;
	unsigned long rate;

	rate = port->uartclk;

	if (baud < 19200) {
		t = BAUDRATE_VAL_M0(baud, rate);
		asc_out (port, BAUDRATE, t);
		return 0;
	} else {
		t = BAUDRATE_VAL_M1(baud, rate);
		asc_out (port, BAUDRATE, t);
		return ASC_CTL_BAUDMODE;
	}
}

void
asc_set_termios_cflag (struct asc_port *ascport, int cflag, int baud)
{
	struct uart_port *port = &ascport->port;
	unsigned int ctrl_val;
	unsigned long flags;

	/* wait for end of current transmission */
	while (!asc_tx_empty(port)){};

	spin_lock_irqsave(&port->lock, flags);

	/* read control register */
	ctrl_val = asc_in (port, CTL);

	/* stop serial port and reset value */
	asc_out (port, CTL, (ctrl_val & ~ASC_CTL_RUN));
	ctrl_val = ASC_CTL_RXENABLE | ASC_CTL_FIFOENABLE;

	/* reset fifo rx & tx */
	asc_out (port, TXRESET, 1);
	asc_out (port, RXRESET, 1);

	/* set character length */
	if ((cflag & CSIZE) == CS7)
		ctrl_val |= ASC_CTL_MODE_7BIT_PAR;
	else {
		if (cflag & PARENB)
			ctrl_val |= ASC_CTL_MODE_8BIT_PAR;
		else
			ctrl_val |= ASC_CTL_MODE_8BIT;
	}

	/* set stop bit */
	if (cflag & CSTOPB)
		ctrl_val |= ASC_CTL_STOP_2BIT;
	else
		ctrl_val |= ASC_CTL_STOP_1BIT;

	/* odd parity */
	if (cflag & PARODD)
		ctrl_val |= ASC_CTL_PARITYODD;

	/* hardware flow control */
	if (cflag & CRTSCTS)
		ctrl_val |= ASC_CTL_CTSENABLE;

	/* hardware flow control */
	if (cflag & CRTSCTS)
		ctrl_val |= ASC_CTL_CTSENABLE;

	/* set speed and baud generator mode */
	ctrl_val |= asc_set_baud (port, baud);
	uart_update_timeout(port, cflag, baud);

	/* Undocumented feature: use max possible baud */
	if (cflag & 0020000)
		asc_out (port, BAUDRATE, 0x0000ffff);

	/* Undocumented feature: use DMA */
	if (cflag & 0040000)
		asc_enable_fdma(port);
	else
		asc_disable_fdma(port);

	/* Undocumented feature: use local loopback */
	if (cflag & 0100000)
		ctrl_val |= ASC_CTL_LOOPBACK;

	/* Set the timeout */
	asc_out(port, TIMEOUT, 16);

	/* write final value and enable port */
	asc_out (port, CTL, (ctrl_val | ASC_CTL_RUN));

	spin_unlock_irqrestore(&port->lock, flags);
}


static inline unsigned asc_hw_txroom(struct uart_port* port)
{
	unsigned long status;

	status = asc_in(port, STA);
	if (status & ASC_STA_THE) {
		return FIFO_SIZE/2;
	} else if (! (status & ASC_STA_TF)) {
		return 1;
	} else {
		return 0;
	}
}

/*
 * Start transmitting chars.
 * This is called from both interrupt and task level.
 * Either way interrupts are disabled.
 */
static void asc_transmit_chars(struct uart_port *port)
{
	struct circ_buf *xmit = &port->info->xmit;
	int txroom;
	unsigned long intenable;
	unsigned char c;

	txroom = asc_hw_txroom(port);

	if ((txroom != 0) && port->x_char) {
		c = port->x_char;
		port->x_char = 0;
		asc_out (port, TXBUF, c);
		port->icount.tx++;
		txroom = asc_hw_txroom(port);
	}

	while (txroom > 0) {
		if (uart_tx_stopped(port) || uart_circ_empty(xmit)) {
			break;
		}

		do {
			c = xmit->buf[xmit->tail];
			xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
			asc_out (port, TXBUF, c);
			port->icount.tx++;
			txroom--;
		} while ((txroom > 0) && (!uart_circ_empty(xmit)));

		txroom = asc_hw_txroom(port);
	}

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS) {
		uart_write_wakeup(port);
	}

	intenable = asc_in(port, INTEN);
	if (port->x_char || (!uart_circ_empty(xmit))) {
		intenable |= ASC_INTEN_THE;
	} else {
		intenable &= ~ASC_INTEN_THE;
	}
	asc_out(port, INTEN, intenable);
}

static inline void asc_receive_chars(struct uart_port *port)
{
	int count;
	struct tty_struct *tty = port->info->tty;
	int copied=0;
	unsigned long status;
	unsigned long c = 0;
	char flag;
	int overrun;

	while (1) {
		status = asc_in(port, STA);
		if (status & ASC_STA_RHF) {
			count = FIFO_SIZE / 2;
		} else if (status & ASC_STA_RBF) {
			count = 1;
		} else {
			count = 0;
		}

		/* Check for overrun before reading any data from the
		 * RX FIFO, as this clears the overflow error condition. */
		overrun = status & ASC_STA_OE;

		/* Don't copy more bytes than there are room for in the buffer */
		count = tty_buffer_request_room(tty, count);

		/* If for any reason we can't copy more data, we're done! */
		if (count == 0)
			break;

		for ( ; count != 0; count--) {
			c = asc_in(port, RXBUF);
			flag = TTY_NORMAL;
			port->icount.rx++;

			if (unlikely(c & ASC_RXBUF_FE)) {
				if (c == ASC_RXBUF_FE) {
					port->icount.brk++;
					if (uart_handle_break(port))
						continue;
					flag = TTY_BREAK;
				} else {
					port->icount.frame++;
					flag = TTY_FRAME;
				}
			} else if (unlikely(c & ASC_RXBUF_PE)) {
				port->icount.parity++;
				flag = TTY_PARITY;
			}

			if (uart_handle_sysrq_char(port, c))
				continue;
			tty_insert_flip_char(tty, c & 0xff, flag);
		}

		if (overrun) {
			port->icount.overrun++;
			tty_insert_flip_char(tty, 0, TTY_OVERRUN);
		}

		copied=1;
	}

	if (copied) {
		/* Tell the rest of the system the news. New characters! */
		tty_flip_buffer_push(tty);
	}
}

static irqreturn_t asc_interrupt(int irq, void *ptr)
{
	struct uart_port *port = ptr;
	unsigned long status;

	spin_lock(&port->lock);

	status = asc_in (port, STA);
	if (status & ASC_STA_RBF) {
		/* Receive FIFO not empty */
		asc_receive_chars(port);
	}

	if ((status & ASC_STA_THE) && (asc_in(port, INTEN) & ASC_INTEN_THE)) {
		/* Transmitter FIFO at least half empty */
		asc_transmit_chars(port);
	}

	spin_unlock(&port->lock);

	return IRQ_HANDLED;
}

static int asc_request_irq(struct uart_port *port)
{
        struct platform_device *pdev = to_platform_device(port->dev);

	if (request_irq(port->irq, asc_interrupt, 0,
			pdev->name, port)) {
		printk(KERN_ERR "stasc: cannot allocate irq.\n");
		return -ENODEV;
	}
	return 0;
}

static void asc_free_irq(struct uart_port *port)
{
	free_irq(port->irq, port);
}

/*----------------------------------------------------------------------*/

#if defined(CONFIG_SH_STANDARD_BIOS) || defined(CONFIG_SH_KGDB)

static int get_char(struct uart_port *port)
{
        int c;
	unsigned long status;

	do {
		status = asc_in(port, STA);
	} while (! (status & ASC_STA_RBF));

	c = asc_in(port, RXBUF);

        return c;
}

/* Taken from sh-stub.c of GDB 4.18 */
static const char hexchars[] = "0123456789abcdef";

static __inline__ char highhex(int  x)
{
	return hexchars[(x >> 4) & 0xf];
}

static __inline__ char lowhex(int  x)
{
	return hexchars[x & 0xf];
}
#endif

static void
put_char (struct uart_port *port, char c)
{
	unsigned long flags;
	unsigned long status;

	spin_lock_irqsave(&port->lock, flags);

	do {
		status = asc_in (port, STA);
	} while (status & ASC_STA_TF);

	asc_out (port, TXBUF, c);

	spin_unlock_irqrestore(&port->lock, flags);
}

/*
 * Send the packet in buffer.  The host gets one chance to read it.
 * This routine does not wait for a positive acknowledge.
 */

static void
put_string (struct uart_port *port, const char *buffer, int count)
{
	int i;
	const unsigned char *p = buffer;
#if defined(CONFIG_SH_STANDARD_BIOS) || defined(CONFIG_SH_KGDB)
	int checksum;
	int usegdb=0;

    	/* This call only does a trap the first time it is
	 * called, and so is safe to do here unconditionally
	 */
	usegdb |= sh_bios_in_gdb_mode();
#ifdef CONFIG_SH_KGDB
	usegdb |= (kgdb_in_gdb_mode && (port == &kgdb_asc_port->port));
#endif

	if (usegdb) {
	    /*  $<packet info>#<checksum>. */
	    do {
		unsigned char c;
		put_char(port, '$');
		put_char(port, 'O'); /* 'O'utput to console */
		checksum = 'O';

		for (i=0; i<count; i++) { /* Don't use run length encoding */
			int h, l;

			c = *p++;
			h = highhex(c);
			l = lowhex(c);
			put_char(port, h);
			put_char(port, l);
			checksum += h + l;
		}
		put_char(port, '#');
		put_char(port, highhex(checksum));
		put_char(port, lowhex(checksum));
	    } while  (get_char(port) != '+');
	} else
#endif /* CONFIG_SH_STANDARD_BIOS || CONFIG_SH_KGDB */

	for (i = 0; i < count; i++) {
		if (*p == 10)
			put_char (port, '\r');
		put_char (port, *p++);
	}
}

/*----------------------------------------------------------------------*/

/*
 *  Setup initial baud/bits/parity. We do two things here:
 *	- construct a cflag setting for the first rs_open()
 *	- initialize the serial port
 *  Return non-zero if we didn't find a serial port.
 */

static int __init
asc_console_setup (struct console *co, char *options)
{
	struct asc_port *ascport = &asc_ports[co->index];
	int     baud = 9600;
	int     bits = 8;
	int     parity = 'n';
	int     flow = 'n';
	int ret;

	if (ascport->port.membase == 0)	/* Port not initialized yet - delay setup */
		return -ENODEV;

	if ((ret = asc_remap_port(ascport, 0)) != 0)
		return ret;

	if (options) {
                uart_parse_options(options, &baud, &parity, &bits, &flow);
	}

	return uart_set_options(&ascport->port, co, baud, parity, bits, flow);
}

/*
 *  Print a string to the serial port trying not to disturb
 *  any possible real use of the port...
 */

static void
asc_console_write (struct console *co, const char *s, unsigned count)
{
	struct uart_port *port = &asc_ports[co->index].port;

	put_string(port, s, count);
}

/*----------------------------------------------------------------------*/
/* KGDB ASC functions */

#ifdef CONFIG_SH_KGDB
/* write a char */
static void kgdb_put_char(struct uart_port *port, char c)
{
	unsigned long flags;
	unsigned long status;

	local_irq_save(flags);

	do {
		status = asc_in (port, STA);
	} while (status & ASC_STA_TF);

	asc_out (port, TXBUF, c);

	local_irq_restore(flags);
}

/* Called from stub to put a character */
static void kgdb_asc_putchar(int c)
{
        kgdb_put_char(&kgdb_asc_port->port, c);
}

/* Called from stub to get a character, i.e. is blocking */
static int kgdb_asc_getchar(void)
{
	return get_char(&kgdb_asc_port->port);
}

/* Initialise the KGDB serial port.
   Called from stub to setup the debug port
*/
int kgdb_asc_setup(void)
{
	int cflag = CREAD | HUPCL | CLOCAL | CSTOPB;

	if ((kgdb_portnum < 0) || (kgdb_portnum >= ASC_NPORTS))
	{
		printk (KERN_ERR "stasc: invalid ASC port number\n");
		return -1;
	}

        kgdb_asc_port = &asc_ports[kgdb_portnum];

	switch (kgdb_baud) {
        case 115200:
                cflag |= B115200;
                break;
	case 57600:
                cflag |= B57600;
                break;
        case 38400:
                cflag |= B38400;
                break;
        case 19200:
                cflag |= B19200;
                break;
	case 9600:
		cflag = B9600;
		break;
        default:
                cflag |= B115200;
                kgdb_baud = 115200;
		printk (KERN_WARNING "stasc: force the kgdb baud as %d\n",
			kgdb_baud);
                break;
        }

	switch (kgdb_bits) {
        case '7':
                cflag |= CS7;
                break;
        default:
        case '8':
                cflag |= CS8;
                break;
        }

        switch (kgdb_parity) {
        case 'O':
                cflag |= PARODD;
                break;
        case 'E':
                cflag |= PARENB;
                break;
        }
        kgdb_cflag = cflag;
        asc_set_termios_cflag(kgdb_asc_port, kgdb_cflag, kgdb_baud);

	/* Setup complete: initialize function pointers */
	kgdb_getchar = kgdb_asc_getchar;
	kgdb_putchar = kgdb_asc_putchar;

        return 0;
}

#ifdef CONFIG_SH_KGDB_CONSOLE
/* Create a console device */
static struct tty_driver *kgdb_console_device(struct console *co, int *index)
{
	struct uart_driver *p = co->data;
	*index = co->index;
	return p->tty_driver;
}

/* Set up the KGDB console */
static int __init kgdb_console_setup(struct console *co, char *options)
{
        /* NB we ignore 'options' because we've already done the setup */
        co->cflag = kgdb_cflag;
        return 0;
}

/* The console structure for KGDB */
static struct console kgdb_console= {
	.name		= "ttyAS",
	.device		= kgdb_console_device,
	.write		= kgdb_console_write,
	.setup		= kgdb_console_setup,
	.flags		= CON_PRINTBUFFER | CON_ENABLED,
	.index		= -1,
	.data		= &asc_uart_driver,
};

#ifdef CONFIG_KGDB_DEFTYPE_ASC
/* Register the KGDB console so we get messages (d'oh!) */
int __init kgdb_console_init(void)
{
        register_console(&kgdb_console);
	return 0;
}
console_initcall(kgdb_console_init);
#endif
#endif /* CONFIG_SH_KGDB_CONSOLE */
#endif /* CONFIG_SH_KGDB */
