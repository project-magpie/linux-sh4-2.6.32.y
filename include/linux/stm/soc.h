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

#endif /* __LINUX_ST_SOC_H */
