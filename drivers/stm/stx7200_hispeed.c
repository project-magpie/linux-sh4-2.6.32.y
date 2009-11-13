#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/ethtool.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/stx7200.h>
#include <asm/irq-ilc.h>



/* USB resources ---------------------------------------------------------- */

static u64 stx7200_usb_dma_mask = DMA_32BIT_MASK;

static struct stm_plat_usb_data stx7200_usb_platform_data[] = {
	[0] = {
		.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT |
				STM_PLAT_USB_FLAGS_STRAP_PLL |
				STM_PLAT_USB_FLAGS_OPC_MSGSIZE_CHUNKSIZE,
		.pad_config = &(struct stm_pad_config) {
			.sysconf_values_num = 3,
			.sysconf_values = (struct stm_pad_sysconf_value []) {
				/* route USB and parts of MAFE instead of DVO:
				 * CONF_PAD_PIO[2] = 0 */
				STM_PAD_SYS_CFG(7, 26, 26, 0),
				/* DVO output selection (probably ignored):
				 * CONF_PAD_PIO[3] = 0 */
				STM_PAD_SYS_CFG(7, 27, 27, 0),
				/* Power up port */
				STM_PAD_SYS_CFG(22, 3, 3, 0),
			},
			.gpio_values_num = 2,
			.gpio_values = (struct stm_pad_gpio_value []) {
				/* Overcurrent detection, must be ALT_BIDIR
				 * for cut 1 - see stx7200_configure_usb() */
				STM_PAD_PIO_IN(7, 0),
				/* USB power enable */
				STM_PAD_PIO_ALT_OUT(7, 1),
			},
		},
	},
	[1] = {
		.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT |
				STM_PLAT_USB_FLAGS_STRAP_PLL |
				STM_PLAT_USB_FLAGS_OPC_MSGSIZE_CHUNKSIZE,
		.pad_config = &(struct stm_pad_config) {
			.sysconf_values_num = 3,
			.sysconf_values = (struct stm_pad_sysconf_value []) {
				/* route USB and parts of MAFE instead of DVO:
				 * CONF_PAD_PIO[2] = 0 */
				STM_PAD_SYS_CFG(7, 26, 26, 0),
				/* DVO output selection (probably ignored):
				 * CONF_PAD_PIO[3] = 0 */
				STM_PAD_SYS_CFG(7, 27, 27, 0),
				/* Power up port */
				STM_PAD_SYS_CFG(22, 4, 4, 0),
			},
			.gpio_values_num = 2,
			.gpio_values = (struct stm_pad_gpio_value []) {
				/* Overcurrent detection, must be ALT_BIDIR
				 * for cut 1 - see stx7200_configure_usb() */
				STM_PAD_PIO_IN(7, 2),
				/* USB power enable */
				STM_PAD_PIO_ALT_OUT(7, 3),
			},
		},
	},
	[2] = {
		.flags = STM_PLAT_USB_FLAGS_STRAP_8BIT |
				STM_PLAT_USB_FLAGS_STRAP_PLL |
				STM_PLAT_USB_FLAGS_OPC_MSGSIZE_CHUNKSIZE,
		.pad_config = &(struct stm_pad_config) {
			.sysconf_values_num = 3,
			.sysconf_values = (struct stm_pad_sysconf_value []) {
				/* route USB and parts of MAFE instead of DVO:
				 * CONF_PAD_PIO[2] = 0 */
				STM_PAD_SYS_CFG(7, 26, 26, 0),
				/* DVO output selection (probably ignored):
				 * CONF_PAD_PIO[3] = 0 */
				STM_PAD_SYS_CFG(7, 27, 27, 0),
				/* Power up port */
				STM_PAD_SYS_CFG(22, 5, 5, 0),
			},
			.gpio_values_num = 2,
			.gpio_values = (struct stm_pad_gpio_value []) {
				/* USB power enable */
				STM_PAD_PIO_ALT_OUT(7, 4),
				/* Overcurrent detection, must be ALT_BIDIR
				 * for cut 1 - see stx7200_configure_usb() */
				STM_PAD_PIO_IN(7, 5),
			},
		},
	},
};

static struct platform_device stx7200_usb_devices[] = {
	[0] = {
		.name = "stm-usb",
		.id = 0,
		.dev = {
			.dma_mask = &stx7200_usb_dma_mask,
			.coherent_dma_mask = DMA_32BIT_MASK,
			.platform_data = &stx7200_usb_platform_data[0],
		},
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM_NAMED("ehci", 0xfd2ffe00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ehci", ILC_IRQ(80), -1),
			STM_PLAT_RESOURCE_MEM_NAMED("ohci", 0xfd2ffc00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ohci", ILC_IRQ(81), -1),
			STM_PLAT_RESOURCE_MEM_NAMED("wrapper", 0xfd200000,
						    0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("protocol", 0xfd2fff00,
						    0x100),
		},
	},
	[1] = {
		.name = "stm-usb",
		.id = 1,
		.dev = {
			.dma_mask = &stx7200_usb_dma_mask,
			.coherent_dma_mask = DMA_32BIT_MASK,
			.platform_data = &stx7200_usb_platform_data[1],
		},
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM_NAMED("ehci", 0xfd3ffe00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ehci", ILC_IRQ(82), -1),
			STM_PLAT_RESOURCE_MEM_NAMED("ohci", 0xfd3ffc00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ohci", ILC_IRQ(83), -1),
			STM_PLAT_RESOURCE_MEM_NAMED("wrapper", 0xfd300000,
						    0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("protocol", 0xfd3fff00,
						    0x100),
		},
	},
	[2] = {
		.name = "stm-usb",
		.id = 2,
		.dev = {
			.dma_mask = &stx7200_usb_dma_mask,
			.coherent_dma_mask = DMA_32BIT_MASK,
			.platform_data = &stx7200_usb_platform_data[2],
		},
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM_NAMED("ehci", 0xfd4ffe00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ehci", ILC_IRQ(84), -1),
			STM_PLAT_RESOURCE_MEM_NAMED("ohci", 0xfd4ffc00, 0x100),
			STM_PLAT_RESOURCE_IRQ_NAMED("ohci", ILC_IRQ(85), -1),
			STM_PLAT_RESOURCE_MEM_NAMED("wrapper", 0xfd400000,
						    0x100),
			STM_PLAT_RESOURCE_MEM_NAMED("protocol", 0xfd4fff00,
						    0x100),
		},
	},
};

/* Workaround for USB problems on 7200 cut 1;
 * alternative to RC delay on board */
static int __init stx7200_usb_soft_jtag_reset(struct stm_pad_config *config,
		void *priv)
{
	int i, j;
	struct sysconf_field *sc;
	static int done;

	if (done)
		return 0;
	done = 1;

	/* Enable soft JTAG mode for USB and SATA
	 * soft_jtag_en = 1 */
	sc = sysconf_claim(SYS_CFG, 33, 6, 6, "usb");
	sysconf_write(sc, 1);
	/* tck = tdi = trstn_usb = tms_usb = 0 */
	sc = sysconf_claim(SYS_CFG, 33, 0, 3, "usb");
	sysconf_write(sc, 0);

	sc = sysconf_claim(SYS_CFG, 33, 0, 6, "usb");

	/* ENABLE SOFT JTAG */
	sysconf_write(sc, 0x00000040);

	/* RELEASE TAP RESET */
	sysconf_write(sc, 0x00000044);

	/* SET TAP INTO IDLE STATE */
	sysconf_write(sc, 0x00000045);

	/* SET TAP INTO SHIFT IR STATE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SHIFT DATA IN TDI = 101 select TCB*/
	sysconf_write(sc, 0x00000046);
	sysconf_write(sc, 0x00000047);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x0000004E);
	sysconf_write(sc, 0x0000004F);

	/* SET TAP INTO IDLE MODE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SET TAP INTO SHIFT DR STATE*/
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SHIFT DATA IN TCB */
	for (i = 0; i <= 53; i++) {
		if ((i == 0) || (i == 1) || (i == 19) || (i == 36)) {
			sysconf_write(sc, 0x00000044);
			sysconf_write(sc, 0x00000045);
		}

		if ((i == 53)) {
			sysconf_write(sc, 0x0000004c);
			sysconf_write(sc, 0x0000004D);
		}
		sysconf_write(sc, 0x00000044);
		sysconf_write(sc, 0x00000045);
	}

	/* SET TAP INTO IDLE MODE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	for (i = 0; i <= 53; i++) {
		sysconf_write(sc, 0x00000045);
		sysconf_write(sc, 0x00000044);
	}

	sysconf_write(sc, 0x00000040);

	/* RELEASE TAP RESET */
	sysconf_write(sc, 0x00000044);

	/* SET TAP INTO IDLE STATE */
	sysconf_write(sc, 0x00000045);

	/* SET TAP INTO SHIFT IR STATE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SHIFT DATA IN TDI = 110 select TPR*/
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000046);
	sysconf_write(sc, 0x00000047);
	sysconf_write(sc, 0x0000004E);
	sysconf_write(sc, 0x0000004F);

	/* SET TAP INTO IDLE MODE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SET TAP INTO SHIFT DR STATE*/
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SHIFT DATA IN TDO */
	for (i = 0; i <= 366; i++) {
		sysconf_write(sc, 0x00000044);
		sysconf_write(sc, 0x00000045);
	}

	for (j = 0; j < 2; j++) {
		for (i = 0; i <= 365; i++) {
			if ((i == 71) || (i == 192) || (i == 313)) {
				sysconf_write(sc, 0x00000044);
				sysconf_write(sc, 0x00000045);
			}
			sysconf_write(sc, 0x00000044);
			sysconf_write(sc, 0x00000045);

			if ((i == 365))	{
				sysconf_write(sc, 0x0000004c);
				sysconf_write(sc, 0x0000004d);
			}
		}
	}

	for (i = 0; i <= 366; i++) {
		sysconf_write(sc, 0x00000045);
		sysconf_write(sc, 0x00000044);
	}

	/* SET TAP INTO IDLE MODE */
	sysconf_write(sc, 0x0000004C);
	sysconf_write(sc, 0x0000004D);
	sysconf_write(sc, 0x0000004C);
	sysconf_write(sc, 0x0000004D);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SET TAP INTO SHIFT IR STATE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SHIFT DATA IN TDI = 101 select TCB */
	sysconf_write(sc, 0x00000046);
	sysconf_write(sc, 0x00000047);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x0000004E);
	sysconf_write(sc, 0x0000004F);

	/* SET TAP INTO IDLE MODE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SET TAP INTO SHIFT DR STATE*/
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SHIFT DATA IN TCB */
	for (i = 0; i <= 53; i++) {
		if ((i == 0) || (i == 1) || (i == 18) ||
				(i == 19) || (i == 36) || (i == 37)) {
			sysconf_write(sc, 0x00000046);
			sysconf_write(sc, 0x00000047);
		}
		if ((i == 53)) {
			sysconf_write(sc, 0x0000004c);
			sysconf_write(sc, 0x0000004D);
		}
		sysconf_write(sc, 0x00000044);
		sysconf_write(sc, 0x00000045);
	}

	/* SET TAP INTO IDLE MODE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);


	for (i = 0; i <= 53; i++) {
		sysconf_write(sc, 0x00000045);
		sysconf_write(sc, 0x00000044);
	}

	/* SET TAP INTO SHIFT IR STATE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SHIFT DATA IN TDI = 110 select TPR*/
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000046);
	sysconf_write(sc, 0x00000047);
	sysconf_write(sc, 0x0000004E);
	sysconf_write(sc, 0x0000004F);

	/* SET TAP INTO IDLE MODE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	/* SET TAP INTO SHIFT DR STATE*/
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	for (i = 0; i <= 366; i++) {
		sysconf_write(sc, 0x00000044);
		sysconf_write(sc, 0x00000045);
	}

	/* SET TAP INTO IDLE MODE */
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x0000004c);
	sysconf_write(sc, 0x0000004d);
	sysconf_write(sc, 0x00000044);
	sysconf_write(sc, 0x00000045);

	mdelay(20);
	sysconf_write(sc, 0x00000040);

	return 0;
}

void __init stx7200_configure_usb(int port)
{
	static int configured[ARRAY_SIZE(stx7200_usb_devices)];

	BUG_ON(port < 0 || port > ARRAY_SIZE(stx7200_usb_devices));

	BUG_ON(configured[port]);
	configured[port] = 1;

	/* Cut 1.0 suffered from (just) a few issues with USB... */
	if (cpu_data->cut_major < 2) {
		struct stm_pad_config *pad_config =
				stx7200_usb_platform_data[port].pad_config;

		pad_config->custom_claim = stx7200_usb_soft_jtag_reset;
		pad_config->gpio_values[0].direction =
				STM_GPIO_DIRECTION_ALT_BIDIR;
	}

	platform_device_register(&stx7200_usb_devices[port]);
}



/* SATA resources --------------------------------------------------------- */

/* Ok to have same private data for both controllers */
static struct stm_plat_sata_data stx7200_sata_platform_data = {
	.phy_init = 0,
	.pc_glue_logic_init = 0,
	.only_32bit = 0,
};

static struct platform_device stx7200_sata_devices[] = {
	[0] = {
		.name = "sata-stm",
		.id = 0,
		.dev.platform_data = &stx7200_sata_platform_data,
		.num_resources = 3,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd520000, 0x1000),
			STM_PLAT_RESOURCE_IRQ_NAMED("hostc", ILC_IRQ(89), -1),
			STM_PLAT_RESOURCE_IRQ_NAMED("dmac", ILC_IRQ(88), -1),
		},
	},
	[1] = {
		.name = "sata-stm",
		.id = 1,
		.dev.platform_data = &stx7200_sata_platform_data,
		.num_resources = 3,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd521000, 0x1000),
			STM_PLAT_RESOURCE_IRQ_NAMED("hostc", ILC_IRQ(91), -1),
			STM_PLAT_RESOURCE_IRQ_NAMED("dmac", ILC_IRQ(90), -1),
		},
	},
};

void __init stx7200_configure_sata(int port)
{
	static int configured[ARRAY_SIZE(stx7200_sata_devices)];
	static int initialised_phy;

	BUG_ON(port < 0 || port > ARRAY_SIZE(stx7200_sata_devices));

	BUG_ON(configured[port]);
	configured[port] = 1;

	if (cpu_data->cut_major < 3) {
		pr_warning("SATA is only supported on cut 3 or later\n");
		return;
	}

	if (!initialised_phy) {
		struct sysconf_field *sc;

		sc = sysconf_claim(SYS_CFG, 33, 6, 6, "SATA");
		sysconf_write(sc, 1);

		stm_sata_miphy_init();
		initialised_phy = 1;
	}

	platform_device_register(&stx7200_sata_devices[port]);
}



/* Ethernet MAC resources ------------------------------------------------- */

static struct stm_pad_config *stx7200_ethernet_pad_configs[] = {
	[0] = (struct stm_pad_config []) {
		[stx7200_ethernet_mode_mii] = {
			.labels_num = 3,
			.labels = (struct stm_pad_label []) {
				STM_PAD_LABEL_STRINGS("MII0", "COL", "CRS",
						"MDC", "MDINT", "MDIO",
						"PHYCLK", "RXCLK", "RXDV",
						"RXERR", "TXCLK", "TXEN"),
				STM_PAD_LABEL_RANGE("MII0.RXD", 0, 3),
				STM_PAD_LABEL_RANGE("MII0.TXD", 0, 3),
			},
			.sysconf_values_num = 8,
			.sysconf_values = (struct stm_pad_sysconf_value []) {
				/* CONF_ETHERNET_MII_MODE0:
				 *   0 = MII, 1 = RMII */
				STM_PAD_SYS_CFG(41, 0, 0, 0),
				/* CONF_ETHERNET_PHY_CLK_EXT0:
				 *   0 = PHY clock provided by the 7200,
				 *   1 = external PHY clock source;
				 *   set by stx7200_configure_ethernet() */
				STM_PAD_SYS_CFG(41, 2, 2, -1),
				/* CONF_ETHERNET_VCI_ACK_SOURCE0 */
				STM_PAD_SYS_CFG(41, 6, 6, 0),
				/* ETHERNET_INTERFACE_ON0 */
				STM_PAD_SYS_CFG(41, 8, 8, 1),
				/* DISABLE_MSG_FOR_READ0 */
				STM_PAD_SYS_CFG(41, 12, 12, 0),
				/* DISABLE_MSG_FOR_WRITE0 */
				STM_PAD_SYS_CFG(41, 14, 14, 0),
				/* CONF_PAD_ETH0: MII0RXERR, MII0TXD.2-3,
				 *   MII0RXCLK, MII0RXD.2-3 pads function:
				 *   0 = ethernet, 1 = transport,
				 * CONF_PAD_ETH1: MII0TXCLK pad direction:
				 *   0 = output, 1 = input, */
				STM_PAD_SYS_CFG(41, 16 + 0, 16 + 1, 0),
				/* CONF_PAD_ETH10: MII0MDC, MII0TXEN &
				 *   MII0TXD[3:0] pads function:
				 *   0 = ethernet, 1 = transport. */
				STM_PAD_SYS_CFG(41, 16 + 10, 16 + 10, 0),
			},
		},
		[stx7200_ethernet_mode_rmii] = {
			.labels_num = 3,
			.labels = (struct stm_pad_label []) {
				STM_PAD_LABEL_STRINGS("MII0", "MDC", "MDINT",
						"MDIO", "PHYCLK", "RXDV",
						"TXEN"),
				STM_PAD_LABEL_RANGE("MII0.RXD", 0, 1),
				STM_PAD_LABEL_RANGE("MII0.TXD", 0, 1),
			},
			.sysconf_values_num = 7,
			.sysconf_values = (struct stm_pad_sysconf_value []) {
				/* CONF_ETHERNET_MII_MODE0:
				 *   0 = MII, 1 = RMII */
				STM_PAD_SYS_CFG(41, 0, 0, 1),
				/* CONF_ETHERNET_PHY_CLK_EXT0:
				 *   0 = PHY clock provided by the 7200,
				 *   1 = external PHY clock source;
				 *   set by stx7200_configure_ethernet() */
				STM_PAD_SYS_CFG(41, 2, 2, -1),
				/* CONF_ETHERNET_VCI_ACK_SOURCE0 */
				STM_PAD_SYS_CFG(41, 6, 6, 0),
				/* ETHERNET_INTERFACE_ON0 */
				STM_PAD_SYS_CFG(41, 8, 8, 1),
				/* DISABLE_MSG_FOR_READ0 */
				STM_PAD_SYS_CFG(41, 12, 12, 0),
				/* DISABLE_MSG_FOR_WRITE0 */
				STM_PAD_SYS_CFG(41, 14, 14, 0),
				/* CONF_PAD_ETH10: MII0MDC, MII0TXEN &
				 *   MII0TXD[3:0] pads function:
				 *   0 = ethernet, 1 = transport. */
				STM_PAD_SYS_CFG(41, 16 + 10, 16 + 10, 0),
			},
		},
	},
	[1] = (struct stm_pad_config []) {
		[stx7200_ethernet_mode_mii] = {
			.labels_num = 3,
			.labels = (struct stm_pad_label []) {
				STM_PAD_LABEL_STRINGS("MII1", "COL", "CRS",
						"MDC", "MDINT", "MDIO",
						"PHYCLK", "RXCLK", "RXDV",
						"RXERR", "TXCLK", "TXEN"),
				STM_PAD_LABEL_RANGE("MII1.RXD", 0, 3),
				STM_PAD_LABEL_RANGE("MII1.TXD", 0, 3),
			},
			.sysconf_values_num = 9,
			.sysconf_values = (struct stm_pad_sysconf_value []) {
				/* CONF_ETHERNET_MII_MODE1:
				 *   0 = MII, 1 = RMII */
				STM_PAD_SYS_CFG(41, 1, 1, 0),
				/* CONF_ETHERNET_PHY_CLK_EXT1:
				 *   0 = PHY clock provided by the 7200,
				 *   1 = external PHY clock source;
				 *   set by stx7200_configure_ethernet() */
				STM_PAD_SYS_CFG(41, 3, 3, -1),
				/* CONF_ETHERNET_VCI_ACK_SOURCE1 */
				STM_PAD_SYS_CFG(41, 7, 7, 0),
				/* ETHERNET_INTERFACE_ON1 */
				STM_PAD_SYS_CFG(41, 9, 9, 1),
				/* DISABLE_MSG_FOR_READ1 */
				STM_PAD_SYS_CFG(41, 13, 13, 0),
				/* DISABLE_MSG_FOR_WRITE1 */
				STM_PAD_SYS_CFG(41, 15, 15, 0),
				/* CONF_PAD_ETH2: MII1TXEN & MII1TXD.1-3
				 *   pads function:
				 *   0 = ethernet, 1 = transport,
				 * CONF_PAD_ETH3: MII1MDC, MII1RXCLK, &
				 *   MIIRXD.0-1 pads function:
				 *   0 = ethernet, 1 = transport,
				 * CONF_PAD_ETH4: MII1RXD.3, MII1TXCLK,
				 *   MII1COL, MII1CRS, MII1MDINT &
				 *   MII1PHYCLK pads function:
				 *   0 = ethernet, 1 = audio */
				STM_PAD_SYS_CFG(41, 16 + 2, 16 + 4, 0),
				/* CONF_PAD_ETH6: MII1TXD.0 pad direction:
				 *   0 = output, 1 = input, */
				STM_PAD_SYS_CFG(41, 16 + 6, 16 + 6, 0),
				/* CONF_PAD_ETH9: MII1RXCLK & MII1RXD.0-1
				 *   pads function:
				 *   0 = ethernet, 1 = transport, */
				STM_PAD_SYS_CFG(41, 16 + 9, 16 + 9, 0),
			},
		},
		[stx7200_ethernet_mode_rmii] = {
			.labels_num = 3,
			.labels = (struct stm_pad_label []) {
				STM_PAD_LABEL_STRINGS("MII1", "MDC", "MDINT",
						"MDIO", "PHYCLK", "RXDV",
						"TXEN"),
				STM_PAD_LABEL_RANGE("MII1.RXD", 0, 1),
				STM_PAD_LABEL_RANGE("MII1.TXD", 0, 1),
			},
			.sysconf_values_num = 9,
			.sysconf_values = (struct stm_pad_sysconf_value []) {
				/* CONF_ETHERNET_MII_MODE1:
				 *   0 = MII, 1 = RMII */
				STM_PAD_SYS_CFG(41, 1, 1, 1),
				/* CONF_ETHERNET_PHY_CLK_EXT1:
				 *   0 = PHY clock provided by the 7200,
				 *   1 = external PHY clock source;
				 *   set by stx7200_configure_ethernet() */
				STM_PAD_SYS_CFG(41, 3, 3, -1),
				/* CONF_ETHERNET_VCI_ACK_SOURCE1 */
				STM_PAD_SYS_CFG(41, 7, 7, 0),
				/* ETHERNET_INTERFACE_ON1 */
				STM_PAD_SYS_CFG(41, 9, 9, 1),
				/* DISABLE_MSG_FOR_READ1 */
				STM_PAD_SYS_CFG(41, 13, 13, 0),
				/* DISABLE_MSG_FOR_WRITE1 */
				STM_PAD_SYS_CFG(41, 15, 15, 0),
				/* CONF_PAD_ETH2: MII1TXEN & MII1TXD.1-3
				 *   pads function:
				 *   0 = ethernet, 1 = transport,
				 * CONF_PAD_ETH3: MII1MDC, MII1RXCLK, &
				 *   MIIRXD.0-1 pads function:
				 *   0 = ethernet, 1 = transport, */
				STM_PAD_SYS_CFG(41, 16 + 2, 16 + 3, 0),
				/* CONF_PAD_ETH6: MII1TXD.0 pad direction:
				 *   0 = output, 1 = input, */
				STM_PAD_SYS_CFG(41, 16 + 6, 16 + 6, 0),
				/* CONF_PAD_ETH9: MII1RXCLK & MII1RXD.0-1
				 *   pads function:
				 *   0 = ethernet, 1 = transport, */
				STM_PAD_SYS_CFG(41, 16 + 9, 16 + 9, 0),
			},
		},
	},
};

static void stx7200_ethernet_fix_mac_speed(void *bsp_priv, unsigned int speed)
{
	struct sysconf_field *mac_speed_sel = bsp_priv;

	sysconf_write(mac_speed_sel, (speed == SPEED_100) ? 1 : 0);
}

static struct stm_plat_stmmacenet_data stx7200_ethernet_platform_data[] = {
	[0] = {
		.pbl = 32,
		.has_gmac = 0,
		.fix_mac_speed = stx7200_ethernet_fix_mac_speed,
		/* .pad_config set in stx7200_configure_ethernet() */
	},
	[1] = {
		.pbl = 32,
		.has_gmac = 0,
		.fix_mac_speed = stx7200_ethernet_fix_mac_speed,
		/* .pad_config set in stx7200_configure_ethernet() */
	},
};

static struct platform_device stx7200_ethernet_devices[] = {
	[0] = {
		.name = "stmmaceth",
		.id = 0,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd500000, 0x10000),
			STM_PLAT_RESOURCE_IRQ_NAMED("macirq", ILC_IRQ(92), -1),
		},
		.dev.platform_data = &stx7200_ethernet_platform_data[0],
	},
	[1] = {
		.name = "stmmaceth",
		.id = 1,
		.num_resources = 2,
		.resource = (struct resource[]) {
			STM_PLAT_RESOURCE_MEM(0xfd510000, 0x10000),
			STM_PLAT_RESOURCE_IRQ_NAMED("macirq", ILC_IRQ(94), -1),
		},
		.dev.platform_data = &stx7200_ethernet_platform_data[1],
	},
};

void __init stx7200_configure_ethernet(int port,
		struct stx7200_ethernet_config *config)
{
	static int configured[ARRAY_SIZE(stx7200_ethernet_devices)];
	struct stx7200_ethernet_config default_config;
	struct stm_pad_config *pad_config;

	BUG_ON(port < 0 || port >= ARRAY_SIZE(stx7200_ethernet_devices));

	BUG_ON(configured[port]);
	configured[port] = 1;

	if (!config)
		config = &default_config;

	pad_config = &stx7200_ethernet_pad_configs[port][config->mode];

	stx7200_ethernet_platform_data[port].pad_config = pad_config;
	stx7200_ethernet_platform_data[port].bus_id = config->phy_bus;

	pad_config->sysconf_values[1].value = (config->ext_clk ? 1 : 0);

	/* MAC_SPEED_SEL */
	stx7200_ethernet_platform_data[port].bsp_priv = sysconf_claim(SYS_CFG,
			41, 4 + port, 4 + port, "stmmac");

	platform_device_register(&stx7200_ethernet_devices[port]);
}
