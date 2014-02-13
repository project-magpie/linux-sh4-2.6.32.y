/*
 * <root>/include/linux/stm/lpm.h
 *
 * Interface file for stm lpm driver
 *
 * Copyright (C) 2012 STMicroelectronics Limited
 *
 * Contributor:Francesco Virlinzi <francesco.virlinzi@st.com>
 * Author:Pooja Agarwal <pooja.agarwal@st.com>
 * Author:Udit Kumar <udit-dlh.kumar@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public License.
 * See linux/COPYING for more information.
 */


#ifndef __LPM_H
#define __LPM_H

#include <linux/rtc.h>

/**
 * enum stm_lpm_wakeup_devices- define wakeup devices
 * One bit for each wakeup device
 */

enum stm_lpm_wakeup_devices{
	STM_LPM_WAKEUP_IR = 1<<0,
	STM_LPM_WAKEUP_CEC = 1<<1,
	STM_LPM_WAKEUP_FRP = 1<<2,
	STM_LPM_WAKEUP_WOL = 1<<3,
	STM_LPM_WAKEUP_RTC = 1<<4,
	STM_LPM_WAKEUP_ASC = 1<<5,
	STM_LPM_WAKEUP_NMI = 1<<6,
	STM_LPM_WAKEUP_HPD = 1<<7,
	STM_LPM_WAKEUP_PIO = 1<<8,
	STM_LPM_WAKEUP_EXT = 1<<9
};

/**
 * enum stm_lpm_reset_type - define reset type
 * @STM_LPM_SOC_RESET:	SOC reset
 * @STM_LPM_SBC_RESET:	Only SBC reset
 * @STM_LPM_BOOT_RESET:	reset SBC and stay in bootloader
 */
enum stm_lpm_reset_type{
	STM_LPM_SOC_RESET = 0,
	STM_LPM_SBC_RESET = 1<<0,
	STM_LPM_BOOT_RESET = 1<<1
};

/**
 * enum stm_lpm_sbc_state - defines SBC state
 * @STM_LPM_SBC_BOOT:	SBC waiting in bootloader
 * @STM_LPM_SBC_RUNNING:	SBC is running
 * @STM_LPM_SBC_STANDBY:	Entering into standby
 */

enum stm_lpm_sbc_state{
	STM_LPM_SBC_BOOT = 1,
	STM_LPM_SBC_RUNNING = 4,
	STM_LPM_SBC_STANDBY = 5
};

/**
 * struct stm_lpm_version - define version information
 * @major_comm_protocol:	Supported Major protocol version
 * @minor_comm_protocol:	Supported Minor protocol version
 * @major_soft:	Major software version
 * @minor_soft:	Minor software version
 * @patch_soft:	Software patch version
 * @month:	Software build month version
 * @day:	Software build day
 * @year:	Software build year
 *
 * Same struct is used for firmware and driver version information
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

/**
 * struct stm_lpm_fp_setting - define front panel setting
 * @owner:	Owner of front panel
 * 		when 0 - SBC firmware will be owner in standby
 *		when 1 - SBC firmware always own frontpanel display
 *		when 2 - Host will always own front panel
 * @am_pm:	AM/PM indicator, when 0 clock will be displayed in 24 hrs format
 * @brightness:	brightness of display, [0-3] bits are used max value is 15
 * This is to inform SBC how front panel display will be used
 */
struct stm_lpm_fp_setting{
	char owner;
	char am_pm;
	char brightness;
};

/**
 * enum stm_lpm_pio_use - to define how pio can be used
 * @STM_LPM_PIO_POWER:	PIO used for power control
 * @STM_LPM_PIO_ETH_MDINT:	PIO used for phy WOL
 * @STM_LPM_PIO_WAKEUP:	PIO used as GPIO interrupt for wakeup
 * @STM_LPM_PIO_EXT_IT:	PIO used as external interrupt
 * @STM_LPM_PIO_OTHER:	Reserved
 */

enum stm_lpm_pio_use{
	STM_LPM_PIO_POWER = 1,
	STM_LPM_PIO_ETH_MDINT = 2,
	STM_LPM_PIO_WAKEUP = 3,
	STM_LPM_PIO_EXT_IT = 4,
	STM_LPM_PIO_OTHER = 5,
};

/**
 * struct stm_lpm_pio_setting - define PIO use
 * @pio_bank:	pio bank number
 * @pio_pin:	pio pin number, valid values [0-7]
 * @pio_direction:	direction of PIO
 *		0 means, pio is used as input.
 *		1 means, pio is used as output.
 * @interrupt_enabled:	If interrupt on this PIO is enabled
 *		0 means, interrupts are disabled.
 *		1 means, interrupt are enabled.
 *		This must be set to 0 when pio is used as output.
 * @pio_level:	PIO level high or low.
 *		0 means, Interrupt/Power off will be done when PIO goes low.
 *		1 means, Interrupt/Power off will be done when PIO goes high.
 * @pio_use:	use of this pio
 *
 */

struct stm_lpm_pio_setting{
	char pio_bank;
	char pio_pin;
	bool pio_direction;
	bool interrupt_enabled;
	bool pio_level;
	enum stm_lpm_pio_use  pio_use;
};

/**
 * enum stm_lpm_adv_feature_name - Define name of advance feature of SBC
 * @STM_LPM_USE_EXT_VCORE:	feature is external VCORE for SBC
 * @STM_LPM_USE_INT_VOLT_DETECT:	internal low voltage detect
 * @STM_LPM_EXT_CLOCK:	external clock
 * @STM_LPM_RTC_SOURCE: RTC source for SBC
 * @STM_LPM_WU_TRIGGERS: wakeup triggers
 */

enum stm_lpm_adv_feature_name{
	STM_LPM_USE_EXT_VCORE = 1,
	STM_LPM_USE_INT_VOLT_DETECT = 2,
	STM_LPM_EXT_CLOCK = 3,
	STM_LPM_RTC_SOURCE = 4,
	STM_LPM_WU_TRIGGERS = 5,
};

/**
 * struct stm_lpm_adv_feature - define advance feature struct
 * @feature_name:	Name of feature
 * @set_params:	Used to set feature on SBC
 *
 * 		when features is STM_LPM_USE_EXT_VCORE,
 *		set_parmas[0] = 0 means Internal Vcore
 *		set_parmas[0] = 1 means Internal Vcore
 *		set_parmas[1] is unused
 *
 * 		when features is STM_LPM_USE_INT_VOLT_DETECT
 *		set_parmas[0] is voltage value to detect low voltage
 *		i..e for 3.3V use 33 and for 3.0V use 20
 *		set_parmas[1] is unused
 *
 * 		when features is STM_LPM_EXT_CLOCK
 *		set_parmas[0] = 1 means external
 *		set_parmas[0] = 2 means external ACG
 *		set_parmas[0] = 3 means Track_32K
 *		set_parmas[1] is unused
 *
 * 		when features is STM_LPM_RTC_SOURCE
 *		set_parmas[0] = 1 means RTC_32K_TCXO
 *		set_parmas[0] = 2 means external ACG
 *		set_parmas[0] = 3 means RTC_32K_OSC
 *		set_parmas[1] is unused
 *
 * 		when features is STM_LPM_WU_TRIGGERS
 *		set_parmas[0-1] is bit map for each wakeup trigger
 *		as defined in enum stm_lpm_wakeup_devices
 * @get_params: Used to get advance feature of SBC
 *		get_params[0-3] is feature set supported by SBC , TBD
 *		get_params[4-5] is wakeup triggers
 *
 */
struct stm_lpm_adv_feature{
	enum stm_lpm_adv_feature_name feature_name;
	union {
		unsigned char set_params[2];
		unsigned char get_params[6];
	} params;
};

/* defines  MAX depth for IR FIFO */
#define MAX_IR_FIFO_DEPTH 64

/**
 * struct stm_lpm_ir_fifo - define one element of IR fifo
 * @mark:	Mark time
 * @symbol:	Symbol time
 *
 */
struct stm_lpm_ir_fifo{
	u16 mark;
	u16 symbol;
};

/**
 * sturct stm_lpm_ir_key - define raw data for IR key
 * @key_index:	Key Index acts as key identifier
 * @num_patterns:	Number of mark/symbol pattern define this key
 * @fifo:	Place holder for mark/symbol data
 *
 * Max value of fifo is kept 64, which is max value of IR IP
 */

struct stm_lpm_ir_key{
	u8 key_index;
	u8 num_patterns;
	struct stm_lpm_ir_fifo fifo[MAX_IR_FIFO_DEPTH];
};


/**
 * struct stm_lpm_ir_keyinfo - define a IR key along with another info
 * @ir_id:	Id of IR hardware, use id 0 for first IR, 1 for second IR
 * 		use id 0x80 for first UHF and 0x81 for second so on
 * @time_period:	Time period for this key , this is dependent on protocol
 * @time_out:	Time out period for this key
 * @tolerance:	Expected tolerance in IR key from standard value
 * @ir_key:	IR key data
 *
 */

struct stm_lpm_ir_keyinfo{
	u8 ir_id;
	u16 time_period;
	u16 time_out;
	u8 tolerance;
	struct stm_lpm_ir_key ir_key;
};

int stm_lpm_configure_wdt(u16 time_in_ms);

int stm_lpm_get_fw_state(enum stm_lpm_sbc_state *fw_state);

int stm_lpm_get_wakeup_device(enum stm_lpm_wakeup_devices *wakeupdevice);

int stm_lpm_get_wakeup_info(enum stm_lpm_wakeup_devices *wakeupdevice,
	u16 *validsize, u16 datasize, char  *data) ;

int stm_lpm_get_version(struct stm_lpm_version *drv_ver,
	struct stm_lpm_version *fw_ver);

int stm_lpm_reset(enum stm_lpm_reset_type reset_type);

int stm_lpm_setup_fp(struct stm_lpm_fp_setting *fp_setting);


int stm_lpm_setup_ir(u8 num_keys, struct stm_lpm_ir_keyinfo *keys);

int stm_lpm_set_rtc(struct rtc_time *new_rtc);

int stm_lpm_set_wakeup_device(u16  wakeup_devices);

int stm_lpm_set_wakeup_time(u32 timeout);

int stm_lpm_setup_pio(struct stm_lpm_pio_setting *pio_setting);

int stm_lpm_setup_keyscan(u16 key_data);

int stm_lpm_set_adv_feature(u8 enabled, struct stm_lpm_adv_feature *feature);

int stm_lpm_get_adv_feature(unsigned char all_features,
				struct stm_lpm_adv_feature *feature);

enum stm_lpm_config_reboot_type {
	stm_lpm_reboot_with_ddr_self_refresh,
	stm_lpm_reboot_with_ddr_off
};

void stm_lpm_config_reboot(enum stm_lpm_config_reboot_type type);

void stm_lpm_power_off(void);
#endif /*__LPM_H*/
