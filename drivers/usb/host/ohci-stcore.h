/*
 * OHCI HCD (Host Controller Driver) for USB
 *
 * (C) copyright STMicroelectronics 2005
 * Author: Mark Glaisher <mark.glaisher@st.com>
 *
 * STMicroelectronics on-chip USB host controller Bus Glue.
 * Based on the StrongArm ohci-sa1111.c file
 *
 * May be copied or modified under the terms of the GNU General Public
 * License.  See linux/COPYING for more information.
 */

#ifndef ST40_ASSIST_H
#define ST40_ASSIST_H

#if defined(CONFIG_CPU_SUBTYPE_STI5528)

	#define CFG_BASE_ADDRESS         0xb9162000
	#define USB_OHCI_0_BASE          0xb9141000
	#define USB_OHCI_1_BASE          0xb9142000

	#define STBUS_USB_0_STATUS_REG   (USB_OHCI_0_BASE + 0x58)
	#define STBUS_USB_1_STATUS_REG   (USB_OHCI_1_BASE + 0x58)
	#define STBUS_USB_0_MASK_REG     (USB_OHCI_0_BASE + 0x5c)
	#define STBUS_USB_1_MASK_REG     (USB_OHCI_1_BASE + 0x5c)

	#define SYSTEM_CONFIG10          (CFG_BASE_ADDRESS + 0x58)

#elif defined(CONFIG_CPU_SUBTYPE_STM8000)

	#define USB_OHCI_0_BASE         0xb4400000
	#define FS_B_BASE              (0xb0420000)
	#define FS_CONFIG_CLK_3        (FS_B_BASE + 0x18)
	#define FS_CONFIG_GENERIC_INFO (FS_B_BASE + 0x00)

	#define STBUS_USB_STATUS_REG    (USB_OHCI_0_BASE + 0x58)
	#define STBUS_USB_MASK_REG      (USB_OHCI_0_BASE + 0x5c)
#endif

#define INT_RMT_WAKEUP			0x01
#define INT_BUF_ACCESS			0x02
#define INT_NEW_FRAME			0x04
#define INT_GENERAL			0x08
#define STBUS_USB_MASK_DEFAULT  (INT_RMT_WAKEUP | INT_BUF_ACCESS | INT_GENERAL)
#define USB_POWER_ENABLE 		((  1 <<4) | ( 1 <<11)) /*power enable usb blk 1/2*/

#endif

