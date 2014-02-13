/*
 * <root>/drivers/stm/lpm_mb.c
 *
 * This driver implements communication with internal Standby Controller
 * over mailbox interface in some STMicroelectronics devices.
 *
 * Copyright (C) 2012 STMicroelectronics Limited.
 *
 * Contributor:Francesco Virlinzi <francesco.virlinzi@st.com>
 * Author:Pooja Agarwal <pooja.agarwal@st.com>
 * Author:Udit Kumar <udit-dlh.kumar@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public License.
 * See linux/COPYING for more information.
 */

#include <linux/stm/lpm.h>
#include <linux/platform_device.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/stm/platform.h>
#include <linux/firmware.h>
#include "lpm_def.h"
#include <linux/libelf.h>
#include <asm/unaligned.h>


#ifdef CONFIG_STM_LPM_DEBUG
#define lpm_debug(fmt, ...) pr_debug(fmt, ##__VA_ARGS__)
#else
#define lpm_debug(fmt, ...)
#endif

enum stm_lpm_pio_level {
	STM_LPM_PIO_LOW		= 0,
	STM_LPM_PIO_HIGH	= 1 << 7
};

enum stm_lpm_pio_direction {
	STM_LPM_PIO_INPUT	= 0,
	STM_LPM_PIO_OUTPUT	= 1 << 5
};

/**
 * stm_lpm_driver_data - Local struct of driver
 * @lpm_mem_base[3]:	memory region mapped by driver
 * 		lpm_mem_base[0] is SBC program and data base address
 * 		size of lpm_mem_base[0] is 0xA0000
 *		lpm_mem_base[1] is SBC mailbox address
 *		size of lpm_mem_base[1] is 0x400
 * 		lpm_mem_base[2] is SBC configuration  address
 * 		size of lpm_mem_base[1] is 0x200
 * @fw_reply_msg:	reply message from firmware
 * @fw_request_msg:	firmware request message
 * @fw_name:	Name of firmware
 * @reply_from_sbc:	reply from SBC, true in case reply received
 * @fw_major_ver:	SBC's firmware Protocol major version number
 * @stm_lpm_wait_queue:	work queue
 * @msg_protection_mutex:	message protection mutex
 * @glbtrans_id:	global transaction id used in communication
 * @lpm_sbc_reply_work:	work struct
 * @sbc_state:	State of SBC firmware
 *
 * We need to keep three addresses because in memory map of SBC,
 * keyscan and HDMI lies in between above address range and there are
 * other driver which map HDMI and keyscan memory.
 */

struct stm_lpm_driver_data {
	void * __iomem lpm_mem_base[3];
	struct platform_device *pdev;
	struct lpm_message fw_reply_msg;
	struct lpm_message fw_request_msg;
	char fw_name[20];
	int reply_from_sbc;
	char fw_major_ver;
	wait_queue_head_t stm_lpm_wait_queue;
	struct mutex msg_protection_mutex;
	unsigned char glbtrans_id;
	struct work_struct lpm_sbc_reply_work;
	enum stm_lpm_sbc_state sbc_state;
};

static struct stm_lpm_driver_data *lpm_drv;

/* Work queue to process SBC firmware request */
static void lpm_sbc_reply_worker(struct work_struct *work);

/**
 * lpm_send_big_message() - To send big message over SBC DMEM
 * @size:	size of message
 * @sbc_msg:	buffer pointer
 *
 * This function is used to send large messages(>LPM_MAX_MSG_DATA)
 * using SBC DMEM.
 *
 * Return - 0 on success
 * Return - negative error code on failure.
 */

static int lpm_send_big_message(u16 size, const char *sbc_msg)
{
	int err = 0;
	unsigned int offset;
	char msg[6];
	struct lpm_internal_send_msg send_msg = {
		.command_id = LPM_MSG_LGWR_OFFSET,
		.msg = msg,
		.msg_size = 2
	};
	struct lpm_message response = {0};
	put_unaligned_le16(size, msg);
	err = lpm_exchange_msg(&send_msg, &response);
	if (likely(err == 0)) {
		/* Get the offset in SBC memory */
		offset = get_unaligned_le32(&response.msg_data[2]);
		/* Copy message in SBC memory */
		memcpy_toio(lpm_drv->lpm_mem_base[0] + DATA_OFFSET + offset ,
		sbc_msg, size);
		/* Send this big message */
		put_unaligned_le16(size, msg);
		put_unaligned_le32(offset, &msg[2]);
		send_msg.command_id = LPM_MSG_BKBD_WRITE;
		send_msg.msg_size = 6;
		err = lpm_exchange_msg(&send_msg, &response);
	}
	return err;
}

/**
 * stm_lpm_setup_ir() - To set ir key setup
 * @num_keys:   Number of IR keys
 * @ir_key_info:        Information of IR keys
 *
 * This function will configure IR information on SBC firmware.
 * User needs to pass on which IR keys wakeup is required and
 * the expected pattern for those keys.
 *
 * Return - 0 on success
 * Return - negative error code on failure.
*/

int stm_lpm_setup_ir(u8 num_keys, struct stm_lpm_ir_keyinfo *ir_key_info)
{
	struct stm_lpm_ir_keyinfo *this_key;
	u16 ir_size;
	char *buf, *org_buf;
	int count, i, err = 0;

	for (count = 0; count < num_keys; count++) {
		struct stm_lpm_ir_key *key_info;
		this_key = ir_key_info;

		ir_key_info++;
		key_info = &this_key->ir_key;
		/* Check key crediantials */
		if (unlikely(this_key->time_period == 0 ||
				key_info->num_patterns >= 64))
			return -EINVAL;

		ir_size = key_info->num_patterns*2 + 12;
		buf = kmalloc(ir_size, GFP_KERNEL);
		org_buf = buf;

		/* Fill buffer */
		*buf++ = LPM_MSG_SET_IR;
		*buf++ = 0;
		*buf++ = this_key->ir_id & 0xF;
		*buf++ = ir_size;
		*buf++ = this_key->time_period & 0xFF;
		*buf++ = (this_key->time_period >> 8) & 0xFF;
		*buf++ = this_key->time_out & 0xFF;
		*buf++ = (this_key->time_out >> 8) & 0xFF;

		if (!this_key->tolerance)
			this_key->tolerance = 10;
		*buf++ = this_key->tolerance;
		*buf++ = key_info->key_index & 0xF;
		*buf++ = key_info->num_patterns;
		/* Now compress the actual data and copy */
		buf = org_buf + 12;
		for (i = 0; i < key_info->num_patterns ; i++) {
			key_info->fifo[i].mark /= this_key->time_period;
			*buf++ = key_info->fifo[i].mark;
			key_info->fifo[i].symbol /=  this_key->time_period;
			*buf++ = key_info->fifo[i].symbol;
		}
		err = lpm_send_big_message(ir_size, org_buf);
		kfree(org_buf);
		if (err < 0)
			break;
	}
	return err;
}
EXPORT_SYMBOL(stm_lpm_setup_ir);

/**
 * stm_lpm_get_wakeup_info() - To get additional info about wakeup device
 * @wakeupdevice:	wakeup device id
 * @validsize:	read valid size will be returned
 * @datasize:	data size to read
 * @data:	data pointer
 *
 * This API will return additional data for wakeup device if required.
 *
 * Return - 0 on success if data read from SBC is <= datasize
 * Return - 1 if data available with SBC is > datasize
 * Return - negative error on failure
*/

int stm_lpm_get_wakeup_info(enum stm_lpm_wakeup_devices *wakeupdevice,
			u16 *validsize, u16 datasize, char *data)
{
	int err = 0;
	unsigned int offset;
	char msg[3];
	struct lpm_internal_send_msg send_msg = {
		.command_id = LPM_MSG_GET_IRQ,
		.msg = msg,
		.msg_size = 3
	};
	struct lpm_message response = {0};
	msg[0] = *wakeupdevice;

	/* Copy size requested */
	put_unaligned_le16(datasize, &msg[1]);
	err = lpm_exchange_msg(&send_msg, &response);
	if (unlikely(err < 0))
		goto exit;

	/* Two response are possible*/
	if (response.command_id == LPM_MSG_BKBD_READ) {
		/*
		 * If SBC replied to read response from its DMEM then
		 * get the offset to read from SBC memory.
		 */
		offset = get_unaligned_le32(&response.msg_data[2]);
		/* Get valid size from SBC */
		*validsize = lpm_read8(lpm_drv, DATA_OFFSET + offset + 2);
		*validsize |= lpm_read8(lpm_drv, DATA_OFFSET + offset + 3) << 8;
		/* Check if bit#15 is set */
		if (*validsize & MASK_BIT_MASK)
			err = 1;
		*validsize &= M_BIT_MASK;
		/*
		 * Below condition is not possible
		 * SBC have to provide data less than or equal to datasize
		 * Added below check, if some bug pops up in firmware
		 */
		if (unlikely(*validsize > datasize))
			*validsize = datasize;

		memcpy_fromio(data, lpm_drv->lpm_mem_base[0] + DATA_OFFSET +
					offset + 4, *validsize);
	} else {
		*validsize = get_unaligned_le16(response.msg_data);
		/* Check if bit#15 is set in mailbox */
		if (*validsize & MASK_BIT_MASK)
			err = 1;
		*validsize &= M_BIT_MASK;
		/*
		 * Below condition is not possible
		 * SBC have to provide data less than or equal to datasize
		 * Added below check, if some bug pops up in firmware
		 */
		if (unlikely(*validsize > datasize))
			*validsize = datasize;
		/* Copy data to user */
		memcpy(data, &response.msg_data[2], *validsize);

	}
exit:
	return err;
}
EXPORT_SYMBOL(stm_lpm_get_wakeup_info);

/**
 * stm_lpm_reset() - To reset part of full SOC
 * @reset_type:	type of reset
 *
 * Return - 0 on success
 * Return - negative error on failure
*/

int stm_lpm_reset(enum stm_lpm_reset_type reset_type)
{
	int err = 0;
	char msg = reset_type;
	struct lpm_internal_send_msg send_msg = {
		.command_id = LPM_MSG_GEN_RESET,
		.msg = &msg,
		.msg_size = 1
	};
	err = lpm_exchange_msg(&send_msg, NULL);
	if (err == 0 && reset_type == STM_LPM_SBC_RESET) {
		/* Set the firmware as booting */
		int i = 0;
		mutex_lock(&lpm_drv->msg_protection_mutex);
		lpm_drv->sbc_state = STM_LPM_SBC_BOOT;
		mutex_unlock(&lpm_drv->msg_protection_mutex);
		/* Wait till 1 second to get response from firmware */
		do {
			mdelay(100);
			err = stm_lpm_get_fw_state(&lpm_drv->sbc_state);
			if (err < 0 ||
				lpm_drv->sbc_state == STM_LPM_SBC_RUNNING)
				break;
			i++;
		} while (i != 10);
	}
	return err;
}
EXPORT_SYMBOL(stm_lpm_reset);

/**
 * lpm_fw_proto_version() - To get firmware major protocol version
 *
 * return major protocol version of firmware
*/

char lpm_fw_proto_version(void)
{
	return lpm_drv->fw_major_ver;
}

/**
 * lpm_isr() - Mailbox ISR
 * @this_irq:	irq
 * @params:	Parameters
 *
 * This ISR is invoked when there is some message from SBC.
 * Message could be reply or some request.
 * If this a request then such message will be posted to a work queue.
 *
 */

static irqreturn_t lpm_isr(int this_irq, void *params)
{
	struct stm_lpm_driver_data *lpm_drv_p;
	u32 msg_read[4], i;
	struct lpm_message *msg;
	char *msg_p;
	lpm_drv_p = (struct stm_lpm_driver_data *)params;

	/*
	 * Read the data from mailbox
	 * SBC will always be in little endian mode
	 * if host is in big endian then reverse int
	 */
	for (i = 0; i < 4; i++) {
		msg_read[i] = lpm_read32(lpm_drv_p, 1, MBX_READ_STATUS1 + i*4);
		msg_read[i] = cpu_to_le32(msg_read[i]);
		}
	/* Copy first message to check if it's reply from SBC or request */
	msg_p = (char *) &msg_read[0];
	/* Check if reply from SBC or request from SBC */
	if ((*msg_p & LPM_MSG_REPLY) ||
	   ((*msg_p && LPM_MSG_BKBD_READ))) {
		msg = &lpm_drv_p->fw_reply_msg;
		lpm_drv_p->reply_from_sbc = 1;
	} else {
		msg = &lpm_drv_p->fw_request_msg;
	}
	/* Copy mailbox data into local structure */
	memcpy(msg, &msg_read, 16);

	/* Signal work queue or API caller depending upon message from SBC */
	if (lpm_drv_p->reply_from_sbc == 1)
		wake_up_interruptible(&lpm_drv_p->stm_lpm_wait_queue);
	else
		schedule_work(&lpm_drv_p->lpm_sbc_reply_work);

	 /* Clear mail box */
	lpm_write32(lpm_drv_p, 1, MBX_READ_CLR_STATUS1, 0xFFFFFFFF);
	return IRQ_HANDLED;
}

/**
 * lpm_sbc_reply_worker() - When SBC wants some data from Host
 * @work:	work
 *
 * This work queue will be signaled when SBC needs some information from Host.
 * Reply to firmware will be sent over mailbox.
 */

static void lpm_sbc_reply_worker(struct work_struct *work)
{
	unsigned char msg[5];
	struct lpm_internal_send_msg send_msg = {
		.msg = msg
	};
	struct lpm_message *msg_p;
	char msg_size, msg_id;
	msg_p = &lpm_drv->fw_request_msg;
	lpm_debug("Send reply to firmware \n");
	lpm_debug("recd command id %x \n", msg_p->command_id);
	if (msg_p->command_id == LPM_MSG_VER) {
		/* In case firmware requested driver version*/
		msg[0] = LPM_MAJOR_PROTO_VER << 4;
		msg[0] |= LPM_MINOR_PROTO_VER;
		msg[1] = LPM_MAJOR_SOFT_VER << 4;
		msg[1] |= LPM_MINOR_SOFT_VER;
		msg[2] = LPM_PATCH_SOFT_VER << 4;
		msg[2] |= LPM_BUILD_MONTH;
		msg[3] = LPM_BUILD_DAY;
		msg[4] = LPM_BUILD_YEAR;
		msg_size = 5;
		msg_id = LPM_MSG_VER | LPM_MSG_REPLY;
	} else {
		/* Send reply to SBC as error*/
		msg[0] = msg_p->command_id;
		msg[1] = -EINVAL;
		msg_size = 2;
		msg_id = LPM_MSG_ERR;
	}
	send_msg.command_id = msg_id;
	send_msg.msg_size = msg_size;
	send_msg.trans_id = msg_p->transaction_id;
	lpm_exchange_msg(&send_msg, NULL);
	msg_p->command_id = 0;
}

/**
 * lpm_send_msg() - Send mailbox message
 * @msg:	message pointer
 * @msg_size:	message size
 *
 * Return - 0 if firmware is running
 * Return - -EREMOTEIO either firmware is not loaded or not running
 */

static int lpm_send_msg(struct lpm_message *msg,
				unsigned char msg_size)
{
	int err = 0, count;
	u32 *tmp_i = (u32 *)msg;
	/* Check if firmware is loaded or not */
	if (!(lpm_drv->sbc_state == STM_LPM_SBC_RUNNING ||
		lpm_drv->sbc_state == STM_LPM_SBC_BOOT))
		return -EREMOTEIO;

	/*
	 * Write data to mailbox, covert data into LE format.
	 * also mailbox is 4 byte deep, we need to write 4 byte always
	 *
	 * First byte of message is used to generate interrupt as well as
	 * serve as command id.
	 * Therefore first four byte of message part are written at last.
	 */
	for (count = (msg_size+1)/4; count >= 0; count--) {
			*(tmp_i+count) = cpu_to_le32(*(tmp_i+count));
			lpm_write32(lpm_drv, 1, (MBX_WRITE_STATUS1 + (count*4)),
			*(tmp_i+count));
	}
	return err;
}

/**
 * lpm_get_response() - To get SBC response
 * @response:	response of SBC
 *
 * This function is to get SBC response in polling mode
 * This will be called when interrupts are disabled and we
 * still need to get response from SBC.
 *
 * Return - 1 on success
 * Return - 0 when SBC firmware is not responding
 */

static int lpm_get_response(struct lpm_message *response)
{
	int count, i;
	u32 msg_read1[4];
	/* Poll time of 1 Second is good enough to see SBC reply */
	for (count = 0; count < 100; count++) {
		msg_read1[0] = lpm_read32(lpm_drv, 1, MBX_READ_STATUS1);
		msg_read1[0] = cpu_to_le32(msg_read1[0]);
		/* If we received a reply then break the loop */
		if (msg_read1[0] & 0xFF)
			break;
		mdelay(10);
	}

	/* If no reply within 1 second then firmware is not responding */
	if (count == 100) {
		pr_err("count %d value %x \n", count, msg_read1[0]);
		return 0;
	}
	/* Get other data from mailbox */
	for (i = 1; i < 4; i++) {
		msg_read1[i] = lpm_read32(lpm_drv, 1, MBX_READ_STATUS1 + i*4);
		msg_read1[i] = cpu_to_le32(msg_read1[i]);
	}
	/* Copy data received from mailbox*/
	memcpy(&lpm_drv->fw_reply_msg, (void *)msg_read1, 16);
	lpm_write32(lpm_drv, 1, MBX_READ_CLR_STATUS1, 0xFFFFFFFF);
	return 1;
}

/**
 * lpm_exchange_msg() - Internal function  used for message exchange with SBC
 * @send_msg:	message to send
 * @response:	response from SBC firmware
 *
 * This function can be called in three contexts
 * One when reply from SBC is expected for this command
 * Second when reply from SBC is not expected
 * Third when called from interrupt disabled but reply is expected
 *
 * Return - 0 on success
 * Return - negative error code on failure.
*/

int lpm_exchange_msg(struct lpm_internal_send_msg *send_msg,
		struct lpm_message *response)
{
	struct lpm_message lpm_msg = {0};
	int err = 0;
	int reply_type =  SBC_REPLY_YES;
	lpm_debug("lpm_exchange_msg \n");

	/*
	 * Lock the mailbox, prevent other caller to access MB write
	 * In case API is called with interrupt disabled from Linux PM
	 * try to lock mutex.
	 */
	if (in_atomic() || irqs_disabled()) {
		err = mutex_trylock(&lpm_drv->msg_protection_mutex);
		reply_type = SBC_REPLY_NO_IRQ;
		if (!err)
			return -EAGAIN;
	} else {
		mutex_lock(&lpm_drv->msg_protection_mutex);
		if (response == NULL)
			reply_type = SBC_REPLY_NO;
	}

	lpm_msg.command_id = send_msg->command_id;

	if (lpm_msg.command_id & LPM_MSG_REPLY)
		lpm_msg.transaction_id = send_msg->trans_id;
	else
		lpm_msg.transaction_id = lpm_drv->glbtrans_id++;

	/* Copy data into mailbox message */
	if (send_msg->msg_size)
		memcpy(&lpm_msg.msg_data, send_msg->msg, send_msg->msg_size);

	/* Print message information for debug purpose */
	lpm_debug("Sending msg {%x, %x} \n", lpm_msg.command_id,
				lpm_msg.transaction_id);

	lpm_drv->reply_from_sbc = 0;

	/* Send message to mailbox write */
	err = lpm_send_msg(&lpm_msg, send_msg->msg_size);
	if (unlikely(err < 0)) {
		pr_err("firmware not loaded \n");
		goto exit_fun;
	}

	switch (reply_type) {
	case SBC_REPLY_NO_IRQ:
		err = lpm_get_response(response);
		break;
	case  SBC_REPLY_YES:
		/*
		 * wait for response here
		 * In case of signal, we can get negative value
		 * In such case wait till timeout or response from SBC
		 */
		do {
			err = wait_event_interruptible_timeout(
				lpm_drv->stm_lpm_wait_queue,
				lpm_drv->reply_from_sbc == 1,
				msecs_to_jiffies(100));
		} while (err < 0);
		break;
	case SBC_REPLY_NO:
		goto exit_fun;
		break;
	}
	lpm_debug("recd reply  %x {%x, %x } \n", err,
		lpm_drv->fw_reply_msg.command_id,
	lpm_drv->fw_reply_msg.transaction_id);

	if (unlikely(err == 0)) {
		pr_err("f/w is not responding \n");
		err = -EAGAIN;
		goto exit_fun;
	}

	BUG_ON(!(lpm_drv->fw_reply_msg.command_id & LPM_MSG_REPLY));

	memcpy(response, &lpm_drv->fw_reply_msg,
		sizeof(struct lpm_message));
	if (lpm_msg.transaction_id == lpm_drv->fw_reply_msg.transaction_id) {
		if (response->command_id == LPM_MSG_ERR) {
			pr_err("Firmware error code %d \n",
						response->msg_data[1]);
			/*
			 * Firmware does not support this command
			 * therefore firmware gave error.
			 * In such cases, return EREMOTEIO as firmware error
			 * code is not yet decided.
			 * To Do
			 * conversion of firmware error code into Linux world
			 */
			err = -EREMOTEIO;
		}
		/* There is possibility we might get response for large msg. */
		if (response->command_id == LPM_MSG_BKBD_READ)
			lpm_debug("Got in reply a big message \n");
	} else {
		/*
		 * Different trans id is expected from SBC as big messages are
		 * encapsulated into LPM_MSG_BKBD_WRIRE message.
		 */
		lpm_debug("Received ID %x \n", response->transaction_id);
	}

exit_fun:
	mutex_unlock(&lpm_drv->msg_protection_mutex);
	/* Convert success error code into 0*/
	if (err > 0)
		err = 0;
	return err;
}

/**
 * lpm_load_segment() - To load SBC firmware
 * @lpm_drv_p:	driver data
 * @elfinfo:	firmware elf information
 * @i:	Index of elf information
 *
 * Return - 0 on success
*/

static int lpm_load_segment(struct stm_lpm_driver_data *lpm_drv_p,
				struct ELF64_info *elfinfo, int i)
{

	Elf64_Phdr *phdr = &elfinfo->progbase[i];
	void *data = elfinfo->base;
	signed long offset = DATA_OFFSET + phdr->p_paddr;
	unsigned long size = phdr->p_memsz;
	/*
	 * Check if we need to write onto program area or data area.
	 * SBC_PRG_MEMORY_ELF_MARKER marker in elf indicate writing to
	 * program area
	 */
	if (phdr->p_paddr == SBC_PRG_MEMORY_ELF_MARKER)
		offset = SBC_PRG_MEMORY_OFFSET;

	memcpy_toio(lpm_drv_p->lpm_mem_base[0] + offset,
		data + phdr->p_offset, size);
	return 0;
}

static int lpm_config_power_pio(void)
{
#ifdef CONFIG_STM_LPM_POWER_PIO
	/* now firmware is loaded inform SBC about wake up PIO */
	int gpio_power = CONFIG_STM_LPM_POWER_PIO;
	struct stm_lpm_pio_setting configurepio = {0};
	int port, pin;

	port = (gpio_power & 0xFF00) >> 0x8;
	pin = (gpio_power & 0xF0) >> 0x4;

	pr_info("stm lpm: configuring gpio_power: GPIO[%d][%d]\n",
		port, pin);
	if (gpio_power & 1)
		configurepio.pio_level = STM_LPM_PIO_HIGH;
	configurepio.interrupt_enabled = 0;
	configurepio.pio_direction = STM_LPM_PIO_OUTPUT;
	configurepio.pio_use = STM_LPM_PIO_POWER;
	configurepio.pio_bank = port;
	configurepio.pio_pin = pin;
	return stm_lpm_setup_pio(&configurepio);
#else
	return 0;
#endif
}

void stm_lpm_config_reboot(enum stm_lpm_config_reboot_type type)
{
	switch (type) {
	case stm_lpm_reboot_with_ddr_self_refresh:
		writel(0x9b, lpm_drv->lpm_mem_base[2] + 0x20);
		break;
	case stm_lpm_reboot_with_ddr_off:
		writel(0x30, lpm_drv->lpm_mem_base[2] + 0x20);
		break;
	default:
		pr_err("%s: configuration NOT supported!\n",
			__func__);
	}
}

void stm_lpm_power_off(void)
{
	/*
	 * Raise the command 'ENTER_PASSIVE' (i.e.: 0x5)
	 * on the mail-box; on that the SBC will remove the power
	 */
	writel(0x5, lpm_drv->lpm_mem_base[1] + MBX_WRITE_STATUS1);
}

/**
 * lpm_load_fw() - Load sbc firmware
 * @fw:	pointer to firmware
 * @lpm_drv_p:	driver date
 *
 * Return - 0 on success
 * Return - negative error on failure
*/

static int lpm_load_fw(const struct firmware *fw,
		struct stm_lpm_driver_data *lpm_drv_p)
{
	struct ELF64_info *elfinfo = NULL;
	struct stm_lpm_version driver_ver, fw_ver;
	int i;
	int err = 0;
	if (unlikely(!fw)) {
		pr_err("LPM: Unable to load LPM firmware: not present?\n");
		return -EINVAL;
	}

	pr_info("LPM: Found sbc f/w \n");
	elfinfo = (struct ELF64_info *)ELF64_initFromMem((uint8_t *)fw->data,
						fw->size, 0);
	if (unlikely(elfinfo == NULL))
			return -ENOMEM;

	for (i = 0; i < elfinfo->header->e_phnum; i++)
		if (elfinfo->progbase[i].p_type == PT_LOAD)
			lpm_load_segment(lpm_drv_p, elfinfo, i);

	/* Initialize sbc lpm */
	i = readl((u32)lpm_drv_p->lpm_mem_base[2] + SBC_CONFIG_OFFSET);
	i |= 0x1;
	writel(i, (u32)lpm_drv_p->lpm_mem_base[2] + SBC_CONFIG_OFFSET);
	kfree((void *)elfinfo);

	/* Wait till 1 second to get response from firmware */
	i = 0;
	do {
		mdelay(100);
		err = stm_lpm_get_fw_state(&lpm_drv_p->sbc_state);
		if (err < 0 || lpm_drv_p->sbc_state == STM_LPM_SBC_RUNNING)
			break;
		i++;
	} while (i != 10);
	if (err == 0) {
		err = stm_lpm_get_version(&driver_ver, &fw_ver);
		if (likely(err == 0))
			lpm_drv_p->fw_major_ver = fw_ver.major_comm_protocol;
		err = lpm_config_power_pio();
		if (err)
			pr_err("stm_lpm: Error while configuring gpio_power\n");
		platform_device_register_data(&lpm_drv_p->pdev->dev,
			"stm-rtc-sbc", 0, NULL, 0);
	}
	/* We do not return error if caused by SBC communication */
	return 1;
}

/**
 * lpm_load_firmware() - Request firmware to load
 * @pdev:	pointer to platform device
 *
 * Return - 0 on success
 * Return - negative error on failure
*/

static int lpm_load_firmware(struct platform_device *pdev)
{
	int err;
	int result;
	struct stm_lpm_driver_data *lpm_drv_p;
	lpm_drv_p = platform_get_drvdata(pdev);
	result = snprintf(lpm_drv_p->fw_name, sizeof(lpm_drv_p->fw_name),
			"lpm_fw%s.elf", get_cpu_subtype(&current_cpu_data));

	/* was the string truncated? */
	BUG_ON(result >= sizeof(lpm_drv_p->fw_name));

	lpm_debug("LPM: Requesting Firmware (%s)...\n",
		lpm_drv_p->fw_name);

	err = request_firmware_nowait(THIS_MODULE, 1, lpm_drv_p->fw_name,
			&pdev->dev, (struct stm_lpm_driver_data *)lpm_drv_p,
			(void *)lpm_load_fw);

	return err;
}

/**
 * stm_lpm_probe() - Probe function of driver
 * @pdev:	platform device pointer
 *
 * Return - 0 on success
 * Return - negative error on failure
*/

static int __init stm_lpm_probe(struct platform_device *pdev)
{
	struct resource *res;
	int err = 0;
	int count = 0;
	lpm_debug("stm lpm probe \n");

	lpm_drv = devm_kzalloc(&pdev->dev, sizeof(struct stm_lpm_driver_data),
				GFP_KERNEL);
	if (unlikely(lpm_drv == NULL)) {
		pr_err("%s: Request memory failed \n", __func__);
		return -ENOMEM;
	}

	lpm_drv->pdev = pdev;

	for (count = 0; count < 3; count++) {
		res = platform_get_resource(pdev, IORESOURCE_MEM, count);
		if (!res) {
			err = -ENODEV;
			goto free_and_exit;
		}
		lpm_debug("mem:SBC res->start %x %x\n", res->start, res->end);
		if (!devm_request_mem_region(&pdev->dev, res->start,
			resource_size(res), "stm-lpm")) {
			pr_err("%s: Request mem 0x%x region failed \n",
				__func__, res->start);
			err = -ENOMEM;
			goto free_and_exit;
		}
		lpm_drv->lpm_mem_base[count] = devm_ioremap_nocache(&pdev->dev,
							res->start,
							resource_size(res));
		if (!lpm_drv->lpm_mem_base[count]) {
			pr_err("%s: Request iomem 0x%x region failed \n",
				__func__, (unsigned int)res->start);
			err = -ENOMEM;
			goto free_and_exit;
		}
		lpm_debug("lpm_add %x \n", (u32)lpm_drv->lpm_mem_base[count]);
	}

	/* Irq request */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		pr_err("%s Request irq %x not done\n", __func__, res->start);
		err = -ENODEV;
		goto free_and_exit;
	}
	if (devm_request_irq(&pdev->dev, res->start, lpm_isr,
			IRQF_DISABLED, "stlmp", (void *)lpm_drv) < 0) {
		pr_err("%s: Request stm lpm irq not done\n", __func__);
		err = -ENODEV;
		goto free_and_exit;
	}

	init_waitqueue_head(&lpm_drv->stm_lpm_wait_queue);

	mutex_init(&lpm_drv->msg_protection_mutex);

	/* stm lpm does not need dedicate work queue so use default queue */
	INIT_WORK(&lpm_drv->lpm_sbc_reply_work, lpm_sbc_reply_worker);

	platform_set_drvdata(pdev , lpm_drv);

	/*
	 * Program Mailbox for interrupt enable
	 */
	lpm_write32(lpm_drv, 1, MBX_INT_SET_ENABLE, 0xFF);
	lpm_drv->sbc_state = STM_LPM_SBC_BOOT;
	lpm_load_firmware(pdev);
	return err;

free_and_exit:
	kfree(lpm_drv);
	return err;
}

/**
 * stm_lpm_remove() - To free used resources
 * @pdev:	device pointer
 * Return code 0
 */

static int stm_lpm_remove(struct platform_device *pdev)
{
	struct stm_lpm_driver_data *lpm_drv_p;
	lpm_debug("stm_lpm_remove \n");
	lpm_drv_p = platform_get_drvdata(pdev);
	kfree(lpm_drv_p);
	return 0;
}

/**
 * stm_lpm_freeze() - Freeze callback
 * @dev:	device pointer
 *
 * This is not really required but keeping this
 * in case needed for future
 * Return - 0 for success
 */

static int stm_lpm_freeze(struct device *dev)
{
	lpm_debug("stm_lpm_freeze \n");
	return 0;
}

/**
 * stm_lpm_restore() - Restore callback
 * @dev:	device pointer
 *
 * This is not really required but keeping this
 * in case needed for future
 * Return - 0 for success
 */

static int stm_lpm_restore(struct device *dev)
{
	lpm_debug("stm_lpm_restore \n");
	return 0;
}

static struct dev_pm_ops stm_lpm_pm_ops = {
		.freeze = stm_lpm_freeze,
		.restore = stm_lpm_restore,
};

static struct platform_driver stm_lpm_driver = {
	.driver.name = "stm-lpm",
	.driver.owner = THIS_MODULE,
	.driver.pm  = &stm_lpm_pm_ops,
	.probe = stm_lpm_probe,
	.remove = stm_lpm_remove,
};

static int __init stm_lpm_init(void)
{
	int err = 0;
	err = platform_driver_register(&stm_lpm_driver);
	if (err)
		pr_err("STM_LPM driver fails on registrating (%x)\n" , err);
	else
		pr_info("STM_LPM driver registered\n");
	return err;
}

void __exit stm_lpm_exit(void)
{
	pr_info("STM_LPM driver removed \n");
	platform_driver_unregister(&stm_lpm_driver);
}

module_init(stm_lpm_init);
module_exit(stm_lpm_exit);

MODULE_AUTHOR("STMicroelectronics  <www.st.com>");
MODULE_DESCRIPTION("lpm device driver for STMicroelectronics devices");
MODULE_LICENSE("GPL");
