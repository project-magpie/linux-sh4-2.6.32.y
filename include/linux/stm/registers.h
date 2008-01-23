#ifndef __STM_REGISTERS_H
#define __STM_REGISTERS_H

#include <asm/io.h>



/* Register access macros */

#define REGISTER_PEEK(base, reg) \
		readl(base + reg)

#define REGISTER_PEEK_N(base, reg, n) \
		readl(base + reg(n))

#define REGISTER_POKE(base, reg, u32value) \
		writel(u32value, base + reg)

#define REGISTER_POKE_N(base, reg, n, u32value) \
		writel(u32value, base + reg(n))



/* Field raw access macros */

#define REGFIELD_PEEK(base, reg, field) \
		regfield_peek(base, reg, reg##__##field##__SHIFT, \
				reg##__##field##__MASK)

#define REGFIELD_PEEK_N(base, reg, n, field) \
		regfield_peek(base, reg(n), reg##__##field##__SHIFT, \
				reg##__##field##__MASK)

#define REGFIELD_POKE(base, reg, field, u32value) \
		regfield_poke(base, reg, reg##__##field##__SHIFT, \
				reg##__##field##__MASK, u32value)

#define REGFIELD_POKE_N(base, reg, n, field, u32value) \
		regfield_poke(base, reg(n), reg##__##field##__SHIFT, \
				reg##__##field##__MASK, u32value)



/* Field named values access macro */

#define REGFIELD_SET(base, reg, field, valuename) \
		regfield_poke(base, reg, reg##__##field##__SHIFT, \
				reg##__##field##__MASK, reg##__##field##__VALUE__##valuename)

#define REGFIELD_SET_N(base, reg, n, field, valuename) \
		regfield_poke(base, reg(n), reg##__##field##__SHIFT, \
				reg##__##field##__MASK, reg##__##field##__VALUE__##valuename)



/* Bitmask generator macros */

#define REGFIELD_MASK(reg, field) \
		(reg##__##field##__MASK << reg##__##field##__SHIFT)

#define REGFIELD_VALUE(reg, field, valuename) \
		(reg##__##field##__VALUE__##valuename << reg##__##field##__SHIFT)



/* Register fields runtime access */

static inline unsigned long regfield_peek(void *base, unsigned long offset,
		int shift, unsigned long mask)
{
	return ((readl(base + offset) >> shift) & mask);
}

static inline void regfield_poke(void *base, unsigned long offset,
		int shift, unsigned long mask, unsigned long value)
{
	writel(((readl(base + offset) & ~(mask << shift)) |
				((value & mask) << shift)), base + offset);
}



/* COMMs registers definitions */

#include <linux/stm/registers/asc.h>
#include <linux/stm/registers/pwm.h>

/* Audio registers definitions */

#include <linux/stm/registers/aud_pcmin.h>
#include <linux/stm/registers/aud_pcmout.h>
#include <linux/stm/registers/aud_spdif.h>
#include <linux/stm/registers/audcfg.h>
#include <linux/stm/registers/audcfg_adac.h>
#include <linux/stm/registers/audcfg_fsyn.h>



#endif
