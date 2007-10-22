/*
	Copyright (C) 2004 - 2007 rt2x00 SourceForge Project
	<http://rt2x00.serialmonkey.com>

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the
	Free Software Foundation, Inc.,
	59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

/*
	Module: rt2x00debug
	Abstract: Data structures for the rt2x00debug module.
	Supported chipsets: RT2460, RT2560, RT2570,
	rt2561, rt2561s, rt2661 & rt2573.
 */

typedef void (debug_access_t)(void *dev, const unsigned long word, void *data);

struct rt2x00debug_reg {
	debug_access_t *read;
	debug_access_t *write;

	unsigned int word_size;
	unsigned int length;
};

struct rt2x00debug {
	/*
	 * Name of the interface.
	 */
	char intf_name[16];

	/*
	 * Reference to the modules structure.
	 */
	struct module *owner;

	/*
	 * Driver module information
	 */
	char *mod_name;
	char *mod_version;

	/*
	 * Register access information.
	 */
	struct rt2x00debug_reg reg_csr;
	struct rt2x00debug_reg reg_eeprom;
	struct rt2x00debug_reg reg_bbp;

	/*
	 * Pointer to driver structure where
	 * this debugfs entry belongs to.
	 */
	void *dev;

	/*
	 * Pointer to rt2x00debug private data,
	 * individual driver should not touch this.
	 */
	void *priv;
};

extern int rt2x00debug_register(struct rt2x00debug *debug);
extern void rt2x00debug_deregister(struct rt2x00debug *debug);
