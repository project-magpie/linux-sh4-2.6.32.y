/*
 * Copyright (C) 2011  STMicroelectronics
 * Author: Francesco M. Virlinzi  <francesco.virlinzi@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 */
#ifndef __stm_synopsys_dwc_ddr32_h__
#define __stm_synopsys_dwc_ddr32_h__

#include <linux/stm/poke_table.h>
/*
 * Synopsys DWC SDram Protocol Controller
 * For registers description see:
 * 'DesignWare Cores DDR3/2 SDRAM Protocol - Controller -
 *  Databook - Version 2.10a - February 4, 2009'
 */

#define DDR_SCTL			0x4
# define DDR_SCTL_CFG				0x1
# define DDR_SCTL_GO				0x2
# define DDR_SCTL_SLEEP				0x3
# define DDR_SCTL_WAKEUP			0x4

#define DDR_STAT			0x8
# define DDR_STAT_MASK				0x7
# define DDR_STAT_CONFIG			0x1
# define DDR_STAT_ACCESS			0x3
# define DDR_STAT_LOW_POWER			0x5

#define DDR_DTU_CFG			0x208
# define DDR_DTU_CFG_ENABLE			0x1

#define DDR_PHY_IOCRV1			0x31C

/*
 * Synopsys DWC SDram Phy Controller
 * For registers description see:
 * 'DesignWare Cores DDR3/2 SDRAM PHY -
 *  Databook - February 5, 2009'
 *
 * - Table 5.1: PHY Control Register Mapping
 * and
 * - Table 5.30: PUB Control Register Mapping
 */
#define DDR_PHY_REG(idx)		(0x400 + (idx) * 4)

#define DDR_PHY_PIR			DDR_PHY_REG(1)		/* 0x04 */
# define DDR_PHY_PIR_PLL_RESET			(1 << 7)
# define DDR_PHY_PIR_PLL_PD			(1 << 8)

#define DDR_PHY_PGCR0			DDR_PHY_REG(2)		/* 0x08 */
#define DDR_PHY_PGCR1			DDR_PHY_REG(3)		/* 0x0c */

#define DDR_PHY_PGSR0			DDR_PHY_REG(4)		/* 0x10 */
# define DDR_PHY_PGSR0_INIT			(1 << 0)
# define DDR_PHY_PGSR0_PLLINIT			(1 << 1)
# define DDR_PHY_PGSR0_CAL			(1 << 2)
# define DDR_PHY_PGSR0_APLOCK			(1 << 5)

#define DDR_PHY_ACIOCR			DDR_PHY_REG(12)		/* 0x30 */
# define DDR_PHY_ACIOCR_OUTPUT_ENABLE		(1 << 1)
# define DDR_PHY_ACIOCR_PDD			(1 << 3)
# define DDR_PHY_ACIOCR_PDR			(1 << 4)

#define DDR_PHY_DXCCR			DDR_PHY_REG(13)		/* 0x34 */
# define DDR_PHY_DXCCR_DXODT			(1 << 0)
# define DDR_PHY_DXCCR_PDR			(1 << 4)

#define DDR_PHY_PLLREADY		(DDR_PHY_PGSR0_INIT | 	\
					DDR_PHY_PGSR0_PLLINIT | \
					DDR_PHY_PGSR0_CAL | 	\
					DDR_PHY_PGSR0_APLOCK)


/*
 * Synopsys DWC SDram Phy Controller
 * For registers description see:
 * 'DesignWare Cores DDR3/2 SDRAM PHY - v3.11
 *  Databook - April 28, 2012'
 *
 * - Table 3.2: PHY Register Address Mapping
 */
#define DDR_uPHY_PGCR_0			DDR_PHY_REG(2)
#define DDR_uPHY_PGCR_1			DDR_PHY_REG(3)
#define DDR_uPHY_PGCR_2			DDR_PHY_REG(35)
#define DDR_uPHY_PGSR_0			DDR_PHY_REG(4)
# define DDR_uPHY_PGSR0_IDONE			(1 << 0)
# define DDR_uPHY_PGSR0_PLDONE			(1 << 1)
# define DDR_uPHY_PGSR0_DCDONE			(1 << 2)
# define DDR_uPHY_PGSR0_ZCDONE			(1 << 3)
# define DDR_uPHY_PGSR0_APLOCK			(1 << 31)
# define DDR_uPHY_READY			(DDR_uPHY_PGSR0_IDONE	|	\
					DDR_uPHY_PGSR0_PLDONE	|	\
					DDR_uPHY_PGSR0_DCDONE	|	\
					DDR_uPHY_PGSR0_ZCDONE	|	\
					DDR_uPHY_PGSR0_APLOCK)


#define DDR_uPHY_PGSR_1			DDR_PHY_REG(5)

#define DDR_uPHY_PLLCR			DDR_PHY_REG(6)
# define DDR_uPHY_PLLCR_PLLPD			(1 << 29)
# define DDR_uPHY_PLLCR_PLLRST			(1 << 30)
# define DDR_uPHY_PLLCR_BYP			(1 << 31)

#define DDR_uPHY_DXCCR			DDR_PHY_REG(15)
# define DDR_uPHY_DXCCR_DXODT			(1 << 0)


/*
 * Synopsys DDR32: in Self-Refresh
 */
#define synopsys_ddr32_in_self_refresh(_ddr_base)			\
  /* Enable the DDR self refresh mode */				\
  /* from ACCESS to LowPower (based on paraghaph. 7.1.4) */		\
  POKE32((_ddr_base) + DDR_SCTL, DDR_SCTL_SLEEP),			\
  WHILE_NE32((_ddr_base) + DDR_STAT, DDR_STAT_MASK, DDR_STAT_LOW_POWER)

/*
 * Synopsys DDR: out of Self-Refresh
 */
#define synopsys_ddr32_out_of_self_refresh(_ddr_base)			\
  /* Disables the DDR self refresh mode	*/				\
  /* from LowPower to Access (based on paraghaph 7.1.3)	*/		\
  POKE32((_ddr_base) + DDR_SCTL, DDR_SCTL_WAKEUP),			\
  WHILE_NE32((_ddr_base) + DDR_STAT, DDR_STAT_MASK, DDR_STAT_ACCESS),	\
									\
  POKE32((_ddr_base) + DDR_SCTL, DDR_SCTL_CFG),				\
  WHILE_NE32((_ddr_base) + DDR_STAT, DDR_STAT_MASK, DDR_STAT_CONFIG),	\
									\
  POKE32((_ddr_base) + DDR_SCTL, DDR_SCTL_GO),				\
  WHILE_NE32((_ddr_base) + DDR_STAT, DDR_STAT_MASK, DDR_STAT_ACCESS)

/*
 * Synopsys DDR Phy: enter/exit Standby
 */
#define synopsys_ddr32_phy_standby_enter(_ddr_base)			\
  OR32((_ddr_base) + DDR_PHY_DXCCR, DDR_PHY_DXCCR_DXODT),		\
  /* DDR_Phy Pll in reset */						\
  OR32((_ddr_base) + DDR_PHY_PIR, DDR_PHY_PIR_PLL_RESET)

#define synopsys_ddr32_phy_standby_exit(_ddr_base)			\
  /* DDR_Phy Pll out of reset */					\
  UPDATE32((_ddr_base) + DDR_PHY_PIR, ~DDR_PHY_PIR_PLL_RESET, 0),	\
  /* Waiting for PLLs to be locked */					\
  WHILE_NE32((_ddr_base) + DDR_PHY_PGSR0, DDR_PHY_PLLREADY,		\
						DDR_PHY_PLLREADY),	\
  UPDATE32((_ddr_base) + DDR_PHY_DXCCR, ~DDR_PHY_DXCCR_DXODT, 0)

/*
 * Synopsys DDR uPhy: enter/exit Standby
 */
#define synopsys_ddr32_uphy_standby_enter(_ddr_base)			\
  OR32((_ddr_base) + DDR_uPHY_DXCCR, DDR_uPHY_DXCCR_DXODT),		\
  OR32((_ddr_base) + DDR_uPHY_PLLCR, DDR_uPHY_PLLCR_PLLRST)

#define synopsys_ddr32_uphy_standby_exit(_ddr_base)			\
  UPDATE32((_ddr_base) + DDR_uPHY_PLLCR, ~DDR_uPHY_PLLCR_PLLRST, 0),	\
  WHILE_NE32((_ddr_base) + DDR_uPHY_PGSR_0, DDR_uPHY_READY,		\
	DDR_uPHY_READY),						\
  UPDATE32((_ddr_base) + DDR_uPHY_DXCCR, ~DDR_uPHY_DXCCR_DXODT, 0)

/*
 * Synopsys DDR Phy: moving in HoM
 */
#define synopsys_ddr32_phy_hom_enter(_ddr_base)				\
  /* 2. Turn in LowPower the DDR-Phy */                           	\
  OR32((_ddr_base) + DDR_PHY_PIR, DDR_PHY_PIR_PLL_RESET),		\
  OR32((_ddr_base) + DDR_PHY_PIR, DDR_PHY_PIR_PLL_PD),            	\
									\
  POKE32((_ddr_base) + DDR_PHY_ACIOCR, -1),                       	\
  UPDATE32((_ddr_base) + DDR_PHY_ACIOCR, ~1, 0),                  	\
									\
  OR32((_ddr_base) + DDR_PHY_DXCCR, DDR_PHY_DXCCR_PDR),           	\
									\
  /* Disable CK going to the SDRAM */                             	\
  UPDATE32((_ddr_base) + DDR_PHY_PGCR0, ~(0x3f << 26), 0),        	\
  UPDATE32((_ddr_base) + DDR_PHY_PGCR1, ~(5 << 12), 0)

#define synopsys_ddr32_in_hom(_ddr_base)				\
  /* Enable DTU */							\
  OR32((_ddr_base) + DDR_DTU_CFG, DDR_DTU_CFG_ENABLE),			\
  synopsys_ddr32_in_self_refresh(_ddr_base),				\
  synopsys_ddr32_phy_hom_enter(_ddr_base)

#endif
