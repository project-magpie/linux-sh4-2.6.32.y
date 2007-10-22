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
#include <linux/phy.h>
#include <asm/sci.h>
#include <linux/stm/710x_fdma.h>
#include <linux/stm/7100_fdma2_firmware.h>
#include <linux/stm/7109_cut2_fdma2_firmware.h>
#include <linux/stm/7109_cut3_fdma2_firmware.h>

#define SYSCONF_BASE 0xb9001000
#define SYSCONF_DEVICEID        (SYSCONF_BASE + 0x000)
#define SYSCONF_SYS_STA(n)      (SYSCONF_BASE + 0x008 + ((n) * 4))
#define SYSCONF_SYS_CFG(n)      (SYSCONF_BASE + 0x100 + ((n) * 4))

#ifdef CONFIG_STMMAC_ETH
#define MII_MODE	    0x00040000 /* RMII interface activated */
#define ETH_IF_ON	    0x00010000 /* ETH interface on */
#define DVO_ETH_PAD_DISABLE 0x00020000 /* DVO eth pad disable */
#endif

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

static struct resource st40_ohci_resources[] = {
	/*this lot for the ohci block*/
	[0] = {
		.start = 0xb9100000 + 0xffc00,
		.end  =  0xb9100000 +0xffcff,
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

/*
 * Defines for the controller register offsets
 */
#define UHOST2C_BASE			0xb9100000
#define AHB2STBUS_WRAPPER_GLUE_BASE	(UHOST2C_BASE)
#define AHB2STBUS_RESERVED1_BASE	(UHOST2C_BASE + 0x000e0000)
#define AHB2STBUS_RESERVED2_BASE	(UHOST2C_BASE + 0x000f0000)
#define AHB2STBUS_OHCI_BASE		(UHOST2C_BASE + 0x000ffc00)
#define AHB2STBUS_EHCI_BASE		(UHOST2C_BASE + 0x000ffe00)
#define AHB2STBUS_PROTOCOL_BASE		(UHOST2C_BASE + 0x000fff00)

#define SYS_CFG2_PLL_POWER_DOWN_BIT	1

static void usb_power_up(void* dev)
{
	unsigned long reg;

	/* Make sure PLL is on */
	reg = readl(SYSCONF_SYS_CFG(2));
	if (reg & SYS_CFG2_PLL_POWER_DOWN_BIT) {
		writel(reg & (~SYS_CFG2_PLL_POWER_DOWN_BIT), SYSCONF_SYS_CFG(2));
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

static struct platform_device  st40_ohci_devices = {
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

static struct platform_device  st40_ehci_devices = {
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

/*
 * ALSA
 */

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

static struct platform_device alsa_710x_device_pcmin = {
	.name			= "710x_ALSA_PCMIN",
	.id			= -1,
	.num_resources		= ARRAY_SIZE(alsa_710x_resource_pcmin),
	.resource		= alsa_710x_resource_pcmin,
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
	{2, 0, 2, 1, 0xff, 0xff},
	{3, 0, 3, 1, 3, 2},
	{4, 0, 4, 1, 0xff, 0xff},
};
static struct plat_ssc_data ssc_private_info = {
	.capability  =
		(SSC_I2C_CAPABILITY << (0*2)) |
		((SSC_SPI_CAPABILITY | SSC_I2C_CAPABILITY) << (1*2)) |
		(SSC_I2C_CAPABILITY << (2*2)),
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
	.name		= "stm-sata",
	.id		= -1,
	.num_resources	= ARRAY_SIZE(sata_resource),
	.resource	= sata_resource,
       .dev = {
               .platform_data = &sata_private_info,
       }
};

#ifdef CONFIG_STB7109_ETH
/* ETH MAC pad configuration */
static void stb7109eth_hw_setup(void)
{
	unsigned long sysconf;
	static struct stpio_pin *ethreset;

	sysconf = ctrl_inl(SYSCONF_SYS_CFG(7));
	sysconf |= (DVO_ETH_PAD_DISABLE | ETH_IF_ON);
	sysconf &= ~MII_MODE;
#ifdef CONFIG_PHY_RMII
	sysconf |= MII_MODE; /* RMII selected*/
#else
	sysconf &= ~MII_MODE; /* MII selected */
#endif
#ifdef CONFIG_STMMAC_EXT_CLK
        sysconf |= PHY_CLK_EXT;
#endif
	ctrl_outl(sysconf, SYSCONF_SYS_CFG(7));

	/* Enable the external PHY interrupts */
	sysconf = ctrl_inl(SYSCONF_SYS_CFG(10));
	sysconf |= 0x0000000f;
	ctrl_outl(sysconf, SYSCONF_SYS_CFG(10));

	/* Remove the PHY clk */
	stpio_reserve_pin(3, 7, "stmmac EXTCLK");
}

/**
 * fix_mac_speed
 * @speed: link speed
 * Description: it is used for changing the MAC speed field in
 * 		the SYS_CFG7 register (required when we are using
 *		the RMII interface).
 */
static void fix_mac_speed(unsigned int speed)
{
#ifdef CONFIG_PHY_RMII
	unsigned long sysconf = ctrl_inl(SYSCONF_SYS_CFG(7));

	if (speed == SPEED_100)
		sysconf |= MAC_SPEED_SEL;
	else if (speed == SPEED_10)
		sysconf &= ~MAC_SPEED_SEL;

	ctrl_outl(sysconf, SYSCONF_SYS_CFG(7));
#endif
	return;
}
#else
static void stb7109eth_hw_setup(void) { }
static void fix_mac_speed(unsigned int speed) { }
#endif

static struct resource stb7109eth_resources[] = {
        [0] = {
                .start = 0x18110000,
                .end   = 0x1811ffff,
                .flags  = IORESOURCE_MEM,
        },
        [1] = {
                .start  = 133,
                .end    = 133,
                .flags  = IORESOURCE_IRQ,
        },
};

static struct plat_stmmacenet_data eth7109_private_data = {
	.bus_id = 0,
	.phy_addr = 14,
	.pbl = 1,
	.interface = PHY_INTERFACE_MODE_MII,
	.fix_mac_speed = fix_mac_speed,
	.hw_setup = stb7109eth_hw_setup,
};

static struct platform_device stb7109eth_device = {
        .name           = "stmmaceth",
        .id             = 0,
        .num_resources  = 3,
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
        	{
			.name   = "phyirq",
                	.start  = 7,
                	.end    = 7,
                	.flags  = IORESOURCE_IRQ,
        	},
	},
	.dev = {
		.platform_data = &eth7109_private_data,
	}
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
	.flags		= PLAT_STM_PWM_OUT0 | PLAT_STM_PWM_OUT1,
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

static struct platform_device *stx710x_devices[] __initdata = {
	&sci_device,
	&wdt_device,
	&rtc_device,
	&st40_ohci_devices,
	&st40_ehci_devices,
	&fdma_710x_device,
	&alsa_710x_device_pcm0,
	&alsa_710x_device_pcm1,
 	&alsa_710x_device_spdif,
	&alsa_710x_device_cnv,
	&ssc_device,
	&sata_device,
	&stb7109eth_device,
	&stm_pwm_device,
};

static int __init stx710x_devices_setup(void)
{
	unsigned long devid;
	unsigned long chip_revision, chip_7109;

	devid = ctrl_inl(SYSCONF_DEVICEID);
	chip_7109 = (((devid >> 12) & 0x3ff) == 0x02c);
	chip_revision = (devid >> 28) + 1;

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

#ifdef CONFIG_STMMAC_ETH
	/* Configure the ethernet MAC PBL depending on the cut of the chip */
	if (chip_7109){
	       if (chip_revision == 1){
			eth7109_private_data.pbl = 1;
		} else {
			eth7109_private_data.pbl = 32;
		}
        }

	stb7109eth_hw_setup();
#endif

	return platform_add_devices(stx710x_devices,
				    ARRAY_SIZE(stx710x_devices));
}
device_initcall(stx710x_devices_setup);

/*
 * INTC style interrupts
 */
static struct ipr_data ipr_map[] = {
	/* IRQ, IPR-idx, shift, priority */
	{ 16, 0, 12, 2 }, /* TMU0 TUNI*/
	{ 17, 0, 12, 2 }, /* TMU1 TUNI */
	{ 18, 0,  4, 2 }, /* TMU2 TUNI */
	{ 19, 0,  4, 2 }, /* TMU2 TIPCI */
	{ 27, 1, 12, 2 }, /* WDT ITI */
	{ 20, 0,  0, 2 }, /* RTC ATI (alarm) */
	{ 21, 0,  0, 2 }, /* RTC PRI (period) */
	{ 22, 0,  0, 2 }, /* RTC CUI (carry) */
	{ 23, 1,  4, 3 }, /* SCI ERI */
	{ 24, 1,  4, 3 }, /* SCI RXI */
	{ 25, 1,  4, 3 }, /* SCI TXI */
	{ 40, 2,  4, 3 }, /* SCIF ERI */
	{ 41, 2,  4, 3 }, /* SCIF RXI */
	{ 42, 2,  4, 3 }, /* SCIF BRI */
	{ 43, 2,  4, 3 }, /* SCIF TXI */
	{ 34, 2,  8, 7 }, /* DMAC DMTE0 */
	{ 35, 2,  8, 7 }, /* DMAC DMTE1 */
	{ 36, 2,  8, 7 }, /* DMAC DMTE2 */
	{ 37, 2,  8, 7 }, /* DMAC DMTE3 */
	{ 28, 2,  8, 7 }, /* DMAC DMAE */
};

/*
 * INTC2-Style interrupts, vectors IRQ 112-175 INTEVT 0x1000-0x17e0
 */
static struct intc2_data intc2_irq_table[] = {
	/* IRQ, IPR index, IPR shift, mask index, mask shift, prio */
	{113, 4,  0, 4,  1, 13},	/* Group0:  pio5 */
	{114, 4,  0, 4,  2, 13},	/*          pio4 */
	{115, 4,  0, 4,  3, 13},	/*          pio3 */
	{116, 4,  0, 4,  4, 13},	/*          mtp (7109) */

	{117, 4,  4, 4,  5, 13},	/* Group1:  ssc2 */
	{118, 4,  4, 4,  6, 13} ,	/*          ssc1 */
	{119, 4,  4, 4,  7, 13},	/*          ssc0 */

	{120, 4,  8, 4,  8, 13},	/* Group2:  uart3 */
	{121, 4,  8, 4,  9, 13},	/*          uart2 */
	{122, 4,  8, 4, 10, 13},	/*          uart1 */
	{123, 4,  8, 4, 11, 13},	/*          uart0 */

	{124, 4, 12, 4, 12, 13},	/* Group3:  irb_wakeup */
	{125, 4, 12, 4, 13, 13},	/*          irb */
	{126, 4, 12, 4, 14, 13},	/*          pwm */
	{127, 4, 12, 4, 15, 13},	/*          mafe */

	{129, 4, 16, 4, 17, 13},	/* Group4:  disqec */
	{130, 4, 16, 4, 18, 13},	/*          daa */
	{131, 4, 16, 4, 19, 13},	/*          ttxt */

	{132, 4, 20, 4, 20, 13},	/* Group5:  empi (7109) */
	{133, 4, 20, 4, 21, 13},	/*          eth_mac (7109) */
	{134, 4, 20, 4, 22, 13},	/*          TS_Merger (7109) */
	{135, 4, 20, 4, 23, 13},	/*          sbatm */

	{136, 4, 24, 4, 24, 13},	/* Group6:  lx_delphi (lx_deltaMu 7109) */
	{137, 4, 24, 4, 25, 13},	/*          lx_aud */
	{138, 4, 24, 4, 26, 13},	/*          dcxo */
	{139, 4, 24, 4, 27, 13},	/*          pti1 (7109) */

	{140, 4, 28, 4, 28, 13},	/* Group7:  fdma_mbox */
	{141, 4, 28, 4, 29, 13},	/*          fdma_gp0 */
	{142, 4, 28, 4, 30, 13},	/*          i2s2spdif */
	{143, 4, 28, 4, 31, 13},	/*          cpxm */

	{144, 8,  0, 8,  0, 13},	/* Group8:  pcmplyr0 */
	{145, 8,  0, 8,  1, 13},	/*          pcmplyr1 */
	{146, 8,  0, 8,  2, 13},	/*          pcmrdr */
	{147, 8,  0, 8,  3, 13},	/*          spdifplyr */

	{148, 8,  4, 8,  4, 13},	/* Group9:  glh */
	{149, 8,  4, 8,  5, 13},	/*          delphi_pre0 */
	{150, 8,  4, 8,  6, 13},	/*          delphi_pre1 */
	{151, 8,  4, 8,  7, 13},	/*          delphi_mbe */

	{152, 8,  8, 8,  8, 13},	/* Group10:  vdp_fifo_empty (7109) */
	{153, 8,  8, 8,  9, 13},	/*           lmu (vdp_end_processing 7109) */
	{154, 8,  8, 8, 10, 13},	/*           vtg1 */
	{155, 8,  8, 8, 11, 13},	/*           vtg2 */

	{156, 8, 12, 8, 12, 13},	/* Group11:  blt (bdisp_aq1 7109) */
	{157, 8, 12, 8, 13, 13},	/*           dvp */
	{158, 8, 12, 8, 14, 13},	/*           hdmi */
	{159, 8, 12, 8, 15, 13},	/*           hdcp */

	{160, 8, 16, 8, 16, 13},	/* Group12:  pti */
	{161, 8, 16, 8, 17, 13},	/*           pdes_esa0_select (7109) */
	{162, 8, 16, 8, 18, 13},	/*           pdes */
	{163, 8, 16, 8, 19, 13},	/*           pdes_read_cw (7109) */

	{164, 8, 20, 8, 20, 13},	/* Group13:  sig_chk (tkdma_tkd 7109) */
	{165, 8, 20, 8, 21, 13},	/*           dma_fin (tkdma_dma 7109) */
	{166, 8, 20, 8, 22, 13},	/*           sec_cp (cripto_sig_dma 7109) */
	{167, 8, 20, 8, 23, 13},	/*           cripto_sig_chk (7109) */

	{168, 8, 24, 8, 24, 13},	/* Group14:  ohci */
	{169, 8, 24, 8, 25, 13},	/*           ehci */
	{170, 8, 24, 8, 26, 13},	/*           sata */
	{171, 8, 28, 8, 27, 13},	/*           bdisp_cq1 (7109) */

	{172, 8, 28, 8, 28, 13},	/* Group15:  icam3_kte (7109) */
	{173, 8, 28, 8, 29, 13},	/*           icam3 (7109) */
	{174, 8, 28, 8, 30, 13},	/*           mes_lmi_vid (7109) */
	{175, 8, 28, 8, 31, 13},	/*           mes_lmi_sys (7109) */
};

static struct intc2_desc intc2_irq_desc __read_mostly = {
	.prio_base	= 0xb9001300,
	.msk_base	= 0xb9001340,
	.mskclr_base	= 0xb9001360,

	.intc2_data	= intc2_irq_table,
	.nr_irqs	= ARRAY_SIZE(intc2_irq_table),

	.chip = {
		.name	= "INTC2-stx710x",
	},
};

static struct ipr_data ipr_irq_table[] = {
	/* IRQ, IPR-idx, shift, priority */
	{ 16, 0, 12, 2 }, /* TMU0 TUNI*/
	{ 17, 0,  8, 2 }, /* TMU1 TUNI */
	{ 18, 0,  4, 2 }, /* TMU2 TUNI */
	{ 27, 1, 12, 2 }, /* WDT ITI */
	{ 32, 2,  0, 7 }, /* HUDI */
/* these here are only valid if INTC_ICR bit 7 is set to 1! */
	{  2, 3, 12, 3 }, /* IRL0 */
	{  5, 3,  8, 3 }, /* IRL1 */
	{  8, 3,  4, 3 }, /* IRL2 */
	{ 11, 3,  0, 3 }, /* IRL3 */
};

static unsigned long ipr_offsets[] = {
	0xffd00004UL,	/* 0: IPRA */
	0xffd00008UL,	/* 1: IPRB */
	0xffd0000cUL,	/* 2: IPRC */
	0xffd00010UL,	/* 3: IPRD */
};

static struct ipr_desc ipr_irq_desc = {
	.ipr_offsets	= ipr_offsets,
	.nr_offsets	= ARRAY_SIZE(ipr_offsets),

	.ipr_data	= ipr_irq_table,
	.nr_irqs	= ARRAY_SIZE(ipr_irq_table),

	.chip = {
		.name	= "IPR-stx710x",
	},
};

void __init plat_irq_setup(void)
{
	register_intc2_controller(&intc2_irq_desc);
	register_ipr_controller(&ipr_irq_desc);
}

#define INTC_ICR	0xffd00000UL
#define INTC_ICR_IRLM   (1<<7)

/* enable individual interrupt mode for external interupts */
void __init ipr_irq_enable_irlm(void)
{
#if defined(CONFIG_CPU_SUBTYPE_SH7750) || defined(CONFIG_CPU_SUBTYPE_SH7091)
	BUG(); /* impossible to mask interrupts on SH7750 and SH7091 */
#endif
//	register_intc_controller(&intc_desc_irlm);

	ctrl_outw(ctrl_inw(INTC_ICR) | INTC_ICR_IRLM, INTC_ICR);
}
