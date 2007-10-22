/*
 * --------------------------------------------------------------------
 *
 * i2c-stm.c
 * i2c algorithms for STMicroelectronics SSC device
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
#include <linux/errno.h>
#include <linux/preempt.h>
#include <asm/processor.h>
#include <asm/delay.h>
#include "./i2c-stm.h"
#include "../../stm/stm_ssc.h"

#undef dgb_print

#ifdef  CONFIG_I2C_STM_DEBUG
#define dgb_print(fmt, args...)  printk("%s: " fmt, __FUNCTION__ , ## args)
#else
#define dgb_print(fmt, args...)
#endif

/* --- Defines for I2C --- */
#define DEVICE_ID                    0x041175

#define I2C_RATE_NORMAL            100000
#define I2C_RATE_FASTMODE          400000
#define NANOSEC_PER_SEC            1000000000

#define REP_START_HOLD_TIME_NORMAL   4000	/* standard */
#define REP_START_HOLD_TIME_FAST     3500	/* it should be 600 */
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
	unsigned int virtual_configuration;
	iic_state_machine_e start_state;
	iic_state_machine_e state;
	iic_state_machine_e next_state;
	struct i2c_msg *msgs_queue;
	int attempt;
	int queue_length;
	int current_msg;	/* the message on going */
	int idx_current_msg;	/* the byte in the message */
	iic_fsm_error_e status_error;
};

static void iic_algo_stm_setup_timing(struct iic_ssc *adapter);

#define jump_on_fsm_start()  { adap->state = IIC_FSM_START; \
				goto be_fsm_start; }

#define jump_on_fsm_repstart()  { adap->state = IIC_FSM_REPSTART; \
                                  goto be_fsm_repstart; }

#define jump_on_fsm_stop()      { adap->state = IIC_FSM_STOP;     \
                                  goto be_fsm_stop;        }

#define jump_on_fsm_abort()     { adap->state = IIC_FSM_ABORT;    \
                                  goto be_fsm_abort;       }

#define check_fastmode(adap)  ( (adap->virtual_configuration & \
                                 IIC_STM_CONFIG_SPEED_MASK   )!=0 ? 1 : 0 )

void iic_algo_state_machine(struct iic_ssc *adap)
{
	unsigned short status;
	unsigned short tx_fifo_status;
	unsigned short rx_fifo_status;
	unsigned int idx;
	unsigned short address;
	struct i2c_msg *pmsg;
	struct ssc_t *ssc_bus;
	struct device *dev;
	char local_fast_mode;
	union {
		char bytes[2];
		short word;
	} tmp;

	dgb_print("\n");
	dev = adap->adapter.dev.parent;
	ssc_bus = container_of(dev, struct ssc_t, dev);
	local_fast_mode = check_fastmode(adap);
	pmsg = adap->msgs_queue + adap->current_msg;

	adap->state = adap->next_state;

	barrier();
#if defined(CONFIG_CPU_SUBTYPE_STB7100)
	tx_fifo_status = ssc_load16(ssc_bus, SSC_TX_FSTAT);
	rx_fifo_status = ssc_load16(ssc_bus, SSC_RX_FSTAT);
#endif
	switch (adap->state) {
	case IIC_FSM_START:
	      be_fsm_start:
		dgb_print("-Start address 0x%x\n", pmsg->addr);
		adap->start_state = IIC_FSM_START;
		ssc_store16(ssc_bus, SSC_CTL, SSC_CTL_SR | SSC_CTL_EN | 0x1);
		ssc_store16(ssc_bus, SSC_BRG,
			    (adap->virtual_configuration &
			     IIC_STM_CONFIG_BAUDRATE_MASK) >> 16);
		ssc_store16(ssc_bus, SSC_CTL,
			    SSC_CTL_EN | SSC_CTL_MS |
			    SSC_CTL_PO | SSC_CTL_PH | SSC_CTL_HB | 0x8);
		ssc_store16(ssc_bus, SSC_CLR, 0xdc0);
		ssc_store16(ssc_bus, SSC_I2C, SSC_I2C_I2CM |
			    (SSC_I2C_I2CFSMODE * local_fast_mode));
		address = (pmsg->addr << 2) | 0x1;
		adap->next_state = IIC_FSM_DATA_WRITE;
		if (pmsg->flags & I2C_M_RD) {
			address |= 0x2;
			adap->next_state = IIC_FSM_PREPARE_2_READ;
		}
		adap->idx_current_msg = 0;
		ssc_store16(ssc_bus, SSC_IEN, SSC_IEN_NACKEN | SSC_IEN_TEEN);
		ssc_store16(ssc_bus, SSC_TBUF, address);
		ssc_store16(ssc_bus, SSC_I2C, SSC_I2C_I2CM |
			    SSC_I2C_STRTG | SSC_I2C_TXENB |
			    (SSC_I2C_I2CFSMODE * local_fast_mode));
		break;
	case IIC_FSM_PREPARE_2_READ:
		/* Just to clear th RBUF */
		ssc_load16(ssc_bus, SSC_RBUF);
		status = ssc_load16(ssc_bus, SSC_STA);
		dgb_print(" Prepare to Read... Status=0x%x\n", status);
		if (status & SSC_STA_NACK)
			jump_on_fsm_abort();
		adap->next_state = IIC_FSM_DATA_READ;
		if (!pmsg->len) {
			dgb_print("Zero Read\n");
			jump_on_fsm_stop();
		}
		ssc_store16(ssc_bus, SSC_TBUF, 0x1ff);
		if (pmsg->len == 1) {
			ssc_store16(ssc_bus, SSC_IEN, SSC_IEN_NACKEN);
			ssc_store16(ssc_bus, SSC_I2C, SSC_I2C_I2CM |
				    (SSC_I2C_I2CFSMODE * local_fast_mode));
		} else {
			ssc_store16(ssc_bus, SSC_I2C, SSC_I2C_I2CM |
				    SSC_I2C_ACKG |
				    (SSC_I2C_I2CFSMODE * local_fast_mode));
			ssc_store16(ssc_bus, SSC_IEN, SSC_IEN_RIEN);
		}
		break;
	case IIC_FSM_DATA_READ:
		status = ssc_load16(ssc_bus, SSC_STA);
		if (!(status & SSC_STA_TE))
			return;
		tmp.word = ssc_load16(ssc_bus, SSC_RBUF);
		tmp.word = tmp.word >> 1;
		pmsg->buf[adap->idx_current_msg++] = tmp.bytes[0];
		dgb_print(" Data Read...Status=0x%x %d-%c\n",
			  status, tmp.bytes[0], tmp.bytes[0]);
		/*Did we finish? */
		if (adap->idx_current_msg == pmsg->len) {
			status &= ~SSC_STA_NACK;
			jump_on_fsm_stop();
		} else {
			ssc_store16(ssc_bus, SSC_TBUF, 0x1ff);
			/*Is this the last byte? */
			if (adap->idx_current_msg == (pmsg->len - 1)) {
				ssc_store16(ssc_bus, SSC_I2C, SSC_I2C_I2CM |
					    (SSC_I2C_I2CFSMODE *
					     local_fast_mode));
				ssc_store16(ssc_bus, SSC_IEN, SSC_IEN_NACKEN);
			}
		}
		break;
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
			jump_on_fsm_abort();
		if (adap->idx_current_msg == pmsg->len || !(pmsg->len))
			jump_on_fsm_stop();;
		dgb_print(" Data Write...Status=0x%x 0x%x-%c\n", status,
			  pmsg->buf[adap->idx_current_msg],
			  pmsg->buf[adap->idx_current_msg]);
		ssc_store16(ssc_bus, SSC_I2C, SSC_I2C_I2CM | SSC_I2C_TXENB |
			    (SSC_I2C_I2CFSMODE * local_fast_mode));

		adap->next_state = IIC_FSM_DATA_WRITE;
		ssc_store16(ssc_bus, SSC_IEN, SSC_IEN_TEEN);

		tmp.bytes[0] = pmsg->buf[adap->idx_current_msg++];
		ssc_store16(ssc_bus, SSC_TBUF, tmp.word << 1 | 0x1);
		break;

	case IIC_FSM_ABORT:
	      be_fsm_abort:
		dgb_print(" Abort\n");
		adap->status_error |= IIC_E_NOTACK;
		/* Don't ADD the break */

	case IIC_FSM_STOP:
	      be_fsm_stop:
		if (!(status & SSC_STA_NACK) &&
		    (++adap->current_msg < adap->queue_length)) {
			jump_on_fsm_repstart();
		}
		dgb_print(" Stop\n");
		ssc_store16(ssc_bus, SSC_CLR, 0xdc0);
		ssc_store16(ssc_bus, SSC_I2C, SSC_I2C_I2CM |
			    SSC_I2C_TXENB | SSC_I2C_STOPG |
			    (SSC_I2C_I2CFSMODE * local_fast_mode));
		adap->next_state = IIC_FSM_COMPLETE;
		ssc_store16(ssc_bus, SSC_IEN, SSC_IEN_STOPEN);
		break;

	case IIC_FSM_COMPLETE:
		dgb_print(" Complete\n");
		ssc_store16(ssc_bus, SSC_IEN, 0x0);
		ssc_store16(ssc_bus, SSC_I2C, 0x0);
/*
 *  If there was some problem i can try again for adap->adapter.retries time...
 */
		if ((adap->status_error & IIC_E_NOTACK) &&	/* there was a problem */
		    adap->start_state == IIC_FSM_START &&	/* it cames from start state */
		    adap->idx_current_msg == 0 &&	/* the problem is on address */
		    ++adap->attempt <= adap->adapter.retries) {
			adap->status_error = 0;
			jump_on_fsm_start();
		}
		if (!(adap->status_error & IIC_E_NOTACK))
			adap->status_error = IIC_E_NO_ERROR;
		wake_up(&(ssc_bus->wait_queue));
		break;
	case IIC_FSM_REPSTART:
	      be_fsm_repstart:
		pmsg = adap->msgs_queue + adap->current_msg;
		dgb_print("-Rep Start addr 0x%x\n", pmsg->addr);
		adap->start_state = IIC_FSM_REPSTART;
		adap->idx_current_msg = 0;
		adap->next_state = IIC_FSM_REPSTART_ADDR;
		ssc_store16(ssc_bus, SSC_CLR, 0xdc0);
		ssc_store16(ssc_bus, SSC_I2C, SSC_I2C_I2CM
			    | SSC_I2C_REPSTRTG | (SSC_I2C_I2CFSMODE *
						  local_fast_mode));
		ssc_store16(ssc_bus, SSC_IEN, SSC_IEN_REPSTRTEN);
		break;
	case IIC_FSM_REPSTART_ADDR:
		ssc_store16(ssc_bus, SSC_CLR, 0xdc0);
		ssc_store16(ssc_bus, SSC_I2C, SSC_I2C_I2CM | SSC_I2C_TXENB |
			    (SSC_I2C_I2CFSMODE * local_fast_mode));
		address = (pmsg->addr << 2) | 0x1;
		adap->next_state = IIC_FSM_DATA_WRITE;
		if (pmsg->flags & I2C_M_RD) {
			address |= 0x2;
			adap->next_state = IIC_FSM_PREPARE_2_READ;
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

/*
  Description: Prepares the controller for a transaction
*/
static int iic_algo_stm_xfer(struct i2c_adapter *i2c_adap,
			     struct i2c_msg msgs[], int num)
{
	struct iic_ssc *adap;
	struct ssc_t *ssc_bus;
	struct device *dev;
	unsigned int local_flag;
	int result;

	dgb_print("\n");
	adap = container_of(i2c_adap, struct iic_ssc, adapter);
	dev = i2c_adap->dev.parent;
	ssc_bus = container_of(dev, struct ssc_t, dev);

	/* Here i have to prepare all the environment */
	adap->msgs_queue = msgs;
	adap->queue_length = num;
	adap->current_msg = 0x0;
	adap->attempt = 0x0;
	adap->status_error = IIC_E_RUNNING;
	adap->next_state = IIC_FSM_START;

	ssc_request_bus(ssc_bus, iic_algo_state_machine, adap);
/*
 * check if the i2c timing register
 * of ssc are ready to use
*/
	if (!(ssc_bus->i2c_timing == SSC_I2C_READY_NORMAL &&
	      !(check_fastmode(adap))
	      ||
	      ssc_bus->i2c_timing == SSC_I2C_READY_FAST &&
	      check_fastmode(adap)))
		iic_algo_stm_setup_timing(adap);

	local_irq_save(local_flag);
	iic_algo_state_machine(adap);
	interruptible_sleep_on_timeout(&(ssc_bus->wait_queue),
				       i2c_adap->timeout * num * HZ);

	result = adap->current_msg;

	if (adap->status_error != IIC_E_NO_ERROR) {	/* There was some problem */
		if (adap->status_error == IIC_E_RUNNING) {	/* There was a timeout !!! */
			/* if there was a timeout we have to
			   - disable the interrupt
			   - generate a stop condition on the bus
			   all this task are done without interrupt....
			 */
			ssc_store16(ssc_bus, SSC_IEN, 0x0);
			ssc_store16(ssc_bus, SSC_I2C, SSC_I2C_I2CM |
				    SSC_I2C_STOPG | SSC_I2C_TXENB |
				    (SSC_I2C_I2CFSMODE * check_fastmode(adap)));
			/* wait until the ssc detects a Stop condition on the bus */
			while((ssc_load16(ssc_bus,SSC_STA) & SSC_STA_STOP) == 0 );
			/* tourn off the ssc */
			ssc_store16(ssc_bus, SSC_I2C, SSC_I2C_I2CM | SSC_I2C_TXENB);
			printk(KERN_ERR
			       "stm-i2c: Error timeout in the finite state machine\n");
		}
		result = -EREMOTEIO;
	}
	local_irq_restore(local_flag);

	while((ssc_load16(ssc_bus,SSC_STA) & SSC_STA_BUSY) != 0 );

		ndelay(BUS_FREE_TIME_FAST);

   if (!check_fastmode(adap))
	ndelay(BUS_FREE_TIME_NORMAL-BUS_FREE_TIME_FAST);

	ssc_release_bus(ssc_bus);

	return result;
}

#ifdef CONFIG_I2C_STM_DEBUG
static void iic_algo_stm_timing_trace(struct iic_ssc *adap)
{
	struct device *dev = adap->adapter.dev.parent;
	struct ssc_t *ssc_bus = container_of(dev, struct ssc_t, dev);
	dgb_print("SSC_BRG  %d\n", ssc_load16(ssc_bus, SSC_BRG));
	dgb_print("SSC_REP_START_HOLD %d\n",
		  ssc_load16(ssc_bus, SSC_REP_START_HOLD));
	dgb_print("SSC_REP_START_SETUP %d\n",
		  ssc_load16(ssc_bus, SSC_REP_START_SETUP));
	dgb_print("SSC_START_HOLD %d\n", ssc_load16(ssc_bus, SSC_START_HOLD));
	dgb_print("SSC_DATA_SETUP %d\n", ssc_load16(ssc_bus, SSC_DATA_SETUP));
	dgb_print("SSC_STOP_SETUP %d\n", ssc_load16(ssc_bus, SSC_STOP_SETUP));
	dgb_print("SSC_BUS_FREE %d\n", ssc_load16(ssc_bus, SSC_BUS_FREE));

#ifdef CONFIG_CPU_SUBTYPE_STB7100
	dgb_print("SSC_PRE_SCALER_BRG %d\n",
		  ssc_load16(ssc_bus, SSC_PRE_SCALER_BRG));
#endif
	dgb_print("SSC_AGFR 0x%x\n", ssc_load8(ssc_bus, SSC_AGFR));
	dgb_print("SSC_PRSC %d\n", ssc_load8(ssc_bus, SSC_PRSC));
}
#endif

static void iic_algo_stm_setup_timing(struct iic_ssc *adap)
{
	struct device *dev = adap->adapter.dev.parent;
	struct ssc_t *ssc_bus = container_of(dev, struct ssc_t, dev);
	unsigned long iic_baudrate;
	unsigned short iic_rep_start_hold;
	unsigned short iic_start_hold, iic_rep_start_setup;
	unsigned short iic_data_setup, iic_stop_setup;
	unsigned short iic_bus_free, iic_pre_scale_baudrate;
	unsigned char iic_agfr, iic_prsc;
	unsigned long NSPerCyc = NANOSEC_PER_SEC / ssc_get_clock();

	dgb_print("Assuming %d MHz for the Timing Setup\n",
		  ssc_get_clock() / 1000000);

	iic_agfr = 0x0;
	iic_prsc = (int)ssc_get_clock() / 10000000;
	iic_pre_scale_baudrate = 0x1;

	if (check_fastmode(adap)) {
		ssc_bus->i2c_timing = SSC_I2C_READY_FAST;
		iic_baudrate = ssc_get_clock()
		    / (2 * I2C_RATE_FASTMODE);
		iic_rep_start_hold = REP_START_HOLD_TIME_FAST / NSPerCyc;
		iic_start_hold = START_HOLD_TIME_FAST / NSPerCyc;
		iic_rep_start_setup = REP_START_SETUP_TIME_FAST / NSPerCyc;
		iic_data_setup = DATA_SETUP_TIME_FAST / NSPerCyc;
		iic_stop_setup = STOP_SETUP_TIME_FAST / NSPerCyc;
		iic_bus_free = BUS_FREE_TIME_FAST / NSPerCyc;
	} else {
		ssc_bus->i2c_timing = SSC_I2C_READY_NORMAL;
		iic_baudrate = ssc_get_clock()
		    / (2 * I2C_RATE_NORMAL);
		iic_rep_start_hold = REP_START_HOLD_TIME_NORMAL / NSPerCyc;
		iic_start_hold = START_HOLD_TIME_NORMAL / NSPerCyc;
		iic_rep_start_setup = REP_START_SETUP_TIME_NORMAL / NSPerCyc;
		iic_data_setup = DATA_SETUP_TIME_NORMAL / NSPerCyc;
		iic_stop_setup = STOP_SETUP_TIME_NORMAL / NSPerCyc;
		iic_bus_free = BUS_FREE_TIME_NORMAL / NSPerCyc;
	}

	adap->virtual_configuration =
	    (adap->virtual_configuration & ~IIC_STM_CONFIG_BAUDRATE_MASK);
	adap->virtual_configuration |= iic_baudrate << 16;

	ssc_store16(ssc_bus, SSC_REP_START_HOLD, iic_rep_start_hold);
	ssc_store16(ssc_bus, SSC_START_HOLD, iic_start_hold);
	ssc_store16(ssc_bus, SSC_REP_START_SETUP, iic_rep_start_setup);
	ssc_store16(ssc_bus, SSC_DATA_SETUP, iic_data_setup);
	ssc_store16(ssc_bus, SSC_STOP_SETUP, iic_stop_setup);
	ssc_store16(ssc_bus, SSC_BUS_FREE, iic_bus_free);
	ssc_store8(ssc_bus, SSC_AGFR, iic_agfr);
	ssc_store8(ssc_bus, SSC_PRSC, iic_prsc);

#ifdef CONFIG_CPU_SUBTYPE_STB7100
	ssc_store16(ssc_bus, SSC_PRE_SCALER_BRG, iic_pre_scale_baudrate);
#endif
#ifdef CONFIG_I2C_STM_DEBUG
	iic_algo_stm_timing_trace(adap);
#endif
	return;
}

static int iic_algo_stm_control(struct i2c_adapter *adapter,
				unsigned int cmd, unsigned long arg)
{
	struct iic_ssc *iic_adap =
	    container_of(adapter, struct iic_ssc, adapter);

	if (cmd == I2C_STM_IOCTL_FAST) {
		dgb_print("IOCTL Fast\n");
		iic_adap->virtual_configuration &= ~IIC_STM_CONFIG_SPEED_MASK;
		if (arg)
			iic_adap->virtual_configuration |=
			    IIC_STM_CONFIG_SPEED_FAST;
	}
/*
 * the timeout and he retries ioctl
 * are managed by i2c core system
 */
	return 0;
}

static u32 iic_algo_stm_func(struct i2c_adapter *adap)
{
	return I2C_FUNC_I2C | I2C_FUNC_SMBUS_EMUL;
}

static struct i2c_algorithm iic_stm_algo = {
	.master_xfer = iic_algo_stm_xfer,
	.functionality = iic_algo_stm_func,
	.algo_control = iic_algo_stm_control
};

static LIST_HEAD(stm_busses);

struct stm_adapter {
	struct iic_ssc iic_adap;
	struct list_head stm_list;
};

static int __init iic_stm_bus_init()
{
	unsigned int ssc_number = ssc_device_available();
	unsigned int idx;
	unsigned int adapnr = 0;
	struct stm_adapter *st_adapter;
	struct iic_ssc *iic_stm;

	for (idx = 0; idx < ssc_number; ++idx) {
		if (!(ssc_capability(idx) & SSC_I2C_CAPABILITY))
			continue;
		st_adapter =
		    (struct stm_adapter *)kmalloc(sizeof(struct stm_adapter),
						  GFP_KERNEL);
		if (!st_adapter) {
			printk(KERN_EMERG
			       "Error on initialization of  ssc-i2c adapter module\n");
			return -ENODEV;
		}
		iic_stm = &(st_adapter->iic_adap);
		iic_stm->virtual_configuration &= ~IIC_STM_CONFIG_SPEED_MASK;
		memset(&(iic_stm->adapter), 0, sizeof(struct i2c_adapter));
		iic_stm->adapter.owner = THIS_MODULE;
		iic_stm->adapter.id = adapnr;
		iic_stm->adapter.timeout = 4;
		iic_stm->adapter.retries = 0;
		iic_stm->adapter.class   = I2C_CLASS_ALL;
		sprintf(iic_stm->adapter.name,"i2c-ssc-%d",adapnr);
		iic_stm->adapter.algo = &iic_stm_algo;
		iic_stm->adapter.dev.bus = &i2c_bus_type;
		iic_stm->adapter.dev.parent = &(ssc_device_request(idx)->dev);
		iic_algo_stm_setup_timing(iic_stm);

		if (i2c_add_adapter(&(iic_stm->adapter)) < 0) {
			printk(KERN_ERR
			       "i2c/stm: The I2C Core refuses the i2c/stm adapter\n");
			kfree(st_adapter);
			return -ENODEV;
		}
		list_add(&(st_adapter->stm_list), &(stm_busses));
		adapnr ++;
	}
	return 0;
}

static void __exit iic_stm_bus_exit(void)
{
	struct stm_adapter *st_adapter;
	struct i2c_adapter *iic_adapter;
	struct list_head *item;
	dgb_print("\n");
	list_for_each(item, &(stm_busses)) {
		st_adapter = container_of(item, struct stm_adapter, stm_list);
		list_del(&st_adapter->stm_list);
		iic_adapter = &(st_adapter->iic_adap.adapter);
		i2c_del_adapter(iic_adapter);
		kfree(st_adapter);
	}
}

late_initcall(iic_stm_bus_init);

module_exit(iic_stm_bus_exit);

MODULE_AUTHOR("STMicroelectronics  <www.st.com>");
MODULE_DESCRIPTION("i2c-stm algorithm for ssc device");
MODULE_LICENSE("GPL");
