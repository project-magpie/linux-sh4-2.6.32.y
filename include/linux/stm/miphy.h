/*
 * (c) 2010 STMicroelectronics Limited
 *
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#ifndef __LINUX_STM_MIPHY_H
#define __LINUX_STM_MIPHY_H

enum miphy_if_type { TAP_IF, UPORT_IF };
enum miphy_mode { SATA_MODE, PCIE_MODE };

struct miphy_device {
	struct list_head ifaces; /* list of registered devices */
	struct semaphore mutex;
	int user_count;
	int state;
};

struct stm_miphy {
	struct miphy_device *dev;
	int port;
	enum miphy_mode mode;
	int interface;
	int (*start)(struct stm_miphy *miphy);
	int (*sata_status)(struct stm_miphy *miphy);
	void (*assert_deserializer)(struct stm_miphy *miphy, int assert);
};

struct miphy_if_ops {
	void (*reg_write)(int port, u8 addr, u8 data);
	u8 (*reg_read)(int port, u8 addr);
};

struct miphy_if{
	struct list_head list;
	struct miphy_if_ops *ops;
	enum miphy_if_type	type;
	int (*start_sata)(int port, struct miphy_if *iface);
	int (*start_pcie)(int port, struct miphy_if *iface);
	void *data;
};

/************************FOR register r/w Interface Drivers ***************/
/* MiPHY register Read/Write interface un-registration */
int miphy_if_unregister(struct miphy_device *dev, enum miphy_if_type type);

/* MiPHY register Read/Write interface registration */
struct miphy_device *miphy_if_register(enum miphy_if_type type,
			void *if_data, struct miphy_if_ops *ops);
/******************End of API's For Register r/w interface drivers *******/

/*
 * MiPHY is initialised depending on the parameters provided in miphy pointer.
 * miphy pointer will be updated with other information if initialisation is
 * successfull.
 */
int stm_miphy_init(struct stm_miphy *miphy);

#endif
