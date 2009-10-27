#ifndef __LINUX_STM_STX7141_H
#define __LINUX_STM_STX7141_H

#include <linux/device.h>
#include <linux/spi/spi.h>
#include <linux/stm/platform.h>


void stx7141_early_device_init(void);


struct stx7141_asc_config {
	union {
		enum { stx7141_asc0_mcard } asc0;
		enum { stx7141_asc1_pio10, stx7141_asc1_mcard } asc1;
		enum { stx7141_asc2_pio1, stx7141_asc2_pio6 } asc2;
	} routing;
	int hw_flow_control;
	int is_console;
};
void stx7141_configure_asc(int asc, struct stx7141_asc_config *config);


/* WARNING! SSCs were numbered starting from 1 in early documents.
 * Later it was changed so the first SSC is SSC0 (zero),
 * and this approach is used in the functions below. */
struct stx7141_ssc_spi_config {
	void (*chipselect)(struct spi_device *spi, int is_on);
};
/* SSC configure functions return I2C/SPI bus number */
int stx7141_configure_ssc_i2c(int ssc);
int stx7141_configure_ssc_spi(int ssc, struct stx7141_ssc_spi_config *config);


struct stx7141_lirc_config {
	enum {
		stx7141_lirc_rx_disabled,
		stx7141_lirc_rx_mode_ir,
		stx7141_lirc_rx_mode_uhf
	} rx_mode;
	int tx_enabled;
	int tx_od_enabled;
};
void stx7141_configure_lirc(struct stx7141_lirc_config *config);


struct stx7141_pwm_config {
	int out0_enabled;
	int out1_enabled;
};
void stx7141_configure_pwm(struct stx7141_pwm_config *config);


struct stx7141_ethernet_config {
	enum {
		stx7141_ethernet_mode_mii,
		stx7141_ethernet_mode_gmii,
		stx7141_ethernet_mode_rgmii,
		stx7141_ethernet_mode_sgmii,
		stx7141_ethernet_mode_rmii,
		stx7141_ethernet_mode_reverse_mii,
	} mode;
	int phy_bus;
};
void stx7141_configure_ethernet(int port,
		struct stx7141_ethernet_config *config);


void stx7141_configure_usb(int port);


void stx7141_configure_sata(void);


struct stx7141_pata_config {
	int emi_bank;
	int pc_mode;
	unsigned int irq;
};
void stx7141_configure_pata(struct stx7141_pata_config *config);

void stx7141_configure_nand_flex(int nr_banks,
                                 struct stm_nand_bank_data *banks,
                                 int rbn_connected);


struct stx7141_audio_config {
	enum {
		stx7141_pcm_player_0_output_disabled,
		stx7141_pcm_player_0_output_2_channels,
		stx7141_pcm_player_0_output_4_channels,
		stx7141_pcm_player_0_output_6_channels,
		stx7141_pcm_player_0_output_8_channels,
		stx7141_pcm_player_0_output_10_channels,
	} pcm_player_0_output;
	int pcm_player_1_output_enabled;
	int spdif_player_output_enabled;
	int pcm_reader_0_input_enabled;
	int pcm_reader_1_input_enabled;
};
void stx7141_configure_audio(struct stx7141_audio_config *config);


#endif
