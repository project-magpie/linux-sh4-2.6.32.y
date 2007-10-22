#ifndef __LINUX_ST_SOC_H
#define __LINUX_ST_SOC_H

/* This is the private platform data for the ssc driver */
struct plat_ssc_pio_t {
	unsigned char sclbank;
	unsigned char sclpin;
	unsigned char sdoutbank;
	unsigned char sdoutpin;
	unsigned char sdinbank;
	unsigned char sdinpin;
};

#define SSC_I2C_CAPABILITY  0x1
#define SSC_SPI_CAPABILITY  0x2

struct plat_ssc_data {
	unsigned short		capability;	/* bitmask on the ssc capability */
	struct plat_ssc_pio_t	*pio;		/* the PIO map */
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

void stx7100_early_device_init(void);
void stb7100_configure_asc(const int *ascs, int num_ascs, int console);
void sysconf_early_init(struct platform_device *pdev);
void stpio_early_init(struct platform_device *pdev, int num_pdevs);

void stx7100_configure_sata(void);
void stx7100_configure_pwm(struct plat_stm_pwm_data *data);
void stx7100_configure_ssc(struct plat_ssc_data *data);
void stx7100_configure_usb(void);
void stx7100_configure_alsa(void);
void stx7100_configure_ethernet(int rmii_mode, int ext_clk, int phy_bus);

void stx7200_early_device_init(void);
void stx7200_configure_asc(const int *ascs, int num_ascs, int console);

void stx7200_configure_pwm(struct plat_stm_pwm_data *data);
void stx7200_configure_ssc(struct plat_ssc_data *data);
void stx7200_configure_usb(void);
void stx7200_configure_ethernet(int mac, int rmii_mode, int ext_clk,
				int phy_bus);

#endif /* __LINUX_ST_SOC_H */
