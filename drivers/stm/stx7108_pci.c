/*
 * (c) 2010 STMicroelectronics Limited
 *
 * Author: Pawel Moll <pawel.moll@st.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */



#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/emi.h>
#include <linux/stm/stx7108.h>
#include <asm/irq-ilc.h>



/* PCI Resources ---------------------------------------------------------- */

/* You may pass one of the PCI_PIN_* constants to use dedicated pin or
 * just pass interrupt number generated with gpio_to_irq() when
 * non-standard PIO pads are used as interrupts. */
int stx7108_pcibios_map_platform_irq(struct stm_plat_pci_config *pci_config,
		u8 pin)
{
	int irq;
	int pin_type;

	if ((pin > 4) || (pin < 1))
		return -1;

	pin_type = pci_config->pci_irq[pin - 1];

	switch (pin_type) {
	case PCI_PIN_DEFAULT:
		irq = ILC_IRQ(122 + pin - 1);
		break;
	case PCI_PIN_ALTERNATIVE:
		/* No alternative here... */
		BUG();
		irq = -1;
		break;
	case PCI_PIN_UNUSED:
		irq = -1; /* Not used */
		break;
	default:
		irq = pin_type; /* Take whatever interrupt you are told */
		break;
	}

	return irq;
}

static struct platform_device stx7108_pci_device = {
	.name = "pci_stm",
	.id = -1,
	.num_resources = 7,
	.resource = (struct resource[]) {
		STM_PLAT_RESOURCE_MEM_NAMED("Memory", 0xc0000000,
				960 * 1024 * 1024),
		{
			.name = "IO",
			.start = 0x0400,
			.end = 0xffff,
			.flags = IORESOURCE_IO,
		},
		STM_PLAT_RESOURCE_MEM_NAMED("EMISS", 0xfdaa8000, 0x17fc),
		STM_PLAT_RESOURCE_MEM_NAMED("PCI-AHB", 0xfea08000, 0xff),
		STM_PLAT_RESOURCE_IRQ_NAMED("DMA", ILC_IRQ(126), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("Error", ILC_IRQ(127), -1),
		/* SERR interrupt set in stx7105_configure_pci() */
		STM_PLAT_RESOURCE_IRQ_NAMED("SERR", -1, -1),
	},
};



#define STX7108_PIO_PCI_SERR stm_gpio(6, 6)

static struct stm_pad_config stx7108_pci_reqgnt_pad_config[] = {
	/* REQ0/GNT0 have dedicated pins... */
	[1] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_IN(8, 6, 2),	/* REQ1 */
			STM_PAD_PIO_OUT(8, 7, 2), 	/* GNT1 */
		},
	},
	[2] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_IN(2, 4, 2),	/* REQ2 */
			STM_PAD_PIO_OUT(2, 5, 2), 	/* GNT2 */
		},
	},
	[3] = {
		.gpios_num = 2,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_IN(9, 4, 2),	/* REQ3 */
			STM_PAD_PIO_OUT(9, 5, 2), 	/* GNT3 */
		},
	},
};

static struct stm_pad_config stx7108_pci_int_pad_config[] = {
	[0] = {
		.gpios_num = 1,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_IN(8, 4, 2),	/* INTA */
		},
	},
	[1] = {
		.gpios_num = 1,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_IN(8, 5, 2),	/* INTB */
		},
	},
	[2] = {
		.gpios_num = 1,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_IN(2, 6, 2),	/* INTC */
		},
	},
	[3] = {
		.gpios_num = 1,
		.gpios = (struct stm_pad_gpio []) {
			STM_PAD_PIO_IN(3, 4, 2),	/* INTD */
		},
	},
};



void __init stx7108_configure_pci(struct stm_plat_pci_config *pci_conf)
{
	struct sysconf_field *sc;
	int i;

	/* LLA clocks have these horrible names... */
	pci_conf->clk_name = "CLKA_PCI";

	/* REQ0 is actually wired to REQ3 to work around NAND problems */
	pci_conf->req0_to_req3 = 1;
	BUG_ON(pci_conf->req_gnt[3] != PCI_PIN_UNUSED);

	/* Fill in the default values */
	if (!pci_conf->ad_override_default) {
		pci_conf->ad_threshold = 5;
		pci_conf->ad_read_ahead = 1;
		pci_conf->ad_chunks_in_msg = 0;
		pci_conf->ad_pcks_in_chunk = 0;
		pci_conf->ad_trigger_mode = 1;
		pci_conf->ad_max_opcode = 5;
		pci_conf->ad_posted = 1;
	}

	/* Copy over platform specific data to driver */
	stx7108_pci_device.dev.platform_data = pci_conf;

#if defined(CONFIG_PM)
#warning TODO: PCI Power Management
#endif
	/* Claim and power up the PCI cell */
	sc = sysconf_claim(SYS_CFG_BANK2, 30, 2, 2, "PCI_PWR_DWN_REQ");
	sysconf_write(sc, 0); /* We will need to stash this somewhere
				 for power management. */
	sc = sysconf_claim(SYS_STA_BANK2, 1, 2, 2, "PCI_PWR_DWN_GRANT");
	while (sysconf_read(sc))
		cpu_relax(); /* Loop until powered up */

	/* REQ/GNT[0] are dedicated EMI pins */
	BUG_ON(pci_conf->req_gnt[0] != PCI_PIN_DEFAULT);

	/* Configure the REQ/GNT[1..2], muxed with PIOs */
	for (i = 1; i <= 2; i++) {
		switch (pci_conf->req_gnt[i]) {
		case PCI_PIN_DEFAULT:
			if (stm_pad_claim(&stx7108_pci_reqgnt_pad_config[i],
					"PCI") == NULL) {
				printk(KERN_ERR "Failed to claim REQ/GNT%d "
						"pads!\n", i);
				BUG();
			}
			break;
		case PCI_PIN_UNUSED:
			/* Unused is unused - nothing to do */
			break;
		default:
			/* No alternative here... */
			BUG();
			break;
		}
	}

	/* Configure INTA..D interrupts */
	for (i = 0; i < 4; i++) {
		switch (pci_conf->pci_irq[i]) {
		case PCI_PIN_DEFAULT:
			if (stm_pad_claim(&stx7108_pci_int_pad_config[i],
						"PCI") == NULL) {
				printk(KERN_ERR "Failed to claim INT%c pad!\n",
						'A' + i);
				BUG();
			}
			set_irq_type(ILC_IRQ(122 + i), IRQ_TYPE_LEVEL_LOW);
			break;
		case PCI_PIN_ALTERNATIVE:
			/* There is no alternative here ;-) */
			BUG();
			break;
		default:
			/* Unused or interrupt number passed, nothing to do */
			break;
		}
	}

	/* Configure the SERR interrupt (if wired up) */
	switch (pci_conf->serr_irq) {
	case PCI_PIN_DEFAULT:
		if (gpio_request(STX7108_PIO_PCI_SERR, "PCI_SERR#") == 0) {
			gpio_direction_input(STX7108_PIO_PCI_SERR);
			pci_conf->serr_irq = gpio_to_irq(STX7108_PIO_PCI_SERR);
			set_irq_type(pci_conf->serr_irq, IRQ_TYPE_LEVEL_LOW);
		} else {
			printk(KERN_WARNING "%s(): Failed to claim PCI SERR# "
					"PIO!\n", __func__);
			pci_conf->serr_irq = PCI_PIN_UNUSED;
		}
		break;
	case PCI_PIN_ALTERNATIVE:
		/* No alternative here */
		BUG();
		pci_conf->serr_irq = PCI_PIN_UNUSED;
		break;
	}
	if (pci_conf->serr_irq != PCI_PIN_UNUSED) {
		struct resource *res = platform_get_resource_byname(
				&stx7108_pci_device, IORESOURCE_IRQ, "SERR");

		BUG_ON(!res);
		res->start = pci_conf->serr_irq;
		res->end = pci_conf->serr_irq;
	}

	/* LOCK is not claimed as is totally pointless, the SOCs do not
	 * support any form of coherency */

	platform_device_register(&stx7108_pci_device);
}
