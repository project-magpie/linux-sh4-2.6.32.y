/*
 * STx710x Setup
 *
 * Copyright (C) 2007 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 */
#include <linux/platform_device.h>
#include <linux/init.h>
#include <linux/serial.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/stm/soc.h>
#include <linux/stm/soc_init.h>
#include <linux/stm/pio.h>
#include <linux/phy.h>
#include <linux/stm/sysconf.h>
#include <asm/sci.h>
#include <linux/stm/fdma-plat.h>
#include <linux/stm/fdma-reqs.h>

static unsigned long chip_revision, chip_7109;
static struct sysconf_field *sys_cfg7_0;

static struct plat_sci_port sci_platform_data[] = {
	{
		.mapbase	= 0xffe00000,
		.flags		= UPF_BOOT_AUTOCONF,
		.type		= PORT_SCIF,
		.irqs		= { 26, 27, 28, 29 },
	}, {
		.mapbase	= 0xffe80000,
		.flags		= UPF_BOOT_AUTOCONF,
		.type		= PORT_SCIF,
		.irqs		= { 43, 44, 45, 46 },
	}, {
		.flags = 0,
	}
};

static struct platform_device sci_device = {
	.name		= "sh-sci",
	.id		= -1,
	.dev		= {
		.platform_data	= sci_platform_data,
	},
};

static struct resource wdt_resource[] = {
	/* Watchdog timer only needs a register address */
	[0] = {
		.start = 0xFFC00008,
		.end = 0xFFC00010,
		.flags = IORESOURCE_MEM,
	}
};

struct platform_device wdt_device = {
	.name = "wdt",
	.id = -1,
	.num_resources = ARRAY_SIZE(wdt_resource),
	.resource = wdt_resource,
};

static struct resource rtc_resource[]= {
	[0] = {
		.start = 0xffc80000,
		.end   = 0xffc80000 + 0x40,
		.flags = IORESOURCE_MEM
	},
	[1] = {
		.start = 20,/* Alarm IRQ   */
		.flags = IORESOURCE_IRQ
	},
	[2] = {
		.start = 21,/* Periodic IRQ*/
		.flags = IORESOURCE_IRQ
	},
};

static struct platform_device rtc_device = {
	.name		= "rtc",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(rtc_resource),
	.resource	= rtc_resource,
};

/* USB resources ----------------------------------------------------------- */

static struct resource st40_ohci_resources[] = {
	[0] = {
		.start	= 0x19100000 + 0xffc00,
		.end	= 0x19100000 + 0xffcff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 168,
		.end	= 168,
		.flags	= IORESOURCE_IRQ,
	}
};
static struct resource st40_ehci_resources[] = {
	[0] =  {
		.start	= 0x19100000 + 0xffe00,
		.end	= 0x19100000 + 0xffeff,
		.flags	= IORESOURCE_MEM,
	},
	[1] = {
		.start	= 169,
		.end	= 169,
		.flags	= IORESOURCE_IRQ,
	},
};

/*
 * Defines for the controller register offsets
 */
#define UHOST2C_BASE			0x19100000
#define AHB2STBUS_WRAPPER_GLUE_BASE	(UHOST2C_BASE)
#define AHB2STBUS_RESERVED1_BASE	(UHOST2C_BASE + 0x000e0000)
#define AHB2STBUS_RESERVED2_BASE	(UHOST2C_BASE + 0x000f0000)
#define AHB2STBUS_OHCI_BASE		(UHOST2C_BASE + 0x000ffc00)
#define AHB2STBUS_EHCI_BASE		(UHOST2C_BASE + 0x000ffe00)
#define AHB2STBUS_PROTOCOL_BASE		(UHOST2C_BASE + 0x000fff00)

static struct sysconf_field *usb_power_sc;

static void usb_power_up(void* dev)
{
	unsigned long reg;

	/* Make sure PLL is on */
	reg = sysconf_read(usb_power_sc);
	if (reg) {
		sysconf_write(usb_power_sc, 0);
		mdelay(100);
	}
}

static struct plat_usb_data usb_wrapper = {
	.ahb2stbus_wrapper_glue_base = AHB2STBUS_WRAPPER_GLUE_BASE,
	.ahb2stbus_protocol_base = AHB2STBUS_PROTOCOL_BASE,
	.power_up = usb_power_up,
	.initialised = 0,
	.port_number = 0,
};

static u64 st40_dma_mask = 0xfffffff;

static struct platform_device  st40_ohci_device = {
	.name = "ST40-ohci",
	.id=1,
	.dev = {
		.dma_mask = &st40_dma_mask,
		.coherent_dma_mask = 0xffffffful,
		.platform_data = &usb_wrapper,
	},
	.num_resources = ARRAY_SIZE(st40_ohci_resources),
	.resource = st40_ohci_resources,
};

static struct platform_device  st40_ehci_device = {
	.name = "ST40-ehci",
	.id=2,
	.dev = {
		.dma_mask = &st40_dma_mask,
		.coherent_dma_mask = 0xffffffful,
		.platform_data = &usb_wrapper,
	},
	.num_resources = ARRAY_SIZE(st40_ehci_resources),
	.resource = st40_ehci_resources,
};

void __init stx7100_configure_usb(void)
{
	static struct stpio_pin *pin;

	/* Work around for USB over-current detection chip being
	 * active low, and the 710x being active high.
	 *
	 * This test is wrong for 7100 cut 3.0 (which needs the work
	 * around), but as we can't reliably determine the minor
	 * revision number, hard luck, this works for most people.
	 */
	if ( ( chip_7109 && (chip_revision < 2)) ||
	     (!chip_7109 && (chip_revision < 3)) ) {
		pin = stpio_request_pin(5,6, "USBOC", STPIO_OUT);
		stpio_set_pin(pin, 0);
	}

	/*
	 * There have been two changes to the USB power enable signal:
	 *
	 * - 7100 upto and including cut 3.0 and 7109 1.0 generated an
	 *   active high enables signal. From 7100 cut 3.1 and 7109 cut 2.0
	 *   the signal changed to active low.
	 *
	 * - The 710x ref board (mb442) has always used power distribution
	 *   chips which have active high enables signals (on rev A and B
	 *   this was a TI TPS2052, rev C used the ST equivalent a ST2052).
	 *   However rev A and B had a pull up on the enables signal, while
	 *   rev C changed this to a pull down.
	 *
	 * The net effect of all this is that the easiest way to drive
	 * this signal is ignore the USB hardware and drive it as a PIO
	 * pin.
	 *
	 * (Note the USB over current input on the 710x changed from active
	 * high to low at the same cuts, but board revs A and B had a resistor
	 * option to select an inverted output from the TPS2052, so no
	 * software work around is required.)
	 */
	pin = stpio_request_pin(5,7, "USBPWR", STPIO_OUT);
	stpio_set_pin(pin, 1);

	usb_power_sc = sysconf_claim(SYS_CFG, 2, 1, 1, "usb");

	platform_device_register(&st40_ohci_device);
	platform_device_register(&st40_ehci_device);
}

/* FDMA resources ---------------------------------------------------------- */

#ifdef CONFIG_STM_DMA

#include <linux/stm/7100_fdma2_firmware.h>
#include <linux/stm/7109_cut2_fdma2_firmware.h>
#include <linux/stm/7109_cut3_fdma2_firmware.h>

static struct fdma_regs stb7100_fdma_regs = {
	.fdma_id		= FDMA2_ID,
	.fdma_ver		= FDAM2_VER,
	.fdma_en		= FDMA2_ENABLE_REG,
	.fdma_clk_gate		= FDMA2_CLOCKGATE,
	.fdma_rev_id		= FDMA2_REV_ID,
	.fdma_cmd_statn		= STB7100_FDMA_CMD_STATn_REG,
	.fdma_ptrn		= STB7100_FDMA_PTR_REG,
	.fdma_cntn		= STB7100_FDMA_COUNT_REG,
	.fdma_saddrn		= STB7100_FDMA_SADDR_REG,
	.fdma_daddrn		= STB7100_FDMA_DADDR_REG,
	.fdma_req_ctln		= STB7100_FDMA_REQ_CTLn_REG,
	.fdma_cmd_sta		= FDMA2_CMD_MBOX_STAT_REG,
	.fdma_cmd_set		= FDMA2_CMD_MBOX_SET_REG,
	.fdma_cmd_clr		= FDMA2_CMD_MBOX_CLR_REG,
	.fdma_cmd_mask		= FDMA2_CMD_MBOX_MASK_REG,
	.fdma_int_sta		= FDMA2_INT_STAT_REG,
	.fdma_int_set		= FDMA2_INT_SET_REG,
	.fdma_int_clr		= FDMA2_INT_CLR_REG,
	.fdma_int_mask		= FDMA2_INT_MASK_REG,
	.fdma_sync_reg		= FDMA2_SYNCREG,
	.fdma_dmem_region	= STB7100_DMEM_OFFSET,
	.fdma_imem_region	= STB7100_IMEM_OFFSET,
};

static struct fdma_regs stb7109_fdma_regs = {
	.fdma_id		= FDMA2_ID,
	.fdma_ver		= FDAM2_VER,
	.fdma_en		= FDMA2_ENABLE_REG,
	.fdma_clk_gate		= FDMA2_CLOCKGATE,
	.fdma_rev_id		= FDMA2_REV_ID,
	.fdma_cmd_statn		= STB7109_FDMA_CMD_STATn_REG,
	.fdma_ptrn		= STB7109_FDMA_PTR_REG,
	.fdma_cntn		= STB7109_FDMA_COUNT_REG,
	.fdma_saddrn		= STB7109_FDMA_SADDR_REG,
	.fdma_daddrn		= STB7109_FDMA_DADDR_REG,
	.fdma_req_ctln		= STB7109_FDMA_REQ_CTLn_REG,
	.fdma_cmd_sta		= FDMA2_CMD_MBOX_STAT_REG,
	.fdma_cmd_set		= FDMA2_CMD_MBOX_SET_REG,
	.fdma_cmd_clr		= FDMA2_CMD_MBOX_CLR_REG,
	.fdma_cmd_mask		= FDMA2_CMD_MBOX_MASK_REG,
	.fdma_int_sta		= FDMA2_INT_STAT_REG,
	.fdma_int_set		= FDMA2_INT_SET_REG,
	.fdma_int_clr		= FDMA2_INT_CLR_REG,
	.fdma_int_mask		= FDMA2_INT_MASK_REG,
	.fdma_sync_reg		= FDMA2_SYNCREG,
	.fdma_dmem_region	= STB7109_DMEM_OFFSET,
	.fdma_imem_region	= STB7109_IMEM_OFFSET,
};

static struct fdma_platform_device_data stb7109_C2_fdma_plat_data = {
	.registers_ptr = &stb7109_fdma_regs,
	.min_ch_num = CONFIG_MIN_STM_DMA_CHANNEL_NR,
	.max_ch_num = CONFIG_MAX_STM_DMA_CHANNEL_NR,
	.fw_device_name = "stb7109_fdmav2.8.bin",
	.fw.data_reg = (unsigned long*)&STB7109_C2_DMEM_REGION,
	.fw.imem_reg = (unsigned long*)&STB7109_C2_IMEM_REGION,
	.fw.imem_fw_sz = STB7109_C2_IMEM_FIRMWARE_SZ,
	.fw.dmem_fw_sz = STB7109_C2_DMEM_FIRMWARE_SZ,
	.fw.dmem_len = STB7109_C2_DMEM_REGION_LENGTH,
	.fw.imem_len = STB7109_C2_IMEM_REGION_LENGTH
};

static struct fdma_platform_device_data stb7109_C3_fdma_plat_data = {
	.registers_ptr =(void*) &stb7109_fdma_regs,
	.min_ch_num = CONFIG_MIN_STM_DMA_CHANNEL_NR,
	.max_ch_num  =CONFIG_MAX_STM_DMA_CHANNEL_NR,
	.fw_device_name = "stb7109_fdmav3.0.bin",
	.fw.data_reg = (unsigned long*)&STB7109_C3_DMEM_REGION,
	.fw.imem_reg = (unsigned long*)&STB7109_C3_IMEM_REGION,
	.fw.imem_fw_sz = STB7109_C3_IMEM_FIRMWARE_SZ,
	.fw.dmem_fw_sz = STB7109_C3_DMEM_FIRMWARE_SZ,
	.fw.dmem_len = STB7109_C3_DMEM_REGION_LENGTH,
	.fw.imem_len = STB7109_C3_IMEM_REGION_LENGTH

};

static struct fdma_platform_device_data stb7100_Cx_fdma_plat_data = {
	.registers_ptr =(void*) &stb7100_fdma_regs,
	.min_ch_num = CONFIG_MIN_STM_DMA_CHANNEL_NR,
	.max_ch_num  =CONFIG_MAX_STM_DMA_CHANNEL_NR,
	.fw_device_name = "stb7100_fdmav2.8.bin",
	.fw.data_reg = (unsigned long*)&STB7100_DMEM_REGION,
	.fw.imem_reg = (unsigned long*)&STB7100_IMEM_REGION,
	.fw.imem_fw_sz = STB7100_IMEM_FIRMWARE_SZ,
	.fw.dmem_fw_sz = STB7100_DMEM_FIRMWARE_SZ,
	.fw.dmem_len = STB7100_DMEM_REGION_LENGTH,
	.fw.imem_len = STB7100_IMEM_REGION_LENGTH
};

#endif /* CONFIG_STM_DMA */

static struct platform_device fdma_710x_device = {
	.name		= "stmfdma",
	.id		= -1,
	.num_resources	= 2,
	.resource = (struct resource[2]) {
		[0] = {
			.start = STB7100_FDMA_BASE,
			.end   = STB7100_FDMA_BASE + 0x10000,
			.flags = IORESOURCE_MEM,
		},
		[1] = {
			.start = LINUX_FDMA_STB7100_IRQ_VECT,
			.end   = LINUX_FDMA_STB7100_IRQ_VECT,
			.flags = IORESOURCE_IRQ,
		},
	},
};

static void fdma_setup(int chip_7109, int chip_revision)
{
#ifdef CONFIG_STM_DMA
	if(chip_7109){
		switch (chip_revision) {
		case 1:
			BUG();
			break;
		case 2:
			fdma_710x_device.dev.platform_data =(void*) &stb7109_C2_fdma_plat_data;
			break;
		default:
			fdma_710x_device.dev.platform_data =(void*) &stb7109_C3_fdma_plat_data;
			break;
		}
	} else {
		/* 7100 */
		fdma_710x_device.dev.platform_data =(void*) &stb7100_Cx_fdma_plat_data;
	}
#endif
}

/* ALSA resources ---------------------------------------------------------- */

static struct resource alsa_710x_resource_pcm0[3] = {

	[0] = {/* allocatable channels*/
		/*.start = runtime dependant*/
		/*.end   = runtime dependant*/
		.flags 	= IORESOURCE_IRQ
	},
	[1]= {/*fdma reqline*/
		/*.start = runtime dependant*/
		/*.end   = runtime dependant*/
		.flags = IORESOURCE_IRQ
	},
	[2] = {/*rising or falling edge I2s clocking
		 1 == FALLING_EDGE
		 0 == RISING EDGE */
		 /*.start = runtime dependant*/
		 /*.end = runtime dependant*/
		.flags = IORESOURCE_IRQ
	}
};

static struct resource alsa_710x_resource_pcm1[3] = {

	[0] = {/* allocatable channels*/
		/*.start = runtime dependant*/
		/*.end   = runtime dependant*/
		.flags 	= IORESOURCE_IRQ,
	},
	[1]= {/*fdma reqline*/
		/*.start = runtime dependant*/
		/*.end   = runtime dependant*/
		.flags = IORESOURCE_IRQ,
	},
	[2] = {/*rising or falling edge I2s clocking
		 1 == FALLING_EDGE
		 0 == RISING EDGE */
		 /*.start = runtime dependant*/
		 /*.end = runtime dependant*/
		.flags = IORESOURCE_IRQ
	}
};

static struct resource alsa_710x_resource_spdif[2] = {

	[0] = {/*min allocatable channels*/
		.start = 2,
		.end   =2,
		.flags = IORESOURCE_IRQ
	},
	[1] = {/*fdma reqline*/
		/*.start = runtime dependant*/
		/*.end   = runtime dependant*/
		.flags = IORESOURCE_IRQ
	}
};

static struct resource alsa_710x_resource_cnv[2] = {

	[0] = {/*min allocatable channels*/
		.start = 10,
		.end   =10,
		.flags = IORESOURCE_IRQ,
	},
	[1] = {/*fdma reqline*/
		/*.start = runtime dependant*/
		/*.end   = runtime dependant*/
		.flags = IORESOURCE_IRQ,
	}
};

static struct resource alsa_710x_resource_pcmin[3] = {

	[0] = {/*min allocatable channels*/
		.start = 0,
		.end   = 0,
		.flags = IORESOURCE_IRQ,
	},
	[1] = {/*fdma reqline*/
		/*.start = runtime dependant*/
		/*.end   = runtime dependant*/
		.flags = IORESOURCE_IRQ,
	},
	[2] = {/*rising or falling edge I2s clocking
		 1 == FALLING_EDGE
		 0 == RISING EDGE */
		/*.start = runtime dependant*/
		/*.end   = runtime dependant*/
		.flags = IORESOURCE_IRQ,
	}
};

static struct platform_device alsa_710x_device_pcm0 = {
	.name			= "710x_ALSA_PCM0",
	.id			= -1,
	.num_resources		= ARRAY_SIZE(alsa_710x_resource_pcm0),
	.resource		= alsa_710x_resource_pcm0,
};

static struct platform_device alsa_710x_device_pcm1 = {
	.name			= "710x_ALSA_PCM1",
	.id			= -1,
	.num_resources		= ARRAY_SIZE(alsa_710x_resource_pcm1),
	.resource		= alsa_710x_resource_pcm1,
};

static struct platform_device alsa_710x_device_spdif = {
	.name			= "710x_ALSA_SPD",
	.id			= -1,
	.num_resources		= ARRAY_SIZE(alsa_710x_resource_spdif),
	.resource		= alsa_710x_resource_spdif,
};

static struct platform_device alsa_710x_device_cnv = {
	.name			= "710x_ALSA_CNV",
	.id			= -1,
	.num_resources		= ARRAY_SIZE(alsa_710x_resource_cnv),
	.resource		= alsa_710x_resource_cnv,
};

static struct platform_device alsa_710x_device_pcmin = {
	.name			= "710x_ALSA_PCMIN",
	.id			= -1,
	.num_resources		= ARRAY_SIZE(alsa_710x_resource_pcmin),
	.resource		= alsa_710x_resource_pcmin,
};

static struct platform_device *alsa_devices[] __initdata = {
	&alsa_710x_device_pcm0,
	&alsa_710x_device_pcm1,
 	&alsa_710x_device_spdif,
	&alsa_710x_device_cnv,
	&alsa_710x_device_pcmin,
};

void __init stx7100_configure_alsa(void)
{
	if (chip_7109) {
		switch (chip_revision) {
		case 1:
			alsa_710x_resource_pcm0[2].start =0;
			alsa_710x_resource_pcm0[2].end = 0;

			alsa_710x_resource_pcm1[2].start =0;
			alsa_710x_resource_pcm1[2].end = 0;

			alsa_710x_resource_pcmin[2].start = 1;
			alsa_710x_resource_pcmin[2].end =   1;
			break;
		case 2:
			alsa_710x_resource_pcm0[2].start =0;
			alsa_710x_resource_pcm0[2].end = 0;

			alsa_710x_resource_pcm1[2].start =0;
			alsa_710x_resource_pcm1[2].end = 0;

			alsa_710x_resource_pcmin[2].start = 0;
			alsa_710x_resource_pcmin[2].end =   0;
			break;
		default:
			/* 7109 cut >= 3.0 */
			alsa_710x_resource_pcm0[2].start =1;
			alsa_710x_resource_pcm0[2].end = 1;

			alsa_710x_resource_pcm1[2].start =1;
			alsa_710x_resource_pcm1[2].end = 1;

			alsa_710x_resource_pcmin[2].start = 0;
			alsa_710x_resource_pcmin[2].end =   0;
			break;
		}

		alsa_710x_resource_pcm0[0].start = 2;
		alsa_710x_resource_pcm0[0].end = 10;

		alsa_710x_resource_pcm1[0].start = 2;
		alsa_710x_resource_pcm1[0].end 	= 2;

		alsa_710x_resource_pcm0[1].start = STB7109_FDMA_REQ_PCM_0;
		alsa_710x_resource_pcm0[1].end = STB7109_FDMA_REQ_PCM_0;

		alsa_710x_resource_pcm1[1].start = STB7109_FDMA_REQ_PCM_1;
		alsa_710x_resource_pcm1[1].end = STB7109_FDMA_REQ_PCM_1;

		alsa_710x_resource_spdif[1].start = STB7109_FDMA_REQ_SPDIF;
		alsa_710x_resource_spdif[1].end =   STB7109_FDMA_REQ_SPDIF;

		alsa_710x_resource_cnv[0].start =2;
		alsa_710x_resource_cnv[0].end = 10;
		alsa_710x_resource_cnv[1].start = STB7109_FDMA_REQ_PCM_0;
		alsa_710x_resource_cnv[1].end = STB7109_FDMA_REQ_PCM_0;

		alsa_710x_resource_pcmin[1].start = STB7109_FDMA_REQ_PCM_READ;
		alsa_710x_resource_pcmin[1].end =   STB7109_FDMA_REQ_PCM_READ;
	} else {
		/* 7100 */
		if(chip_revision >=3){
			alsa_710x_resource_pcm0[0].start = 2;
			alsa_710x_resource_pcm0[0].end = 10;
			alsa_710x_resource_pcm1[0].start =2;
			alsa_710x_resource_pcm1[0].end 	= 2;
			alsa_710x_resource_cnv[0].start =2;
			alsa_710x_resource_cnv[0].end = 10;
		}
		else {
			alsa_710x_resource_pcm0[0].start = 10;
			alsa_710x_resource_pcm0[0].end = 10;
			alsa_710x_resource_pcm1[0].start = 10;
			alsa_710x_resource_pcm1[0].end 	= 10;
			alsa_710x_resource_cnv[0].start =10;
			alsa_710x_resource_cnv[0].end = 10;
		}
		alsa_710x_resource_pcm0[1].start = STB7100_FDMA_REQ_PCM_0;
		alsa_710x_resource_pcm0[1].end = STB7100_FDMA_REQ_PCM_0;

		alsa_710x_resource_pcm1[1].start = STB7100_FDMA_REQ_PCM_1;
		alsa_710x_resource_pcm1[1].end = STB7100_FDMA_REQ_PCM_1;

		alsa_710x_resource_spdif[1].start =  STB7100_FDMA_REQ_SPDIF;
		alsa_710x_resource_spdif[1].end =  STB7100_FDMA_REQ_SPDIF;

		alsa_710x_resource_cnv[1].start = STB7100_FDMA_REQ_PCM_0;
		alsa_710x_resource_cnv[1].end = STB7100_FDMA_REQ_PCM_0;

		alsa_710x_resource_pcmin[1].start = STB7100_FDMA_REQ_PCM_READ;
		alsa_710x_resource_pcmin[1].end =   STB7100_FDMA_REQ_PCM_READ;

		alsa_710x_resource_pcm0[2].start =0;
		alsa_710x_resource_pcm0[2].end = 0;

		alsa_710x_resource_pcm1[2].start =0;
		alsa_710x_resource_pcm1[2].end = 0;

		alsa_710x_resource_pcmin[2].start = 0;
		alsa_710x_resource_pcmin[2].end =   0;
	}

	platform_add_devices(alsa_devices, ARRAY_SIZE(alsa_devices));
}

/* SSC resources ----------------------------------------------------------- */

static struct resource ssc_resource[] = {
        [0] = {
		.start	= 0xB8040000,
		.end	= 0xB8040000 + 0x108,
		.flags	= IORESOURCE_MEM,
	},
        [1] = {
		.start	= 0xB8041000,
		.end	= 0xB8041000 + 0x108,
		.flags	= IORESOURCE_MEM,
	},
        [2] = {
		.start	= 0xB8042000,
		.end	= 0xB8042000 + 0x108,
		.flags	= IORESOURCE_MEM,
	},
        [3] = {
		.start	= 119,
		.end	= 119,
		.flags	= IORESOURCE_IRQ,
	},
        [4] = {
		.start	= 118,
		.end	= 118,
		.flags	= IORESOURCE_IRQ,
	},
        [5] = {
		.start	= 117,
		.end	= 117,
               .flags	= IORESOURCE_IRQ,
	},
};

static struct plat_ssc_pio_t ssc_pio[] = {
	{2, 0, 2, 1, 2, 2},
	{3, 0, 3, 1, 3, 2},
	{4, 0, 4, 1, 0xff, 0xff},
};

struct platform_device ssc_device = {
        .name = "ssc",
        .id = -1,
        .num_resources = ARRAY_SIZE(ssc_resource),
        .resource = ssc_resource,
};

void __init stx7100_configure_ssc(struct plat_ssc_data *data)
{
	int i;
	int capability;
	struct sysconf_field* ssc_sc;

	data->pio = ssc_pio;
	ssc_device.dev.platform_data = data;

	for (i=0, capability = data->capability;
	     i<3;
	     i++, capability >>= 2) {
		if (! (capability & ((SSC_SPI_CAPABILITY|SSC_I2C_CAPABILITY) << (i*2))))
			continue;

		if (i== 0) {
			ssc_sc = sysconf_claim(SYS_CFG, 7, 10, 10, "ssc");
			sysconf_write(ssc_sc, 0);
		}

		ssc_sc = sysconf_claim(SYS_CFG, 7, i+1, i+1, "ssc");
		sysconf_write(ssc_sc,
			      capability & SSC_I2C_CAPABILITY ? 0 : 1);
	}

	platform_device_register(&ssc_device);
}

/* SATA resources ---------------------------------------------------------- */

static struct resource sata_resource[]= {
	[0] = {
		.start = 0x18000000 + 0x01209000,
		.end   = 0x18000000 + 0x01209000 + 0xfff,
		.flags = IORESOURCE_MEM
	},
	[1] = {
		.start = 0xaa,
		.flags = IORESOURCE_IRQ
	},
};

static struct plat_sata_data sata_private_info;

static struct platform_device sata_device = {
	.name		= "sata_stm",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(sata_resource),
	.resource	= sata_resource,
       .dev = {
               .platform_data = &sata_private_info,
       }
};

void __init stx7100_configure_sata(void)
{
	if ((! chip_7109) && (chip_revision == 1)) {
		/* 7100 cut 1.x */
		sata_private_info.phy_init = 0x0013704A;
	} else {
		/* 7100 cut 2.x and cut 3.x and 7109 */
		sata_private_info.phy_init = 0x388fc;
	}

	if ((! chip_7109) || (chip_7109 && (chip_revision == 1))) {
		sata_private_info.only_32bit = 1;
		sata_private_info.pc_glue_logic_init = 0x1ff;
	} else {
		sata_private_info.only_32bit = 0;
		sata_private_info.pc_glue_logic_init = 0x100ff;
	}

	platform_device_register(&sata_device);
}

/* Ethernet MAC resources -------------------------------------------------- */

static struct sysconf_field *mac_speed_sc;

static void fix_mac_speed(void* priv, unsigned int speed)
{
	sysconf_write(mac_speed_sc, (speed == SPEED_100) ? 0 : 1);
}

/* Hopefully I can remove this now */
static void stb7109eth_hw_setup_null(void)
{
}

static struct plat_stmmacenet_data eth7109_private_data = {
	.bus_id = 0,
	.pbl = 1,
	.fix_mac_speed = fix_mac_speed,
	.hw_setup = stb7109eth_hw_setup_null,
};

static struct platform_device stb7109eth_device = {
        .name           = "stmmaceth",
        .id             = 0,
        .num_resources  = 2,
        .resource       = (struct resource[]) {
        	{
	                .start = 0x18110000,
        	        .end   = 0x1811ffff,
                	.flags  = IORESOURCE_MEM,
        	},
        	{
			.name   = "macirq",
                	.start  = 133,
                	.end    = 133,
                	.flags  = IORESOURCE_IRQ,
        	},
	},
	.dev = {
		.platform_data = &eth7109_private_data,
	}
};

void stx7100_configure_ethernet(int rmii_mode, int ext_clk, int phy_bus)
{
	struct sysconf_field *sc;

	eth7109_private_data.bus_id = phy_bus;

	/* DVO_ETH_PAD_DISABLE and ETH_IF_ON */
	sc = sysconf_claim(SYS_CFG, 7, 16, 17, "stmmac");
	sysconf_write(sc, 3);

	/* RMII_MODE */
	sc = sysconf_claim(SYS_CFG, 7, 18, 18, "stmmac");
	sysconf_write(sc, rmii_mode ? 1 : 0);

	/* PHY_CLK_EXT */
	sc = sysconf_claim(SYS_CFG, 7, 19, 19, "stmmac");
	sysconf_write(sc, ext_clk ? 1 : 0);

	/* MAC_SPEED_SEL */
	mac_speed_sc = sysconf_claim(SYS_CFG, 7, 20, 20, "stmmac");

	/* Remove the PHY clk */
	stpio_request_pin(3, 7, "stmmac EXTCLK", STPIO_ALT_OUT);

	/* Configure the ethernet MAC PBL depending on the cut of the chip */
	if (chip_7109){
	       if (chip_revision == 1){
			eth7109_private_data.pbl = 1;
		} else {
			eth7109_private_data.pbl = 32;
		}
        }

	platform_device_register(&stb7109eth_device);
}

/* PWM resources ----------------------------------------------------------- */

static struct resource stm_pwm_resource[]= {
	[0] = {
		.start	= 0x18010000,
		.end	= 0x18010000 + 0x67,
		.flags	= IORESOURCE_MEM
	},
	[1] = {
		.start	= 126,
		.flags	= IORESOURCE_IRQ
	}
};

static struct platform_device stm_pwm_device = {
	.name		= "stm-pwm",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(stm_pwm_resource),
	.resource	= stm_pwm_resource,
};

void stx7100_configure_pwm(struct plat_stm_pwm_data *data)
{
	stm_pwm_device.dev.platform_data = data;

	if (data->flags & PLAT_STM_PWM_OUT0) {
		if (sys_cfg7_0 == NULL)
			sys_cfg7_0 = sysconf_claim(SYS_CFG, 7, 0, 0, "pwm");
		sysconf_write(sys_cfg7_0, 0);
		stpio_request_pin(4, 6, "PWM", STPIO_ALT_OUT);
	}

	if (data->flags & PLAT_STM_PWM_OUT1) {
		stpio_request_pin(4, 7, "PWM", STPIO_ALT_OUT);
	}

	platform_device_register(&stm_pwm_device);
}

/* LiRC resources ---------------------------------------------------------- */

void __init stb7100_configure_lirc(void)
{
	/* This is a place holder for when we do have LIRC */

	/* Ideally the PIO claiming should be moved into the driver. */

	/* Configure the pio pins for LIRC */
	stpio_request_pin(3, 3, "IR", STPIO_IN);
	stpio_request_pin(3, 4, "IR", STPIO_IN);
	stpio_request_pin(3, 5, "IR", STPIO_ALT_OUT);
	stpio_request_pin(3, 6, "IR", STPIO_ALT_OUT);
}

/* ASC resources ----------------------------------------------------------- */

static struct platform_device stm_stasc_devices[] = {
	STASC_DEVICE(0x18030000, 123, 0, 0, 1, 4, 7), /* oe pin: 6 */
	STASC_DEVICE(0x18031000, 122, 1, 0, 1, 4, 5), /* oe pin: 6 */
	STASC_DEVICE(0x18032000, 121, 4, 3, 2, 4, 5),
	STASC_DEVICE(0x18033000, 120, 5, 0, 1, 2, 3),
};

/* the serial console device */
struct platform_device *asc_default_console_device;

/* Platform devices to register */
static struct platform_device *stasc_configured_devices[ARRAY_SIZE(stm_stasc_devices)] __initdata;
static int stasc_configured_devices_count __initdata = 0;

/* Configure the ASC's for this board.
 * This has to be called before console_init().
 */
void __init stb7100_configure_asc(const int *ascs, int num_ascs, int console)
{
	int i;
	struct platform_device *pdev;

	for (i=0; i<num_ascs; i++) {
		pdev = &stm_stasc_devices[ascs[i]];

		switch (ascs[i]) {
		case 2:
			if (sys_cfg7_0 == NULL)
				sys_cfg7_0 = sysconf_claim(SYS_CFG, 7, 0, 0, "asc");
			sysconf_write(sys_cfg7_0, 0);
			break;
		}

		pdev->id = i;
		stasc_configured_devices[stasc_configured_devices_count++] = pdev;
	}

	asc_default_console_device = stasc_configured_devices[console];
}

/* Add platform device as configured by board specific code */
static int __init stb7100_add_asc(void)
{
	return platform_add_devices(stasc_configured_devices,
				    stasc_configured_devices_count);
}
arch_initcall(stb7100_add_asc);

/* Early resources (sysconf and PIO) --------------------------------------- */

static struct platform_device sysconf_device = {
	.name		= "sysconf",
	.id		= -1,
	.num_resources	= 1,
	.resource	= (struct resource[]) {
		{
			.start	= 0x19001000,
			.end	= 0x19001000 + 0x100,
			.flags	= IORESOURCE_MEM
		}
	},
	.dev = {
		.platform_data = &(struct plat_sysconf_data) {
			.sys_device_offset = 0,
			.sys_sta_offset = 8,
			.sys_cfg_offset = 0x100,
		}
	}
};

static struct platform_device stpio_devices[] = {
	STPIO_DEVICE(0, 0x18020000, 80),
	STPIO_DEVICE(1, 0x18021000, 84),
	STPIO_DEVICE(2, 0x18022000, 88),
	STPIO_DEVICE(3, 0x18023000, 115),
	STPIO_DEVICE(4, 0x18024000, 114),
	STPIO_DEVICE(5, 0x18025000, 113),
};

/* Initialise devices which are required early in the boot process. */
void __init stx7100_early_device_init(void)
{
	struct sysconf_field *sc;
	unsigned long devid;

	/* Initialise PIO and sysconf drivers */

	sysconf_early_init(&sysconf_device);
	stpio_early_init(stpio_devices, ARRAY_SIZE(stpio_devices));

	sc = sysconf_claim(SYS_DEV, 0, 0, 31, "devid");
	devid = sysconf_read(sc);
	chip_7109 = (((devid >> 12) & 0x3ff) == 0x02c);
	chip_revision = (devid >> 28) + 1;

	printk("%s version %ld.x\n",
	       chip_7109 ? "STx7109" : "STx7100", chip_revision);

	sc = sysconf_claim(SYS_STA, 9, 0, 7, "devid");
	devid = sysconf_read(sc);
	printk("Chip version %ld.%ld\n", (devid >> 4)+1, devid & 0xf);

	/* Configure the ST40 RTC to source its clock from clockgenB.
	 * In theory this should be board specific, but so far nobody
	 * has ever done this. */
	sc = sysconf_claim(SYS_CFG, 8, 1, 1, "rtc");
	sysconf_write(sc, 1);

	/* We haven't configured the LPC, so the sleep instruction may
	 * do bad things. Thus we disable it here. */
	disable_hlt();
}

static void __init pio_late_setup(void)
{
	int i;
	struct platform_device *pdev = stpio_devices;

	for (i=0; i<ARRAY_SIZE(stpio_devices); i++,pdev++) {
		platform_device_register(pdev);
	}
}

/* Late resources ---------------------------------------------------------- */

static struct platform_device *stx710x_devices[] __initdata = {
	&sci_device,
	&wdt_device,
	&rtc_device,
	&fdma_710x_device,
	&sysconf_device,
};

static int __init stx710x_devices_setup(void)
{
	fdma_setup(chip_7109, chip_revision);
	pio_late_setup();

	return platform_add_devices(stx710x_devices,
				    ARRAY_SIZE(stx710x_devices));
}
device_initcall(stx710x_devices_setup);

/* Interrupt initialisation ------------------------------------------------ */

enum {
	UNUSED = 0,

	/* interrupt sources */
	IRL0, IRL1, IRL2, IRL3, /* only IRLM mode described here */
	TMU0, TMU1, TMU2_TUNI, TMU2_TICPI,
	RTC_ATI, RTC_PRI, RTC_CUI,
	SCIF_ERI, SCIF_RXI, SCIF_BRI, SCIF_TXI,
	WDT,
	HUDI,

	SATA_DMAC, SATA_HOSTC,
	PIO0, PIO1, PIO2,
	PIO5, PIO4, PIO3, MTP,			/* Group 0 */
	SSC2, SSC1, SSC0,			/* Group 1 */
	UART3, UART2, UART1, UART0,		/* Group 2 */
	IRB_WAKEUP, IRB, PWM, MAFE,		/* Group 3 */
	DISEQC, DAA, TTXT,			/* Group 4 */
	EMPI, ETH_MAC, TS_MERGER,		/* Group 5 */
	ST231_DELTA, ST231_AUD, DCXO, PTI1,	/* Group 6 */
	FDMA_MBOX, FDMA_GP0, I2S2SPDIF, CPXM,	/* Group 7 */
	PCMPLYR0, PCMPLYR1, PCMRDR, SPDIFPLYR,	/* Group 8 */
	MPEG2, DELTA_PRE0, DELTA_PRE1, DELTA_MBE,	/* Group 9 */
	VDP_FIFO_EMPTY, VDP_END_PROC, VTG1, VTG2,	/* Group 10 */
	BDISP_AQ1, DVP, HDMI, HDCP,			/* Group 11 */
	PTI, PDES_ESA0, PDES, PRES_READ_CW,		/* Group 12 */
	SIG_CHK, TKDMA, CRIPTO_SIG_DMA, CRIPTO_SIG_CHK,	/* Group 13 */
	OHCI, EHCI, SATA, BDISP_CQ1,			/* Group 14 */
	ICAM3_KTE, ICAM3, MES_LMI_VID, MES_LMI_SYS,	/* Group 15 */

	/* interrupt groups */
	TMU2, RTC, SCIF,
	SATA_SPLIT,
	GROUP0, GROUP1, GROUP2, GROUP3,
	GROUP4, GROUP5, GROUP6, GROUP7,
	GROUP8, GROUP9, GROUP10, GROUP11,
	GROUP12, GROUP13, GROUP14, GROUP15,
};

static struct intc_vect vectors[] = {
	INTC_VECT(TMU0, 0x400), INTC_VECT(TMU1, 0x420),
	INTC_VECT(TMU2_TUNI, 0x440), INTC_VECT(TMU2_TICPI, 0x460),
	INTC_VECT(RTC_ATI, 0x480), INTC_VECT(RTC_PRI, 0x4a0),
	INTC_VECT(RTC_CUI, 0x4c0),
	INTC_VECT(SCIF_ERI, 0x4e0), INTC_VECT(SCIF_RXI, 0x500),
	INTC_VECT(SCIF_BRI, 0x520), INTC_VECT(SCIF_TXI, 0x540),
	INTC_VECT(WDT, 0x560),
	INTC_VECT(HUDI, 0x600),

	INTC_VECT(SATA_DMAC, 0xa20), INTC_VECT(SATA_HOSTC, 0xa40),
	INTC_VECT(PIO0, 0xc00), INTC_VECT(PIO1, 0xc80), INTC_VECT(PIO2, 0xd00),
	INTC_VECT(MTP, 0x1000),INTC_VECT(PIO5, 0x1020),
	INTC_VECT(PIO4, 0x1040), INTC_VECT(PIO3, 0x1060),
	INTC_VECT(SSC2, 0x10a0),
	INTC_VECT(SSC1, 0x10c0), INTC_VECT(SSC0, 0x10e0),
	INTC_VECT(UART3, 0x1100), INTC_VECT(UART2, 0x1120),
	INTC_VECT(UART1, 0x1140), INTC_VECT(UART0, 0x1160),
	INTC_VECT(IRB_WAKEUP, 0x1180), INTC_VECT(IRB, 0x11a0),
	INTC_VECT(PWM, 0x11c0), INTC_VECT(MAFE, 0x11e0),
	INTC_VECT(DISEQC, 0x1220),
	INTC_VECT(DAA, 0x1240), INTC_VECT(TTXT, 0x1260),
	INTC_VECT(EMPI, 0x1280), INTC_VECT(ETH_MAC, 0x12a0),
	INTC_VECT(TS_MERGER, 0x12c0),
	INTC_VECT(ST231_DELTA, 0x1300), INTC_VECT(ST231_AUD, 0x1320),
	INTC_VECT(DCXO, 0x1340), INTC_VECT(PTI1, 0x1360),
	INTC_VECT(FDMA_MBOX, 0x1380), INTC_VECT(FDMA_GP0, 0x13a0),
	INTC_VECT(I2S2SPDIF, 0x13c0), INTC_VECT(CPXM, 0x13e0),
	INTC_VECT(PCMPLYR0, 0x1400), INTC_VECT(PCMPLYR1, 0x1420),
	INTC_VECT(PCMRDR, 0x1440), INTC_VECT(SPDIFPLYR, 0x1460),
	INTC_VECT(MPEG2, 0x1480), INTC_VECT(DELTA_PRE0, 0x14a0),
	INTC_VECT(DELTA_PRE1, 0x14c0), INTC_VECT(DELTA_MBE, 0x14e0),
	INTC_VECT(VDP_FIFO_EMPTY, 0x1500), INTC_VECT(VDP_END_PROC, 0x1520),
	INTC_VECT(VTG1, 0x1540), INTC_VECT(VTG2, 0x1560),
	INTC_VECT(BDISP_AQ1, 0x1580), INTC_VECT(DVP, 0x15a0),
	INTC_VECT(HDMI, 0x15c0), INTC_VECT(HDCP, 0x15e0),
	INTC_VECT(PTI, 0x1600), INTC_VECT(PDES_ESA0, 0x1620),
	INTC_VECT(PDES, 0x1640), INTC_VECT(PRES_READ_CW, 0x1660),
	INTC_VECT(SIG_CHK, 0x1680), INTC_VECT(TKDMA, 0x16a0),
	INTC_VECT(CRIPTO_SIG_DMA, 0x16c0), INTC_VECT(CRIPTO_SIG_CHK, 0x16e0),
	INTC_VECT(OHCI, 0x1700), INTC_VECT(EHCI, 0x1720),
	INTC_VECT(SATA, 0x1740), INTC_VECT(BDISP_CQ1, 0x1760),
	INTC_VECT(ICAM3_KTE, 0x1780), INTC_VECT(ICAM3, 0x17a0),
	INTC_VECT(MES_LMI_VID, 0x17c0), INTC_VECT(MES_LMI_SYS, 0x17e0)
};

static struct intc_group groups[] = {
	INTC_GROUP(TMU2, TMU2_TUNI, TMU2_TICPI),
	INTC_GROUP(RTC, RTC_ATI, RTC_PRI, RTC_CUI),
	INTC_GROUP(SCIF, SCIF_ERI, SCIF_RXI, SCIF_BRI, SCIF_TXI),

	INTC_GROUP(SATA_SPLIT, SATA_DMAC, SATA_HOSTC),
	INTC_GROUP(GROUP0, PIO5, PIO4, PIO3, MTP),
	INTC_GROUP(GROUP1, SSC2, SSC1, SSC0),
	INTC_GROUP(GROUP2, UART3, UART2, UART1, UART0),
	INTC_GROUP(GROUP3, IRB_WAKEUP, IRB, PWM, MAFE),
	INTC_GROUP(GROUP4, DISEQC, DAA, TTXT),
	INTC_GROUP(GROUP5, EMPI, ETH_MAC, TS_MERGER),
	INTC_GROUP(GROUP6, ST231_DELTA, ST231_AUD, DCXO, PTI1),
	INTC_GROUP(GROUP7, FDMA_MBOX, FDMA_GP0, I2S2SPDIF, CPXM),
	INTC_GROUP(GROUP8, PCMPLYR0, PCMPLYR1, PCMRDR, SPDIFPLYR),
	INTC_GROUP(GROUP9, MPEG2, DELTA_PRE0, DELTA_PRE1, DELTA_MBE),
	INTC_GROUP(GROUP10, VDP_FIFO_EMPTY, VDP_END_PROC, VTG1, VTG2),
	INTC_GROUP(GROUP11, BDISP_AQ1, DVP, HDMI, HDCP),
	INTC_GROUP(GROUP12, PTI, PDES_ESA0, PDES, PRES_READ_CW),
	INTC_GROUP(GROUP13, SIG_CHK, TKDMA, CRIPTO_SIG_DMA, CRIPTO_SIG_CHK),
	INTC_GROUP(GROUP14, OHCI, EHCI, SATA, BDISP_CQ1),
	INTC_GROUP(GROUP15, ICAM3_KTE, ICAM3, MES_LMI_VID, MES_LMI_SYS),
};

static struct intc_prio priorities[] = {
	INTC_PRIO(SCIF, 3),
};

static struct intc_prio_reg prio_registers[] = {
					   /*   15-12, 11-8,  7-4,   3-0 */
	{ 0xffd00004, 0, 16, 4, /* IPRA */     { TMU0, TMU1, TMU2,   RTC } },
	{ 0xffd00008, 0, 16, 4, /* IPRB */     {  WDT,    0, SCIF,     0 } },
	{ 0xffd0000c, 0, 16, 4, /* IPRC */     {    0,    0,    0,  HUDI } },
	{ 0xffd00010, 0, 16, 4, /* IPRD */     { IRL0, IRL1,  IRL2, IRL3 } },
						/* 31-28,   27-24,   23-20,   19-16 */
						/* 15-12,    11-8,     7-4,     3-0 */
	{ 0xb9001300, 0, 32, 4, /* INTPRI00 */ {       0,       0,    PIO2,    PIO1,
						    PIO0,       0, SATA_SPLIT,    0 } },
	{ 0xb9001304, 0, 32, 4, /* INTPRI04 */ {  GROUP7,  GROUP6,  GROUP5,  GROUP4,
						  GROUP3,  GROUP2,  GROUP1,  GROUP0 } },
	{ 0xb9001308, 0, 32, 4, /* INTPRI08 */ { GROUP15, GROUP14, GROUP13, GROUP12,
						 GROUP11, GROUP10,  GROUP9,  GROUP8 } },
};

static struct intc_mask_reg mask_registers[] = {
	{ 0xb9001340, 0xb9001360, 32, /* INTMSK00 / INTMSKCLR00 */
	  { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,	/* 31..16 */
	    0, PIO2, PIO1, PIO0,				/* 15..12 */
	    0, 0, 0, 0,						/* 11...8 */
	    0, 0, 0, 0,						/*  7...4 */
	    0, SATA_HOSTC, SATA_DMAC, 0 } },			/*  3...0 */
	{ 0xb9001344, 0xb9001364, 32, /* INTMSK04 / INTMSKCLR04 */
	  { CPXM, I2S2SPDIF, FDMA_GP0, FDMA_MBOX,		/* 31..28 */
	    PTI1, DCXO, ST231_AUD, ST231_DELTA,			/* 27..24 */
	    0, TS_MERGER, ETH_MAC, EMPI,			/* 23..20 */
	    TTXT, DAA, DISEQC, 0,				/* 19..16 */
	    MAFE, PWM, IRB, IRB_WAKEUP, 			/* 15..12 */
	    UART0, UART1, UART2, UART3,				/* 11...8 */
	    SSC0, SSC1, SSC2, 0,				/*  7...4 */
	    PIO3, PIO4, PIO5, MTP } },				/*  3...0 */
	{ 0xb9001348, 0xb9001368, 32, /* INTMSK08 / INTMSKCLR08 */
	  { MES_LMI_SYS, MES_LMI_VID, ICAM3, ICAM3_KTE, 	/* 31..28 */
	    BDISP_CQ1, SATA, EHCI, OHCI,			/* 27..24 */
	    CRIPTO_SIG_CHK, CRIPTO_SIG_DMA, TKDMA, SIG_CHK,	/* 23..20 */
	    PRES_READ_CW, PDES, PDES_ESA0, PTI,			/* 19..16 */
	    HDCP, HDMI, DVP, BDISP_AQ1,				/* 15..12 */
	    VTG2, VTG1, VDP_END_PROC, VDP_FIFO_EMPTY,		/* 11...8 */
	    DELTA_MBE, DELTA_PRE1, DELTA_PRE0, MPEG2,		/*  7...4 */
	    SPDIFPLYR, PCMRDR, PCMPLYR1, PCMPLYR0 } }		/*  3...0 */
};

static DECLARE_INTC_DESC(intc_desc, "stx7100", vectors, groups,
			 priorities, mask_registers, prio_registers, NULL);

static struct intc_vect vectors_irlm[] = {
	INTC_VECT(IRL0, 0x240), INTC_VECT(IRL1, 0x2a0),
	INTC_VECT(IRL2, 0x300), INTC_VECT(IRL3, 0x360),
};

static DECLARE_INTC_DESC(intc_desc_irlm, "stx7100_irlm", vectors_irlm, NULL,
			 priorities, NULL, prio_registers, NULL);

void __init plat_irq_setup(void)
{
	struct sysconf_field *sc;

	/* Configure the external interrupt pins as inputs */
	sc = sysconf_claim(SYS_CFG, 10, 0, 3, "irq");
	sysconf_write(sc, 0xf);

	register_intc_controller(&intc_desc);
}

#define INTC_ICR	0xffd00000UL
#define INTC_ICR_IRLM   (1<<7)

void __init plat_irq_setup_pins(int mode)
{
	switch (mode) {
	case IRQ_MODE_IRQ: /* individual interrupt mode for IRL3-0 */
		register_intc_controller(&intc_desc_irlm);
		ctrl_outw(ctrl_inw(INTC_ICR) | INTC_ICR_IRLM, INTC_ICR);
		break;
	default:
		BUG();
	}
}
