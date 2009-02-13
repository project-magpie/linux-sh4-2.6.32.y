/*
 * -------------------------------------------------------------------------
 * <linux_root>/arch/sh/kernel/cpu/sh4/soc-stx5197.h
 * -------------------------------------------------------------------------
 * Copyright (C) 2009  STMicroelectronics
 *
 * May be copied or modified under the terms of the GNU General Public
 * License V.2 ONLY.  See linux/COPYING for more information.
 *
 * -------------------------------------------------------------------------
 */

#ifndef __soc_stx5197_h__
#define __soc_stx5197_h__

/* Values for mb704 */
#define XTAL	30000000

#define SYS_SERV_BASE_ADDR	0xfdc00000

#define CLK_PLL_CONFIG0(x)	((x*8)+0x0)
#define CLK_PLL_CONFIG1(x)	((x*8)+0x4)
  #define CLK_PLL_CONFIG1_POFF	(1<<13)

#define CLKDIV0_CONFIG0		0x90
#define CLKDIV1_4_CONFIG0(n)	(0x0a0 + ((n-1)*0xc))
#define CLKDIV6_10_CONFIG0(n)	(0x0d0 + ((n-6)*0xc))

#define CLKDIV_CONF0(x)		(((x) == 0) ? CLKDIV0_CONFIG0 : ((x) < 5) ? \
			CLKDIV1_4_CONFIG0(x) : CLKDIV6_10_CONFIG0(x))

#define CLKDIV_CONF1(x)		(CLKDIV_CONF0(x) + 0x4)
#define CLKDIV_CONF2(x)		(CLKDIV_CONF0(x) + 0x8)


#define CLK_MODE_CTRL		0x110
 #define CLK_MODE_CTRL_NULL	0x0
 #define CLK_MODE_CTRL_X1	0x1
 #define CLK_MODE_CTRL_PROG	0x2
 #define CLK_MODE_CTRL_STDB	0x3

/*
 * The REDUCED_PM is used in CLK_MODE_CTRL_PROG...
 */
#define CLK_REDUCED_PM_CTRL	0x114
 #define CLK_REDUCED_ON_XTAL_MEMSTDBY	(1<<11)
 #define CLK_REDUCED_ON_XTAL_STDBY	(~(0x22))

#define CLK_LP_MODE_DIS0	0x118
  #define CLK_LP_MODE_DIS0_VALUE	((0x3 << 11) | (0x7ff & ~(1<<9)))

#define CLK_LP_MODE_DIS2	0x11C

#define CLK_DYNAMIC_PWR		0x128

#define CLK_PLL_SELECT_CFG	0x180
#define CLK_DIV_FORCE_CFG	0x184
#define CLK_OBSERVE		0x188

#define CLK_LOCK_CFG		0x300

/*
 * Utility macros
 */
#define CLK_UNLOCK()	{	writel(0xf0, SYS_SERV_BASE_ADDR + CLK_LOCK_CFG); \
				writel(0x0f, SYS_SERV_BASE_ADDR + CLK_LOCK_CFG); }

#define CLK_LOCK()		writel(0x100, SYS_SERV_BASE_ADDR + CLK_LOCK_CFG);


#define CFG_CONTROL_C   (0x00 / 4)
#define CFG_CONTROL_D   (0x04 / 4)
#define CFG_CONTROL_E   (0x08 / 4)
#define CFG_CONTROL_F   (0x0c / 4)
#define CFG_CONTROL_G   (0x10 / 4)
#define CFG_CONTROL_H   (0x14 / 4)
#define CFG_CONTROL_I   (0x18 / 4)
#define CFG_CONTROL_J   (0x1c / 4)

#define CFG_CONTROL_K   (0x40 / 4)
#define CFG_CONTROL_L   (0x44 / 4)
#define CFG_CONTROL_M   (0x48 / 4)
#define CFG_CONTROL_N   (0x4c / 4)
#define CFG_CONTROL_O   (0x50 / 4)
#define CFG_CONTROL_P   (0x54 / 4)
#define CFG_CONTROL_Q   (0x58 / 4)
#define CFG_CONTROL_R   (0x5c / 4)

#define CFG_MONITOR_C   (0x20 / 4)
#define CFG_MONITOR_D   (0x24 / 4)
#define CFG_MONITOR_E   (0x28 / 4)
#define CFG_MONITOR_F   (0x2c / 4)
#define CFG_MONITOR_G   (0x30 / 4)
#define CFG_MONITOR_H   (0x34 / 4)
#define CFG_MONITOR_I   (0x38 / 4)
#define CFG_MONITOR_J   (0x3c / 4)

#define CFG_MONITOR_K   (0x60 / 4)
#define CFG_MONITOR_L   (0x64 / 4)
#define CFG_MONITOR_M   (0x68 / 4)
#define CFG_MONITOR_N   (0x6c / 4)
#define CFG_MONITOR_O   (0x70 / 4)
#define CFG_MONITOR_P   (0x74 / 4)
#define CFG_MONITOR_Q   (0x78 / 4)
#define CFG_MONITOR_R   (0x7c / 4)

#endif

