/*
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

#ifndef SPI_STM_H
#define SPI_STM_H 1

/*#include "../../include/linux/ioctl.h"*/
/*
 *  The following macro are used for ioctl
 */
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

#endif				/* SPI_STM_H */
