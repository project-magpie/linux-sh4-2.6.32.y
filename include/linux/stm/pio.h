/*
 * include/linux/st_pio.h
 *
 * Copyright (c) 2004 STMicroelectronics Limited
 * Author: Stuart Menefy <stuart.menefy@st.com>
 *
 * ST40 General Purpose IO pins support.
 *
 * This layer enables other device drivers to configure PIO
 * pins, get and set their values, and register an interrupt
 * routine for when input pins change state.
 */

#ifndef _LINUX_ST_PIO_H_
#define _LINUX_ST_PIO_H_ 1

/*
 * The ST40GX1 has two blocks of PIO registers:
 *   3 in the ST40 core peripherals (PIO0 to PIO2)
 *   2 in the ST20 legacy peripherals (comms block) (IO_PIO0 and IO_PIO1)
 */
#define STPIO_PIO_BANK(n) ((n)+0)
#define STPIO_IO_PIO_BANK(n) ((n)+3)

/* Pin configuration constants */
/* Note that behaviour for modes 0, 6 and 7 differ between the ST40STB1
 * datasheet (implementation restrictions appendix), and the ST40
 * architectural defintions.
 */
#define STPIO_NONPIO		0	/* Non-PIO function (ST40 defn) */
#define STPIO_BIDIR_Z1     	0	/* Input weak pull-up (arch defn) */
#define STPIO_BIDIR		1	/* Bidirectonal open-drain */
#define STPIO_OUT		2	/* Output push-pull */
/*efine STPIO_BIDIR		3	 * Bidirectional open drain */
#define STPIO_IN		4	/* Input Hi-Z */
/*efine STPIO_IN		5	 * Input Hi-Z */
#define STPIO_ALT_OUT		6	/* Alt output push-pull (arch defn) */
#define STPIO_ALT_BIDIR		7	/* Alt bidir open drain (arch defn) */

struct stpio_pin;

/* Request and release exclusive use of a PIO pin */
struct stpio_pin* stpio_request_pin(unsigned portno, unsigned pinno,
				    const char* name, int direction);
struct stpio_pin* stpio_request_set_pin(unsigned portno, unsigned pinno,
				    const char* name, int direction, unsigned int value);
void stpio_free_pin(struct stpio_pin* pin);

/* Get, set value */
void stpio_set_pin(struct stpio_pin* pin, unsigned int value);
unsigned int stpio_get_pin(struct stpio_pin* pin);

/* Change the mode of an existing pin */
void stpio_configure_pin(struct stpio_pin* pin, int direction);

/* Interrupt on external value change */
void stpio_request_irq(struct stpio_pin* pin, int mode,
		       void (*handler)(struct stpio_pin *pin, void *dev),
		       void *dev);
void stpio_free_irq(struct stpio_pin* pin);
void stpio_enable_irq(struct stpio_pin* pin, int mode);
void stpio_disable_irq(struct stpio_pin* pin);

#endif /* _LINUX_ST_PIO_H_ */
