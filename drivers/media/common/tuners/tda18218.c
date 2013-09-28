/*
 *  Driver for NXP TDA18218 silicon tuner
 *
 *  Copyright (C) 2010 Lauris Ding <lding@gmx.de>
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
#include "tda18218.h"
#include "linux/compat.h"
#include "tda18218_priv.h"

static int tda18218_write_reg(struct dvb_frontend *fe, u8 reg, u8 val)
{
	struct tda18218_priv *priv = fe->tuner_priv;
	u8 buf[2] = { reg, val };
	struct i2c_msg msg = { .addr = priv->cfg->i2c_address, .flags = 0,
			       .buf = buf, .len = 2 };
	int ret;

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1);
	/* write register */
	ret = i2c_transfer(priv->i2c, &msg, 1);
	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0);

	if (ret != 1)
		printk(KERN_WARNING "I2C write failed ret: %d reg: %02x\n", ret, reg);

	return (ret == 1 ? 0 : ret);
}

static int tda18218_write_regs(struct dvb_frontend *fe, u8 reg,
	u8 *val, u8 len)
{
	struct tda18218_priv *priv = fe->tuner_priv;
	u8 buf[1+len];
	struct i2c_msg msg = {
		.addr = priv->cfg->i2c_address,
		.flags = 0,
		.len = sizeof(buf),
		.buf = buf };
		
	int ret;

	buf[0] = reg;
	memcpy(&buf[1], val, len);
	
	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1);
	ret = i2c_transfer(priv->i2c, &msg, 1);
	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1);

	if (ret != 1)
		printk(KERN_WARNING "I2C write failed ret: %d reg: %02x len: %d\n", ret, reg, len);
	
	return (ret == 1 ? 0 : ret);
}

/*static int tda18218_read_reg(struct tda18218_priv *priv, u16 reg, u8 *val)
{
	u8 obuf[3] = { reg >> 8, reg & 0xff, 0 };
	u8 ibuf[1];
	struct i2c_msg msg[2] = {
		{
			.addr = 0x3a,
			.flags = 0,
			.len = sizeof(obuf),
			.buf = obuf
		}, {
			.addr = 0x3a,
			.flags = I2C_M_RD,
			.len = sizeof(ibuf),
			.buf = ibuf
		}
	};

	if (i2c_transfer(priv->i2c, msg, 2) != 2) {
		printk(KERN_WARNING "I2C read failed reg:%04x\n", reg);
		return -EREMOTEIO;
	}
	*val = ibuf[0];
	return 0;
}*/

static int tda18218_read_regs(struct dvb_frontend *fe)
{
	struct tda18218_priv *priv = fe->tuner_priv;
	u8 *regs = priv->tda18218_regs;
	u8 buf = 0x00;
	int ret;
	//int i;
	struct i2c_msg msg[] = {
		{ .addr = 0xc0, .flags = 0,
		  .buf = &buf, .len = 1 },
		{ .addr = 0xc0, .flags = I2C_M_RD,
		  .buf = regs, .len = 59 }
	};

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 1);

	/* read all registers */
	ret = i2c_transfer(priv->i2c, msg, 2);

	if (fe->ops.i2c_gate_ctrl)
		fe->ops.i2c_gate_ctrl(fe, 0);

	if (ret != 2)
		printk(KERN_WARNING "I2C read failed ret: %d\n", ret);
	
	/*for(i = 0; i <= 58; i++)
		printk("Register %d: %02x\n", i, 0xff & regs[i]);*/

	return (ret == 2 ? 0 : ret);
}

static int tda18218_set_params(struct dvb_frontend *fe, struct dvb_frontend_parameters *params)
{
	struct tda18218_priv *priv = fe->tuner_priv;
	u8 *regs = priv->tda18218_regs;
	u8 Fc, BP;
	int i, ret;
	u16 if1, bw;
	u32 freq;
	
	u8 paramsbuf[4][6] = {
		{ 0x03, 0x1a },
		{ 0x04, 0x0a },
		{ 0x01, 0x0f },
		{ 0x01, 0x0f },
	};
	
	u8 agcbuf[][2] = {
		{ 0x1a, 0x0e },
		{ 0x20, 0x60 },
		{ 0x23, 0x02 },
		{ 0x20, 0xa0 },
		{ 0x23, 0x09 },
		{ 0x20, 0xe0 },
		{ 0x23, 0x0c },
		{ 0x20, 0x40 },
		{ 0x23, 0x01 },
		{ 0x20, 0x80 },
		{ 0x23, 0x08 },
		{ 0x20, 0xc0 },
		{ 0x23, 0x0b },
		{ 0x24, 0x1c },
		{ 0x24, 0x0c },
	};
	
	switch (params->u.ofdm.bandwidth) {
	case BANDWIDTH_6_MHZ:
		bw = 6000;
		Fc = 0;
		break;
	case BANDWIDTH_7_MHZ:
		bw = 7000;
		Fc = 1;
		break;
	case BANDWIDTH_8_MHZ:
		bw = 8000;
		Fc = 2;
		break;
	default:
		printk(KERN_WARNING "Invalid bandwidth");
		return -EINVAL;
	}
	
	if1 = bw / 2;
	
	if((params->frequency >= 174000000) && (params->frequency < 188000000)) {
		BP = 3;
	}
	else if((params->frequency >= 188000000) && (params->frequency < 253000000)) {
		BP = 4;
	}
	else if((params->frequency >= 253000000) && (params->frequency < 343000000)) {
		BP = 5;
	}
	else if((params->frequency >= 343000000) && (params->frequency <= 870000000)) {
		BP = 6;
	}
	else {
		printk(KERN_WARNING "Frequency out of range");
		return -EINVAL;
	}
	
	freq = params->frequency;
	freq /= 1000;
	freq +=if1;
	freq *= 16;
	
	tda18218_read_regs(fe);
	
	paramsbuf[0][2] = regs[0x1a] | BP;
	paramsbuf[0][3] = regs[0x1b] & ~3;
	paramsbuf[0][3] = regs[0x1b] | Fc;
	paramsbuf[0][4] = regs[0x1c] | 0x0a;
	
	paramsbuf[1][2] = freq >> 16;
	paramsbuf[1][3] = freq >> 8;
	paramsbuf[1][4] = (freq & 0xf0) | (regs[0x0c] & 0x0f);
	paramsbuf[1][5] = 0xff;
	paramsbuf[2][2] = regs[0x0f] | 0x40;
	paramsbuf[3][2] = 0x09;
	
	tda18218_write_reg(fe, 0x04, 0x03);

	for(i = 0; i < ARRAY_SIZE(paramsbuf); i++) {

		/* write registers */
		ret = tda18218_write_regs(fe, paramsbuf[i][1], &paramsbuf[i][2], paramsbuf[i][0]);

		if (ret)
			goto error;
	}
	for(i = 0; i < ARRAY_SIZE(agcbuf); i++) {
		tda18218_write_reg(fe, agcbuf[i][0], agcbuf[i][1]);
	}
	
	//tda18218_write_reg(fe, 0x03, 0x00);
	//tda18218_write_reg(fe, 0x04, 0x00);
	//tda18218_write_reg(fe, 0x20, 0xc7);
	
	msleep(60);
	i = 0;
	while(i < 10) {
		tda18218_read_regs(fe);
		if((regs[0x01] & 0x60) == 0x60)
			printk(KERN_INFO "We've got a lock!"); break;
		msleep(20);
		i++;
	}
	
	priv->bandwidth = params->u.ofdm.bandwidth;
	priv->frequency = params->frequency;
	return 0;
error:
	return ret;
}

static int tda18218_get_frequency(struct dvb_frontend *fe, u32 *frequency)
{
	struct tda18218_priv *priv = fe->tuner_priv;
	*frequency = priv->frequency;
	return 0;
}

static int tda18218_get_bandwidth(struct dvb_frontend *fe, u32 *bandwidth)
{
	struct tda18218_priv *priv = fe->tuner_priv;
	*bandwidth = priv->bandwidth;
	return 0;
}

static int tda18218_init(struct dvb_frontend *fe)
{
	//struct tda18218_priv *priv = fe->tuner_priv;
	//u8 *regs = priv->tda18218_regs;
	int i;
	int ret;
	
	u8 initbuf[][18] = {
		{ 0x10, 0x05, 0x00, 0x00, 0xd0, 0x00, 0x40, 0x00, 0x00, 0x07, 0xff, 0x84, 0x09, 0x00, 0x13, 0x00, 0x00, 0x01 },
		{ 0x0b, 0x15, 0x84, 0x09, 0xf0, 0x19, 0x0a, 0x0e, 0x29, 0x98, 0x00, 0x00, 0x58 },
		{ 0x10, 0x24, 0x0c, 0x48, 0x85, 0xc9, 0xa7, 0x00, 0x00, 0x00, 0x30, 0x81, 0x80, 0x00, 0x39, 0x00, 0x8a, 0x00 },
		{ 0x07, 0x34, 0x00, 0x00, 0x00, 0x00, 0x00, 0xf6, 0xf6 },
	};
	
	u8 initbuf2[4];

	for(i = 0; i < ARRAY_SIZE(initbuf); i++) {
		
		/* write registers */
		ret = tda18218_write_regs(fe, initbuf[i][1], &initbuf[i][2], initbuf[i][0]);

		if (ret != 0) {
			printk(KERN_ERR "init: ERROR: i2c_transfer returned: %d\n", ret);
			return -EREMOTEIO;
		}
		if(i == 1) {
			tda18218_write_reg(fe, 0x22, 0x8c);
		}
	}
	
	tda18218_write_reg(fe, 0x05, 0x80);
	tda18218_write_reg(fe, 0x05, 0x00);
	tda18218_write_reg(fe, 0x05, 0x20);
	tda18218_write_reg(fe, 0x05, 0x00);
	tda18218_write_reg(fe, 0x27, 0xde);
	tda18218_write_reg(fe, 0x17, 0xf8);
	tda18218_write_reg(fe, 0x18, 0x0f);
	tda18218_write_reg(fe, 0x1c, 0x8b);
	tda18218_write_reg(fe, 0x29, 0x02);
	tda18218_write_reg(fe, 0x19, 0x1a);
	tda18218_write_reg(fe, 0x11, 0x13);

	initbuf2[0] = 0x0a;
	initbuf2[1] = 0x5c;
	initbuf2[2] = 0xc6;
	initbuf2[3] = 0x07;
	tda18218_write_regs(fe, initbuf2[0], &initbuf2[1], 3);
	tda18218_write_reg(fe, 0x0f, 0x49);
	tda18218_write_reg(fe, 0x05, 0x40);
	tda18218_write_reg(fe, 0x05, 0x00);
	tda18218_write_reg(fe, 0x05, 0x20);
	tda18218_write_reg(fe, 0x11, 0xed);
	tda18218_write_reg(fe, 0x0f, 0x49);
	tda18218_write_reg(fe, 0x19, 0x2a);
	tda18218_write_reg(fe, 0x05, 0x58);
	tda18218_write_reg(fe, 0x05, 0x18);
	tda18218_write_reg(fe, 0x05, 0x38);
	tda18218_write_reg(fe, 0x29, 0x03);
	tda18218_write_reg(fe, 0x19, 0x1a);
	tda18218_write_reg(fe, 0x11, 0x13);
	initbuf2[0] = 0x0a;
	initbuf2[1] = 0xbe;
	initbuf2[2] = 0x6e;
	initbuf2[3] = 0x07;
	tda18218_write_regs(fe, initbuf2[0], &initbuf2[1], 3);
	tda18218_write_reg(fe, 0x0f, 0x49);
	tda18218_write_reg(fe, 0x05, 0x58);
	tda18218_write_reg(fe, 0x05, 0x18);
	tda18218_write_reg(fe, 0x05, 0x38);
	tda18218_write_reg(fe, 0x11, 0xed);
	tda18218_write_reg(fe, 0x0f, 0x49);
	tda18218_write_reg(fe, 0x19, 0x2a);
	tda18218_write_reg(fe, 0x05, 0x58);
	tda18218_write_reg(fe, 0x05, 0x18);
	tda18218_write_reg(fe, 0x05, 0x38);
	tda18218_write_reg(fe, 0x19, 0x0a);
	tda18218_write_reg(fe, 0x27, 0xc9);
	tda18218_write_reg(fe, 0x11, 0x13);
	initbuf2[0] = 0x17;
	initbuf2[1] = 0xf0;
	initbuf2[2] = 0x19;
	initbuf2[3] = 0x00;
	tda18218_write_regs(fe, initbuf2[0], &initbuf2[1], 2);
	tda18218_write_reg(fe, 0x1c, 0x98);
	tda18218_write_reg(fe, 0x29, 0x03);
	tda18218_write_reg(fe, 0x2a, 0x00);
	tda18218_write_reg(fe, 0x2a, 0x01);
	tda18218_write_reg(fe, 0x2a, 0x02);
	tda18218_write_reg(fe, 0x2a, 0x03);
	tda18218_write_reg(fe, 0x1c, 0x98);
	tda18218_write_reg(fe, 0x18, 0x19);
	tda18218_write_reg(fe, 0x22, 0x9c);
	tda18218_write_reg(fe, 0x1f, 0x58);
	tda18218_write_reg(fe, 0x24, 0x0c);
	tda18218_write_reg(fe, 0x1c, 0x88);
	tda18218_write_reg(fe, 0x20, 0x10);
	tda18218_write_reg(fe, 0x21, 0x4c);
	tda18218_write_reg(fe, 0x20, 0x00);
	tda18218_write_reg(fe, 0x21, 0x48);
	tda18218_write_reg(fe, 0x1f, 0x5b);
	tda18218_write_reg(fe, 0x20, 0x00);
	tda18218_write_reg(fe, 0x1f, 0x59);
	tda18218_write_reg(fe, 0x20, 0x00);
	tda18218_write_reg(fe, 0x1f, 0x5a);
	tda18218_write_reg(fe, 0x20, 0x00);
	tda18218_write_reg(fe, 0x1f, 0x5f);
	tda18218_write_reg(fe, 0x20, 0x00);
	tda18218_write_reg(fe, 0x1f, 0x5d);
	tda18218_write_reg(fe, 0x20, 0x00);
	tda18218_write_reg(fe, 0x1f, 0x5e);
	tda18218_write_reg(fe, 0x20, 0x00);
	tda18218_write_reg(fe, 0x20, 0x60);
	tda18218_write_reg(fe, 0x23, 0x02);
	tda18218_write_reg(fe, 0x20, 0xa0);
	tda18218_write_reg(fe, 0x23, 0x09);
	tda18218_write_reg(fe, 0x20, 0xe0);
	tda18218_write_reg(fe, 0x23, 0x0c);
	tda18218_write_reg(fe, 0x20, 0x40);
	tda18218_write_reg(fe, 0x23, 0x01);
	tda18218_write_reg(fe, 0x20, 0x80);
	tda18218_write_reg(fe, 0x23, 0x08);
	tda18218_write_reg(fe, 0x20, 0xc0);
	tda18218_write_reg(fe, 0x23, 0x0b);
	tda18218_write_reg(fe, 0x1c, 0x98);
	tda18218_write_reg(fe, 0x22, 0x8c);
	initbuf2[0] = 0x17;
	initbuf2[1] = 0xb0;
	initbuf2[2] = 0x59;
	initbuf2[3] = 0x00;
	//tda18218_write_regs(fe, initbuf2[0], &initbuf2[1], 2);
	initbuf2[0] = 0x1a;
	initbuf2[1] = 0x0e;
	initbuf2[2] = 0x2a;
	initbuf2[3] = 0x98;
	tda18218_write_regs(fe, initbuf2[0], &initbuf2[1], 3);
	initbuf2[0] = 0x17;
	initbuf2[1] = 0xb0;
	initbuf2[2] = 0x59;
	initbuf2[3] = 0x00;
	tda18218_write_regs(fe, initbuf2[0], &initbuf2[1], 2);
	tda18218_write_reg(fe, 0x2d, 0x81);
	tda18218_write_reg(fe, 0x29, 0x02);
	
	return 0;
}
	
static int tda18218_release(struct dvb_frontend *fe)
{
	kfree(fe->tuner_priv);
	fe->tuner_priv = NULL;
	return 0;
}

static const struct dvb_tuner_ops tda18218_tuner_ops = {
	.info = {
		.name           = "NXP TDA18218",
		.frequency_min  = TDA18218_MIN_FREQ,
		.frequency_max  = TDA18218_MAX_FREQ,
		.frequency_step = TDA18218_STEP,
	},

	.release       = tda18218_release,
	.init          = tda18218_init,
	
	.set_params = tda18218_set_params,
	.get_frequency = tda18218_get_frequency,
	.get_bandwidth = tda18218_get_bandwidth,
};

struct dvb_frontend * tda18218_attach(struct dvb_frontend *fe,
				    struct i2c_adapter *i2c,
				    struct tda18218_config *cfg)
{
	struct tda18218_priv *priv = NULL;

	priv = kzalloc(sizeof(struct tda18218_priv), GFP_KERNEL);
	if (priv == NULL)
		return NULL;

	priv->cfg = cfg;
	priv->i2c = i2c;

	fe->tuner_priv = priv;
	
	tda18218_read_regs(fe);
	if (priv->tda18218_regs[0x00] != 0xc0) {
		printk(KERN_WARNING "Device is not a TDA18218!\n");
		kfree(priv);
		return NULL;
	}
	
	printk(KERN_INFO "NXP TDA18218 successfully identified.\n");
	memcpy(&fe->ops.tuner_ops, &tda18218_tuner_ops,
	       sizeof(struct dvb_tuner_ops));
	
	return fe;
}
EXPORT_SYMBOL(tda18218_attach);

MODULE_DESCRIPTION("NXP TDA18218 silicon tuner driver");
MODULE_AUTHOR("Lauris Ding <lding@gmx.de>");
MODULE_VERSION("0.1");
MODULE_LICENSE("GPL");
