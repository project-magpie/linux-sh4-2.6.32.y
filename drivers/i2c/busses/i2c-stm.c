/*
 * --------------------------------------------------------------------
 *
 * i2c-stm.c
 * i2c algorithms for STMicroelectronics device
 * Version: 2.0 (1 April 2007)
 * Version: 2.0.1 (20 Dec 2007)
 *   + Removed the ssc layer.
 * Version: 2.1 (3 Jan 2008)
 *   + Added the glitch suppression support
 *
 * --------------------------------------------------------------------
 *
 *  Copyright (C) 2006: STMicroelectronics
 *  Copyright (C) 2007: STMicroelectronics
 *  Copyright (C) 2008: STMicroelectronics
 *  Author: Francesco Virlinzi     <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License version 2.0 ONLY.  See linux/COPYING for more information.
 *
 */

#include <linux/i2c.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <linux/stm/soc_init.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <asm/io.h>
#include <linux/module.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/errno.h>
#include <asm/delay.h>
#include "./i2c-stm.h"
#include <linux/stm/stssc.h>

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

#if 0
#define REP_START_HOLD_TIME_NORMAL	4000	/* standard */
#define REP_START_HOLD_TIME_FAST	 600	/* standard*/
#define START_HOLD_TIME_NORMAL		4000	/* standard */
#define START_HOLD_TIME_FAST		 600	/* standard */
#define REP_START_SETUP_TIME_NORMAL	4700	/* standard */
#define REP_START_SETUP_TIME_FAST	 600	/* standard */
#define DATA_SETUP_TIME_NORMAL		 250	/* standard */
#define DATA_SETUP_TIME_FAST		 100	/* standard */
#define STOP_SETUP_TIME_NORMAL		4000	/* standard */
#define STOP_SETUP_TIME_FAST		 600	/* standard */
#define BUS_FREE_TIME_NORMAL		4700	/* standard */
#define BUS_FREE_TIME_FAST		1300	/* standard */
#else
/* These valus cames directly from hw boys... */
#define REP_START_HOLD_TIME_NORMAL	4000
#define REP_START_HOLD_TIME_FAST	6500
#define START_HOLD_TIME_NORMAL		4500
#define START_HOLD_TIME_FAST		800
#define REP_START_SETUP_TIME_NORMAL	4700
#define REP_START_SETUP_TIME_FAST	800
#define DATA_SETUP_TIME_NORMAL		300
#define DATA_SETUP_TIME_FAST		300
#define STOP_SETUP_TIME_NORMAL		4200
#define STOP_SETUP_TIME_FAST		800
#define BUS_FREE_TIME_NORMAL		5700
#define BUS_FREE_TIME_FAST		1500
#endif

/* Define for glitch suppression support */
#ifdef CONFIG_I2C_STM_GLITCH_SUPPORT
  #if CONFIG_GLITCH_CLK_WIDTH > 0
    #define GLITCH_WIDTH_CLOCK			CONFIG_GLITCH_CLK_WIDTH
  #else
    #define GLITCH_WIDTH_CLOCK			500 /* in nanosecs */
  #endif
  #if CONFIG_GLITCH_DATA_WIDTH > 0
    #define GLITCH_WIDTH_DATA			CONFIG_GLITCH_DATA_WIDTH
  #else
    #define GLITCH_WIDTH_DATA			500 /* in nanosecs */
  #endif
#else
    #define GLITCH_WIDTH_DATA			0
    #define GLITCH_WIDTH_CLOCK			0
#endif

/* To manage normal vs fast mode */
#define IIC_STM_CONFIG_SPEED_MASK          0x1
#define IIC_STM_CONFIG_SPEED_FAST          0x1
#define IIC_STM_READY_SPEED_MASK	   0x2
#define IIC_STM_READY_SPEED_FAST	   0x2

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

/*
 * With the struct iic_transaction more information
 * on the required transaction are moved on
 * the thread stack instead of (iic_ssc) adapter descriptor...
 */
struct iic_transaction {
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

struct iic_ssc {
	unsigned long base;
	struct iic_transaction *trns;
	struct i2c_adapter adapter;
	unsigned long config;
	wait_queue_head_t wait_queue;
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

#define check_ready_fastmode(adap)	(((adap)->config & \
				IIC_STM_READY_SPEED_FAST ) ? 1 : 0 )

#define set_ready_fastmode(adap) ((adap)->config |= IIC_STM_READY_SPEED_FAST)

#define clear_ready_fastmode(adap) ((adap)->config &= ~IIC_STM_READY_SPEED_FAST)

static void iic_stm_setup_timing(struct iic_ssc *adap,unsigned long rate);

static irqreturn_t iic_state_machine(int this_irq, struct iic_ssc* adap)
{
	struct iic_transaction *trsc = adap->trns;
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
		 * check if the i2c timing register
		 * of ssc are ready to use
		 */
		if (check_fastmode(adap) && !check_ready_fastmode(adap) ||
		    !check_fastmode(adap) && check_ready_fastmode(adap))
			iic_stm_setup_timing(adap,
				clk_get_rate(clk_get(NULL,"comms_clk")));
		jump_on_fsm_start(trsc);
		break;

	case IIC_FSM_START:
	      be_fsm_start:
		dgb_print2("-Start address 0x%x\n", pmsg->addr);
		ssc_store32(adap, SSC_CTL, SSC_CTL_SR | SSC_CTL_EN | 0x1);
		ssc_store32(adap, SSC_CTL,
			    SSC_CTL_EN | SSC_CTL_MS |
			    SSC_CTL_PO | SSC_CTL_PH | SSC_CTL_HB | 0x8);
		ssc_store32(adap, SSC_CLR, 0xdc0);
		ssc_store32(adap, SSC_I2C, SSC_I2C_I2CM |
			    (SSC_I2C_I2CFSMODE * fast_mode));
		address = (pmsg->addr << 2) | 0x1;
		trsc->start_state = IIC_FSM_START;
		trsc->next_state  = IIC_FSM_DATA_WRITE;
		if (pmsg->flags & I2C_M_RD){
			address |= 0x2;
			trsc->next_state = IIC_FSM_PREPARE_2_READ;
		}
		trsc->idx_current_msg = 0;
		ssc_store32(adap, SSC_IEN, SSC_IEN_NACKEN | SSC_IEN_TEEN);
		ssc_store32(adap, SSC_TBUF, address);
		ssc_store32(adap, SSC_I2C, SSC_I2C_I2CM |
			    SSC_I2C_STRTG | SSC_I2C_TXENB |
			    (SSC_I2C_I2CFSMODE * fast_mode));
		break;
	case IIC_FSM_PREPARE_2_READ:
		/* Just to clear th RBUF */
		ssc_load32(adap, SSC_RBUF);
		status = ssc_load32(adap, SSC_STA);
		dgb_print2(" Prepare to Read... Status=0x%x\n", status);
		if (status & SSC_STA_NACK)
			jump_on_fsm_abort(trsc);
		trsc->next_state = IIC_FSM_DATA_READ;
#if !defined(CONFIG_I2C_STM_HW_FIFO)
		if (!pmsg->len) {
			dgb_print("Zero Read\n");
			jump_on_fsm_stop(trsc);
		}
		ssc_store32(adap, SSC_TBUF, 0x1ff);
		if (pmsg->len == 1) {
			ssc_store32(adap, SSC_IEN, SSC_IEN_NACKEN);
			ssc_store32(adap, SSC_I2C, SSC_I2C_I2CM |
				    (SSC_I2C_I2CFSMODE * fast_mode));
		} else {
			ssc_store32(adap, SSC_I2C, SSC_I2C_I2CM |
				    SSC_I2C_ACKG |
				    (SSC_I2C_I2CFSMODE * fast_mode));
			ssc_store32(adap, SSC_IEN, SSC_IEN_RIEN);
		}
                break;
#else
		switch (pmsg->len) {
		case 0: dgb_print2("Zero Read\n");
			jump_on_fsm_stop(trsc);

		case 1: ssc_store32(adap, SSC_TBUF, 0x1ff);
			ssc_store32(adap, SSC_I2C, SSC_I2C_I2CM |
				(SSC_I2C_I2CFSMODE * fast_mode));
			ssc_store32(adap, SSC_IEN, SSC_IEN_NACKEN);
		   break;
		default:
			/* enable the fifos */
			ssc_store32(adap, SSC_CTL, SSC_CTL_EN | SSC_CTL_MS |
				SSC_CTL_PO | SSC_CTL_PH | SSC_CTL_HB | 0x8 |
				SSC_CTL_EN_TX_FIFO | SSC_CTL_EN_RX_FIFO );
			ssc_store32(adap, SSC_CLR, 0xdc0);
			ssc_store32(adap, SSC_I2C, SSC_I2C_I2CM | SSC_I2C_ACKG |
				(SSC_I2C_I2CFSMODE * fast_mode));
			/* P.S.: in any case the last byte has to be
			 *       managed in a different manner
			 */
			for ( idx = 0;  idx < SSC_RXFIFO_SIZE &&
					idx < pmsg->len-1 ;  ++idx )
				ssc_store32(adap, SSC_TBUF, 0x1ff);
			ssc_store32(adap, SSC_IEN, SSC_IEN_RIEN | SSC_IEN_TIEN);
		}
		break;
#endif
	case IIC_FSM_DATA_READ:
#if !defined(CONFIG_I2C_STM_HW_FIFO)
		status = ssc_load32(adap, SSC_STA);
		if (!(status & SSC_STA_TE))
			break;
		tmp.word = ssc_load32(adap, SSC_RBUF);
		tmp.word = tmp.word >> 1;
		pmsg->buf[trsc->idx_current_msg++] = tmp.bytes[0];
		dgb_print2(" Data Read...Status=0x%x %d-%c\n",
			status, tmp.bytes[0], tmp.bytes[0]);
		/*Did we finish? */
		if (trsc->idx_current_msg == pmsg->len) {
			status &= ~SSC_STA_NACK;
			jump_on_fsm_stop(trsc);
		} else {
			ssc_store32(adap, SSC_TBUF, 0x1ff);
			/*Is this the last byte? */
			if (trsc->idx_current_msg == (pmsg->len - 1)) {
				ssc_store32(adap, SSC_I2C, SSC_I2C_I2CM |
					 (SSC_I2C_I2CFSMODE * fast_mode));
				ssc_store32(adap, SSC_IEN, SSC_IEN_NACKEN);
			}
		}
		break;
#else
		status = ssc_load32(adap, SSC_STA);
		if (!(status & SSC_STA_TE))
			break;
		dgb_print2(" Data Read...Status=0x%x\n",status);
		/* 1.0 Is it the last byte */
		if (trsc->idx_current_msg == pmsg->len-1) {
			tmp.word = ssc_load32(adap, SSC_RBUF);
			tmp.word = tmp.word >> 1;
			pmsg->buf[trsc->idx_current_msg++] = tmp.bytes[0];
			dgb_print2(" Rx Data %d-%c\n",tmp.bytes[0], tmp.bytes[0]);
		} else
		/* 1.1 take the bytes from Rx fifo */
		for (idx = 0 ;  idx < SSC_RXFIFO_SIZE &&
			trsc->idx_current_msg < pmsg->len-1; ++idx ) {
				tmp.word = ssc_load32(adap, SSC_RBUF);
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
			ssc_store32(adap, SSC_TBUF, 0x1ff);
		dgb_print2(" Asked %x bytes in fifo mode\n",idx);
		ssc_store32(adap,SSC_IEN,SSC_IEN_RIEN | SSC_IEN_TIEN);
		/*Is the next byte the last byte? */
		if (trsc->idx_current_msg == (pmsg->len - 1)) {
			dgb_print2(" Asked the last byte\n");
			ssc_store32(adap, SSC_CLR, 0xdc0);
			/* disable the fifos */
			ssc_store32(adap, SSC_CTL, SSC_CTL_EN | SSC_CTL_MS |
				SSC_CTL_PO | SSC_CTL_PH | SSC_CTL_HB | 0x8 );
			ssc_store32(adap, SSC_TBUF, 0x1ff);
			ssc_store32(adap, SSC_I2C, SSC_I2C_I2CM |
					    (SSC_I2C_I2CFSMODE * fast_mode) );
			ssc_store32(adap,SSC_IEN,SSC_IEN_NACKEN);
		}
		break;
#endif
	case IIC_FSM_DATA_WRITE:
		/* just to clear some bits in the STATUS register */
		ssc_load32(adap, SSC_RBUF);
/*
 * Be careful!!!!
 * Here I don't have to use 0xdc0 for
 * the SSC_CLR register
 */
		ssc_store32(adap, SSC_CLR, 0x9c0);
		status = ssc_load32(adap, SSC_STA);
		if (status & SSC_STA_NACK)
			jump_on_fsm_abort(trsc);
#if defined(CONFIG_I2C_STM_HW_FIFO)
		tx_fifo_status = ssc_load32(adap,SSC_TX_FSTAT);
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
		ssc_store32(adap, SSC_I2C, SSC_I2C_I2CM | SSC_I2C_TXENB |
			    (SSC_I2C_I2CFSMODE * fast_mode));

		trsc->next_state = IIC_FSM_DATA_WRITE;
#if !defined(CONFIG_I2C_STM_HW_FIFO)
		ssc_store32(adap, SSC_IEN, SSC_IEN_TEEN | SSC_IEN_NACKEN);
#else
		ssc_store32(adap, SSC_IEN, SSC_IEN_TEEN | SSC_IEN_NACKEN);
		ssc_store32(adap, SSC_CTL, SSC_CTL_EN | SSC_CTL_MS |
                            SSC_CTL_PO | SSC_CTL_PH | SSC_CTL_HB | 0x8 |
			    SSC_CTL_EN_TX_FIFO);
		for (; tx_fifo_status < SSC_TXFIFO_SIZE &&
			trsc->idx_current_msg < pmsg->len ;++tx_fifo_status )
#endif
		{
		tmp.bytes[0] = pmsg->buf[trsc->idx_current_msg++];
		ssc_store32(adap, SSC_TBUF, tmp.word << 1 | 0x1);
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
		ssc_store32(adap, SSC_CLR, 0xdc0);
		ssc_store32(adap, SSC_I2C, SSC_I2C_I2CM |
			    SSC_I2C_TXENB | SSC_I2C_STOPG |
			    (SSC_I2C_I2CFSMODE * fast_mode));
		trsc->next_state = IIC_FSM_COMPLETE;
		ssc_store32(adap, SSC_IEN, SSC_IEN_STOPEN);
		break;

	case IIC_FSM_COMPLETE:
		be_fsm_complete:
		dgb_print2(" Complete\n");
		ssc_store32(adap, SSC_IEN, 0x0);
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
		wake_up(&(adap->wait_queue));
		break;
	case IIC_FSM_REPSTART:
	      be_fsm_repstart:
		pmsg = trsc->msgs_queue + trsc->current_msg;
		dgb_print2("-Rep Start (0x%x)\n",pmsg->addr);
		trsc->start_state = IIC_FSM_REPSTART;
		trsc->idx_current_msg = 0;
		trsc->next_state = IIC_FSM_REPSTART_ADDR;
		ssc_store32(adap, SSC_CLR, 0xdc0);
		ssc_store32(adap, SSC_I2C, SSC_I2C_I2CM | SSC_I2C_TXENB
			    | SSC_I2C_REPSTRTG | (SSC_I2C_I2CFSMODE *
						  fast_mode));
		ssc_store32(adap, SSC_IEN, SSC_IEN_REPSTRTEN);
		break;
	case IIC_FSM_REPSTART_ADDR:
		dgb_print2("-Rep Start addr 0x%x\n", pmsg->addr);
		ssc_store32(adap, SSC_CLR, 0xdc0);
		ssc_store32(adap, SSC_I2C, SSC_I2C_I2CM | SSC_I2C_TXENB |
			    (SSC_I2C_I2CFSMODE * fast_mode));
		address = (pmsg->addr << 2) | 0x1;
		trsc->next_state = IIC_FSM_DATA_WRITE;
		if (pmsg->flags & I2C_M_RD) {
			address |= 0x2;
			trsc->next_state = IIC_FSM_PREPARE_2_READ;
		}
		ssc_store32(adap, SSC_TBUF, address);
		ssc_store32(adap, SSC_IEN, SSC_IEN_NACKEN | SSC_IEN_TEEN);
		break;
	default:
		printk(KERN_ERR " Error in the FSM\n");
		;
	}
	return IRQ_HANDLED;
}

static void iic_wait_stop_condition(struct iic_ssc *adap)
{
  unsigned int idx;
/*
 * Look for a stop condition on the bus
 */
  dgb_print("\n");
  for ( idx = 0; idx < 5 ; ++idx )
    if ((ssc_load32(adap,SSC_STA) & SSC_STA_STOP) == 0)
        mdelay(2);
/*
 * At this point I hope I detected a stop condition
 * but in any case I return and I will tour off the ssc....
 */
}

static void iic_wait_free_bus(struct iic_ssc *adap)
{
  unsigned int idx;
/*
 * Look for a free condition on the bus
 */
  dgb_print("\n");
  for ( idx = 0; idx < 5 ; ++idx ) {
    if (!(ssc_load32(adap,SSC_STA) & SSC_STA_BUSY) )
	return ;
    mdelay(2);
  }
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
	struct iic_transaction transaction = {
			.msgs_queue   = msgs,
			.queue_length = num,
			.current_msg  = 0x0,
			.attempt      = 0x0,
			.status_error = IIC_E_RUNNING,
			.next_state   = IIC_FSM_PREPARE,
			.waitcondition = 1,
		};

	dgb_print("\n");

	iic_wait_free_bus(adap);

	adap->trns = &transaction;

	iic_state_machine(NULL,adap);

	timeout = wait_event_interruptible_timeout(adap->wait_queue,
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
			ssc_store32(adap, SSC_IEN, 0x0);
			ssc_store32(adap, SSC_I2C, SSC_I2C_I2CM |
				    SSC_I2C_STOPG | SSC_I2C_TXENB |
				    (SSC_I2C_I2CFSMODE * check_fastmode(adap)));
			/* wait until the ssc detects a Stop condition on the bus */
			/* but before we do that we enable all the interrupts     */
			local_irq_restore(flag);

			iic_wait_stop_condition(adap);
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

	return result;
}

static void iic_stm_timing_trace(struct iic_ssc *adap)
{
	dgb_print("SSC_BRG  %d\n",ssc_load32(adap, SSC_BRG));
	dgb_print("SSC_REP_START_HOLD %d\n",
		  ssc_load32(adap, SSC_REP_START_HOLD));
	dgb_print("SSC_REP_START_SETUP %d\n",
		  ssc_load32(adap, SSC_REP_START_SETUP));
	dgb_print("SSC_START_HOLD %d\n", ssc_load32(adap, SSC_START_HOLD));
	dgb_print("SSC_DATA_SETUP %d\n", ssc_load32(adap, SSC_DATA_SETUP));
	dgb_print("SSC_STOP_SETUP %d\n", ssc_load32(adap, SSC_STOP_SETUP));
	dgb_print("SSC_BUS_FREE %d\n", ssc_load32(adap, SSC_BUS_FREE));
	dgb_print("SSC_PRE_SCALER_BRG %d\n",
		  ssc_load32(adap, SSC_PRE_SCALER_BRG));
	dgb_print("SSC_NOISE_SUPP_WIDTH %d\n",
			ssc_load32(adap, SSC_NOISE_SUPP_WIDTH));
	dgb_print("SSC_PRSCALER %d\n",
			ssc_load32(adap, SSC_PRSCALER));
	dgb_print("SSC_NOISE_SUPP_WIDTH_DATAOUT %d\n",
			ssc_load32(adap, SSC_NOISE_SUPP_WIDTH_DATAOUT));
	dgb_print("SSC_PRSCALER_DATAOUT %d\n",
			ssc_load32(adap, SSC_PRSCALER_DATAOUT));
}

static void iic_stm_setup_timing(struct iic_ssc *adap, unsigned long clock)
{
	unsigned long  iic_baudrate;
	unsigned short iic_rep_start_hold;
	unsigned short iic_start_hold;
	unsigned short iic_rep_start_setup;
	unsigned short iic_data_setup;
	unsigned short iic_stop_setup;
	unsigned short iic_bus_free;
	unsigned short iic_pre_scale_baudrate = 1;
	unsigned short iic_glitch_width;
	unsigned short iic_glitch_width_dataout;
	unsigned char  iic_prescaler;
	unsigned short iic_prescaler_dataout ;
	unsigned long  ns_per_clk;

	dgb_print("Assuming %d MHz for the Timing Setup\n",
		  clock / 1000000);

	clock += 5000000; /* +5000000 for rounding */
	ns_per_clk = NANOSEC_PER_SEC / clock;

	iic_prescaler		= clock / 10000000;
	iic_prescaler_dataout	= clock / 10000000;
	iic_glitch_width_dataout = GLITCH_WIDTH_DATA/100;

	if (check_fastmode(adap)) {
		set_ready_fastmode(adap);
		iic_baudrate = clock / (2 * I2C_RATE_FASTMODE);
		iic_glitch_width = 0;
		iic_rep_start_hold  =(REP_START_HOLD_TIME_FAST +GLITCH_WIDTH_DATA) /ns_per_clk;
		iic_rep_start_setup =(REP_START_SETUP_TIME_FAST+GLITCH_WIDTH_CLOCK) /ns_per_clk;
		if(GLITCH_WIDTH_DATA<200)
			iic_start_hold =(START_HOLD_TIME_FAST+GLITCH_WIDTH_DATA) /ns_per_clk;
		else
			iic_start_hold =(5*GLITCH_WIDTH_DATA) /ns_per_clk;
		iic_data_setup =(DATA_SETUP_TIME_FAST+GLITCH_WIDTH_DATA) /ns_per_clk;
		iic_stop_setup =(STOP_SETUP_TIME_FAST+GLITCH_WIDTH_CLOCK) /ns_per_clk;
		iic_bus_free =(BUS_FREE_TIME_FAST+GLITCH_WIDTH_DATA) /ns_per_clk;
	} else {
		clear_ready_fastmode(adap);
		iic_baudrate = clock  / (2 * I2C_RATE_NORMAL);
		iic_glitch_width = (GLITCH_WIDTH_DATA*(clock/10000000))/(iic_prescaler*1000);
		iic_rep_start_hold =( REP_START_HOLD_TIME_NORMAL+GLITCH_WIDTH_DATA) / ns_per_clk;
		iic_rep_start_setup =( REP_START_SETUP_TIME_NORMAL+GLITCH_WIDTH_CLOCK) / ns_per_clk;
		if(GLITCH_WIDTH_DATA<1200)
			iic_start_hold =( START_HOLD_TIME_NORMAL+GLITCH_WIDTH_DATA) / ns_per_clk;
		else
			iic_start_hold =( 5*GLITCH_WIDTH_DATA) / ns_per_clk;
		iic_data_setup =( DATA_SETUP_TIME_NORMAL+GLITCH_WIDTH_DATA) / ns_per_clk;
		iic_stop_setup =( STOP_SETUP_TIME_NORMAL+GLITCH_WIDTH_CLOCK) / ns_per_clk;
		iic_bus_free =( BUS_FREE_TIME_NORMAL+GLITCH_WIDTH_DATA) / ns_per_clk;
	}

	ssc_store32(adap, SSC_BRG,iic_baudrate);
	ssc_store32(adap, SSC_REP_START_HOLD, iic_rep_start_hold);
	ssc_store32(adap, SSC_START_HOLD, iic_start_hold);
	ssc_store32(adap, SSC_REP_START_SETUP, iic_rep_start_setup);
	ssc_store32(adap, SSC_DATA_SETUP, iic_data_setup);
	ssc_store32(adap, SSC_STOP_SETUP, iic_stop_setup);
	ssc_store32(adap, SSC_BUS_FREE, iic_bus_free);
	ssc_store32(adap, SSC_PRE_SCALER_BRG, iic_pre_scale_baudrate);
	ssc_store32(adap, SSC_PRSCALER, iic_prescaler);
	ssc_store32(adap, SSC_NOISE_SUPP_WIDTH, iic_glitch_width);
	ssc_store32(adap, SSC_NOISE_SUPP_WIDTH_DATAOUT, iic_glitch_width_dataout);
	ssc_store32(adap, SSC_PRSCALER_DATAOUT, iic_prescaler_dataout);

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
		dgb_print("ioctl fast 0x%x\n",arg);
		iic_adap->config &= ~IIC_STM_CONFIG_SPEED_MASK;
		if (arg)
			iic_adap->config |=
			    IIC_STM_CONFIG_SPEED_FAST;
		break;
	default:
		printk(KERN_WARNING" %s: i2c-ioctl not managed\n",__FUNCTION__);
	}
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

static int __init iic_stm_probe(struct platform_device *pdev)
{
	struct ssc_pio_t *pio_info =
			(struct ssc_pio_t *)pdev->dev.platform_data;
	struct iic_ssc *i2c_stm;
	struct resource *res;

	i2c_stm = devm_kzalloc(&pdev->dev,sizeof(struct iic_ssc), GFP_KERNEL);

	if (!i2c_stm)
		return -ENOMEM;

	if (!(res=platform_get_resource(pdev, IORESOURCE_MEM, 0)))
		return -ENODEV;
	if (!devm_request_mem_region(&pdev->dev, res->start, res->end - res->start, "i2c")){
		printk(KERN_ERR "%s: Request mem 0x%x region not done\n",__FUNCTION__,res->start);
		return -ENOMEM;
	}
	if (!(i2c_stm->base =
		devm_ioremap_nocache(&pdev->dev, res->start, res->end - res->start))){
		printk(KERN_ERR "%s: Request iomem 0x%x region not done\n",__FUNCTION__,res->start);
		return -ENOMEM;
	}
	if (!(res=platform_get_resource(pdev, IORESOURCE_IRQ, 0))){
		printk(KERN_ERR "%s Request irq %d not done\n",__FUNCTION__,res->start);
		return -ENODEV;
	}
	if(devm_request_irq(&pdev->dev,res->start, iic_state_machine,
		IRQF_DISABLED, "i2c", i2c_stm)<0){
		printk(KERN_ERR "%s: Request irq not done\n",__FUNCTION__);
		return -ENODEV;
	}
	pio_info->clk = stpio_request_pin(pio_info->pio_port,pio_info->pio_pin[0],
				"I2C Clock", STPIO_ALT_BIDIR);
	if(!pio_info->clk){
		printk(KERN_ERR "%s: Faild to clk pin allocation\n",__FUNCTION__);
		return -ENODEV;
	}

	pio_info->sdout = stpio_request_pin(pio_info->pio_port,pio_info->pio_pin[1],
				"I2C Data", STPIO_ALT_BIDIR);
	if(!pio_info->sdout){
		printk(KERN_ERR "%s: Faild to sda pin allocation\n",__FUNCTION__);
		return -ENODEV;
		}
	pdev->dev.driver_data = i2c_stm;
	i2c_stm->adapter.id = I2C_HW_STM_SSC;
	i2c_stm->adapter.timeout = 2;
	i2c_stm->adapter.class   = I2C_CLASS_ALL;
	sprintf(i2c_stm->adapter.name,"i2c-hw-%d",pdev->id);
	i2c_stm->adapter.algo = &iic_stm_algo;
	i2c_stm->adapter.dev.parent = &(pdev->dev);
	iic_stm_setup_timing(i2c_stm,clk_get_rate(clk_get(NULL,"comms_clk")));
	init_waitqueue_head(&(i2c_stm->wait_queue));
	if (i2c_add_adapter(&(i2c_stm->adapter)) < 0) {
		printk(KERN_ERR
		       "%s: The I2C Core refuses the i2c/stm adapter\n",__FUNCTION__);
		return -ENODEV;
	} else {
		device_create_file(&(i2c_stm->adapter.dev), &dev_attr_fastmode);
	}
	return 0;

}

static int iic_stm_remove(struct platform_device *pdev)
{
	struct resource *res;
	struct iic_ssc *iic_stm = pdev->dev.driver_data ;
	struct ssc_pio_t *pio_info =
			(struct ssc_pio_t *)pdev->dev.platform_data;

	i2c_del_adapter(&iic_stm->adapter);
	/* irq */
	res=platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	devm_free_irq(&pdev->dev,res->start,iic_stm);
	/* mem */
	res=platform_get_resource(pdev, IORESOURCE_MEM,0);
	devm_iounmap(&pdev->dev,res->start);
	/* pio */
	stpio_free_pin(pio_info->clk);
	stpio_free_pin(pio_info->sdout);
	/* kmem */
	devm_kfree(&pdev->dev,iic_stm);
	return 0;
}

#ifdef CONFIG_PM
static int iic_stm_suspend(struct platform_device *pdev,pm_message_t state)
{
	struct iic_ssc *i2c_bus = pdev->dev.driver_data;
	ssc_store32(i2c_bus, SSC_IEN,0);
	ssc_store32(i2c_bus, SSC_CTL,0);
	return 0;
}
static int iic_stm_resume(struct platform_device *pdev)
{
	struct iic_ssc *i2c_bus =pdev->dev.driver_data;
	iic_stm_setup_timing(i2c_bus, clk_get_rate(clk_get(NULL,"comms_clk")));
	return 0;
}
#else
#define iic_stm_suspend		NULL
#define	iic_stm_resume		NULL
#endif

static struct platform_driver i2c_stm_driver = {
        .driver.name = "i2c_st",
        .driver.owner = THIS_MODULE,
        .probe = iic_stm_probe,
	.remove = iic_stm_remove,
	.suspend = iic_stm_suspend,
	.resume  = iic_stm_resume,
};


static int __init iic_stm_init(void)
{
	platform_driver_register(&i2c_stm_driver);
	return 0;
}

static void __exit iic_stm_exit(void)
{
	platform_driver_unregister(&i2c_stm_driver);
}

module_init(iic_stm_init);
module_exit(iic_stm_exit);

MODULE_AUTHOR("STMicroelectronics  <www.st.com>");
MODULE_DESCRIPTION("i2c-stm algorithm for STMicroelectronics devices");
MODULE_LICENSE("GPL");
