#ifndef __LINUX_ST_SOC_H
#define __LINUX_ST_SOC_H

/* This is the private platform data for the ssc driver */
struct ssc_pio_t {
	unsigned char pio_port;
	unsigned char pio_pin[3];
	struct stpio_pin* clk;
	struct stpio_pin* sdout;
	struct stpio_pin* sdin;
};

#define SSC_I2C_CAPABILITY  0x0
#define SSC_SPI_CAPABILITY  0x1
#define SSC_UNCONFIGURED    0x2
/*
 *   This macro could be used to build the capability field
 *   of struct plat_ssc_data for each SoC
 */
#define ssc_capability(idx_ssc, cap)  \
         ( (cap) & (SSC_I2C_CAPABILITY | SSC_SPI_CAPABILITY | SSC_UNCONFIGURED) ) << ((idx_ssc)*2)

#define ssc0_has(cap)  ssc_capability(0,cap)
#define ssc1_has(cap)  ssc_capability(1,cap)
#define ssc2_has(cap)  ssc_capability(2,cap)
#define ssc3_has(cap)  ssc_capability(3,cap)
#define ssc4_has(cap)  ssc_capability(4,cap)
#define ssc5_has(cap)  ssc_capability(5,cap)
#define ssc6_has(cap)  ssc_capability(6,cap)
#define ssc7_has(cap)  ssc_capability(7,cap)
#define ssc8_has(cap)  ssc_capability(8,cap)
#define ssc9_has(cap)  ssc_capability(9,cap)

struct plat_ssc_data {
	unsigned short		capability;	/* bitmask on the ssc capability */
};


/* Private data for the SATA driver */
struct plat_sata_data {
	unsigned long phy_init;
	unsigned long pc_glue_logic_init;
	unsigned int only_32bit;
};

/* Private data for the PWM driver */
struct plat_stm_pwm_data {
	unsigned long flags;
};

#define PLAT_STM_PWM_OUT0	(1<<0)
#define PLAT_STM_PWM_OUT1	(1<<1)

/* This is the private platform data for the lirc driver */
struct lirc_pio {
	unsigned int bank;
	unsigned int pin;
	unsigned int dir;
        struct stpio_pin* pinaddr;
};

struct plat_lirc_data {
	unsigned int irbclock;		/* IRB block clock (set to 0 for auto) */
	unsigned int irbclkdiv;		/* IRB block clock divisor (set to 0 for auto) */
	unsigned int irbperiodmult;	/* manual setting period multiplier */
	unsigned int irbperioddiv;	/* manual setting period divisor */
	unsigned int irbontimemult;	/* manual setting pulse period multiplier */
	unsigned int irbontimediv;	/* manual setting pulse period divisor */
	unsigned int irbrxmaxperiod;	/* maximum rx period in uS */
	unsigned int irbversion;	/* IRB version type (1,2 or 3) */
	unsigned int sysclkdiv;		/* factor to divide system bus clock by */
	unsigned int rxpolarity;        /* flag to set gpio rx polarity (usually set to 1) */
	unsigned int subcarrwidth;      /* Subcarrier width in percent - this is used to */
					/* make the subcarrier waveform square after passing */
					/* through the 555-based threshold detector on ST boards */
	struct lirc_pio *pio_pin_arr;	/* PIO pin settings for driver */
	unsigned int num_pio_pins;
};

/* Private data for the STM on-board ethernet driver */
struct plat_stmmacenet_data {
	int bus_id;
	int pbl;
	void (*fix_mac_speed)(void *priv, unsigned int speed);
	void (*hw_setup)(void);
	void *bsp_priv;
};

struct plat_stmmacphy_data {
	int bus_id;
	int phy_addr;
	unsigned int phy_mask;
	int interface;
	int (*phy_reset)(void *priv);
	void *priv;
};

struct plat_usb_data {
	unsigned long ahb2stbus_wrapper_glue_base;
	unsigned long ahb2stbus_protocol_base;
	void (*power_up)(void* dev);
	int initialised;
	int port_number;
};

struct stasc_uart_data {
	unsigned char pio_port;
	unsigned char pio_pin[4]; /* Tx, Rx, CTS, RTS */
};

extern struct platform_device *asc_default_console_device;

struct plat_sysconf_data {
	int sys_device_offset;
	int sys_sta_offset;
	int sys_cfg_offset;
};

/* NAND configuration data */
struct nand_config_data {
	unsigned int emi_bank;			/* EMI Bank#			*/
	unsigned int emi_withinbankoffset;	/* Offset within EMI Bank	*/
	void *emi_timing_data;			/* Timing data for EMI config   */
	void *mtd_parts;			/* MTD partition table		*/
	unsigned int chip_delay;		/* Read busy time for NAND chip */
	int nr_parts;				/* Number of partitions		*/
	int rbn_port;				/*  # : 'nand_RBn' PIO port #   */
						/* -1 : if unconnected		*/
	int rbn_pin;			        /*      'nand_RBn' PIO pin      */
						/* (assumes shared RBn signal   */
						/*  for multiple chips)		*/
};



/* Audio subsystem board level configuration -
 * PCM formats & additional ALSA controls */
struct plat_audio_config {
	/* PCM format configuration - logical sum of PLAT_STM_AUDIO__*
	 * flags listed below */
	unsigned long pcm_format;

	/* Required oversampling value (leave 0 to get default 256 ;-) */
	unsigned int oversampling;
};

/* If CUSTOM bit is defined the rest of pcm_format will be treated
 * as a AUD_PCMOUT_FMT register content and mirrored there; this
 * is not recommended - be aware of chips differences (eg. hardware
 * bug of inverted SCLK_EDGE on 7100 and _some_ cuts of 7109...) */
#define PLAT_STM_AUDIO__CUSTOM          0x80000000
/* Select data size & digital serial audio format; note that 0 describes
 * I2S 24 bits mode, which is actually default :-) */
#define PLAT_STM_AUDIO__OUTPUT_SUBFRAME_32_BITS 0x00000000
#define PLAT_STM_AUDIO__OUTPUT_SUBFRAME_16_BITS 0x00000001
#define PLAT_STM_AUDIO__DATA_SIZE_24_BITS       0x00000000
#define PLAT_STM_AUDIO__DATA_SIZE_20_BITS       0x00000010
#define PLAT_STM_AUDIO__DATA_SIZE_18_BITS       0x00000020
#define PLAT_STM_AUDIO__DATA_SIZE_16_BITS       0x00000030
#define PLAT_STM_AUDIO__FORMAT_I2S              0x00000000
#define PLAT_STM_AUDIO__FORMAT_LEFT_JUSTIFIED   0x00000100
#define PLAT_STM_AUDIO__FORMAT_RIGHT_JUSTIFIED  0x00000200
/* Bit masks for above settings */
#define PLAT_STM_AUDIO__OUTPUT_SUBFRAME_MASK    0x0000000f
#define PLAT_STM_AUDIO__DATA_SIZE_MASK          0x000000f0
#define PLAT_STM_AUDIO__FORMAT_MASK             0x00000f00



void stx7100_early_device_init(void);
void stb7100_configure_asc(const int *ascs, int num_ascs, int console);
void sysconf_early_init(struct platform_device *pdev);
void stpio_early_init(struct platform_device *pdev, int num_pdevs);

void stx7100_configure_sata(void);
void stx7100_configure_pwm(struct plat_stm_pwm_data *data);
void stx7100_configure_ssc(struct plat_ssc_data *data);
void stx7100_configure_usb(void);
void stx7100_configure_ethernet(int rmii_mode, int ext_clk, int phy_bus);
void stx7100_configure_lirc(void);
void stx7100_configure_pata(int bank, int irq);
void stx7100_configure_audio(struct plat_audio_config *pcm_reader_config,
		struct plat_audio_config *pcm_player_0_config,
		struct plat_audio_config *pcm_player_1_config);

void stx7200_early_device_init(void);
void stx7200_configure_asc(const int *ascs, int num_ascs, int console);

void stx7200_configure_pwm(struct plat_stm_pwm_data *data);
void stx7200_configure_ssc(struct plat_ssc_data *data);
void stx7200_configure_usb(void);
void stx7200_configure_ethernet(int mac, int rmii_mode, int ext_clk,
				int phy_bus);
void stx7200_configure_lirc(void);
void stx7200_configure_nand(struct nand_config_data *data);
void stx7200_configure_audio(struct plat_audio_config *pcm_reader_config,
		struct plat_audio_config *pcm_player_0_config,
		struct plat_audio_config *pcm_player_1_config,
		struct plat_audio_config *pcm_player_2_config,
		struct plat_audio_config *pcm_player_3_config,
		struct plat_audio_config *hdmi_pcm_player_config);

#endif /* __LINUX_ST_SOC_H */
