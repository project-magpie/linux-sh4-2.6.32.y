#ifndef __LINUX_STM_SOC_H
#define __LINUX_STM_SOC_H

#include <linux/stm/lirc.h>
#include <linux/compiler.h>

/* This is the private platform data for the ssc driver */
struct ssc_pio_t {
	struct {
		unsigned char pio_port;
		unsigned char pio_pin;
	} pio[3]; /* clk, in, out */
	int clk_unidir;
	struct stpio_pin* clk;
	struct stpio_pin* sdout;
	struct stpio_pin* sdin;
	/* chip-select for SPI bus (struct spi_device *spi) -> (void *)*/
	void (*chipselect)(void *spi, int is_on);
};

#define SSC_I2C_CAPABILITY  0x00
#define SSC_SPI_CAPABILITY  0x01
#define SSC_UNCONFIGURED    0x02
#define SSC_I2C_CLK_UNIDIR  0x04

#define SSC_BITS_SIZE       0x03
/*
 *   This macro could be used to build the capability field
 *   of struct plat_ssc_data for each SoC
 */
#define ssc_capability(idx_ssc, cap)  \
	(((cap) & \
	 (SSC_I2C_CAPABILITY | SSC_SPI_CAPABILITY |\
	  SSC_UNCONFIGURED | SSC_I2C_CLK_UNIDIR)) \
	  << ((idx_ssc)*SSC_BITS_SIZE))

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

/* Set pio[x].pio_port to SSC_NO_PIO for hard wired SSC's */
#define SSC_NO_PIO	0xff

struct plat_ssc_data {
	unsigned short		capability;	/* bitmask on the ssc capability */
	unsigned long		routing;
	/* chip-select for SPI bus (struct spi_device *spi) -> (void *)*/
	void (*spi_chipselects[])(void *spi, int is_on);
};

#ifdef CONFIG_CPU_SUBTYPE_STX5197
/* SSC0 routine depends on whether port configured for SPI or I2C */

#define SSC1_QAM_SCLT_SDAT	(0<<1)
#define SSC1_QPSK		(1<<1)
#endif

#ifdef CONFIG_CPU_SUBTYPE_STX7105
#define SSC_SCLK_SHIFT(n)	(0+(n*6))
#define SSC_MTSR_SHIFT(n)	(2+(n*6))
#define SSC_MRST_SHIFT(n)	(4+(n*6))

#define SSC2_SCLK_PIO3_4	(0 << SSC_SCLK_SHIFT(2))
#define SSC2_SCLK_PIO12_0	(2 << SSC_SCLK_SHIFT(2))
#define SSC2_SCLK_PIO13_4	(3 << SSC_SCLK_SHIFT(2))

#define SSC2_MTSR_PIO3_5	(0 << SSC_MTSR_SHIFT(2))
#define SSC2_MTSR_PIO12_1	(1 << SSC_MTSR_SHIFT(2))
#define SSC2_MTSR_PIO13_5	(2 << SSC_MTSR_SHIFT(2))

#define SSC2_MRST_PIO2_0	(0 << SSC_MRST_SHIFT(2))
#define SSC2_MRST_PIO3_5	(1 << SSC_MRST_SHIFT(2))
#define SSC2_MRST_PIO12_1	(2 << SSC_MRST_SHIFT(2))
#define SSC2_MRST_PIO13_5	(3 << SSC_MRST_SHIFT(2))

#define SSC3_SCLK_PIO3_6	(0 << SSC_SCLK_SHIFT(3))
#define SSC3_SCLK_PIO13_2	(1 << SSC_SCLK_SHIFT(3))
#define SSC3_SCLK_PIO13_6	(2 << SSC_SCLK_SHIFT(3))

#define SSC3_MTSR_PIO3_7	(0 << SSC_MTSR_SHIFT(3))
#define SSC3_MTSR_PIO13_3	(1 << SSC_MTSR_SHIFT(3))
#define SSC3_MTSR_PIO13_7	(2 << SSC_MTSR_SHIFT(3))

#define SSC3_MRST_PIO2_1	(0 << SSC_MRST_SHIFT(3))
#define SSC3_MRST_PIO3_7	(1 << SSC_MRST_SHIFT(3))
#define SSC3_MRST_PIO13_3	(2 << SSC_MRST_SHIFT(3))
#define SSC3_MRST_PIO13_7	(3 << SSC_MRST_SHIFT(3))
#endif

#define SPI_LINE_SHIFT		0x0
#define SPI_LINE_MASK		0x7
#define SPI_BANK_SHIFT		0x3
#define SPI_BANK_MASK		0xff
#define spi_get_bank(address)  (((address) >> SPI_BANK_SHIFT) & SPI_BANK_MASK)
#define spi_get_line(address)  (((address) >> SPI_LINE_SHIFT) & SPI_LINE_MASK)
#define spi_set_cs(bank, line) ((((bank) & SPI_BANK_MASK) << SPI_BANK_SHIFT) | \
				 (((line) & SPI_LINE_MASK) << SPI_LINE_SHIFT))
/* each spi bus is able to manage 'all' the pios as chip selector
   therefore each master must have 8(pioline)x20(piobank)
   20 pio banks is enough for our boards
   SPI_NO_CHIPSELECT to specify SPI device with no CS (ie CS tied to 'active')
*/
#define SPI_NO_CHIPSELECT	(spi_set_cs(19, 7) + 1)


#define PCI_PIN_ALTERNATIVE -2 	/* Use alternative PIO rather than default */
#define PCI_PIN_DEFAULT -1 	/* Use whatever the default is for that pin */
#define PCI_PIN_UNUSED   0	/* Pin not in use */

/* In the board setup, you can pass in the external interrupt numbers instead
 * if you have wired up your board that way. It has the advantage that the PIO
 * pins freed up can then be used for something else
 */
struct pci_config_data {
	int pci_irq[4]; /* PCI_PIN_DEFAULT/PCI_PIN_UNUSED. Other IRQ can be passed in */
	int serr_irq;   /* As above for SERR */
	char idsel_lo;	/* Lowest address line connected to an idsel  - slot 0 */
	char idsel_hi;	/* Highest address line connected to an idsel - slot n */
	char req_gnt[4]; /* Set to PCI_PIN_DEFAULT if the corresponding req/gnt lines are in use */
	unsigned long pci_clk; /* PCI clock in Hz. If zero default to 33MHz */

	/* If you supply a pci_reset() function, that will be used to reset the
	 * PCI bus.  Otherwise it is assumed that the reset is done via PIO,
	 * the number is specified here. Specify -EINVAL if no PIO reset is
	 * required either, for example if the PCI reset is done as part of
	 * power on reset.
	 */
	unsigned pci_reset_pio;
	void (*pci_reset)(void);

	/* Various PCI tuning parameters. Set by SOC layer. You don't have to specify
	 * these as the defaults are usually fine. However, if you need to change them, you
	 * can set ad_override_default and plug in your own values
	 */
	unsigned ad_threshold:4;
	unsigned ad_chunks_in_msg:5;
	unsigned ad_pcks_in_chunk:5;
	unsigned ad_trigger_mode:1;
	unsigned ad_posted:1;
	unsigned ad_max_opcode:4;
	unsigned ad_read_ahead:1;

	unsigned ad_override_default:1; /* Set to override default values for your board */

	/* Cut3 7105/ cut 2 7141 connected req0 pin to req3 to work around some
	 * problems with nand. This bit will be auto-probed by the chip layer,
	 * the board layer should NOT have to set this
	 */
	unsigned req0_to_req3:1;
};

u8 pci_synopsys_inb(unsigned long port);
u16 pci_synopsys_inw(unsigned long port);
u32 pci_synopsys_inl(unsigned long port);

u8 pci_synopsys_inb_p(unsigned long port);
u16 pci_synopsys_inw_p(unsigned long port);
u32 pci_synopsys_inl_p(unsigned long port);

void pci_synopsys_insb(unsigned long port, void *dst, unsigned long count);
void pci_synopsys_insw(unsigned long port, void *dst, unsigned long count);
void pci_synopsys_insl(unsigned long port, void *dst, unsigned long count);

void pci_synopsys_outb(u8 val, unsigned long port);
void pci_synopsys_outw(u16 val, unsigned long port);
void pci_synopsys_outl(u32 val, unsigned long port);

void pci_synopsys_outb_p(u8 val, unsigned long port);
void pci_synopsys_outw_p(u16 val, unsigned long port);
void pci_synopsys_outl_p(u32 val, unsigned long port);

void pci_synopsys_outsb(unsigned long port, const void *src, unsigned long count);
void pci_synopsys_outsw(unsigned long port, const void *src, unsigned long count);
void pci_synopsys_outsl(unsigned long port, const void *src, unsigned long count);

/* Macro used to fill in the IO machine vector at the board level */
#ifdef CONFIG_SH_ST_SYNOPSYS_PCI
/* We have to hook all the in/out functions as they cannot be memory
 * mapped with the synopsys PCI IP
 *
 * Also, for PCI we use the generic iomap implementation, and so do
 * not need the ioport_map function, instead using the generic cookie
 * based implementation.
 */
#define STM_PCI_IO_MACHINE_VEC(board)		\
	.mv_inb = pci_synopsys_inb,		\
        .mv_inw = pci_synopsys_inw,		\
        .mv_inl = pci_synopsys_inl,		\
        .mv_outb = pci_synopsys_outb,		\
        .mv_outw = pci_synopsys_outw,		\
        .mv_outl = pci_synopsys_outl,		\
        .mv_inb_p = pci_synopsys_inb_p,		\
        .mv_inw_p = pci_synopsys_inw,		\
        .mv_inl_p = pci_synopsys_inl,		\
        .mv_outb_p = pci_synopsys_outb_p,	\
        .mv_outw_p = pci_synopsys_outw,		\
        .mv_outl_p = pci_synopsys_outl,		\
        .mv_insb = pci_synopsys_insb,		\
        .mv_insw = pci_synopsys_insw,		\
        .mv_insl = pci_synopsys_insl,		\
        .mv_outsb = pci_synopsys_outsb,		\
        .mv_outsw = pci_synopsys_outsw,		\
        .mv_outsl = pci_synopsys_outsl,
#else
#define STM_PCI_IO_MACHINE_VEC(board)	board##_ioport_map,
#endif

#define STM_MACHINE_VEC(board)				\
	.mv_name		= #board,		\
	.mv_setup		= board##_setup,	\
	.mv_nr_irqs		= NR_IRQS,		\
	.mv_init_irq		= board##_init_irq,	\
	STM_PCI_IO_MACHINE_VEC(board)

/* Private data for the SATA driver */
struct plat_sata_data {
	unsigned long phy_init;
	unsigned long pc_glue_logic_init;
	unsigned int only_32bit;
};

/* Private data for the PWM driver */
struct plat_stm_pwm_data {
	unsigned long flags;
	unsigned long routing;
};

#define PLAT_STM_PWM_OUT0	(1<<0)
#define PLAT_STM_PWM_OUT1	(1<<1)

#ifdef CONFIG_CPU_SUBTYPE_STX7105
#define PWM_OUT0_PIO4_4		(0 << 0)
#define PWM_OUT0_PIO13_0	(1 << 0)
#define PWM_OUT1_PIO4_5		(0 << 1)
#define PWM_OUT1_PIO13_1	(1 << 1)
#endif

/* Platform data for the temperature sensor driver */
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

/* Private data for the STM on-board ethernet driver */
struct plat_stmmacenet_data {
	int bus_id;
	int pbl;
	int has_gmac;
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
	unsigned long flags;
};

#define USB_FLAGS_STRAP_8BIT			(1<<0)
#define USB_FLAGS_STRAP_16BIT			(2<<0)
#define USB_FLAGS_STRAP_PLL			(1<<2)
#define USB_FLAGS_OPC_MSGSIZE_CHUNKSIZE		(1<<3)
#define USB_FLAGS_STBUS_CONFIG_THRESHOLD128	(1<<4)
#define USB_FLAGS_STBUS_CONFIG_THRESHOLD256	(2<<4)

/**
 * struct usb_init_data - initialisation data for a USB port
 * @oc_en: enable OC detection (0 or 1)
 * @oc_actlow: whether OC detection is active low (0 or 1)
 * @oc_pinsel: use alternate pin for OC detection (0 or 1)
 * @pwr_en: enable power enable (0 or 1)
 * @pwr_pinsel: use alternate pin for power enable (0 or 1)
 */
struct usb_init_data {
	char oc_en;
	char oc_actlow;
	int oc_pinsel;
	char pwr_en;
	int pwr_pinsel;
};

#ifdef CONFIG_CPU_SUBTYPE_STX7105
#define USB0_OC_PIO4_4		0
#define USB0_OC_PIO12_5		1
#define USB0_PWR_PIO4_5		0
#define USB0_PWR_PIO12_6	1

#define USB1_OC_PIO4_6		0
#define USB1_OC_PIO14_6		1
#define USB1_PWR_PIO4_7		0
#define USB1_PWR_PIO14_7	1
#endif



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



struct stasc_uart_data {
	struct {
		unsigned char pio_port;
		unsigned char pio_pin;
		unsigned char pio_direction;
	} pios[4]; /* TXD, RXD, CTS, RTS */
	unsigned char flags;
};

#define STASC_FLAG_NORTSCTS	1

extern int stasc_console_device;
extern struct platform_device *stasc_configured_devices[];
extern unsigned int stasc_configured_devices_count;

#ifdef CONFIG_CPU_SUBTYPE_STX7141
#define ASC1_MCARD		0
#define ASC1_PIO10		2

#define ASC2_PIO1		0
#define ASC2_PIO6		4
#endif

#define PLAT_SYSCONF_GROUP(_id, _offset) \
	{ \
		.group = _id, \
		.offset = _offset, \
		.name = #_id \
	}

struct plat_sysconf_group {
	int group;
	unsigned long offset;
	const char *name;
	const char *(*field_name)(int num);
};

struct plat_sysconf_data {
	int groups_num;
	struct plat_sysconf_group *groups;
};



/* STM NAND configuration data (plat_nand/STM_NAND_EMI/FLEX/AFM) */
struct plat_stmnand_data {
	/* plat_nand/STM_NAND_EMI paramters */
	unsigned int	emi_withinbankoffset;  /* Offset within EMI Bank      */
	int		rbn_port;		/*  # : 'nand_RBn' PIO port   */
						/* -1 : if unconnected	      */
	int		rbn_pin;	        /*      'nand_RBn' PIO pin    */
						/* (assumes shared RBn signal */
						/*  for multiple chips)	      */

	/* STM_NAND_EMI/FLEX/AFM paramters */
	void		*timing_data;		/* Timings for EMI/NandC      */
	unsigned char	flex_rbn_connected;	/* RBn signal connected?      */
						/* (Required for NAND_AFM)    */

	/* Legacy data for backwards compatibility with plat_nand driver      */
	/*   will be removed once all platforms updated to use STM_NAND_EMI!  */
	unsigned int	emi_bank;		/* EMI bank                   */
	void		*emi_timing_data;	/* Timing data for EMI config */
	void		*mtd_parts;		/* MTD partition table	      */
	int		nr_parts;		/* Numer of partitions	      */
	unsigned int	chip_delay;		/* Read-busy delay	      */

};


void stx5197_early_device_init(void);
void stx5197_configure_asc(const int *ascs, int num_ascs, int console);
void stx5197_configure_usb(void);
void stx5197_configure_ethernet(int rmii, int ext_clk, int phy_bus);
void stx5197_configure_ssc(struct plat_ssc_data *data);
void stx5197_configure_pwm(struct plat_stm_pwm_data *data);
void stx5197_configure_lirc(void);

void stx7100_early_device_init(void);
void stb7100_configure_asc(const int *ascs, int num_ascs, int console);
void sysconf_early_init(struct platform_device *pdevs, int pdevs_num);
void stpio_early_init(struct platform_device *pdev, int num_pdevs, int irq);

void stx7100_configure_sata(void);
void stx7100_configure_pwm(struct plat_stm_pwm_data *data);
void stx7100_configure_ssc(struct plat_ssc_data *data);
void stx7100_configure_usb(void);
void stx7100_configure_ethernet(int rmii_mode, int ext_clk, int phy_bus);
void stx7100_configure_lirc(void);
void stx7100_configure_pata(int bank, int pc_mode, int irq);

void stx7105_configure_sata(void);
void stx7105_early_device_init(void);
void stx7105_configure_asc(const int *ascs, int num_ascs, int console);
void stx7105_configure_pwm(struct plat_stm_pwm_data *data);
void stx7105_configure_ssc(struct plat_ssc_data *data);
void stx7105_configure_usb(int port, struct usb_init_data *data);
void stx7105_configure_ethernet(int reverse_mii, int rmii_mode, int mode,
				int ext_mdio, int ext_clk, int phy_bus);
void stx7105_configure_nand(struct platform_device *pdev);
void stx7105_configure_lirc(void);
void stx7105_configure_pata(int bank, int pc_mode, int irq);
void stx7105_configure_audio_pins(int pcmout, int spdif, int pcmin);
void stx7105_configure_pci(struct pci_config_data *pci_config);
int  stx7105_pcibios_map_platform_irq(struct pci_config_data *pci_config, u8 pin);

void stx7111_early_device_init(void);
void stx7111_configure_asc(const int *ascs, int num_ascs, int console);
void stx7111_configure_pwm(struct plat_stm_pwm_data *data);
void stx7111_configure_ssc(struct plat_ssc_data *data);
void stx7111_configure_usb(int inv_enable);
void stx7111_configure_ethernet(int en_mii, int sel, int ext_clk, int phy_bus);
void stx7111_configure_nand(struct plat_stmnand_data *data);
void stx7111_configure_lirc(void);
void stx7111_configure_pci(struct pci_config_data *pci_config);
int  stx7111_pcibios_map_platform_irq(struct pci_config_data *pci_config, u8 pin);

void stx7141_early_device_init(void);
void stx7141_configure_asc(const int *ascs, int num_ascs, int console);
void stx7141_configure_sata(void);
void stx7141_configure_pwm(struct plat_stm_pwm_data *data);
void stx7141_configure_ssc(struct plat_ssc_data *data);
void stx7141_configure_usb(int port);
void stx7141_configure_ethernet(int port, int reverse_mii, int mode,
				int phy_bus);
void stx7141_configure_audio_pins(int pcmout1, int pcmout2, int spdif,
		int pcmin1, int pcmint2);
void stx7141_configure_lirc(void);

void stx7200_early_device_init(void);
void stx7200_configure_asc(const int *ascs, int num_ascs, int console);
void stx7200_configure_pwm(struct plat_stm_pwm_data *data);
void stx7200_configure_ssc(struct plat_ssc_data *data);
void stx7200_configure_usb(int port);
void stx7200_configure_sata(unsigned int port);
void stx7200_configure_ethernet(int mac, int rmii_mode, int ext_clk,
				int phy_bus);
void stx7200_configure_lirc(void);
void stx7200_configure_nand(struct platform_device *pdev);
void stx7200_configure_pata(int bank, int pc_mode, int irq);

void stm_sata_miphy_init(void);

#endif /* __LINUX_ST_SOC_H */
