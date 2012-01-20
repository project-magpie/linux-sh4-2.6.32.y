/* ---------------------------------------------------------------------------
 * Copyright (C) 2012 STMicroelectronics Limited
 *
 * Author: Pooja Agarwal <pooja.agarwal@st.com>
 * Author: Udit Kumar <udit-dlh.kumar@st.cm>
 * Contributor: Francesco Virlinzi <francesco.virlinzi@st.com>
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
#include "lpm_def.h"

#ifdef CONFIG_STM_LPM_DEBUG
#define lpm_debug printk
#else
#define lpm_debug(fmt, arg...);
#endif

int stm_lpm_get_version(struct stm_lpm_version *driver_version,
	struct stm_lpm_version *fw_version)
{
	int err = 0;
	struct stm_lpm_message response = {0};
	struct stlpm_internal_send_msg	send_msg;
	if ((driver_version == NULL) || (fw_version == NULL))
		return -EINVAL;

		LPM_FILL_MSG(send_msg, STM_LPM_MSG_VER, NULL, MSG_ZERO_SIZE,
		MSG_ID_AUTO, SBC_REPLY_YES);
	err = lpm_exchange_msg(&send_msg, &response);
	if (err == 0) {
		fw_version->major_comm_protocol = response.msg_data[0] >> 4;
		fw_version->minor_comm_protocol = response.msg_data[0] & 0x0F;
		fw_version->major_soft = response.msg_data[1] >> 4;
		fw_version->minor_soft = response.msg_data[1] & 0x0F;
		fw_version->patch_soft = response.msg_data[2] >> 4;
		fw_version->month = response.msg_data[2] & 0x0F;
		memcpy(&fw_version->day, &response.msg_data[3], 2);

		driver_version->major_comm_protocol = STM_LPM_MAJOR_PROTO_VER;
		driver_version->minor_comm_protocol = STM_LPM_MINOR_PROTO_VER;
		driver_version->major_soft = STM_LPM_MAJOR_SOFT_VER;
		driver_version->minor_soft = STM_LPM_MINOR_SOFT_VER;
		driver_version->patch_soft = STM_LPM_PATCH_SOFT_VER;
		driver_version->month = STM_LPM_BUILD_MONTH;
		driver_version->day = STM_LPM_BUILD_DAY;
		driver_version->year = STM_LPM_BUILD_YEAR;
	}
	return err;
}
EXPORT_SYMBOL(stm_lpm_get_version);

int stm_lpm_configure_wdt(u16 time_in_ms)
{
	char msg[2] = {0};
	struct stlpm_internal_send_msg	send_msg = {0};
	struct stm_lpm_message response;
	if (!time_in_ms)
		return -EINVAL;
	memcpy(msg, &time_in_ms, 2);
	LPM_FILL_MSG(send_msg, STM_LPM_MSG_SET_WDT, msg, 2, MSG_ID_AUTO,
	SBC_REPLY_YES);
	return lpm_exchange_msg(&send_msg, &response);
}
EXPORT_SYMBOL(stm_lpm_configure_wdt);

int stm_lpm_get_fw_state(enum stm_lpm_sbc_state *fw_state)
{
	int err = 0;
	struct stlpm_internal_send_msg send_msg;
	struct stm_lpm_message reply_msg = {0};
	if (fw_state == NULL)
		return -EINVAL;
	LPM_FILL_MSG(send_msg, STM_LPM_MSG_GET_STATUS, NULL, MSG_ZERO_SIZE,
	MSG_ID_AUTO, SBC_REPLY_YES);
	err = lpm_exchange_msg(&send_msg, &reply_msg);

	if (reply_msg.command_id == (STM_LPM_MSG_GET_STATUS|STM_LPM_MSG_REPLY))
		*fw_state = reply_msg.msg_data[0];
	return err;
}
EXPORT_SYMBOL(stm_lpm_get_fw_state);

int stm_lpm_reset(enum stm_lpm_reset_type reset_type)
{
	int err = 0;
	struct stlpm_internal_send_msg send_msg = {0};
	struct stm_lpm_message response;
	char msg = reset_type;
	if (!reset_type)
		return -EINVAL;
	msg = reset_type;
	LPM_FILL_MSG(send_msg, STM_LPM_MSG_GEN_RESET, &msg, 1, MSG_ID_AUTO,
	SBC_REPLY_NO);
	err = lpm_exchange_msg(&send_msg, &response);
	return err;
}
EXPORT_SYMBOL(stm_lpm_reset);

int stm_lpm_set_wakeup_device(unsigned char devices)
{
	int err = 0;
	char msg[2] = {0};
	struct stm_lpm_message response;
	struct stlpm_internal_send_msg send_msg = {0};
	msg[0] = devices;
	LPM_FILL_MSG(send_msg, STM_LPM_MSG_SET_WUD, msg, 2, MSG_ID_AUTO,
	SBC_REPLY_NO_IRQ);
	err = lpm_exchange_msg(&send_msg, &response);
	return err;
}
EXPORT_SYMBOL(stm_lpm_set_wakeup_device);

int stm_lpm_set_wakeup_time(unsigned int timeout)
{
	int err = 0;
	char msg[4] = {0};
	struct stm_lpm_message response;
	struct stlpm_internal_send_msg send_msg = {0};
	memcpy(msg, &timeout, 4);
	LPM_FILL_MSG(send_msg, STM_LPM_MSG_SET_TIMER, msg, 4, MSG_ID_AUTO,
	SBC_REPLY_YES);
	err = lpm_exchange_msg(&send_msg, &response);
	return err;
}
EXPORT_SYMBOL(stm_lpm_set_wakeup_time);

int stm_lpm_set_rtc(struct rtc_time new_rtc)
{
	int err = 0;
	char msg[3] = {0};
	struct stm_lpm_message response = {0};
	struct stlpm_internal_send_msg send_msg = {0};

	msg[2] = new_rtc.tm_sec;
	msg[1] = new_rtc.tm_min;
	msg[0] = new_rtc.tm_hour;
	LPM_FILL_MSG(send_msg, STM_LPM_MSG_SET_RTC, msg, 3, MSG_ID_AUTO,
	SBC_REPLY_YES);
	err = lpm_exchange_msg(&send_msg, &response);
	return err;
}
EXPORT_SYMBOL(stm_lpm_set_rtc);

int stm_lpm_get_wakeup_device(enum stm_lpm_wakeup_devices *wakeup_device)
{
	int err = 0;
	struct stm_lpm_message response = {0};
	struct stlpm_internal_send_msg send_msg;
	if (wakeup_device == NULL)
		return -EINVAL;

	LPM_FILL_MSG(send_msg, STM_LPM_MSG_GET_WUD, NULL, MSG_ZERO_SIZE,
	MSG_ID_AUTO, SBC_REPLY_NO_IRQ);
	err = lpm_exchange_msg(&send_msg, &response);
	if (response.command_id == (STM_LPM_MSG_GET_WUD|STM_LPM_MSG_REPLY))
		*wakeup_device = response.msg_data[0];
	return err;
}
EXPORT_SYMBOL(stm_lpm_get_wakeup_device);

int stm_lpm_setup_fp(struct stm_lpm_fp_setting *fp_setting)
{
	char msg[4] = {0};
	int err = 0;
	struct stm_lpm_message response;
	struct stlpm_internal_send_msg send_msg = {0};
	if (fp_setting == NULL)
		return -EINVAL;

	msg[0] = ((fp_setting->owner&OWNER_MASK)|((fp_setting->am_pm & 1) << 2)
	| (0 << 3) | ((fp_setting->brightness & BRIGHT_MASK) << 4));
	LPM_FILL_MSG(send_msg, STM_LPM_MSG_SET_FP, msg, 1, MSG_ID_AUTO,
	SBC_REPLY_YES);
	err = lpm_exchange_msg(&send_msg, &response);
	return err;
}
EXPORT_SYMBOL(stm_lpm_setup_fp);

