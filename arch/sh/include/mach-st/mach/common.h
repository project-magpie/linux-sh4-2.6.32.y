#ifndef __ARCH_SH_BOARDS_ST_COMMON_COMMON_H
#define __ARCH_SH_BOARDS_ST_COMMON_COMMON_H

#include <linux/platform_device.h>

/* epld.c */

struct plat_epld_data {
	int opsize;
};

void epld_write(unsigned long value, unsigned long offset);
unsigned long epld_read(unsigned long offset);
void epld_early_init(struct platform_device *device);

/* harp.c */

void harp_init_irq(void);

#endif
