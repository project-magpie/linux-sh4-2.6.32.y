/*
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#ifndef __LINUX_STM_EMI_H
#define __LINUX_STM_EMI_H

#define EMI_BANKS 5

struct emi_timing_data {
	int rd_cycle_time;
	int rd_oee_start;
	int rd_oee_end;
	int rd_latchpoint;
	int busreleasetime;

	int wr_cycle_time;
	int wr_oee_start;
	int wr_oee_end;

	int wait_active_low;
};

unsigned long emi_bank_base(int bank);

/*
 * EMI Config Data[0-3] bit fields definitions, as used by emi_bank_configure()
 */
#define EMI_CFG0_WE_USE_OE_CFG		(1 << 26)
#define EMI_CGF0_WAIT_POLARITY_LOW	(1 << 25)
#define EMI_CFG0_LATCH_POINT(v)		(((v) & 0x1f) << 20)
#define EMI_CFG0_DRIVE_DELAY(v)		(((v) & 0x1f) << 15)
#define EMI_CFG0_BUS_RELEASE(v)		(((v) & 0x0f) << 11)
#define ACTIVE_CODE_OFF			0x0
#define ACTIVE_CODE_RD			0x1
#define ACTIVE_CODE_WR			0x2
#define ACTIVE_CODE_RDWR		0x3
#define EMI_CFG0_CS_ACTIVE(v)		(((v) & 0x3) << 9)
#define EMI_CFG0_OE_ACTIVE(v)		(((v) & 0x3) << 7)
#define EMI_CFG0_BE_ACTIVE(v)		(((v) & 0x3) << 5)
#define EMI_CFG0_PORTSIZE_32BIT		(0x1 << 3)
#define EMI_CFG0_PORTSIZE_16BIT		(0x2 << 3)
#define EMI_CFG0_PORTSIZE_8BIT		(0x3 << 3)
#define EMI_CFG0_DEVICE_NORMAL		0x1
#define EMI_CFG0_DEVICE_BURST		0x4

#define EMI_CFG1_READ_CYCLESNOTPHASE	(1 << 31)
#define EMI_CFG1_READ_CYCLES(v)		(((v) & 0x7f) << 24)
#define EMI_CFG1_READ_CSE1(v)		(((v) & 0x0f) << 20)
#define EMI_CFG1_READ_CSE2(v)		(((v) & 0x0f) << 16)
#define EMI_CFG1_READ_OEE1(v)		(((v) & 0x0f) << 12)
#define EMI_CFG1_READ_OEE2(v)		(((v) & 0x0f) << 8)
#define EMI_CFG1_READ_BEE1(v)		(((v) & 0x0f) << 4)
#define EMI_CFG1_READ_BEE2(v)		((v) & 0x0f)

#define EMI_CFG2_WRITE_CYCLESNOTPHASE	(1 << 31)
#define EMI_CFG2_WRITE_CYCLES(v)	(((v) & 0x7f) << 24)
#define EMI_CFG2_WRITW_CSE1(v)		(((v) & 0x0f) << 20)
#define EMI_CFG2_WRITE_CSE2(v)		(((v) & 0x0f) << 16)
#define EMI_CFG2_WRITE_OEE1(v)		(((v) & 0x0f) << 12)
#define EMI_CFG2_WRITE_OEE2(v)		(((v) & 0x0f) << 8)
#define EMI_CFG2_WRITE_BEE1(v)		(((v) & 0x0f) << 4)
#define EMI_CFG2_WRITE_BEE2(v)		((v) & 0x0f)

void emi_bank_configure(int bank, unsigned long data[4]);
void emi_bank_write_cs_enable(int bank, int enable);
void emi_config_pcmode(int bank, int pc_mode);

void emi_config_pata(int bank, int pc_mode);
void emi_config_nand(int bank, struct emi_timing_data *timing_data);
struct stm_plat_pci_config;
void emi_config_pci(struct stm_plat_pci_config *pci_config);

enum nandi_controllers {STM_NANDI_UNCONFIGURED,
			STM_NANDI_HAMMING,
			STM_NANDI_BCH};
void emiss_nandi_select(enum nandi_controllers controller);

#endif
