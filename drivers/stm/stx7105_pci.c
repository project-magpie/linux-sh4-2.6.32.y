#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/emi.h>
#include <linux/stm/stx7105.h>
#include <asm/irq-ilc.h>

/*
 * This function assumes you are using the dedicated pins. Production boards will
 * more likely use the external interrupt pins and save the PIOs
 */

int stx7105_pcibios_map_platform_irq(struct stm_plat_pci_config *pci_config, u8 pin)
{
	int irq;
	int pin_type;

	if((pin > 4) || (pin < 1)) return -1;

	pin_type = pci_config->pci_irq[pin - 1];

	switch(pin_type) {
		case PCI_PIN_DEFAULT:
		case PCI_PIN_ALTERNATIVE:
			irq = ILC_EXT_IRQ(pin + 25);
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

#ifdef CONFIG_32BIT
#define PCI_WINDOW_START 0xc0000000
#define PCI_WINDOW_SIZE  0x20000000 /* 512 Megs */
#else
#define PCI_WINDOW_START 0x08000000
#define PCI_WINDOW_SIZE  0x04000000 /* 64 Megs */
#endif

/* Various controls bits in sysconfig 5 */
/* Relative to the start of the PCI block, so they
 * can be plugged into sysconf(read/write) calls
 */

#define PCI_DEVICE_NOT_HOST_ENABLE 	(1<<13)
#define PCI_CLOCK_MASTER_NOT_SLAVE 	(1<<12)
#define PCI_INT0_SRC_SEL		(1<<11)
#define PCI_LOCK_IN_SEL			(1<<9)
#define PCI_SYS_ERROR_ENABLE		(1<<8)
#define PCI_RESETN_ENABLE		(1<<7)
#define PCI_INT_TO_HOST_ENABLE		(1<<6)
#define PCI_INT_FROM_DEVICE(n)		(1 << (5 - (n)))
#define PCI_LOCK_IN_ENABLE		(1<<1)
#define PCI_PME_IN_ENABLE		(1<<0)

static struct platform_device pci_device =
{
	.name = "pci_stm",
	.num_resources = 6,
	.resource = (struct resource[]) {
		[0] = STM_PLAT_RESOURCE_MEM(0xfe400000, 0x17fc),
		[1] = STM_PLAT_RESOURCE_MEM(0xfe560000, 0xff),
		[2] = STM_PLAT_RESOURCE_MEM(PCI_WINDOW_START, PCI_WINDOW_SIZE),
		[3] = {
			.start = 0x1024,
			.end = 0xffff,
			.flags = IORESOURCE_IO,
		},
		[4] = STM_PLAT_RESOURCE_IRQ_NAMED("PCI DMA", evt2irq(0x1280), -1),
		[5] = STM_PLAT_RESOURCE_IRQ_NAMED("PCI SERR", ILC_IRQ(25), -1)
	}
};

void __init stx7105_configure_pci(struct stm_plat_pci_config *pci_conf)
{
#ifndef CONFIG_PCI
	return;
#else
	int i;
	struct sysconf_field *sc;
	static const char *int_name[] = {"PCI INT A","PCI INT B","PCI INT C","PCI INT D"};
	static const char *req_name[] = {"PCI REQ 0 ","PCI REQ 1","PCI REQ 2","PCI REQ 3"};
	static const char *gnt_name[] = {"PCI GNT 0 ","PCI GNT 1","PCI GNT 2","PCI GNT 3"};
	int use_alt_for_int0;
	int sys5_int_enables = 0;

	/* Cut 3 has req0 wired to req3 to work around NAND problems */
        pci_conf->req0_to_req3 = (cpu_data->cut_major >= 3);

	/* Fill in the default values for the 7105 */
	if(!pci_conf->ad_override_default) {
		pci_conf->ad_threshold = 5;
		pci_conf->ad_read_ahead = 1;
		pci_conf->ad_chunks_in_msg = 0;
		pci_conf->ad_pcks_in_chunk = 0;
		pci_conf->ad_trigger_mode = 1;
		pci_conf->ad_max_opcode = 5;
		pci_conf->ad_posted = 1;
	}

	/* Copy over platform specific data to driver */
	pci_device.dev.platform_data = pci_conf;

	/* Claim and power up the PCI cell */
	sc = sysconf_claim(SYS_CFG, 32, 2, 2, "PCI Power");
	sysconf_write(sc, 0); // We will need to stash this somewhere for power management.

	sc = sysconf_claim(SYS_STA, 15, 2, 2, "PCI Power status");
	while(sysconf_read(sc)); // Loop until powered up

	/* Claim and set pads into PCI mode */
	sc = sysconf_claim(SYS_CFG, 31, 20, 20, "PCI");
	sysconf_write(sc, 1);

	/* SERR is only an output on Cut2, designed for device mode. So no point in enabling it.
	 * LOCK is totally pointless, the SOCs do not support any form of coherency
	 */
	sc = sysconf_claim(SYS_CFG, 5, 16, 29, "PCI Config");
	use_alt_for_int0 = (pci_conf->pci_irq[0] == PCI_PIN_ALTERNATIVE);
	sysconf_write(sc, PCI_CLOCK_MASTER_NOT_SLAVE | ( (use_alt_for_int0) ? PCI_INT0_SRC_SEL : 0 ) );

	if(use_alt_for_int0 ) {
		set_irq_type(ILC_EXT_IRQ(26), IRQ_TYPE_LEVEL_LOW);
		gpio_request(stm_gpio(15, 3), int_name[0]);
		stm_gpio_direction(stm_gpio(15, 3), STM_GPIO_DIRECTION_IN);
		sys5_int_enables |= PCI_INT_FROM_DEVICE(0);
	}

	for(i = 0; i < 4; i++) {
                if(pci_conf->pci_irq[i] == PCI_PIN_DEFAULT) {
			set_irq_type(ILC_EXT_IRQ(26 + i), IRQ_TYPE_LEVEL_LOW);
			gpio_request(stm_gpio(6, i), int_name[i]);
			stm_gpio_direction(stm_gpio(6, i), STM_GPIO_DIRECTION_IN);
			/* Set the alternate function correctly in sysconfig */
			sys5_int_enables |= PCI_INT_FROM_DEVICE(i);
                }
        }

	/* Set the approprate enabled interrupts */
	sysconf_write(sc, sysconf_read(sc) | sys5_int_enables);

	/* REQ/GNT 0 are dedicated pins, so we start  from 1 */
	for(i = 1; i < 4; i++ ) {
		if(pci_conf->req_gnt[i] == PCI_PIN_DEFAULT) {
			gpio_request(stm_gpio(6, 4 + i), req_name[i]);
			stm_gpio_direction(stm_gpio(6, 4 + i), STM_GPIO_DIRECTION_IN);
			gpio_request(stm_gpio(7, i), gnt_name[i]);
			stm_gpio_direction(stm_gpio(7, i), STM_GPIO_DIRECTION_ALT_OUT);

			/* stx7105_pio_sysconf_alt(7, i, 0x11, gnt_name[i]); */
			sc = sysconf_claim(SYS_CFG, 37, i, i, gnt_name[i]);
			sysconf_write(sc, 1);
			sc = sysconf_claim(SYS_CFG, 37, i+8, i+8, gnt_name[i]);
			sysconf_write(sc, 1);
		}
	}

	/* Enable the SERR interrupt if wired up */
	if(pci_conf->serr_irq == PCI_PIN_DEFAULT ) {
		/* Use the default */
		gpio_request(stm_gpio(6, 4), pci_device.resource[5].name);
		stm_gpio_direction(stm_gpio(6, 4), STM_GPIO_DIRECTION_IN);
	} else {
		pci_device.resource[5].start = 	pci_device.resource[5].end = pci_conf->serr_irq;
	}

	platform_device_register(&pci_device);

#endif /* CONFIG_PCI */
}
