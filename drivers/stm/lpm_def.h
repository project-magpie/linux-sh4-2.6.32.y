/*
 * <root>/drivers/stm/lpm_def.h
 *
 * Copyright (C) 2012 STMicroelectronics Limited
 *
 * Contributor:Francesco Virlinzi <francesco.virlinzi@st.com>
 * Author:Pooja Agarwal <pooja.agarwal@st.com>
 * Author:Udit Kumar <udit-dlh.kumar@st.com>
 *
 * May be copied or modified under the terms of the GNU General Public License.
 * See linux/COPYING for more information.
 */

#ifndef __LPM_DEF_H_
#define __LPM_DEF_H_
#include <linux/i2c.h>
/*
 * LPM protocol has following architecture
 * |	byte0	|	byte1	|	byte2	|	byte3	|
 * |	cmd id	|	transid	|	msgdata	|	msgdata	|
 *
 * cmd id is command id
 * transid is Transaction ID
 * msgdata is data part of message
 * msg data size can vary depending upon command
 *
 * In case of internal SBC, communication will be done using mailbox.
 * mailbox has depth of 16 bytes.
 * When message is greater than 16 bytes, such messages are treated as big
 * messages and will be send by directly writing into DMEM of SBC.
 *
 * In case of external SBC (7108), which is connected with SOC via i2c
 * communication will be done using i2c bus.
 *
 * Internal and external standby controllers expect message in
 * little endian mode
 */

/* Message command ids */

/* No operation */
#define LPM_MSG_NOP		0x0

/* Command id to retrieve version number */
#define LPM_MSG_VER		0x1

/* Command id to read current RTC value */
#define LPM_MSG_READ_RTC	0x3

/* Command id to trim RTC */
#define LPM_MSG_SET_TRIM	0x4

/* Command id to enter in passive standby mode */
#define LPM_MSG_ENTER_PASSIVE	0x5

/* Command id  to set watch dog timeout of SBC */
#define LPM_MSG_SET_WDT		0x6

/* Command id  to set new RTC value for SBC */
#define LPM_MSG_SET_RTC		0x7

/* Command id  to configure frontpanel display */
#define LPM_MSG_SET_FP		0x8

/* Command id to set wakeup time */
#define LPM_MSG_SET_TIMER	0x9

/* Command id to get status of SBC CPU */
#define LPM_MSG_GET_STATUS	0xA

/* Command id to generate reset */
#define LPM_MSG_GEN_RESET	0xB

/* Command id to set wakeup device */
#define LPM_MSG_SET_WUD		0xC

/* Command id to get wakeup device */
#define LPM_MSG_GET_WUD		0xD

/* Command id to offset in SBC memory */
#define LPM_MSG_LGWR_OFFSET	0x10

/* Command id to inform PIO setting */
#define LPM_MSG_SET_PIO		0x11

/* Command id to get advance features */
#define LPM_MSG_GET_ADV_FEA	0x12

/* Command id to set advance features */
#define LPM_MSG_SET_ADV_FEA	0x13

/* Command id to set key scan data */
#define LPM_MSG_SET_KEY_SCAN	0x14

/*
 * Command id to set IR information on SBC CPU,
 * these are IR keys on which SBC will do wakeup.
 */
#define LPM_MSG_SET_IR		0x41

/* Command id to get data associated with some wakeup device */
#define LPM_MSG_GET_IRQ		0x42

/*
 * Command id to inform trace data of SBC,
 * SBC can send trace data to host using this command
 */
#define LPM_MSG_TRACE_DATA	0x43

/* Command id to read message from SBC memory */
#define LPM_MSG_BKBD_READ	0x44

/* Command id inform SBC that write to SBC memory is done */
#define LPM_MSG_BKBD_WRITE	0x45

/* Bit-7 of command id used to mark reply from other CPU */
#define LPM_MSG_REPLY		0x80

/* Command for error */
#define LPM_MSG_ERR		0x82

/*
 * Version number of driver , this has following fields
 * protocol major and minor number
 * software major, minor and patch number
 * software release build, month, day and year
 */
#define LPM_MAJOR_PROTO_VER	1
#define LPM_MINOR_PROTO_VER	0
#define LPM_MAJOR_SOFT_VER	1
#define LPM_MINOR_SOFT_VER	3
#define LPM_PATCH_SOFT_VER	0
#define LPM_BUILD_MONTH		9
#define LPM_BUILD_DAY		7
#define LPM_BUILD_YEAR		12

/*
 * Maximum size of message data that can be send over mailbox,
 * mailbox is 16 byte deep, and 2 bytes are reserved for command
 * and transaction id.
 */
#define LPM_MAX_MSG_DATA	14

/*
 * Address of external standby controller which is connected with i2c bus.
 * This address is kept fixed in external standby controller's firmware.
 */
#define ADDRESS_OF_EXT_MC	0x94

/*
 * For internal standby controller
 * Various offset of LPM IP, These offsets are w.r.t LPM memory resources.
 * There are three LPM memory resources used
 * first is for SBC DMEM and IMEM,
 * second is for SBC mailbox,
 * third is for SBC configuration registers.
 */

/* SBC data memory offset as seen by Host w.r.t mem resource 1 */
#define DATA_OFFSET		0x010000

/* SBC program memory as offset seen by Host w.r.t mem resource 1 */
#define SBC_PRG_MEMORY_OFFSET	0x018000

/* Marker in elf file to indicate writing to program area */
#define SBC_PRG_MEMORY_ELF_MARKER	0x00400000

/* SBC mailbox offset as seen by Host w.r.t mem source 2 */
#define SBC_MBX_OFFSET		0

/* SBC configuration register offset as seen on Host w.r.t mem source 3 */
#define SBC_CONFIG_OFFSET	0

/*
 * Mailbox registers to be written by host,
 * SBC firmware will read below registers to get host message.
 * There are four such registers in mailbox each of 4 bytes.
 */

#define MBX_WRITE_STATUS1	(SBC_MBX_OFFSET + 0x004)
#define MBX_WRITE_STATUS2	(SBC_MBX_OFFSET + 0x008)
#define MBX_WRITE_STATUS3	(SBC_MBX_OFFSET + 0x00C)
#define MBX_WRITE_STATUS4	(SBC_MBX_OFFSET + 0x010)

/*
 * Mailbox registers to be read by host,
 * SBC firmware will write below registers to send message.
 * There are four such registers in mailbox each of 4 bytes.
 */

#define MBX_READ_STATUS1	(SBC_MBX_OFFSET + 0x104)
#define MBX_READ_STATUS2	(SBC_MBX_OFFSET + 0x108)
#define MBX_READ_STATUS3	(SBC_MBX_OFFSET + 0x10C)
#define MBX_READ_STATUS4	(SBC_MBX_OFFSET + 0x110)

/* To clear mailbox interrupt status */
#define MBX_READ_CLR_STATUS1	(SBC_MBX_OFFSET + 0x144)

/* To enable/disable mailbox interrupt on Host :RW */
#define MBX_INT_ENABLE		(SBC_MBX_OFFSET + 0x164)

/* To enable mailbox interrupt on Host : WO only set allowed */
#define MBX_INT_SET_ENABLE	(SBC_MBX_OFFSET + 0x184)

/* To disable mailbox interrupt on Host : WO only clear allowed */
#define MBX_INT_CLR_ENABLE	(SBC_MBX_OFFSET + 0x1A4)

/*
 * From host there are three type of message can be send to SBC
 * No reply expected from SBC i.e. reset SBC, Passive standby
 * Reply is expected from SBC i.e. get version etc.
 * Reply is expected but interrupts are disabled
 */
#define SBC_REPLY_NO		0
#define SBC_REPLY_YES		0x1
#define SBC_REPLY_NO_IRQ	0x2

/* Used to mask a byte */
#define BYTE_MASK		0xFF

/* For FP setting, mask to get owner's 2 bits */
#define OWNER_MASK		0x3

/* For FP setting, mask to brightness of LED 's 4 bits */
#define NIBBLE_MASK		0xF

/* Mask to get MSB of a half word */
#define M_BIT_MASK		0x7FFFF

/**
 * Mask for PIO level, interrupt and  direction
 * Bit 7 is used for level
 * bit 6 is used for interrupt and
 * bit 5 is used for direction of PIO
 */
#define PIO_LEVEL_SHIFT 	7
#define PIO_IT_SHIFT 		6
#define PIO_DIRECTION_SHIFT 	5

/* Mask to get MSB of a half word */
#define MASK_BIT_MASK		0x8000

/* Message send to SBC does not have msg data */
#define MSG_ZERO_SIZE		0

/* Transaction id will be generated by lpm itself */
#define MSG_ID_AUTO		0

/* To write 8 bit data into LPM */
#define lpm_write8(drv, offset, value)     iowrite8(value,	\
				(drv)->lpm_mem_base[0] + offset)

/* To read 8 bit data into LPM */
#define lpm_read8(drv, offset) ioread8((drv)->lpm_mem_base[0] + \
					offset)

/* To write 32 bit data into LPM */
#define lpm_write32(drv, index, offset, value)    iowrite32(value, \
			(drv)->lpm_mem_base[index] + offset)

/* To read 32 bit data into LPM */
#define lpm_read32(drv, idx, offset)	ioread32( \
			(drv)->lpm_mem_base[idx] + offset)

/**
 * lpm_message - LPM message for cross CPU communication
 * @command_id:	Command ID
 * @transaction_id: 	Transaction id
 * @msg_data:	Message data associated with this command
 *
 * Normally each message is less than 16 bytes
 * Any message more than 16 bytes considered as big message
 *
 * Internal and external SBC treats big message in different way
 *
 * In case of internal SBC, where size of mailbox is 16 byte
 * If message is large then 16 bytes then direct write to SBC memory is done
 *
 * In case of external SBC,
 * No special treatment for big messages, such messages are sent over
 * serial i2c bus
 */

struct lpm_message {
	unsigned char command_id;
	unsigned char transaction_id;
	unsigned char msg_data[LPM_MAX_MSG_DATA];
} __attribute__((packed));

/**
 * lpm_internal_send_msg - Internal struct of driver
 * @command_id:	command id
 * @msg :	message part of command
 * @msg_size:	size of message
 * @trans_id :	Transaction id
 * @reply_type :	reply type
 */
struct lpm_internal_send_msg {
	unsigned char command_id;
	unsigned char *msg;
	unsigned char msg_size;
	unsigned char trans_id;
};


/**
 * lpm_exchange_msg - Internal function to exchange message with SBC
 * @send_msg :	Message sent to SBC
 * @response :	Response of SBC
 */
int lpm_exchange_msg(struct lpm_internal_send_msg *send_msg,
		struct lpm_message *response);

/**
 * lpm_fw_proto_version - Internal function to get major firmware version
 */
char lpm_fw_proto_version(void);

#ifdef CONFIG_STM_LPM_RD_MONITOR
/* To monitor power pin on 7108*/
void lpm_stop_power_monitor(struct i2c_client *client);
int  lpm_start_power_monitor(struct i2c_client *client);
#endif


#endif /*__LPM_DEF_H*/
