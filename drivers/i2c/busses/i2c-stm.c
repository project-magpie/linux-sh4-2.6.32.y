/*
 * --------------------------------------------------------------------
 *
 * i2c-stm.c
 * i2c algorithms for STMicroelectronics SSC device
 * Version: 2.0 (1 April 2007)
 *
 * --------------------------------------------------------------------
 *
 *  Copyright (C) 2006  Virlinzi Francesco
 *                   <francesco.virlinzi@st.com>
 *
 * 23 August 2006 - Modified to support the 2.6.17 kernel version
 *	Virlinzi Francesco <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 */

#include <linux/i2c.h>
#include <linux/stm/pio.h>
#include <linux/spinlock.h>
#include <asm/io.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/errno.h>
#include <linux/preempt.h>
#include <asm/processor.h>
#include <asm/delay.h>
#include "./i2c-stm.h"
#include "../../stm/stm_ssc.h"

#undef dgb_print

#ifdef  CONFIG_I2C_DEBUG_BUS
#define dgb_print(fmt, args...)  printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#define dgb_print(fmt, args...)
#endif

#undef dgb_print2
#ifdef  CONFIG_I2C_DEBUG_ALGO
#define dgb_print2(fmt, args...)  printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#define dgb_print2(fmt, args...)
#endif

/* --- Defines for I2C --- */
#define DEVICE_ID                    0x041175

#define I2C_RATE_NORMAL            100000
#define I2C_RATE_FASTMODE          400000
#define NANOSEC_PER_SEC            1000000000

#define REP_START_HOLD_TIME_NORMAL   4000	/* standard */
#define REP_START_HOLD_TIME_FAST      600	/* it was 3500 but 600 is standard*/
#define START_HOLD_TIME_NORMAL       4000	/* standard */
#define START_HOLD_TIME_FAST          600	/* standard */
#define REP_START_SETUP_TIME_NORMAL  4700	/* standard */
#define REP_START_SETUP_TIME_FAST     600	/* standard */
#define DATA_SETUP_TIME_NORMAL        250	/* standard */
#define DATA_SETUP_TIME_FAST          100	/* standard */
#define STOP_SETUP_TIME_NORMAL       4000	/* standard */
#define STOP_SETUP_TIME_FAST          600	/* standard */
#define BUS_FREE_TIME_NORMAL         4700	/* standard */
#define BUS_FREE_TIME_FAST           1300	/* standard */

/* To manage normal vs fast mode */
#define IIC_STM_CONFIG_SPEED_MASK          0x1
#define IIC_STM_CONFIG_SPEED_NORMAL        0x0
#define IIC_STM_CONFIG_SPEED_FAST          0x1

#define IIC_STM_CONFIG_BAUDRATE_MASK       0xffff0000

typedef enum _iic_state_machine_e {
	IIC_FSM_VOID = 0,
	IIC_FSM_PREPARE,
	IIC_FSM_START,
	IIC_FSM_DATA_WRITE,
	IIC_FSM_PREPARE_2_READ,
	IIC_FSM_DATA_READ,
	IIC_FSM_STOP,
	IIC_FSM_COMPLETE,
	IIC_FSM_REPSTART,
	IIC_FSM_REPSTART_ADDR,
	IIC_FSM_ABORT
} iic_state_machine_e;

typedef enum _iic_fsm_error_e {
	IIC_E_NO_ERROR = 0x0,
	IIC_E_RUNNING = 0x1,
	IIC_E_NOTACK = 0x2
} iic_fsm_error_e;

struct iic_ssc {
	unsigned int iic_idx;
	struct i2c_adapter adapter;
	unsigned long config;
	struct list_head list;
};

/*
 * With the struct iic_transaction more information
 * on the required transaction are moved on
 * the thread stack instead of (iic_ssc) adapter descriptor...
 */
struct iic_transaction {
	struct iic_ssc *adapter;
	iic_state_machine_e start_state;
	iic_state_machine_e state;
	iic_state_machine_e next_state;
	struct i2c_msg *msgs_queue;
	int attempt;
	int queue_length;
	int current_msg;		/* the message on going */
	int idx_current_msg;		/* the byte in the message */
	iic_fsm_error_e status_error;
	int waitcondition;
};

#define jump_on_fsm_start(x)	{ (x)->state = IIC_FSM_START;	\
				goto be_fsm_start;	}

#define jump_on_fsm_repstart(x)	{ (x)->state = IIC_FSM_REPSTART; \
                                goto be_fsm_repstart;	}

#define jump_on_fsm_complete(x)	{ (x)->state = IIC_FSM_COMPLETE; \
				goto be_fsm_complete;	}

#define jump_on_fsm_stop(x)	{ (x)->state = IIC_FSM_STOP;	\
                                  goto be_fsm_stop;	}

#define jump_on_fsm_abort(x)	{ (x)->state = IIC_FSM_ABORT;    \
                                  goto be_fsm_abort;	}

#define check_fastmode(adap)	(((adap)->config & \
                                 IIC_STM_CONFIG_SPEED_MASK ) ? 1 : 0 )


static void iic_stm_setup_timing(struct iic_ssc *adap);

static void iic_state_machine(struct iic_transaction *trsc)
{
	struct iic_ssc* adap = trsc->adapter;
	struct ssc_t *ssc_bus =
		(struct ssc_t *)container_of(adap->adapter.dev.parent,struct ssc_t, pdev.dev);
	unsigned short status;
	short tx_fifo_status;
	unsigned int idx;
	unsigned short address;
	struct i2c_msg *pmsg;
	char fast_mode;
	union {
		char bytes[2];
		short word;
	} tmp;

	dgb_print2("\n");

	fast_mode = check_fastmode(adap);
	pmsg = trsc->msgs_queue + trsc->current_msg;

	trsc->state = trsc->next_state;

	barrier();
	switch (trsc->state) {
	case IIC_FSM_PREPARE:
		dgb_print2("-Prepare\n");
		/*
		 * Here we set the right Pio configuration
		 * because in the future SPI could change them
		 */
		stpio_set_pin(ssc_bus->pio_clk,  STPIO_ALT_BIDIR);
		stpio_set_pin(ssc_bus->pio_data, STPIO_ALT_BIDIR);
		/*
		 * check if the i2c timing register
		 * of ssc are ready to use
		 */
		if (check_fastmode(adap) && ssc_bus->i2c_timing != SSC_I2C_READY_FAST ||
		   !check_fastmode(adap) && ssc_bus->i2c_timing != SSC_I2C_READY_NORMAL )
			iic_stm_setup_timing(adap);
		jump_on_fsm_start(trsc);
		break;

	case IIC_FSM_START:
	      be_fsm_start:
		dgb_print2("-Start address 0x%x\n", pmsg->addr);
		ssc_store16(ssc_bus, SSC_CTL, SSC_CTL_SR | SSC_CTL_EN | 0x1);
		ssc_store16(ssc_bus, SSC_BRG,
			    (adap->config &
			     IIC_STM_CONFIG_BAUDRATE_MASK) >> 16);
		ssc_store16(ssc_bus, SSC_CTL,
			    SSC_CTL_EN | SSC_CTL_MS |
			    SSC_CTL_PO | SSC_CTL_PH | SSC_CTL_HB | 0x8);
		ssc_store16(ssc_bus, SSC_CLR, 0xdc0);
		ssc_store16(ssc_bus, SSC_I2C, SSC_I2C_I2CM |
			    (SSC_I2C_I2CFSMODE * fast_mode));
		address = (pmsg->addr << 2) | 0x1;
		trsc->start_state = IIC_FSM_START;
		trsc->next_state  = IIC_FSM_DATA_WRITE;
		if (pmsg->flags & I2C_M_RD){
			address |= 0x2;
			trsc->next_state = IIC_FSM_PREPARE_2_READ;
		}
		trsc->idx_current_msg = 0;
		ssc_store16(ssc_bus, SSC_IEN, SSC_IEN_NACKEN | SSC_IEN_TEEN);
		ssc_store16(ssc_bus, SSC_TBUF, address);
		ssc_store16(ssc_bus, SSC_I2C, SSC_I2C_I2CM |
			    SSC_I2C_STRTG | SSC_I2C_TXENB |
			    (SSC_I2C_I2CFSMODE * fast_mode));
		break;
	case IIC_FSM_PREPARE_2_READ:
		/* Just to clear th RBUF */
		ssc_load16(ssc_bus, SSC_RBUF);
		status = ssc_load16(ssc_bus, SSC_STA);
		dgb_print2(" Prepare to Read... Status=0x%x\n", status);
		if (status & SSC_STA_NACK)
			jump_on_fsm_abort(trsc);
		trsc->next_state = IIC_FSM_DATA_READ;
#if !defined(CONFIG_I2C_STM_HW_FIFO)
		if (!pmsg->len) {
			dgb_print("Zero Read\n");
			jump_on_fsm_stop(trsc);
		}
		ssc_store16(ssc_bus, SSC_TBUF, 0x1ff);
		if (pmsg->len == 1) {
			ssc_store16(ssc_bus, SSC_IEN, SSC_IEN_NACKEN);
			ssc_store16(ssc_bus, SSC_I2C, SSC_I2C_I2CM |
				    (SSC_I2C_I2CFSMODE * fast_mode));
		} else {
			ssc_store16(ssc_bus, SSC_I2C, SSC_I2C_I2CM |
				    SSC_I2C_ACKG |
				    (SSC_I2C_I2CFSMODE * fast_mode));
			ssc_store16(ssc_bus, SSC_IEN, SSC_IEN_RIEN);
		}
                break;
#else
		switch (pmsg->len) {
		case 0: dgb_print2("Zero Read\n");
			jump_on_fsm_stop(trsc);

		case 1: ssc_store16(ssc_bus, SSC_TBUF, 0x1ff);
			ssc_store16(ssc_bus, SSC_I2C, SSC_I2C_I2CM |
				(SSC_I2C_I2CFSMODE * fast_mode));
			ssc_store16(ssc_bus, SSC_IEN, SSC_IEN_NACKEN);
		   break;
		default:
			/* enable the fifos */
			ssc_store16(ssc_bus, SSC_CTL, SSC_CTL_EN | SSC_CTL_MS |
				SSC_CTL_PO | SSC_CTL_PH | SSC_CTL_HB | 0x8 |
				SSC_CTL_EN_TX_FIFO | SSC_CTL_EN_RX_FIFO );
			ssc_store16(ssc_bus, SSC_CLR, 0xdc0);
			ssc_store16(ssc_bus, SSC_I2C, SSC_I2C_I2CM | SSC_I2C_ACKG |
				(SSC_I2C_I2CFSMODE * fast_mode));
			/* P.S.: in any case the last byte has to be
			 *       managed in a different manner
			 */
			for ( idx = 0;  idx < SSC_RXFIFO_SIZE &&
					idx < pmsg->len-1 ;  ++idx )
				ssc_store16(ssc_bus, SSC_TBUF, 0x1ff);
			ssc_store16(ssc_bus, SSC_IEN, SSC_IEN_RIEN | SSC_IEN_TIEN);
		}
		break;
#endif
	case IIC_FSM_DATA_READ:
#if !defined(CONFIG_I2C_STM_HW_FIFO)
		status = ssc_load16(ssc_bus, SSC_STA);
		if (!(status & SSC_STA_TE))
			return;
		tmp.word = ssc_load16(ssc_bus, SSC_RBUF);
		tmp.word = tmp.word >> 1;
		pmsg->buf[trsc->idx_current_msg++] = tmp.bytes[0];
		dgb_print2(" Data Read...Status=0x%x %d-%c\n",
			status, tmp.bytes[0], tmp.bytes[0]);
		/*Did we finish? */
		if (trsc->idx_current_msg == pmsg->len) {
			status &= ~SSC_STA_NACK;
			jump_on_fsm_stop(trsc);
		} else {
			ssc_store16(ssc_bus, SSC_TBUF, 0x1ff);
			/*Is this the last byte? */
			if (trsc->idx_current_msg == (pmsg->len - 1)) {
				ssc_store16(ssc_bus, SSC_I2C, SSC_I2C_I2CM |
					 (SSC_I2C_I2CFSMODE * fast_mode));
				ssc_store16(ssc_bus, SSC_IEN, SSC_IEN_NACKEN);
			}
		}
		break;
#else
		status = ssc_load16(ssc_bus, SSC_STA);
		if (!(status & SSC_STA_TE))
			return;
		dgb_print2(" Data Read...Status=0x%x\n",status);
		/* 1.0 Is it the last byte */
		if (trsc->idx_current_msg == pmsg->len-1) {
			tmp.word = ssc_load16(ssc_bus, SSC_RBUF);
			tmp.word = tmp.word >> 1;
			pmsg->buf[trsc->idx_current_msg++] = tmp.bytes[0];
			dgb_print2(" Rx Data %d-%c\n",tmp.bytes[0], tmp.bytes[0]);
		} else
		/* 1.1 take the bytes from Rx fifo */
		for (idx = 0 ;  idx < SSC_RXFIFO_SIZE &&
			trsc->idx_current_msg < pmsg->len-1; ++idx ) {
				tmp.word = ssc_load16(ssc_bus, SSC_RBUF);
				tmp.word = tmp.word >> 1;
				pmsg->buf[trsc->idx_current_msg++] = tmp.bytes[0];
				dgb_print2(" Rx Data %d-%c\n",tmp.bytes[0], tmp.bytes[0]);
				}
		/* 2. Do we finish? */
		if (trsc->idx_current_msg == pmsg->len) {
			status &= ~SSC_STA_NACK;
			jump_on_fsm_stop(trsc);
		}
		/* 3. Ask other 'idx' bytes in fifo mode
		 *    but we want save the latest [pmsg->len-1]
		 *    in any case...
		 */
		for (idx=0; idx<SSC_TXFIFO_SIZE &&
			   (trsc->idx_current_msg+idx)<pmsg->len-1; ++idx)
			ssc_store16(ssc_bus, SSC_TBUF, 0x1ff);
		dgb_print2(" Asked %x bytes in fifo mode\n",idx);
		ssc_store16(ssc_bus,SSC_IEN,SSC_IEN_RIEN | SSC_IEN_TIEN);
		/*Is the next byte the last byte? */
		if (trsc->idx_current_msg == (pmsg->len - 1)) {
			dgb_print2(" Asked the last byte\n");
			ssc_store16(ssc_bus, SSC_CLR, 0xdc0);
			/* disable the fifos */
			ssc_store16(ssc_bus, SSC_CTL, SSC_CTL_EN | SSC_CTL_MS |
				SSC_CTL_PO | SSC_CTL_PH | SSC_CTL_HB | 0x8 );
			ssc_store16(ssc_bus, SSC_TBUF, 0x1ff);
			ssc_store16(ssc_bus, SSC_I2C, SSC_I2C_I2CM |
					    (SSC_I2C_I2CFSMODE * fast_mode) );
			ssc_store16(ssc_bus,SSC_IEN,SSC_IEN_NACKEN);
		}
		break;
#endif
	case IIC_FSM_DATA_WRITE:
		/* just to clear some bits in the STATUS register */
		ssc_load16(ssc_bus, SSC_RBUF);
/*
 * Be careful!!!!
 * Here I don't have to use 0xdc0 for
 * the SSC_CLR register
 */
		ssc_store16(ssc_bus, SSC_CLR, 0x9c0);
		status = ssc_load16(ssc_bus, SSC_STA);
		if (status & SSC_STA_NACK)
			jump_on_fsm_abort(trsc);
#if defined(CONFIG_I2C_STM_HW_FIFO)
		tx_fifo_status = ssc_load16(ssc_bus,SSC_TX_FSTAT);
		if ( tx_fifo_status ) {
			dgb_print2(" Fifo not empty\n");
			break;
		}
#endif
		if (trsc->idx_current_msg == pmsg->len || !(pmsg->len))
			jump_on_fsm_stop(trsc);
		dgb_print2(" Data Write...Status=0x%x 0x%x-%c\n", status,
			  pmsg->buf[trsc->idx_current_msg],
			  pmsg->buf[trsc->idx_current_msg]);
		ssc_store16(ssc_bus, SSC_I2C, SSC_I2C_I2CM | SSC_I2C_TXENB |
			    (SSC_I2C_I2CFSMODE * fast_mode));

		trsc->next_state = IIC_FSM_DATA_WRITE;
#if !defined(CONFIG_I2C_STM_HW_FIFO)
		ssc_store16(ssc_bus, SSC_IEN, SSC_IEN_TEEN | SSC_IEN_NACKEN);
#else
		ssc_store16(ssc_bus, SSC_IEN, SSC_IEN_TIEN | SSC_IEN_NACKEN);
		ssc_store16(ssc_bus, SSC_CTL, SSC_CTL_EN | SSC_CTL_MS |
                            SSC_CTL_PO | SSC_CTL_PH | SSC_CTL_HB | 0x8 |
			    SSC_CTL_EN_TX_FIFO);
		for (; tx_fifo_status < SSC_TXFIFO_SIZE &&
			trsc->idx_current_msg < pmsg->len ;++tx_fifo_status )
#endif
		{
		tmp.bytes[0] = pmsg->buf[trsc->idx_current_msg++];
		ssc_store16(ssc_bus, SSC_TBUF, tmp.word << 1 | 0x1);
		}
		break;

	case IIC_FSM_ABORT:
	      be_fsm_abort:
		dgb_print2(" Abort\n");
		trsc->status_error |= IIC_E_NOTACK;
		/* Don't ADD the break */

	case IIC_FSM_STOP:
	      be_fsm_stop:
		if (!(status & SSC_STA_NACK) &&
		    (++trsc->current_msg < trsc->queue_length)) {
			jump_on_fsm_repstart(trsc);
		}
		dgb_print2(" Stop\n");
		ssc_store16(ssc_bus, SSC_CLR, 0xdc0);
		ssc_store16(ssc_bus, SSC_I2C, SSC_I2C_I2CM |
			    SSC_I2C_TXENB | SSC_I2C_STOPG |
			    (SSC_I2C_I2CFSMODE * fast_mode));
		trsc->next_state = IIC_FSM_COMPLETE;
		ssc_store16(ssc_bus, SSC_IEN, SSC_IEN_STOPEN);
		break;

	case IIC_FSM_COMPLETE:
		be_fsm_complete:
		dgb_print2(" Complete\n");
		ssc_store16(ssc_bus, SSC_IEN, 0x0);
/*
 *  If there was some problem i can try again for adap->adapter.retries time...
 */
		if ((trsc->status_error & IIC_E_NOTACK) &&	/* there was a problem */
		    trsc->start_state == IIC_FSM_START &&	/* it cames from start state */
		    trsc->idx_current_msg == 0 &&		/* the problem is on address */
		    ++trsc->attempt <= adap->adapter.retries) {
			trsc->status_error = 0;
			jump_on_fsm_start(trsc);
		}
		if (!(trsc->status_error & IIC_E_NOTACK))
			trsc->status_error = IIC_E_NO_ERROR;
		trsc->waitcondition = 0;
		wake_up(&(ssc_bus->wait_queue));
		break;
	case IIC_FSM_REPSTART:
	      be_fsm_repstart:
		pmsg = trsc->msgs_queue + trsc->current_msg;
		dgb_print2("-Rep Start (0x%x)\n",pmsg->addr);
		trsc->start_state = IIC_FSM_REPSTART;
		trsc->idx_current_msg = 0;
		trsc->next_state = IIC_FSM_REPSTART_ADDR;
		ssc_store16(ssc_bus, SSC_CLR, 0xdc0);
		ssc_store16(ssc_bus, SSC_I2C, SSC_I2C_I2CM | SSC_I2C_TXENB
			    | SSC_I2C_REPSTRTG | (SSC_I2C_I2CFSMODE *
						  fast_mode));
		ssc_store16(ssc_bus, SSC_IEN, SSC_IEN_REPSTRTEN);
		break;
	case IIC_FSM_REPSTART_ADDR:
		dgb_print2("-Rep Start addr 0x%x\n", pmsg->addr);
		ssc_store16(ssc_bus, SSC_CLR, 0xdc0);
		ssc_store16(ssc_bus, SSC_I2C, SSC_I2C_I2CM | SSC_I2C_TXENB |
			    (SSC_I2C_I2CFSMODE * fast_mode));
		address = (pmsg->addr << 2) | 0x1;
		trsc->next_state = IIC_FSM_DATA_WRITE;
		if (pmsg->flags & I2C_M_RD) {
			address |= 0x2;
			trsc->next_state = IIC_FSM_PREPARE_2_READ;
		}
		ssc_store16(ssc_bus, SSC_TBUF, address);
		ssc_store16(ssc_bus, SSC_IEN, SSC_IEN_NACKEN | SSC_IEN_TEEN);
		break;
	default:
		printk(KERN_ERR " Error in the FSM\n");
		;
	}
	return;
}

static void iic_wait_stop_condition(struct ssc_t *ssc_bus)
{
  unsigned int idx;
/*
 * Look for a stop condition on the bus
 */
  dgb_print("\n");
  for ( idx = 0; idx < 5 ; ++idx )
    if ((ssc_load16(ssc_bus,SSC_STA) & SSC_STA_STOP) == 0)
        mdelay(2);
/*
 * At this point I hope I detected a stop condition
 * but in any case I return and I will tour off the ssc....
 */
}

static void iic_wait_free_bus(struct ssc_t *ssc_bus)
{
#if 1
  unsigned int idx;
/*
 * Look for a free condition on the bus
 */
  dgb_print("\n");
  for ( idx = 0; idx < 5 ; ++idx ) {
    if (!(ssc_load16(ssc_bus,SSC_STA) & SSC_STA_BUSY) )
	return ;
    mdelay(2);
  }
#endif
/*
 * At this point I hope I detected a free bus
 * but in any case I return and I will tour off the ssc....
 */
}

/*
 * Description: Prepares the controller for a transaction
 */
static int iic_stm_xfer(struct i2c_adapter *i2c_adap,
			     struct i2c_msg msgs[], int num)
{
	unsigned int flag;
	int result;
	int timeout;
	struct iic_ssc *adap =
			(struct iic_ssc *)container_of(i2c_adap, struct iic_ssc, adapter);
	struct ssc_t *ssc_bus =
			(struct ssc_t *)container_of(i2c_adap->dev.parent,struct ssc_t, pdev.dev);
	struct iic_transaction transaction = {
			.adapter      = adap,
			.msgs_queue   = msgs,
			.queue_length = num,
			.current_msg  = 0x0,
			.attempt      = 0x0,
			.status_error = IIC_E_RUNNING,
			.next_state   = IIC_FSM_PREPARE,
			.waitcondition = 1,
		};

	dgb_print("\n");
	ssc_request_bus(ssc_bus, iic_state_machine, &transaction);
	iic_wait_free_bus(ssc_bus);

	iic_state_machine(&transaction);

	timeout = wait_event_interruptible_timeout(ssc_bus->wait_queue,
					(transaction.waitcondition==0),
					i2c_adap->timeout *HZ );

	local_irq_save(flag);

	result = transaction.current_msg;

	if (unlikely(transaction.status_error != IIC_E_NO_ERROR || timeout <= 0)) {
		/* There was some problem */
		if(timeout<=0){
			/* There was a timeout !!!
			   - disable the interrupt
			   - generate a stop condition on the bus
			   all this task are done without interrupt....
			 */
			ssc_store16(ssc_bus, SSC_IEN, 0x0);
			ssc_store16(ssc_bus, SSC_I2C, SSC_I2C_I2CM |
				    SSC_I2C_STOPG | SSC_I2C_TXENB |
				    (SSC_I2C_I2CFSMODE * check_fastmode(adap)));
			/* wait until the ssc detects a Stop condition on the bus */
			/* but before we do that we enable all the interrupts     */
			local_irq_restore(flag);

			iic_wait_stop_condition(ssc_bus);

			/* turn off the ssc */
/*
 * Don't disable the SSC as this causes the SDA to go low, causing problems
 * for some slave devices.
 *			ssc_store16(ssc_bus, SSC_I2C, 0 );
 *			ssc_store16(ssc_bus, SSC_CTL, SSC_CTL_SR);
 *			ssc_store16(ssc_bus, SSC_CTL, 0 );
 */
		} else
			local_irq_restore(flag);

		if (!timeout){
			printk(KERN_ERR
			       "stm-i2c: Error timeout in the finite state machine\n");
			result = -ETIMEDOUT;
		} else if (timeout < 0) {
			dgb_print("stm-i2c: interrupt or error in wait event\n");
			result = timeout;
		} else
			result = -EREMOTEIO;
	} else
		local_irq_restore(flag);

	ssc_release_bus(ssc_bus);

	return result;
}

static void iic_stm_timing_trace(struct iic_ssc *adap)
{
	struct ssc_t *ssc_bus =
			container_of(adap->adapter.dev.parent, struct ssc_t, pdev.dev);
	dgb_print("SSC_BRG  %d\n", adap->config >> 16);
	dgb_print("SSC_REP_START_HOLD %d\n",
		  ssc_load16(ssc_bus, SSC_REP_START_HOLD));
	dgb_print("SSC_REP_START_SETUP %d\n",
		  ssc_load16(ssc_bus, SSC_REP_START_SETUP));
	dgb_print("SSC_START_HOLD %d\n", ssc_load16(ssc_bus, SSC_START_HOLD));
	dgb_print("SSC_DATA_SETUP %d\n", ssc_load16(ssc_bus, SSC_DATA_SETUP));
	dgb_print("SSC_STOP_SETUP %d\n", ssc_load16(ssc_bus, SSC_STOP_SETUP));
	dgb_print("SSC_BUS_FREE %d\n", ssc_load16(ssc_bus, SSC_BUS_FREE));
	dgb_print("SSC_PRE_SCALER_BRG %d\n",
		  ssc_load16(ssc_bus, SSC_PRE_SCALER_BRG));
	dgb_print("SSC_AGFR 0x%x\n", ssc_load8(ssc_bus, SSC_AGFR));
	dgb_print("SSC_PRSC %d\n", ssc_load8(ssc_bus, SSC_PRSC));
}

static void iic_stm_setup_timing(struct iic_ssc *adap)
{
	struct ssc_t *ssc_bus =
			container_of(adap->adapter.dev.parent, struct ssc_t, pdev.dev);
	unsigned long iic_baudrate;
	unsigned short iic_rep_start_hold;
	unsigned short iic_start_hold, iic_rep_start_setup;
	unsigned short iic_data_setup, iic_stop_setup;
	unsigned short iic_bus_free, iic_pre_scale_baudrate;
	unsigned char iic_agfr, iic_prsc;
	unsigned long clock = ssc_get_clock();
	unsigned long NSPerCyc = NANOSEC_PER_SEC / clock;

	NSPerCyc = NANOSEC_PER_SEC /clock;
	dgb_print("Assuming %d MHz for the Timing Setup %d\n",
		  clock / 1000000,NSPerCyc);

	iic_agfr = 0x0;
	iic_prsc = (int)clock / 10000000;
	iic_pre_scale_baudrate = 0x1;

	if (check_fastmode(adap)) {
		ssc_bus->i2c_timing = SSC_I2C_READY_FAST;
		iic_baudrate = clock / (2 * I2C_RATE_FASTMODE);
		iic_rep_start_hold = REP_START_HOLD_TIME_FAST / NSPerCyc;
		iic_start_hold = START_HOLD_TIME_FAST / NSPerCyc;
		iic_rep_start_setup = REP_START_SETUP_TIME_FAST / NSPerCyc;
		iic_data_setup = DATA_SETUP_TIME_FAST / NSPerCyc;
		iic_stop_setup = STOP_SETUP_TIME_FAST / NSPerCyc;
		iic_bus_free = BUS_FREE_TIME_FAST / NSPerCyc;
	} else {
		ssc_bus->i2c_timing = SSC_I2C_READY_NORMAL;
		iic_baudrate = clock  / (2 * I2C_RATE_NORMAL);
		iic_rep_start_hold = REP_START_HOLD_TIME_NORMAL / NSPerCyc;
		iic_start_hold = START_HOLD_TIME_NORMAL / NSPerCyc;
		iic_rep_start_setup = REP_START_SETUP_TIME_NORMAL / NSPerCyc;
		iic_data_setup = DATA_SETUP_TIME_NORMAL / NSPerCyc;
		iic_stop_setup = STOP_SETUP_TIME_NORMAL / NSPerCyc;
		iic_bus_free = BUS_FREE_TIME_NORMAL / NSPerCyc;
	}

	adap->config &= ~IIC_STM_CONFIG_BAUDRATE_MASK;
	adap->config |= iic_baudrate << 16;

	ssc_store16(ssc_bus, SSC_REP_START_HOLD, iic_rep_start_hold);
	ssc_store16(ssc_bus, SSC_START_HOLD, iic_start_hold);
	ssc_store16(ssc_bus, SSC_REP_START_SETUP, iic_rep_start_setup);
	ssc_store16(ssc_bus, SSC_DATA_SETUP, iic_data_setup);
	ssc_store16(ssc_bus, SSC_STOP_SETUP, iic_stop_setup);
	ssc_store16(ssc_bus, SSC_BUS_FREE, iic_bus_free);
	ssc_store8(ssc_bus, SSC_AGFR, iic_agfr);
	ssc_store8(ssc_bus, SSC_PRSC, iic_prsc);
	ssc_store16(ssc_bus, SSC_PRE_SCALER_BRG, iic_pre_scale_baudrate);
	iic_stm_timing_trace(adap);
	return;
}

static int iic_stm_control(struct i2c_adapter *adapter,
				unsigned int cmd, unsigned long arg)
{
	struct iic_ssc *iic_adap =
	    container_of(adapter, struct iic_ssc, adapter);
	switch (cmd) {
	case I2C_STM_IOCTL_FAST:
		dgb_print("ioctl fast\n");
		iic_adap->config &= ~IIC_STM_CONFIG_SPEED_MASK;
		if (arg)
			iic_adap->config |=
			    IIC_STM_CONFIG_SPEED_FAST;
		break;
	default:
		printk(KERN_WARNING" i2c-ioctl not managed\n");
	}
/*
 * the timeout and the retries ioctl
 * are managed by i2c core system
 */
	return 0;
}

static u32 iic_stm_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static struct i2c_algorithm iic_stm_algo = {
	.master_xfer   = iic_stm_xfer,
	.functionality = iic_stm_func,
	.algo_control  = iic_stm_control
};

static ssize_t iic_bus_show_fastmode(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	struct i2c_adapter *adapter = container_of(dev, struct i2c_adapter, dev);
	struct iic_ssc     *iic_stm = container_of(adapter,struct iic_ssc,adapter);
	return sprintf(buf, "%u\n",check_fastmode(iic_stm));
}

static ssize_t iic_bus_store_fastmode(struct device *dev,struct device_attribute *attr,
			 const char *buf,size_t count)
{
	struct i2c_adapter *adapter = container_of(dev, struct i2c_adapter, dev);
	unsigned long val = simple_strtoul(buf, NULL, 10);

	iic_stm_control(adapter,I2C_STM_IOCTL_FAST,val);

	return count;
}

static DEVICE_ATTR(fastmode, S_IRUGO | S_IWUSR, iic_bus_show_fastmode,
			iic_bus_store_fastmode);

static LIST_HEAD(stm_busses);

static int __init iic_stm_bus_init(void)
{
	unsigned int ssc_number = ssc_device_available();
	unsigned int idx;
	unsigned int adapnr = 0;
	struct iic_ssc *iic_stm;

	for (idx = 0; idx < ssc_number; ++idx) {
		if (!(ssc_capability(idx) & SSC_I2C_CAPABILITY))
			continue;
		iic_stm =
		    (struct iic_ssc *)kzalloc(sizeof(struct iic_ssc), GFP_KERNEL);
		if (!iic_stm) {
			printk(KERN_EMERG
			       "Error on initialization of  ssc-i2c adapter module\n");
			return -ENODEV;
		}
/*
 * P.S.: with the "kzalloc" the iic_stm->config is zero
 *       this means:
 *       - i2c speed  = normal
 */
		iic_stm->adapter.owner = THIS_MODULE;
		iic_stm->adapter.id = adapnr;
		iic_stm->adapter.timeout = 4;
		iic_stm->adapter.class   = I2C_CLASS_ALL;
		sprintf(iic_stm->adapter.name,"i2c-ssc-%d",adapnr);
		iic_stm->adapter.algo = &iic_stm_algo;
//		iic_stm->adapter.dev.bus = &i2c_bus_type;
		iic_stm->adapter.dev.parent = &(ssc_device_request(idx)->pdev.dev);
/*
		iic_stm->adapter.dev.release
*/
		iic_stm_setup_timing(iic_stm);

		if (i2c_add_adapter(&(iic_stm->adapter)) < 0) {
			printk(KERN_ERR
			       "i2c/stm: The I2C Core refuses the i2c/stm adapter\n");
			kfree(iic_stm);
			return -ENODEV;
		} else {
			device_create_file(&(iic_stm->adapter.dev), &dev_attr_fastmode);
		}
		list_add(&(iic_stm->list), &(stm_busses));
		adapnr ++;
	}
	return 0;
}

static void __exit iic_stm_bus_exit(void)
{
	struct iic_ssc *iic_stm;
	struct i2c_adapter *iic_adapter;
	struct list_head *item;
	dgb_print("\n");
	list_for_each(item, &(stm_busses)) {
		iic_stm = container_of(item, struct iic_ssc, list);
		list_del(&iic_stm->list);
		iic_adapter = &(iic_stm->adapter);
		i2c_del_adapter(iic_adapter);
		kfree(iic_stm);
	}
}

late_initcall(iic_stm_bus_init);

module_exit(iic_stm_bus_exit);

MODULE_AUTHOR("STMicroelectronics  <www.st.com>");
MODULE_DESCRIPTION("i2c-stm algorithm for ssc device");
MODULE_LICENSE("GPL");
