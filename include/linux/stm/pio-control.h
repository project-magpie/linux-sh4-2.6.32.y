/*
 * (c) 2010 STMicroelectronics Limited
 *
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#ifndef __LINUX_STM_PIO_CONTROL_H
#define __LINUX_STM_PIO_CONTROL_H

struct stm_pio_control_mode_config {
	int oe:1;
	int pu:1;
	int od:1;
};

struct stm_pio_control_retime_config {
	int retime:2;
	int clk1notclk0:2;
	int clknotdata:2;
	int double_edge:2;
	int invertclk:2;
	int delay_input:3;
};

#endif
