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
};

#endif /* __LINUX_ST_SOC_H */
