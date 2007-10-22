/*
   --------------------------------------------------------------------

   stm_ssc.h
   define and struct for STMicroelectronics SSC device

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

#ifndef STM_SSC_H
#define STM_SSC_H 1

#include <linux/platform_device.h>
#include <linux/wait.h>
#include <linux/stm/pio.h>
#include <linux/mutex.h>
#include <asm/io.h>


/* SSC Baud Rate generator */
#define SSC_BRG                  0x0
/* SSC Transmitter Buffer  */
#define SSC_TBUF                 0x4
/* SSC Receive Buffer      */
#define SSC_RBUF                 0x8
/*SSC Control              */
#define SSC_CTL                  0xC
#define SSC_CTL_DATA_WIDTH_9     0x8
#define SSC_CTL_BM               0xf
#define SSC_CTL_HB               0x10
#define SSC_CTL_PH               0x20
#define SSC_CTL_PO		 0x40
#define SSC_CTL_SR		 0x80
#define SSC_CTL_MS		 0x100
#define SSC_CTL_EN		 0x200
#define SSC_CTL_LPB		 0x400
#define SSC_CTL_EN_TX_FIFO       0x800
#define SSC_CTL_EN_RX_FIFO       0x1000
#define SSC_CTL_EN_CLST_RX       0x2000

/* SSC Interrupt Enable */
#define SSC_IEN               	0x10
#define SSC_IEN_RIEN		0x1
#define SSC_IEN_TIEN		0x2
#define SSC_IEN_TEEN		0x4
#define SSC_IEN_REEN		0x8
#define SSC_IEN_PEEN		0x10
#define SSC_IEN_AASEN		0x40
#define SSC_IEN_STOPEN		0x80
#define SSC_IEN_ARBLEN		0x100
#define SSC_IEN_NACKEN		0x400
#define SSC_IEN_REPSTRTEN	0x800
#define SSC_IEN_TX_FIFO_HALF	0x1000
#define SSC_IEN_RX_FIFO_HALF_FULL	0x4000

/* SSC Status */
#define SSC_STA                   0x14
#define SSC_STA_RIR		  0x1
#define SSC_STA_TIR		  0x2
#define SSC_STA_TE		  0x4
#define SSC_STA_RE		  0x8
#define SSC_STA_PE		 0x10
#define SSC_STA_CLST		 0x20
#define SSC_STA_AAS		 0x40
#define SSC_STA_STOP		 0x80
#define SSC_STA_ARBL		0x100
#define SSC_STA_BUSY		0x200
#define SSC_STA_NACK		0x400
#define SSC_STA_REPSTRT		0x800
#define SSC_STA_TX_FIFO_HALF	0x1000
#define SSC_STA_TX_FIFO_FULL    0x2000
#define SSC_STA_RX_FIFO_HALF    0x4000

/*SSC I2C Control */
#define SSC_I2C               	0x18
#define SSC_I2C_I2CM		0x1
#define SSC_I2C_STRTG		0x2
#define SSC_I2C_STOPG		0x4
#define SSC_I2C_ACKG		0x8
#define SSC_I2C_AD10		0x10
#define SSC_I2C_TXENB		0x20
#define SSC_I2C_REPSTRTG	0x800
#define SSC_I2C_I2CFSMODE	0x1000
/* SSC Slave Address */
#define SSC_SLAD              	0x1C
/* SSC I2C bus repeated start hold time */
#define SSC_REP_START_HOLD    	0x20
/* SSC I2C bus start hold time */
#define SSC_START_HOLD        	0x24
/* SSC I2C bus repeated start setup time */
#define SSC_REP_START_SETUP   	 0x28
/* SSC I2C bus repeated stop setup time */
#define SSC_DATA_SETUP		0x2C
/* SSC I2C bus stop setup time */
#define SSC_STOP_SETUP		0x30
/* SSC I2C bus free time */
#define SSC_BUS_FREE		0x34

/* SSC Tx FIFO Status */
#define SSC_TX_FSTAT            0x38
#define SSC_TX_FSTAT_STATUS     0x07

/* SSC Rx FIFO Status */
#define SSC_RX_FSTAT            0x3C
#define SSC_RX_FSTAT_STATUS     0x07

/* SSC Prescaler value value for clock */
#define SSC_PRE_SCALER_BRG      0x40

/* SSC Clear bit operation */
#define SSC_CLR			0x80
#define SSC_CLR_SSCAAS 		0x40
#define SSC_CLR_SSCSTOP 	0x80
#define SSC_CLR_SSCARBL 	0x100
#define SSC_CLR_NACK    	0x400
#define SSC_CLR_REPSTRT     	0x800

/* SSC Noise suppression Width */
#define SSC_AGFR		0x100
/* SSC Clock Prescaler */
#define SSC_PRSC		0x104
#define SSC_PRSC_VALUE          0x0f

/* SSC Max delay width*/
#define SSC_MAX_DELAY		0x108

/* SSC Prescaler for delay in dataout */
#define SSC_PRSC_DATAOUT	0x10c

#define SSC_TXFIFO_SIZE         0x8
#define SSC_RXFIFO_SIZE         0x8
/*
 * The I2C timing register could be ready
 * for normal or fast rate
 */
#define SSC_I2C_READY_NORMAL    0x0
#define SSC_I2C_READY_FAST      0x1
struct ssc_t {
	struct stpio_pin *pio_clk;
	struct stpio_pin *pio_data;
	struct stpio_pin *pio_data_in;
	wait_queue_head_t wait_queue;
	struct mutex	  mutex_bus;
	void *base;
	void (*irq_function) (void *);
	void *irq_private_data;
        unsigned char    i2c_timing;
        struct platform_device pdev;
};

struct ssc_t *ssc_device_request(unsigned int ssc_id);

/*
 *  How many ssc device are available on this platform
 */
unsigned int ssc_device_available(void);

/*
 *  The input clock for each SSC device
 */

unsigned int ssc_get_clock(void);

/*
 *  To say if the ssc_is ssc can support I2C and/or SPI protocol
 */

#define SSC_I2C_CAPABILITY  0x1
#define SSC_SPI_CAPABILITY  0x2

unsigned int ssc_capability(unsigned int ssc_id);
/*
 *   To request the bus access
 *   The user registers also the function and the data that
 *   they want use in the IRQ_Function
 */
void ssc_request_bus(struct ssc_t *, void (*irq_function) (void *),
		     void *irq_data);

/*
 *   To release the bus
 */
void ssc_release_bus(struct ssc_t *);

/*
   we have to use the following macro
   to access the SSC I/O Memory
*/
#define ssc_store16(ssc , offset, value) iowrite16(value,ssc->base+offset)
#define ssc_store8( ssc , offset, value) iowrite8( value,ssc->base+offset)

#define ssc_load16( ssc,offset)          ioread16(ssc->base+offset)
#define ssc_load8(  ssc,offset)	         ioread8( ssc->base+offset)

/*
 *   This macro could be used to built the capability field
 *   of struct plat_ssc_data for each SoC
 */
#define ssc_ability(idx_ssc, cap)  \
         ( cap & (SSC_I2C_CAPABILITY | SSC_SPI_CAPABILITY ) ) << (idx_ssc*2)

#define ssc0_ability(cap)  ssc_ability(0,cap)
#define ssc1_ability(cap)  ssc_ability(1,cap)
#define ssc2_ability(cap)  ssc_ability(2,cap)
#define ssc3_ability(cap)  ssc_ability(3,cap)
#define ssc4_ability(cap)  ssc_ability(4,cap)

#endif				/* STM_SSC_H */
