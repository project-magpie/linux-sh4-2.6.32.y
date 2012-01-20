/*---------------------------------------------------------------------------
* /include/linux/stm/lpm.h
* Copyright (C) 2011 STMicroelectronics Limited
* Contributor:Francesco Virlinzi <francesco.virlinzi@st.com>
* Author:Pooja Agarwal <pooja.agarwal@st.com>
* Author:Udit Kumar <udit-dlh.kumar@st.cm>
* May be copied or modified under the terms of the GNU General Public
* License.  See linux/COPYING for more information.
*----------------------------------------------------------------------------
*/
#ifndef __LPM_H
#define __LPM_H

#include <linux/rtc.h>
#define STM_LPM_MAX_BYTE_COUNT 4

/*
* stm_lpm_wakeupdevices
*
* Define wakeup devices
*/
enum stm_lpm_wakeup_devices{
		STM_LPM_WAKEUP_IR = 1<<0,
		STM_LPM_WAKEUP_CEC = 1<<1,
		STM_LPM_WAKEUP_FRP = 1<<2,
		STM_LPM_WAKEUP_WOL = 1<<3,
		STM_LPM_WAKEUP_RTC = 1<<4,
		STM_LPM_WAKEUP_ASC = 1<<5,
		STM_LPM_WAKEUP_NMI = 1<<6,
		STM_LPM_WAKEUP_HPD = 1<<7
};

/*
* stlpm_resettype_e
*
* Define reset type
*/
enum stm_lpm_reset_type{
		STM_LPM_SOC_RESET = 1<<0,
		STM_LPM_LPM_RESET = 1<<1,
		STM_LPM_BOOT_RESET = 1<<2
};

/*
* stm_lpm_sbcstate
*
* Define state of SBC software
*/
enum stm_lpm_sbc_state{
		STM_LPM_SBC_BOOT = 1,
		STM_LPM_SBC_WAIT = 3,
		STM_LPM_SBC_RUNNING = 4,
		STM_LPM_SBC_STANDBY = 5
};

/*
* stlpm_version
*
* Defines the version information of STLPM
*/
struct stm_lpm_version{
		char major_comm_protocol;
		char minor_comm_protocol;
		char major_soft;
		char minor_soft;
		char patch_soft;
		char month;
		char day;
		char year;
};

/*
* stlpm_fpsetting
*
* Defines the frontpanel display settings
*/
struct stm_lpm_fp_setting{
		char owner;
		char am_pm;
		char brightness;
};

/*
* stlpm_irsetting
*
* Defines IR settings
*/
struct stm_lpm_ir_setting{
	unsigned char byte_code[STM_LPM_MAX_BYTE_COUNT];
};

int stm_lpm_configure_wdt(u16 time_in_ms);

int stm_lpm_get_fw_state(enum stm_lpm_sbc_state *fw_state);

int stm_lpm_get_wakeup_device(enum stm_lpm_wakeup_devices *wakeupdevice);

int stm_lpm_get_wakeup_info(enum stm_lpm_wakeup_devices *wakeupdevice,
	int *validsize, int datasize, int *data) ;

int stm_lpm_get_version(struct stm_lpm_version *drv_ver,
	struct stm_lpm_version *fw_ver);

int stm_lpm_reset(enum stm_lpm_reset_type reset_type);

int stm_lpm_setup_fp(struct stm_lpm_fp_setting *fp_setting);

int stm_lpm_setup_ir(int num_of_sequence, int byte_count, int protocol,
	struct stm_lpm_ir_setting *byte_sequence);

int stm_lpm_set_rtc(struct rtc_time new_rtc);

int stm_lpm_set_wakeup_device(unsigned char wakeup_devices);

int stm_lpm_set_wakeup_time(unsigned int timeout);

#endif /*__LPM_H*/
