/*
 *  drivers/serial/stasc-dma.c
 *  Asynchronous serial controller (ASC) driver - 710x FDMA extension
 */

#include <linux/stm/710x_fdma.h>
#include <linux/timer.h>
#include <linux/stm/stm-dma.h>
#include <asm/cacheflush.h>

#include "stasc.h"

/* Key running performance parameters */
#ifdef CONFIG_STB7100_FDMA
#define DMA_RXBUFSIZE 1024
#define DMA_RXBUFERS 8 /* must be power of 2 */
#define DMA_TXBUFSIZE 2048
#define DMA_TXBUFERS 4 /* must be power of 2 */
#define RXPOLL_PERIOD (50 * HZ /1000)
#endif

struct asc_dma_port
{
	int rxdma_running;
	int txdma_running;
	int rxdma_chid;
	int txdma_chid;
	struct stm_dma_params rxdmap;
	struct stm_dma_params txdmap;
	unsigned char *rxdmabuf[DMA_RXBUFERS];
	int rxdmabuf_count[DMA_RXBUFERS];
	unsigned char *txdmabuf[DMA_TXBUFERS];
	int txdmabuf_count[DMA_TXBUFERS];
	int rxdmabuf_head;
	int rxdmabuf_tail;
	int txdmabuf_head;
	int txdmabuf_tail;
	int last_residue;
	struct timer_list rxpoll_timer;
};

static unsigned long FDMA_RXREQ[ASC_NPORTS];
static unsigned long FDMA_TXREQ[ASC_NPORTS];
static struct asc_dma_port asc_dma_ports[ASC_NPORTS];

#define SYSCONF_BASE		0xb9001000
#define SYSCONF_DEVICEID	(SYSCONF_BASE + 0x000)

void asc_fdma_setreq(void)
{
	u32 devid = ctrl_inl(SYSCONF_DEVICEID);
	u32 cpu_subtype = (((devid >> 12) & 0x3ff) == 0x02c) ? 7109 : 7100;

	if (cpu_subtype == 7100)
	{
		FDMA_RXREQ[0] = STB7100_FDMA_REQ_UART_2_RX;
		FDMA_RXREQ[1] = STB7100_FDMA_REQ_UART_3_RX;
		FDMA_TXREQ[0] = STB7100_FDMA_REQ_UART_2_TX;
		FDMA_TXREQ[1] = STB7100_FDMA_REQ_UART_3_TX;
	}
	else
	{
		FDMA_RXREQ[0] = STB7109_FDMA_REQ_UART_2_RX;
		FDMA_RXREQ[1] = STB7109_FDMA_REQ_UART_3_RX;
		FDMA_TXREQ[0] = STB7109_FDMA_REQ_UART_2_TX;
		FDMA_TXREQ[1] = STB7109_FDMA_REQ_UART_3_TX;
	}
}

static int asc_dma_rxflush_one_buffer(struct asc_port *ascport,
      struct asc_dma_port *ascdmaport,  struct tty_struct *tty, int space)
{
	unsigned char *buffer = ascdmaport->rxdmabuf[ascdmaport->rxdmabuf_tail];
	int count = ascdmaport->rxdmabuf_count[ascdmaport->rxdmabuf_tail];

	/* Copy as much data as possibly to the tty receive buffer */
	tty->ldisc.receive_buf(tty, buffer, NULL, min(count, space));

	/* If we didn't use up all the data in the buffer */
	if (count > space) {
		/* Keep the bit we didn't use */
		int residue = count - space;
		ascport->port.icount.rx += space;
		memmove (buffer, buffer + space, residue);
		ascdmaport->rxdmabuf_count[ascdmaport->rxdmabuf_tail] = residue;
		space = 0;
	} else {
		/* Otherwise move on to the next buffer */
		ascport->port.icount.rx += count;
		space = tty->receive_room;
		ascdmaport->rxdmabuf_count[ascdmaport->rxdmabuf_tail] = 0;
		if (ascdmaport->rxdmabuf_head != ascdmaport->rxdmabuf_tail)
			ascdmaport->rxdmabuf_tail = (ascdmaport->rxdmabuf_tail + 1) & (DMA_RXBUFERS - 1);
	}

	return space;
}

static void asc_dma_rxflush(struct uart_port *port)
{
	struct asc_port *ascport = &asc_ports[port->line];
	struct asc_dma_port *ascdmaport = &asc_dma_ports[port->line];
	struct tty_struct *tty = port->info->tty;
	int space = tty->receive_room;
	int err=0;

	/* Make space to start new DMA on new block, if necessary */
	if (ascdmaport->rxdmabuf_count[ascdmaport->rxdmabuf_head] != 0) {
		if (((ascdmaport->rxdmabuf_head + 1) & (DMA_RXBUFERS - 1)) == ascdmaport->rxdmabuf_tail) {
			space = asc_dma_rxflush_one_buffer(ascport, ascdmaport, tty, space);
			if (((ascdmaport->rxdmabuf_head + 1) & (DMA_RXBUFERS - 1)) == ascdmaport->rxdmabuf_tail) {
/*				printk(KERN_WARNING "ASC RX FDMA overflow on buffer#%d\n", ascdmaport->rxdmabuf_head); */
				return;
			}
		}
		
		/* Use next free block to receive into */
		ascdmaport->rxdmabuf_head = (ascdmaport->rxdmabuf_head + 1) & (DMA_RXBUFERS - 1);
	}
	
	/* Try to set RX DMA going again */
	dma_parms_addrs(&ascdmaport->rxdmap,
			ascdmaport->rxdmap.sar,
			virt_to_bus(ascdmaport->rxdmabuf[ascdmaport->rxdmabuf_head]),
			ascdmaport->rxdmap.node_bytes);

	/* Assuming current num_bytes parm is valid */
	dma_parms_paced(&ascdmaport->rxdmap,
			ascdmaport->rxdmap.node_bytes,
			FDMA_RXREQ[port->line]);

	if((err = dma_compile_list(&ascdmaport->rxdmap)) < 0) {
		printk(KERN_ERR "ASC RX FDMA failed to reconfigure, error %d\n", err);
		return;
	}

	dma_cache_wback_inv(ascdmaport->rxdmabuf[ascdmaport->rxdmabuf_head], DMA_RXBUFSIZE);

	if((err = dma_xfer_list(ascdmaport->rxdma_chid,&ascdmaport->rxdmap)) < 0) {
		printk(KERN_ERR "ASC RX FDMA failed to start, error %d\n", err);
		return;
	}
	ascdmaport->rxdma_running = 1;

	/* Transfer as many buffers as possible to the tty receive buffer */
	while (space > 0 && ascdmaport->rxdmabuf_head != ascdmaport->rxdmabuf_tail) {
		space = asc_dma_rxflush_one_buffer(ascport, ascdmaport, tty, space);
	}
}

static void asc_rxfmda_done(void* param)
{
	struct uart_port *port = (struct uart_port *)param;
	struct asc_dma_port *ascdmaport = &asc_dma_ports[port->line];
	struct tty_struct *tty = port->info->tty;
	int overrun = asc_in(port, STA) & ASC_STA_OE;
	int residue, count;

	/* If stopping, do no more */
	if (!ascdmaport->rxdma_running)
		return;

	/* Report any overrun errors */
	if (overrun) {
		unsigned char n = '\0';
		char o = TTY_OVERRUN;
		port->icount.overrun++;
		tty->ldisc.receive_buf(tty, &n, &o, 1);
	}

	/* Determine if the DMA was stopped */
	ascdmaport->rxdma_running = 0;
	residue = get_dma_residue(ascdmaport->rxdma_chid);
	count = DMA_RXBUFSIZE - residue;

	/* If it was stopped, remove any data from the ASC FIFO */
	if (residue) {
		int i;
		unsigned char *buffer = ascdmaport->rxdmabuf[ascdmaport->rxdmabuf_head] + count;
		residue = min(FIFO_SIZE/2, residue);

		/* FIFO will be maximum half full */
		for (i=0; i<residue; i++) {
			if (asc_in(port, STA) & ASC_STA_RBF) {
				count++;
				*buffer++ = (unsigned char)asc_in(port, RXBUF);
			} else
				break;
		}
	}

	/* Flush the data to the tty buffer and set DMA going again */
	ascdmaport->rxdmabuf_count[ascdmaport->rxdmabuf_head] = count;
	asc_dma_rxflush(port);
}

static void asc_rxfmda_error(void* param)
{
	printk(KERN_ERR "ASC RX FDMA error\n");
}

void asc_fdma_start_tx(struct uart_port *port)
{
	struct circ_buf *xmit = &port->info->xmit;
	struct asc_dma_port *ascdmaport = &asc_dma_ports[port->line];
	int newtail, count, to_send = 0;
	unsigned char *insert;

	/* Don't do anything if nothing left to send */
	if (uart_tx_stopped(port) || (uart_circ_empty(xmit) && !port->x_char))
		return;

	/* Also do nothing if no buffer space left */
	if (ascdmaport->txdmabuf_tail == ((ascdmaport->txdmabuf_head + 1) & (DMA_TXBUFERS-1)))
		return;

	/* Work out where to put the new data to send */
	newtail = (ascdmaport->txdmabuf_tail - 1) & (DMA_TXBUFERS-1);
	insert = ascdmaport->txdmabuf[newtail];
	count = uart_circ_chars_pending(xmit);
	if (count > DMA_TXBUFSIZE)
		count = DMA_TXBUFSIZE;

	/* Handle xon/xoff character if required */
	if (port->x_char) {
		*insert++ = port->x_char;
		port->icount.tx++;
		to_send++;
		if (count == DMA_TXBUFSIZE)
			count--;

		/* Corner case: must send at least 4 bytes */
		while (to_send + count < 4) {
			*insert++ = port->x_char;
			to_send++;
		}

		port->x_char = 0;
	}
	to_send += count;

	/* Ensure that a multiple of 4 bytes is sent */
	if (to_send > 4) {
		while (to_send % 4) {
			to_send--;
			count--;
		}
	}

	/* Fill up the buffer with the data to send */
	if (count > 0 && to_send >= 4) {
		int bytes_to_end = CIRC_CNT_TO_END(xmit->head, xmit->tail, UART_XMIT_SIZE);

		/* If the circular buffer wraps round */
		if (count > bytes_to_end) {
			/* Copy up to the end of the buffer */
			memcpy(insert, xmit->buf + xmit->tail, bytes_to_end);
			xmit->tail = (xmit->tail + bytes_to_end) & (UART_XMIT_SIZE - 1);
			count -= bytes_to_end;
			insert += bytes_to_end;
			port->icount.tx += bytes_to_end;

			/* Copy remainder at the beginning */
			memcpy(insert, xmit->buf + xmit->tail, count);
			xmit->tail = (xmit->tail + count) & (UART_XMIT_SIZE - 1);
			port->icount.tx += count;
		} else {
			/* Just copy to fill up the remaining bytes */
			memcpy(insert, xmit->buf + xmit->tail, count);
			xmit->tail = (xmit->tail + count) & (UART_XMIT_SIZE - 1);
			port->icount.tx += count;
		}
	}

	/* Send any data */
	if (to_send >= 4) {
		/* Post the buffer to send */
		ascdmaport->txdmabuf_count[newtail] = to_send;
		ascdmaport->txdmabuf_tail = newtail;

		/* Start the transfer going if DMA is not running */
		if (!ascdmaport->txdma_running) {
			int err;
			ascdmaport->txdmabuf_head = ascdmaport->txdmabuf_tail;
			ascdmaport->txdma_running = 1;

			dma_parms_addrs(&ascdmaport->txdmap,
					virt_to_bus(ascdmaport->txdmabuf[newtail]),
					virt_to_bus((void*)(port->mapbase + ASC_TXBUF)),
					to_send);

			dma_parms_paced(&ascdmaport->txdmap,
					to_send,
					FDMA_TXREQ[port->line]);

			err = dma_compile_list(&ascdmaport->txdmap);

			dma_cache_wback(ascdmaport->txdmabuf[newtail], DMA_TXBUFSIZE);

			if(err==0)
				dma_xfer_list(ascdmaport->txdma_chid,&ascdmaport->txdmap);
			else
				printk(KERN_ERR "ASC TX FDMA failed to configure, error %d\n",err);
		}
	} else if (to_send > 0 && !uart_circ_empty(xmit) && !ascdmaport->txdma_running) {

		/* Feed out last little bit if stopped */
		while(!uart_circ_empty(xmit)) {
			asc_out (port, TXBUF, xmit->buf[xmit->tail]);
			xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
			port->icount.tx++;
		}
	}

	/* Wake up UART driver if necessary */
	if (uart_circ_chars_pending(xmit) < WAKEUP_CHARS)
		uart_write_wakeup(port);
}

static void asc_txfmda_done(void* param)
{
	struct uart_port *port = (struct uart_port *)param;
	struct asc_dma_port *ascdmaport = &asc_dma_ports[port->line];

	/* If on a stop request, do no more */
	if (!ascdmaport->txdma_running)
		return;

	ascdmaport->txdmabuf_count[ascdmaport->txdmabuf_head] = 0;

	/* If completed, tidy up */
	if (ascdmaport->txdmabuf_tail == ascdmaport->txdmabuf_head) {
		struct circ_buf *xmit = &port->info->xmit;
		int count = uart_circ_chars_pending(xmit);
		ascdmaport->txdma_running = 0;

		/* Feed out any odd bytes left over */
		if (count < 4) {
			while(!uart_circ_empty(xmit)) {
				asc_out (port, TXBUF, xmit->buf[xmit->tail]);
				xmit->tail = (xmit->tail + 1) & (UART_XMIT_SIZE - 1);
				port->icount.tx++;
			}
		}
	} else {
		/* Otherwise start the new block going */
		int newhead = (ascdmaport->txdmabuf_head - 1) & (DMA_TXBUFERS - 1);
		ascdmaport->txdmabuf_head = newhead;
		if (ascdmaport->txdmabuf_count[newhead] != 0) {
			int err;

			dma_parms_addrs(&ascdmaport->txdmap,
					virt_to_bus(ascdmaport->txdmabuf[newhead]),
					ascdmaport->txdmap.dar,
					ascdmaport->txdmabuf_count[newhead]);
			dma_parms_paced(&ascdmaport->txdmap,
					ascdmaport->txdmabuf_count[newhead],
					FDMA_TXREQ[port->line]);

			if((err=dma_compile_list(&ascdmaport->txdmap))<0) {
				printk(KERN_ERR "ASC TX FDMA  failed to reconfigure, error %d\n",err);
				return;
			}

			dma_cache_wback(ascdmaport->txdmabuf[newhead], DMA_TXBUFSIZE);

			if((err=dma_xfer_list(ascdmaport->txdma_chid,&ascdmaport->txdmap))<0) {
				printk(KERN_ERR "ASC TX FDMA  failed to restart, error %d\n",err);
				return;
			}
			asc_fdma_start_tx(port);
		} else
			ascdmaport->txdma_running = 0;
	}
}

static void asc_txfmda_error(void* param)
{
	printk(KERN_ERR "ASC TX FDMA error\n");
}

static void asc_rxtimer_fn(unsigned long param)
{
	struct uart_port *port = (struct uart_port *)param;
	struct asc_dma_port *ascdmaport = &asc_dma_ports[port->line];
	int residue;

	/* If not running, process and try to run */
	if (!ascdmaport->rxdma_running) {
		asc_dma_rxflush(port);
	} else {
		/* If receive has paused with data, stop so as to flush all received so far */
		residue = get_dma_residue(ascdmaport->rxdma_chid);
		if (residue == ascdmaport->last_residue && (residue != DMA_RXBUFSIZE || 
                       ascdmaport->rxdmabuf_count[ascdmaport->rxdmabuf_tail] != 0)) {
			dma_stop_channel(ascdmaport->rxdma_chid);
			ascdmaport->last_residue = 0;
		}
		else
			ascdmaport->last_residue = residue;
	}

	/* Reschedule the timer */
	mod_timer(&ascdmaport->rxpoll_timer, jiffies + RXPOLL_PERIOD);
}

void asc_fdma_stop_tx(struct uart_port *port)
{
	struct asc_dma_port *ascdmaport = &asc_dma_ports[port->line];

	if (ascdmaport->txdma_running) {
		int i;
		ascdmaport->txdma_running = 0;
		for (i=0; i<DMA_TXBUFERS; i++)
			ascdmaport->txdmabuf_count[i] = 0;
		ascdmaport->txdmabuf_head = ascdmaport->txdmabuf_tail = 0;
		dma_stop_channel(ascdmaport->txdma_chid);
		dma_wait_for_completion(ascdmaport->txdma_chid);
	}
}

void asc_fdma_stop_rx(struct uart_port *port)
{
	struct asc_dma_port *ascdmaport = &asc_dma_ports[port->line];
	int i;

	del_timer_sync(&ascdmaport->rxpoll_timer);

	if (ascdmaport->rxdma_running) {
		ascdmaport->rxdma_running = 0;
		dma_stop_channel(ascdmaport->rxdma_chid);
	}

	for (i=0; i<DMA_RXBUFERS; i++)
		ascdmaport->rxdmabuf_count[i] = 0;
	ascdmaport->rxdmabuf_head = ascdmaport->rxdmabuf_tail = 0;
}

int asc_enable_fdma(struct uart_port *port)
{
	struct asc_port *ascport = &asc_ports[port->line];
	struct asc_dma_port *ascdmaport = &asc_dma_ports[port->line];
	const char * fdmac_id =STM_DMAC_ID;
	const char * lb_cap_channel = STM_DMA_CAP_LOW_BW;
	/* Allocate the 2 DMA channels */

	if (!ascport->dma_enabled) {
		int i, err=0;
		ascdmaport->rxdma_chid = request_dma_bycap(&fdmac_id,&lb_cap_channel, "ASC_RX");
		if (ascdmaport->rxdma_chid < 0) {
			return -EBUSY;
		}

		ascdmaport->txdma_chid = request_dma_bycap(&fdmac_id,&lb_cap_channel, "ASC_TX");
		if (ascdmaport->txdma_chid < 0) {
			free_dma(ascdmaport->rxdma_chid);
			return -EBUSY;
		}

		/* Allocate tx/rx DMA buffers and get the DMA channels */
		for (i=0; i<DMA_RXBUFERS; i++) {
			ascdmaport->rxdmabuf[i] = kmalloc(DMA_RXBUFSIZE, __GFP_DMA | GFP_KERNEL);
			ascdmaport->rxdmabuf_count[i] = 0;
		}
		for (i=0; i<DMA_TXBUFERS; i++) {
			ascdmaport->txdmabuf[i] = kmalloc(DMA_TXBUFSIZE, __GFP_DMA | GFP_KERNEL);
			ascdmaport->txdmabuf_count[i] = 0;
		}

		ascdmaport->rxdmabuf_head = 0;
		ascdmaport->rxdmabuf_tail = 0;
		ascdmaport->txdmabuf_head = 0;
		ascdmaport->txdmabuf_tail = 0;

		ascdmaport->rxdma_running = 0;

		/* Set up the rx DMA parameters */
		declare_dma_parms(&ascdmaport->rxdmap,
				  MODE_PACED,
				  STM_DMA_LIST_OPEN,
				  STM_DMA_SETUP_CONTEXT_TASK,
				  STM_DMA_NOBLOCK_MODE,
				  (char*)STM_DMAC_ID);

		dma_parms_interrupts(&ascdmaport->rxdmap,STM_DMA_LIST_COMP_INT);

		dma_parms_comp_cb(&ascdmaport->rxdmap,
				  asc_rxfmda_done,
				  (void*)port,
				  STM_DMA_CB_CONTEXT_TASKLET);

		dma_parms_err_cb(&ascdmaport->rxdmap,
				 asc_rxfmda_error,
				 (void*)port,
				 STM_DMA_CB_CONTEXT_TASKLET);

		dma_parms_addrs(&ascdmaport->rxdmap,
				virt_to_bus((void*)(port->mapbase + ASC_RXBUF)),
				virt_to_bus(ascdmaport->rxdmabuf[0]),
				DMA_RXBUFSIZE);

		dma_parms_paced(&ascdmaport->rxdmap,DMA_RXBUFSIZE,FDMA_RXREQ[port->line]);

		if((err=dma_compile_list(&ascdmaport->rxdmap)) < 0) {
			printk(KERN_ERR "%s RX failed, err %d\n",__FUNCTION__,err);
			return -ENODEV;
		}

		/* Set up the tx DMA parameters */
		ascdmaport->txdma_running = 0;

		declare_dma_parms(&ascdmaport->txdmap,
				  MODE_PACED,
				  STM_DMA_LIST_OPEN,
				  STM_DMA_SETUP_CONTEXT_TASK,
				  STM_DMA_NOBLOCK_MODE,
				  (char*)STM_DMAC_ID);

		dma_parms_interrupts(&ascdmaport->txdmap,STM_DMA_LIST_COMP_INT);

		dma_parms_comp_cb(&ascdmaport->txdmap,
				  asc_txfmda_done,
				  (void*)port,
				  STM_DMA_CB_CONTEXT_TASKLET);

		dma_parms_err_cb(&ascdmaport->txdmap,
				 asc_txfmda_error,
				 (void*)port,
				 STM_DMA_CB_CONTEXT_TASKLET);

		/* We can delay compilation of the transmit descriptor until
		 * we know which port we are on*/

		/* Disable rx/tx interrupts */
		asc_out(port, INTEN, asc_in(port, INTEN) & ~(ASC_INTEN_THE | ASC_INTEN_RBE));
		ascport->dma_enabled = 1;

		/* Start reception going */
		ascdmaport->last_residue = 0;
		dma_cache_wback_inv(ascdmaport->rxdmabuf[0], DMA_RXBUFSIZE);
		if ((err = dma_xfer_list(ascdmaport->rxdma_chid,&ascdmaport->rxdmap)) < 0)
			printk(KERN_ERR "ASC RX FDMA failed to start - error %d\n",err);
		else
			ascdmaport->rxdma_running = 1;

		/* Start reception poll timer going */
		init_timer(&ascdmaport->rxpoll_timer);
		ascdmaport->rxpoll_timer.function = asc_rxtimer_fn;
		ascdmaport->rxpoll_timer.data = (unsigned long)port;
		ascdmaport->rxpoll_timer.expires = jiffies + RXPOLL_PERIOD;
		add_timer(&ascdmaport->rxpoll_timer);
	}

	return 0;
}

void asc_disable_fdma(struct uart_port *port)
{
	struct asc_port *ascport = &asc_ports[port->line];
	struct asc_dma_port *ascdmaport = &asc_dma_ports[port->line];

	if (ascport->dma_enabled) {
		int i;

		/* Stop and release DMA and buffer resources */
		asc_fdma_stop_rx(port);
		asc_fdma_stop_tx(port);
		free_dma(ascdmaport->rxdma_chid);
		free_dma(ascdmaport->txdma_chid);
		dma_free_descriptor(&ascdmaport->rxdmap);
		dma_free_descriptor(&ascdmaport->txdmap);
		memset(&ascdmaport->rxdmap,0,sizeof(struct stm_dma_params));
		memset(&ascdmaport->txdmap,0,sizeof(struct stm_dma_params));

		for (i=0; i<DMA_RXBUFERS; i++) {
			kfree(ascdmaport->rxdmabuf[i]);
			ascdmaport->rxdmabuf[i] = NULL;
		}
		for (i=0; i<DMA_TXBUFERS; i++) {
			kfree(ascdmaport->txdmabuf[i]);
			ascdmaport->txdmabuf[i] = NULL;
		}
		ascport->dma_enabled = 0;
	}
}
