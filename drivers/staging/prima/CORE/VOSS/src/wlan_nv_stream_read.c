/*
 * Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/*===========================================================================
                       EDIT HISTORY FOR FILE

  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.

  $Header:$ $DateTime: $ $Author: $

  when        who        what, where, why
  --------    ---        --------------------------------------------------------
  04/10/13    kumarpra   nv stream layer creation
===========================================================================*/

#include "vos_memory.h"
#include "wlan_nv_stream.h"

_STREAM_BUF streamBuf;

static tANI_U32 deCodeData(tANI_U8 *ipdata, tANI_U32 length, tANI_U8 *opdata,
   tANI_U32 *currentIndex);

/*----------------------------------------------------------------------------
  \brief initReadStream() - stream Initialization
  This function will initialize stream read
  \param readBuf, length - ptr to read Buffer, number of bytes
  \return success on init
  \sa
--------------------------------------------------------------------------*/
_STREAM_RC initReadStream(tANI_U8 *readBuf, tANI_U32 length)
{
   _STREAM_RC rc = RC_SUCCESS;
   streamBuf.currentIndex = 0;
   streamBuf.totalLength = 0;
   streamBuf.totalLength = length;
   streamBuf.dataBuf = (_NV_STREAM_BUF *)&readBuf[0];
   return rc;
}

/*----------------------------------------------------------------------------
  \brief nextStream() - get next Stream in buffer
  This function will provide next stream in the buffere initalized
  \param readBuf, length - ptr to stream length, stream data
  \return success when stream length is non-zero else error
  \sa
--------------------------------------------------------------------------*/

_STREAM_RC nextStream(tANI_U32 *length, tANI_U8 *dataBuf)
{
   _STREAM_RC rc = RC_SUCCESS;

   if (streamBuf.currentIndex >= streamBuf.totalLength)
   {
       *length = 0;
   }
   else
   {
       *length = deCodeData(&streamBuf.dataBuf[streamBuf.currentIndex],
                    (streamBuf.totalLength - streamBuf.currentIndex), dataBuf,
                    &streamBuf.currentIndex);
   }

   if (*length == 0)
   {
      rc = RC_FAIL;
   }

   return rc;
}

/*----------------------------------------------------------------------------
  \brief decodeData() - decode the input data
  This function will decode stream read
  \param readBuf, length - ptr to input stream, length, output stream data,
  \index pointer
  \return success when stream length is non-zero else error
  \sa
--------------------------------------------------------------------------*/

tANI_U32 deCodeData(tANI_U8 *ipdata, tANI_U32 length, tANI_U8 *opdata,
   tANI_U32 *currentIndex)
{
   tANI_U16 oplength = 0;

   oplength = ipdata[0];
   oplength = oplength | (ipdata[1] << 8);

   vos_mem_copy(opdata, &ipdata[sizeof(tANI_U16)], oplength);

   *currentIndex = *currentIndex + sizeof(tANI_U16) + oplength;

   return oplength;
}
