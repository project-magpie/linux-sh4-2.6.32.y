#ifndef __LINUX_STM_STX5197_H
#define __LINUX_STM_STX5197_H

#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/stm/platform.h>


void stx5197_early_device_init(void);


struct stx5197_asc_config {
	int hw_flow_control;
	int is_console;
};
void stx5197_configure_asc(int asc, struct stx5197_asc_config *config);


struct stx5197_ssc_i2c_config {
	union {
		enum {
			/* SCL = PIO1.6, SDA = PIO1.7 */
			stx5197_ssc0_i2c_pio1,
			/* SCL = SPI_CLK, SDA = SPI_DATAIN */
			stx5197_ssc0_i2c_spi,

		} ssc0;
		enum {
			/* internal bus */
			stx5197_ssc1_i2c_qpsk,
			/* SCL = QAM_SCLT, SDA = QAM_SDAT */
			stx5197_ssc1_i2c_qam,
		} ssc1;
		enum {
			/* SCL = PIO3.3, SDA = PIO3.2 */
			stx5197_ssc2_i2c_pio3,
		} ssc2;
	} routing;
};
/* SSC configure functions return I2C/SPI bus number */
int stx5197_configure_ssc_i2c(int ssc, struct stx5197_ssc_i2c_config *config);
int stx5197_configure_ssc_spi(int ssc);


struct stx5197_lirc_config {
	enum {
		stx5197_lirc_rx_disabled,
		stx5197_lirc_rx_mode_ir,
		stx5197_lirc_rx_mode_uhf
	} rx_mode;
	int tx_enabled;
};
void stx5197_configure_lirc(struct stx5197_lirc_config *config);


struct stx5197_pwm_config {
	int out0_enabled;
};
void stx5197_configure_pwm(struct stx5197_pwm_config *config);


struct stx5197_ethernet_config {
	enum {
		stx5197_ethernet_mode_mii,
		stx5197_ethernet_mode_rmii,
	} mode;
	int ext_clk;
	int phy_bus;
};
void stx5197_configure_ethernet(struct stx5197_ethernet_config *config);


void stx5197_configure_usb(void);


#endif

