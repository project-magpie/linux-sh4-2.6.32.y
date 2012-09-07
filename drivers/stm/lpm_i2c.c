/*
 * <root>/drivers/stm/lpm_i2c.c
 *
 * This driver implements communication with external SBC
 * over i2c bus in some STMicroelectronics devices.
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
#include <linux/module.h>
#include <linux/stm/platform.h>
#include "lpm_def.h"

#ifdef CONFIG_STM_LPM_DEBUG
#define lpm_debug(fmt, ...) pr_debug(fmt, ##__VA_ARGS__)
#else
#define lpm_debug(fmt, ...)
#endif

/**
 * struct stm_lpm_driver_data - Driver data for i2c based lpm driver
 * @i2c_sbc_adapter:	I2C adapter used for communication with SBC
 * @gbltrans_id: 	transaction id
 * @fw_major_ver: 	major firmware version of Standby Controller
 * @msg_protection_mutex:	protection mutex for i2c bus access
 */

struct stm_lpm_driver_data{
	struct i2c_adapter *i2c_sbc_adapter;
	u8 gbltrans_id;
	char fw_major_ver;
	struct mutex msg_protection_mutex;
};

static struct stm_lpm_driver_data *lpm_drv;

static const struct i2c_device_id lpm_ids[] = {
	{ "stm-lpm", 0 },
	{ /* END OF LIST */ }
};
MODULE_DEVICE_TABLE(i2c, lpm_ids);

/**
 * stm_lpm_setup_ir() - To set ir key setup
 * @num_keys:	number of IR keys
 * @ir_key_info:	information of IR keys
 *
 * External SBC firmware does not support this.
 */

int stm_lpm_setup_ir(u8 num_keys, struct stm_lpm_ir_keyinfo *ir_key_info)
{
	return -ENOSYS;
}
EXPORT_SYMBOL(stm_lpm_setup_ir);

/**
 * stm_lpm_get_wakeup_info() - To get additional data from wakeup device
 * @wakeupdevice:	device ID
 * @validsize:	read valid size will be returned
 * @datasize: 	data size to read
 * @data: 	data pointer
 *
 * External SBC firmware does not support this.
 */

int stm_lpm_get_wakeup_info(enum stm_lpm_wakeup_devices *wakeupdevice,
	u16 *validsize, u16 datasize, char *data)
{
	return -ENOSYS;
}
EXPORT_SYMBOL(stm_lpm_get_wakeup_info);

/**
 * stm_lpm_reset() - To reset part of full SOC
 * @reset_type:	type of reset
 *
 * External SBC firmware does not support this.
 */

int stm_lpm_reset(enum stm_lpm_reset_type reset_type)
{
	return -ENOSYS;
}
EXPORT_SYMBOL(stm_lpm_reset);

/**
 * lpm_recv_response_i2c() - To get response from SBC
 * @command_id:	command id of message
 * @msg:	message pointer
 *
 * Return - Code returned from i2c_transfer
 */

static int lpm_recv_response_i2c(char command_id, struct lpm_message *msg)
{
	struct i2c_msg i2c_message = {
		.addr = ADDRESS_OF_EXT_MC,
		.flags = I2C_M_RD,
		.buf = (char *)msg,
		.len = 2
	};
	int err;

	/* Fill message expected length */
	switch (command_id) {
	default:
		break;

	case LPM_MSG_VER:
		i2c_message.len = 7;
		break;
	case LPM_MSG_READ_RTC:
		i2c_message.len = 8;
		break;
	case LPM_MSG_GET_STATUS:
	case LPM_MSG_GET_WUD:
		i2c_message.len = 4;
		break;
	case LPM_MSG_GET_ADV_FEA:
		i2c_message.len = 8;
	}
	/* Transact with SBC */
	err = i2c_transfer(lpm_drv->i2c_sbc_adapter, &i2c_message, 1);
	lpm_debug("i2c_transfer response  is %d \n", err);
	return err;
}

/**
 * lpm_send_msg() - Send message to SBC
 * @msg:	message pointer
 * @msg_size: 	message size
 *
 * Return - Code returned from i2c_transfer
 */

static int lpm_send_msg(char *msg, unsigned char msg_size)
{
	int err;
	struct i2c_msg i2c_message = {
		.addr = ADDRESS_OF_EXT_MC,
		.flags = 0,
		.buf = (char *) msg,
		.len = msg_size+2
	};
	lpm_debug("lpm_send_msg, size of msg %d \n", msg_size + 2);
	err = i2c_transfer(lpm_drv->i2c_sbc_adapter, &i2c_message, 1);
	lpm_debug("i2c_transfer send  is %d \n", err);
	return err;
}

/**
 * lpm_exchange_msg() - Internal function  used for message exchange with SBC
 * @send_msg:	message to send
 * @response:	response from SBC firmware
 *
 * Return - 0 on success
 * Return - negative error code on failure
 */

int lpm_exchange_msg(struct lpm_internal_send_msg *send_msg,
		struct lpm_message *response)
{
	char  *lpm_msg = NULL;
	struct lpm_message msg = {0};
	int err = 0;
	int response_needed = 1;

	lpm_debug("lpm_exchange_msg \n");
	if (unlikely(send_msg->msg_size > LPM_MAX_MSG_DATA)) {
		/* We would enter into this condition very rarely */
		lpm_msg = kmalloc(send_msg->msg_size + 2, GFP_KERNEL);
		if (unlikely(!lpm_msg))
			return -ENOMEM;
	} else {
		lpm_msg = (char *)&msg;
	}

	if (response == NULL)
		response_needed = 0;

	mutex_lock(&lpm_drv->msg_protection_mutex);
	lpm_msg[0] = send_msg->command_id;
	lpm_msg[1] = lpm_drv->gbltrans_id++;
	if (send_msg->msg_size)
		memcpy(&lpm_msg[2], send_msg->msg, send_msg->msg_size);

	lpm_debug("Send msg {%x, %x } \n", lpm_msg[0], lpm_msg[1]);

	err = lpm_send_msg(lpm_msg, send_msg->msg_size);
	if (unlikely(err <= 0)) {
		pr_err("f/w is not responding \n");
		err = -EAGAIN;
		goto exit;
	}
	if (response_needed == 0)
		goto exit;

	err = lpm_recv_response_i2c(send_msg->command_id, response);
	if (unlikely(err <= 0)) {
		pr_err("f/w reply not received \n");
		err = -EAGAIN;
		goto exit;
	}
	lpm_debug("recd reply  %d {%x, %x } \n", err, response->command_id,
		response->transaction_id);

	BUG_ON(!(response->command_id & LPM_MSG_REPLY));
	if (lpm_msg[1] == response->transaction_id) {
		if (response->command_id == LPM_MSG_ERR) {
			pr_err("f/w error code %d \n", response->msg_data[1]);
			/*
			 * Firmware does not support this command
			 * therefore firmware gave error.
			 * In such cases, return EREMOTEIO as firmware error
			 * code is not yet decided.
			 * To Do
			 * conversion of firmware error into Linux world
			 */
			err = -EREMOTEIO;
		}
	} else {
		/* In case of i2c, same tran id is expected as of command */
		pr_err("Received  ID %d expected %d", response->transaction_id,
				lpm_msg[1]);
		err = -EREMOTEIO;
	}

exit:
	mutex_unlock(&lpm_drv->msg_protection_mutex);

	/* If we have allocated lpm_msg then free this message */
	if (send_msg->msg_size > LPM_MSG_REPLY)
		kfree(lpm_msg);
	if (err > 0)
		err = 0;
	return err;
}

/**
 * lpm_enter_passive_standby() - To inform SBC to get ready for standby
 *
 * This will inform SBC to get ready for standby
 * Actual power off command to SBC will be send by kernel itself
 * by generating i2c violation.
 *
 * Return code 0 on success
 * Return code  negative error code on failure.
 */

static int lpm_enter_passive_standby(void)
{
	struct lpm_internal_send_msg send_msg = {
		.command_id = LPM_MSG_ENTER_PASSIVE,
		.msg_size = 0
	};
	return lpm_exchange_msg(&send_msg, NULL);
}

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
 * stm_lpm_probe() - Probe function of driver
 * @client_data:	i2c client data
 * @id:	2c device id
 *
 * Return - 0 on success
 * Return - negative error  on failure
 */

static int __init stm_lpm_probe(struct i2c_client *client_data,
				const struct i2c_device_id *id)
{
	struct stm_lpm_i2c_data *i2c_data;
	int err = 0;
	struct stm_lpm_version driver_ver, fw_ver;
	lpm_debug("stm lpm probe \n");
	/* Allocate data structure */
	lpm_drv = kzalloc(sizeof(struct stm_lpm_driver_data), GFP_KERNEL);
	if (unlikely(lpm_drv == NULL)) {
		pr_err("%s: Request memory not done\n", __func__);
		return -ENOMEM;
	}
	i2c_data = i2c_get_clientdata(client_data);
	if (unlikely(i2c_data == NULL)) {
		pr_err("No i2c_bus data\n");
		err = -ENOENT;
		goto exit;
	}

	lpm_drv->i2c_sbc_adapter = i2c_data->i2c_adap;
	if (lpm_drv->i2c_sbc_adapter == NULL) {
		pr_err("i2c adapter not found \n");
		err = -ENODEV;
		goto exit;
	}
	lpm_debug("stm lpm i2c adapter found at %d i2c is %x \n",
	i2c_data->number_i2c, (unsigned int)lpm_drv->i2c_sbc_adapter);

	/* Mark parent */
	client_data->dev.parent = &lpm_drv->i2c_sbc_adapter->dev;
	/* Mutex initialization */
	mutex_init(&lpm_drv->msg_protection_mutex);
	err = stm_lpm_get_version(&driver_ver, &fw_ver);
	if (unlikely(err < 0)) {
		pr_err("No SBC firmware available \n");
		goto exit;
	}
	lpm_drv->fw_major_ver = fw_ver.major_comm_protocol;
#ifdef CONFIG_STM_LPM_RD_MONITOR
	/* Start monitor front panel power key */
	err = lpm_start_power_monitor(client_data);
#endif
	return err;
exit:
	kfree(lpm_drv);
	return err;
}

/**
 * stm_lpm_remove() - To free used resources
 * @client:	i2c client data
 * Return code  0
 */

static int stm_lpm_remove(struct i2c_client *client)
{
	lpm_debug("stm_lpm_remove \n");
#ifdef CONFIG_STM_LPM_RD_MONITOR
	lpm_stop_power_monitor(client);
#endif
	kfree(lpm_drv);
	return 0;
}

/**
 * stm_lpm_freeze() - Freeze callback
 * @i2c_data:	i2c client data
 *
 * Return - 0 on sucess
 * Return - negative error on failure
 */

static int stm_lpm_freeze(struct i2c_client *i2c_data)
{
	lpm_debug("stm_lpm_freeze state \n");
	return lpm_enter_passive_standby();
}

/**
 * stm_lpm_restore() - Restore callback
 * @i2c_data:	i2c client data
 *
 * Return - 0 on success
 */

static int stm_lpm_restore(struct i2c_client *i2c_data)
{
	struct stm_lpm_i2c_data *i2c_device_data;
	lpm_debug("stm_lpm_restore \n");
	i2c_device_data = i2c_get_clientdata(i2c_data);
	/* We return from CPS, mark power key as not pressed */
	i2c_device_data->status_gpio = 1;
	return 0;
}

static struct i2c_driver stm_lpm_driver = {
	.driver.name = "stm-lpm",
	.driver.owner = THIS_MODULE,
	.probe = stm_lpm_probe,
	.remove = stm_lpm_remove,
	.freeze = stm_lpm_freeze,
	.restore = stm_lpm_restore,
	.id_table = lpm_ids,
};

static int __init stm_lpm_init(void)
{
	int err = -ENXIO;
	err = i2c_add_driver(&stm_lpm_driver);
	lpm_debug("stm lpm init err %d \n", err);
	return err;
}

void __exit stm_lpm_exit(void)
{
	i2c_del_driver(&stm_lpm_driver);
	lpm_debug("stm lpm driver removed \n");
}

module_init(stm_lpm_init);
module_exit(stm_lpm_exit);

MODULE_AUTHOR("STMicroelectronics  <www.st.com>");
MODULE_DESCRIPTION("lpm device driver for STMicroelectronics devices");
MODULE_LICENSE("GPL");
