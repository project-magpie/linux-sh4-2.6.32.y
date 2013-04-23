/*
 * Copyright (C) 2008 STMicroelectronics Limited
 * Author: Angus Clark <angus.clark@st.com
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#ifndef __LINUX_STM_NAND_H
#define __LINUX_STM_NAND_H

/*
 * Legacy specification for NAND timing parameters.  Deprecated in favour of
 * include/mtd/nand.h:nand_timing_spec.
 */
struct stm_nand_timing_data {
	/* Times specified in ns.  (Will be rounded up to nearest multiple of
	   EMI clock period.) */
	int sig_setup;
	int sig_hold;
	int CE_deassert;
	int WE_to_RBn;

	int wr_on;
	int wr_off;

	int rd_on;
	int rd_off;

	int chip_delay;		/* delay in us */
};

/*
 * Board-level specification relating to a 'bank' of NAND Flash
 */
struct stm_nand_bank_data {
	int			csn;
	int			nr_partitions;
	struct mtd_partition	*partitions;
	unsigned int		options;
	unsigned int		bbt_options;
	unsigned int emi_withinbankoffset;

	/*
	 * The AC specification of the NAND device can be used to optimise how
	 * the STM NAND drivers interact with the NAND device.  During
	 * initialisation, NAND accesses are configured according to one of the
	 * following methods, in order of precedence:
	 *
	 *   1. Using the data in 'struct nand_timing_spec', if supplied.
	 *
	 *   2. Using the data in 'struct stm_nand_timing_data', if supplied.
	 *      Not supported by the stm-nand-bch driver, and deprecated in
	 *      favour of method 1.
	 *
	 *   3. Using the ONFI timing mode, as advertised by the device during
	 *      ONFI-probing (ONFI-compliant NAND only).
	 *
	 */
	struct stm_nand_timing_data *timing_data; /* [DEPRECATED] */

	struct nand_timing_spec *timing_spec;

	/*
	 * No. of IP clk cycles by which to 'relax' the timing configuration.
	 * Required on some boards to to accommodate board-level limitations.
	 * Used in conjunction with 'nand_timing_spec' and ONFI configuration.
	 */
	int			timing_relax;
};

#endif /* __LINUX_STM_NAND_H */
