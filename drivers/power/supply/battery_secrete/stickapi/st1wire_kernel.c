/**
  * attention
  *
  * <h2><center>&copy; COPYRIGHT 2020 STMicroelectronics</center></h2>
  *
  * STSAFE DRIVER SOFTWARE LICENSE AGREEMENT (SLA0088)
  *
  * BY INSTALLING, COPYING, DOWNLOADING, ACCESSING OR OTHERWISE USING THIS SOFTWARE
  * OR ANY PART THEREOF (AND THE RELATED DOCUMENTATION) FROM STMICROELECTRONICS
  * INTERNATIONAL N.V, SWISS BRANCH AND/OR ITS AFFILIATED COMPANIES (STMICROELECTRONICS),
  * THE RECIPIENT, ON BEHALF OF HIMSELF OR HERSELF, OR ON BEHALF OF ANY ENTITY BY WHICH
  * SUCH RECIPIENT IS EMPLOYED AND/OR ENGAGED AGREES TO BE BOUND BY THIS SOFTWARE LICENSE
  * AGREEMENT.
  *
  * Under STMicroelectronics? intellectual property rights, the redistribution,
  * reproduction and use in source and binary forms of the software or any part thereof,
  * with or without modification, are permitted provided that the following conditions
  * are met:
  * 1.  Redistribution of source code (modified or not) must retain any copyright notice,
  *     this list of conditions and the disclaimer set forth below as items 10 and 11.
  * 2.  Redistributions in binary form, except as embedded into a microcontroller or
  *     microprocessor device or a software update for such device, must reproduce any
  *     copyright notice provided with the binary code, this list of conditions, and the
  *     disclaimer set forth below as items 10 and 11, in documentation and/or other
  *     materials provided with the distribution.
  * 3.  Neither the name of STMicroelectronics nor the names of other contributors to this
  *     software may be used to endorse or promote products derived from this software or
  *     part thereof without specific written permission.
  * 4.  This software or any part thereof, including modifications and/or derivative works
  *     of this software, must be used and execute solely and exclusively in combination
  *     with a secure microcontroller device from STSAFE family manufactured by or for
  *     STMicroelectronics.
  * 5.  No use, reproduction or redistribution of this software partially or totally may be
  *     done in any manner that would subject this software to any Open Source Terms.
  *     ?Open Source Terms? shall mean any open source license which requires as part of
  *     distribution of software that the source code of such software is distributed
  *     therewith or otherwise made available, or open source license that substantially
  *     complies with the Open Source definition specified at www.opensource.org and any
  *     other comparable open source license such as for example GNU General Public
  *     License(GPL), Eclipse Public License (EPL), Apache Software License, BSD license
  *     or MIT license.
  * 6.  STMicroelectronics has no obligation to provide any maintenance, support or
  *     updates for the software.
  * 7.  The software is and will remain the exclusive property of STMicroelectronics and
  *     its licensors. The recipient will not take any action that jeopardizes
  *     STMicroelectronics and its licensors' proprietary rights or acquire any rights
  *     in the software, except the limited rights specified hereunder.
  * 8.  The recipient shall comply with all applicable laws and regulations affecting the
  *     use of the software or any part thereof including any applicable export control
  *     law or regulation.
  * 9.  Redistribution and use of this software or any part thereof other than as  permitted
  *     under this license is void and will automatically terminate your rights under this
  *     license.
  * 10. THIS SOFTWARE IS PROVIDED BY STMICROELECTRONICS AND CONTRIBUTORS "AS IS" AND ANY
  *     EXPRESS, IMPLIED OR STATUTORY WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
  *     WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
  *     OF THIRD PARTY INTELLECTUAL PROPERTY RIGHTS, WHICH ARE DISCLAIMED TO THE FULLEST
  *     EXTENT PERMITTED BY LAW. IN NO EVENT SHALL STMICROELECTRONICS OR CONTRIBUTORS BE
  *     LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  *     DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
  *     LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
  *     THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
  *     NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
  *     ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  * 11. EXCEPT AS EXPRESSLY PERMITTED HEREUNDER, NO LICENSE OR OTHER RIGHTS, WHETHER EXPRESS
  *     OR IMPLIED, ARE GRANTED UNDER ANY PATENT OR OTHER INTELLECTUAL PROPERTY RIGHTS OF
  *     STMICROELECTRONICS OR ANY THIRD PARTY.
  ******************************************************************************
  */

#include <linux/mutex.h>
// header for kernel module to kernel module communication
#include "stick1w.h"

// Library header
#include "st1wire.h"


#define stick_debug(FMT, ...) pr_info("stickapi_OW " FMT, ## __VA_ARGS__)
#define stick_printf(FMT, ...) pr_info("stickapi_OW " FMT, ## __VA_ARGS__)

DEFINE_MUTEX(owlock);

#define STICK_RECV_MAX_LEN 256

struct stick_device *kernel_sd = NULL;



int8_t st1wire_init (void) {
  int ret;
  bool debug = false;

  stick_debug("st1wire_init enter\n");
  // Check we are not init yet.
  mutex_init(&owlock);
  if (kernel_sd != NULL) {
    stick_printf("ST1Wire already init!\n");
    ret = -1;
  } else {
    // try open the driver, return error if fails.
    ret = stick_kernel_open(&kernel_sd, debug);
    if (ret < 0) {
      stick_printf("Unable to open stick (%d)\n", ret);
      ret = -1;
    } else {
      ret = 0;
    }
  }

  // done
  return ret;
}

int8_t st1wire_deinit (void) {
  int ret = -1;
  // Check we are init.
  mutex_lock(&owlock);
  if (kernel_sd == NULL) {
    stick_printf("ST1Wire already deinit!\n");
  } else {
    // try open the driver, return error if fails.
    ret = stick_kernel_release(kernel_sd);
    if (ret < 0) {
      stick_printf("Unable to close stick device (%d) \n", ret);
      ret = -1;
    } else {
      kernel_sd = NULL;
      ret = 0;
    }
  }
  mutex_unlock(&owlock);

  // done
  return ret;
}


int8_t st1wire_SendFrame(uint8_t bus_addr, uint8_t speed, uint8_t* frame , uint8_t frame_length)
{
  int ret = -1;
  ssize_t rret;
  // Check we are init.
  mutex_lock(&owlock);
  if (kernel_sd == NULL) {
    stick_printf("ST1Wire is not init!\n");
  } else {

    // Write the frame, the kernel will handle sending the bytes.
    rret = stick_send_frame(kernel_sd, frame, frame_length);
    if (rret != ST1WIRE_OK) {
      // stick_printf("Unable to write to stick (%d) \n", rret);
      ret = -1;
    } else {
      ret = ST1WIRE_OK;
    }
  }
  mutex_unlock(&owlock);

  // done
  return ret;
}

int8_t st1wire_ReceiveFrame(uint8_t bus_addr , uint8_t speed, uint8_t* frame , uint8_t* pframe_length)
{
  ssize_t ret = -1;
  uint8_t recvBuf[STICK_RECV_MAX_LEN]; // can contain any frame from STICK

  // Check we are init.
  mutex_lock(&owlock);
  if (kernel_sd == NULL) {
    stick_debug("ST1Wire is not init!\n");
  } else {
    // read from driver, we always pass a buffer big enough, then trucate to actual received size
    ret = stick_read_frame(kernel_sd, recvBuf);
    if (ret < 0) {
      // stick_printf("Unable to read from stick (%zd)\n", ret);
    } else {
      *pframe_length = (uint8_t)ret;
      if (ret > 0) {
        memcpy(frame, recvBuf, ret);
        ret = ST1WIRE_OK;
      } else {
        ret = -1;
        stick_printf(" <ERROR\n");
      }
    }
  }
  mutex_unlock(&owlock);

  // done
  return (int8_t)ret;
}

void st1wire_wake (uint8_t bus_addr)
{
  // @TODO
  // use an IOCTL if this is needed
  return;
}

void st1wire_recovery (uint8_t bus_addr, uint8_t speed)
{
  stick_debug("Recovery sequence\n");

  // Check we are init.
  mutex_lock(&owlock);
  if (kernel_sd == NULL) {
    stick_debug("ST1Wire is not init!\n");
  } else {
    stick_kernel_reset(kernel_sd);
  }
  mutex_unlock(&owlock);

  return;
}
