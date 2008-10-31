#ifndef __SND_STM_AUDCFG_ADAC_H
#define __SND_STM_AUDCFG_ADAC_H

/*
 * IP versions
 */

/* 7100 2.0, 7100 3.0 */
#define ver__AUDCFG_ADAC__90_1_0 1

/* 7109 2.0, 7109 3.0 */
#define ver__AUDCFG_ADAC__90_1_5_0 2

/* 7200 1.0 */
#define ver__AUDCFG_ADAC__65_3_0_0 3

/* 7111, 7200 2.0 */
#define ver__AUDCFG_ADAC__65_3_2_a 4



/*
 * AUDCFG_ADAC_CTRL
 */

#define offset__AUDCFG_ADAC_CTRL(ip) 0x00
#define get__AUDCFG_ADAC_CTRL(ip) readl(ip->base + \
	offset__AUDCFG_ADAC_CTRL(ip))
#define set__AUDCFG_ADAC_CTRL(ip, value) writel(value, ip->base + \
	offset__AUDCFG_ADAC_CTRL(ip))

/* NRST */

#define shift__AUDCFG_ADAC_CTRL__NRST(ip) 0
#define mask__AUDCFG_ADAC_CTRL__NRST(ip) 0x1
#define get__AUDCFG_ADAC_CTRL__NRST(ip) ((readl(ip->base + \
	offset__AUDCFG_ADAC_CTRL(ip)) >> shift__AUDCFG_ADAC_CTRL__NRST(ip)) & \
	mask__AUDCFG_ADAC_CTRL__NRST(ip))
#define set__AUDCFG_ADAC_CTRL__NRST(ip, value) writel((readl(ip->base \
	+ offset__AUDCFG_ADAC_CTRL(ip)) & ~(mask__AUDCFG_ADAC_CTRL__NRST(ip) \
	<< shift__AUDCFG_ADAC_CTRL__NRST(ip))) | (((value) & \
	mask__AUDCFG_ADAC_CTRL__NRST(ip)) << \
	shift__AUDCFG_ADAC_CTRL__NRST(ip)), ip->base + \
	offset__AUDCFG_ADAC_CTRL(ip))

#define value__AUDCFG_ADAC_CTRL__NRST__RESET(ip) 0x0
#define mask__AUDCFG_ADAC_CTRL__NRST__RESET(ip) \
	(value__AUDCFG_ADAC_CTRL__NRST__RESET(ip) << \
	shift__AUDCFG_ADAC_CTRL__NRST(ip))
#define set__AUDCFG_ADAC_CTRL__NRST__RESET(ip) \
	set__AUDCFG_ADAC_CTRL__NRST(ip, \
	value__AUDCFG_ADAC_CTRL__NRST__RESET(ip))

#define value__AUDCFG_ADAC_CTRL__NRST__NORMAL(ip) 0x1
#define mask__AUDCFG_ADAC_CTRL__NRST__NORMAL(ip) \
	(value__AUDCFG_ADAC_CTRL__NRST__NORMAL(ip) << \
	shift__AUDCFG_ADAC_CTRL__NRST(ip))
#define set__AUDCFG_ADAC_CTRL__NRST__NORMAL(ip) \
	set__AUDCFG_ADAC_CTRL__NRST(ip, \
	value__AUDCFG_ADAC_CTRL__NRST__NORMAL(ip))

/* MODE */

#define shift__AUDCFG_ADAC_CTRL__MODE(ip) 1
#define mask__AUDCFG_ADAC_CTRL__MODE(ip) 0x3
#define get__AUDCFG_ADAC_CTRL__MODE(ip) ((readl(ip->base + \
	offset__AUDCFG_ADAC_CTRL(ip)) >> shift__AUDCFG_ADAC_CTRL__MODE(ip)) & \
	mask__AUDCFG_ADAC_CTRL__MODE(ip))
#define set__AUDCFG_ADAC_CTRL__MODE(ip, value) writel((readl(ip->base \
	+ offset__AUDCFG_ADAC_CTRL(ip)) & ~(mask__AUDCFG_ADAC_CTRL__MODE(ip) \
	<< shift__AUDCFG_ADAC_CTRL__MODE(ip))) | (((value) & \
	mask__AUDCFG_ADAC_CTRL__MODE(ip)) << \
	shift__AUDCFG_ADAC_CTRL__MODE(ip)), ip->base + \
	offset__AUDCFG_ADAC_CTRL(ip))

#define value__AUDCFG_ADAC_CTRL__MODE__DEFAULT(ip) 0x0
#define mask__AUDCFG_ADAC_CTRL__MODE__DEFAULT(ip) \
	(value__AUDCFG_ADAC_CTRL__MODE__DEFAULT(ip) << \
	shift__AUDCFG_ADAC_CTRL__MODE(ip))
#define set__AUDCFG_ADAC_CTRL__MODE__DEFAULT(ip) \
	set__AUDCFG_ADAC_CTRL__MODE(ip, \
	value__AUDCFG_ADAC_CTRL__MODE__DEFAULT(ip))

/* NSB */

#define shift__AUDCFG_ADAC_CTRL__NSB(ip) 3
#define mask__AUDCFG_ADAC_CTRL__NSB(ip) 0x1
#define get__AUDCFG_ADAC_CTRL__NSB(ip) ((readl(ip->base + \
	offset__AUDCFG_ADAC_CTRL(ip)) >> shift__AUDCFG_ADAC_CTRL__NSB(ip)) & \
	mask__AUDCFG_ADAC_CTRL__NSB(ip))
#define set__AUDCFG_ADAC_CTRL__NSB(ip, value) writel((readl(ip->base + \
	offset__AUDCFG_ADAC_CTRL(ip)) & ~(mask__AUDCFG_ADAC_CTRL__NSB(ip) << \
	shift__AUDCFG_ADAC_CTRL__NSB(ip))) | (((value) & \
	mask__AUDCFG_ADAC_CTRL__NSB(ip)) << shift__AUDCFG_ADAC_CTRL__NSB(ip)), \
	ip->base + offset__AUDCFG_ADAC_CTRL(ip))

#define value__AUDCFG_ADAC_CTRL__NSB__POWER_DOWN(ip) 0x0
#define mask__AUDCFG_ADAC_CTRL__NSB__POWER_DOWN(ip) \
	(value__AUDCFG_ADAC_CTRL__NSB__POWER_DOWN(ip) << \
	shift__AUDCFG_ADAC_CTRL__NSB(ip))
#define set__AUDCFG_ADAC_CTRL__NSB__POWER_DOWN(ip) \
	set__AUDCFG_ADAC_CTRL__NSB(ip, \
	value__AUDCFG_ADAC_CTRL__NSB__POWER_DOWN(ip))

#define value__AUDCFG_ADAC_CTRL__NSB__NORMAL(ip) 0x1
#define mask__AUDCFG_ADAC_CTRL__NSB__NORMAL(ip) \
	(value__AUDCFG_ADAC_CTRL__NSB__NORMAL(ip) << \
	shift__AUDCFG_ADAC_CTRL__NSB(ip))
#define set__AUDCFG_ADAC_CTRL__NSB__NORMAL(ip) \
	set__AUDCFG_ADAC_CTRL__NSB(ip, \
	value__AUDCFG_ADAC_CTRL__NSB__NORMAL(ip))

/* SOFTMUTE */

#define shift__AUDCFG_ADAC_CTRL__SOFTMUTE(ip) 4
#define mask__AUDCFG_ADAC_CTRL__SOFTMUTE(ip) 0x1
#define get__AUDCFG_ADAC_CTRL__SOFTMUTE(ip) ((readl(ip->base + \
	offset__AUDCFG_ADAC_CTRL(ip)) >> \
	shift__AUDCFG_ADAC_CTRL__SOFTMUTE(ip)) & \
	mask__AUDCFG_ADAC_CTRL__SOFTMUTE(ip))
#define set__AUDCFG_ADAC_CTRL__SOFTMUTE(ip, value) \
	writel((readl(ip->base + offset__AUDCFG_ADAC_CTRL(ip)) & \
	~(mask__AUDCFG_ADAC_CTRL__SOFTMUTE(ip) << \
	shift__AUDCFG_ADAC_CTRL__SOFTMUTE(ip))) | (((value) & \
	mask__AUDCFG_ADAC_CTRL__SOFTMUTE(ip)) << \
	shift__AUDCFG_ADAC_CTRL__SOFTMUTE(ip)), ip->base + \
	offset__AUDCFG_ADAC_CTRL(ip))

#define value__AUDCFG_ADAC_CTRL__SOFTMUTE__NORMAL(ip) 0x0
#define mask__AUDCFG_ADAC_CTRL__SOFTMUTE__NORMAL(ip) \
	(value__AUDCFG_ADAC_CTRL__SOFTMUTE__NORMAL(ip) << \
	shift__AUDCFG_ADAC_CTRL__SOFTMUTE(ip))
#define set__AUDCFG_ADAC_CTRL__SOFTMUTE__NORMAL(ip) \
	set__AUDCFG_ADAC_CTRL__SOFTMUTE(ip, \
	value__AUDCFG_ADAC_CTRL__SOFTMUTE__NORMAL(ip))

#define value__AUDCFG_ADAC_CTRL__SOFTMUTE__MUTE(ip) 0x1
#define mask__AUDCFG_ADAC_CTRL__SOFTMUTE__MUTE(ip) \
	(value__AUDCFG_ADAC_CTRL__SOFTMUTE__MUTE(ip) << \
	shift__AUDCFG_ADAC_CTRL__SOFTMUTE(ip))
#define set__AUDCFG_ADAC_CTRL__SOFTMUTE__MUTE(ip) \
	set__AUDCFG_ADAC_CTRL__SOFTMUTE(ip, \
	value__AUDCFG_ADAC_CTRL__SOFTMUTE__MUTE(ip))

/* PDNANA */

#define shift__AUDCFG_ADAC_CTRL__PDNANA(ip) 5
#define mask__AUDCFG_ADAC_CTRL__PDNANA(ip) 0x1
#define get__AUDCFG_ADAC_CTRL__PDNANA(ip) ((readl(ip->base + \
	offset__AUDCFG_ADAC_CTRL(ip)) >> shift__AUDCFG_ADAC_CTRL__PDNANA(ip)) \
	& mask__AUDCFG_ADAC_CTRL__PDNANA(ip))
#define set__AUDCFG_ADAC_CTRL__PDNANA(ip, value) \
	writel((readl(ip->base + offset__AUDCFG_ADAC_CTRL(ip)) & \
	~(mask__AUDCFG_ADAC_CTRL__PDNANA(ip) << \
	shift__AUDCFG_ADAC_CTRL__PDNANA(ip))) | (((value) & \
	mask__AUDCFG_ADAC_CTRL__PDNANA(ip)) << \
	shift__AUDCFG_ADAC_CTRL__PDNANA(ip)), ip->base + \
	offset__AUDCFG_ADAC_CTRL(ip))

#define value__AUDCFG_ADAC_CTRL__PDNANA__POWER_DOWN(ip) 0x0
#define mask__AUDCFG_ADAC_CTRL__PDNANA__POWER_DOWN(ip) \
	(value__AUDCFG_ADAC_CTRL__PDNANA__POWER_DOWN(ip) << \
	shift__AUDCFG_ADAC_CTRL__PDNANA(ip))
#define set__AUDCFG_ADAC_CTRL__PDNANA__POWER_DOWN(ip) \
	set__AUDCFG_ADAC_CTRL__PDNANA(ip, \
	value__AUDCFG_ADAC_CTRL__PDNANA__POWER_DOWN(ip))

#define value__AUDCFG_ADAC_CTRL__PDNANA__NORMAL(ip) 0x1
#define mask__AUDCFG_ADAC_CTRL__PDNANA__NORMAL(ip) \
	(value__AUDCFG_ADAC_CTRL__PDNANA__NORMAL(ip) << \
	shift__AUDCFG_ADAC_CTRL__PDNANA(ip))
#define set__AUDCFG_ADAC_CTRL__PDNANA__NORMAL(ip) \
	set__AUDCFG_ADAC_CTRL__PDNANA(ip, \
	value__AUDCFG_ADAC_CTRL__PDNANA__NORMAL(ip))

/* PDNBG */

#define shift__AUDCFG_ADAC_CTRL__PDNBG(ip) 6
#define mask__AUDCFG_ADAC_CTRL__PDNBG(ip) 0x1
#define get__AUDCFG_ADAC_CTRL__PDNBG(ip) ((readl(ip->base + \
	offset__AUDCFG_ADAC_CTRL(ip)) >> shift__AUDCFG_ADAC_CTRL__PDNBG(ip)) & \
	mask__AUDCFG_ADAC_CTRL__PDNBG(ip))
#define set__AUDCFG_ADAC_CTRL__PDNBG(ip, value) writel((readl(ip->base \
	+ offset__AUDCFG_ADAC_CTRL(ip)) & ~(mask__AUDCFG_ADAC_CTRL__PDNBG(ip) \
	<< shift__AUDCFG_ADAC_CTRL__PDNBG(ip))) | (((value) & \
	mask__AUDCFG_ADAC_CTRL__PDNBG(ip)) << \
	shift__AUDCFG_ADAC_CTRL__PDNBG(ip)), ip->base + \
	offset__AUDCFG_ADAC_CTRL(ip))

#define value__AUDCFG_ADAC_CTRL__PDNBG__POWER_DOWN(ip) 0x0
#define mask__AUDCFG_ADAC_CTRL__PDNBG__POWER_DOWN(ip) \
	(value__AUDCFG_ADAC_CTRL__PDNBG__POWER_DOWN(ip) << \
	shift__AUDCFG_ADAC_CTRL__PDNBG(ip))
#define set__AUDCFG_ADAC_CTRL__PDNBG__POWER_DOWN(ip) \
	set__AUDCFG_ADAC_CTRL__PDNBG(ip, \
	value__AUDCFG_ADAC_CTRL__PDNBG__POWER_DOWN(ip))

#define value__AUDCFG_ADAC_CTRL__PDNBG__NORMAL(ip) 0x1
#define mask__AUDCFG_ADAC_CTRL__PDNBG__NORMAL(ip) \
	(value__AUDCFG_ADAC_CTRL__PDNBG__NORMAL(ip) << \
	shift__AUDCFG_ADAC_CTRL__PDNBG(ip))
#define set__AUDCFG_ADAC_CTRL__PDNBG__NORMAL(ip) \
	set__AUDCFG_ADAC_CTRL__PDNBG(ip, \
	value__AUDCFG_ADAC_CTRL__PDNBG__NORMAL(ip))



#endif
