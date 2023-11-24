/*!
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

#ifndef STICK_PLATFORM_H
#define STICK_PLATFORM_H

#include <linux/kernel.h>
#include <linux/module.h>


/*--------------------- STICK Library configuration --------------------------- */

#define STICK_MAX_API_RETRY 50
#define STICK_MAX_REPEAT 50

/*--------------------- STICK platform HAL functions --------------------------- */


/*!
  * \brief      Platform initialization function
  * \param[in]  none
  * \param[out] none
  * \return     none
  */
int stick_platform_init (void);


/*!
  * \brief      Compute a 16-bit crc value on specific 8-bit buffer of buffe length
  * \param[in]  pbuffer pointer to crc input buffer
  * \param[in]  length  input buffer length
  * \return     16-bit CRC value
  */
uint16_t stick_platform_Crc16 (uint8_t *pbuffer, uint16_t length);

/*!
  * \brief      Perform a delay of "delay_val" ms
  * \param[in]  delay_val length of the delay in milliseconds
  * \return     None
  */
void stick_platform_Delay_ms (uint32_t delay_val);


/*!
  * \brief      Generate a uint32_t random number
  * \return     32-bit random number
  */
uint32_t stick_platform_Random (void);

/*!
  * \brief      Verify a signature, using EDDSA and curve edwards25519
  * \param[in]  pPubKey       Buffer with the public key (32 byte)
  * \param[in]  pDigest       Buffer with the digest to sign
  * \param[in]  digestLen     Byte length of the digest
  * \param[in]  pSignature    Buffer with the signature (64 byte)
  * \return     0 if the signature is correct, otherwise an error occurred
  *             (internal error or the signature is wrong)
  */
uint8_t stick_platform_ed25519_verify(const uint8_t *pPubKey,
									  uint8_t 		*pDigest,
									  uint32_t 		 digestLen,
									  uint8_t 		*pSignature);


/*!
 *  \brief Perform an X25519 shared secret computation
 *  \param[in]  pPubKey       Buffer with the receiver's public key (32 byte)
 *  \param[in]  pPrivKey      Buffer with the caller's private key (32 byte)
 *  \param[out] pSharedSecret Buffer with the resulting shared secret (32 byte)
 *  \return     0 if correct, otherwise an error occurred
 */
uint8_t stick_platform_curve25519_ecdh(const uint8_t *pPubKey,
									   const uint8_t *pPrivKey,
									   uint8_t       *pSharedSecret);

/*!
 *  \brief Perform an X25519 shared secret computation
 *  \param[in, out] pPrivKey  Buffer containing 32 bytes of random (input)
 *                            and will contain the 32 bytes private key (output)
 *  \param[out]     pPubKey   Buffer with the public key (32 byte)
 *  \return         0 if correct, otherwise an error occurred
 */
uint8_t stick_platform_curve25519_keygen(uint8_t *pPrivKey,
							 	 	 	 uint8_t *pPubKey);

/*!
 *  \brief Perform an AES CMAC encryption
 *  \param[in] 		*pPayload 		Pointer to Payload
 *  \param[in] 		payload_length 	Length of the payload in byte
 *  \param[in] 		*pKey 			Pointer to key
 *  \param[in] 		key_length 		Length of the key in byte
 *  \param[in] 		exp_tag_size 	Expected  tag size in byte
 *  \param[in] 		key_length 		Length of the key in byte
 *  \param[out] 	*pTag			Pointer to Tag
 *  \param[out]  	*pTag_length	Pointer to Tag length value output
 *  \return 	0 if correct, otherwise an error occurred
 */
uint8_t stick_platform_aes_cmac_enc(const uint8_t *pPayload, 	 int32_t   payload_length,
									const uint8_t *pKey, 		 int32_t   key_length,
									uint32_t       exp_tag_size,
									uint8_t       *pTag, 		 uint32_t *pTag_length);

/*!
 *  \brief Perform an AES CMAC decryption
 *  \param[in] 		*pPayload 		Pointer to Payload
 *  \param[in] 		payload_length 	Length of the payload in byte
 *  \param[in] 		*pKey 			Pointer to key
 *  \param[in] 		key_length 		Length of the key in byte
 *  \param[in]		*pTag			Pointer to Tag
 *  \param[in]  	tag_length		Pointer to Tag length value output
 *  \return 0 if correct, otherwise an error occurred
 */
uint8_t stick_platform_aes_cmac_dec(const uint8_t *pPayload, int32_t payload_length,
									const uint8_t *pKey, 	 int32_t key_length,
									const uint8_t *pTag, 	 int32_t tag_length);


/*!
 *  \brief Perform an AES CCM encryption
 *  \param[in]  *pPlaintext 			Pointer to the plaintext data
 *  \param[in]  plaintext_length 		Length of the plaintext data
 *  \param[in]  *pKey 					Pointer to the key
 *  \param[in]  key_length 				Length of the key
 *  \param[in]  *pNonce 				Pointer to Nonce
 *  \param[in]  nonce_length 			Length of the Nonce
 *  \param[in]  *pAssocData 			Pointer to Associated Data
 *  \param[in]	assocData_length 		Length of the Associated Data
 *  \param[out] *pEncryptedtext 		Pointer to the encrypted payload
 *  \param[out]  pEncryptedtext_length 	Length of encrypted payload
 *  \param[out] *pTag 					Pointer to the tag
 *  \param[out] tag_length 				Length of the tag
 *  \return 0 if correct, otherwise an error occurred
 */
uint8_t stick_platform_aes_ccm_enc(const uint8_t *pPlaintext, 	  int32_t  plaintext_length,
								   const uint8_t *pKey, 		  int32_t  key_length,
								   const uint8_t *pNonce, 		  int32_t  nonce_length,
								   const uint8_t *pAssocData, 	  int32_t  assocData_length,
								   uint8_t       *pEncryptedtext, int32_t *pEncryptedtext_length,
								   uint8_t       *pTag, 		  int32_t  tag_length);


/*!
 *  \brief
 *  \param[in]  *pEncryptedtext Pointer to the encrypted payload
 *  \param[in]  encryptedtext_length Length of encrypted payload
 *  \param[in]  *pTag Pointer to the tag
 *  \param[in]  tag_length Length of the tag
 *  \param[in]  *pKey pointer to the key
 *  \param[in]  key_length Length of the key
 *  \param[in]  *pNonce pointer to Nonce
 *  \param[in]  nonce_length Length of the Nonce
 *  \param[in]  *pAssocData pointer to Associated Data
 *  \param[in]  assocData_length Length of the Associated Data
 *  \param[out] *pPlaintext pointer to PlainText payload
 *  \param[out] plaintext_length Length of the PlainText payload
 *  \return
 */
uint8_t stick_platform_aes_ccm_dec(const uint8_t *pEncryptedtext, int32_t encryptedtext_length,
								   const uint8_t *pTag, 		  int32_t tag_length,
								   const uint8_t *pKey, 		  int32_t key_length,
								   const uint8_t *pNonce, 		  int32_t nonce_length,
								   const uint8_t *pAssocData, 	  int32_t assocData_length,
								   uint8_t *pPlaintext, 	  int32_t plaintext_length);



#endif /*STICK_PLATFORM_H*/
