/*
 * OS Specific CLOCK LLA mappings
 *
 * Copyright (c)2008 STMicroelectronics
 */

#ifndef __CLKLLA_OSLAYER_H
#define __CLKLLA_OSLAYER_H

#ifdef __cplusplus
extern "C" {
#endif


#ifdef ST_OS21

#ifndef DEFINED_U32
typedef unsigned int U32;
#define DEFINED_U32
#endif

/* Clock operation registration macro (used by clock-xxxx.c) */
#define REGISTER_OPS( _name, _desc, _init, _setparent, _setfreq, _recalc, _enable, _disable, \
			_observe, _measure ) 				\
static struct clk_ops  _name= {						\
	.init=_init,							\
	.set_parent=_setparent,						\
	.set_rate=_setfreq,						\
	.recalc=_recalc, 						\
	.enable=_enable,						\
	.disable=_disable,						\
	.observe=_observe,						\
	.get_measure=_measure, 						\
 }

/* Clock registration macro (used by clock-xxxx.c) */
#define REGISTER_CLK( _id, _ops, _nominal, _flags ) 			\
[_id] = (clk_t){ .name = #_id,  \
                 .id = _id,     \
                 .ops = (_ops), \
                 .flags=_flags, \
                 .nominal_rate = _nominal \
}

/* Registers access functions */
#include "stsys.h"
#include "clock-regs.h"     /* Defined in "clock_lla/socs" */

/* Register access macros.
   WARNING: take care. Different macros depending on register type !!
   CLOCKGENs => CLK_READ, CLK_WRITE
   SYSCONF   => SYSCONF_READ, SYSCONF_WRITE
   PIO       => PIO_SET_MODE to output clock
 */
#define CLK_READ(a)     STSYS_ReadRegDev32LE(a)
#define CLK_WRITE(a,d)  STSYS_WriteRegDev32LE(a,d)

#define SYS_DEV 0
#define SYS_STA 1
#define SYS_CFG 2

static inline U32 SYSCONF_READ(int type, int num, int lsb, int msb)
{
	unsigned long offset = ((type == 0) ? 0 : ((type == 1) ? 8 : 0x100)) + ((num) * 4 );
	unsigned long tmp;
	int field_bits = msb - lsb + 1;
	tmp = STSYS_ReadRegDev32LE(offset + SYSCFG_BASE_ADDRESS);
	if (field_bits != 32) {
		tmp >>= lsb;
		tmp &= (1 << field_bits) -1;
	}
	return tmp;
}

static inline void SYSCONF_WRITE(int type, int num, int lsb, int msb,
	unsigned long value)
{
	unsigned long offset = ((type == 0) ? 0 : ((type == 1) ? 8 : 0x100)) + ((num) * 4 );
	unsigned long tmp;
	int field_bits = msb - lsb + 1;
	tmp = STSYS_ReadRegDev32LE(offset + SYSCFG_BASE_ADDRESS);
	if (field_bits != 32) {
		unsigned long mask = ~(((1 << field_bits) -1) << lsb);
		tmp &= mask;
		tmp |= value << lsb;
	}
	STSYS_WriteRegDev32LE(offset + SYSCFG_BASE_ADDRESS, tmp);
}

#define STPIO_NONPIO		0	/* Non-PIO function (ST40 defn) */
#define STPIO_BIDIR_Z1		0	/* Input weak pull-up (arch defn) */
#define STPIO_BIDIR		1	/* Bidirectonal open-drain */
#define STPIO_OUT		2	/* Output push-pull */
#define STPIO_IN		4	/* Input Hi-Z */
#define STPIO_ALT_OUT		6	/* Alt output push-pull (arch defn) */
#define STPIO_ALT_BIDIR		7	/* Alt bidir open drain (arch defn) */

static inline void PIO_SET_MODE(unsigned long bank, unsigned long line, long mode)
{
    unsigned long piobase = PIO_BASE_ADDRESS(bank);
    unsigned long offset = ((mode & (1 << 0)) ? 0x4 : 0x8);
    STSYS_WriteRegDev32LE((void*)(piobase + PIO_CLEAR_PnC0 + offset),
        (0x1 << line));
    STSYS_WriteRegDev32LE((void*)(piobase + PIO_SET_PnC1 + offset),
        (0x1 << line));
    STSYS_WriteRegDev32LE((void*)(piobase + PIO_CLEAR_PnC2 + offset),
        (0x1 << line));
}

#else   /* LINUX */

#include <linux/io.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/pio.h>
#include <asm-generic/errno-base.h>

#define clk_t	struct clk
#define U32 					unsigned long

/* Register access macros */
#define CLK_READ(addr)          		ioread32(addr)
#define CLK_WRITE(addr,val)			iowrite32(val, addr)
#define STSYS_ReadRegDev32LE(addr)		ioread32(addr)
#define STSYS_WriteRegDev32LE(addr,val)		iowrite32(val, addr)

static inline U32  SYSCONF_READ(unsigned long type, unsigned long num,
	unsigned long lsb, unsigned long msb)
{
	static struct sysconf_field *sc;
	if (!sc)
		sc = sysconf_claim(type, num,  lsb, msb, "Clk lla");
	return sysconf_read(sc);;
}

static inline void SYSCONF_WRITE(unsigned long type, unsigned long num,
	unsigned long lsb, unsigned long msb, unsigned long value)
{
	static struct sysconf_field *sc;
	if (!sc)
		sc = sysconf_claim(type, num,  lsb, msb, "Clk lla");
	sysconf_write(sc, value);
}

static inline void PIO_SET_MODE(unsigned long bank, unsigned long line, long mode)
{
	static struct stpio_pin *pio;
	if (!pio)
		pio = stpio_request_pin(bank, line, "Clk Observer", mode);
	else
		stpio_configure_pin(pio, mode);
}

#ifdef CONFIG_CLK_LOW_LEVEL_DEBUG
typedef struct clk_ops_dbg
{
        const char *name;       /* Clocks group description ("clockgen A","LMI"...) */
} clk_ops_dbg_t;

/*
 *  Clock operation with debug info
 */

/* Clock operation registration macro (used by clock-xxxx.c) */
#define REGISTER_OPS( _name, _desc, _init, _setparent, _setfreq, _recalc, _enable, _disable, \
			_observe, _measure )				\
static struct clk_ops  _name= {						\
	.init=_init,							\
	.set_parent=_setparent,						\
	.set_rate=_setfreq,						\
	.recalc=_recalc,						\
	.enable=_enable,						\
	.disable=_disable,						\
	.observe=_observe,						\
	.get_measure=_measure,						\
	.private_data=(void*)&(clk_ops_dbg_t){ .name=_desc },		\
}

/* Clock registration macro (used by clock-xxxx.c) */
#define REGISTER_CLK( _id, _ops, _nominal, _flags )			\
[_id] = (clk_t){ .name = #_id,						\
		 .id = _id,						\
		 .ops = (_ops),						\
		 .flags=_flags,						\
		 .nominal_rate=_nominal,				\
}
#else
/*
 * CLock Operation without debug info
 */
#define REGISTER_OPS( _name, _desc, _init, _setparent, _setfreq, _recalc, _enable, _disable, \
			_observe, _measure )				\
static struct clk_ops  _name= {						\
	.init=_init,							\
	.set_parent=_setparent,						\
	.set_rate=_setfreq,						\
	.recalc=_recalc,						\
	.enable=_enable,						\
	.disable=_disable,						\
	.observe=_observe,						\
	.get_measure=_measure,						\
}

/* Clock registration macro (used by clock-xxxx.c) */
#define REGISTER_CLK( _id, _ops, _nominal, _flags )			\
[_id] = (clk_t){ .name = #_id,						\
		 .id = _id,						\
		 .ops = (_ops),						\
		 .flags=_flags,						\
}
#endif

#define time_ticks_per_sec()                    CONFIG_HZ
#define task_delay(x)                           mdelay((x)/CONFIG_HZ)

/* Low level API errors */
typedef enum clk_err
{
	CLK_ERR_NONE = 0,
	CLK_ERR_FEATURE_NOT_SUPPORTED = -EPERM,
	CLK_ERR_BAD_PARAMETER = -EINVAL,
	CLK_ERR_INTERNAL = -EFAULT /* Internal & fatal error */
} clk_err_t;

#endif	/* End Linux */


#ifdef __cplusplus
}
#endif
#endif /* #ifndef __CLKLLA_OSLAYER_H */
