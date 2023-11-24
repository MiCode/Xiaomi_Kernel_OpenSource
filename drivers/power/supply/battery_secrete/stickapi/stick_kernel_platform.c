/*!
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
  * Under STMicroelectronics’ intellectual property rights, the redistribution,
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
  *     “Open Source Terms” shall mean any open source license which requires as part of
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
#include "stick_platform.h"
#include "libsticknvm.h"
#include "st1wire.h"
#include "stick_NVM_attributes.h"
/* Include crypto library header */
#include "./libstickcrypto/crypto.h"

#include <linux/delay.h>

#define stick_debug(FMT, ...) pr_info("stickapi_PLA " FMT, ## __VA_ARGS__)



int stick_platform_init (void)
{
	// Initialize the NVM area
	stick_platform_nvm_load ((void *)&NVM_ATTRIBUTES.STICK_KEY_TABLE[0], sizeof(NVM_ATTRIBUTES));

	// Initialize the other libs
	crypto_init();
	return 0;
}

void stick_platform_wake (void)
{
	// nothing special
}

uint16_t stick_platform_Crc16 (uint8_t *pbuffer, uint16_t length)
{
	return crypto_Crc16(pbuffer, length);
}

void stick_platform_Delay_ms (uint32_t delay_val)
{
	msleep(delay_val);
}

uint32_t stick_platform_Random (void)
{
	return crypto_Random();
}

uint8_t stick_platform_ed25519_verify(const uint8_t *pPubKey,
						  uint8_t *pDigest,
						  uint32_t DigestLen,
						  uint8_t *pSignature)
{
	return crypto_ed25519_verify( pPubKey, pDigest, DigestLen, pSignature);
}


uint8_t stick_platform_curve25519_ecdh(const uint8_t *pPubKey,
						   const uint8_t *pPrivKey,
						   uint8_t       *pSharedSecret)
{
	return crypto_curve25519_ecdh(pPubKey,pPrivKey,pSharedSecret);
}


uint8_t stick_platform_curve25519_keygen(uint8_t *pPrivKey,
							 uint8_t *pPubKey)
{
	return crypto_curve25519_keygen(pPrivKey,pPubKey);
}


uint8_t stick_platform_aes_cmac_enc(const uint8_t *pPayload, int32_t payload_length,
						const uint8_t *pKey, int32_t key_length,
						uint32_t       exp_tag_size,
						uint8_t       *pTag, uint32_t *pTag_length)
{
	return crypto_aes_cmac_enc(pPayload,payload_length,
			     pKey,key_length,
				 exp_tag_size,
				 pTag,(int32_t*)pTag_length
	);
}


uint8_t stick_platform_aes_cmac_dec(const uint8_t *pPayload, int32_t payload_length,
						const uint8_t *pKey, int32_t key_length,
						const uint8_t *pTag, int32_t tag_length)
{
	return crypto_aes_cmac_dec(pPayload,payload_length,
			     pKey,key_length,pTag,tag_length
	);
}


uint8_t stick_platform_aes_ccm_enc(const uint8_t *pPlaintext, int32_t plaintext_length,
					   const uint8_t *pKey, int32_t key_len,
					   const uint8_t *pNonce, int32_t nonce_length,
					   const uint8_t *pAssoc_data, int32_t assoc_data_length,
					   uint8_t       *pEncryptedtext, int32_t *pEncryptedtext_length,
					   uint8_t       *pTag, int32_t tag_length)
{
	uint8_t ret = crypto_aes_ccm_enc( pPlaintext, plaintext_length,
			     pKey, key_len,
				 pNonce, nonce_length,
			     pAssoc_data, assoc_data_length,
				 pEncryptedtext, pEncryptedtext_length,
			     pTag, tag_length
	);

	return ret;
}

uint8_t stick_platform_aes_ccm_dec(const uint8_t *pEncryptedtext, int32_t Encryptedtext_length,
					   const uint8_t *pTag, int32_t tag_length,
					   const uint8_t *pKey, int32_t pKey_length,
					   const uint8_t *pNonce, int32_t nonce_length,
					   const uint8_t *pAssoc_data, int32_t assoc_data_length,
					   uint8_t *pPlaintext, int32_t plaintext_length)
{
	return crypto_aes_ccm_dec(pEncryptedtext, Encryptedtext_length,
			    pTag, tag_length,
			    pKey, pKey_length,
			    pNonce, nonce_length,
				pAssoc_data, assoc_data_length,
				pPlaintext, plaintext_length
	);
}


