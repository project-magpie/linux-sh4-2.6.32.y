/*
 * LIRC plugin for the STMicroelectronics IRDA devices
 *
 * Copyright (C) 2004-2005 STMicroelectronics
 *
 * June 2004:  first implementation for a 2.4 Linux kernel
 *             Giuseppe Cavallaro  <peppe.cavallaro@st.com>
 * Marc 2005:  review to support pure raw mode and to adapt to Linux 2.6
 *             Giuseppe Cavallaro  <peppe.cavallaro@st.com>
 * June 2005:  Change to a MODE2 receive driver and made into a generic
 *             ST driver.
 *             Carl Shaw <carl.shaw@st.com>
 * July 2005:  fix STB7100 MODE2 implementation and improve performance
 *             of STm8000 version. <carl.shaw@st.com>
 * Aug  2005:  Added clock autoconfiguration support.  Fixed module exit code.
 * 	       Added UHF support (kernel build only).
 * 	       Carl Shaw <carl.shaw@st.com>
 * Sep  2005:  Added first transmit support
 *             Added ability to set rxpolarity register
 * 	       Angelo Castello <angelo.castello@st.com>
 * 	       and Carl Shaw <carl.shaw@st.com>
 * Oct  2005:  Added 7100 transmit
 *             Added carrier width configuration
 * 	       Carl Shaw <carl.shaw@st.com>
 * Sept 2006:  Update:
 * 		fix timing issues (bugzilla 764)
 * 		Thomas Betker <thomas.betker@siemens.com>
 * 		allocate PIO pins in driver
 * 		update transmit
 * 		improve fault handling on init
 * 		Carl Shaw <carl.shaw@st.com>
 * Oct  2007:  Added both lirc-0.8.2 common interface and integrated out IRB driver  
 *             to be working for linux-2.6.23-rc7. Removed old platform support...
 *             Sti5528 STb8000. Added new IR rx intq mechanism to reduce the amount 
 *             intq needed to identify one button. Fix TX transmission loop setting up 
 *             correctly the irq clean register.
 * 	       Angelo Castello <angelo.castello@st.com>
 *
 *
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <asm/io.h>
#include <asm/irq.h>
#include <asm/clock.h>
#include <linux/ioport.h>
#include <linux/interrupt.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/time.h>
#include <linux/lirc.h>
#include "lirc_dev.h"

/* General debugging */
#undef LIRC_STM_DEBUG

/* TX wait queue */
static DECLARE_WAIT_QUEUE_HEAD(tx_waitq);

#ifdef  LIRC_STM_DEBUG
#define DPRINTK(fmt, args...) printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#define DPRINTK(fmt, args...)
#endif

/*
 * Infra Red: hardware register map
 */
#if defined(CONFIG_IRB_RECEIVER) || defined(MODULE)
static int ir_uhf_switch = 0;
#else
static int ir_uhf_switch = 1;
#endif

static int ir_or_uhf_offset = 0;	/* = 0 for IR mode */
static int irb_irq = 0;		/* IR block irq */
static void *irb_base_address;	/* IR block register base address */

/* RX timing fine control */
static int rx_symbol_mult;
static int rx_symbol_div;
static int rx_pulse_mult;
static int rx_pulse_div;

/* TX timing fine control */
static unsigned int tx_mult;
static unsigned int tx_div;
static unsigned int tx_carrier_freq = 38000;	// in Hz


/* IR transmitter registers */
#define IRB_TX_PRESCALAR		(irb_base_address + 0x00)	/* clock prescalar */
#define IRB_TX_SUBCARRIER		(irb_base_address + 0x04)	/* subcarrier frequency */
#define IRB_TX_SYMPERIOD		(irb_base_address + 0x08)	/* symbol period (space + pulse) */
#define IRB_TX_ONTIME			(irb_base_address + 0x0c)	/* symbol pulse time */
#define IRB_TX_INT_ENABLE		(irb_base_address + 0x10)	/* TX irq enable */
#define IRB_TX_INT_STATUS		(irb_base_address + 0x14)	/* TX irq status */
#define IRB_TX_ENABLE			(irb_base_address + 0x18)	/* TX enable */
#define IRB_TX_INT_CLEAR		(irb_base_address + 0x1c)	/* TX interrupt clear */
#define IRB_TX_SUBCARRIER_WIDTH		(irb_base_address + 0x20)	/* subcarrier frequency width */
#define IRB_TX_STATUS			(irb_base_address + 0x24)	/* TX status */

#define TX_INT_PENDING			0x01
#define TX_INT_UNDERRUN			0x02

#define TX_FIFO_DEPTH			7
#define TX_FIFO_USED			((readl(IRB_TX_STATUS) >> 8) & 0x07)

/* IR receiver registers */
#define IRB_RX_ON	    		(irb_base_address + 0x40 + ir_or_uhf_offset)	/* RX pulse time capture */
#define IRB_RX_SYS          		(irb_base_address + 0x44 + ir_or_uhf_offset)	/* RX sym period capture */
#define IRB_RX_INT_EN	    		(irb_base_address + 0x48 + ir_or_uhf_offset)	/* RX IRQ enable (R/W)   */
#define IRB_RX_INT_STATUS      		(irb_base_address + 0x4C + ir_or_uhf_offset)	/* RX IRQ status (R/W)   */
#define IRB_RX_EN	    		(irb_base_address + 0x50 + ir_or_uhf_offset)	/* Receive enable (R/W)  */
#define IRB_MAX_SYM_PERIOD  		(irb_base_address + 0x54 + ir_or_uhf_offset)	/* end of sym. max value */
#define IRB_RX_INT_CLEAR 		(irb_base_address + 0x58 + ir_or_uhf_offset)	/* overrun status (W)    */
#define IRB_RX_STATUS	    		(irb_base_address + 0x6C + ir_or_uhf_offset)	/* receive status        */
#define IRB_RX_NOISE_SUPPR  		(irb_base_address + 0x5C + ir_or_uhf_offset)	/* noise suppression     */
#define IRB_RX_POLARITY_INV 		(irb_base_address + 0x68 + ir_or_uhf_offset)	/* polarity inverter     */

/* IRB and UHF common registers */
#define IRB_RX_RATE_COMMON   		(irb_base_address + 0x64)	/* sampling frequency divisor */
#define IRB_RX_CLOCK_SELECT  		(irb_base_address + 0x70)	/* clock selection (for low-power mode) */
#define IRB_RX_CLOCK_SELECT_STATUS 	(irb_base_address + 0x74)	/* clock selection status */
#define IRB_RX_NOISE_SUPP_WIDTH 	(irb_base_address + 0x9C)

#define LIRC_STM_NAME	"lirc_stm"
#define LIRC_STM_MINOR		0

/* SOC dependent section - these values are set in the appropriate 
 * arch/sh/kernel/cpu/sh4/setup-* files and
 * transfered when the lirc device is opened
 */

static unsigned int rx_fifo_has_data = 0;
static unsigned int rx_clear_overrun = 0;
static unsigned int rx_overrun_err = 0;
static unsigned int rx_sampling_freq_div = 0;
static unsigned int rx_enable_irq = 0;
#define RX_CLEAR_IRQ(x) writel((x), IRB_RX_INT_CLEAR)
#define HOW_MANY_WORDS_IN_FIFO() (readl(IRB_RX_STATUS) & 0x0700 )

/* Definition of a single RC symbol */
typedef struct symbol_s {
	unsigned int PulseUs;
	unsigned int SpaceUs;
} symbol_t;

/* InfraRed receive control structure */
#define MAX_SYMBOLS	100
struct st_plugin_data_t {
	int open_count;		/* INC at any open                      */
	int error;		/* true if receive error.. skip symbols */
	int symbols;		/* how many symbols in buf..            */
	symbol_t buf[MAX_SYMBOLS];
	struct timeval sync;	/* start of sync space */
	unsigned int sumUs;	/* sum of symbols */
} pd;

/* IR transmit buffer */
static lirc_t wbuf[MAX_SYMBOLS];
static volatile int off_wbuf = 0;

/* LIRC subsytem symbol buffer */
struct lirc_buffer stlirc_buffer;	/* managed only via common lirc routines */
/* user process read symbols from here  */

static inline void reset_irq_data(struct st_plugin_data_t *pd)
{
	pd->error = 0;
	pd->symbols = 0;
	memset((unsigned char *)pd->buf, 0, sizeof(pd->buf));
	pd->sumUs = 0;
}

#ifdef LIRC_STM_DEBUG
/* For debug only: in memory structure to trace interrupt
 * status registers and symbols timing received.
 */
#define MAX_IRD		200
typedef struct ird_t {
	unsigned int a_intsta;
	unsigned int b_intsta;
	unsigned int c_intsta;
	unsigned int a_status;
	unsigned int b_status;
	unsigned int sym;
	unsigned int mark;
	unsigned int sr;
} ird_t;
static int ird_valid = -1;
static ird_t ird[MAX_IRD];

static void trace_prt(void)
{
	int i;

	if (ird_valid == -1)
		return;

	DPRINTK
	    ("  #   IRQst  stats      S      M   IRQst  stats  IRQst   s.r\n");
	for (i = 0; i < ird_valid; i++)
		DPRINTK
		    ("%3d    %04x  %04x   %5d  %5d    %04x   %04x   %04x   %3d\n",
		     i, ird[i].a_intsta, ird[i].a_status, ird[i].sym,
		     ird[i].mark, ird[i].b_intsta, ird[i].b_status,
		     ird[i].c_intsta, ird[i].sr);
	memset(ird, 0, sizeof(ird));
	ird_valid = -1;
}

#define TRACEA(a,b)   { if (ird_valid >= MAX_IRD) \
				ird_valid = -1; \
			ird_valid++; \
			ird[ird_valid].a_intsta = (a); \
			ird[ird_valid].a_status = (b); }
#define TRACES(a,b,c) { if (ird_valid >= 0 && ird_valid <= MAX_IRD) \
			{\
				ird[ird_valid].sym  = (a); \
				ird[ird_valid].mark = (b); \
				ird[ird_valid].sr   = (c); \
			}\
		      }
#define TRACEC(a,b)   { if (ird_valid >= 0 && ird_valid <= MAX_IRD) \
	                {\
				ird[ird_valid].b_intsta = (a); \
				ird[ird_valid].b_status = (b); \
			}\
		      }
#define TRACEE(a)     { if (ird_valid >= 0 && ird_valid <= MAX_IRD) \
				ird[ird_valid].c_intsta = (a); }
#define TRACE_PRT()     trace_prt();
#else
#define TRACEA(a,b)
#define TRACES(a,b,c)
#define TRACEC(a,b)
#define TRACEE(a)
#define TRACE_PRT()

#endif

static inline unsigned int lirc_stm_time_to_cycles(unsigned int microsecondtime)
{
	/* convert a microsecond time to the nearest number of subcarrier clock
	 * cycles
	 */
	microsecondtime *= tx_mult;
	microsecondtime /= tx_div;
	return (microsecondtime * tx_carrier_freq / 1000000);
}

static void lirc_stm_tx_interrupt(int irq, void *dev_id)
{
	unsigned int symbol, mark, done = 0;
	unsigned int tx_irq_status = readl(IRB_TX_INT_STATUS);
        
	if ((tx_irq_status & TX_INT_PENDING) != TX_INT_PENDING) 
            return;

        while (done == 0) {
            if ((readl(IRB_TX_INT_STATUS) & TX_INT_UNDERRUN) ==
                TX_INT_UNDERRUN) {
                /* There has been an underrun - clear flag, switch
                 * off transmitter and signal possible exit
                 */
                printk(KERN_ERR "lirc_stm: transmit underrun!\n");
                writel(0x02, IRB_TX_INT_CLEAR);
                writel(0x00, IRB_TX_INT_ENABLE);
                writel(0x00, IRB_TX_ENABLE);
                done = 1;
                DPRINTK("disabled TX\n");
                wake_up_interruptible(&tx_waitq);
            } else {
                int fifoslots = TX_FIFO_USED;

                while (fifoslots < TX_FIFO_DEPTH) {
                    mark = wbuf[(off_wbuf * 2)];
                    symbol = mark + wbuf[(off_wbuf * 2) + 1];
                    DPRINTK("TX raw m %d s %d ", mark, symbol);

                    mark =lirc_stm_time_to_cycles(mark) + 1;
                    symbol =lirc_stm_time_to_cycles(symbol) + 2;
                    DPRINTK("cal m %d s %d\n", mark, symbol);

                    if ((wbuf[(off_wbuf * 2)] == 0xFFFF) || 
                        (wbuf[(off_wbuf * 2) + 1] == 0xFFFF)) 
                    {
                        /* Dump out last symbol */
                        writel(mark * 2, IRB_TX_SYMPERIOD);
                        writel(mark, IRB_TX_ONTIME);

                        DPRINTK("TX end m %d s %d\n", mark, mark * 2);

                        /* flush transmit fifo */
                        while (TX_FIFO_USED != 0) {
                        };
                        writel(0, IRB_TX_SYMPERIOD);
                        writel(0, IRB_TX_ONTIME);
                        /* spin until TX fifo empty */
                        while (TX_FIFO_USED != 0) {
                        };
                        /* disable tx interrupts and transmitter */
                        writel(0x07, IRB_TX_INT_CLEAR);
                        writel(0x00, IRB_TX_INT_ENABLE);
                        writel(0x00, IRB_TX_ENABLE);
                        DPRINTK("TX disabled\n");
                        off_wbuf = 0;
                        fifoslots = 999;
                        done = 1;
                    } else {
                        writel(symbol,IRB_TX_SYMPERIOD);
                        writel(mark, IRB_TX_ONTIME);

                        DPRINTK("Nm %d s %d\n", mark, symbol);

                        off_wbuf++;
                        fifoslots = TX_FIFO_USED;
                    }
                }
            }
        }
}

static void lirc_stm_rx_interrupt(int irq, void *dev_id)
{
	struct st_plugin_data_t *pd = (struct st_plugin_data_t *)dev_id;
	unsigned int symbol, mark = 0;
	int lastSymbol, clear_irq = 1;

	for (;;) {
		/* if received FIFO is empty exit from loop */
		/* also deal with fifo underrun interrupt */
		if (HOW_MANY_WORDS_IN_FIFO() == 0) {
			RX_CLEAR_IRQ(rx_fifo_has_data | 0x02);
                        writel(rx_enable_irq, IRB_RX_INT_EN);
                        clear_irq = 1;
			break;
		} else {
			unsigned int rx_irq_status = readl(IRB_RX_INT_STATUS);

			/* discard the entire collection in case of errors!  */
			if (rx_irq_status & rx_overrun_err) {
				printk(KERN_INFO "IR overrun\n");
				writel(rx_clear_overrun, IRB_RX_INT_CLEAR);
				pd->error = 1;
			}

			TRACEA(readl(IRB_RX_INT_STATUS), readl(IRB_RX_STATUS));

			/* get the symbol times from FIFO */
			symbol = (readl(IRB_RX_SYS));
			mark = (readl(IRB_RX_ON));

                        if (clear_irq) {
                            /*  Clear the interrupt (not required for some boards) 
                             * and take only the underrun irq enabled */
                            RX_CLEAR_IRQ(rx_fifo_has_data);
                            writel(0x04, IRB_RX_INT_EN);
                            clear_irq = 0;
                        }

			if (pd->symbols >= MAX_SYMBOLS) {
				printk("IR too many symbols (max %d)\n",
				       MAX_SYMBOLS);
				pd->error = 1;
			}

			/* now handle the data depending on error condition */
			if (pd->error) {
				/*  Try again */
				reset_irq_data(pd);
				continue;
			}
                        if (symbol == 0xFFFF)
                             lastSymbol = 1;
                        else lastSymbol = 0;

			/* A sequence seems to start with a constant time symbol (1us)
			 * pulse and symbol time length, both of 1us. We ignore this.
			 */
			if ((mark > 2) && (symbol > 1)) {
				TRACES(symbol, mark, pd->symbols);

				/* Make fine adjustments to timings */
				symbol -= mark;	/* to get space timing */
				symbol *= rx_symbol_mult;
				symbol /= rx_symbol_div;
				mark *= rx_pulse_mult;
				mark /= rx_pulse_div;

				/* The ST hardware returns the pulse time and the period, which is
				 * the pulse time + space time, so we need to subtract the pulse time from
				 * the period to get the space time.
				 * For a pulse in LIRC MODE2, we need to set the PULSE_BIT ON
				 */
				pd->buf[pd->symbols].PulseUs = mark | PULSE_BIT;
				pd->buf[pd->symbols].SpaceUs = symbol;
				pd->sumUs += mark + symbol;
				pd->symbols++;

				if (lastSymbol) {
                                    /* move the entire collection into user buffer if enough
                                     * space, drop it otherwise (perhaps too crude a recovery?)
                                     */
                                    if (lirc_buffer_available(&stlirc_buffer) >=
                                        (2 * pd->symbols)) {
                                        struct timeval now;
                                        lirc_t syncSpace;

                                        DPRINTK("W symbols = %d\n", pd->symbols);

                                        /*  Calculate and write the leading space
                                         *  All spaces and pulses together sum up to the microseconds
                                         *  elapsed since we sent the previous block of data
                                         */
                                        do_gettimeofday(&now);
                                        if (now.tv_sec - pd->sync.tv_sec < 0)
                                            syncSpace = 0;
                                        else if (now.tv_sec - pd->sync.tv_sec
							 > PULSE_MASK / 1000000)
                                            syncSpace = PULSE_MASK;
                                        else {
                                            syncSpace = (now.tv_sec - pd->sync.tv_sec) * 
                                                1000000	+ (now.tv_usec -pd->sync.tv_usec);
                                            syncSpace-= (pd->sumUs - pd->buf[pd->symbols - 1].SpaceUs);
                                            if (syncSpace < 0)
                                                syncSpace = 0;
                                            else if (syncSpace > PULSE_MASK)
                                                syncSpace = PULSE_MASK;
                                        }
                                        lirc_buffer_write_1(&stlirc_buffer,
                                                            (unsigned char *)&syncSpace);
                                        pd->sync = now;

                                        /*  Now write the pulse / space pairs EXCEPT FOR THE LAST SPACE
                                         *  The last space value should be 0xFFFF to denote a timeout
                                         */
                                        lirc_buffer_write_n(&stlirc_buffer,
                                                            (unsigned char *)pd->buf,
                                                            (2 * pd->symbols) - 1);
						wake_up(&stlirc_buffer.wait_poll);
					} else
						printk(KERN_ERR
						       "Not enough space in user buffer\n");

					TRACE_PRT();
					reset_irq_data(pd);
				}
			}
			TRACEC(readl(IRB_RX_INT_STATUS), readl(IRB_RX_STATUS));
		} /* receive handler */
	}

	TRACEE(readl(IRB_RX_INT_STATUS));
}

static irqreturn_t lirc_stm_interrupt(int irq, void *dev_id)
{
	lirc_stm_tx_interrupt(irq, dev_id);

	lirc_stm_rx_interrupt(irq, dev_id);

	return IRQ_HANDLED;
}

static int stm_set_use_inc(void *data)
{
	struct st_plugin_data_t *pd = (struct st_plugin_data_t *)data;

	DPRINTK("entering (open N. %d)\n", pd->open_count);

	/* enable the device only at the first open */
	if (pd->open_count++ == 0) {
		unsigned long flags;
		DPRINTK("Enabled\n");
		local_irq_save(flags);

		/* enable interrupts and receiver */
		writel(rx_enable_irq, IRB_RX_INT_EN);
		writel(0x01, IRB_RX_EN);
		reset_irq_data(pd);
		pd->sync.tv_sec = 0;
		pd->sync.tv_usec = 0;
		local_irq_restore(flags);
	} else
		DPRINTK("Already open\n");

	return 0;
}

static void flush_stm_lirc(struct st_plugin_data_t *pd)
{
        /* Disable receiver */ 
	writel(0x00, IRB_RX_EN);
        /* TBD: set one word in FIFO ??? and disable interrupt */
        writel(0x20, IRB_RX_INT_EN);
        /* clean the buffer */
	reset_irq_data(pd);
}

/*
** Called by lirc_dev as a last action on a real close
*/
static void stm_set_use_dec(void *data)
{
	struct st_plugin_data_t *pd = (struct st_plugin_data_t *)data;
	DPRINTK("entering (close N. %d)\n", pd->open_count);

	/* The last close disable the receiver */
	if (--pd->open_count == 0)
		flush_stm_lirc(pd);
	TRACE_PRT();
}

static int lirc_stm_ioctl(struct inode *node, struct file *filep,
			  unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	unsigned long value = 0;
	char *msg = "";

	switch (cmd) {
	case LIRC_GET_FEATURES:
		/*
		 * Our driver can receive in mode2 and send in pulse mode.
		 * TODO: We can generate our own carrier freq (LIRC_CAN_SET_SEND_CARRIER)
		 *       and also change duty cycle (LIRC_CAN_SET_SEND_DUTY_CYCLE)
		 */
		DPRINTK("LIRC_GET_FEATURES return REC_MODE2|SEND_PULSE\n");
		retval = put_user(LIRC_CAN_REC_MODE2 |
				  LIRC_CAN_SEND_PULSE, (unsigned long *)arg);
		break;

	case LIRC_GET_REC_MODE:
		DPRINTK("LIRC_GET_REC_MODE return LIRC_MODE_MODE2\n");
		retval = put_user(LIRC_MODE_MODE2, (unsigned long *)arg);
		break;

	case LIRC_SET_REC_MODE:
		retval = get_user(value, (unsigned long *)arg);
		DPRINTK("LIRC_SET_REC_MODE to 0x%lx\n", value);
		if (value != LIRC_MODE_MODE2)
			retval = -ENOSYS;
		break;

	case LIRC_GET_SEND_MODE:
		DPRINTK("LIRC_GET_SEND_MODE return LIRC_MODE_PULSE\n");
		retval = put_user(LIRC_MODE_PULSE, (unsigned long *)arg);
		break;

	case LIRC_SET_SEND_MODE:
		retval = get_user(value, (unsigned long *)arg);
		DPRINTK("LIRC_SET_SEND_MODE to 0x%lx\n", value);
		/* only LIRC_MODE_PULSE supported */
		if (value != LIRC_MODE_PULSE)
			return (-ENOSYS);
		break;

	case LIRC_GET_REC_RESOLUTION:
		msg = "LIRC_GET_REC_RESOLUTION";
		goto _not_supported;

	case LIRC_GET_REC_CARRIER:
		msg = "LIRC_GET_REC_CARRIER";
		goto _not_supported;

	case LIRC_SET_REC_CARRIER:
		msg = "LIRC_SET_REC_CARRIER";
		goto _not_supported;

	case LIRC_GET_SEND_CARRIER:
		msg = "LIRC_GET_SEND_CARRIER";
		goto _not_supported;

	case LIRC_SET_SEND_CARRIER:
		msg = "LIRC_SET_SEND_CARRIER";
		goto _not_supported;

	case LIRC_GET_REC_DUTY_CYCLE:
		msg = "LIRC_GET_REC_DUTY_CYCLE";
		goto _not_supported;

	case LIRC_SET_REC_DUTY_CYCLE:
		msg = "LIRC_SET_REC_DUTY_CYCLE";
		goto _not_supported;

	case LIRC_GET_SEND_DUTY_CYCLE:
		msg = "LIRC_GET_SEND_DUTY_CYCLE";
		goto _not_supported;

	case LIRC_SET_SEND_DUTY_CYCLE:
		msg = "LIRC_SET_SEND_DUTY_CYCLE";
		goto _not_supported;

	case LIRC_GET_LENGTH:
		msg = "LIRC_GET_LENGTH";
		goto _not_supported;

	default:
		msg = "???";
	      _not_supported:
		DPRINTK("command %s (0x%x) not supported\n", msg, cmd);
		retval = -ENOIOCTLCMD;
	}

	return retval;
}

static ssize_t lirc_stm_write(struct file *file, const char *buf,
			      size_t n, loff_t * ppos)
{
	int i;
	size_t rdn = n / sizeof(size_t);
	unsigned int symbol, mark;
	int fifosyms;

	if (n % sizeof(lirc_t))
		return -EINVAL;

	if (off_wbuf != 0 && (file->f_flags & O_NONBLOCK))
		return -EAGAIN;

	/* Wait for transmit to become free... */
	if (wait_event_interruptible(tx_waitq, off_wbuf == 0))
		return -ERESTARTSYS;

	/* Prevent against buffer overflow... */
	if (rdn > MAX_SYMBOLS) rdn = MAX_SYMBOLS;

	n -= rdn * sizeof(size_t);

	if (copy_from_user((char *)wbuf, buf, rdn * sizeof(size_t))) {
		return -EFAULT;
	}

	if (n == 0) wbuf[rdn - 1] = 0xFFFF;

	/* load the first words into the FIFO */
	fifosyms = rdn;

	if (fifosyms > TX_FIFO_DEPTH)
		fifosyms = TX_FIFO_DEPTH;

	for (i = 0; i < fifosyms; i++) {
            mark = wbuf[(i * 2)];
            symbol = mark + wbuf[(i * 2) + 1];
            DPRINTK("TX raw m %d s %d ", mark, symbol);

            mark = lirc_stm_time_to_cycles(mark) + 1;
            symbol = lirc_stm_time_to_cycles(symbol) + 2;
            DPRINTK("cal m %d s %d\n", mark, symbol);

            off_wbuf++;
            writel(mark, IRB_TX_ONTIME);
            writel(symbol, IRB_TX_SYMPERIOD);
	}

	/* enable the transmit */
	writel(0x07, IRB_TX_INT_ENABLE);
	writel(0x01, IRB_TX_ENABLE);
        DPRINTK("TX enabled\n");

	return n;
}

static void lirc_stm_calc_tx_clocks(unsigned int clockfreq,
				    unsigned int carrierfreq,
				    unsigned int subwidthpercent)
{
	/*  We know the system base clock and the required IR carrier frequency
	 *  We now want a divisor of the system base clock that gives the nearest
	 *  integer multiple of the carrier frequency
	 */

	const unsigned int clkratio = clockfreq / carrierfreq;
	unsigned int scalar, n;
	int delta;
	unsigned int diffbest = clockfreq, nbest = 0, scalarbest = 0;
	unsigned int nmin = clkratio / 255;

	if ((nmin & 0x01) == 1)
		nmin++;

	for (n = nmin; n < clkratio; n += 2) {
		scalar = clkratio / n;
		if ((scalar & 0x01) == 0 && scalar != 0) {
			delta = clockfreq - (scalar * carrierfreq * n);
			if (delta < 0)
				delta *= -1;

			if (delta < diffbest) {	/* better set of parameters ? */
				diffbest = delta;
				nbest = n;
				scalarbest = scalar;
			}
			if (delta == 0)	/* an exact multiple */
				break;
		}
	}

	scalarbest /= 2;
	nbest *= 2;

	DPRINTK("TX clock scalar = %d\n", scalarbest);
	DPRINTK("TX subcarrier scalar = %d\n", nbest);

	/*  Set the registers now  */

	writel(scalarbest, IRB_TX_PRESCALAR);
	writel(nbest, IRB_TX_SUBCARRIER);
	writel(nbest * subwidthpercent / 100, IRB_TX_SUBCARRIER_WIDTH);

	/*  Now calculate timing to subcarrier cycles factors which compensate for
	 *  any remaining difference between our clock ratios and real times in
	 *  microseconds
	 */

	if (diffbest == 0) {
		/* no adjustment required - our clock is running at the required speed */
		tx_mult = 1;
		tx_div = 1;
	} else {
		/* adjustment is required */
		delta = scalarbest * carrierfreq * nbest;
		tx_mult = delta / (clockfreq / 10000);

		if (delta < clockfreq) {	/* our clock is running too fast */
			DPRINTK("Clock running slow at %d\n", delta);
			tx_div = tx_mult;
			tx_mult = 10000;
		} else {	/* our clock is running too slow */

			DPRINTK("Clock running fast at %d\n", delta);
			tx_div = 10000;
		}
	}

	DPRINTK("TX fine adjustment mult = %d\n", tx_mult);
	DPRINTK("TX fine adjustment div  = %d\n", tx_div);
}

static struct file_operations lirc_stm_fops = {
      write:lirc_stm_write,
};

static struct lirc_plugin lirc_stm_plugin = {
	.name  = LIRC_STM_NAME,
	.minor = LIRC_STM_MINOR,
	.code_length = 1,
	.sample_rate = 0,
	/* plugin can receive raw pulse and space timings for each symbol */
	.features = LIRC_CAN_REC_MODE2,
	/* plugin private data  */
	.data = (void *)&pd,
	/* buffer handled by upper layer */
	.add_to_buf = NULL,
	.get_queue = NULL,
	.set_use_inc = stm_set_use_inc,
	.set_use_dec = stm_set_use_dec,
	.ioctl = lirc_stm_ioctl,
	.fops = &lirc_stm_fops,
	.rbuf = &stlirc_buffer,
	.owner = THIS_MODULE,
};

static int __init lirc_stm_init(void)
{
	int ret = -EINVAL;
	struct platform_device *lirc_plat_dev = NULL;
	struct plat_lirc_data *lirc_private_data = NULL;
	struct resource *res;
	int baseclock;
	int piopins;
	unsigned int scwidth;
	struct clk *clk;
	struct lirc_pio *p;

	DPRINTK("initializing the IR receiver...\n");

	/* inform the top level driver that we use our own user buffer */
	if (lirc_buffer_init(&stlirc_buffer, sizeof(lirc_t), (2 * MAX_SYMBOLS))) {
		printk(KERN_ERR
		       "lirc_stm: lirc_stm_init: buffer init failed\n");
		goto lirc_out4;
	}

	request_module("lirc_dev");
	if ((ret = lirc_register_plugin(&lirc_stm_plugin)) < 0) {
		printk(KERN_ERR
		       "lirc_stm: lirc_stm_init: plug-in registration failed\n");
		goto lirc_out4;
	}

	/*  At this point, we need to get a pointer to the platform-specific data */
	if ((lirc_plat_dev = (struct platform_device *)lirc_get_config()) == NULL) {
		printk(KERN_ERR
		       "lirc_stm: lirc_stm_init: platform data not found\n");
		goto lirc_out4;
	}

	/* Request the IRQ */
	if ((irb_irq = platform_get_irq(lirc_plat_dev, 0)) == 0) {
		printk(KERN_ERR
		       "lirc_stm: lirc_stm_init: IRQ configuration not found\n");
		ret = -ENODEV;
		goto lirc_out4;
	}

	if ((ret = request_irq(irb_irq, lirc_stm_interrupt, IRQF_SHARED,
			       LIRC_STM_NAME, (void *)&pd)) < 0) {
		printk(KERN_ERR
		       "lirc_stm: lirc_stm_init: IRQ register failed\n");
		ret = -EIO;
		goto lirc_out4;
	}

	/* Configure for ir or uhf. ir_uhf_switch==1 is IRB */
	if (ir_uhf_switch) 
             ir_or_uhf_offset = 0x40;
	else ir_or_uhf_offset = 0x00;

	printk(KERN_INFO "lirc_stm: STM LIRC plugin has IRQ %d using %s mode\n", 
               irb_irq, (ir_or_uhf_offset == 0 ? "IRB" : "UHF"));

	/* Hardware IR block setup - the PIO ports should already be set up
	 * in the board-dependent configuration.  We need to remap the
	 * IR registers into kernel space - we do this in one chunk
	 */

	res = platform_get_resource(lirc_plat_dev, IORESOURCE_MEM, 0);
	if (!res) {
		printk(KERN_ERR "lirc_stm: lirc_stm_init: IO MEM not found\n");
		ret = -ENODEV;
		goto lirc_out3;
	}

	if (!request_mem_region(res->start, 
                                res->end - res->start, 
                                LIRC_STM_NAME)) {
		printk(KERN_ERR
		       "lirc_stm: lirc_stm_init: request_mem_region failed\n");
		ret = -EBUSY;
		goto lirc_out3;
	}

	irb_base_address = ioremap(res->start, res->end - res->start);
	if (irb_base_address == NULL) {
		printk(KERN_ERR "lirc_stm: lirc_stm_init: ioremap failed\n");
		release_mem_region(res->start, res->end - res->start);
		ret = -ENOMEM;
		goto lirc_out2;
	}

	DPRINTK(KERN_INFO "ioremapped register block at 0x%lx\n", res->start);
	DPRINTK(KERN_INFO "ioremapped to 0x%x\n",
		(unsigned int)irb_base_address);

	/*  set up the hardware version dependent setup parameters */
	lirc_private_data = (struct plat_lirc_data *)lirc_plat_dev->dev.platform_data;

	/* Allocate the PIO pins */
	piopins = lirc_private_data->num_pio_pins;
	while (piopins > 0) {
		p = &(lirc_private_data->pio_pin_arr[piopins - 1]);
		if (!(p->pinaddr = stpio_request_pin(p->bank, 
                                                     p->pin,
						     LIRC_STM_NAME, 
                                                     p->dir))) {
			printk(KERN_ERR
			       "lirc_stm: lirc_stm_init: STPIO[%d,%d] request failed\n",
			       p->bank, p->pin);
			ret = -EBUSY;
			goto lirc_out1;
		}
		piopins--;
	}

	/* Set the polarity inversion bit to the correct state */
	writel(lirc_private_data->rxpolarity, IRB_RX_POLARITY_INV);

	rx_overrun_err   = 0x04;
        rx_fifo_has_data = 0x38;
        rx_clear_overrun = 0x04;
        /* IRQ set: Enable full FIFO                 1  -> bit  3;
         *          Enable overrun IRQ               1  -> bit  2;
         *          Enable last symbol IRQ           1  -> bit  1:
         *          Enable RX interrupt              1  -> bit  0;
         */
        rx_enable_irq = 0x0f;

	/*  Get or calculate the clock and timing adjustment values.
	 *  We can auto-calculate these in some cases
	 */

	if (lirc_private_data->irbclock == 0) {
		clk = clk_get(NULL, "comms_clk");
		baseclock = clk_get_rate(clk) / lirc_private_data->sysclkdiv;
	} else
		baseclock = lirc_private_data->irbclock;

	if (lirc_private_data->irbclkdiv == 0) {
		/* Auto-calculate clock divisor */

		int freqdiff;

		rx_sampling_freq_div = baseclock / 10000000;

		/* Work out the timing adjustment factors */
		freqdiff = baseclock - (rx_sampling_freq_div * 10000000);

		/* freqdiff contains the difference between our clock and a
		 * true 10 MHz clock which the IR block wants
		 */

		if (freqdiff == 0) {
			/* no adjustment required - our clock is running at the required speed */
			rx_symbol_mult = 1;
			rx_pulse_mult = 1;
			rx_symbol_div = 1;
			rx_pulse_div = 1;
		} else {
			/* adjustment is required */
			rx_symbol_mult =
			    baseclock / (10000 * rx_sampling_freq_div);

			if (freqdiff > 0) {
				/* our clock is running too fast */
				rx_pulse_mult = 1000;
				rx_pulse_div = rx_symbol_mult;
				rx_symbol_mult = rx_pulse_mult;
				rx_symbol_div = rx_pulse_div;
			} else {
				/* our clock is running too slow */
				rx_symbol_div = 1000;
				rx_pulse_mult = rx_symbol_mult;
				rx_pulse_div = 1000;
			}

		}

	} else {
		rx_sampling_freq_div = (lirc_private_data->irbclkdiv);
		rx_symbol_mult = (lirc_private_data->irbperiodmult);
		rx_symbol_div = (lirc_private_data->irbperioddiv);
		rx_pulse_mult = (lirc_private_data->irbontimemult);
		rx_pulse_div = (lirc_private_data->irbontimediv);
	}

	writel(rx_sampling_freq_div, IRB_RX_RATE_COMMON);
	DPRINTK(KERN_INFO "IRB clock is %d\n", baseclock);
	DPRINTK(KERN_INFO "IRB clock divisor is %d\n", rx_sampling_freq_div);
	DPRINTK(KERN_INFO "IRB clock divisor readlack is %d\n",
		readl(IRB_RX_RATE_COMMON));
	DPRINTK(KERN_INFO "IRB period mult factor is %d\n", rx_symbol_mult);
	DPRINTK(KERN_INFO "IRB period divisor factor is %d\n", rx_symbol_div);
	DPRINTK(KERN_INFO "IRB pulse mult factor is %d\n", rx_pulse_mult);
	DPRINTK(KERN_INFO "IRB pulse divisor factor is %d\n", rx_pulse_div);

        {
            /* maximum symbol period.  
             * Symbol periods longer than this will generate
             * an interrupt and terminate a command
             */
            unsigned int rx_max_symbol_per;
            if ((lirc_private_data->irbrxmaxperiod) != 0)
		rx_max_symbol_per =
		    (lirc_private_data->irbrxmaxperiod) * rx_symbol_mult /
		    rx_symbol_div;
            else
		rx_max_symbol_per = 0;

            DPRINTK(KERN_INFO "IRB RX Maximum symbol period register 0x%x\n",
                    rx_max_symbol_per);
            writel(rx_max_symbol_per, IRB_MAX_SYM_PERIOD);
        }
        
	/*  Set up the transmit timings  */
	if (lirc_private_data->subcarrwidth != 0)
		scwidth = lirc_private_data->subcarrwidth;
	else
		scwidth = 50;

	if (scwidth > 100)
		scwidth = 50;

	DPRINTK(KERN_INFO "Subcarrier width set to %d %%\n", scwidth);
	lirc_stm_calc_tx_clocks(baseclock, tx_carrier_freq, scwidth);

	printk(KERN_INFO "STMicroelectronics LIRC driver configured\n");

	return 0;

      lirc_out1:
	while (piopins < lirc_private_data->num_pio_pins)
		stpio_free_pin(lirc_private_data->pio_pin_arr[piopins++].
			       pinaddr);
	iounmap(irb_base_address);
      lirc_out2:
	release_mem_region(res->start, res->end - res->start);
      lirc_out3:
	free_irq(irb_irq, (void *)&pd);
      lirc_out4:
	return ret;
}

void __exit lirc_stm_release(void)
{
	int ret_value, piopins;
	struct resource *res;
	struct platform_device *lirc_plat_dev = NULL;
	struct plat_lirc_data *lirc_private_data = NULL;

	DPRINTK("removing STM lirc plugin\n");

	flush_stm_lirc(&pd);

        /* unplug the lirc stm driver */
	if ((ret_value = lirc_unregister_plugin(LIRC_STM_MINOR)) < 0)
		printk(KERN_ERR "STM InfraRed plug-in unregister failed\n");

	iounmap(irb_base_address);

        /* deallocate the STPIO pins */
	lirc_plat_dev = (struct platform_device *)lirc_get_config();
	lirc_private_data = (struct plat_lirc_data *)lirc_plat_dev->dev.platform_data;
	piopins = lirc_private_data->num_pio_pins;
	while (piopins > 0)
		stpio_free_pin(lirc_private_data->pio_pin_arr[--piopins].
			       pinaddr);

        /* release platform resource */
	res = platform_get_resource(lirc_plat_dev, IORESOURCE_MEM, 0);
	release_mem_region(res->start, res->end - res->start);

	free_irq(irb_irq, (void *)&pd);
	printk(KERN_INFO "STMicroelectronics LIRC driver removed\n");
}

module_param(ir_uhf_switch, bool, 0644);
MODULE_PARM_DESC(ir_uhf_switch, "Enable uhf mode");

module_init(lirc_stm_init);
module_exit(lirc_stm_release);
MODULE_DESCRIPTION
    ("Linux InfraRed receiver plugin for STMicroelectronics platforms");
MODULE_AUTHOR("Carl Shaw <carl.shaw@st.com>");
MODULE_LICENSE("GPL");
