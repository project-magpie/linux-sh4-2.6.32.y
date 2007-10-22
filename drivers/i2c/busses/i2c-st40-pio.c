/* linux/drivers/i2c/busses/i2c-st40-pio.c

   Copyright (c) 2004 STMicroelectronics Limited
   Author: Stuart Menefy <stuart.menefy@st.com>

   ST40 I2C bus driver using PIO pins

   Derived from i2c-velleman.c which was:
   Copyright (C) 1995-96, 2000 Simon G. Vogl

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/errno.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/i2c-algo-bit.h>
#include <linux/stm/pio.h>
#include <asm/io.h>
#include <asm/param.h> /* for HZ */

#define NAME "i2c_st40_pio"

typedef struct {
	int sclbank;
	int sclpin;
	int sdabank;
	int sdapin;
} pio_address;

typedef struct {
	struct stpio_pin* scl;
	struct stpio_pin* sda;
} pio_pins;


#if defined(CONFIG_CPU_SUBTYPE_STI5528)

#define NR_I2C_BUSSES 1
static pio_address i2c_address[NR_I2C_BUSSES] = {{3,1,3,0}};

#elif defined(CONFIG_CPU_SUBTYPE_STM8000)

#if defined(CONFIG_SH_STM8000_DEMO)
#define NR_I2C_BUSSES 2
static pio_address i2c_address[NR_I2C_BUSSES] = {
	{6,0,6,1},
	{6,2,6,3}  // This isn't strictly speaking I2C but some boards use it as such
};
#elif defined(CONFIG_SH_ST220_EVAL)
#define NR_I2C_BUSSES 1
static pio_address i2c_address[NR_I2C_BUSSES] = {
	{6,0,6,1}
	//  The "second" bus on the eval board is unconnected and hence floating
	//  this causes a temporary hang on probe
};
#endif

#elif defined(CONFIG_CPU_SUBTYPE_STB7100)

#define NR_I2C_BUSSES 3
static pio_address i2c_address[NR_I2C_BUSSES] = {
	{2,0,2,1},
	{3,0,3,1},
	{4,0,4,1}
};

#else
#error Need to configure the default I2C pins for this chip
#endif

static pio_pins i2c_busses[NR_I2C_BUSSES] = {{0}};

static void bit_st40_pio_setscl(void *data, int state)
{
	stpio_set_pin(((pio_pins*)data)->scl, state);
}

static void bit_st40_pio_setsda(void *data, int state)
{
	stpio_set_pin(((pio_pins*)data)->sda, state);
}

static int bit_st40_pio_getscl(void *data)
{
	return stpio_get_pin(((pio_pins*)data)->scl);
}

static int bit_st40_pio_getsda(void *data)
{
	return stpio_get_pin(((pio_pins*)data)->sda);
}

static int bit_st40_pio_init(void)
{
	int i;
	for(i = 0; i<NR_I2C_BUSSES; i++) {
		i2c_busses[i].scl = stpio_request_pin(i2c_address[i].sclbank,
						      i2c_address[i].sclpin,
						      "I2C Clock",
						      STPIO_BIDIR);

		printk(KERN_INFO NAME ": allocated pin (%d,%d) for scl (0x%p)\n",i2c_address[i].sclbank, i2c_address[i].sclpin, i2c_busses[i].scl);

		i2c_busses[i].sda = stpio_request_pin(i2c_address[i].sdabank,
						      i2c_address[i].sdapin,
						      "I2C Data",
						      STPIO_BIDIR);

		printk(KERN_INFO NAME ": allocated pin (%d,%d) for sda (0x%p)\n",i2c_address[i].sdabank, i2c_address[i].sdapin, i2c_busses[i].sda);

		if(i2c_busses[i].scl == NULL || i2c_busses[i].sda == NULL)
		{
			printk(KERN_INFO NAME ": failed to allocate bus pins\n");
			return -1;
		}


		stpio_set_pin(i2c_busses[i].sda, 0);
		stpio_set_pin(i2c_busses[i].scl, 0);
	}

	return 0;
}

static void bit_st40_pio_free(void)
{
	int i;
	for(i=0; i<NR_I2C_BUSSES; i++) {
		if(i2c_busses[i].scl)
		{
			stpio_free_pin(i2c_busses[i].scl);
			i2c_busses[i].scl = NULL;
		}

		if(i2c_busses[i].sda)
		{
			stpio_free_pin(i2c_busses[i].sda);
			i2c_busses[i].sda = NULL;
		}
	}
}

static struct i2c_algo_bit_data bit_st40_pio_data[NR_I2C_BUSSES] = {
{
	.data		= &i2c_busses[0],
	.setsda		= bit_st40_pio_setsda,
	.setscl		= bit_st40_pio_setscl,
	.getsda		= bit_st40_pio_getsda,
	.getscl		= bit_st40_pio_getscl,
	.udelay		= 10,
	.timeout	= HZ
},
#if NR_I2C_BUSSES > 1
{
	.data		= &i2c_busses[1],
	.setsda		= bit_st40_pio_setsda,
	.setscl		= bit_st40_pio_setscl,
	.getsda		= bit_st40_pio_getsda,
	.getscl		= bit_st40_pio_getscl,
	.udelay		= 10,
	.timeout	= HZ
},
#if NR_I2C_BUSSES > 2
{
        .data           = &i2c_busses[2],
        .setsda         = bit_st40_pio_setsda,
        .setscl         = bit_st40_pio_setscl,
        .getsda         = bit_st40_pio_getsda,
        .getscl         = bit_st40_pio_getscl,
        .udelay         = 10,
        .timeout        = HZ
},
#endif
#endif
};

static struct i2c_adapter bit_st40_pio_ops[NR_I2C_BUSSES] = {
{
	.owner		= THIS_MODULE,
	.name		= "ST40 (PIO based)",
	.id		= I2C_HW_B_ST40_PIO,
	.algo_data	= &bit_st40_pio_data[0],
},
#if NR_I2C_BUSSES > 1
{
	.owner		= THIS_MODULE,
	.name		= "ST40 (PIO based)",
	.id		= I2C_HW_B_ST40_PIO,
	.algo_data	= &bit_st40_pio_data[1],
},
#if NR_I2C_BUSSES > 2
{
	.owner		= THIS_MODULE,
	.name		= "ST40 (PIO based)",
	.id		= I2C_HW_B_ST40_PIO,
	.algo_data	= &bit_st40_pio_data[2],
}
#endif
#endif
};

static void bit_st40_pio_unregister(void)
{
	int i;

	for(i=0;i<NR_I2C_BUSSES;i++)
	{
		i2c_del_adapter(&bit_st40_pio_ops[i]);
	}

	bit_st40_pio_free();
}

static int __init i2c_st40_pio_init(void)
{
	int i;

	printk(KERN_INFO NAME ": ST40 PIO based I2C Driver\n");

	if (bit_st40_pio_init() < 0) {
		printk(KERN_INFO NAME ": initialization failed\n");
		bit_st40_pio_free();
	}

	for(i=0;i<NR_I2C_BUSSES;i++)
	{
		printk(KERN_INFO NAME " bus %d: SCL=PIO%u[%u], SDA=PIO%u[%u]\n", i,
			i2c_address[i].sclbank, i2c_address[i].sclpin,
			i2c_address[i].sdabank, i2c_address[i].sdapin);

		if (i2c_bit_add_bus(&bit_st40_pio_ops[i]) < 0) {
			printk(KERN_ERR NAME ": adapter registration failed\n");
			bit_st40_pio_unregister();
			return -ENODEV;
		}
	}
	return 0;
}

static void __exit i2c_st40_pio_exit(void)
{
	bit_st40_pio_unregister();
}

MODULE_AUTHOR("Stuart Menefy <stuart.menefy@st.com>");
MODULE_DESCRIPTION("ST40 PIO based I2C Driver");
MODULE_LICENSE("GPL");

module_init(i2c_st40_pio_init);
module_exit(i2c_st40_pio_exit);

