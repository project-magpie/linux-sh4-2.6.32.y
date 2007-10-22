/*
 * arch/sh/boards/st/hms1/setup.c
 *
 * Copyright (C) 2006 STMicroelectronics Limited
 * Author: Stuart Menefy (stuart.menefy@st.com)
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 *
 * HMS1 board support.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/stm/pio.h>
#include <linux/stm/soc.h>
#include <asm/io.h>
#include <linux/stm/710x_fdma.h>
#include <linux/stm/7100_fdma2_firmware.h>
#include <linux/stm/7109_cut2_fdma2_firmware.h>
#include <linux/stm/7109_cut3_fdma2_firmware.h>

#define SYSCONF_BASE 0xb9001000
#define SYSCONF_DEVICEID	(SYSCONF_BASE + 0x000)
#define SYSCONF_SYS_STA(n)	(SYSCONF_BASE + 0x008 + ((n) * 4))
#define SYSCONF_SYS_CFG(n)	(SYSCONF_BASE + 0x100 + ((n) * 4))

/*
 * Initialize the board
 */
void __init platform_setup(void)
{
	unsigned long sysconf;
	unsigned long chip_revision, chip_7109;

	printk("HMS1 board initialisation\n");

	sysconf = ctrl_inl(SYSCONF_DEVICEID);
	chip_7109 = (((sysconf >> 12) & 0x3ff) == 0x02c);
	chip_revision = (sysconf >> 28) +1;

	if (chip_7109)
		printk("STb7109 version %ld.x\n", chip_revision);
	else
		printk("STb7100 version %ld.x\n", chip_revision);

	sysconf = ctrl_inl(SYSCONF_SYS_CFG(7));

	/* SCIF_PIO_OUT_EN=0 */
	/* Route UART2 and PWM to PIO4 instead of SCIF */
	sysconf &= ~(1<<0);

	/* Set SSC2_MUX_SEL = 0 */
	/* Treat SSC2 as I2C instead of SSC */
	sysconf &= ~(1<<3);

	ctrl_outl(sysconf, SYSCONF_SYS_CFG(7));

	/* Permanently enable Flash VPP */
	{
		static struct stpio_pin *pin;
		pin = stpio_request_pin(2,5, "VPP", STPIO_OUT);
		stpio_set_pin(pin, 1);
	}

	/* The ST40RTC sources its clock from clock */
	/* generator B */
	sysconf = ctrl_inl(SYSCONF_SYS_CFG(8));
	ctrl_outl(sysconf | 0x2, SYSCONF_SYS_CFG(8));

	/* Work around for USB over-current detection chip being
	 * active low, and the 710x being active high.
	 *
	 * This test is wrong for 7100 cut 3.0 (which needs the work
	 * around), but as we can't reliably determine the minor
	 * revision number, hard luck, this works for most people.
	 */
	if ( ( chip_7109 && (chip_revision < 2)) ||
	     (!chip_7109 && (chip_revision < 3)) ) {
		static struct stpio_pin *pin;
		pin = stpio_request_pin(5,6, "USBOC", STPIO_OUT);
		stpio_set_pin(pin, 0);
	}

	/* Currently all STB1 chips have problems with the sleep instruction,
	 * so disable it here.
	 */
	disable_hlt();


	/* Configure the pio pins for LIRC */
	stpio_request_pin(3, 3, "IR", STPIO_IN);
	stpio_request_pin(3, 4, "IR", STPIO_IN);
	stpio_request_pin(3, 5, "IR", STPIO_ALT_OUT);
	stpio_request_pin(3, 6, "IR", STPIO_ALT_OUT);

#ifdef CONFIG_STM_PWM
	stpio_request_pin(4, 7, "PWM", STPIO_ALT_OUT);
#endif
}

const char *get_system_type(void)
{
	return "HMS1 board";
}

static struct resource ssc_resource[] = {
	[0] = {
		.start = 0xB8040000,
		.end = 0xB8040000 + 0x108,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = 0xB8041000,
		.end = 0xB8041000 + 0x108,
		.flags = IORESOURCE_MEM,
	},
	[2] = {
		.start = 0xB8042000,
		.end = 0xB8042000 + 0x108,
		.flags = IORESOURCE_MEM,
	},
	[3] = {
		.start = 119,
		.end = 119,
		.flags = IORESOURCE_IRQ,
	},
	[4] = {
		.start = 118,
		.end = 118,
		.flags = IORESOURCE_IRQ,
	},
	[5] = {
		.start = 117,
		.end = 117,
		.flags = IORESOURCE_IRQ,
	},
};

static struct plat_ssc_pio_t ssc_pio[] = {
	{2, 0, 2, 1, 2, 2},
	{3, 0, 3, 1, 3, 2},
	{4, 0, 4, 1, 0xff, 0xff},
};
static struct plat_ssc_data ssc_private_info = {
	.capability  = 0x1f,
	.pio         = ssc_pio
};
struct platform_device ssc_device = {
	.name = "ssc",
	.id = -1,
	.num_resources = ARRAY_SIZE(ssc_resource),
	.resource = ssc_resource,
	.dev = {
		.platform_data = &ssc_private_info
	}
};

static struct plat_lirc_data lirc_private_info = {
	/* For the 7100, the clock settings will be calculated by the driver
	 * from the system clock
	 */
	.irbclock	= 0, /* use current_cpu data */
	.irbclkdiv	= 0, /* automatically calculate */
	.irbperiodmult	= 0,
	.irbperioddiv	= 0,
	.irbontimemult	= 0,
	.irbontimediv	= 0,
	.irbrxmaxperiod = 0x5000,
	.irbversion	= 2,
	.sysclkdiv	= 1,
	.rxpolarity	= 1,
	.subcarrwidth	= 50
};

static struct resource st40_ohci_resources[] = {
	/*this lot for the ohci block*/
	[0] = {
		.start = 0xb9100000 + 0xffc00,
		.end   = 0xb9100000 +0xffcff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = 168,
		.end   = 168,
		.flags = IORESOURCE_IRQ,
	}
};

static struct resource st40_ehci_resources[] = {
	/*now this for the ehci*/
	[0] =  {
		.start = 0xb9100000 + 0xffe00,
		.end = 0xb9100000 + 0xffeff,
		.flags = IORESOURCE_MEM,
	},
	[1] = {
		.start = 169,
		.end   = 169,
		.flags = IORESOURCE_IRQ,
	},
};

/* Watchdog timer parameters */
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


static struct resource lirc_resource[]= {
	/* This is the main LIRC register block, as defined by the spec */
	[0] = {
		.start = 0x18018000,
		.end   = 0x18018000 + 0xa0,
		.flags = IORESOURCE_MEM
	},
	/* The LIRC block has one interrupt */
	[1] = {
		.start = 125,
		.end   = 125,
		.flags = IORESOURCE_IRQ
	},
};

static u64 st40_dma_mask = 0xfffffff;

static struct platform_device  st40_ohci_devices = {
	.name = "ST40-ohci",
	.id=1,
	.dev = {
		.dma_mask = &st40_dma_mask,
		.coherent_dma_mask = 0xffffffful,
	},
	.num_resources = ARRAY_SIZE(st40_ohci_resources),
	.resource = st40_ohci_resources,
};

static struct platform_device  st40_ehci_devices = {
	.name = "ST40-ehci",
	.id=2,
	.dev = {
		.dma_mask = &st40_dma_mask,
		.coherent_dma_mask = 0xffffffful,
	},
	.num_resources = ARRAY_SIZE(st40_ehci_resources),
	.resource = st40_ehci_resources,
};

static struct platform_device lirc_device = {
	.name		= "lirc",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(lirc_resource),
	.resource	= lirc_resource,
	.dev = {
		.platform_data = &lirc_private_info
	}
};

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

static struct plat_sata_data sata_private_info = {
	.phy_init	= 0,
};

static struct platform_device sata_device = {
	.name		= "stm-sata",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(sata_resource),
	.resource	= sata_resource,
	.dev = {
		.platform_data = &sata_private_info,
	}
};

static struct resource rtc_resource[]= {
	[0] = {
		.start = 0xffc80000,
		.end   = 0xffc80000 + 0x40,
		.flags = IORESOURCE_MEM
	},
/* Be careful the
 * arch/sh/kernel/cpu/irq_ipr.c
 * must be update with the right value
 */
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

static struct plat_stm_pwm_data pwm_private_info = {
	.flags		= PLAT_STM_PWM_OUT1,
};

static struct platform_device stm_pwm_device = {
	.name		= "stm-pwm",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(stm_pwm_resource),
	.resource	= stm_pwm_resource,
	.dev = {
		.platform_data = &pwm_private_info,
	}
};

/*
 *  FDMA parameters
 */

static fdmareq_RequestConfig_t stb7100_fdma_req_config[] ={
/*=========================== 7100 ============================================*/

/*
		Req  			RnW,    Opcode,  Transfer Incr addr,   Hold_off Initiator), */
/*           	1-32     				 cnt 1-4  On/Off       0-2        Used) */

/*0*/{STB7100_FDMA_REQ_SPDIF_TEST,  		WRITE,  OPCODE_4,  1,     ENABLE_FLG,  0,     1 },/* SPDIF Testing */
/*1*/{STB7100_FDMA_REQ_NOT_CONN_1,   		UNUSED, UNUSED,    1,     UNUSED,      0,     1 },/* NOT CONNECTED */
/*2*/{STB7100_FDMA_REQ_NOT_CONN_2,   		UNUSED, UNUSED,    1,     UNUSED,      0,     1 },/* NOT CONNECTED */
/*3*/{STB7100_FDMA_REQ_VIDEO_HDMI,   		READ,   OPCODE_8,  1,     DISABLE_FLG, 0,     1 },/* Video HDMI */
/*4*/{STB7100_FDMA_REQ_DISEQC_HALF_EMPTY,	WRITE,  OPCODE_4,  2,     DISABLE_FLG, 0,     1 },/* DiseqC half empty */
/*5*/{STB7100_FDMA_REQ_DISEQC_HALF_FULL, 	READ,   OPCODE_4,  2,     DISABLE_FLG, 0,     1 },/* DiseqC half full */
/*6*/{STB7100_FDMA_REQ_SH4_SCIF_RX,   		READ,   OPCODE_4,  2,     DISABLE_FLG, 0,     1 },  /* SH4/SCIF */
/*7*/{STB7100_FDMA_REQ_SH4_SCIF_TX,   		WRITE,  OPCODE_4,  2,     DISABLE_FLG, 0,     1 },  /* SH4/SCIF */
/*8*/{STB7100_FDMA_REQ_SSC_0_RX,   		READ,   OPCODE_2,  4,     DISABLE_FLG, 0,     1 },  /* SSC 0 rxbuff full */
/*9*/{STB7100_FDMA_REQ_SSC_1_RX,   		READ,   OPCODE_2,  4,     DISABLE_FLG, 0,     1 },  /* SSC 1 rxbuff full */
/*10*/{STB7100_FDMA_REQ_SSC_2_RX,  		READ,   OPCODE_2,  4,     DISABLE_FLG, 0,     1 },  /* SSC 2 rxbuff full */
/*11*/{STB7100_FDMA_REQ_SSC_0_TX,  		WRITE,  OPCODE_2,  4,     DISABLE_FLG, 0,     1 },  /* SSC 0 txbuff empty */
/*12*/{STB7100_FDMA_REQ_SSC_1_TX,  		WRITE,  OPCODE_2,  4,     DISABLE_FLG, 0,     1 },  /* SSC 1 txbuff empty */
/*13*/{STB7100_FDMA_REQ_SSC_2_TX,  		WRITE,  OPCODE_2,  4,     DISABLE_FLG, 0,     1 },  /* SSC 2 txbuff empty */
/*14*/{STB7100_FDMA_REQ_UART_0_RX,  		READ,   OPCODE_1,  4,     DISABLE_FLG, 0,     1 },  /* UART 0 rx half full */
/*15*/{STB7100_FDMA_REQ_UART_1_RX,  		READ,   OPCODE_1,  4,     DISABLE_FLG, 0,     1 },  /* UART 1 rx half full */
/*16*/{STB7100_FDMA_REQ_UART_2_RX,  		READ,   OPCODE_1,  4,     DISABLE_FLG, 0,     1 },  /* UART 2 rx half full */
/*17*/{STB7100_FDMA_REQ_UART_3_RX,  		READ,   OPCODE_1,  4,     DISABLE_FLG, 0,     1 },  /* UART 3 rx half full */
/*18*/{STB7100_FDMA_REQ_UART_0_TX,  		WRITE,  OPCODE_1,  1,     DISABLE_FLG, 0,     1 },  /* UART 0 tx half empty */
/*19*/{STB7100_FDMA_REQ_UART_1_TX,  		WRITE,  OPCODE_1,  1,     DISABLE_FLG, 0,     1 },  /* UART 1 tx half empty */
/*20*/{STB7100_FDMA_REQ_UART_2_TX,  		WRITE,  OPCODE_1,  1,     DISABLE_FLG, 0,     1 },  /* UART 2 tx half emtpy */
/*21*/{STB7100_FDMA_REQ_UART_3_TX,  		WRITE,  OPCODE_1,  1,     DISABLE_FLG, 0,     1 },  /* UART 3 tx half empty */
/*22*/{STB7100_FDMA_REQ_EXT_PIO_0,  		READ,   OPCODE_4,  1,     DISABLE_FLG, 0,     1 },  /* External 0 (PIO2bit5) hi priority */
/*23*/{STB7100_FDMA_REQ_EXT_PIO_1,  		READ,   OPCODE_4,  1,     DISABLE_FLG, 0,     1 },  /* External 1 (PIO2bit6) hi priority */
/*24*/{STB7100_FDMA_REQ_CPXM_DECRYPT,  		READ,   OPCODE_4,  4,     DISABLE_FLG, 0,     1 },  /* CPxM decrypted data request */
/*25*/{STB7100_FDMA_REQ_CPXM_ENCRYPT,  		WRITE,  OPCODE_4,  4,     DISABLE_FLG, 0,     1 },  /* CPxm encrypted data request */
/*26*/{STB7100_FDMA_REQ_PCM_0,  		WRITE,  OPCODE_4,  1,     DISABLE_FLG, 0,     1 },  /* Audio PCM Player 0 */
/*27*/{STB7100_FDMA_REQ_PCM_1,  		WRITE,  OPCODE_4,  1,     DISABLE_FLG, 0,     1 },  /* Audio PCM Player 1 */
/*28*/{STB7100_FDMA_REQ_PCM_READ,  		READ,   OPCODE_4,  1,     DISABLE_FLG, 1,     1 },  /* Audio PCM Reader */
/*29*/{STB7100_FDMA_REQ_SPDIF,  		WRITE,  OPCODE_4,  2,     DISABLE_FLG, 0,     1 },  /* Audio SPDIF - 2xST4*/
/*30*/{STB7100_FDMA_REQ_SWTS,  			WRITE,  OPCODE_16, 1,     DISABLE_FLG, 0,     1 },   /* SWTS */
/*31*/{STB7100_FDMA_REQ_UNUSED,  		UNUSED, UNUSED,    1,     UNUSED,      0,     1 }, /* Reserved */

};

/*
				Req  				RnW,    Opcode,  Transfer Incr addr,   Hold_off Initiator), */
/*           	1-32     		 			cnt 1-4  On/Off       0-2        Used) */
static  fdmareq_RequestConfig_t stb7109_fdma_req_config[]= {
/*=========================== 7109 ============================================*/

/*  {  Req  RnW, 						Opcode,    TransferCount  Inc,       Hold_off        Initiator), */
/*  {               						1-32       1-4            On/Off       0-2           Used) */
/*0*/	{STB7109_FDMA_REQ_UNUSED,			UNUSED, UNUSED,    1,     	  UNUSED,      0,     	     1 },/* NOT CONNECTED */
/*1*/	{STB7109_FDMA_DMA_REQ_HDMI_AVI,			READ,   OPCODE_8,  1,             DISABLE_FLG, 0,            1 },  /* Video HDMI */
/*2*/	{STB7109_FDMA_REQ_DISEQC_HALF_EMPTY,		WRITE,  OPCODE_4,  2,             DISABLE_FLG, 0,            1 },  /* DiseqC half empty */
/*3*/	{STB7109_FDMA_REQ_DISEQC_HALF_FULL,		READ,   OPCODE_4,  2,             DISABLE_FLG, 0,            1 },  /* DiseqC half full */
/*4*/	{STB7109_FDMA_REQ_SH4_SCIF_RX,			READ,   OPCODE_4,  2,             DISABLE_FLG, 0,            1 },  /* SH4/SCIF */
/*5*/	{STB7109_FDMA_REQ_SH4_SCIF_TX,			WRITE,  OPCODE_4,  2,             DISABLE_FLG, 0,            1 },  /* SH4/SCIF */
/*6*/	{STB7109_FDMA_REQ_SSC_0_RX,			READ,   OPCODE_2,  4,             DISABLE_FLG, 0,            1 },  /* SSC 0 rxbuff full */
/*7*/	{STB7109_FDMA_REQ_SSC_1_RX,			READ,   OPCODE_2,  4,             DISABLE_FLG, 0,            1 },  /* SSC 1 rxbuff full */
/*8*/	{STB7109_FDMA_REQ_SSC_2_RX,			READ,   OPCODE_2,  4,             DISABLE_FLG, 0,            1 },  /* SSC 2 rxbuff full */
/*9*/	{STB7109_FDMA_REQ_SSC_0_TX,			WRITE,  OPCODE_2,  4,             DISABLE_FLG, 0,            1 },  /* SSC 0 txbuff empty */
/*10*/	{STB7109_FDMA_REQ_SSC_1_TX,			WRITE,  OPCODE_2,  4,             DISABLE_FLG, 0,            1 },  /* SSC 1 txbuff empty */
/*11*/	{STB7109_FDMA_REQ_SSC_2_TX,			WRITE,  OPCODE_2,  4,             DISABLE_FLG, 0,            1 },  /* SSC 1 txbuff empty */
/*12*/  {STB7109_FDMA_REQ_UART_0_RX,			READ,   OPCODE_1,  4,             DISABLE_FLG, 0,            1 },  /* UART 0 rx half full */
/*13*/	{STB7109_FDMA_REQ_UART_1_RX,			READ,   OPCODE_1,  4,             DISABLE_FLG, 0,            1 },  /* UART 1 rx half full */
/*14*/	{STB7109_FDMA_REQ_UART_2_RX,			READ,   OPCODE_1,  4,             DISABLE_FLG, 0,            1 },  /* UART 2 rx half full */
/*15*/	{STB7109_FDMA_REQ_UART_3_RX,			READ,   OPCODE_1,  4,             DISABLE_FLG, 0,            1 },  /* UART 3 rx half full */
/*16*/	{STB7109_FDMA_REQ_UART_0_TX,			WRITE,  OPCODE_1,  1,             DISABLE_FLG, 0,            1 },  /* UART 0 tx half empty */
/*17*/	{STB7109_FDMA_REQ_UART_1_TX,			WRITE,  OPCODE_1,  1,             DISABLE_FLG, 0,            1 },  /* UART 1 tx half empty */
/*18*/	{STB7109_FDMA_REQ_UART_2_TX,			WRITE,  OPCODE_1,  1,             DISABLE_FLG, 0,            1 },  /* UART 2 tx half emtpy */
/*19*/	{STB7109_FDMA_REQ_UART_3_TX,			WRITE,  OPCODE_1,  1,             DISABLE_FLG, 0,            1 },  /* UART 3 tx half empty */
/*20*/	{STB7109_FDMA_REQ_REQ_EXT_PIO_0,		READ,   OPCODE_4,  1,             DISABLE_FLG, 0,            1 },  /* External 0 (PIO2bit5) hi priority */
/*21*/	{STB7109_FDMA_REQ_REQ_EXT_PIO_1,		READ,   OPCODE_4,  1,             DISABLE_FLG, 0,            1 },  /* External 1 (PIO2bit6) hi priority */
/*22*/	{STB7109_FDMA_REQ_CPXM_DECRYPT,  	     	READ,   OPCODE_4,  4,             DISABLE_FLG, 0,            1 },  /* CPxM decrypted data request */
/*23*/  {STB7109_FDMA_REQ_CPXM_ENCRYPT,  	     	WRITE,  OPCODE_4,  4,             DISABLE_FLG, 0,            1 },  /* CPxm encrypted data request */
/*24*/	{STB7109_FDMA_REQ_PCM_0,			WRITE,  OPCODE_4,  1,             DISABLE_FLG, 0,            0 },  /* Audio PCM Player 0 */
/*25*/	{STB7109_FDMA_REQ_PCM_1,			WRITE,  OPCODE_4,  1,             DISABLE_FLG, 0,            0 },  /* Audio PCM Player 1 */
/*26*/	{STB7109_FDMA_REQ_PCM_READ,			READ,   OPCODE_4,  1,             DISABLE_FLG, 0,            0 },  /* Audio PCM Reader */
/*27*/	{STB7109_FDMA_REQ_SPDIF,			WRITE,  OPCODE_4,  1,             DISABLE_FLG, 0,            0 },  /* Audio SPDIF - 2xST4*/
/*29*/	{STB7109_FDMA_REQ_SWTS_0,			WRITE,  OPCODE_32, 1,             DISABLE_FLG, 0,            0 },  /* SWTS 0 */
/*29*/	{STB7109_FDMA_REQ_SWTS_1,			WRITE,  OPCODE_32, 1,             DISABLE_FLG, 0,            0 },  /* SWTS 1 */
/*30*/	{STB7109_FDMA_REQ_SWTS_2,			WRITE,  OPCODE_32, 1,             DISABLE_FLG, 0,            0 },  /* SWTS 2 */
/*31*/  {STB7109_FDMA_REQ_UNUSED,           		UNUSED, UNUSED,    1,             UNUSED,      0,            0 },  /* Reserved */
};

static fdma_regs_t stb7100_fdma_regs = {
	 .fdma_id =     	  FDMA2_ID,
	 .fdma_ver = 		  FDAM2_VER,
	 .fdma_en= 		  FDMA2_ENABLE_REG,
	 .fdma_clk_gate = 	  FDMA2_CLOCKGATE,
	 .fdma_rev_id = 	  FDMA2_REV_ID,
	 .fdma_cmd_statn = 	  STB7100_FDMA_CMD_STATn_REG,
	 .fdma_ptrn = 		  STB7100_FDMA_PTR_REG,
	 .fdma_cntn = 		  STB7100_FDMA_COUNT_REG,
	 .fdma_saddrn = 	  STB7100_FDMA_SADDR_REG,
	 .fdma_daddrn = 	  STB7100_FDMA_DADDR_REG,
	 .fdma_req_ctln = 	  STB7100_FDMA_REQ_CTLn_REG,
	 .fdma_cmd_sta =	  FDMA2_CMD_MBOX_STAT_REG,
	 .fdma_cmd_set =	  FDMA2_CMD_MBOX_SET_REG,
	 .fdma_cmd_clr = 	  FDMA2_CMD_MBOX_CLR_REG,
	 .fdma_cmd_mask =	  FDMA2_CMD_MBOX_MASK_REG,
	 .fdma_int_sta = 	  FDMA2_INT_STAT_REG,
	 .fdma_int_set = 	  FDMA2_INT_SET_REG,
	 .fdma_int_clr= 	  FDMA2_INT_CLR_REG,
	 .fdma_int_mask= 	  FDMA2_INT_MASK_REG,
	 .fdma_sync_reg= 	  FDMA2_SYNCREG,
	 .fdma_dmem_region =  	  STB7100_DMEM_OFFSET,
	 .fdma_imem_region =  	  STB7100_IMEM_OFFSET,
};

static fdma_regs_t stb7109_fdma_regs = {
	.fdma_id= FDMA2_ID,
	.fdma_ver = FDAM2_VER,
	.fdma_en = FDMA2_ENABLE_REG,
	.fdma_clk_gate = FDMA2_CLOCKGATE,
	.fdma_rev_id = FDMA2_REV_ID,
	.fdma_cmd_statn = STB7109_FDMA_CMD_STATn_REG,
	.fdma_ptrn = STB7109_FDMA_PTR_REG,
	.fdma_cntn = STB7109_FDMA_COUNT_REG,
	.fdma_saddrn = STB7109_FDMA_SADDR_REG,
	.fdma_daddrn = STB7109_FDMA_DADDR_REG,
	.fdma_req_ctln = STB7109_FDMA_REQ_CTLn_REG,
	.fdma_cmd_sta = FDMA2_CMD_MBOX_STAT_REG,
	.fdma_cmd_set = FDMA2_CMD_MBOX_SET_REG,
	.fdma_cmd_clr = FDMA2_CMD_MBOX_CLR_REG,
	.fdma_cmd_mask = FDMA2_CMD_MBOX_MASK_REG,
	.fdma_int_sta = FDMA2_INT_STAT_REG,
	.fdma_int_set = FDMA2_INT_SET_REG,
	.fdma_int_clr= FDMA2_INT_CLR_REG,
	.fdma_int_mask= FDMA2_INT_MASK_REG,
	.fdma_sync_reg= FDMA2_SYNCREG,
	.fdma_dmem_region = STB7109_DMEM_OFFSET,
	.fdma_imem_region = STB7109_IMEM_OFFSET,
};

static struct fdma_platform_device_data stb7109_C1_fdma_plat_data ={
	.cpu_subtype = 7109,
	.cpu_rev = 1,
};

static struct fdma_platform_device_data stb7109_C2_fdma_plat_data = {
	.req_line_tbl_adr = (void*)&stb7109_fdma_req_config,
	.registers_ptr =(void*) &stb7109_fdma_regs,
	.cpu_subtype = 7109,
	.cpu_rev = 2,
	.min_ch_num = CONFIG_MIN_STM_DMA_CHANNEL_NR,
	.max_ch_num  =CONFIG_MAX_STM_DMA_CHANNEL_NR,
	.fdma_base  = STB7109_FDMA_BASE,
	.irq_vect = LINUX_FDMA_STB7109_IRQ_VECT,
	.fw_device_name = "STB7109_fdma_fw",
	.nr_reqlines = (sizeof(stb7109_fdma_req_config) / sizeof(fdmareq_RequestConfig_t)),
	.fw.data_reg = (unsigned long*)&STB7109_C2_DMEM_REGION,
	.fw.imem_reg = (unsigned long*)&STB7109_C2_IMEM_REGION,
	.fw.imem_fw_sz = STB7109_C2_IMEM_FIRMWARE_SZ,
	.fw.dmem_fw_sz = STB7109_C2_DMEM_FIRMWARE_SZ,
	.fw.dmem_len = STB7109_C2_DMEM_REGION_LENGTH,
	.fw.imem_len = STB7109_C2_IMEM_REGION_LENGTH
};

static struct fdma_platform_device_data stb7109_C3_fdma_plat_data = {
	.req_line_tbl_adr = (void*)&stb7109_fdma_req_config,
	.registers_ptr =(void*) &stb7109_fdma_regs,
	.cpu_subtype = 7109,
	.cpu_rev = 3,
	.min_ch_num = CONFIG_MIN_STM_DMA_CHANNEL_NR,
	.max_ch_num  =CONFIG_MAX_STM_DMA_CHANNEL_NR,
	.fdma_base  = STB7109_FDMA_BASE,
	.irq_vect = LINUX_FDMA_STB7109_IRQ_VECT,
	.fw_device_name = "STB7109_C3_fdma_fw",
	.nr_reqlines = (sizeof(stb7109_fdma_req_config) / sizeof(fdmareq_RequestConfig_t)),
	.fw.data_reg = (unsigned long*)&STB7109_C3_DMEM_REGION,
	.fw.imem_reg = (unsigned long*)&STB7109_C3_IMEM_REGION,
	.fw.imem_fw_sz = STB7109_C3_IMEM_FIRMWARE_SZ,
	.fw.dmem_fw_sz = STB7109_C3_DMEM_FIRMWARE_SZ,
	.fw.dmem_len = STB7109_C3_DMEM_REGION_LENGTH,
	.fw.imem_len = STB7109_C3_IMEM_REGION_LENGTH

};

static struct fdma_platform_device_data stb7100_Cx_fdma_plat_data = {
	.req_line_tbl_adr = (void*)&stb7100_fdma_req_config,
	.registers_ptr =(void*) &stb7100_fdma_regs,
	.cpu_subtype = 7100,
/*	.cpu_rev = ! defined at runtime */
	.min_ch_num = CONFIG_MIN_STM_DMA_CHANNEL_NR,
	.max_ch_num  =CONFIG_MAX_STM_DMA_CHANNEL_NR,
	.fdma_base  = STB7100_FDMA_BASE,
	.irq_vect = LINUX_FDMA_STB7100_IRQ_VECT,
	.fw_device_name = "STB7100_Cx_fdma_fw",
	.nr_reqlines = (sizeof(stb7100_fdma_req_config) / sizeof(fdmareq_RequestConfig_t)),
	.fw.data_reg = (unsigned long*)&STB7100_DMEM_REGION,
	.fw.imem_reg = (unsigned long*)&STB7100_IMEM_REGION,
	.fw.imem_fw_sz = STB7100_IMEM_FIRMWARE_SZ,
	.fw.dmem_fw_sz = STB7100_DMEM_FIRMWARE_SZ,
	.fw.dmem_len = STB7100_DMEM_REGION_LENGTH,
	.fw.imem_len = STB7100_IMEM_REGION_LENGTH
};


static struct platform_device fdma_710x_device = {
        .name           = "710x_FDMA",
        .id             = -1,
};


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
	[2] = {/*rising or falling edge I2s clocking*/
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
	[2] = {/*rising or falling edge I2s clocking*/
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
	}};


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
	}};


static struct platform_device alsa_710x_device_pcm0 = {
	.name			= "710x_ALSA_PCM0",
	.id 			= -1,
	.num_resources	= 	ARRAY_SIZE(alsa_710x_resource_pcm0),
	.resource		= alsa_710x_resource_pcm0,
};


static struct platform_device alsa_710x_device_pcm1 = {
	.name			= "710x_ALSA_PCM1",
	.id 			= -1,
	.num_resources	= 	ARRAY_SIZE(alsa_710x_resource_pcm1),
	.resource		= alsa_710x_resource_pcm1,
};

static struct platform_device alsa_710x_device_spdif = {
	.name			= "710x_ALSA_SPD",
	.id 			= -1,
	.num_resources	= 	ARRAY_SIZE(alsa_710x_resource_spdif),
	.resource		= alsa_710x_resource_spdif,
};

static struct platform_device alsa_710x_device_cnv = {
	.name			= "710x_ALSA_CNV",
	.id 			= -1,
	.num_resources	= 	ARRAY_SIZE(alsa_710x_resource_cnv),
	.resource		= alsa_710x_resource_cnv,
};

static struct platform_device *hms1_devices[] __initdata = {
	&st40_ohci_devices,
	&st40_ehci_devices,
	&lirc_device,
	&sata_device,
	&wdt_device,
	&ssc_device,
	&rtc_device,
	&stm_pwm_device,
	&fdma_710x_device,
	&alsa_710x_device_pcm0,
	&alsa_710x_device_pcm1,
	&alsa_710x_device_spdif,
	&alsa_710x_device_cnv,
};

static int __init device_init(void)
{
	int ret =0;
	unsigned long devid;
	unsigned long chip_revision, chip_7109;

	devid = ctrl_inl(SYSCONF_DEVICEID);
	chip_7109 = (((devid >> 12) & 0x3ff) == 0x02c);
	chip_revision = (devid >> 28) + 1;

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
	if(chip_7109){
		if(chip_revision==3){
			alsa_710x_resource_pcm0[2].start =1;
			alsa_710x_resource_pcm0[2].end = 1;

			alsa_710x_resource_pcm1[2].start =1;
			alsa_710x_resource_pcm1[2].end = 1;

			fdma_710x_device.dev.platform_data =(void*) &stb7109_C3_fdma_plat_data;
		}
		else if(chip_revision==2){
			fdma_710x_device.dev.platform_data =(void*) &stb7109_C2_fdma_plat_data;
			alsa_710x_resource_pcm0[2].start =0;
			alsa_710x_resource_pcm0[2].end = 0;

			alsa_710x_resource_pcm1[2].start =0;
			alsa_710x_resource_pcm1[2].end = 0;
		}
		else{
			fdma_710x_device.dev.platform_data =(void*) &stb7109_C1_fdma_plat_data;
			alsa_710x_resource_pcm0[2].start =0;
			alsa_710x_resource_pcm0[2].end = 0;

			alsa_710x_resource_pcm1[2].start =0;
			alsa_710x_resource_pcm1[2].end = 0;
		}

		alsa_710x_resource_pcm0[0].start = 2;
		alsa_710x_resource_pcm0[0].end = 10;

		alsa_710x_resource_pcm1[0].start = 2;
		alsa_710x_resource_pcm1[0].end 	= 2;

		alsa_710x_resource_pcm0[1].start = STB7109_FDMA_REQ_PCM_0;
		alsa_710x_resource_pcm0[1].end = STB7109_FDMA_REQ_PCM_0;

		alsa_710x_resource_pcm1[1].start = STB7109_FDMA_REQ_PCM_1;
		alsa_710x_resource_pcm1[1].end = STB7109_FDMA_REQ_PCM_1;

		/*here we are telling the ALSA drivers whetther to use rising or falling
		 * edge I2s sampling to DAC.  I want to aviod including ALSA headers into
		 * this file hence the cyptic value.*/

		alsa_710x_resource_spdif[1].start = STB7109_FDMA_REQ_SPDIF;
		alsa_710x_resource_spdif[1].end =   STB7109_FDMA_REQ_SPDIF;

		alsa_710x_resource_cnv[0].start =2;
		alsa_710x_resource_cnv[0].end = 10;
		alsa_710x_resource_cnv[1].start = STB7109_FDMA_REQ_PCM_0;
		alsa_710x_resource_cnv[1].end = STB7109_FDMA_REQ_PCM_0;
	}
	else {
		stb7100_Cx_fdma_plat_data.cpu_rev = chip_revision;
		fdma_710x_device.dev.platform_data =(void*) &stb7100_Cx_fdma_plat_data;

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
		alsa_710x_resource_pcm0[2].start =0;
		alsa_710x_resource_pcm0[2].end = 0;

		alsa_710x_resource_pcm1[2].start =0;
		alsa_710x_resource_pcm1[2].end = 0;

		alsa_710x_resource_pcm0[1].start = STB7100_FDMA_REQ_PCM_0;
		alsa_710x_resource_pcm0[1].end = STB7100_FDMA_REQ_PCM_0;

		alsa_710x_resource_pcm1[1].start = STB7100_FDMA_REQ_PCM_1;
		alsa_710x_resource_pcm1[1].end = STB7100_FDMA_REQ_PCM_1;

		alsa_710x_resource_spdif[1].start =  STB7100_FDMA_REQ_SPDIF;
		alsa_710x_resource_spdif[1].end =  STB7100_FDMA_REQ_SPDIF;

		alsa_710x_resource_cnv[1].start = STB7100_FDMA_REQ_PCM_0;
		alsa_710x_resource_cnv[1].end = STB7100_FDMA_REQ_PCM_0;
	}

	ret = platform_add_devices(hms1_devices, ARRAY_SIZE(hms1_devices));
	return ret;
}

subsys_initcall(device_init);
