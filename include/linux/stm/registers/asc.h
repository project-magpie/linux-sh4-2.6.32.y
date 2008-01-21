#ifndef __STM_REGISTERS_ASC_H
#define __STM_REGISTERS_ASC_H



/* ASC baudrate generator */

#define ASC_BAUDRATE 0x00 /* RW, reset value: 1 */
/* This register is the dual function baudrate generator and reload value register. A read
 * from this register returns the content of the 16-bit counter/accumulator; writing to it
 * updates the 16-bit reload register.
 * If bit ASC_CTRL.RUN is 1, then any value written in the ASC_BaudRate register
 * is immediately copied to the timer. However, if the RUN bit is 0 when the register is
 * written, then the timer is not reloaded until the first comms clock cycle after the RUN
 * bit is 1.
 * The mode of operation of the baudrate generator depends on the setting of bit
 * ASC_CTRL.BAUDMODE.
 * Mode 0
 * When bit ASC_CTRL.BAUDMODE is set to 0, the baudrate and the required reload
 * value for a given baudrate can be determined by the following formulae:
 * where: ASCBaudRate represents the content of the ASC_BaudRate register,
 * taken as an unsigned 16-bit integer.
 * fcomms is the frequency of the comms clock (clock channel CLK_IC_DIV2).
 * Mode 0 should be used for all baudrates below 19.2 Kbaud.
 * Table 39 lists commonly used baudrates with the required reload values and the
 * approximate deviation errors for an example baudrate with a comms clock of
 * 100 MHz.
 * Mode 1
 * When bit ASC_CTRL.BAUDMODE is set to 1, the baudrate is given by:
 * where: fcomms is the comms clock frequency and ASCBaudRate is the value written to
 * the ASC_BaudRate register. Mode 1 should be used for baudrates of 19.2 Kbytes
 * and above as it has a lower deviation error than mode 0 at higher frequencies. */

#define ASC_BAUDRATE__RELOAD_VAL__SHIFT 0
#define ASC_BAUDRATE__RELOAD_VAL__MASK  0x0000ffff



/* ASC transmit buffer */

#define ASC_TX_BUF 0x04 /* W, reset value: 0 */
/* A transmission is started by writing to the transmit buffer register ASC_TX_BUF.
 * Serial data transmission is only possible when the baudrate generator bit
 * ASC_CTRL.RUN is set to 1.
 * Data transmission is double buffered or uses a FIFO, so a new character may be
 * written to the transmit buffer register before the transmission of the previous
 * character is complete. This allows characters to be sent back to back without gaps. */

#define ASC_TX_BUF__TD__SHIFT 0
#define ASC_TX_BUF__TD__MASK  0x000001ff
/* TD[8]:
 * Transmit buffer data D8, or parity bit, or wake up bit or undefined depending on the operating
 * mode (the setting of field ASC_CTRL.MODE).
 * If the MODE field selects an 8-bit frame then this bit should be written as 0.
 * TD[7]:
 * Transmit buffer data D7, or parity bit depending on the operating mode (the setting of field
 * ASC_CTRL.MODE).
 * TD[6:0]: transmit buffer data D6 to D0 */



/* ASC receive buffer */

#define ASC_RX_BUF 0x08 /* R, reset value: 0 */
/* Serial data reception is only possible when the baudrate generator bit
 * ASC_CTRL.RUN is set to 1. */

#define ASC_RX_BUF__RD__SHIFT 0
#define ASC_RX_BUF__RD__MASK  0x000001ff
/* RD[8]:
 * Receive buffer data D8, or parity error bit, or wake up bit depending on the operating mode (the
 * setting of field ASC_CTRL.MODE)
 * If the MODE field selects an 8-bit frame then this bit is undefined. Software should ignore this
 * bit when reading 8-bit frames
 * RD[7]:
 * Receive buffer data D7, or parity error bit depending on the operating mode (the setting of field
 * ASC_CTRL.MODE)
 * RD[6:0]:
 * Receive buffer data D6 to D0 */



/* ASC control */

#define ASC_CTRL 0x0c /* RW, reset value: 0 */
/* This register controls the operating mode of the ASC and contains control bits for
 * mode and error check selection, and status flags for error identification.
 * Programming the mode control field (MODE) to one of the reserved combinations
 * may result in unpredictable behavior. Serial data transmission or reception is only
 * possible when the baudrate generator run bit (RUN) is set to 1. When the RUN bit is
 * set to 0, TxD is 1. Setting the RUN bit to 0 immediately freezes the state of the
 * transmitter and receiver. This should only be done when the ASC is idle.
 * Serial data transmission or reception is only possible when the baudrate generator
 * RUN bit is set to 1. A transmission is started by writing to the transmit buffer register
 * ASC_Tx_Buf. */

#define ASC_CTRL__MODE__SHIFT 0
#define ASC_CTRL__MODE__MASK  0x00000007
/* MODE: ASC mode control
 * 000: reserved	001: 8-bit data
 * 010: reserved	011: 7-bit data + parity
 * 100: 9-bit data	101: 8-bit data + wake up bit
 * 110: reserved	111: 8-bit data + parity */

#define ASC_CTRL__STOPBITS__SHIFT 3
#define ASC_CTRL__STOPBITS__MASK  0x00000003
/* STOPBITS: number of stop bits selection
 * 00: 0.5 stop bits	01: 1 stop bits
 * 10: 1.5 stop bits	11: 2 stop bits */

#define ASC_CTRL__PARITYODD__SHIFT 5
#define ASC_CTRL__PARITYODD__MASK  0x00000001
/* PARITYODD: parity selection
 * 0: even parity (parity bit set on odd number of 1's in data)
 * 1: odd parity (parity bit set on even number of 1's in data) */

#define ASC_CTRL__LOOPBACK__SHIFT 6
#define ASC_CTRL__LOOPBACK__MASK  0x00000001
/* LOOPBACK: loopback mode enable bit
 * 0: standard transmit/receive mode	1: loopback mode enabled */

#define ASC_CTRL__RUN__SHIFT 7
#define ASC_CTRL__RUN__MASK  0x00000001
/* RUN: baudrate generator run bit
 * 0: baudrate generator disabled (ASC inactive)	1: baudrate generator enabled */

#define ASC_CTRL__RX_EN__SHIFT 8
#define ASC_CTRL__RX_EN__MASK  0x00000001
/* RX_EN: receiver enable bit
 * 0: receiver disabled	1: receiver enabled */

#define ASC_CTRL__SC_EN__SHIFT 9
#define ASC_CTRL__SC_EN__MASK  0x00000001
/* SC_EN: smartcard enable
 * 0: smartcard mode disabled	1: smartcard mode enabled */

#define ASC_CTRL__FIFO_EN__SHIFT 10
#define ASC_CTRL__FIFO_EN__MASK  0x00000001
/* FIFO_EN: FIFO enable:
 * 0: FIFO disabled	1: FIFO enabled */

#define ASC_CTRL__CTS_EN__SHIFT 11
#define ASC_CTRL__CTS_EN__MASK  0x00000001
/* CTS_EN: CTS enable
 * 0: CTS ignored		1: CTS enabled */

#define ASC_CTRL__BAUDMODE__SHIFT 12
#define ASC_CTRL__BAUDMODE__MASK  0x00000001
/* BAUDMODE: baudrate generation mode
 * 0: baud counter decrements, ticks when it reaches 1	1: baud counter added to itself, ticks when
 * there is a carry */

#define ASC_CTRL__NACK_DISABLE__SHIFT 13
#define ASC_CTRL__NACK_DISABLE__MASK  0x00000001
/* NACK_DISABLE: NACKing behavior control
 * 0: NACKing behavior in smartcard mode	1: no NACKing behavior in smartcard mode */



/* ASC interrupt enable */

#define ASC_INT_EN 0x10 /* RW, reset value: 0 */

#define ASC_INT_EN__RX_BUFFULL__SHIFT 0
#define ASC_INT_EN__RX_BUFFULL__MASK  0x00000001
/* RX_BUFFULL: receiver buffer full interrupt enable
 * 0: receiver buffer full interrupt disable	1: receiver buffer full interrupt enable */

#define ASC_INT_EN__TX_EMPTY__SHIFT 1
#define ASC_INT_EN__TX_EMPTY__MASK  0x00000001
/* TX_EMPTY: transmitter empty interrupt enable
 * 0: transmitter empty interrupt disable	1: transmitter empty interrupt enable */

#define ASC_INT_EN__TX_HALFEMPTY__SHIFT 2
#define ASC_INT_EN__TX_HALFEMPTY__MASK  0x00000001
/* TX_HALFEMPTY: transmitter buffer half empty interrupt enable
 * 0: transmitter buffer half empty interrupt disable	1: transmitter buffer half empty interrupt
 * enable */

#define ASC_INT_EN__PARITY_ERR__SHIFT 3
#define ASC_INT_EN__PARITY_ERR__MASK  0x00000001
/* PARITY_ERR: parity error interrupt enable:
 * 0: parity error interrupt disable	1: parity error interrupt enable */

#define ASC_INT_EN__FRAME_ERR__SHIFT 4
#define ASC_INT_EN__FRAME_ERR__MASK  0x00000001
/* FRAME_ERR: framing error interrupt enable
 * 0: framing error interrupt disable	1: framing error interrupt enable */

#define ASC_INT_EN__OVERRUN_ERROR__SHIFT 5
#define ASC_INT_EN__OVERRUN_ERROR__MASK  0x00000001
/* OVERRUN_ERR: overrun error interrupt enable
 * 0: overrun error interrupt disable	1: overrun error interrupt enable */

#define ASC_INT_EN__TIMEOUT_NOTEMPTY__SHIFT 6
#define ASC_INT_EN__TIMEOUT_NOTEMPTY__MASK  0x00000001
/* TIMEOUT_NOTEMPTY: time out when not empty interrupt enable
 * 0: time out when input FIFO or buffer not empty interrupt disable
 * 1: time out when input FIFO or buffer not empty interrupt enable */

#define ASC_INT_EN__TIMEOUT_IDLE__SHIFT 7
#define ASC_INT_EN__TIMEOUT_IDLE__MASK  0x00000001
/* TIMEOUT_IDLE: time out when the receiver FIFO is empty interrupt enable
 * 0: time out when the input FIFO or buffer is empty interrupt disable
 * 1: time out when the input FIFO or buffer is empty interrupt enable */

#define ASC_INT_EN__RX_HALFFULL__SHIFT 8
#define ASC_INT_EN__RX_HALFFULL__MASK  0x00000001
/* RX_HALFFULL: receiver FIFO is half full interrupt enable
 * 0: receiver FIFO is half full interrupt disable	1: receiver FIFO is half full interrupt enable */



/* ASC interrupt status */

#define ASC_STA 0x14 /* R, reset value: 3 (Rx buffer full and Tx buffer empty) */

#define ASC_STA__RX_BUFFULL__SHIFT 0
#define ASC_STA__RX_BUFFULL__MASK  0x00000001
/* RX_BUFFULL: Receiver FIFO not empty (FIFO operation) or buffer full (double buffered
 * operation)
 * 0: receiver FIFO is empty or buffer is not full	1: receiver FIFO is not empty or buffer is full */

#define ASC_STA__TX_EMPTY__SHIFT 1
#define ASC_STA__TX_EMPTY__MASK  0x00000001
/* TX_EMPTY: Transmitter empty flag
 * 0: transmitter is not empty	1: transmitter is empty */

#define ASC_STA__TX_HALFEMPTY__SHIFT 2
#define ASC_STA__TX_HALFEMPTY__MASK  0x00000001
/* TX_HALFEMPTY: Transmitter FIFO at least half empty flag or buffer empty
 * 0: the FIFOs are enabled and the transmitter FIFO is more than half full (more than eight
 * characters) or the FIFOs are disabled and the transmit buffer is not empty.
 * 1: the FIFOs are enabled and the transmitter FIFO is at least half empty (eight or less
 * characters) or the FIFOs are disabled and the transmit buffer is empty */

#define ASC_STA__PARITY_ERR__SHIFT 3
#define ASC_STA__PARITY_ERR__MASK  0x00000001
/* PARITY_ERR: Input parity error flag:
 * 0: no parity error	1: parity error */

#define ASC_STA__FRAME_ERR__SHIFT 4
#define ASC_STA__FRAME_ERR__MASK  0x00000001
/* FRAME_ERR: Input frame error flag
 * 0: no framing error	1: framing error (stop bits not found) */

#define ASC_STA__OVERRUN_ERR__SHIFT 5
#define ASC_STA__OVERRUN_ERR__MASK  0x00000001
/* OVERRUN_ERR: Overrun error flag
 * 0: no overrun error
 * 1: overrun error, that is, data received when the input buffer is full */

#define ASC_STA__TONE__SHIFT 6
#define ASC_STA__TONE__MASK  0x00000001
/* TONE: Time out when the receiver FIFO or buffer is not empty
 * 0: no time out or the receiver FIFO or buffer is empty
 * 1: time out when the receiver FIFO or buffer is not empty */

#define ASC_STA__TOE__SHIFT 7
#define ASC_STA__TOE__MASK  0x00000001
/* TOE: Time out when the receiver FIFO or buffer is empty
 * 0: no time out or the receiver FIFO or buffer is not empty
 * 1: time out when the receiver FIFO or buffer is empty */

#define ASC_STA__RX_HALFFULL__SHIFT 8
#define ASC_STA__RX_HALFFULL__MASK  0x00000001
/* RX_HALFFULL: Receiver FIFO is half full
 * 0: the receiver FIFO contains eight characters or less
 * 1: the receiver FIFO contains more than eight characters */

#define ASC_STA__TX_FULL__SHIFT 9
#define ASC_STA__TX_FULL__MASK  0x00000001
/* TX_FULL: Transmitter FIFO or buffer is full
 * 0: the FIFOs are enabled and the transmitter FIFO is empty or contains less than 16
 * characters or the FIFOs are disabled and the transmit buffer is empty
 * 1: the FIFOs are enabled and the transmitter FIFO contains 16 characters or the FIFOs are
 * disabled and the transmit buffer is full */

#define ASC_STA__NKD__SHIFT 10
#define ASC_STA__NKD__MASK  0x00000001
/* NKD: Transmission failure acknowledgement by receiver in smartcard mode.
 * 0: data transmitted successfully
 * 1: data transmission unsuccessful (data NACKed by smartcard) */



/* ASC guard time */

#define ASC_GUARDTIME 0x18 /* RW, reset value: 0 */
/* This register defines the number of stop bits and the delay of the assertion of the
 * interrupt TX_EMPTY by a programmable number of baud clock ticks. The value in the
 * register is the number of baud clock ticks to delay assertion of TX_EMPTY. This
 * value must be in the range 0 to 511. */

#define ASC_GUARDTIME__GUARDTIME__SHIFT 0
#define ASC_GUARDTIME__GUARDTIME__MASK  0x000001ff



/* ASC time out */

#define ASC_TIMEOUT 0x1c /* RW, reset value: 0 */
/* The time out period in baudrate ticks. The ASC contains an 8-bit time out counter,
 * which reloads from ASC_TIMEOUT when one or more of the following is true:
 * If none of these conditions hold, the counter decrements to 0 at every baudrate tick.
 * The TONE (time out when not empty) bit of the ASC_STA register is 1 when the
 * input FIFO is not empty and the time out counter is zero. The TIMEOUT_IDLE bit of
 * the ASC_STA register is 1 when the input FIFO is empty and the time out counter is
 * zero.
 * When the software has emptied the input FIFO, the time out counter resets and starts
 * decrementing. If no more characters arrive, when the counter reaches zero the
 * TIMEOUT_IDLE bit of the ASC_STA register is set. */

#define ASC_TIMEOUT__TIMEOUT__SHIFT 0
#define ASC_TIMEOUT__TIMEOUT__MASK  0x000000ff



/* ASC transmit FIFO reset */

#define ASC_TX_RST 0x20 /* W */
/* Reset the transmit FIFO. Registers ASC_TX_RST have no storage associated with
 * them. A write of any value to these registers resets the corresponding transmitter
 * FIFO. */



/* ASC receive FIFO reset */

#define ASC_RX_RST 0x24 /* W */
/* Reset the receiver FIFO. The registers ASC_RX_RST have no actual storage
 * associated with them. A write of any value to one of these registers resets the
 * corresponding receiver FIFO. */



/* ASC number of retries on transmission */

#define ASC_RETRIES 0x28 /* RW, reset value: 1 */
/* Defines the number of transmissions attempted on a piece of data before the UART
 * discards the data. If a transmission still fails after NUM_RETRIES, the NKD bit is set
 * in the ASC_STA register where it can be read and acted on by software. This
 * register does not have to be reinitialized after a NACK error. */

#define ASC_RETRIES__NUM_RETRIES__SHIFT 0
#define ASC_RETRIES__NUM_RETRIES__MASK  0x000000ff



#endif
