#ifndef __LINUX_STM_STX7100_H
#define __LINUX_STM_STX7100_H

#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/stm/platform.h>


void stx7100_early_device_init(void);


struct stx7100_asc_config {
	int hw_flow_control;
	int is_console;
};
void stx7100_configure_asc(int asc, struct stx7100_asc_config *config);


struct stx7100_ssc_spi_config {
	void (*chipselect)(struct spi_device *spi, int is_on);
};
/* SSC configure functions return I2C/SPI bus number */
int stx7100_configure_ssc_i2c(int ssc);
int stx7100_configure_ssc_spi(int ssc, struct stx7100_ssc_spi_config *config);


struct stx7100_lirc_config {
	enum {
		stx7100_lirc_rx_disabled,
		stx7100_lirc_rx_mode_ir,
		stx7100_lirc_rx_mode_uhf
	} rx_mode;
	int tx_enabled;
	int tx_od_enabled;
};
void stx7100_configure_lirc(struct stx7100_lirc_config *config);


struct stx7100_pwm_config {
	int out0_enabled;
	int out1_enabled;
};
void stx7100_configure_pwm(struct stx7100_pwm_config *config);


struct stx7100_ethernet_config {
	enum {
		stx7100_ethernet_mode_mii,
		stx7100_ethernet_mode_rmii,
	} mode;
	int ext_clk;
	int phy_bus;
};
void stx7100_configure_ethernet(struct stx7100_ethernet_config *config);


void stx7100_configure_usb(void);


void stx7100_configure_sata(void);


struct stx7100_pata_config {
	int emi_bank;
	int pc_mode;
	unsigned int irq;
};
void stx7100_configure_pata(struct stx7100_pata_config *config);


#endif
