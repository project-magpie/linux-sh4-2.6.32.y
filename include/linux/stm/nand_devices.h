/*
 * include/linux/stm/nand_devices.h
 *
 * Timing specifications for NAND devices found on ST Reference Boards.
 *
 * Author: Angus Clark <angus.clark@st.com>
 *
 * Copyright (C) 2012-2013 STMicroelectronics Limited
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 */
#ifndef NAND_DEVICES_H
#define NAND_DEVICES_H

/* Hynix HY27UH08AG
 * - tCEA not directly specified in datahseet: using tCEA = tREA + tCR.
 * - tCSD not specified, assuming other constraints (i.e. tCOH, tCHZ) are
 *   limiting factor.
 */
#define NAND_TSPEC_HYNIX_HY27UH08AG5B		\
	((struct nand_timing_spec) {		\
		.tR	= 25,			\
		.tCLS	= 12,			\
		.tCS	= 20,			\
		.tALS	= 12,			\
		.tDS	= 12,			\
		.tWP	= 12,			\
		.tCLH	= 5,			\
		.tCH	= 5,			\
		.tALH	= 5,			\
		.tDH	= 5,			\
		.tWB	= 100,			\
		.tWH	= 10,			\
		.tWC	= 25,			\
		.tRP	= 12,			\
		.tREH	= 10,			\
		.tRC	= 25,			\
		.tREA	= 20,			\
		.tRHOH	= 15,			\
		.tCEA	= 30,			\
		.tCOH	= 15,			\
		.tCHZ	= 50,			\
		.tCSD	= 0,			\
	})

/* Macronix MX30LF1G08AM
 * - tRHOH and tCOH not specified in datahseet, but equivalent to tOH.
 * - tCSD not specified, assuming other constraints (i.e. tCOH, tCHZ) are
 *   limiting factor.
 */
#define NAND_TSPEC_MACRONIX_MX30LF1G08AM	\
	((struct nand_timing_spec) {		\
		.tR	= 25,			\
		.tCLS	= 15,			\
		.tCS	= 20,			\
		.tALS	= 15,			\
		.tDS	= 5,			\
		.tWP	= 15,			\
		.tCLH	= 5,			\
		.tCH	= 5,			\
		.tALH	= 5,			\
		.tDH	= 5,			\
		.tWB	= 100,			\
		.tWH	= 10,			\
		.tWC	= 30,			\
		.tRP	= 15,			\
		.tREH	= 10,			\
		.tRC	= 30,			\
		.tREA	= 20,			\
		.tRHOH	= 10,			\
		.tCEA	= 25,			\
		.tCOH	= 10,			\
		.tCHZ	= 50,			\
		.tCSD	= 0,			\
	})

/* Samsung K9F2G08U0C */
#define NAND_TSPEC_SAMSUNG_K9F2G08U0C		\
	((struct nand_timing_spec) {		\
		.tR	= 40,			\
		.tCLS	= 12,			\
		.tCS	= 20,			\
		.tALS	= 12,			\
		.tDS	= 12,			\
		.tWP	= 12,			\
		.tCLH	= 5,			\
		.tCH	= 5,			\
		.tALH	= 5,			\
		.tDH	= 5,			\
		.tWB	= 100,			\
		.tWH	= 10,			\
		.tWC	= 25,			\
		.tRP	= 12,			\
		.tREH	= 15,			\
		.tRC	= 25,			\
		.tREA	= 20,			\
		.tRHOH	= 15,			\
		.tCEA	= 25,			\
		.tCOH	= 15,			\
		.tCHZ	= 30,			\
		.tCSD	= 10,			\
	})

/* Spansion S34ML0xG1
 * - Spec in datasheet is more performant than advertised ONFI timing mode.
 */
#define NAND_TSPEC_SPANSION_S34ML01G1		\
	((struct nand_timing_spec) {		\
		.tR	= 25,			\
		.tCLS	= 12,			\
		.tCS	= 20,			\
		.tALS	= 10,			\
		.tDS	= 10,			\
		.tWP	= 12,			\
		.tCLH	= 5,			\
		.tCH	= 5,			\
		.tALH	= 5,			\
		.tDH	= 5,			\
		.tWB	= 100,			\
		.tWH	= 10,			\
		.tWC	= 25,			\
		.tRP	= 12,			\
		.tREH	= 10,			\
		.tRC	= 25,			\
		.tREA	= 20,			\
		.tRHOH	= 15,			\
		.tCEA	= 30,			\
		.tCOH	= 15,			\
		.tCHZ	= 30,			\
		.tCSD	= 10,			\
	})

#define NAND_TSPEC_SPANSION_S34ML02G1	NAND_TSPEC_SPANSION_S34ML01G1
#define NAND_TSPEC_SPANSION_S34ML04G1	NAND_TSPEC_SPANSION_S34ML01G1

/* Spansion S34ML0xG2
 * - Spec in datasheet is more performant than advertised ONFI timing mode.
 */
#define NAND_TSPEC_SPANSION_S34ML01G2		\
	((struct nand_timing_spec) {		\
		.tR	= 25,			\
		.tCLS	= 12,			\
		.tCS	= 20,			\
		.tALS	= 12,			\
		.tDS	= 10,			\
		.tWP	= 12,			\
		.tCLH	= 5,			\
		.tCH	= 5,			\
		.tALH	= 5,			\
		.tDH	= 5,			\
		.tWB	= 100,			\
		.tWH	= 10,			\
		.tWC	= 25,			\
		.tRP	= 12,			\
		.tREH	= 10,			\
		.tRC	= 25,			\
		.tREA	= 20,			\
		.tRHOH	= 15,			\
		.tCEA	= 30,			\
		.tCOH	= 15,			\
		.tCHZ	= 30,			\
		.tCSD	= 10,			\
	})

/* ST NAND08GW3B2CN6
 * - Spec in datasheet is more performant than advertised ONFI timing mode.
 * - Now a Micron part
 */
#define NAND_TSPEC_ST_NAND08GW3B2CN6		\
	((struct nand_timing_spec) {		\
		.tR	= 25,			\
		.tCLS	= 12,			\
		.tCS	= 20,			\
		.tALS	= 12,			\
		.tDS	= 12,			\
		.tWP	= 12,			\
		.tCLH	= 5,			\
		.tCH	= 5,			\
		.tALH	= 5,			\
		.tDH	= 5,			\
		.tWB	= 100,			\
		.tWH	= 10,			\
		.tWC	= 25,			\
		.tRP	= 12,			\
		.tREH	= 10,			\
		.tRC	= 25,			\
		.tREA	= 20,			\
		.tRHOH	= 15,			\
		.tCEA	= 25,			\
		.tCOH	= 15,			\
		.tCHZ	= 30,			\
		.tCSD	= 0,			\
	})

/* ST NAND256W3A2BN6 */
#define NAND_TSPEC_ST_NAND256W3A2BN6		\
	((struct nand_timing_spec) {		\
		.tR	= 12,			\
		.tCLS	= 0,			\
		.tCS	= 0,			\
		.tALS	= 0,			\
		.tDS	= 20,			\
		.tWP	= 25,			\
		.tCLH	= 10,			\
		.tCH	= 10,			\
		.tALH	= 10,			\
		.tDH	= 10,			\
		.tWB	= 100,			\
		.tWH	= 15,			\
		.tWC	= 50,			\
		.tRP	= 30,			\
		.tREH	= 15,			\
		.tRC	= 50,			\
		.tREA	= 35,			\
		.tRHOH	= 15,			\
		.tCEA	= 45,			\
		.tCOH	= 15,			\
		.tCHZ	= 20,			\
		.tCSD	= 0,			\
	})

#endif /* NAND_DEVICES_H */
