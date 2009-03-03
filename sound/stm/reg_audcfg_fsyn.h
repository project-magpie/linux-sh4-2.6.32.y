#ifndef __SND_STM_AUDCFG_FSYN_H
#define __SND_STM_AUDCFG_FSYN_H

/*
 * IP versions
 */

/* 7100 2.0, 7100 3.0 */
#define ver__AUDCFG_FSYN__90_1_0_3 1

/* 7109 2.0, 7109 3.0 */
#define ver__AUDCFG_FSYN__90_2_3 2

/* 7200 1.0 */
#define ver__AUDCFG_FSYN__65_2_1_2 3

/* 7111 */
#define ver__AUDCFG_FSYN__65_3_1 4

/* 7200 2.0 */
#define ver__AUDCFG_FSYN__65_3_3 5



/*
 * AUDCFG_FSYN_CFG
 */

#define offset__AUDCFG_FSYN_CFG(ip) 0x00
#define get__AUDCFG_FSYN_CFG(ip) readl(ip->base + \
	offset__AUDCFG_FSYN_CFG(ip))
#define set__AUDCFG_FSYN_CFG(ip, value) writel(value, ip->base + \
	offset__AUDCFG_FSYN_CFG(ip))

/* RSTP */

#define shift__AUDCFG_FSYN_CFG__RSTP(ip) 0
#define mask__AUDCFG_FSYN_CFG__RSTP(ip) 0x1
#define get__AUDCFG_FSYN_CFG__RSTP(ip) ((readl(ip->base + \
	offset__AUDCFG_FSYN_CFG(ip)) >> shift__AUDCFG_FSYN_CFG__RSTP(ip)) & \
	mask__AUDCFG_FSYN_CFG__RSTP(ip))
#define set__AUDCFG_FSYN_CFG__RSTP(ip, value) writel((readl(ip->base + \
	offset__AUDCFG_FSYN_CFG(ip)) & ~(mask__AUDCFG_FSYN_CFG__RSTP(ip) << \
	shift__AUDCFG_FSYN_CFG__RSTP(ip))) | (((value) & \
	mask__AUDCFG_FSYN_CFG__RSTP(ip)) << shift__AUDCFG_FSYN_CFG__RSTP(ip)), \
	ip->base + offset__AUDCFG_FSYN_CFG(ip))

#define value__AUDCFG_FSYN_CFG__RSTP__RUNNING(ip) 0x0
#define mask__AUDCFG_FSYN_CFG__RSTP__RUNNING(ip) \
	(value__AUDCFG_FSYN_CFG__RSTP__RUNNING(ip) << \
	shift__AUDCFG_FSYN_CFG__RSTP(ip))
#define set__AUDCFG_FSYN_CFG__RSTP__RUNNING(ip) \
	set__AUDCFG_FSYN_CFG__RSTP(ip, \
	value__AUDCFG_FSYN_CFG__RSTP__RUNNING(ip))

#define value__AUDCFG_FSYN_CFG__RSTP__RESET(ip) 0x1
#define mask__AUDCFG_FSYN_CFG__RSTP__RESET(ip) \
	(value__AUDCFG_FSYN_CFG__RSTP__RESET(ip) << \
	shift__AUDCFG_FSYN_CFG__RSTP(ip))
#define set__AUDCFG_FSYN_CFG__RSTP__RESET(ip) \
	set__AUDCFG_FSYN_CFG__RSTP(ip, \
	value__AUDCFG_FSYN_CFG__RSTP__RESET(ip))

/* PCM_CLK_SEL */

#define shift__AUDCFG_FSYN_CFG__PCM_CLK_SEL(ip) 2
#define mask__AUDCFG_FSYN_CFG__PCM_CLK_SEL(ip) 0xf
#define get__AUDCFG_FSYN_CFG__PCM_CLK_SEL(ip) ((readl(ip->base + \
	offset__AUDCFG_FSYN_CFG(ip)) >> \
	shift__AUDCFG_FSYN_CFG__PCM_CLK_SEL(ip)) & \
	mask__AUDCFG_FSYN_CFG__PCM_CLK_SEL(ip))
#define set__AUDCFG_FSYN_CFG__PCM_CLK_SEL(ip, value) \
	writel((readl(ip->base + offset__AUDCFG_FSYN_CFG(ip)) & \
	~(mask__AUDCFG_FSYN_CFG__PCM_CLK_SEL(ip) << \
	shift__AUDCFG_FSYN_CFG__PCM_CLK_SEL(ip))) | (((value) & \
	mask__AUDCFG_FSYN_CFG__PCM_CLK_SEL(ip)) << \
	shift__AUDCFG_FSYN_CFG__PCM_CLK_SEL(ip)), ip->base + \
	offset__AUDCFG_FSYN_CFG(ip))

#define value__AUDCFG_FSYN_CFG__PCM_CLK_SEL__EXTCLK(ip, n) (ip->ver < \
	ver__AUDCFG_FSYN__65_2_1_2 ? (0 << n) : (ip->ver < \
	ver__AUDCFG_FSYN__65_3_1 ? (1 << n) : (ip->ver < \
	ver__AUDCFG_FSYN__65_3_3 ? (0 << n) : (1 << n))))
#define mask__AUDCFG_FSYN_CFG__PCM_CLK_SEL__EXTCLK(ip, n) \
	(value__AUDCFG_FSYN_CFG__PCM_CLK_SEL__EXTCLK(ip, n) << \
	shift__AUDCFG_FSYN_CFG__PCM_CLK_SEL(ip))
#define set__AUDCFG_FSYN_CFG__PCM_CLK_SEL__EXTCLK(ip, n) \
	set__AUDCFG_FSYN_CFG__PCM_CLK_SEL(ip, \
	value__AUDCFG_FSYN_CFG__PCM_CLK_SEL__EXTCLK(ip, n))

#define value__AUDCFG_FSYN_CFG__PCM_CLK_SEL__FSYNTH(ip, n) (ip->ver < \
	ver__AUDCFG_FSYN__65_2_1_2 ? (1 << n) : (ip->ver < \
	ver__AUDCFG_FSYN__65_3_1 ? (0 << n) : (ip->ver < \
	ver__AUDCFG_FSYN__65_3_3 ? (1 << n) : (0 << n))))
#define mask__AUDCFG_FSYN_CFG__PCM_CLK_SEL__FSYNTH(ip, n) \
	(value__AUDCFG_FSYN_CFG__PCM_CLK_SEL__FSYNTH(ip, n) << \
	shift__AUDCFG_FSYN_CFG__PCM_CLK_SEL(ip))
#define set__AUDCFG_FSYN_CFG__PCM_CLK_SEL__FSYNTH(ip, n) \
	set__AUDCFG_FSYN_CFG__PCM_CLK_SEL(ip, \
	value__AUDCFG_FSYN_CFG__PCM_CLK_SEL__FSYNTH(ip, n))

/* FS_EN */

#define shift__AUDCFG_FSYN_CFG__FS_EN(ip) (ip->ver < \
	ver__AUDCFG_FSYN__65_2_1_2 ? 6 : (ip->ver < ver__AUDCFG_FSYN__65_3_1 ? \
	-1 : (ip->ver < ver__AUDCFG_FSYN__65_3_3 ? 6 : -1)))
#define mask__AUDCFG_FSYN_CFG__FS_EN(ip) (ip->ver < \
	ver__AUDCFG_FSYN__65_2_1_2 ? 0x7 : (ip->ver < ver__AUDCFG_FSYN__65_3_1 \
	? -1 : (ip->ver < ver__AUDCFG_FSYN__65_3_3 ? 0xf : -1)))
#define get__AUDCFG_FSYN_CFG__FS_EN(ip) ((readl(ip->base + \
	offset__AUDCFG_FSYN_CFG(ip)) >> shift__AUDCFG_FSYN_CFG__FS_EN(ip)) & \
	mask__AUDCFG_FSYN_CFG__FS_EN(ip))
#define set__AUDCFG_FSYN_CFG__FS_EN(ip, value) writel((readl(ip->base \
	+ offset__AUDCFG_FSYN_CFG(ip)) & ~(mask__AUDCFG_FSYN_CFG__FS_EN(ip) << \
	shift__AUDCFG_FSYN_CFG__FS_EN(ip))) | (((value) & \
	mask__AUDCFG_FSYN_CFG__FS_EN(ip)) << \
	shift__AUDCFG_FSYN_CFG__FS_EN(ip)), ip->base + \
	offset__AUDCFG_FSYN_CFG(ip))

#define value__AUDCFG_FSYN_CFG__FS_EN__DISABLED(ip, n) (ip->ver < \
	ver__AUDCFG_FSYN__65_2_1_2 ? (0 << n) : (ip->ver < \
	ver__AUDCFG_FSYN__65_3_1 ? -1 : (ip->ver < ver__AUDCFG_FSYN__65_3_3 ? \
	(0 << n) : -1)))
#define mask__AUDCFG_FSYN_CFG__FS_EN__DISABLED(ip, n) \
	(value__AUDCFG_FSYN_CFG__FS_EN__DISABLED(ip, n) << \
	shift__AUDCFG_FSYN_CFG__FS_EN(ip))
#define set__AUDCFG_FSYN_CFG__FS_EN__DISABLED(ip, n) \
	set__AUDCFG_FSYN_CFG__FS_EN(ip, \
	value__AUDCFG_FSYN_CFG__FS_EN__DISABLED(ip, n))

#define value__AUDCFG_FSYN_CFG__FS_EN__ENABLED(ip, n) (ip->ver < \
	ver__AUDCFG_FSYN__65_2_1_2 ? (1 << n) : (ip->ver < \
	ver__AUDCFG_FSYN__65_3_1 ? -1 : (ip->ver < ver__AUDCFG_FSYN__65_3_3 ? \
	(1 << n) : -1)))
#define mask__AUDCFG_FSYN_CFG__FS_EN__ENABLED(ip, n) \
	(value__AUDCFG_FSYN_CFG__FS_EN__ENABLED(ip, n) << \
	shift__AUDCFG_FSYN_CFG__FS_EN(ip))
#define set__AUDCFG_FSYN_CFG__FS_EN__ENABLED(ip, n) \
	set__AUDCFG_FSYN_CFG__FS_EN(ip, \
	value__AUDCFG_FSYN_CFG__FS_EN__ENABLED(ip, n))

/* NSB */

#define shift__AUDCFG_FSYN_CFG__NSB(ip) 10
#define mask__AUDCFG_FSYN_CFG__NSB(ip) 0xf
#define get__AUDCFG_FSYN_CFG__NSB(ip) ((readl(ip->base + \
	offset__AUDCFG_FSYN_CFG(ip)) >> shift__AUDCFG_FSYN_CFG__NSB(ip)) & \
	mask__AUDCFG_FSYN_CFG__NSB(ip))
#define set__AUDCFG_FSYN_CFG__NSB(ip, value) writel((readl(ip->base + \
	offset__AUDCFG_FSYN_CFG(ip)) & ~(mask__AUDCFG_FSYN_CFG__NSB(ip) << \
	shift__AUDCFG_FSYN_CFG__NSB(ip))) | (((value) & \
	mask__AUDCFG_FSYN_CFG__NSB(ip)) << shift__AUDCFG_FSYN_CFG__NSB(ip)), \
	ip->base + offset__AUDCFG_FSYN_CFG(ip))

#define value__AUDCFG_FSYN_CFG__NSB__STANDBY(ip, n) (0 << n)
#define mask__AUDCFG_FSYN_CFG__NSB__STANDBY(ip, n) \
	(value__AUDCFG_FSYN_CFG__NSB__STANDBY(ip, n) << \
	shift__AUDCFG_FSYN_CFG__NSB(ip))
#define set__AUDCFG_FSYN_CFG__NSB__STANDBY(ip, n) \
	set__AUDCFG_FSYN_CFG__NSB(ip, value__AUDCFG_FSYN_CFG__NSB__STANDBY(ip, \
	n))

#define value__AUDCFG_FSYN_CFG__NSB__ACTIVE(ip, n) (1 << n)
#define mask__AUDCFG_FSYN_CFG__NSB__ACTIVE(ip, n) \
	(value__AUDCFG_FSYN_CFG__NSB__ACTIVE(ip, n) << \
	shift__AUDCFG_FSYN_CFG__NSB(ip))
#define set__AUDCFG_FSYN_CFG__NSB__ACTIVE(ip, n) \
	set__AUDCFG_FSYN_CFG__NSB(ip, value__AUDCFG_FSYN_CFG__NSB__ACTIVE(ip, \
	n))

/* NPDA */

#define shift__AUDCFG_FSYN_CFG__NPDA(ip) 14
#define mask__AUDCFG_FSYN_CFG__NPDA(ip) 0x1
#define get__AUDCFG_FSYN_CFG__NPDA(ip) ((readl(ip->base + \
	offset__AUDCFG_FSYN_CFG(ip)) >> shift__AUDCFG_FSYN_CFG__NPDA(ip)) & \
	mask__AUDCFG_FSYN_CFG__NPDA(ip))
#define set__AUDCFG_FSYN_CFG__NPDA(ip, value) writel((readl(ip->base + \
	offset__AUDCFG_FSYN_CFG(ip)) & ~(mask__AUDCFG_FSYN_CFG__NPDA(ip) << \
	shift__AUDCFG_FSYN_CFG__NPDA(ip))) | (((value) & \
	mask__AUDCFG_FSYN_CFG__NPDA(ip)) << shift__AUDCFG_FSYN_CFG__NPDA(ip)), \
	ip->base + offset__AUDCFG_FSYN_CFG(ip))

#define value__AUDCFG_FSYN_CFG__NPDA__POWER_DOWN(ip) 0x0
#define mask__AUDCFG_FSYN_CFG__NPDA__POWER_DOWN(ip) \
	(value__AUDCFG_FSYN_CFG__NPDA__POWER_DOWN(ip) << \
	shift__AUDCFG_FSYN_CFG__NPDA(ip))
#define set__AUDCFG_FSYN_CFG__NPDA__POWER_DOWN(ip) \
	set__AUDCFG_FSYN_CFG__NPDA(ip, \
	value__AUDCFG_FSYN_CFG__NPDA__POWER_DOWN(ip))

#define value__AUDCFG_FSYN_CFG__NPDA__NORMAL(ip) 0x1
#define mask__AUDCFG_FSYN_CFG__NPDA__NORMAL(ip) \
	(value__AUDCFG_FSYN_CFG__NPDA__NORMAL(ip) << \
	shift__AUDCFG_FSYN_CFG__NPDA(ip))
#define set__AUDCFG_FSYN_CFG__NPDA__NORMAL(ip) \
	set__AUDCFG_FSYN_CFG__NPDA(ip, \
	value__AUDCFG_FSYN_CFG__NPDA__NORMAL(ip))

/* NDIV */

#define shift__AUDCFG_FSYN_CFG__NDIV(ip) 15
#define mask__AUDCFG_FSYN_CFG__NDIV(ip) 0x1
#define get__AUDCFG_FSYN_CFG__NDIV(ip) ((readl(ip->base + \
	offset__AUDCFG_FSYN_CFG(ip)) >> shift__AUDCFG_FSYN_CFG__NDIV(ip)) & \
	mask__AUDCFG_FSYN_CFG__NDIV(ip))
#define set__AUDCFG_FSYN_CFG__NDIV(ip, value) writel((readl(ip->base + \
	offset__AUDCFG_FSYN_CFG(ip)) & ~(mask__AUDCFG_FSYN_CFG__NDIV(ip) << \
	shift__AUDCFG_FSYN_CFG__NDIV(ip))) | (((value) & \
	mask__AUDCFG_FSYN_CFG__NDIV(ip)) << shift__AUDCFG_FSYN_CFG__NDIV(ip)), \
	ip->base + offset__AUDCFG_FSYN_CFG(ip))

#define value__AUDCFG_FSYN_CFG__NDIV__27_30_MHZ(ip) 0x0
#define mask__AUDCFG_FSYN_CFG__NDIV__27_30_MHZ(ip) \
	(value__AUDCFG_FSYN_CFG__NDIV__27_30_MHZ(ip) << \
	shift__AUDCFG_FSYN_CFG__NDIV(ip))
#define set__AUDCFG_FSYN_CFG__NDIV__27_30_MHZ(ip) \
	set__AUDCFG_FSYN_CFG__NDIV(ip, \
	value__AUDCFG_FSYN_CFG__NDIV__27_30_MHZ(ip))

#define value__AUDCFG_FSYN_CFG__NDIV__54_60_MHZ(ip) 0x1
#define mask__AUDCFG_FSYN_CFG__NDIV__54_60_MHZ(ip) \
	(value__AUDCFG_FSYN_CFG__NDIV__54_60_MHZ(ip) << \
	shift__AUDCFG_FSYN_CFG__NDIV(ip))
#define set__AUDCFG_FSYN_CFG__NDIV__54_60_MHZ(ip) \
	set__AUDCFG_FSYN_CFG__NDIV(ip, \
	value__AUDCFG_FSYN_CFG__NDIV__54_60_MHZ(ip))

/* BW_SEL */

#define shift__AUDCFG_FSYN_CFG__BW_SEL(ip) 16
#define mask__AUDCFG_FSYN_CFG__BW_SEL(ip) 0x3
#define get__AUDCFG_FSYN_CFG__BW_SEL(ip) ((readl(ip->base + \
	offset__AUDCFG_FSYN_CFG(ip)) >> shift__AUDCFG_FSYN_CFG__BW_SEL(ip)) & \
	mask__AUDCFG_FSYN_CFG__BW_SEL(ip))
#define set__AUDCFG_FSYN_CFG__BW_SEL(ip, value) writel((readl(ip->base \
	+ offset__AUDCFG_FSYN_CFG(ip)) & ~(mask__AUDCFG_FSYN_CFG__BW_SEL(ip) \
	<< shift__AUDCFG_FSYN_CFG__BW_SEL(ip))) | (((value) & \
	mask__AUDCFG_FSYN_CFG__BW_SEL(ip)) << \
	shift__AUDCFG_FSYN_CFG__BW_SEL(ip)), ip->base + \
	offset__AUDCFG_FSYN_CFG(ip))

#define value__AUDCFG_FSYN_CFG__BW_SEL__VERY_GOOD_REFERENCE(ip) 0x0
#define mask__AUDCFG_FSYN_CFG__BW_SEL__VERY_GOOD_REFERENCE(ip) \
	(value__AUDCFG_FSYN_CFG__BW_SEL__VERY_GOOD_REFERENCE(ip) << \
	shift__AUDCFG_FSYN_CFG__BW_SEL(ip))
#define set__AUDCFG_FSYN_CFG__BW_SEL__VERY_GOOD_REFERENCE(ip) \
	set__AUDCFG_FSYN_CFG__BW_SEL(ip, \
	value__AUDCFG_FSYN_CFG__BW_SEL__VERY_GOOD_REFERENCE(ip))

#define value__AUDCFG_FSYN_CFG__BW_SEL__GOOD_REFERENCE(ip) 0x1
#define mask__AUDCFG_FSYN_CFG__BW_SEL__GOOD_REFERENCE(ip) \
	(value__AUDCFG_FSYN_CFG__BW_SEL__GOOD_REFERENCE(ip) << \
	shift__AUDCFG_FSYN_CFG__BW_SEL(ip))
#define set__AUDCFG_FSYN_CFG__BW_SEL__GOOD_REFERENCE(ip) \
	set__AUDCFG_FSYN_CFG__BW_SEL(ip, \
	value__AUDCFG_FSYN_CFG__BW_SEL__GOOD_REFERENCE(ip))

#define value__AUDCFG_FSYN_CFG__BW_SEL__BAD_REFERENCE(ip) 0x2
#define mask__AUDCFG_FSYN_CFG__BW_SEL__BAD_REFERENCE(ip) \
	(value__AUDCFG_FSYN_CFG__BW_SEL__BAD_REFERENCE(ip) << \
	shift__AUDCFG_FSYN_CFG__BW_SEL(ip))
#define set__AUDCFG_FSYN_CFG__BW_SEL__BAD_REFERENCE(ip) \
	set__AUDCFG_FSYN_CFG__BW_SEL(ip, \
	value__AUDCFG_FSYN_CFG__BW_SEL__BAD_REFERENCE(ip))

#define value__AUDCFG_FSYN_CFG__BW_SEL__VERY_BAD_REFERENCE(ip) 0x3
#define mask__AUDCFG_FSYN_CFG__BW_SEL__VERY_BAD_REFERENCE(ip) \
	(value__AUDCFG_FSYN_CFG__BW_SEL__VERY_BAD_REFERENCE(ip) << \
	shift__AUDCFG_FSYN_CFG__BW_SEL(ip))
#define set__AUDCFG_FSYN_CFG__BW_SEL__VERY_BAD_REFERENCE(ip) \
	set__AUDCFG_FSYN_CFG__BW_SEL(ip, \
	value__AUDCFG_FSYN_CFG__BW_SEL__VERY_BAD_REFERENCE(ip))

/* REF_CLK_IN */

#define shift__AUDCFG_FSYN_CFG__REF_CLK_IN(ip) (ip->ver < \
	ver__AUDCFG_FSYN__65_2_1_2 ? 23 : (ip->ver < ver__AUDCFG_FSYN__65_3_1 \
	? 24 : (ip->ver < ver__AUDCFG_FSYN__65_3_3 ? 23 : 24)))
#define mask__AUDCFG_FSYN_CFG__REF_CLK_IN(ip) (ip->ver < \
	ver__AUDCFG_FSYN__65_2_1_2 ? 0x1 : (ip->ver < ver__AUDCFG_FSYN__65_3_1 \
	? 0x1 : (ip->ver < ver__AUDCFG_FSYN__65_3_3 ? 0x3 : 0x1)))
#define get__AUDCFG_FSYN_CFG__REF_CLK_IN(ip) ((readl(ip->base + \
	offset__AUDCFG_FSYN_CFG(ip)) >> \
	shift__AUDCFG_FSYN_CFG__REF_CLK_IN(ip)) & \
	mask__AUDCFG_FSYN_CFG__REF_CLK_IN(ip))
#define set__AUDCFG_FSYN_CFG__REF_CLK_IN(ip, value) \
	writel((readl(ip->base + offset__AUDCFG_FSYN_CFG(ip)) & \
	~(mask__AUDCFG_FSYN_CFG__REF_CLK_IN(ip) << \
	shift__AUDCFG_FSYN_CFG__REF_CLK_IN(ip))) | (((value) & \
	mask__AUDCFG_FSYN_CFG__REF_CLK_IN(ip)) << \
	shift__AUDCFG_FSYN_CFG__REF_CLK_IN(ip)), ip->base + \
	offset__AUDCFG_FSYN_CFG(ip))

#define value__AUDCFG_FSYN_CFG__REF_CLK_IN__FE900_CLOCK(ip) (ip->ver < \
	ver__AUDCFG_FSYN__65_3_1 ? -1 : (ip->ver < ver__AUDCFG_FSYN__65_3_3 ? \
	0x0 : -1))
#define mask__AUDCFG_FSYN_CFG__REF_CLK_IN__FE900_CLOCK(ip) \
	(value__AUDCFG_FSYN_CFG__REF_CLK_IN__FE900_CLOCK(ip) << \
	shift__AUDCFG_FSYN_CFG__REF_CLK_IN(ip))
#define set__AUDCFG_FSYN_CFG__REF_CLK_IN__FE900_CLOCK(ip) \
	set__AUDCFG_FSYN_CFG__REF_CLK_IN(ip, \
	value__AUDCFG_FSYN_CFG__REF_CLK_IN__FE900_CLOCK(ip))

#define value__AUDCFG_FSYN_CFG__REF_CLK_IN__30_MHZ_CLOCK(ip) (ip->ver \
	< ver__AUDCFG_FSYN__65_3_1 ? 0x0 : (ip->ver < ver__AUDCFG_FSYN__65_3_3 \
	? 0x1 : 0x0))
#define mask__AUDCFG_FSYN_CFG__REF_CLK_IN__30_MHZ_CLOCK(ip) \
	(value__AUDCFG_FSYN_CFG__REF_CLK_IN__30_MHZ_CLOCK(ip) << \
	shift__AUDCFG_FSYN_CFG__REF_CLK_IN(ip))
#define set__AUDCFG_FSYN_CFG__REF_CLK_IN__30_MHZ_CLOCK(ip) \
	set__AUDCFG_FSYN_CFG__REF_CLK_IN(ip, \
	value__AUDCFG_FSYN_CFG__REF_CLK_IN__30_MHZ_CLOCK(ip))

#define value__AUDCFG_FSYN_CFG__REF_CLK_IN__SYSCLKINALT(ip) (ip->ver < \
	ver__AUDCFG_FSYN__65_3_1 ? 0x1 : (ip->ver < ver__AUDCFG_FSYN__65_3_3 ? \
	0x2 : 0x1))
#define mask__AUDCFG_FSYN_CFG__REF_CLK_IN__SYSCLKINALT(ip) \
	(value__AUDCFG_FSYN_CFG__REF_CLK_IN__SYSCLKINALT(ip) << \
	shift__AUDCFG_FSYN_CFG__REF_CLK_IN(ip))
#define set__AUDCFG_FSYN_CFG__REF_CLK_IN__SYSCLKINALT(ip) \
	set__AUDCFG_FSYN_CFG__REF_CLK_IN(ip, \
	value__AUDCFG_FSYN_CFG__REF_CLK_IN__SYSCLKINALT(ip))



/*
 * AUDCFG_FSYN_MD
 */

#define offset__AUDCFG_FSYN_MD(ip, n) ((n + 1) * 0x10 + 0x00)
#define get__AUDCFG_FSYN_MD(ip, n) readl(ip->base + \
	offset__AUDCFG_FSYN_MD(ip, n))
#define set__AUDCFG_FSYN_MD(ip, n, value) writel(value, ip->base + \
	offset__AUDCFG_FSYN_MD(ip, n))

/* MD */

#define shift__AUDCFG_FSYN_MD__MD(ip) 0
#define mask__AUDCFG_FSYN_MD__MD(ip) 0x1f
#define get__AUDCFG_FSYN_MD__MD(ip, n) ((readl(ip->base + \
	offset__AUDCFG_FSYN_MD(ip, n)) >> shift__AUDCFG_FSYN_MD__MD(ip)) & \
	mask__AUDCFG_FSYN_MD__MD(ip))
#define set__AUDCFG_FSYN_MD__MD(ip, n, value) writel((readl(ip->base + \
	offset__AUDCFG_FSYN_MD(ip, n)) & ~(mask__AUDCFG_FSYN_MD__MD(ip) << \
	shift__AUDCFG_FSYN_MD__MD(ip))) | (((value) & \
	mask__AUDCFG_FSYN_MD__MD(ip)) << shift__AUDCFG_FSYN_MD__MD(ip)), \
	ip->base + offset__AUDCFG_FSYN_MD(ip, n))



/*
 * AUDCFG_FSYN_PE
 */

#define offset__AUDCFG_FSYN_PE(ip, n) ((n + 1) * 0x10 + 0x04)
#define get__AUDCFG_FSYN_PE(ip, n) readl(ip->base + \
	offset__AUDCFG_FSYN_PE(ip, n))
#define set__AUDCFG_FSYN_PE(ip, n, value) writel(value, ip->base + \
	offset__AUDCFG_FSYN_PE(ip, n))

/* PE */

#define shift__AUDCFG_FSYN_PE__PE(ip) 0
#define mask__AUDCFG_FSYN_PE__PE(ip) 0xffff
#define get__AUDCFG_FSYN_PE__PE(ip, n) ((readl(ip->base + \
	offset__AUDCFG_FSYN_PE(ip, n)) >> shift__AUDCFG_FSYN_PE__PE(ip)) & \
	mask__AUDCFG_FSYN_PE__PE(ip))
#define set__AUDCFG_FSYN_PE__PE(ip, n, value) writel((readl(ip->base + \
	offset__AUDCFG_FSYN_PE(ip, n)) & ~(mask__AUDCFG_FSYN_PE__PE(ip) << \
	shift__AUDCFG_FSYN_PE__PE(ip))) | (((value) & \
	mask__AUDCFG_FSYN_PE__PE(ip)) << shift__AUDCFG_FSYN_PE__PE(ip)), \
	ip->base + offset__AUDCFG_FSYN_PE(ip, n))



/*
 * AUDCFG_FSYN_SDIV
 */

#define offset__AUDCFG_FSYN_SDIV(ip, n) ((n + 1) * 0x10 + 0x08)
#define get__AUDCFG_FSYN_SDIV(ip, n) readl(ip->base + \
	offset__AUDCFG_FSYN_SDIV(ip, n))
#define set__AUDCFG_FSYN_SDIV(ip, n, value) writel(value, ip->base + \
	offset__AUDCFG_FSYN_SDIV(ip, n))

/* SDIV */

#define shift__AUDCFG_FSYN_SDIV__SDIV(ip) 0
#define mask__AUDCFG_FSYN_SDIV__SDIV(ip) 0x7
#define get__AUDCFG_FSYN_SDIV__SDIV(ip, n) ((readl(ip->base + \
	offset__AUDCFG_FSYN_SDIV(ip, n)) >> shift__AUDCFG_FSYN_SDIV__SDIV(ip)) \
	& mask__AUDCFG_FSYN_SDIV__SDIV(ip))
#define set__AUDCFG_FSYN_SDIV__SDIV(ip, n, value) \
	writel((readl(ip->base + offset__AUDCFG_FSYN_SDIV(ip, n)) & \
	~(mask__AUDCFG_FSYN_SDIV__SDIV(ip) << \
	shift__AUDCFG_FSYN_SDIV__SDIV(ip))) | (((value) & \
	mask__AUDCFG_FSYN_SDIV__SDIV(ip)) << \
	shift__AUDCFG_FSYN_SDIV__SDIV(ip)), ip->base + \
	offset__AUDCFG_FSYN_SDIV(ip, n))



/*
 * AUDCFG_FSYN_PROGEN
 */

#define offset__AUDCFG_FSYN_PROGEN(ip, n) ((n + 1) * 0x10 + 0x0c)
#define get__AUDCFG_FSYN_PROGEN(ip, n) readl(ip->base + \
	offset__AUDCFG_FSYN_PROGEN(ip, n))
#define set__AUDCFG_FSYN_PROGEN(ip, n, value) writel(value, ip->base + \
	offset__AUDCFG_FSYN_PROGEN(ip, n))

/* PROG_EN */

#define shift__AUDCFG_FSYN_PROGEN__PROG_EN(ip) 0
#define mask__AUDCFG_FSYN_PROGEN__PROG_EN(ip) 0x1
#define get__AUDCFG_FSYN_PROGEN__PROG_EN(ip, n) ((readl(ip->base + \
	offset__AUDCFG_FSYN_PROGEN(ip, n)) >> \
	shift__AUDCFG_FSYN_PROGEN__PROG_EN(ip)) & \
	mask__AUDCFG_FSYN_PROGEN__PROG_EN(ip))
#define set__AUDCFG_FSYN_PROGEN__PROG_EN(ip, n, value) \
	writel((readl(ip->base + offset__AUDCFG_FSYN_PROGEN(ip, n)) & \
	~(mask__AUDCFG_FSYN_PROGEN__PROG_EN(ip) << \
	shift__AUDCFG_FSYN_PROGEN__PROG_EN(ip))) | (((value) & \
	mask__AUDCFG_FSYN_PROGEN__PROG_EN(ip)) << \
	shift__AUDCFG_FSYN_PROGEN__PROG_EN(ip)), ip->base + \
	offset__AUDCFG_FSYN_PROGEN(ip, n))

#define value__AUDCFG_FSYN_PROGEN__PROG_EN__PE0_MD0_IGNORED(ip) 0x0
#define mask__AUDCFG_FSYN_PROGEN__PROG_EN__PE0_MD0_IGNORED(ip) \
	(value__AUDCFG_FSYN_PROGEN__PROG_EN__PE0_MD0_IGNORED(ip) << \
	shift__AUDCFG_FSYN_PROGEN__PROG_EN(ip))
#define set__AUDCFG_FSYN_PROGEN__PROG_EN__PE0_MD0_IGNORED(ip, n) \
	set__AUDCFG_FSYN_PROGEN__PROG_EN(ip, n, \
	value__AUDCFG_FSYN_PROGEN__PROG_EN__PE0_MD0_IGNORED(ip))

#define value__AUDCFG_FSYN_PROGEN__PROG_EN__PE0_MD0_USED(ip) 0x1
#define mask__AUDCFG_FSYN_PROGEN__PROG_EN__PE0_MD0_USED(ip) \
	(value__AUDCFG_FSYN_PROGEN__PROG_EN__PE0_MD0_USED(ip) << \
	shift__AUDCFG_FSYN_PROGEN__PROG_EN(ip))
#define set__AUDCFG_FSYN_PROGEN__PROG_EN__PE0_MD0_USED(ip, n) \
	set__AUDCFG_FSYN_PROGEN__PROG_EN(ip, n, \
	value__AUDCFG_FSYN_PROGEN__PROG_EN__PE0_MD0_USED(ip))



#endif
