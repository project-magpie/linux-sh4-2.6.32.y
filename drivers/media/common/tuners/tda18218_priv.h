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

#ifndef TDA18218_PRIV_H
#define TDA18218_PRIV_H

#define TDA18218_STEP         1000 /* 1 kHz */
#define TDA18218_MIN_FREQ   174000000 /*   174 MHz */
#define TDA18218_MAX_FREQ  864000000 /*  864 MHz */

struct tda18218_priv {
	u8 tda18218_regs[0x3b];
	struct tda18218_config *cfg;
	struct i2c_adapter *i2c;

	u32 frequency;
	u32 bandwidth;
};

#endif
