/*
 *   STMicroelectronics System-on-Chips' DAC abstraction layer
 *
 *   Copyright (c) 2005-2007 STMicroelectronics Limited
 *
 *   Author: Pawel MOLL <pawel.moll@st.com>
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/init.h>
#include <linux/io.h>
#include <linux/device.h>
#include <linux/list.h>
#include <linux/stm/soc.h>
#include <linux/stm/registers.h>
#include <sound/driver.h>
#include <sound/core.h>

#undef TRACE /* See common.h debug features */
#define MAGIC 2 /* See common.h debug features */
#include "common.h"



/*
 * Audio DAC public interface implementation
 */

int snd_stm_dac_internal_get_config(struct device *device,
		unsigned long *pcm_format, unsigned int *oversampling);
int snd_stm_dac_internal_shut_down(struct device *device);
int snd_stm_dac_internal_wake_up(struct device *device);
int snd_stm_dac_internal_mute(struct device *device);
int snd_stm_dac_internal_unmute(struct device *device);



/* Gets PCM format required by DAC as described in <linux/stm/soc.h> */
int snd_stm_dac_get_config(struct device *device,
		unsigned long *pcm_format, unsigned int *oversampling)
{
	/* TODO */
	return snd_stm_dac_internal_get_config(device, pcm_format,
			oversampling);
}

int snd_stm_dac_shut_down(struct device *device)
{
	/* TODO */
	return snd_stm_dac_internal_shut_down(device);;
}

int snd_stm_dac_wake_up(struct device *device)
{
	/* TODO */
	return snd_stm_dac_internal_wake_up(device);
}

int snd_stm_dac_mute(struct device *device)
{
	/* TODO */
	return snd_stm_dac_internal_mute(device);
}

int snd_stm_dac_unmute(struct device *device)
{
	return snd_stm_dac_internal_unmute(device);
}
