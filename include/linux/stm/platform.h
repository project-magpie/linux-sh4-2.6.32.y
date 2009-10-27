#ifndef __LINUX_STM_PLATFORM_H
#define __LINUX_STM_PLATFORM_H

#include <linux/gpio.h>
#include <linux/lirc.h>
#include <linux/device.h>
#include <linux/gpio.h>
#include <linux/spi/spi.h>
#include <linux/stm/pad.h>
#include <linux/stm/nand.h>


/*** Platform definition helpers ***/

#define STM_PLAT_RESOURCE_MEM(_start, _size) \
		{ \
			.start = (_start), \
			.end = (_start) + (_size) - 1, \
			.flags = IORESOURCE_MEM, \
		}

#if defined(CONFIG_CPU_SUBTYPE_ST40)

#define STM_PLAT_RESOURCE_IRQ(_st40, _st200) \
		{ \
			.start = (_st40), \
			.end = (_st40), \
			.flags = IORESOURCE_IRQ, \
		}

#define STM_PLAT_RESOURCE_IRQ_NAMED(_name, _st40, _st200) \
		{ \
			.start = (_st40), \
			.end = (_st40), \
			.name = (_name), \
			.flags = IORESOURCE_IRQ, \
		}

#else

#error Unknown architecture

#endif

#define STM_PLAT_RESOURCE_DMA(_req_line) \
		{ \
			.start = (_req_line), \
			.end = (_req_line), \
			.flags = IORESOURCE_DMA, \
		}

#define STM_PLAT_RESOURCE_DMA_NAMED(_name, _req_line) \
		{ \
			.start = (_req_line), \
			.end = (_req_line), \
			.name = (_name), \
			.flags = IORESOURCE_DMA, \
		}



/*** ASC platform data ***/

struct stm_plat_asc_data {
	int hw_flow_control:1;
	struct stm_pad_config *pad_config;
};

extern int stm_asc_console_device;
extern unsigned int stm_asc_configured_devices_num;
extern struct platform_device *stm_asc_configured_devices[];



/*** SSC platform data ***/

struct stm_plat_ssc_data {
	unsigned gpio_sclk; /* I2C = SCL, SPI = Clock */
	unsigned gpio_mtsr; /* I2C = SDA, SPI = Master Transmit Slave Receive */
	unsigned gpio_mrst; /* SPI (only) = Master Receive Slave Transmit */

	struct stm_pad_config *pad_config_ssc;
	struct stm_pad_config *pad_config_gpio;

	void (*spi_chipselect)(struct spi_device *, int);
};



/*** LiRC platform data ***/

struct stm_plat_lirc_data {
	unsigned int irbclock;		/* IRB block clock
					 * (set to 0 for auto) */
	unsigned int irbclkdiv;		/* IRB block clock divison
					 * (set to 0 for auto) */
	unsigned int irbperiodmult;	/* manual setting period multiplier */
	unsigned int irbperioddiv;	/* manual setting period divisor */
	unsigned int irbontimemult;	/* manual setting pulse period
					 * multiplier */
	unsigned int irbontimediv;	/* manual setting pulse period
					 * divisor */
	unsigned int irbrxmaxperiod;	/* maximum rx period in uS */
	unsigned int irbversion;	/* IRB version type (1,2 or 3) */
	unsigned int sysclkdiv;		/* factor to divide system bus
					   clock by */
	unsigned int rxpolarity;	/* flag to set gpio rx polarity
					 * (usually set to 1) */
	unsigned int subcarrwidth;	/* Subcarrier width in percent - this
					 * is used to make the subcarrier
					 * waveform square after passing
					 * through the 555-based threshold
					 * detector on ST boards */
	struct stm_pad_config *pads;	/* pads to be claimed */
	unsigned int rxuhfmode:1;	/* RX UHF mode enabled */
	unsigned int txenabled:1;	/* TX operation is possible */
};



/*** PWM platform data ***/

/* Private data for the PWM driver */
struct stm_plat_pwm_data {
	int channel_enabled[2];
	struct stm_pad_config *channel_pad_config[2];
};


/*** Temperature sensor data ***/

struct plat_stm_temp_data {
	const char *name;
	struct {
		int group, num, lsb, msb;
	} pdn, dcorrect, overflow, data;
	int calibrated:1;
	int calibration_value;
	void (*custom_set_dcorrect)(void *priv);
	unsigned long (*custom_get_data)(void *priv);
	void *custom_priv;
};


/*** Ethernet (STMMAC) platform data ***/

/* Private data for the STM on-board ethernet driver */
struct stm_plat_stmmacenet_data {
	int bus_id;
	int pbl;
	int has_gmac;
	void (*fix_mac_speed)(void *priv, unsigned int speed);
	void (*hw_setup)(void);

	struct stm_pad_config *pad_config;

	void *bsp_priv;
};

struct stm_plat_stmmacphy_data {
	int bus_id;
	int phy_addr;
	unsigned int phy_mask;
	int interface;
	int (*phy_reset)(void *priv);
	void *priv;
};



/*** USB platform data ***/

#define STM_PLAT_USB_FLAGS_STRAP_8BIT			(1<<0)
#define STM_PLAT_USB_FLAGS_STRAP_16BIT			(2<<0)
#define STM_PLAT_USB_FLAGS_STRAP_PLL			(1<<2)
#define STM_PLAT_USB_FLAGS_OPC_MSGSIZE_CHUNKSIZE	(1<<3)
#define STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD128	(1<<4)
#define STM_PLAT_USB_FLAGS_STBUS_CONFIG_THRESHOLD256	(2<<4)

struct stm_plat_usb_data {
	unsigned long flags;
	struct stm_pad_config *pad_config;
};



/*** SATA platform data ***/

struct stm_plat_sata_data {
	unsigned long phy_init;
	unsigned long pc_glue_logic_init;
	unsigned int only_32bit;
};


/** PIO platform data ***/

#define STM_PLAT_PIO_DATA_PAD_LABEL(_port_no, _pin_no) \
	{ \
		.labels_num = 1, \
		.labels = (struct stm_pad_label []) { \
			STM_PAD_LABEL("PIO" #_port_no "." #_pin_no), \
		}, \
	}

#define STM_PLAT_PIO_DATA_LABELS_ONLY(_port_no) \
	(struct stm_plat_pio_data) { \
		.pad_configs = (struct stm_pad_config []) { \
			[0] = STM_PLAT_PIO_DATA_PAD_LABEL(_port_no, 0), \
			[1] = STM_PLAT_PIO_DATA_PAD_LABEL(_port_no, 1), \
			[2] = STM_PLAT_PIO_DATA_PAD_LABEL(_port_no, 2), \
			[3] = STM_PLAT_PIO_DATA_PAD_LABEL(_port_no, 3), \
			[4] = STM_PLAT_PIO_DATA_PAD_LABEL(_port_no, 4), \
			[5] = STM_PLAT_PIO_DATA_PAD_LABEL(_port_no, 5), \
			[6] = STM_PLAT_PIO_DATA_PAD_LABEL(_port_no, 6), \
			[7] = STM_PLAT_PIO_DATA_PAD_LABEL(_port_no, 7), \
		}, \
	}

struct stm_plat_pio_data {
	struct stm_pad_config *pad_configs;
};



/*** Sysconf block platform data ***/

#define PLAT_SYSCONF_GROUP(_id, _offset) \
	{ \
		.group = _id, \
		.offset = _offset, \
		.name = #_id \
	}

struct stm_plat_sysconf_group {
	int group;
	unsigned long offset;
	const char *name;
	const char *(*field_name)(int num);
};

struct stm_plat_sysconf_data {
	int groups_num;
	struct stm_plat_sysconf_group *groups;
};



/*** NAND flash platform data ***/

struct stm_plat_nand_flex_data {
	unsigned int nr_banks;
	struct stm_nand_bank_data *banks;
	unsigned int flex_rbn_connected:1;
	struct stm_pad_config *pad_config;
};

struct stm_plat_nand_emi_data {
	unsigned int nr_banks;
	struct stm_nand_bank_data *banks;
	int emi_rbn_gpio;
};



/*** FDMA platform data ***/

struct stm_plat_fdma_slim_regs {
	unsigned long id;
	unsigned long ver;
	unsigned long en;
	unsigned long clk_gate;
};

struct stm_plat_fdma_periph_regs {
	unsigned long sync_reg;
	unsigned long cmd_sta;
	unsigned long cmd_set;
	unsigned long cmd_clr;
	unsigned long cmd_mask;
	unsigned long int_sta;
	unsigned long int_set;
	unsigned long int_clr;
	unsigned long int_mask;
};

struct stm_plat_fdma_hw {
	struct stm_plat_fdma_slim_regs slim_regs;
	struct stm_plat_fdma_periph_regs periph_regs;
	unsigned long dmem_offset;
	unsigned long dmem_size;
	unsigned long imem_offset;
	unsigned long imem_size;
};

struct stm_plat_fdma_fw_regs {
	unsigned long rev_id;
	unsigned long cmd_statn;
	unsigned long req_ctln;
	unsigned long ptrn;
	unsigned long cntn;
	unsigned long saddrn;
	unsigned long daddrn;
};

struct stm_plat_fdma_fw {
	const char *name;
	struct stm_plat_fdma_fw_regs fw_regs;
	void *dmem;
	unsigned long dmem_len;
	void *imem;
	unsigned long imem_len;
};

struct stm_plat_fdma_data {
	struct stm_plat_fdma_hw *hw;
	struct stm_plat_fdma_fw *fw;
	int min_ch_num;
	int max_ch_num;
};

/*** PCI platform data ***/

#define PCI_PIN_ALTERNATIVE	-2	/* Use alternative PIO rather than default */
#define PCI_PIN_DEFAULT		-1	/* Use whatever the default is for that pin */
#define PCI_PIN_UNUSED		0	/* Pin not in use */

/*
 * In the board setup, you can pass in the external interrupt numbers
 * instead if you have wired up your board that way. It has the
 * advantage that the PIO pins freed up can then be used for something
 * else.
 */
struct stm_plat_pci_config {
	int pci_irq[4];		/* PCI_PIN_DEFAULT/PCI_PIN_UNUSED.
				 * Other IRQ can be passed in */
	int serr_irq;		/* As above for SERR */
	char idsel_lo;		/* Lowest address line connected to an
				 * idsel  - slot 0 */
	char idsel_hi;		/* Highest address line connected to an
				 * idsel - slot n */
	char req_gnt[4];	/* Set to PCI_PIN_DEFAULT if the
				 * corresponding req/gnt lines are in use */
	unsigned pci_clk;	/* PCI clock rate in Hz. If zero will
				 * default to 33MHz*/

	/*
	 * If you supply a pci_reset() function, that will be used to reset the
	 * PCI bus.  Otherwise it is assumed that the reset is done via PIO,
	 * the number is specified here. Specify -EINVAL if no PIO reset is
	 * required either, for example if the PCI reset is done as part of
	 * power on reset.
	 */
	unsigned pci_reset_pio;
	void (*pci_reset)(void);

	/*
	 * Various PCI tuning parameters. Set by SOC layer. You don't
	 * have to specify these as the defaults are usually
	 * fine. However, if you need to change them, you can set
	 * ad_override_default and plug in your own values
	 */
	unsigned ad_threshold:4;
	unsigned ad_chunks_in_msg:5;
	unsigned ad_pcks_in_chunk:5;
	unsigned ad_trigger_mode:1;
	unsigned ad_posted:1;
	unsigned ad_max_opcode:4;
	unsigned ad_read_ahead:1;
	unsigned ad_override_default:1; /* Set to override default
					 * values for your board */

	/*
	 * Cut3 7105/ cut 2 7141 connected req0 pin to req3 to work
	 * around some problems with nand. This bit will be
	 * auto-probed by the chip layer, the board layer should NOT
	 * have to set this.
	 */
	unsigned req0_to_req3:1;

};

#endif
