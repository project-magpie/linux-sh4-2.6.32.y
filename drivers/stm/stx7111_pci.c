#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/stm/pad.h>
#include <linux/stm/sysconf.h>
#include <linux/stm/emi.h>
#include <linux/stm/stx7105.h>
#include <asm/irq-ilc.h>

/* This function assumes you are using the dedicated pins. Production boards will
 * more likely use the external interrupt pins and save the PIOs
 */
int stx7111_pcibios_map_platform_irq(struct stm_plat_pci_config *pci_config, u8 pin)
{
	int irq;
	int pin_type;

	if((pin > 4) || (pin < 1)) return -1;

	pin_type = pci_config->pci_irq[pin - 1];

	switch(pin_type) {
		case PCI_PIN_DEFAULT:
			irq = evt2irq(0xa00 + ( (pin - 1) * 0x20 ));
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

static struct platform_device pci_device =
{
	.name = "pci_stm",
	.id = -1,
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
		[5] = STM_PLAT_RESOURCE_IRQ_NAMED("PCI SERR", evt2irq(0x1200), -1)
	}
};

void __init stx7111_configure_pci(struct stm_plat_pci_config *pci_conf)
{
	int i;
	struct sysconf_field *sc;
	static const char *int_name[] = {"PCI INT A","PCI INT B","PCI INT C","PCI INT D"};
	static const char *req_name[] = {"PCI REQ 0 ","PCI REQ 1","PCI REQ 2","PCI REQ 3"};
	static const char *gnt_name[] = {"PCI GNT 0 ","PCI GNT 1","PCI GNT 2","PCI GNT 3"};


#ifndef CONFIG_PCI
	return;
#else

	/* Fill in the default values for the 7111 */
	if(!pci_conf->ad_override_default) {
		pci_conf->ad_threshold = 5;pci_conf->ad_read_ahead = 1;
		pci_conf->ad_chunks_in_msg = 0; pci_conf->ad_pcks_in_chunk = 0;
		pci_conf->ad_trigger_mode = 1; pci_conf->ad_max_opcode = 5;
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

	for(i = 0; i < 4; i++) {
                if(pci_conf->pci_irq[i] == PCI_PIN_DEFAULT) {
			/* Set the alternate function correctly in sysconfig */
			int pin = (i == 0) ? 7 : (3 - i);
			gpio_request(stm_gpio(3, pin), int_name[i]);
			stm_gpio_direction(stm_gpio(3, pin), STM_GPIO_DIRECTION_IN);
			sc = sysconf_claim(SYS_CFG, 5, 9 + i, 9 + i, int_name[i]);
			sysconf_write(sc, 1);
                }
        }

	/* REQ/GNT 0 are dedicated pins */
	for( i = 1; i < 4; i++ ) {
		if(pci_conf->req_gnt[i] == PCI_PIN_DEFAULT) {
			int req_pin = ((3 - i) * 2) + 2;
			int gnt_pin = (3 - i) + 5;
			gpio_request(stm_gpio(0, req_pin), req_name[i]);
			stm_gpio_direction(stm_gpio(0, req_pin), STM_GPIO_DIRECTION_IN);
			gpio_request(stm_gpio(2, gnt_pin), gnt_name[i]);
			stm_gpio_direction(stm_gpio(2, gnt_pin), STM_GPIO_DIRECTION_ALT_OUT);
			sc = sysconf_claim(SYS_CFG, 5, (3 - i) + 2,  (3 - i) + 2, req_name[i]);
			sysconf_write(sc, 1);
		}
	}

	/* Enable the SERR interrupt if wired up */
	if(pci_conf->serr_irq == PCI_PIN_DEFAULT ) {
		/* Use the default */
		gpio_request(stm_gpio(5, 5), pci_device.resource[5].name);
		stm_gpio_direction(stm_gpio(5, 5), STM_GPIO_DIRECTION_IN);
	} else {
		pci_device.resource[5].start = 	pci_device.resource[5].end = pci_conf->serr_irq;
	}

	platform_device_register(&pci_device);
#endif
}
