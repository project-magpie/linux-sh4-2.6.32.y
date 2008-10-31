#ifndef __SND_STM_710X_AUDCFG_H
#define __SND_STM_710X_AUDCFG_H



/*
 * 710X_AUDCFG_IO_CTRL
 */

#define offset__710X_AUDCFG_IO_CTRL(ip) 0x00
#define get__710X_AUDCFG_IO_CTRL(ip) readl(ip->base + \
	offset__710X_AUDCFG_IO_CTRL(ip))
#define set__710X_AUDCFG_IO_CTRL(ip, value) writel(value, ip->base + \
	offset__710X_AUDCFG_IO_CTRL(ip))

/* PCM_CLK_EN */

#define shift__710X_AUDCFG_IO_CTRL__PCM_CLK_EN(ip) 0
#define mask__710X_AUDCFG_IO_CTRL__PCM_CLK_EN(ip) 0x1
#define get__710X_AUDCFG_IO_CTRL__PCM_CLK_EN(ip) ((readl(ip->base + \
	offset__710X_AUDCFG_IO_CTRL(ip)) >> \
	shift__710X_AUDCFG_IO_CTRL__PCM_CLK_EN(ip)) & \
	mask__710X_AUDCFG_IO_CTRL__PCM_CLK_EN(ip))
#define set__710X_AUDCFG_IO_CTRL__PCM_CLK_EN(ip, value) \
	writel((readl(ip->base + offset__710X_AUDCFG_IO_CTRL(ip)) & \
	~(mask__710X_AUDCFG_IO_CTRL__PCM_CLK_EN(ip) << \
	shift__710X_AUDCFG_IO_CTRL__PCM_CLK_EN(ip))) | (((value) & \
	mask__710X_AUDCFG_IO_CTRL__PCM_CLK_EN(ip)) << \
	shift__710X_AUDCFG_IO_CTRL__PCM_CLK_EN(ip)), ip->base + \
	offset__710X_AUDCFG_IO_CTRL(ip))

#define value__710X_AUDCFG_IO_CTRL__PCM_CLK_EN__INPUT(ip) 0x0
#define mask__710X_AUDCFG_IO_CTRL__PCM_CLK_EN__INPUT(ip) \
	(value__710X_AUDCFG_IO_CTRL__PCM_CLK_EN__INPUT(ip) << \
	shift__710X_AUDCFG_IO_CTRL__PCM_CLK_EN(ip))
#define set__710X_AUDCFG_IO_CTRL__PCM_CLK_EN__INPUT(ip) \
	set__710X_AUDCFG_IO_CTRL__PCM_CLK_EN(ip, \
	value__710X_AUDCFG_IO_CTRL__PCM_CLK_EN__INPUT(ip))

#define value__710X_AUDCFG_IO_CTRL__PCM_CLK_EN__OUTPUT(ip) 0x1
#define mask__710X_AUDCFG_IO_CTRL__PCM_CLK_EN__OUTPUT(ip) \
	(value__710X_AUDCFG_IO_CTRL__PCM_CLK_EN__OUTPUT(ip) << \
	shift__710X_AUDCFG_IO_CTRL__PCM_CLK_EN(ip))
#define set__710X_AUDCFG_IO_CTRL__PCM_CLK_EN__OUTPUT(ip) \
	set__710X_AUDCFG_IO_CTRL__PCM_CLK_EN(ip, \
	value__710X_AUDCFG_IO_CTRL__PCM_CLK_EN__OUTPUT(ip))

/* DATA0_EN */

#define shift__710X_AUDCFG_IO_CTRL__DATA0_EN(ip) 1
#define mask__710X_AUDCFG_IO_CTRL__DATA0_EN(ip) 0x1
#define get__710X_AUDCFG_IO_CTRL__DATA0_EN(ip) ((readl(ip->base + \
	offset__710X_AUDCFG_IO_CTRL(ip)) >> \
	shift__710X_AUDCFG_IO_CTRL__DATA0_EN(ip)) & \
	mask__710X_AUDCFG_IO_CTRL__DATA0_EN(ip))
#define set__710X_AUDCFG_IO_CTRL__DATA0_EN(ip, value) \
	writel((readl(ip->base + offset__710X_AUDCFG_IO_CTRL(ip)) & \
	~(mask__710X_AUDCFG_IO_CTRL__DATA0_EN(ip) << \
	shift__710X_AUDCFG_IO_CTRL__DATA0_EN(ip))) | (((value) & \
	mask__710X_AUDCFG_IO_CTRL__DATA0_EN(ip)) << \
	shift__710X_AUDCFG_IO_CTRL__DATA0_EN(ip)), ip->base + \
	offset__710X_AUDCFG_IO_CTRL(ip))

#define value__710X_AUDCFG_IO_CTRL__DATA0_EN__INPUT(ip) 0x0
#define mask__710X_AUDCFG_IO_CTRL__DATA0_EN__INPUT(ip) \
	(value__710X_AUDCFG_IO_CTRL__DATA0_EN__INPUT(ip) << \
	shift__710X_AUDCFG_IO_CTRL__DATA0_EN(ip))
#define set__710X_AUDCFG_IO_CTRL__DATA0_EN__INPUT(ip) \
	set__710X_AUDCFG_IO_CTRL__DATA0_EN(ip, \
	value__710X_AUDCFG_IO_CTRL__DATA0_EN__INPUT(ip))

#define value__710X_AUDCFG_IO_CTRL__DATA0_EN__OUTPUT(ip) 0x1
#define mask__710X_AUDCFG_IO_CTRL__DATA0_EN__OUTPUT(ip) \
	(value__710X_AUDCFG_IO_CTRL__DATA0_EN__OUTPUT(ip) << \
	shift__710X_AUDCFG_IO_CTRL__DATA0_EN(ip))
#define set__710X_AUDCFG_IO_CTRL__DATA0_EN__OUTPUT(ip) \
	set__710X_AUDCFG_IO_CTRL__DATA0_EN(ip, \
	value__710X_AUDCFG_IO_CTRL__DATA0_EN__OUTPUT(ip))

/* DATA1_EN */

#define shift__710X_AUDCFG_IO_CTRL__DATA1_EN(ip) 2
#define mask__710X_AUDCFG_IO_CTRL__DATA1_EN(ip) 0x1
#define get__710X_AUDCFG_IO_CTRL__DATA1_EN(ip) ((readl(ip->base + \
	offset__710X_AUDCFG_IO_CTRL(ip)) >> \
	shift__710X_AUDCFG_IO_CTRL__DATA1_EN(ip)) & \
	mask__710X_AUDCFG_IO_CTRL__DATA1_EN(ip))
#define set__710X_AUDCFG_IO_CTRL__DATA1_EN(ip, value) \
	writel((readl(ip->base + offset__710X_AUDCFG_IO_CTRL(ip)) & \
	~(mask__710X_AUDCFG_IO_CTRL__DATA1_EN(ip) << \
	shift__710X_AUDCFG_IO_CTRL__DATA1_EN(ip))) | (((value) & \
	mask__710X_AUDCFG_IO_CTRL__DATA1_EN(ip)) << \
	shift__710X_AUDCFG_IO_CTRL__DATA1_EN(ip)), ip->base + \
	offset__710X_AUDCFG_IO_CTRL(ip))

#define value__710X_AUDCFG_IO_CTRL__DATA1_EN__INPUT(ip) 0x0
#define mask__710X_AUDCFG_IO_CTRL__DATA1_EN__INPUT(ip) \
	(value__710X_AUDCFG_IO_CTRL__DATA1_EN__INPUT(ip) << \
	shift__710X_AUDCFG_IO_CTRL__DATA1_EN(ip))
#define set__710X_AUDCFG_IO_CTRL__DATA1_EN__INPUT(ip) \
	set__710X_AUDCFG_IO_CTRL__DATA1_EN(ip, \
	value__710X_AUDCFG_IO_CTRL__DATA1_EN__INPUT(ip))

#define value__710X_AUDCFG_IO_CTRL__DATA1_EN__OUTPUT(ip) 0x1
#define mask__710X_AUDCFG_IO_CTRL__DATA1_EN__OUTPUT(ip) \
	(value__710X_AUDCFG_IO_CTRL__DATA1_EN__OUTPUT(ip) << \
	shift__710X_AUDCFG_IO_CTRL__DATA1_EN(ip))
#define set__710X_AUDCFG_IO_CTRL__DATA1_EN__OUTPUT(ip) \
	set__710X_AUDCFG_IO_CTRL__DATA1_EN(ip, \
	value__710X_AUDCFG_IO_CTRL__DATA1_EN__OUTPUT(ip))

/* SPDIF_EN */

#define shift__710X_AUDCFG_IO_CTRL__SPDIF_EN(ip) 3
#define mask__710X_AUDCFG_IO_CTRL__SPDIF_EN(ip) 0x1
#define get__710X_AUDCFG_IO_CTRL__SPDIF_EN(ip) ((readl(ip->base + \
	offset__710X_AUDCFG_IO_CTRL(ip)) >> \
	shift__710X_AUDCFG_IO_CTRL__SPDIF_EN(ip)) & \
	mask__710X_AUDCFG_IO_CTRL__SPDIF_EN(ip))
#define set__710X_AUDCFG_IO_CTRL__SPDIF_EN(ip, value) \
	writel((readl(ip->base + offset__710X_AUDCFG_IO_CTRL(ip)) & \
	~(mask__710X_AUDCFG_IO_CTRL__SPDIF_EN(ip) << \
	shift__710X_AUDCFG_IO_CTRL__SPDIF_EN(ip))) | (((value) & \
	mask__710X_AUDCFG_IO_CTRL__SPDIF_EN(ip)) << \
	shift__710X_AUDCFG_IO_CTRL__SPDIF_EN(ip)), ip->base + \
	offset__710X_AUDCFG_IO_CTRL(ip))

#define value__710X_AUDCFG_IO_CTRL__SPDIF_EN__DISABLE(ip) 0x0
#define mask__710X_AUDCFG_IO_CTRL__SPDIF_EN__DISABLE(ip) \
	(value__710X_AUDCFG_IO_CTRL__SPDIF_EN__DISABLE(ip) << \
	shift__710X_AUDCFG_IO_CTRL__SPDIF_EN(ip))
#define set__710X_AUDCFG_IO_CTRL__SPDIF_EN__DISABLE(ip) \
	set__710X_AUDCFG_IO_CTRL__SPDIF_EN(ip, \
	value__710X_AUDCFG_IO_CTRL__SPDIF_EN__DISABLE(ip))

#define value__710X_AUDCFG_IO_CTRL__SPDIF_EN__ENABLE(ip) 0x1
#define mask__710X_AUDCFG_IO_CTRL__SPDIF_EN__ENABLE(ip) \
	(value__710X_AUDCFG_IO_CTRL__SPDIF_EN__ENABLE(ip) << \
	shift__710X_AUDCFG_IO_CTRL__SPDIF_EN(ip))
#define set__710X_AUDCFG_IO_CTRL__SPDIF_EN__ENABLE(ip) \
	set__710X_AUDCFG_IO_CTRL__SPDIF_EN(ip, \
	value__710X_AUDCFG_IO_CTRL__SPDIF_EN__ENABLE(ip))

/* HDMI_AUD_SRC */

#define shift__710X_AUDCFG_IO_CTRL__HDMI_AUD_SRC(ip) 4
#define mask__710X_AUDCFG_IO_CTRL__HDMI_AUD_SRC(ip) 0x1
#define get__710X_AUDCFG_IO_CTRL__HDMI_AUD_SRC(ip) ((readl(ip->base + \
	offset__710X_AUDCFG_IO_CTRL(ip)) >> \
	shift__710X_AUDCFG_IO_CTRL__HDMI_AUD_SRC(ip)) & \
	mask__710X_AUDCFG_IO_CTRL__HDMI_AUD_SRC(ip))
#define set__710X_AUDCFG_IO_CTRL__HDMI_AUD_SRC(ip, value) \
	writel((readl(ip->base + offset__710X_AUDCFG_IO_CTRL(ip)) & \
	~(mask__710X_AUDCFG_IO_CTRL__HDMI_AUD_SRC(ip) << \
	shift__710X_AUDCFG_IO_CTRL__HDMI_AUD_SRC(ip))) | (((value) & \
	mask__710X_AUDCFG_IO_CTRL__HDMI_AUD_SRC(ip)) << \
	shift__710X_AUDCFG_IO_CTRL__HDMI_AUD_SRC(ip)), ip->base + \
	offset__710X_AUDCFG_IO_CTRL(ip))

#define value__710X_AUDCFG_IO_CTRL__HDMI_AUD_SRC__PCM_PLAYER(ip) 0x0
#define mask__710X_AUDCFG_IO_CTRL__HDMI_AUD_SRC__PCM_PLAYER(ip) \
	(value__710X_AUDCFG_IO_CTRL__HDMI_AUD_SRC__PCM_PLAYER(ip) << \
	shift__710X_AUDCFG_IO_CTRL__HDMI_AUD_SRC(ip))
#define set__710X_AUDCFG_IO_CTRL__HDMI_AUD_SRC__PCM_PLAYER(ip) \
	set__710X_AUDCFG_IO_CTRL__HDMI_AUD_SRC(ip, \
	value__710X_AUDCFG_IO_CTRL__HDMI_AUD_SRC__PCM_PLAYER(ip))

#define value__710X_AUDCFG_IO_CTRL__HDMI_AUD_SRC__SPDIF_PLAYER(ip) 0x1
#define mask__710X_AUDCFG_IO_CTRL__HDMI_AUD_SRC__SPDIF_PLAYER(ip) \
	(value__710X_AUDCFG_IO_CTRL__HDMI_AUD_SRC__SPDIF_PLAYER(ip) << \
	shift__710X_AUDCFG_IO_CTRL__HDMI_AUD_SRC(ip))
#define set__710X_AUDCFG_IO_CTRL__HDMI_AUD_SRC__SPDIF_PLAYER(ip) \
	set__710X_AUDCFG_IO_CTRL__HDMI_AUD_SRC(ip, \
	value__710X_AUDCFG_IO_CTRL__HDMI_AUD_SRC__SPDIF_PLAYER(ip))



#endif
