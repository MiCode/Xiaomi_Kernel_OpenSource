
/*******************************************************************************
* Copyright (C) 2016 Maxim Integrated Products, Inc., All rights Reserved.
* * This software is protected by copyright laws of the United States and
* of foreign countries. This material may also be protected by patent laws
* and technology transfer regulations of the United States and of foreign
* countries. This software is furnished under a license agreement and/or a
* nondisclosure agreement and may only be used or reproduced in accordance
* with the terms of those agreements. Dissemination of this information to
* any party or parties not specified in the license agreement and/or
* nondisclosure agreement is expressly prohibited.
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
* OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL MAXIM INTEGRATED BE LIABLE FOR ANY CLAIM, DAMAGES
* OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
* ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
* OTHER DEALINGS IN THE SOFTWARE.
*
* Except as contained in this notice, the name of Maxim Integrated
* Products, Inc. shall not be used except as stated in the Maxim Integrated
* Products, Inc. Branding Policy.
*
* The mere transfer of this software does not imply any licenses
* of trade secrets, proprietary technology, copyrights, patents,
* trademarks, maskwork rights, or any other form of intellectual
* property whatsoever. Maxim Integrated Products, Inc. retains all
* ownership rights.
*******************************************************************************
*/

/** @file sha256_hmac.c
*   @brief HMAC using SHA_256 
*/

#include <linux/time.h>

#ifdef USE_SOFTWARE_COPROC
#include "ucl_sha256.h"
#else
#include "sha256_software.h"
#endif

#define SHA256_HMAC
#include "sha256_hmac.h"

//int sha256_hmac(char *key, int key_len, char *message, int msg_len, char *mac);

//extern int dprintf(char *format, ...);
extern int sha_debug;

//---------------------------------------------------------------------------
/// Compute HMAC using SHA-256. 
///
/// @param[in] key
/// buffer for key
/// @param[in] ken_len
/// length of key
/// @param[in] message
/// buffer for message
/// @param[in] msg_len
/// length of message
/// @param[out] mac
/// 32 byte output mac
///
/// Restrictions:
///  Key length limited to 32 bytes.
///  Message Length is limited to 512 bytes.
///
/// @return
/// TRUE - command successful @n
/// FALSE - command failed
///
int sha256_hmac(char *key, int key_len, char *message, int msg_len, char *mac)
{
  /* int i;
   char thash[64];
   char cat_input_thash[1024];
   char cat_input_final[1024];

   int blocksize = 64;
   char opad[64];
   char ipad[64];

   memset(opad,0x5C,blocksize);
   memset(ipad,0x36,blocksize);

   // Compute an HMAC 
   if (sha_debug)
   {
      dprintf("\nkey in hex (length %d):\n", key_len);
      for (i = 0; i < key_len; i++)
      {
         dprintf("%02x",key[i]);
         if (((i + 1) % 8) == 0)
            dprintf("\n");
      }

      dprintf("\nmessage in hex (length %d):\n", msg_len);
      for (i = 0; i < msg_len; i++)
      {
         dprintf("%02x",message[i]);
         if (((i + 1) % 8) == 0)
            dprintf("\n");
      }
   }

   //  Check to see if key is larger then blocksize
   if (key_len > blocksize)
      return 0;  // Not supported

   // check for blocks too big
   if (msg_len > 512)
      return 0;

   // Loop through bytes of ipad/opad and XOR with key
   for (i = 0; i < key_len; i++)
   {
      // XOR ipad with key
      ipad[i] ^= key[i]; 
      // XOR opad with key
      opad[i] ^= key[i];
   }

   // thash = hash(ipad || message)
   memcpy(cat_input_thash,ipad,64);
   memcpy(&cat_input_thash[64],message,msg_len);

#ifdef USE_SOFTWARE_COPROC
   ucl_sha256(thash, cat_input_thash, 64 + msg_len);
#else
   ComputeSHA256(cat_input_thash, 64 + msg_len, FALSE, FALSE, thash);
#endif

   //   return hash(opad || thash) 
   memcpy(cat_input_final,opad,64);
   memcpy(&cat_input_final[64],thash,32);

#ifdef USE_SOFTWARE_COPROC
   ucl_sha256(mac, cat_input_final, 64 + 32);
#else
   ComputeSHA256(cat_input_final, 64 + 32, FALSE, FALSE, mac);
#endif

   // display calculated MAC
   if (sha_debug)
   {
      dprintf("\n\nCalculated HMAC Result:\n");
      for (i = 0; i < 32; i++)
      {
         dprintf("%02x",mac[i]);
         if (((i + 1) % 4) == 0)
            dprintf(" ");
      }
      dprintf("\n");
   }*/

   return 1;
}  

