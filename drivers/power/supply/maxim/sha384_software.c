/*******************************************************************************
* Copyright (C) 2017 Maxim Integrated Products, Inc., All Rights Reserved.
* Copyright (C) 2021 XiaoMi, Inc.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
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
//  SHA3_HMAC - HMAC using SHA3-256
#include "ucl_sha3.h"
#include <linux/string.h>
#define SHA3_256_HMAC
#include "sha384_software.h"

//---------------------------------------------------------------------------
/// Compute HMAC using SHA3-256.
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
int sha3_256_hmac(unsigned char *key, int key_len, unsigned char *message, int msg_len, unsigned char *mac)
{
	int i;
	unsigned char thash[256];
	unsigned char tmac[256];
	unsigned char cat_input_thash[1024];
	unsigned char cat_input_final[1024];

	int blocksize = 136;
	int hashsize = 32;
	unsigned char opad[136];
	unsigned char ipad[136];

	memset(opad, 0x5C, blocksize);
	memset(ipad, 0x36, blocksize);

	//  Check to see if key is larger then blocksize
	if (key_len > blocksize)
		return 0;  // Not supported

	// check for blocks too big
	if (msg_len > 512)
		return 0;

	// Loop through bytes of ipad/opad and XOR with key
	for (i = 0; i < key_len; i++) {
		// XOR ipad with key
		ipad[i] ^= key[i];
		// XOR opad with key
		opad[i] ^= key[i];
	}

	// thash = hash(ipad || message)
	memcpy(cat_input_thash, ipad, blocksize);
	memcpy(&cat_input_thash[blocksize], message, msg_len);

	ucl_sha3_256(thash, cat_input_thash, blocksize + msg_len);

	// return hash(opad || thash)
	memcpy(cat_input_final, opad, blocksize);
	memcpy(&cat_input_final[blocksize], thash, hashsize);

	ucl_sha3_256(tmac, cat_input_final, blocksize + hashsize);

	memcpy(mac, tmac, hashsize);

	return 1;
}
