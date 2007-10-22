/*
 * STb7100 common OHCI/EHCI controller functions.
 *
 * Copyright (c) 2005 STMicroelectronics Limited
 * Author: Mark Glaisher <mark.glaisher@st.com>
 *
 * This file is licenced under the GPL.
 */

/*
 * Some of these register are described in ADCS 7518758.
 */

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

/* The transaction opcode is programmed in this register */
#define AHB2STBUS_STBUS_OPC		(AHB2STBUS_PROTOCOL_BASE + 0x00)
#define AHB2STBUS_STBUS_OPC_4BIT	0x00
#define AHB2STBUS_STBUS_OPC_8BIT	0x01
#define AHB2STBUS_STBUS_OPC_16BIT	0x02
#define AHB2STBUS_STBUS_OPC_32BIT	0x03
#define AHB2STBUS_STBUS_OPC_64BIT	0x04

/* The message length in number of packets is programmed in this register. */
#define AHB2STBUS_MSGSIZE		(AHB2STBUS_PROTOCOL_BASE + 0x04)
#define AHB2STBUS_MSGSIZE_DISABLE	0x0
#define AHB2STBUS_MSGSIZE_2		0x1
#define AHB2STBUS_MSGSIZE_4		0x2
#define AHB2STBUS_MSGSIZE_8		0x3
#define AHB2STBUS_MSGSIZE_16		0x4
#define AHB2STBUS_MSGSIZE_32		0x5
#define AHB2STBUS_MSGSIZE_64		0x6

/* The chunk size in number of packets is programmed in this register */
#define AHB2STBUS_CHUNKSIZE		(AHB2STBUS_PROTOCOL_BASE + 0x08)
#define AHB2STBUS_CHUNKSIZE_DISABLE	0x0
#define AHB2STBUS_CHUNKSIZE_2		0x1
#define AHB2STBUS_CHUNKSIZE_4		0x2
#define AHB2STBUS_CHUNKSIZE_8		0x3
#define AHB2STBUS_CHUNKSIZE_16		0x4
#define AHB2STBUS_CHUNKSIZE_32		0x5
#define AHB2STBUS_CHUNKSIZE_64		0x6

/* This register holds the timeout value in number of STBus clock cycles */
#define AHB2STBUS_REQ_TIMEOUT		(AHB2STBUS_PROTOCOL_BASE + 0x0c)

/* Undocumented */
#define AHB2STBUS_PC_STATUS		(AHB2STBUS_PROTOCOL_BASE + 0x10)
#define AHB2STBUS_PC_STATUS_IDLE	1


/* This register implements interrupt status for the OHCI controller */
#define AHB2STBUS_OHCI_INT_STS		(AHB2STBUS_WRAPPER_GLUE_BASE + 0x08)

/* This register implements interrupt mask for the OHCI controller */
#define AHB2STBUS_OHCI_INT_MASK		(AHB2STBUS_WRAPPER_GLUE_BASE + 0x0c)

/* This register implements interrupt status for the EHCI controller */
#define AHB2STBUS_EHCI_INT_STS		(AHB2STBUS_WRAPPER_GLUE_BASE + 0x10)


#define AHB2STBUS_STRAP			(AHB2STBUS_WRAPPER_GLUE_BASE + 0x14)
#define AHB2STBUS_STRAP_PLL		0x08	/* undocumented */
#define AHB2STBUS_STRAP_16_BIT		0x04	/* ss_word_if */


/*
 * SYSCONF stuff
 */

#define SYSCONF_BASE			0xb9001000
#define SYS_CFG2			(SYSCONF_BASE + 0x108)
#define SYS_CFG2_PLL_POWER_DOWN_BIT	1

static inline void usb_hcd_st40_wait_irq(void)
{
	/*
	 * Fix required to work around a problem which causes controller
	 * memory writes to be overtaken by interrupt requests.
	 *
	 * From the document:
	 * STBus USB Host 2.0 Controller
	 * Known Problems and Workaround
	 *
	 * 2.1 Interrupt Generation not linked with completed read/write
	 * on STBUS
	 *
	 * This limitation occurs because any writes issued by the AHB
	 * Master of the Synopsys Controller to the AHB Slave of the
	 * protocol converter is acknowledged by the STBUS target
	 * immediately to the AHB Master, before the transfer is
	 * completed by the STBUS Inititator.This causes the OHCI
	 * Interrupt to be generated before the transfer is completed
	 * on the STBUS. This may cause problems when the OHCI ISR
	 * successfully reads the memory location before the completion
	 * of the write by the STBUS Initiator of the DUT.
	 *
	 * This problems has also been observed in the EHCI controller.
	 */
	int count = 0;

	while ((readl(AHB2STBUS_PC_STATUS) & AHB2STBUS_PC_STATUS_IDLE) == 0) {
		count++;
		if (count == 100) {
			warn("OHCI AHB interrupt sync looped too many times");
			break;
		}
	}
}

void ST40_start_host_control(struct platform_device *dev);
