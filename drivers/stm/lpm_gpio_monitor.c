/*
 * <root>/drivers/stm/lpm_gpio_monitor.c
 *
 * This will intercept GPIO connected with external SBC
 * and export this GPIO activity into user space
 *
 * Use case: On user press of powerkey, box should go in standby,
 * on subsequent standby key press, box should be become active.
 *
 * External SBC will drive this PIO,
 * When user presses standby key on front panel then SBC will drive it to low,
 * on getting low on this gpio, an interrupt will be received on host for gpio.
 * Host will export this as powerkey press for user in sys-fs interface.
 * On seeing power key press, user must initiate movement into HoM mode.
 * On exiting HoM mode, SBC will drive it to high.
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

#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/stm/gpio.h>
#include <linux/stm/lpm.h>
#include <linux/stm/platform.h>

/**
 * monitor_gpio_handler - ISR for GPIO
 * @irq:	irq
 * @ptr:	data
 */

static irqreturn_t monitor_gpio_handler(int irq, void *ptr)
{
	struct stm_lpm_i2c_data *i2c_data;
	struct i2c_client *client_data = ptr;
	i2c_data = i2c_get_clientdata(client_data);
	i2c_data->status_gpio = 0;
	return IRQ_HANDLED;
}

/**
 * stm_lpm_show_powerkey - to show power key in user space
 * @dev:	device pointer
 * @attr:	attribute pointer
 * @buf :	buffer pointer
 */

static ssize_t stm_lpm_show_powerkey(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret = 0;
	struct stm_lpm_i2c_data  *i2c_data = dev->platform_data;
	pr_debug("\n power key pressed %s\n",
	i2c_data->status_gpio == 0 ? "Yes" : "No");
	ret = sprintf(buf, "%d \n", i2c_data->status_gpio);
	return ret;
}

static DEVICE_ATTR(powerkey, S_IRUGO, stm_lpm_show_powerkey, NULL);

/**
 * lpm_start_power_monitor - gpio monitor init function
 * @client_data:	i2c client info
 *
 * This function register ISR with GPIO which is controlled by SBC
 * and exports this GPIO into user land.
 *
 * Return - 0 on success
 * Return - negative error on failure.
 */

int __init lpm_start_power_monitor(struct i2c_client *client_data)
{
	int ret = 0;
	struct stm_lpm_i2c_data *i2c_data;
	i2c_data = i2c_get_clientdata(client_data);
	pr_debug("Platform data gpio no %d", i2c_data->number_gpio);
	/* Request gpio */
	ret = gpio_request(i2c_data->number_gpio, "monitor_gpio");

	if (ret < 0) {
		pr_err("stm_lpm : ERROR %d gpio pin request failed\n", ret);
		return ret;
	}
	/* Configure gpio for power key press */
	gpio_direction_input(i2c_data->number_gpio);
	set_irq_type(gpio_to_irq(i2c_data->number_gpio), IRQF_TRIGGER_FALLING);

	ret = request_irq(gpio_to_irq(i2c_data->number_gpio),
				monitor_gpio_handler, IRQF_DISABLED,
				"monitor_irq", client_data);
	if (ret < 0) {
		gpio_free(i2c_data->number_gpio);
		pr_err("stm_lpm : ERROR %d irq request failed\n", ret);
		return ret;
	}
	/* At init mark power key is not pressed */
	i2c_data->status_gpio = 1;
	ret = device_create_file(&(client_data->dev), &dev_attr_powerkey);
	if (ret < 0) {
		pr_err("stm_lpm : ERROR %d dev file creation failed\n", ret);
		gpio_free(i2c_data->number_gpio);
	}
	return ret;
}
/**
 * lpm_stop_power_monitor - free resources used for gpio monitor
 * @client_data:	i2c client info
 */

void lpm_stop_power_monitor(struct i2c_client *client_data)
{
	struct stm_lpm_i2c_data *i2c_data = i2c_get_clientdata(client_data);
	device_remove_file(&(client_data->dev), &dev_attr_powerkey);
	gpio_free(i2c_data->number_gpio);
}
