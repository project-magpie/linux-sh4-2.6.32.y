/* ---------------------------------------------------------------------------
 *
 * Copyright (C) 2012 STMicroelectronics Limited
 *
 * Author:Pooja Agarwal <pooja.agarwal@st.com>
 * Author:Udit Kumar <udit-dlh.kumar@st.cm>
 * Contributor:Francesco Virlinzi <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 * ----------------------------------------------------------------------------
 */

#include <linux/stm/lpm.h>
#include <linux/platform_device.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/errno.h>
#include <linux/kthread.h>
#include <linux/semaphore.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/stm/platform.h>
#include <linux/firmware.h>
#include "lpm_def.h"
#include <linux/libelf.h>

#include <linux/string.h>
#include <linux/module.h>
#include <linux/elf.h>


#ifdef CONFIG_STM_LPM_DEBUG
#define lpm_debug printk
#else
#define lpm_debug(fmt, arg...);
#endif

struct stm_lpm_driver_data {
	unsigned int lpm_mem_base;
	struct stm_lpm_message fw_reply_msg;
	struct stm_lpm_message fw_request_msg;
	unsigned char stm_thread_terminate;
	char fw_name[20];
	int reply_from_sbc;
	int query_from_sbc;
	wait_queue_head_t stm_lpm_wait_queue;
	struct mutex msg_protection_mutex;
	struct task_struct *th;
	int glbtrans_id;
};

static struct stm_lpm_driver_data *lpm_drv;

int stm_lpm_setup_ir(int num_of_sequence, int byte_count, int protocol,
		struct stm_lpm_ir_setting *byte_sequence)
{
	char msg[14] = {0};
	int ir_msg_count = 0, offset, err = 0, msg_size;
	struct stlpm_internal_send_msg	send_msg = {0};
	struct stm_lpm_message response = {0};
	if (!num_of_sequence || !byte_count || !protocol)
		return -EINVAL;

	if (num_of_sequence*byte_count < MAX_SEQ_IN_MAILBOX) {
		msg[0] = (protocol<<4);
		msg[1] = num_of_sequence;
		msg[2] = byte_count;
		ir_msg_count = 3;
		memcpy(&msg[ir_msg_count], &byte_sequence,
			(num_of_sequence*4));
		msg_size = (num_of_sequence*4)+3;
		LPM_FILL_MSG(send_msg, STM_LPM_MSG_SET_IR, msg, msg_size,
				MSG_ID_AUTO, SBC_REPLY_YES);
		err = lpm_exchange_msg(&send_msg, &response);
	} else {
		/* To get OFFSET to write large messg in DMEM */
		msg[0] = ((SIZE_DATA_BYTES(num_of_sequence))&(0xff));
		msg[1] = (((SIZE_DATA_BYTES(num_of_sequence))&(0xff00)) >> 8);
		LPM_FILL_MSG(send_msg, STM_LPM_MSG_LGWR_OFFSET, msg, 1,
			MSG_ID_AUTO, SBC_REPLY_YES);
		err = lpm_exchange_msg(&send_msg, &response);

		if (response.command_id != STM_LPM_MSG_ERR) {
			memcpy((void *)&offset, &response.msg_data[2], 4);
			/* Write large messg in DMEM*/
			lpm_write8(lpm_drv, SBC_DATA_MEMORY_OFFSET + offset,
				STM_LPM_MSG_SET_IR);
			lpm_write8(lpm_drv,
				SBC_DATA_MEMORY_OFFSET+(offset+1), 0);
			lpm_write8(lpm_drv, SBC_DATA_MEMORY_OFFSET+(offset+2),
				(protocol<<4));
			lpm_write8(lpm_drv, SBC_DATA_MEMORY_OFFSET+(offset+3),
				num_of_sequence);
			lpm_write8(lpm_drv, SBC_DATA_MEMORY_OFFSET+(offset+4),
				byte_count);
			ir_msg_count = 5;
			memcpy((void *)lpm_drv->lpm_mem_base +
			SBC_DATA_MEMORY_OFFSET+(offset+ir_msg_count),
			&byte_sequence, (num_of_sequence*4));

			/*BKBD_WRITE*/
			msg[0] = ((SIZE_DATA_BYTES(num_of_sequence))&(0xff));
			msg[1] = (((SIZE_DATA_BYTES
				(num_of_sequence))&(0xff00)) >> 8);
			memcpy(&msg[2], &response.msg_data[2], 4);
			LPM_FILL_MSG(send_msg, STM_LPM_MSG_BKBD_WRITE, msg,
				6, MSG_ID_AUTO, SBC_REPLY_YES);
			err = lpm_exchange_msg(&send_msg, &response);
			if (response.command_id == STM_LPM_MSG_ERR)
				return -EINVAL;
		} else {
		/*
		 * Error Handling
		 */
		}
	}
	return err;
}
EXPORT_SYMBOL(stm_lpm_setup_ir);

int stm_lpm_get_wakeup_info(enum stm_lpm_wakeup_devices *wakeupdevice,
					int *validsize, int datasize, int *data)
{
	int err = 0;
	unsigned int offset;
	u16 size;
	char msg[4] = {0};
	struct stlpm_internal_send_msg send_msg = {0};
	struct stm_lpm_message response = {0};

	msg[0] = *wakeupdevice;
	memcpy(&msg[1], &datasize, 2);

	LPM_FILL_MSG(send_msg, STM_LPM_MSG_GET_IRQ, msg, 3,
				MSG_ID_AUTO, SBC_REPLY_YES);
	err = lpm_exchange_msg(&send_msg, &response);

	memcpy((void *)&size, &response.msg_data[0], 2);
	/* to get the actual size reset 15th bit(m) */
	size &= M_BIT_MASK;
	/* Two response are possible*/
	if (response.command_id == STM_LPM_MSG_BKBD_READ) {
		/* 1 large msg */
		memcpy((void *)&offset, &response.msg_data[2], 4);
		memcpy((void *)data,
		(const void *)(SBC_DATA_MEMORY_OFFSET+offset + 4), size);
		memcpy((void *)validsize,
		(const void *)(SBC_DATA_MEMORY_OFFSET+(offset + 2)), 2);
	} else {
		/*2 Small msg*/
		memcpy((void *)data,
		(const void *)&response.msg_data[4], size);
		*validsize = size;
	}

	/*Check for m: more data available or not*/
	if (response.msg_data[1] & 0x80) {
		/* Returning positive error as need to read again*/
		return EAGAIN;
	}
	return err;
}
EXPORT_SYMBOL(stm_lpm_get_wakeup_info);

static irqreturn_t lpm_isr(int this_irq, void *params)
{
	/*Read the data from mailbox memory*/
	struct stm_lpm_driver_data *lpm_drv_p;
	unsigned int msg_read[4], i;
	struct stm_lpm_message *msg;
	lpm_drv_p = (struct stm_lpm_driver_data *)params;

	/*Read the data from mailbox memory*/
	for (i = 0; i < 4; i++)
		msg_read[i] = lpm_read32(lpm_drv_p, MBX_READ_STATUS1 + i * 4);
	if ((msg_read[0] & STM_LPM_MSG_REPLY) ||
	   ((msg_read[0] & STM_LPM_MSG_BKBD_READ)
	   && (lpm_drv_p->reply_from_sbc == 0))) {
		msg = &lpm_drv_p->fw_reply_msg;
		lpm_drv_p->reply_from_sbc = 1;
	} else {
		/*Can we ask FW to send msg by msg*/
		/* if not then handle message*/
		msg = &lpm_drv_p->fw_request_msg;
		lpm_drv_p->query_from_sbc = 1;
	}
	memcpy(msg, &msg_read, 16);
	wake_up_interruptible(&lpm_drv_p->stm_lpm_wait_queue);
	/*Clear mail box registery*/
	lpm_write32(lpm_drv_p, MBX_READ_CLR_STATUS1, 0xFF);
	return IRQ_HANDLED;
}

static int lpm_msg_request_task(void *params)
{
	int i, offset, size, err = 0;
	struct stm_lpm_large_trace_data trace_data = {0};
	struct stlpm_internal_send_msg send_msg = {0};
	unsigned char msg[8] = {0};
	struct stm_lpm_driver_data *lpm_drv_p;
	struct stm_lpm_message *msg_p;
	lpm_drv_p = (struct stm_lpm_driver_data *)params;
	msg_p = &lpm_drv_p->fw_request_msg;
	lpm_debug("Entering into my thread \n");
	while (lpm_drv_p->stm_thread_terminate == 0) {
		lpm_drv_p->query_from_sbc = 0;
		err = wait_event_interruptible(
			lpm_drv_p->stm_lpm_wait_queue,
			lpm_drv_p->query_from_sbc == 1);
		if (err >= 0 && msg_p->command_id != 0) {
			lpm_debug("recd command id %x \n",
						msg_p->command_id);
			switch (msg_p->command_id) {
			/*case STM_LPM_MSG_SET_TRIM:
			case STM_LPM_MSG_ENTER_PASSIVE:
			case STM_LPM_MSG_SET_WDT:
			case STM_LPM_MSG_SET_RTC:
			case STM_LPM_MSG_SET_FP:
			case STM_LPM_MSG_SET_TIMER:
			case STM_LPM_MSG_GET_STATUS:
			case STM_LPM_MSG_GEN_RESET:
			case STM_LPM_MSG_SET_WUD:
			case STM_LPM_MSG_GET_WUD:
			case STM_LPM_MSG_SET_IR:
			case STM_LPM_MSG_BKBD_WRITE:
			case STM_LPM_MSG_LGWR_OFFSET:
			case STM_LPM_MSG_READ_RTC:
			case STM_LPM_MSG_GET_IRQ:*/
			/*Covers above cases */
			default:
				/* to be handled later on read RTC*/
				msg[0] = msg_p->command_id;
				msg[1] = -EINVAL;
				LPM_FILL_MSG(send_msg, STM_LPM_MSG_ERR, msg, 2,
				msg_p->transaction_id, SBC_REPLY_NO);
				lpm_exchange_msg(&send_msg, NULL);
				break;
			case STM_LPM_MSG_TRACE_DATA:
				memcpy(&size, &msg_p->msg_data[0], 2);
				for (i = 0; i < size; i++)
					printk(KERN_ERR "%c \n",
					msg_p->msg_data[2+i]);
				memcpy(msg, &msg_p->msg_data, 2);
				LPM_FILL_MSG(send_msg, STM_LPM_MSG_TRACE_DATA|
				STM_LPM_MSG_REPLY, msg, 2,
				msg_p->transaction_id,
				SBC_REPLY_NO);
				lpm_exchange_msg(&send_msg, NULL);
				break;
			case STM_LPM_MSG_VER:
				msg[0] = STM_LPM_MAJOR_PROTO_VER << 4
						| STM_LPM_MINOR_PROTO_VER;
				msg[1] = STM_LPM_MAJOR_SOFT_VER << 4
						| STM_LPM_MINOR_SOFT_VER;
				msg[2] = STM_LPM_PATCH_SOFT_VER << 4
						| STM_LPM_BUILD_MONTH;
				msg[3] = STM_LPM_BUILD_DAY;
				msg[4] = STM_LPM_BUILD_YEAR;
				LPM_FILL_MSG(send_msg, STM_LPM_MSG_VER
				|STM_LPM_MSG_REPLY, msg, 5,
				msg_p->transaction_id, SBC_REPLY_NO);
				lpm_exchange_msg(&send_msg, NULL);
				break;
			case STM_LPM_MSG_BKBD_READ:
				memcpy(&offset, &msg_p->msg_data[2], 4);
				memcpy(&size, &msg_p->msg_data[0], 2);
				memcpy((void *)&trace_data,
				(void *)(lpm_drv_p->lpm_mem_base+
				SBC_DATA_MEMORY_OFFSET+offset),
				size);
				if (trace_data.commant_id ==
					STM_LPM_MSG_TRACE_DATA)
					/*Print data here*/
				LPM_FILL_MSG(send_msg,
				STM_LPM_MSG_BKBD_READ|STM_LPM_MSG_REPLY,
				msg, 2, msg_p->transaction_id, SBC_REPLY_NO);
				lpm_exchange_msg(&send_msg, NULL);
			}
			msg_p->command_id = 0;
			}
	}

	return 0;
}

static int lpm_send_msg(struct stm_lpm_message *msg,
				unsigned char msg_size)
{
	/* For ORLY only*/
	int err = 0, count;
	int *tmp_i = (int *)msg;

	for (count = (msg_size+1)/4; count >= 0; count--)
		lpm_write32(lpm_drv, (MBX_WRITE_STATUS1+(count*4)),
		*(tmp_i+count));
	return err;
}

static int lpm_get_response(struct stm_lpm_message *response)
{
	int count;
	int msg_read1;
	/*To check if this count is ok*/
	for (count = 0; count < 30; count++) {
		msg_read1 = lpm_read32(lpm_drv, MBX_READ_STATUS1);
		if (msg_read1&0xFF)
			break;
	}
	if (count == 10)
		/*To do exact error code for this */
		return -EINVAL;
	memcpy(&lpm_drv->fw_reply_msg, (void *)&msg_read1, 4);
	return 1;
}

int lpm_exchange_msg(struct stlpm_internal_send_msg *send_msg,
					struct stm_lpm_message *response)
{
	struct stm_lpm_message lpm_msg = {0};
	int count = 0, err = 0;

	lpm_debug("lpm_exchange_msg \n");
	if (send_msg->reply_type != SBC_REPLY_NO_IRQ)
		mutex_lock(&lpm_drv->msg_protection_mutex);
	lpm_msg.command_id = send_msg->command_id;
	if (lpm_msg.command_id&STM_LPM_MSG_REPLY)
		lpm_msg.transaction_id = send_msg->trans_id;
	else
		lpm_msg.transaction_id = lpm_drv->glbtrans_id++;
	if (send_msg->msg_size)
		memcpy(&lpm_msg.msg_data, send_msg->msg, send_msg->msg_size);

	lpm_debug("Sending msg {%x, %x ", lpm_msg.command_id,
				lpm_msg.transaction_id);
	for (count = 0; count < send_msg->msg_size; count++)
		lpm_debug(" %x", lpm_msg.msg_data[count]);
	lpm_debug(" } \n");
	lpm_drv->reply_from_sbc = 0;
	err = lpm_send_msg(&lpm_msg, send_msg->msg_size);

	switch (send_msg->reply_type) {
	case SBC_REPLY_NO_IRQ:
		err = lpm_get_response(response);
		break;
	case  SBC_REPLY_YES:
		/*wait for response here */
		err = wait_event_interruptible_timeout(
				lpm_drv->stm_lpm_wait_queue,
				lpm_drv->reply_from_sbc == 1,
				msecs_to_jiffies(100));
		break;
	case SBC_REPLY_NO:
		goto exit_fun;
		break;
	}

	lpm_debug("recd reply error %x {%x, %x ", err,
		lpm_drv->fw_reply_msg.command_id,
	lpm_drv->fw_reply_msg.transaction_id);
	for (count = 0; count < STM_LPM_MAX_MSG_DATA; count++)
		lpm_debug(" %x", lpm_drv->fw_reply_msg.msg_data[count]);
	lpm_debug("} \n");
	if (err) {
		memcpy(response, &lpm_drv->fw_reply_msg,
			sizeof(struct stm_lpm_message));
		if (lpm_msg.transaction_id ==
			lpm_drv->fw_reply_msg.transaction_id) {
			/* Check for error in FWLPM response*/
			if (response->command_id == STM_LPM_MSG_ERR) {
				/*just check error for command*/
				printk(KERN_ERR "Error respone \n");
				/* Get the actual error */
				err = response->msg_data[3];
			}
		/* there is possibility we might get
		response for large msg*/
		} else if (response->command_id == STM_LPM_MSG_BKBD_READ) {
			lpm_debug("Got in reply a big message \n");
		} else {
			lpm_debug("Received response tran ID %x instead \
			of %x \n", response->transaction_id, \
			lpm_msg.transaction_id);
			/* invalid trans id*/
			err = -EAGAIN;
		}
	} else {
		lpm_debug("f/w is not reponding \n");
	}

exit_fun:
	if (send_msg->reply_type != SBC_REPLY_NO_IRQ)
		mutex_unlock(&lpm_drv->msg_protection_mutex);
	return err;
}

static int lpm_load_segment(struct stm_lpm_driver_data *lpm_drv_p,
				struct ELF64_info *elfinfo, int i)
{

	Elf64_Phdr *phdr = &elfinfo->progbase[i];
	void *data = elfinfo->base;
	signed long offset = SBC_DATA_MEMORY_OFFSET + phdr->p_paddr;
	unsigned long size = phdr->p_memsz;
	if (phdr->p_paddr == 0x00400000)
		offset = SBC_PRG_MEMORY_OFFSET;

	memcpy_toio(((void *)lpm_drv_p->lpm_mem_base + offset),
		data + phdr->p_offset, size);
	return 0;
}


static int lpm_load_fw(const struct firmware *fw,
					struct stm_lpm_driver_data *lpm_drv_p)
{
	struct ELF64_info *elfinfo = NULL;
	int i;

	if (!fw) {
		lpm_debug("LPM: Unable to load LPM firmware: not present?\n");
		return -EINVAL;
	}

	lpm_debug("LPM: Found sbc f/w \n");
	elfinfo = (struct ELF64_info *)ELF64_initFromMem((uint8_t *)fw->data,
				fw->size, 0);
	if (elfinfo == NULL)
			return -ENOMEM;

	for (i = 0; i < elfinfo->header->e_phnum; i++)
		if (elfinfo->progbase[i].p_type == PT_LOAD)
			lpm_load_segment(lpm_drv_p, elfinfo, i);

	/* Initialize sbc lpm */
	i = readl(lpm_drv_p->lpm_mem_base + SBC_CONFIG_OFFSET);
	i |= 0x1;
	writel(i, lpm_drv_p->lpm_mem_base + SBC_CONFIG_OFFSET);

	kfree((void *)elfinfo);
	return 1;
}

static int lpm_load_firmware(struct device *dev)
{
	int err;
	int result;
	struct stm_lpm_driver_data *lpm_drv_p;
	lpm_drv_p = dev->platform_data;
	result = snprintf(lpm_drv_p->fw_name, sizeof(lpm_drv_p->fw_name),
			"lpm_fw%s.elf", lpm_get_cpu_type());

	/* was the string truncated? */
	BUG_ON(result >= sizeof(lpm_drv_p->fw_name));

	lpm_debug("LPM: Loading Firmware (%s)...\n",
		lpm_drv_p->fw_name);

	err = request_firmware_nowait(THIS_MODULE, 1, lpm_drv_p->fw_name, dev,
	(struct stm_lpm_driver_data *)dev->platform_data, (void *)lpm_load_fw);
	if (err)
			return -ENOMEM;

	return 0;
}

static int __init stm_lpm_probe(struct platform_device *pdev)
{
	struct resource *res;
	int err = 0;
	lpm_debug("stm lpm probe \n");
	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!res)
		return -ENODEV;
	lpm_debug("mem: res->start %x %x \n", res->start, res->end);
	if (!devm_request_mem_region(&pdev->dev, res->start,
		res->end - res->start, "stm-lpm")) {

		printk(KERN_ERR "%s: Request mem 0x%x region not done\n",
				__func__, res->start);
		return -ENOMEM;
	}

	/*Allocate here to save few goto*/
	lpm_drv = kmalloc(sizeof(struct stm_lpm_driver_data), GFP_KERNEL);
	if (unlikely(lpm_drv == NULL)) {
		printk(KERN_ERR "%s: Request memory not done\n", __func__);
		return -ENOMEM;
	}
	memset(lpm_drv, 0, sizeof(struct stm_lpm_driver_data));
	pdev->dev.platform_data = lpm_drv;
	lpm_drv->lpm_mem_base = (unsigned int)devm_ioremap_nocache(&pdev->dev,
			res->start, (int)(res->end - res->start));
	if (!lpm_drv->lpm_mem_base) {
		printk(KERN_ERR "%s: Request iomem 0x%x region not done\n",
			__func__, (unsigned int)res->start);
		err = -ENOMEM;
		goto free_and_exit;
	}
	lpm_debug("lpm_add %x \n", lpm_drv->lpm_mem_base);

	/*irq request */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (!res) {
		printk(KERN_ERR "%s Request irq %x not done\n",
			__func__, res->start);
		err = -ENODEV;
		goto free_io;
	}
	if (devm_request_irq(&pdev->dev, res->start, lpm_isr,
			IRQF_DISABLED, "stlmp", (void *)lpm_drv) < 0) {
		printk(KERN_ERR "%s: Request stlpm irq not done\n",
			__func__);
		err = -ENODEV;
		goto free_io;
	}
	/*Semaphore initialization*/
	init_waitqueue_head(&lpm_drv->stm_lpm_wait_queue);
	mutex_init(&lpm_drv->msg_protection_mutex);

	lpm_drv->th = kthread_run(lpm_msg_request_task,
				(void *)lpm_drv, "lpm_read_task");
	if (IS_ERR(lpm_drv->th)) {
		printk(KERN_ERR "Unable to start thread");
		err = -ENOMEM;
		goto free_irq;
	}
	/*
	 * Program Mailbox for interrupt enable
	 */
	lpm_write32(lpm_drv, MBX_INT_SET_ENABLE, 0xFF);
	lpm_write32(lpm_drv, MBX_WRITE_STATUS1, 0);
	lpm_load_firmware(&pdev->dev);
	return err;

free_irq:
	devm_free_irq(&pdev->dev, res->start, NULL);
free_io:
	devm_iounmap(&pdev->dev, (void *)lpm_drv->lpm_mem_base);
free_and_exit:
	kfree(lpm_drv);
	return err;
}

static int stm_lpm_remove(struct platform_device *pdev)
{
	struct resource *res;
	struct stm_lpm_driver_data *lpm_drv_p;
	lpm_debug("stm_lpm_remove \n");
	lpm_drv_p = pdev->dev.platform_data;
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	devm_free_irq(&pdev->dev, res->start, NULL);
	devm_iounmap(&pdev->dev, (void *)lpm_drv_p->lpm_mem_base);
	lpm_drv_p->stm_thread_terminate = 1;
	lpm_drv_p->query_from_sbc = 1;
	wake_up_interruptible(&lpm_drv_p->stm_lpm_wait_queue);
	kthread_stop(lpm_drv_p->th);
	kfree(lpm_drv_p);
	return 0;
}

static struct platform_driver stm_lpm_driver = {
	.driver.name	= "stm-lpm",
	.driver.owner	= THIS_MODULE,
	.probe		= stm_lpm_probe,
	.remove		= stm_lpm_remove,
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
	printk(KERN_ERR "STM_LPM driver removed \n");
	platform_driver_unregister(&stm_lpm_driver);
}

module_init(stm_lpm_init);
module_exit(stm_lpm_exit);

MODULE_AUTHOR("STMicroelectronics  <www.st.com>");
MODULE_DESCRIPTION("lpm device driver for STMicroelectronics devices");
MODULE_LICENSE("GPL");
