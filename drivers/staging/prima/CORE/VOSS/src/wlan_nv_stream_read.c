/*
 * Copyright (c) 2013, The Linux Foundation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *    * Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 *      copyright notice, this list of conditions and the following
 *      disclaimer in the documentation and/or other materials provided
 *      with the distribution.
 *    * Neither the name of The Linux Foundation nor the names of its
 *      contributors may be used to endorse or promote products derived
 *      from this software without specific prior written permission.
 *
 *THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
 *WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 *MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
 *ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 *BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 *BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 *WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 *OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
 *IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

   memcpy(opdata, &ipdata[sizeof(tANI_U16)], oplength);

   *currentIndex = *currentIndex + sizeof(tANI_U16) + oplength;

   return oplength;
}
