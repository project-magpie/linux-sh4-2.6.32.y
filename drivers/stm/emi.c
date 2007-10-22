/*
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/io.h>
#include <linux/stm/emi.h>

static unsigned long emi_memory_base;
static void __iomem *emi_control;

#define BANK_BASEADDRESS(b)		(0x800 + (0x10*b))
#define BANK_EMICONFIGDATA(b, r)	(0x100 + (0x40*b) + (8*r))

int __init emi_init(unsigned long memory_base, unsigned long control_base)
{
	if (!request_mem_region(control_base, 0x864, "EMI"))
		return -EBUSY;

	emi_control = ioremap(control_base, 0x864);
	if (emi_control == NULL)
		return -ENOMEM;

	emi_memory_base = memory_base;

	return 0;
}

unsigned long __init emi_bank_base(int bank)
{
	unsigned long reg = readl(emi_control + BANK_BASEADDRESS(bank));
	return emi_memory_base + (reg << 22);
}

/*
               ______________________________
FMIADDR    ___/                              \________
              \______________________________/


(The cycle time specified in nano seconds)

               |-----------------------------| cycle_time
                ______________                ___________
CYCLE_TIME     /              \______________/


(IORD_start the number of nano seconds after the start of the cycle the
RD strobe is asserted
 IORD_end   the number of nano seconds before the end of the cycle the
RD strob is de-asserted.)
                  _______________________
IORD       ______/                       \________

              |--|                       |---|
                ^--- IORD_start            ^----- IORD_end

(RD_latch the number of nano seconds at the end of the cycle the read
data is latched)
                                 __
RD_LATCH  ______________________/__\________

                                |------------|
                                     ^---------- RD_latch

(IOWR_start the number of nano seconds after the start of the cycle the
WR strobe is asserted
 IOWR_end   the number of nano seconds before the end of the cycle the
WR strob is de-asserted.)
                  _______________________
IOWR       ______/                       \________

              |--|                       |---|
                ^--- IOWR_start            ^----- IOWR_end



*/

/* NOTE: these calculations assume a 100MHZ clock */

static void __init set_read_timings(int bank, int cycle_time,int IORD_start,
				    int IORD_end,int RD_latch)
{
	cycle_time = cycle_time / 10;
	IORD_start = IORD_start / 5 ;
	IORD_end   = IORD_end / 5 ;
	RD_latch   = RD_latch / 10;

	writel((cycle_time << 24) | (IORD_start << 8) | (IORD_end << 12),
	       emi_control+BANK_EMICONFIGDATA(bank,1));
	writel(0x791 | (RD_latch << 20),
	       emi_control+BANK_EMICONFIGDATA(bank,0));
}

static void __init set_write_timings(int bank, int cycle_time,int IOWR_start,
				     int IOWR_end)
{
	cycle_time = cycle_time / 10;
	IOWR_start = IOWR_start / 5 ;
	IOWR_end   = IOWR_end / 5 ;

	writel((cycle_time << 24) | (IOWR_start << 8) | (IOWR_end << 12),
	       emi_control+BANK_EMICONFIGDATA(bank,2));
}

void __init emi_config_pata(int bank)
{
	/* Set timings for PIO4 */
	set_read_timings(bank, 120,35,30,20);
	set_write_timings(bank, 120,35,30);
}
