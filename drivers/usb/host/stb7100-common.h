/*
 * STb7100 common OHCI/EHCI controller functions.
 *
 * Copyright (c) 2005 STMicroelectronics Limited
 * Author: Mark Glaisher <mark.glaisher@st.com>
 *
 * This file is licenced under the GPL.
 */

/*
 * This file attempts to support all the various flavours of USB wrappers,
 * thus some of the registers appear to overlap.
 *
 * Some of these register are described in ADCS 7518758 and 7618754
 */

/* Protocol converter registers (separate registers) */

/* The transaction opcode is programmed in this register */
#define AHB2STBUS_STBUS_OPC_OFFSET	0x00	/* From AHB2STBUS_PROTOCOL_BASE */
#define AHB2STBUS_STBUS_OPC_4BIT	0x00
#define AHB2STBUS_STBUS_OPC_8BIT	0x01
#define AHB2STBUS_STBUS_OPC_16BIT	0x02
#define AHB2STBUS_STBUS_OPC_32BIT	0x03
#define AHB2STBUS_STBUS_OPC_64BIT	0x04

/* The message length in number of packets is programmed in this register. */
#define AHB2STBUS_MSGSIZE_OFFSET	0x04	/* From AHB2STBUS_PROTOCOL_BASE */
#define AHB2STBUS_MSGSIZE_DISABLE	0x0
#define AHB2STBUS_MSGSIZE_2		0x1
#define AHB2STBUS_MSGSIZE_4		0x2
#define AHB2STBUS_MSGSIZE_8		0x3
#define AHB2STBUS_MSGSIZE_16		0x4
#define AHB2STBUS_MSGSIZE_32		0x5
#define AHB2STBUS_MSGSIZE_64		0x6

/* The chunk size in number of packets is programmed in this register */
#define AHB2STBUS_CHUNKSIZE_OFFSET	0x08	/* From AHB2STBUS_PROTOCOL_BASE */
#define AHB2STBUS_CHUNKSIZE_DISABLE	0x0
#define AHB2STBUS_CHUNKSIZE_2		0x1
#define AHB2STBUS_CHUNKSIZE_4		0x2
#define AHB2STBUS_CHUNKSIZE_8		0x3
#define AHB2STBUS_CHUNKSIZE_16		0x4
#define AHB2STBUS_CHUNKSIZE_32		0x5
#define AHB2STBUS_CHUNKSIZE_64		0x6


/* Protocol converter registers (combined register) */

#define AHB2STBUS_STBUS_CONFIG		0x04	/* From AHB2STBUS_PROTOCOL_BASE */


/* Wrapper Glue registers */

#define AHB2STBUS_STRAP_OFFSET		0x14	/* From AHB2STBUS_WRAPPER_GLUE_BASE */
#define AHB2STBUS_STRAP_PLL		0x08	/* undocumented */
#define AHB2STBUS_STRAP_8_BIT		0x00	/* ss_word_if */
#define AHB2STBUS_STRAP_16_BIT		0x04	/* ss_word_if */


/* Extensions to the standard USB register set */

/* Define a bus wrapper IN/OUT threshold of 128 */
#define AHB2STBUS_INSREG01_OFFSET	(0x10 + 0x84) /* From AHB2STBUS_EHCI_BASE */
#define AHB2STBUS_INOUT_THRESHOLD	0x00800080


int ST40_start_host_control(struct platform_device *dev);
