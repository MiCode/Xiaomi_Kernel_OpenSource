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
#include "crypto.h"

#include "tomcrypt.h"

#define stick_debug(FMT, ...)                        \
    pr_info("stickapi_CRYPTO " FMT "\n", ##__VA_ARGS__)
#define stick_printf(FMT, ...)                       \
    pr_info("stickapi_CRYPTO " FMT "\n", ##__VA_ARGS__)

void crypto_init(void)
{
    /* register SHA512 */
    if (register_hash(&sha512_desc) == -1)
    {
        stick_printf("Error registering SHA512\n");
    }
    /* register AES */
    if (register_cipher(&aes_desc) == -1)
    {
        stick_printf("Error registering AES\n");
    }
}

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
                                     uint32_t P_DigestLen,
                                     const uint8_t *P_pSignature)
{
    curve25519_key k;
    int err;
    int stat;

    err = ed25519_import_raw(P_pPubKey, 32, PK_PUBLIC, &k);
    if (err != CRYPT_OK)
    {
        stick_debug("ERROR %s:%d: %d\n", __FILE__, __LINE__, err);
        return -1;
    }

    err = ed25519_verify(P_pDigest, P_DigestLen, P_pSignature, 64, &stat, &k);
    if (err != CRYPT_OK)
    {
        stick_debug("ERROR %s:%d: %d\n", __FILE__, __LINE__, err);
        return -1;
    }

    return ((stat == 1) ? 0 : -1);
}

/**
 *  @brief Perform an X25519 shared secret computation
 *  @param[in]  P_pPubKey       Buffer with the receiver's public key (32 byte)
 *  @param[in]  P_pPrivKey      Buffer with the caller's private key (32 byte)
 *  @param[out] P_pSharedSecret Buffer with the resulting shared secret (32 byte)
 *  @return     0 if correct, otherwise an error occurred
 */
int crypto_curve25519_ecdh(const uint8_t *P_pPubKey,
                                      const uint8_t *P_pPrivKey,
                                      uint8_t *P_pSharedSecret)
{
    int err;
    unsigned long len;
    curve25519_key pubkey, privkey;

    err = ed25519_import_raw(P_pPubKey, 32, PK_PUBLIC, &pubkey);
    if (err != CRYPT_OK)
    {
        stick_debug("ERROR %s:%d: %d\n", __FILE__, __LINE__, err);
        return -1;
    }

    err = ed25519_import_raw(P_pPrivKey, 32, PK_PRIVATE, &privkey);
    if (err != CRYPT_OK)
    {
        stick_debug("ERROR %s:%d: %d\n", __FILE__, __LINE__, err);
        return -1;
    }

    len = 32;
    err = x25519_shared_secret(&privkey, &pubkey, P_pSharedSecret, &len);
    if ((err != CRYPT_OK) || (len != 32))
    {
        stick_debug("ERROR %s:%d: %d %ld\n", __FILE__, __LINE__, err, len);
        return -1;
    }

    return 0;
}

/**
 *  @brief Perform an X25519 shared secret computation
 *  @param[in,out]  P_pPrivKey  Buffer containing 32 bytes of random (input)
 *                              and will contain the 32 bytes private key (output)
 *  @param[out]     P_pPubKey   Buffer with the public key (32 byte)
 *  @return         0 if correct, otherwise an error occurred
 */
int crypto_curve25519_keygen(uint8_t *P_pPrivKey,
                                        uint8_t *P_pPubKey)
{
    int err;
    unsigned long len;
    curve25519_key key;

    /* clamp the random */
    P_pPrivKey[0] &= 248;
    P_pPrivKey[31] &= 127;
    P_pPrivKey[31] |= 64;

    /* Import the random as priv key */
    if ((err = x25519_import_raw(P_pPrivKey, 32, PK_PRIVATE, &key)) != CRYPT_OK)
    {
        stick_debug("ERROR %s:%d: %d\n", __FILE__, __LINE__, err);
        return -1;
    }

    len = 32;
    if ((err = x25519_export(P_pPrivKey, &len, PK_PRIVATE, &key)) != CRYPT_OK)
    {
        stick_debug("ERROR %s:%d: %d\n", __FILE__, __LINE__, err);
        return -1;
    }
    len = 32;
    if ((err = x25519_export(P_pPubKey, &len, PK_PUBLIC, &key)) != CRYPT_OK)
    {
        stick_debug("ERROR %s:%d: %d\n", __FILE__, __LINE__, err);
        return -1;
    }

    return 0;
}

static omac_state omac;

/*!
 *  \brief Perform an AES CMAC encryption
 *  \param[in, out]
 *  \param[out]
 *  \return
 */
int crypto_aes_cmac_enc(const uint8_t *p, int32_t p_len,
                                   const uint8_t *k, int32_t k_len,
                                   int32_t exp_tag_size,
                                   uint8_t *t, int32_t *t_len)
{
    int idx, err;
    unsigned long len;

    /* get index of AES in cipher descriptor table */
    idx = find_cipher("aes");

    /* we would make up our symmetric key in "key[]" here */

    /* start the OMAC */
    if ((err = omac_init(&omac, idx, k, k_len)) != CRYPT_OK)
    {
        stick_debug("ERROR %s:%d: %d\n", __FILE__, __LINE__, err);
        return -1;
    }

    /* process a few octets */
    if ((err = omac_process(&omac, p, p_len)) != CRYPT_OK)
    {
        stick_debug("ERROR %s:%d: %d\n", __FILE__, __LINE__, err);
        return -1;
    }

    /* get result (presumably to use it somehow...) */
    len = (unsigned long)exp_tag_size;
    if ((err = omac_done(&omac, t, &len)) != CRYPT_OK)
    {
        stick_debug("ERROR %s:%d: %d\n", __FILE__, __LINE__, err);
        return -1;
    }
    *t_len = (int32_t)len;

    /* return */
    return 0;
}

/*!
 *  \brief
 *  \param[in, out]
 *  \param[out]
 *  \return
 */
int crypto_aes_cmac_dec(const uint8_t *p, int32_t p_len,
                                   const uint8_t *k, int32_t k_len,
                                   const uint8_t *t, int32_t t_len)
{
    // @TODO
    return -1;
}

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
                                  uint8_t *c, int32_t *c_len,
                                  uint8_t *t, int32_t t_len)
{
    int idx, err;
    unsigned long taglen = (unsigned long)t_len;

    /* get index of AES in cipher descriptor table */
    idx = find_cipher("aes");

    if ((err = ccm_memory(idx, k, k_len, NULL, n, (unsigned long)n_len, a, (unsigned long)a_len, (uint8_t *)p, (unsigned long)p_len,
                          c, t, &taglen, CCM_ENCRYPT)) != CRYPT_OK)
    {
        stick_debug("ERROR %s:%d: %d\n", __FILE__, __LINE__, err);
        return err;
    }
    *c_len = p_len;
    if (t_len != taglen)
    {
        stick_debug("ERROR %s:%d: %ld\n", __FILE__, __LINE__, taglen);
        return -1;
    }

    return 0;
}

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
                                  uint8_t *exp_p, int32_t exp_p_len)
{
    int idx, err;
    unsigned long taglen = (unsigned long)t_len;

    if (exp_p_len != c_len)
    {
        stick_debug("ERROR %s:%d: %d\n", __FILE__, __LINE__, c_len);
        return -1;
    }

    /* get index of AES in cipher descriptor table */
    idx = find_cipher("aes");

    if ((err = ccm_memory(idx, k, k_len, NULL, n, (unsigned long)n_len, a, (unsigned long)a_len, exp_p,
                          (unsigned long)c_len, (uint8_t *)c, (uint8_t *)t, &taglen, CCM_DECRYPT)) != CRYPT_OK)
    {
        stick_debug("ERROR %s:%d: %d\n", __FILE__, __LINE__, err);
        return -1;
    }

    if ((t_len != taglen))
    {
        stick_debug("ERROR %s:%d: %ld\n", __FILE__, __LINE__, taglen);
        return -1;
    }

    return 0;
}

// reverse bits in a byte
static uint8_t reverse(uint8_t b)
{
    b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
    b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
    b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
    return b;
}

// compute CRC16
uint16_t crypto_Crc16(uint8_t *pbuffer, uint16_t length)
{

    uint16_t Crc = 0xFFFF;

    static const uint16_t CrcTable[] = {// Byte Table, 0x1021 Left Shifting Polynomial - sourcer32@gmail.com
                                        0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7,
                                        0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
                                        0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6,
                                        0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
                                        0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485,
                                        0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
                                        0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4,
                                        0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
                                        0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
                                        0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
                                        0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12,
                                        0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
                                        0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41,
                                        0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
                                        0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70,
                                        0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
                                        0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F,
                                        0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
                                        0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E,
                                        0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
                                        0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D,
                                        0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
                                        0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C,
                                        0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
                                        0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB,
                                        0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
                                        0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
                                        0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
                                        0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9,
                                        0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
                                        0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8,
                                        0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0};

    while (length--)
    {
        uint8_t v = reverse(*pbuffer++);
        Crc = Crc ^ ((uint16_t)v << 8);

        Crc = (Crc << 8) ^ CrcTable[Crc >> 8]; // Process 8-bits at a time
    }

    return ~(reverse(Crc & 0xFF) << 8 | reverse(Crc >> 8));
}
