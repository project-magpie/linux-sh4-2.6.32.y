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
#include <linux/stm/fli7510.h>
#include <asm/irq-ilc.h>



/* PCI Resources ---------------------------------------------------------- */

/* You may pass one of the PCI_PIN_* constants to use dedicated pin or
 * just pass interrupt number generated with gpio_to_irq() when PIO pads
 * are used as interrupts or IRLx_IRQ when using external interrupts inputs */
int fli7510_pcibios_map_platform_irq(struct stm_plat_pci_config *pci_config,
		u8 pin)
{
	int result;
	int type;

	BUG_ON(cpu_data->type != CPU_FLI7510);

	if (pin < 1 || pin > 4)
		return -1;

	type = pci_config->pci_irq[pin - 1];

	switch (type) {
	case PCI_PIN_ALTERNATIVE:
		/* Actually there are no alternative pins... */
		BUG();
		result = -1;
		break;
	case PCI_PIN_DEFAULT:
		/* Only INTA/INTB are described as "dedicated" PCI
		 * interrupts and even if these two are described as that,
		 * they are actually just "normal" external interrupt
		 * inputs (INT2 & INT3)... Additionally, depending
		 * on the spec version, the number below may seem wrong,
		 * but believe me - they are correct :-) */
		switch (pin) {
		case 1 ... 2:
			result = ILC_IRQ(119 - (pin - 1));
			/* Configure the ILC input to be active low,
			 * which is the PCI way... */
			set_irq_type(result, IRQ_TYPE_LEVEL_LOW);
			break;
		default:
			/* Other should be passed just as interrupt number
			 * (eg. result of the ILC_IRQ() macro) */
			BUG();
			result = -1;
			break;
		}
		break;
	case PCI_PIN_UNUSED:
		result = -1; /* Not used */
		break;
	default:
		/* Take whatever interrupt you are told */
		result = type;
		break;
	}

	return result;
}

static struct platform_device fli7510_pci_device = {
	.name = "pci_stm",
	.id = -1,
	.num_resources = 7,
	.resource = (struct resource[]) {
		/* 512 MB */
		STM_PLAT_RESOURCE_MEM_NAMED("Memory", 0xc0000000, 0x20000000),
		{
			.name = "IO",
			.start = 0x0400,
			.end = 0xffff,
			.flags = IORESOURCE_IO,
		},
		STM_PLAT_RESOURCE_MEM_NAMED("EMISS", 0xfd200000, 0x17fc),
		STM_PLAT_RESOURCE_MEM_NAMED("PCI-AHB", 0xfd080000, 0xff),
		STM_PLAT_RESOURCE_IRQ_NAMED("DMA", ILC_IRQ(47), -1),
		STM_PLAT_RESOURCE_IRQ_NAMED("Error", ILC_IRQ(48), -1),
		/* SERR interrupt set in fli7510_configure_pci() */
		STM_PLAT_RESOURCE_IRQ_NAMED("SERR", -1, -1),
	},
};

#define FLI7510_PIO_PCI_REQ(i)   stm_gpio(15, (i - 1) * 2)
#define FLI7510_PIO_PCI_GNT(i)   stm_gpio(15, ((i - 1) * 2) + 1)
#define FLI7510_PIO_PCI_INT(i)   stm_gpio(25, i)
#define FLI7510_PIO_PCI_SERR     stm_gpio(16, 6)

void __init fli7510_configure_pci(struct stm_plat_pci_config *pci_conf)
{
	struct sysconf_field *sc;
	int i;

	/* PCI is available in FLI7510 only! */
	if (cpu_data->type != CPU_FLI7510) {
		BUG();
		return;
	}

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

	/* The EMI_BUS_REQ[0] pin (also know just as EmiBusReq) is
	 * internally wired to the arbiter's PCI request 3 line.
	 * And the answer to the obvious question is: That's right,
	 * the EMI_BUSREQ[3] is not wired at all... */
	pci_conf->req0_to_req3 = 1;
	BUG_ON(pci_conf->req_gnt[3] != PCI_PIN_UNUSED);

	/* Copy over platform specific data to driver */
	fli7510_pci_device.dev.platform_data = pci_conf;

	/* REQ/GNT[0] are dedicated EMI pins */
	BUG_ON(pci_conf->req_gnt[0] != PCI_PIN_DEFAULT);

	/* REQ/GNT[1..2] PIOs setup */
	for (i = 1; i <= 2; i++) {
		switch (pci_conf->req_gnt[i]) {
		case PCI_PIN_DEFAULT:
			/* emiss_bus_req_enable */
			sc = sysconf_claim(CFG_COMMS_CONFIG_1,
					24 + (i - 1), 24 + (i - 1), "PCI");
			sysconf_write(sc, 1);
			if (gpio_request(FLI7510_PIO_PCI_REQ(i),
					"PCI_REQ") == 0)
				stm_gpio_direction(FLI7510_PIO_PCI_REQ(i),
						STM_GPIO_DIRECTION_IN);
			else
				printk(KERN_ERR "Unable to configure PIO for "
						"PCI_REQ%d\n", i);
			if (gpio_request(FLI7510_PIO_PCI_GNT(i),
					"PCI_GNT") == 0)
				stm_gpio_direction(FLI7510_PIO_PCI_GNT(i),
						STM_GPIO_DIRECTION_ALT_OUT);
			else
				printk(KERN_ERR "Unable to configure PIO for "
						"PCI_GNT%d\n", i);
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

	/* REG/GNT[3] are... unavailable... */
	BUG_ON(pci_conf->req_gnt[3] != PCI_PIN_UNUSED);

	/* Claim "dedicated" interrupt pins... */
	for (i = 0; i < 4; i++) {
		static const char *int_name[] = {
			"PCI INTA",
			"PCI INTB",
		};

		switch (pci_conf->pci_irq[i]) {
		case PCI_PIN_DEFAULT:
			if (i > 1) {
				BUG();
				break;
			}
			if (gpio_request(FLI7510_PIO_PCI_INT(i),
					int_name[i]) != 0)
				printk(KERN_ERR "Unable to claim PIO for "
						"%s\n", int_name[0]);
			break;
		case PCI_PIN_ALTERNATIVE:
			/* No alternative here... */
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
		if (gpio_request(FLI7510_PIO_PCI_SERR, "PCI SERR#") == 0) {
			stm_gpio_direction(FLI7510_PIO_PCI_SERR,
					STM_GPIO_DIRECTION_IN);
			pci_conf->serr_irq = gpio_to_irq(FLI7510_PIO_PCI_SERR);
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
	if (pci_conf->serr_irq == PCI_PIN_UNUSED) {
		/* "Disable" the SERR IRQ resource (it's last on the list) */
		fli7510_pci_device.num_resources--;
	} else {
		struct resource *res = platform_get_resource_byname(
				&fli7510_pci_device, IORESOURCE_IRQ, "SERR");

		res->start = pci_conf->serr_irq;
		res->end = pci_conf->serr_irq;
	}

#if defined(CONFIG_PM)
#warning TODO: PCI Power Management
#endif
	/* pci_pwr_dwn_req */
	sc = sysconf_claim(CFG_PWR_DWN_CTL, 2, 2, "PCI");
	sysconf_write(sc, 0);

	/* status_pci_pwr_dwn_grant */
	sc = sysconf_claim(CFG_PCI_ROPC_STATUS, 18, 18, "PCI");
	while (sysconf_read(sc))
		cpu_relax();

	platform_device_register(&fli7510_pci_device);
}
