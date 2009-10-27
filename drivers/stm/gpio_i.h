/*
 * drivers/stm/gpio_i.h
 *
 * (c) 2009 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * Internal defintions to allow sharing between gpio.c and pio10.c
 */

struct stm_gpio_port;

void stm_gpio_irq_handler(const struct stm_gpio_port *port);

struct stm_gpio_port *stm_gpio_irq_init(int port_no, const char* name);
