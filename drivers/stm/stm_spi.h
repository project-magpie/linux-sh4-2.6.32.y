/*
   --------------------------------------------------------------------

   stm_spi.h
   define and struct for SPI device driver
   based on STMicroelectronics SSC device

   --------------------------------------------------------------------

 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 *  THIS  SOFTWARE  IS PROVIDED   ``AS  IS'' AND   ANY  EXPRESS OR IMPLIED
 *  WARRANTIES,   INCLUDING, BUT NOT  LIMITED  TO, THE IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN
 *  NO  EVENT  SHALL   THE AUTHOR  BE    LIABLE FOR ANY   DIRECT, INDIRECT,
 *  INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 *  NOT LIMITED   TO, PROCUREMENT OF  SUBSTITUTE GOODS  OR SERVICES; LOSS OF
 *  USE, DATA,  OR PROFITS; OR  BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 *  ANY THEORY OF LIABILITY, WHETHER IN  CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 *  THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  You should have received a copy of the  GNU General Public License along
 *  with this program; if not, write  to the Free Software Foundation, Inc.,
 *  675 Mass Ave, Cambridge, MA 02139, USA.

*/
#ifndef STM_SPI
#define STM_SPI

#define SPI_IOCTL_WIDEFRAME     0x01
#define SPI_ARG_WIDE_16BITS     0x01
#define SPI_ARG_WIDE_8BITS      0x00

#define SPI_IOCTL_PHASE         0x02
#define SPI_ARG_PHASE_HIGH      0x01
#define SPI_ARG_PHASE_LOW       0x00

#define SPI_IOCTL_POLARITY      0x04
#define SPI_ARG_POLARITY_HIGH   0x01
#define SPI_ARG_POLARIT_LOWY    0x00

#define SPI_IOCTL_HEADING       0x08
#define SPI_ARG_HEADING_MSB     0x01
#define SPI_ARG_HEADING_LSB     0x00

#define SPI_IOCTL_CSACTIVE      0x10
#define SPI_ARG_CSACTIVE_HIGH   0x01
#define SPI_ARG_CSACTIVE_LOW    0x00

#define SPI_IOCTL_BUADRATE      0x20

#define SPI_IOCTL_ADDRESS       0x40

#define SPI_IOCTL_TIMEOUT       0x80

/*#define SPI_IOCTL_NOSELECTION   0x100*/


#ifdef __KERNEL__
#include <linux/stm/stssc.h>
#include <linux/device.h>
#include <linux/wait.h>
#include <linux/mutex.h>

extern struct bus_type spi_bus_type;

#define SPI_DEV_BUS_ADAPTER      0x01
#define SPI_DEV_CLIENT_ADAPTER   0x02

struct spi_transaction_t;

struct spi_device_t {
	unsigned int idx_dev;
        unsigned int dev_type; /* SPI_DEV_BUS_ADAPTER xor SPI_DEV_CLIENT_ADAPTER*/
	struct device dev;
	struct class_device *class_dev;
	unsigned long base;
	struct mutex      mutex_bus;
        wait_queue_head_t wait_queue;
	struct spi_transaction_t *trns;
};

struct spi_client_t {
	struct spi_device_t *dev;       /* the bus device used */
	struct stpio_pin *pio_chip;
	char *wr_buf;
	char *rd_buf;
	unsigned long config;		/* the clinet configuration */
	unsigned long timeout;
};

struct spi_client_t* spi_create_client(int bus_number);

int spi_client_release(struct spi_client_t* spi);

int spi_client_control(struct spi_client_t* spi, int cmd, int arg);

int spi_write(struct spi_client_t* spi, char *wr_buffer, size_t count);

int spi_read(struct spi_client_t* spi, char *rd_buffer, size_t count);

int spi_write_then_read(struct spi_client_t* spi,char *wr_buffer,
			char *rd_buffer, size_t count);

#endif

#endif
