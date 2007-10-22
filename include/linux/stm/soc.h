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
	int phy_addr;
	unsigned int phy_mask;
	char *phy_name;
	int pbl;
	int (*phy_reset)(void* priv);
	void (*fix_mac_speed)(void *priv, unsigned int speed);
	void* bsp_priv;
};

struct plat_usb_data {
	unsigned long ahb2stbus_wrapper_glue_base;
	unsigned long ahb2stbus_protocol_base;
	void (*power_up)(void* dev);
	int initialised;
	int port_number;
};

#endif /* __LINUX_ST_SOC_H */
