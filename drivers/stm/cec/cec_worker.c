/*
 * 
 * (c) 2010 konfetti, schischu
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/string.h>
#include <linux/slab.h>
#include <asm/uaccess.h>
#include <linux/delay.h>

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/string.h>

#include <linux/platform_device.h>

#include <asm/system.h>
#include <asm/io.h>



#include <linux/interrupt.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,26)
#include <asm/semaphore.h>
#else
#include <linux/semaphore.h>
#endif

//#include <linux/i2c.h> 

#include "cec_internal.h"
#include "cec_opcodes.h"

//----

static unsigned char isReceiving = 0;
static unsigned int indexOfRecvBuf = 0;
static unsigned char recvBuf[RECV_BUF_SIZE];


static unsigned char isSending = 0;
static unsigned int indexOfSendBuf = 0;
static unsigned int sizeOfSendBuf = 0;
static unsigned char sendBuf[SEND_BUF_SIZE];
//-----
#define DEFAULT_RETRY_COUNT 3
static unsigned char retries = DEFAULT_RETRY_COUNT;
//-----

static struct semaphore sendCommand;
static unsigned char sendCommandWithDelay = 0;

int sendMessageWithRetry(unsigned int len, unsigned char buf[], unsigned int retry)
{
  unsigned long Counter = 100;
  unsigned char value = 0;
  unsigned char src;
  unsigned char dst;

  while(isSending && --Counter)
  {
    // We are to fast, lets make a break
    udelay(10000);
  }

  if (Counter == 0)
  {
    printk("[CEC] %s - failed (Timeout!!!)\n", __func__);
    return -1;
  }

  sizeOfSendBuf = len;
  indexOfSendBuf = 0;

  isSending = 1;

  retries = retry;

  // The first byte contains src and dst
  src = getLogicalDeviceType(); //buf[0] >> 4;
  dst = buf[0] & 0x0F;

  // Correct address of the message
  buf[0] = (src << 4) + (dst & 0xF);

  memcpy(sendBuf, buf, len);

  cec_set_own_address(src);

  value = sendBuf[indexOfSendBuf];
  cec_write_data((unsigned int)value);

  indexOfSendBuf++;

  printk("[CEC] sendCommand - up ->\n");
  up(&sendCommand);
  printk("[CEC] sendCommand - up <-\n");

  /*if(sizeOfSendBuf == 1) // PING
  {
    cec_start_sending(1);
  }
  else
  {
    cec_start_sending(0);
  }*/
  return 0;
}

void sendMessage(unsigned int len, unsigned char buf[])
{
  sendMessageWithRetry(len, buf, DEFAULT_RETRY_COUNT);
  //printk("[CEC] sendCommand - up ->\n");
  //up(&sendCommand);
  //printk("[CEC] sendCommand - up <-\n");
}

void cec_worker_init(void)
{
  cec_set_own_address(/*DEVICE_TYPE_UNREG*/ 0x0F);

  printk("[CEC] *** CEC INIT ***\n");
  str_status(cec_get_status());
  str_error(cec_get_error());
  printk("[CEC] ~~~ CEC INIT ~~~\n");

  sendPingWithAutoIncrement();
}

/* *************************** */
/* worker task                 */
/* *************************** */

int cec_task(void* dummy);

static unsigned char notEndTask = 1;

void startTask(void)
{
  notEndTask = 0;
  kernel_thread(cec_task, NULL, 0);
  while(notEndTask == 0)
    udelay(5000);
}

void endTask(void)
{
  notEndTask = 0;
  up(&sendCommand);
  while(notEndTask == 0)
    udelay(10000);
}

int cec_task(void* dummy)
{
  printk("[CEC] cec_task started\n");

  daemonize("cec_repeater");

  allow_signal(SIGTERM);

  sema_init(&sendCommand, 0);
  notEndTask = 1;

  while(notEndTask)
  {
  printk("[CEC] sendCommand - down ->\n");
    down(&sendCommand);
  printk("[CEC] sendCommand - down <-\n");
    if(!notEndTask)
      break;
    //if(sendCommand || sendCommandWithDelay)
    {
      printk("[CEC] task - sendCommand || sendCommandWithDelay\n");
      if(sendCommandWithDelay--)
        udelay(10000);

      if(sizeOfSendBuf == 1) // PING
      {
        cec_start_sending(1);
      }
      else
      {
        cec_start_sending(0);
      }
      //sendCommand = 0;
      
    }
    //udelay(10000);
  }

  notEndTask = 1;
  printk("[CEC] task died\n");
  return 0;
}

/* *************************** */
/* irq handler                 */
/* *************************** */

irqreturn_t cec_interrupt(int irq, void *dev_id)
{
  u8 status, error;
  
  //printk("#### CEC INTERUPT ####\n");

  status = cec_get_status();
  error  = cec_get_error();
  
  //str_status(status);
  //str_error(error);

  if (status & CEC_STATUS_RECV_ERR) // Error while receiving
  {
    printk("[CEC] ++++ CEC ERROR (RECV) ++++\n");
    str_error(error);
    str_status(status);
    cec_acknowledge();
  }

  else if (status & CEC_STATUS_SEND_ERR) // Error while sending
  {
    printk("[CEC] ++++ CEC ERROR (SEND) ++++\n");
    str_error(error);
    str_status(status);
    if (status & CEC_STATUS_SEND_EOMSG) // End of Message
      cec_acknowledge_eom();
    else
      cec_acknowledge();

    isSending = 0;

    printk("[CEC] Retries: %d\n", retries);
    printk("[CEC] ---- CEC ERROR ----\n");

    if(getIsFirstKiss() == 1)
    {
        if (error & CEC_ERROR_ACK)
        {
          printk("[CEC] The above error is a wanted behaviour as this was a ping!\n");
          setIsFirstKiss(0);
        }
        else if(retries > 0)
        {
          sendMessageWithRetry(sizeOfSendBuf, sendBuf, retries - 1);
          //printk("[CEC] sendCommand - up ->\n");
          //sendCommandWithDelay++;
          //up(&sendCommand);
          //printk("[CEC] sendCommand - up <-\n");
        }
    }
    else if(retries > 0)
    {
      sendMessageWithRetry(sizeOfSendBuf, sendBuf, retries - 1);
      //printk("[CEC] sendCommand - up ->\n");
      //sendCommandWithDelay++;
      //up(&sendCommand);
      //printk("[CEC] sendCommand - up <-\n");
    }
  }

  else if (status & CEC_STATUS_RECV_BTF) // Receiving
  {
    if (status & CEC_STATUS_RECV_SOMSG) // Start of Message
    {
      isReceiving = 1;
      indexOfRecvBuf = 0; 
      memset(recvBuf, 0, RECV_BUF_SIZE);
    }

    recvBuf[indexOfRecvBuf] = cec_read_data();
    //printk("RECV 0x%0x\n", recvBuf[indexOfRecvBuf]);
    cec_acknowledge();
    indexOfRecvBuf++;

    if (status & CEC_STATUS_RECV_EOMSG) // End of Message
    {
      isReceiving = 0;
      printk("[CEC] ++++ CEC MESSAGE RECEIVED ++++\n");
      parseRawMessage(indexOfRecvBuf, recvBuf);
      printk("[CEC] ---- CEC MESSAGE RECEIVED ----\n");
    }
  }

  else if (status & CEC_STATUS_SEND_BTF) // Transmitting
  {
    if (status & CEC_STATUS_SEND_EOMSG) // End of Message
    {
      isSending = 0;
      cec_acknowledge();

      printk("[CEC] ++++ CEC MESSAGE SENT ++++\n");
      parseRawMessage(indexOfSendBuf, sendBuf);
      printk("[CEC] ---- CEC MESSAGE SENT ----\n");
    }
    else
    {
      cec_write_data(sendBuf[indexOfSendBuf]);
      indexOfSendBuf++;

      if(indexOfSendBuf == sizeOfSendBuf)
      {
        cec_acknowledge_eom();
      }
      else
      {
        cec_acknowledge();
      }
    }
  }
  
  return IRQ_HANDLED;
}

//----------------------------


