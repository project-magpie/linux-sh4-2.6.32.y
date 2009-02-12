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
#include <linux/stm/fdma-reqs.h>

#include <asm/system.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/uaccess.h>
#include <asm/bitops.h>
#include <asm/clock.h>

#ifdef CONFIG_KGDB
#include <linux/kgdb.h>
#endif

#ifdef CONFIG_SH_STANDARD_BIOS
#include <asm/sh_bios.h>
#endif

#include "stasc.h"

#define DRIVER_NAME "stasc"

#ifdef CONFIG_SERIAL_ST_ASC_CONSOLE
static struct console asc_console;
#endif

struct asc_port asc_ports[ASC_MAX_PORTS];

/*---- Forward function declarations---------------------------*/
static int  asc_request_irq(struct uart_port *);
static void asc_free_irq(struct uart_port *);
static void asc_transmit_chars(struct uart_port *);
static int asc_remap_port(struct asc_port *ascport, int req);
void asc_set_termios_cflag (struct asc_port *, int ,int);
static inline void asc_receive_chars(struct uart_port *);

#ifdef CONFIG_SERIAL_ST_ASC_CONSOLE
static void asc_console_write (struct console *, const char *,
				  unsigned );
static int __init asc_console_setup (struct console *, char *);
#endif

#ifdef CONFIG_KGDB_ST_ASC
static int kgdbasc_baud = CONFIG_KGDB_BAUDRATE;
static int kgdbasc_portno = CONFIG_KGDB_PORT_NUM;
static struct asc_port *kgdb_asc_port;
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
	if (asc_fdma_enabled(port))
		asc_fdma_tx_start(port);
	else {
		struct circ_buf *xmit = &port->info->xmit;
		asc_transmit_chars(port);
		if (!uart_circ_empty(xmit))
			asc_enable_tx_interrupts(port);
	}
}

/*
 * Transmit stop - interrupts disabled on entry
 */
static void asc_stop_tx(struct uart_port *port)
{
	if (asc_fdma_enabled(port))
		asc_fdma_tx_stop(port);
	else
		asc_disable_tx_interrupts(port);
}

/*
 * Receive stop - interrupts still enabled on entry
 */
static void asc_stop_rx(struct uart_port *port)
{
	if (asc_fdma_enabled(port))
		asc_fdma_rx_stop(port);
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
	asc_fdma_startup(port);
	asc_transmit_chars(port);
	asc_enable_rx_interrupts(port);

	return 0;
}

static void asc_shutdown(struct uart_port *port)
{
	asc_disable_tx_interrupts(port);
	asc_disable_rx_interrupts(port);
	asc_fdma_shutdown(port);
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
	struct asc_port *ascport = container_of(port, struct asc_port, port);
	struct platform_device *pdev = to_platform_device(port->dev);
	int size = pdev->resource[0].end - pdev->resource[0].start + 1;
	int i;

	release_mem_region(port->mapbase, size);

	if (port->flags & UPF_IOREMAP) {
		iounmap(port->membase);
		port->membase = NULL;
	}

	for (i=0; i<((ascport->flags & STASC_FLAG_NORTSCTS) ? 2 : 4); i++)
		stpio_free_pin(ascport->pios[i]);
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
	if ((flags & UART_CONFIG_TYPE) &&
	    (asc_request_port(port) == 0))
		port->type = PORT_ASC;
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

	port->iotype	= UPIO_MEM;
	port->flags	= UPF_BOOT_AUTOCONF;
	port->ops	= &asc_uart_ops;
	port->fifosize	= FIFO_SIZE;
	port->line	= pdev->id;
	port->dev	= &pdev->dev;

	port->mapbase	= pdev->resource[0].start;
	port->irq	= pdev->resource[1].start;

#ifdef CONFIG_SERIAL_ST_ASC_FDMA
	ascport->fdma.rx_req_id = pdev->resource[2].start;
	ascport->fdma.tx_req_id = pdev->resource[3].start;
#endif

	/* Assume that we can always use ioremap */
	port->flags	|= UPF_IOREMAP;
	port->membase	= NULL;

	clk = clk_get(NULL, "comms_clk");
	if (IS_ERR(clk)) clk = clk_get(NULL, "bus_clk");
	rate = clk_get_rate(clk);
	clk_put(clk);

	ascport->port.uartclk = rate;

	ascport->flags = data->flags;
}

static struct uart_driver asc_uart_driver = {
	.owner		= THIS_MODULE,
	.driver_name	= DRIVER_NAME,
	.dev_name	= "ttyAS",
	.major		= ASC_MAJOR,
	.minor		= ASC_MINOR_START,
	.nr		= ASC_MAX_PORTS,
#ifdef CONFIG_SERIAL_ST_ASC_CONSOLE
	.cons		= &asc_console,
#endif
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

static void __init asc_init_ports(void)
{
	int i;

	for (i = 0; i < stasc_configured_devices_count; i++)
		asc_init_port(&asc_ports[i], stasc_configured_devices[i]);
}

/*
 * Early console initialization.
 */
static int __init asc_console_init(void)
{
	if (!stasc_configured_devices_count)
		return 0;

	asc_init_ports();
	register_console(&asc_console);
	if (stasc_console_device != -1)
		add_preferred_console("ttyAS", stasc_console_device, NULL);

        return 0;
}
console_initcall(asc_console_init);

/*
 * Late console initialization.
 */
static int __init asc_late_console_init(void)
{
	if (!(asc_console.flags & CON_ENABLED))
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

#ifdef CONFIG_PM
static int asc_serial_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct asc_port *ascport = &asc_ports[pdev->id];
	struct uart_port *port   = &(ascport->port);

	if (!device_can_wakeup(&(pdev->dev)))
		return 0; /* the other ASCs... */

	if (state.event == PM_EVENT_SUSPEND && device_may_wakeup(&(pdev->dev))){
#ifndef CONFIG_DISABLE_CONSOLE_SUSPEND
		ascport->flags |= ASC_SUSPENDED;
		if (ascport->pios[0])
			stpio_configure_pin(ascport->pios[0], STPIO_IN); /* Tx  */
		asc_disable_tx_interrupts(port);
#endif
		return 0; /* leaves the rx interrupt enabled! */
	}

	if (state.event == PM_EVENT_FREEZE) {
		asc_disable_rx_interrupts(port);
		return 0;
	}
	if (ascport->pios[0])
		stpio_configure_pin(ascport->pios[0], STPIO_IN); /* Tx  */
	if (ascport->pios[3])
		stpio_configure_pin(ascport->pios[3], STPIO_IN); /* RTS */
	ascport->flags |= ASC_SUSPENDED;
	asc_disable_tx_interrupts(port);
	asc_disable_rx_interrupts(port);
	return 0;
}

static int asc_set_baud (struct uart_port *port, int baud);
static int asc_serial_resume(struct platform_device *pdev)
{
	struct asc_port *ascport = &asc_ports[pdev->id];
	struct uart_port *port   = &(ascport->port);
	struct stasc_uart_data *pdata =
		(struct stasc_uart_data *)pdev->dev.platform_data;
	int i;

	if (!device_can_wakeup(&(pdev->dev)))
		return 0; /* the other ASCs... */

	/* Reconfigure the Pio Pins */
	for (i = 0; i < 4; ++i)
		if (ascport->pios[i])
			stpio_configure_pin(ascport->pios[i], pdata->pio_direction[i]);

	asc_enable_rx_interrupts(port);
	asc_enable_tx_interrupts(port);
	ascport->flags &= ~ASC_SUSPENDED;
	return 0;
}
#else
#define asc_serial_suspend	NULL
#define asc_serial_resume	NULL
#endif

static struct platform_driver asc_serial_driver = {
	.probe		= asc_serial_probe,
	.remove		= __devexit_p(asc_serial_remove),
	.driver	= {
		.name	= DRIVER_NAME,
		.owner	= THIS_MODULE,
	},
	.suspend = asc_serial_suspend,
	.resume	= asc_serial_resume,
};

static int __init asc_init(void)
{
	int ret;
	static char banner[] __initdata =
		KERN_INFO "STMicroelectronics ASC driver initialized\n";

	printk(banner);

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
	struct stasc_uart_data *pdata =
		(struct stasc_uart_data *)pdev->dev.platform_data;
	int size = pdev->resource[0].end - pdev->resource[0].start + 1;
	int i;

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

	for (i=0; i<((ascport->flags & STASC_FLAG_NORTSCTS) ? 2 : 4); i++) {
		ascport->pios[i] = stpio_request_pin(pdata->pio_port,
			pdata->pio_pin[i], DRIVER_NAME, pdata->pio_direction[i]);
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
	if ((cflag & CRTSCTS) && (!(ascport->flags & STASC_FLAG_NORTSCTS)))
		ctrl_val |= ASC_CTL_CTSENABLE;

	/* set speed and baud generator mode */
	ascport->baud = baud;
	ctrl_val |= asc_set_baud (port, baud);
	uart_update_timeout(port, cflag, baud);

	/* Undocumented feature: use max possible baud */
	if (cflag & 0020000)
		asc_out(port, BAUDRATE, 0x0000ffff);

	/* Undocumented feature: FDMA "acceleration" */
	if ((cflag & 0040000) && !asc_fdma_enabled(port)) {
		/* TODO: check parameters if suitable for FDMA transmission */
		asc_disable_tx_interrupts(port);
		asc_disable_rx_interrupts(port);
		if (asc_fdma_enable(port) != 0) {
			asc_enable_rx_interrupts(port);
			asc_enable_tx_interrupts(port);
		}
	} else if (!(cflag & 0040000) && asc_fdma_enabled(port)) {
		asc_fdma_disable(port);
		asc_enable_rx_interrupts(port);
		asc_enable_tx_interrupts(port);
	}

	/* Undocumented feature: use local loopback */
	if (cflag & 0100000)
		ctrl_val |= ASC_CTL_LOOPBACK;
	else
		ctrl_val &= ~ASC_CTL_LOOPBACK;

	/* Set the timeout */
	asc_out(port, TIMEOUT, 20);

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
	unsigned char c;

	txroom = asc_hw_txroom(port);

	if ((txroom != 0) && port->x_char) {
		c = port->x_char;
		port->x_char = 0;
		asc_out (port, TXBUF, c);
		port->icount.tx++;
		txroom = asc_hw_txroom(port);
	}

	if (uart_tx_stopped(port)) {
		/*
		 * We should try and stop the hardware here, but I
		 * don't think the ASC has any way to do that.
		 */
		asc_disable_tx_interrupts(port);
		return;
	}

	if (uart_circ_empty(xmit)) {
		asc_disable_tx_interrupts(port);
		return;
	}

	if (txroom == 0)
		return;

	do {
		c = xmit->buf[xmit->tail];
		xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
		asc_out (port, TXBUF, c);
		port->icount.tx++;
		txroom--;
	} while ((txroom > 0) && (!uart_circ_empty(xmit)));

	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS) {
		uart_write_wakeup(port);
	}

	if (uart_circ_empty(xmit))
		asc_disable_tx_interrupts(port);
}

static inline void asc_receive_chars(struct uart_port *port)
{
	int count;
	struct tty_struct *tty = port->info->port.tty;
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
#if defined(CONFIG_KGDB_ST_ASC)
		if (port == &kgdb_asc_port->port) {
			if ((strncmp(tty->buf.head->char_buf_ptr,
			     "$Hc-1#09",8) == 0)) {
				breakpoint();
				return;
			}
		}
#endif
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

/*
 * The BRK_STATUS value has been calculated looking the status
 * value into asc_interrupt function. When, on the system serial console,
 * a <break> command is sent (for example "^af" using, under linux host,
 * the minicom tool) the status is 0x57.
 * In this case, the ASCx_STA register has: the RXBUFFFULL bit = 1
 * (RX FIFO not empty), the Transmitter empty flag = 1, there is not
 * a parity error and the FRAMEERROR bit is 1 (stop bit not found).
 */
#define BRK_STATUS 0x57

static irqreturn_t asc_interrupt(int irq, void *ptr)
{
	struct uart_port *port = ptr;
	unsigned long status;

	spin_lock(&port->lock);

#if defined(CONFIG_KGDB_ST_ASC)
        /* To be Fixed: it seems that on a lot of ST40 platforms the breakpoint
           condition is not checked without this delay. This problem probably
           depends on an invalid port speed configuration.
         */
        udelay(1000);
#endif

	status = asc_in (port, STA);

	if (asc_fdma_enabled(port)) {
		/* FDMA transmission, only timeout-not-empty
		 * interrupt shall be enabled */
		if (likely(status & ASC_STA_TNE))
			asc_fdma_rx_timeout(port);
		else
			printk(KERN_ERR"Unknown ASC interrupt for port %p!"
			    "(ASC_STA = %08x)\n", port, asc_in(port, STA));
	} else {
		if (status & ASC_STA_RBF) {
			/* Receive FIFO not empty */
			asc_receive_chars(port);
		}

#if defined(CONFIG_KGDB_ST_ASC)
	if ((port == &kgdb_asc_port->port) &&
			(status == BRK_STATUS)){
		breakpoint();
	}
#endif

		if ((status & ASC_STA_THE) &&
				(asc_in(port, INTEN) & ASC_INTEN_THE)) {
			/* Transmitter FIFO at least half empty */
			asc_transmit_chars(port);
		}
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

#if defined(CONFIG_SH_STANDARD_BIOS) || defined(CONFIG_KGDB)

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

#ifdef CONFIG_SERIAL_ST_ASC_CONSOLE
static void
put_char (struct uart_port *port, char c)
{
	unsigned long flags;
	unsigned long status;
	struct asc_port *ascport = container_of(port, struct asc_port, port);

	if (ascport->flags & ASC_SUSPENDED)
		return;
try_again:
	do {
		status = asc_in (port, STA);
	} while (status & ASC_STA_TF);

	spin_lock_irqsave(&port->lock, flags);

	status = asc_in (port, STA);
	if (status & ASC_STA_TF) {
		spin_unlock_irqrestore(&port->lock, flags);
		goto try_again;
	}

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
#if defined(CONFIG_SH_STANDARD_BIOS)
	int checksum;
	int usegdb=0;

    	/* This call only does a trap the first time it is
	 * called, and so is safe to do here unconditionally
	 */
	usegdb |= sh_bios_in_gdb_mode();

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
#endif /* CONFIG_SH_STANDARD_BIOS */

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
	struct asc_port *ascport;
	int     baud = 9600;
	int     bits = 8;
	int     parity = 'n';
	int     flow = 'n';
	int ret;

	if (co->index >= ASC_MAX_PORTS)
		co->index = 0;

	ascport = &asc_ports[co->index];
	if ((ascport->port.mapbase == 0))
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
#endif /* CONFIG_SERIAL_ST_ASC_CONSOLE */


/* ===============================================================================
				KGDB Functions
   =============================================================================== */
#ifdef CONFIG_KGDB_ST_ASC
static int kgdbasc_read_char(void)
{
	return get_char(&kgdb_asc_port->port);
}

/* Called from kgdbstub.c to put a character, just a wrapper */
static void kgdbasc_write_char(u8 c)
{
	put_char(&kgdb_asc_port->port, c);
}

static int kgdbasc_set_termios(void)
{
	struct termios termios;

	memset(&termios, 0, sizeof(struct termios));

	termios.c_cflag = CREAD | HUPCL | CLOCAL | CS8;
	switch (kgdbasc_baud) {
		case 9600:
			termios.c_cflag |= B9600;
			break;
		case 19200:
			termios.c_cflag |= B19200;
			break;
		case 38400:
			termios.c_cflag |= B38400;
			break;
		case 57600:
			termios.c_cflag |= B57600;
			break;
		case 115200:
			termios.c_cflag |= B115200;
			break;
	}
	asc_set_termios_cflag(kgdb_asc_port, termios.c_cflag, kgdbasc_baud);
	return 0;
}

static irqreturn_t kgdbasc_interrupt(int irq, void *ptr)
{
	/*
	* If  there is some other CPU in KGDB then this is a
	* spurious interrupt. so return without even checking a byte
	*/
	if (atomic_read(&debugger_active))
		return IRQ_NONE;

	breakpoint();
	return IRQ_HANDLED;
}

static void __init kgdbasc_lateinit(void)
{
	kgdb_asc_port = &asc_ports[kgdbasc_portno];

	if (asc_console.index != kgdbasc_portno) {
		kgdbasc_set_termios();

		if (request_irq(kgdb_asc_port->port.irq, kgdbasc_interrupt,
			0, "stasc", &kgdb_asc_port->port)) {
			printk(KERN_ERR "kgdb asc: cannot allocate irq.\n");
			return;
		}
		asc_enable_rx_interrupts(&kgdb_asc_port->port);
	}
	return;
}

/* Early I/O initialization not yet supported ... */
struct kgdb_io kgdb_io_ops = {
	.read_char = kgdbasc_read_char,
	.write_char = kgdbasc_write_char,
	.late_init = kgdbasc_lateinit,
};

/*
* Syntax for this cmdline option is "kgdbasc=ttyno,baudrate".
*/
static int __init
kgdbasc_opt(char *str)
{
	if (*str < '0' || *str > ASC_MAX_PORTS + '0')
		goto errout;
	kgdbasc_portno = *str - '0';
	str++;
	if (*str != ',')
		goto errout;
	str++;
	kgdbasc_baud = simple_strtoul(str, &str, 10);
	if (kgdbasc_baud != 9600 && kgdbasc_baud != 19200 &&
		kgdbasc_baud != 38400 && kgdbasc_baud != 57600 &&
		kgdbasc_baud != 115200)
			goto errout;

	return 0;

errout:
	printk(KERN_ERR "Invalid syntax for option kgdbasc=\n");
	return 1;
}
early_param("kgdbasc", kgdbasc_opt);
#endif /* CONFIG_KGDB_ST_ASC */
