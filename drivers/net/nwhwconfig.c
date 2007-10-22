/*
 * Configuration of network device hardware from the kernel command line.
 *
 * Copyright (c) STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 */

#include <linux/types.h>
#include <linux/string.h>
#include <linux/kernel.h>
#include <linux/jiffies.h>
#include <linux/random.h>
#include <linux/init.h>
#include <linux/utsname.h>
#include <linux/in.h>
#include <linux/if.h>
#include <linux/inet.h>
#include <linux/netdevice.h>
#include <linux/if_arp.h>
#include <linux/skbuff.h>
#include <linux/ip.h>
#include <linux/socket.h>
#include <linux/route.h>
#include <linux/udp.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/major.h>
#include <linux/root_dev.h>
#include <linux/ethtool.h>
#include <net/arp.h>
#include <net/ip.h>
#include <net/ipconfig.h>

#include <asm/uaccess.h>
#include <net/checksum.h>
#include <asm/processor.h>

static char user_dev_name[IFNAMSIZ] __initdata = { 0, };
static char user_hw_addr[18] __initdata = { 0, };
static char user_speed = -1;
static char user_duplex = -1;

static int __init hex_conv_nibble(char x)
{
        if ((x >= '0') && (x <= '9'))
                return x - '0';
        if ((x >= 'a') && (x <= 'f'))
                return x - 'a' + 10;
        if ((x >= 'A') && (x <= 'F'))
                return x - 'A' + 10;

        return -1;
}

static int __init parse_ether(const char *mac_addr_str, struct sockaddr *addr)
{
        int i, c1, c2;
	char* mac_addr = addr->sa_data;

        /*
         * Pull out 6 two-digit hex chars
         */
        for (i = 0; i < 6; i++) {

                c1 = hex_conv_nibble(*mac_addr_str++);
                c2 = hex_conv_nibble(*mac_addr_str++);

                if ((c1 == -1) || (c2 == -1))
                        return 0;

                mac_addr[i] = (c1 << 4) | c2;

                if ((i != 5) && (*mac_addr_str++ != ':'))
                        return 0;
        }

        return 1;
}


static int __init nwhw_config(void)
{
	struct net_device *dev;
	struct sockaddr ether_addr;
	int valid_ether = user_hw_addr[0];

	if (valid_ether) {
		valid_ether = parse_ether(user_hw_addr, &ether_addr);
		if (! valid_ether) {
			printk("Failed to parse ether addr: %s\n", user_hw_addr);
		}
	}

        dev = __dev_get_by_name(user_dev_name);
	if (! dev) {
		printk("%s: device not found\n", __FUNCTION__);
		return -1;
	}

	if (valid_ether) {
		if (!dev->set_mac_address ||
		    dev->set_mac_address(dev, &ether_addr)) {
			printk(KERN_WARNING "%s: not set MAC address...\n",
				__FUNCTION__);
		}
	}

	if ((user_speed != -1) || (user_duplex != -1)) {
		struct ethtool_cmd cmd = { ETHTOOL_GSET };

		if (! dev->ethtool_ops->get_settings ||
		    (dev->ethtool_ops->get_settings(dev, &cmd) < 0)) {
			printk("Failed to read ether device settings\n");
		} else {
			cmd.cmd = ETHTOOL_SSET;
			cmd.autoneg = AUTONEG_DISABLE;
			if (user_speed != -1)
				cmd.speed = user_speed;
			if (user_duplex != -1)
				cmd.duplex = user_duplex;
			if (! dev->ethtool_ops->set_settings ||
			    (dev->ethtool_ops->set_settings(dev, &cmd) < 0)) {
				printk("Failed to set ether device settings\n");
			}
		}
	}

	return 0;
}

device_initcall(nwhw_config);

#if defined (CONFIG_NETPOLL)
void nwhw_uconfig(struct net_device *dev)
{
	struct sockaddr ether_addr;
	int valid_ether = user_hw_addr[0];

	if (valid_ether) {
		valid_ether = parse_ether(user_hw_addr, &ether_addr);
		if (! valid_ether) {
			printk("%s: failed to parse ether addr: %s\n",
			__FUNCTION__, user_hw_addr);
			return;
		}
	}
	if (!dev->set_mac_address || dev->set_mac_address(dev, &ether_addr)) {
		printk(KERN_WARNING "%s: not set MAC address\n", __FUNCTION__);
		return;
	}
}
#endif

static int __init nwhw_config_setup(char* str)
{
	char* opt;

	if (!str || !*str)
		return 0;

	while ((opt=strsep(&str, ",")) != NULL) {
		if (! strncmp(opt, "device:", 7)) {
			strlcpy(user_dev_name, opt+7, sizeof(user_dev_name));
		}
		else if (! strncmp(opt, "hwaddr:", 7)) {
			strlcpy(user_hw_addr, opt+7, sizeof(user_hw_addr));
		}
		else if (! strncmp(opt, "speed:", 6)) {
			switch (simple_strtoul(opt+6, NULL, 0)) {
			case 10:
				user_speed = SPEED_10;
				break;
			case 100:
				user_speed = SPEED_100;
				break;
			}
		}
		else if (! strcmp(opt, "duplex:full")) {
			user_duplex = DUPLEX_FULL;
		}
		else if (! strcmp(opt, "duplex:half")) {
			user_duplex = DUPLEX_HALF;
		}
	}

	return 1;
}

__setup("nwhwconf=", nwhw_config_setup);
