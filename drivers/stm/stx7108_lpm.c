/*
 * <root>/drivers/stm/stx7108_lpm.c
 *
 * This define resources for external SBC
 *
 * Copyright (C) 2012 STMicroelectronics Limited
 *
 * Contributor:Francesco Virlinzi <francesco.virlinzi@st.com>
 * Author:Pooja Agarwal <pooja.agarwal@st.com>
 * Author:Udit Kumar <udit-dlh.kumar@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public License.
 * See linux/COPYING for more information.
*/

#include <linux/i2c.h>
#include <linux/stm/platform.h>
#include "linux/stm/stx7108.h"


static struct i2c_board_info board_info = {
		I2C_BOARD_INFO("stm-lpm", 0),
		.platform_data = &(struct stm_lpm_i2c_data){
			.number_i2c = 0,
			.number_gpio = 0,
		},
};

void stx7108_configure_lpm_i2c_interface(struct stx7108_lpm_i2c_config  *config)
{
	struct stm_lpm_i2c_data *i2c_data = board_info.platform_data;
	i2c_data->number_i2c = config->number_i2c;
	i2c_data->number_gpio = config->number_gpio;
}

static int __init stx7108_lpm_init(void)
{
	int err = 0;
	struct stm_lpm_i2c_data *i2c_data;
	struct i2c_client *client;
	/* get 7108 specific i2c data */
	struct i2c_board_info *info = &board_info;
	i2c_data = info->platform_data;
	/* get adapter on which i2c stm8 is connected */
	i2c_data->i2c_adap = i2c_get_adapter(i2c_data->number_i2c);
	if (i2c_data->i2c_adap == NULL)
		return err;
	/* add new device with above adapter */
	client = i2c_new_device(i2c_data->i2c_adap, &board_info);
	if (client == NULL)
		return err;
	/* set i2c_data as client i2c data */
	i2c_set_clientdata(client , i2c_data);
	return err;
}

module_init(stx7108_lpm_init);
