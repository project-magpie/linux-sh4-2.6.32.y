/*
 * Copyright (C) 2007-09 STMicroelectronics Limited
 * Author: Angelo Castello <angelo.castello@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#define LIRC_STM_NAME "lirc_stm"

/*
 * start code detect (SCD) support
 */
struct lirc_scd_s {
	unsigned int code;		/* code symbols to be detect. */
	unsigned int codelen;		/* start code length */
	unsigned int alt_code;		/* alternative SCD to be detected */
	unsigned int alt_codelen;	/* alternative start code length */
	unsigned int nomtime;		/* nominal symbol time in us */
	unsigned int noiserecov;	/* noise recovery configuration */
};

#define LIRC_SCD_CONFIGURE             _IOW('i', 0x00000021, __u32)
#define LIRC_SCD_ENABLE                _IOW('i', 0x00000022, __u32)
#define LIRC_SCD_DISABLE               _IOW('i', 0x00000023, __u32)
#define LIRC_SCD_STATUS                _IOW('i', 0x00000024, __u32)

/*
 * This is the private platform data for the lirc driver
 */
#define LIRC_PIO_ON		0x08	/* PIO pin available */
#define LIRC_IR_RX		0x04	/* IR RX PIO line available */
#define LIRC_IR_TX		0x02	/* IR TX PIOs lines available */
#define LIRC_UHF_RX		0x01	/* UHF RX PIO line available */

struct lirc_pio_s {
	unsigned int bank;
	unsigned int pin;
	unsigned int dir;
	char pinof;
	struct stpio_pin *pinaddr;
};

struct lirc_plat_data_s {
	unsigned int irbclock;		/* IRB clock (0 == auto) */
	unsigned int irbclkdiv;		/* IRB clock divisor (0 == auto) */
	unsigned int irbperiodmult;	/* manual period multiplier */
	unsigned int irbperioddiv;	/* manual period divisor */
	unsigned int irbontimemult;	/* manual pulse period multiplier */
	unsigned int irbontimediv;	/* manual pulse period divisor */
	unsigned int irbrxmaxperiod;	/* maximum rx period in uS */
	unsigned int irbversion;	/* IRB version type (1,2 or 3) */
	unsigned int sysclkdiv;		/* factor to divide system bus clock */
	unsigned int rxpolarity;        /* gpio rx polarity (usually is 1) */
	unsigned int subcarrwidth;      /* Subcarrier width in percent */
	struct lirc_pio_s *pio_pin_arr;	/* PIO pin array for driver */
	unsigned int num_pio_pins;
#ifdef CONFIG_PM
	unsigned long clk_on_low_power; /* system clock rate in lowpower mode */
#endif
};
