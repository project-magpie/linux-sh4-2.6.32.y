#ifndef __SND_STM_7111_AUDCFG_H
#define __SND_STM_7111_AUDCFG_H



/*
 * 7111_AUDCFG_IO_CTRL
 */

#define offset__7111_AUDCFG_IO_CTRL(ip) 0x00
#define get__7111_AUDCFG_IO_CTRL(ip) readl(ip->base + \
	offset__7111_AUDCFG_IO_CTRL(ip))
#define set__7111_AUDCFG_IO_CTRL(ip, value) writel(value, ip->base + \
	offset__7111_AUDCFG_IO_CTRL(ip))

/* PCM_CLK_EN */

#define shift__7111_AUDCFG_IO_CTRL__PCM_CLK_EN(ip) 0
#define mask__7111_AUDCFG_IO_CTRL__PCM_CLK_EN(ip) 0x1
#define get__7111_AUDCFG_IO_CTRL__PCM_CLK_EN(ip) ((readl(ip->base + \
	offset__7111_AUDCFG_IO_CTRL(ip)) >> \
	shift__7111_AUDCFG_IO_CTRL__PCM_CLK_EN(ip)) & \
	mask__7111_AUDCFG_IO_CTRL__PCM_CLK_EN(ip))
#define set__7111_AUDCFG_IO_CTRL__PCM_CLK_EN(ip, value) \
	writel((readl(ip->base + offset__7111_AUDCFG_IO_CTRL(ip)) & \
	~(mask__7111_AUDCFG_IO_CTRL__PCM_CLK_EN(ip) << \
	shift__7111_AUDCFG_IO_CTRL__PCM_CLK_EN(ip))) | (((value) & \
	mask__7111_AUDCFG_IO_CTRL__PCM_CLK_EN(ip)) << \
	shift__7111_AUDCFG_IO_CTRL__PCM_CLK_EN(ip)), ip->base + \
	offset__7111_AUDCFG_IO_CTRL(ip))

#define value__7111_AUDCFG_IO_CTRL__PCM_CLK_EN__INPUT(ip) 0x0
#define mask__7111_AUDCFG_IO_CTRL__PCM_CLK_EN__INPUT(ip) \
	(value__7111_AUDCFG_IO_CTRL__PCM_CLK_EN__INPUT(ip) << \
	shift__7111_AUDCFG_IO_CTRL__PCM_CLK_EN(ip))
#define set__7111_AUDCFG_IO_CTRL__PCM_CLK_EN__INPUT(ip) \
	set__7111_AUDCFG_IO_CTRL__PCM_CLK_EN(ip, \
	value__7111_AUDCFG_IO_CTRL__PCM_CLK_EN__INPUT(ip))

#define value__7111_AUDCFG_IO_CTRL__PCM_CLK_EN__OUTPUT(ip) 0x1
#define mask__7111_AUDCFG_IO_CTRL__PCM_CLK_EN__OUTPUT(ip) \
	(value__7111_AUDCFG_IO_CTRL__PCM_CLK_EN__OUTPUT(ip) << \
	shift__7111_AUDCFG_IO_CTRL__PCM_CLK_EN(ip))
#define set__7111_AUDCFG_IO_CTRL__PCM_CLK_EN__OUTPUT(ip) \
	set__7111_AUDCFG_IO_CTRL__PCM_CLK_EN(ip, \
	value__7111_AUDCFG_IO_CTRL__PCM_CLK_EN__OUTPUT(ip))

/* SPDIFHDMI_EN */

#define shift__7111_AUDCFG_IO_CTRL__SPDIFHDMI_EN(ip) 3
#define mask__7111_AUDCFG_IO_CTRL__SPDIFHDMI_EN(ip) 0x1
#define get__7111_AUDCFG_IO_CTRL__SPDIFHDMI_EN(ip) ((readl(ip->base + \
	offset__7111_AUDCFG_IO_CTRL(ip)) >> \
	shift__7111_AUDCFG_IO_CTRL__SPDIFHDMI_EN(ip)) & \
	mask__7111_AUDCFG_IO_CTRL__SPDIFHDMI_EN(ip))
#define set__7111_AUDCFG_IO_CTRL__SPDIFHDMI_EN(ip, value) \
	writel((readl(ip->base + offset__7111_AUDCFG_IO_CTRL(ip)) & \
	~(mask__7111_AUDCFG_IO_CTRL__SPDIFHDMI_EN(ip) << \
	shift__7111_AUDCFG_IO_CTRL__SPDIFHDMI_EN(ip))) | (((value) & \
	mask__7111_AUDCFG_IO_CTRL__SPDIFHDMI_EN(ip)) << \
	shift__7111_AUDCFG_IO_CTRL__SPDIFHDMI_EN(ip)), ip->base + \
	offset__7111_AUDCFG_IO_CTRL(ip))

#define value__7111_AUDCFG_IO_CTRL__SPDIFHDMI_EN__INPUT(ip) 0x0
#define mask__7111_AUDCFG_IO_CTRL__SPDIFHDMI_EN__INPUT(ip) \
	(value__7111_AUDCFG_IO_CTRL__SPDIFHDMI_EN__INPUT(ip) << \
	shift__7111_AUDCFG_IO_CTRL__SPDIFHDMI_EN(ip))
#define set__7111_AUDCFG_IO_CTRL__SPDIFHDMI_EN__INPUT(ip) \
	set__7111_AUDCFG_IO_CTRL__SPDIFHDMI_EN(ip, \
	value__7111_AUDCFG_IO_CTRL__SPDIFHDMI_EN__INPUT(ip))

#define value__7111_AUDCFG_IO_CTRL__SPDIFHDMI_EN__OUTPUT(ip) 0x1
#define mask__7111_AUDCFG_IO_CTRL__SPDIFHDMI_EN__OUTPUT(ip) \
	(value__7111_AUDCFG_IO_CTRL__SPDIFHDMI_EN__OUTPUT(ip) << \
	shift__7111_AUDCFG_IO_CTRL__SPDIFHDMI_EN(ip))
#define set__7111_AUDCFG_IO_CTRL__SPDIFHDMI_EN__OUTPUT(ip) \
	set__7111_AUDCFG_IO_CTRL__SPDIFHDMI_EN(ip, \
	value__7111_AUDCFG_IO_CTRL__SPDIFHDMI_EN__OUTPUT(ip))

/* PCMPLHDMI_EN */

#define shift__7111_AUDCFG_IO_CTRL__PCMPLHDMI_EN(ip) 5
#define mask__7111_AUDCFG_IO_CTRL__PCMPLHDMI_EN(ip) 0x1
#define get__7111_AUDCFG_IO_CTRL__PCMPLHDMI_EN(ip) ((readl(ip->base + \
	offset__7111_AUDCFG_IO_CTRL(ip)) >> \
	shift__7111_AUDCFG_IO_CTRL__PCMPLHDMI_EN(ip)) & \
	mask__7111_AUDCFG_IO_CTRL__PCMPLHDMI_EN(ip))
#define set__7111_AUDCFG_IO_CTRL__PCMPLHDMI_EN(ip, value) \
	writel((readl(ip->base + offset__7111_AUDCFG_IO_CTRL(ip)) & \
	~(mask__7111_AUDCFG_IO_CTRL__PCMPLHDMI_EN(ip) << \
	shift__7111_AUDCFG_IO_CTRL__PCMPLHDMI_EN(ip))) | (((value) & \
	mask__7111_AUDCFG_IO_CTRL__PCMPLHDMI_EN(ip)) << \
	shift__7111_AUDCFG_IO_CTRL__PCMPLHDMI_EN(ip)), ip->base + \
	offset__7111_AUDCFG_IO_CTRL(ip))

#define value__7111_AUDCFG_IO_CTRL__PCMPLHDMI_EN__INPUT(ip) 0x0
#define mask__7111_AUDCFG_IO_CTRL__PCMPLHDMI_EN__INPUT(ip) \
	(value__7111_AUDCFG_IO_CTRL__PCMPLHDMI_EN__INPUT(ip) << \
	shift__7111_AUDCFG_IO_CTRL__PCMPLHDMI_EN(ip))
#define set__7111_AUDCFG_IO_CTRL__PCMPLHDMI_EN__INPUT(ip) \
	set__7111_AUDCFG_IO_CTRL__PCMPLHDMI_EN(ip, \
	value__7111_AUDCFG_IO_CTRL__PCMPLHDMI_EN__INPUT(ip))

#define value__7111_AUDCFG_IO_CTRL__PCMPLHDMI_EN__OUTPUT(ip) 0x1
#define mask__7111_AUDCFG_IO_CTRL__PCMPLHDMI_EN__OUTPUT(ip) \
	(value__7111_AUDCFG_IO_CTRL__PCMPLHDMI_EN__OUTPUT(ip) << \
	shift__7111_AUDCFG_IO_CTRL__PCMPLHDMI_EN(ip))
#define set__7111_AUDCFG_IO_CTRL__PCMPLHDMI_EN__OUTPUT(ip) \
	set__7111_AUDCFG_IO_CTRL__PCMPLHDMI_EN(ip, \
	value__7111_AUDCFG_IO_CTRL__PCMPLHDMI_EN__OUTPUT(ip))

/* CLKREC_SEL */

#define shift__7111_AUDCFG_IO_CTRL__CLKREC_SEL(ip) 9
#define mask__7111_AUDCFG_IO_CTRL__CLKREC_SEL(ip) 0x3
#define get__7111_AUDCFG_IO_CTRL__CLKREC_SEL(ip) ((readl(ip->base + \
	offset__7111_AUDCFG_IO_CTRL(ip)) >> \
	shift__7111_AUDCFG_IO_CTRL__CLKREC_SEL(ip)) & \
	mask__7111_AUDCFG_IO_CTRL__CLKREC_SEL(ip))
#define set__7111_AUDCFG_IO_CTRL__CLKREC_SEL(ip, value) \
	writel((readl(ip->base + offset__7111_AUDCFG_IO_CTRL(ip)) & \
	~(mask__7111_AUDCFG_IO_CTRL__CLKREC_SEL(ip) << \
	shift__7111_AUDCFG_IO_CTRL__CLKREC_SEL(ip))) | (((value) & \
	mask__7111_AUDCFG_IO_CTRL__CLKREC_SEL(ip)) << \
	shift__7111_AUDCFG_IO_CTRL__CLKREC_SEL(ip)), ip->base + \
	offset__7111_AUDCFG_IO_CTRL(ip))

#define value__7111_AUDCFG_IO_CTRL__CLKREC_SEL__PCMPLHDMI(ip) 0x0
#define mask__7111_AUDCFG_IO_CTRL__CLKREC_SEL__PCMPLHDMI(ip) \
	(value__7111_AUDCFG_IO_CTRL__CLKREC_SEL__PCMPLHDMI(ip) << \
	shift__7111_AUDCFG_IO_CTRL__CLKREC_SEL(ip))
#define set__7111_AUDCFG_IO_CTRL__CLKREC_SEL__PCMPLHDMI(ip) \
	set__7111_AUDCFG_IO_CTRL__CLKREC_SEL(ip, \
	value__7111_AUDCFG_IO_CTRL__CLKREC_SEL__PCMPLHDMI(ip))

#define value__7111_AUDCFG_IO_CTRL__CLKREC_SEL__SPDIFHDMI(ip) 0x1
#define mask__7111_AUDCFG_IO_CTRL__CLKREC_SEL__SPDIFHDMI(ip) \
	(value__7111_AUDCFG_IO_CTRL__CLKREC_SEL__SPDIFHDMI(ip) << \
	shift__7111_AUDCFG_IO_CTRL__CLKREC_SEL(ip))
#define set__7111_AUDCFG_IO_CTRL__CLKREC_SEL__SPDIFHDMI(ip) \
	set__7111_AUDCFG_IO_CTRL__CLKREC_SEL(ip, \
	value__7111_AUDCFG_IO_CTRL__CLKREC_SEL__SPDIFHDMI(ip))

#define value__7111_AUDCFG_IO_CTRL__CLKREC_SEL__PCMPL1(ip) 0x2
#define mask__7111_AUDCFG_IO_CTRL__CLKREC_SEL__PCMPL1(ip) \
	(value__7111_AUDCFG_IO_CTRL__CLKREC_SEL__PCMPL1(ip) << \
	shift__7111_AUDCFG_IO_CTRL__CLKREC_SEL(ip))
#define set__7111_AUDCFG_IO_CTRL__CLKREC_SEL__PCMPL1(ip) \
	set__7111_AUDCFG_IO_CTRL__CLKREC_SEL(ip, \
	value__7111_AUDCFG_IO_CTRL__CLKREC_SEL__PCMPL1(ip))



#endif
