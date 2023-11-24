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
#include <linux/kernel.h>
#include <linux/module.h>

#ifdef __cplusplus
extern "C" {
#endif

void crypto_init (void);

/**
  * @brief      Verify a signature, using EDDSA and curve edwards25519
  * @param[in]  P_pPubKey       Buffer with the public key (32 byte)
  * @param[in]  P_pDigest       Buffer with the digest to sign
  * @param[in]  P_DigestLen     Byte length of the digest
  * @param[in]  P_pSignature    Buffer with the signature (64 byte)
  * @return     0 if the signature is correct, otherwise an error occurred
  *             (internal error or the signature is wrong)
  */
int crypto_ed25519_verify(const uint8_t *P_pPubKey,
                          const uint8_t *P_pDigest,
                          uint32_t      P_DigestLen,
                          const uint8_t *P_pSignature);

/**
 *  @brief Perform an X25519 shared secret computation
 *  @param[in]  P_pPubKey       Buffer with the receiver's public key (32 byte)
 *  @param[in]  P_pPrivKey      Buffer with the caller's private key (32 byte)
 *  @param[out] P_pSharedSecret Buffer with the resulting shared secret (32 byte)
 *  @return     0 if correct, otherwise an error occurred
 */
int crypto_curve25519_ecdh(const uint8_t *P_pPubKey,
                           const uint8_t *P_pPrivKey,
                           uint8_t       *P_pSharedSecret);

/**
 *  @brief Perform an X25519 shared secret computation
 *  @param[in,out]  P_pPrivKey  Buffer containing 32 bytes of random (input)
 *                              and will contain the 32 bytes private key (output)
 *  @param[out]     P_pPubKey   Buffer with the public key (32 byte)
 *  @return         0 if correct, otherwise an error occurred
 */
int crypto_curve25519_keygen(uint8_t *P_pPrivKey,
                             uint8_t *P_pPubKey);

/*!
 *  \brief Perform an AES CMAC encryption
 *  \param[in, out]
 *  \param[out]
 *  \return
 */
int crypto_aes_cmac_enc(const uint8_t *p, int32_t p_len,
                        const uint8_t *k, int32_t k_len,
                        int32_t       exp_tag_size,
                        uint8_t       *t, int32_t *t_len);

/*!
 *  \brief
 *  \param[in, out]
 *  \param[out]
 *  \return
 */
int crypto_aes_cmac_dec(const uint8_t *p, int32_t p_len,
                        const uint8_t *k, int32_t k_len,
                        const uint8_t *t, int32_t t_len);


/*!
 *  \brief
 *  \param[in, out]
 *  \param[out]
 *  \return
 */
int crypto_aes_ccm_enc(const uint8_t *p, int32_t p_len,
                       const uint8_t *k, int32_t k_len,
                       const uint8_t *n, int32_t n_len,
                       const uint8_t *a, int32_t a_len,
                       uint8_t       *c, int32_t *c_len,
                       uint8_t       *t, int32_t t_len);

/*!
 *  \brief
 *  \param[in]  *c    Pointer to the encrypted payload
 *  \param[in]  c_len Length of encrypted payload
 *  \param[in]  *t    Pointer to the tag
 *  \param[in]  t_len Length of the tag
 *  \param[in]  *k    pointer to the key
 *  \param[in]  k_len Length of the key
 *  \param[in]  *n    pointer to Nonce
 *  \param[in]  n_len Length of the Nonce
 *  \param[in]  *a    pointer to Associated Data
 *  \param[in]  n_len Length of the Associated Data
 *  \param[out] exp_p pointer to PlainText payload
 *  \param[out] exp_p_len Length of the PlainText payload
 *  \return
 */
int crypto_aes_ccm_dec(const uint8_t *c, int32_t c_len,
                       const uint8_t *t, int32_t t_len,
                       const uint8_t *k, int32_t k_len,
                       const uint8_t *n, int32_t n_len,
                       const uint8_t *a, int32_t a_len,
                       uint8_t *exp_p, int32_t exp_p_len);

/*!
  * \brief      Compute a 16-bit crc value on specific 8-bit buffer of buffe length
  * \param[in]  pbuffer pointer to crc input buffer
  * \param[in]  length  input buffer length
  * \return     16-bit CRC value
  */
uint16_t crypto_Crc16 (uint8_t *pbuffer, uint16_t length);

/*!
  * \return     32b random data
  */
uint32_t crypto_Random (void);

#ifdef __cplusplus
}
#endif
