/*
  *
  *
  * COPYRIGHT 2016 STMicroelectronics
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
  *     in the software, except the limited rights specified here under.
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

/* Includes ------------------------------------------------------------------*/

#include "stick_api.h"

#include "stick_NVM_attributes.h"
#include "libsticknvm.h"
#include "st1wire.h"
#include "stick_core.h"
#include "stick_platform.h"
#include <linux/slab.h>

#ifdef STICK_DEBUG_LOG
#define stick_debug(FMT, ...) pr_info("stickapi_API " FMT "\n", ##__VA_ARGS__)
#else
#define stick_debug(FMT, ...)
#endif
#define stick_printf(FMT, ...) pr_info("stickapi_API " FMT "\n", ##__VA_ARGS__)

/* Private defines -----------------------------------------------------------*/
#define STICK_LIB_VERSION_MAIN (0x04U) /*!< [31:24] main version */
#define STICK_LIB_VERSION_SUB1 (0x02U) /*!< [23:16] sub1 version */
#define STICK_LIB_VERSION_SUB2 (0x00U) /*!< [15:8]  sub2 version */
#define STICK_LIB_VERSION_RC (0x01U)   /*!< [7:0]  release candidate */
#define STICK_LIB_VERSION ((STICK_LIB_VERSION_MAIN << 24U) | (STICK_LIB_VERSION_SUB1 << 16U) | (STICK_LIB_VERSION_SUB2 << 8U) | (STICK_LIB_VERSION_RC))

Non_Volatile_Attributes_t nvm_attribute_copy;

/* Static functions prototypes ------------------------------------------------------*/
void _change_buffer_endianess(uint8_t *pbuffer, uint8_t buffer_size);

/*!
  * \brief      Send Keys and challenge to the cartridge and receive information required for Cartridge and pen pairing process
  * \param[in]  *pSTICK		STICK handler pointer
  * \return     STICK_OK if  successful
  *             STICK_AUTHENTICATION_ERROR in case of error in certificate verification
  */
static stick_ReturnCode_t _stick_Start_Pairing(stick_Handler_t *pSTICK);

/*!
 * \brief : this function sends establish Key pairingFinalize command to the cartridge
 *          and perform in parallel of command processing in the STICK the following crypto commands :
 *          - Verification of public key certificate
 *          - Verification of signature from start or resume response
 *          - X25519 for computing the shared secret and deriving the shared key
 * \param[in] *pSTICK Pointer to target STICK handler
 * \result : status STICK_OK if successful or STICK_AUTHENTICATION_ERROR in case of error in certificate verification
 */
static stick_ReturnCode_t _stick_Resume_Pairing(stick_Handler_t *pSTICK);

/*!
 * \brief : this function sends establish Key pairingFinalize command to the cartridge
 *          and perform in parallel of command processing in the STICK the following crypto commands :
 *          - Verification of public key certificate
 *          - Verification of signature from start or resume response
 *          - X25519 for computing the shared secret and deriving the shared key
 * \param[in] *pSTICK Pointer to target STICK handler
 * \result : status STICK_OK if successful or STICK_AUTHENTICATION_ERROR in case of error in certificate verification
 */
static stick_ReturnCode_t _stick_Finalize_Pairing(stick_Handler_t *pSTICK);

/*!
 * \brief : This function perform a single open session process (call several time in case of retry operation on API)
 * \param[in] *pSTICK Pointer to target STICK handler
 * \result : status STICK_OK if successful otherwise STICK error code  */
static stick_ReturnCode_t _stick_open_session(stick_Handler_t *pSTICK);

/*!
 * \brief : This function perform a single open session process (call several time in case of retry operation on API)
 * \param[in] *pSTICK Pointer to target STICK handler
 * \result : status STICK_OK if successful otherwise STICK error code  */
static stick_ReturnCode_t _stick_open_sigma_key_session(stick_Handler_t *pSTICK);

/* Static Functions Definition ------------------------------------------------------*/

void _change_buffer_endianess(uint8_t *pbuffer, uint8_t buffer_size)
{
	uint8_t i, tmp;

	for (i = 0; i < buffer_size / 2; ++i)
	{
		tmp = pbuffer[i];
		pbuffer[i] = pbuffer[buffer_size - 1 - i];
		pbuffer[buffer_size - 1 - i] = tmp;
	}
}

static stick_ReturnCode_t _stick_Start_Pairing(stick_Handler_t *pSTICK)
{
	uint32_t *pRandom;
	uint32_t crypto_res = 1;
	stick_ReturnCode_t ret = STICK_INVALID_PARAMETER;

	/* - Check parameters */
	if (pSTICK == NULL)
	{
		return ret;
	}

	/* - Store certificate in STICK handler context */
	memcpy(pSTICK->ctx.public_key_certificate.raw_data,
		   pSTICK->rsp.payload.get_public_key_certificate.raw_data,
		   106);

	/* - Establish Key pairing[start] (UM 6.3 - 1) */
	pSTICK->cmd.header = STICK_CMD_ESTABLISH_PAIRING_KEY_START;

	/* - Copy static Host_ECDH_key_pair.public_key  into STICK TX frame buffer */
	memcpy(pSTICK->cmd.payload.establish_pairing_key_start.host_ecdh_public_key,
		   Host_ECDH_key_pair.public_key,
		   32);

	/* - Generate Random challenge */
	pRandom = (uint32_t *)pSTICK->cmd.payload.establish_pairing_key_start.host_challenge;
	*(pRandom++) = stick_platform_Random();
	*(pRandom++) = stick_platform_Random();
	*(pRandom++) = stick_platform_Random();
	*(pRandom++) = stick_platform_Random();
	memcpy(pSTICK->ctx.message_digest.Host_challenge, pSTICK->cmd.payload.establish_pairing_key_start.host_challenge, 16);

	/* - Generate Random Magic_value Number */
	pRandom = (uint32_t *)pSTICK->cmd.payload.establish_pairing_key_start.magic_value;
	*(pRandom++) = stick_platform_Random();
	*(pRandom++) = stick_platform_Random();
	*(pRandom++) = stick_platform_Random();
	*(pRandom++) = stick_platform_Random();

	/* - Set transfer length */
	pSTICK->cmd.length = 64;

	/* - Send the Establish Pairing Key Start command to the cartridge */
	ret = stick_Transmit(pSTICK, STICK_POLLING_RETRY_COUNT_DEFAULT);
	if (ret != STICK_OK)
	{
		stick_printf(" - Establish Pairing Key Start  : failed");
		/* - Transmitting Establish Key pairing[start] Command failed */
		if (ret > 0)
		{
			stick_powerOnReset(pSTICK);
		}
		return ret;
	}
	stick_debug(" ## Establish Pairing Key Start (send) ");

/* - Verify public key certificate using CA public key value */
#ifndef STICK_USE_PRODUCTION_CA_KEY_ONLY
	/* - Verify target STICK Certificate using Generic CA pub. key*/
	if (pSTICK->ctx.public_key_certificate.element.ca_key_id == Generic_CA_public_key.Key_ID)
	{
		crypto_res = stick_platform_ed25519_verify(Generic_CA_public_key.Key_value,						  // Production CA public key value
												   pSTICK->ctx.public_key_certificate.raw_data,			  // Message M TBS from table
												   42,													  // Size of the Message
												   pSTICK->ctx.public_key_certificate.element.signature_r // Buffer with the signature
		);
	}
	else
	{
#endif
		/* - Verify target STICK Certificate using Production CA pub. key*/
		crypto_res = stick_platform_ed25519_verify(Production_CA_public_key.Key_value,					  // Production CA public key value
												   pSTICK->ctx.public_key_certificate.raw_data,			  // Message M TBS from table
												   42,													  // Size of the Message
												   pSTICK->ctx.public_key_certificate.element.signature_r // Buffer with the signature
		);
#ifndef STICK_USE_PRODUCTION_CA_KEY_ONLY
	}
#endif
	if (crypto_res != 0)
	{
		stick_printf("Verify cartridge certificate Failed");
	}
	else
	{
		stick_debug(" ## stick_crypto_ed25519_verify (Verify cartridge certificate) : Verified");
	}
	if (crypto_res != 0)
	{
		stick_powerOnReset(pSTICK);
		/* - Crypto Library returns an error */
		return STICK_AUTHENTICATION_ERROR;
	}
	pSTICK->ctx.state = STICK_PKC_RECEIVED;
	stick_debug(" ## Establish Pairing Key Start (receive) : ");
	stick_platform_Delay_ms(STICK_EXEC_TIME_ESTABLISH_PAIRING_KEY_START);
	ret = stick_Receive(pSTICK, STICK_POLLING_RETRY_COUNT_MAX);
	if (ret != STICK_OK)
	{
		stick_printf(" Error (communication) ");
		/* - Establish Key pairing[start] Command returns an error */
		return ret;
	}
	if (pSTICK->rsp.length != 98)
	{
		stick_debug(" Error (received frame length) ");
		/* - Establish Key pairing[start] response data length is not 98-bytes  */
		stick_powerOnReset(pSTICK);
		if (pSTICK->rsp.length == 0)
		{
			return (stick_ReturnCode_t)pSTICK->rsp.header;
		}
		else
		{
			return STICK_AUTHENTICATION_ERROR;
		}
	}

	/* - Update Volatile attributes (UM 6.3 - 5)*/
	memcpy(pSTICK->ctx.message_digest.host_ecdh_public_key,
		   pSTICK->cmd.payload.establish_pairing_key_start.host_ecdh_public_key,
		   32);

	/*   o cartridge slot number (UM 6.3 - 5.1) */
	memcpy(pSTICK->ctx.message_digest.cartridge_slot_number,
		   pSTICK->rsp.payload.establish_pairing_key_start.cartridge_slot_number,
		   2);

	/*   o cartridge ECDHE public key (UM 6.3 - 5.2) */
	memcpy(pSTICK->ctx.message_digest.cartridge_ecdh_public_key,
		   pSTICK->rsp.payload.establish_pairing_key_start.cartridge_ecdh_public_key,
		   32);

	/*   o signature value (UM 6.3 - 5.3) */
	memcpy(pSTICK->ctx.message_signature.r,
		   pSTICK->rsp.payload.establish_pairing_key_start.signature.r,
		   64);

	pSTICK->ctx.state = STICK_PAIRING_STARTED;
	stick_debug(" \t=> STICK Key slot : 0x%02X%02X ", pSTICK->ctx.message_digest.cartridge_slot_number[0],
				pSTICK->ctx.message_digest.cartridge_slot_number[1]);
	/*- Call processEstablishKeyFinalize (UM 6.3 - 6)*/
	ret = _stick_Finalize_Pairing(pSTICK);

	/* return (UM 6.3 - 7) */
	return ret;
}

static stick_ReturnCode_t _stick_Resume_Pairing(stick_Handler_t *pSTICK)
{
	uint32_t *pRandom;
	uint32_t crypto_res = 1;
	stick_ReturnCode_t ret = STICK_INVALID_PARAMETER;

	/* - Check parameters */
	if (pSTICK == NULL)
	{
		return ret;
	}
	/* - Establish Key pairing[start] */
	pSTICK->cmd.header = STICK_CMD_ESTABLISH_PAIRING_KEY_RESUME;
	/* - Copy static Host_ECDH_key_pair.public_key  into STICK TX frame buffer (UM 6.3 - 1.1) */
	memcpy(pSTICK->cmd.payload.establish_pairing_key_start.host_ecdh_public_key, Host_ECDH_key_pair.public_key, 32);
	/* - Use Random challenge from context */
	memcpy(pSTICK->cmd.payload.establish_pairing_key_start.host_challenge, pSTICK->ctx.message_digest.Host_challenge, 16);
	/* - Use Random Magic_value */
	pRandom = (uint32_t *)pSTICK->cmd.payload.establish_pairing_key_start.magic_value;
	*(pRandom++) = stick_platform_Random();
	*(pRandom++) = stick_platform_Random();
	*(pRandom++) = stick_platform_Random();
	*(pRandom++) = stick_platform_Random();
	/* - Set transfer length */
	pSTICK->cmd.length = 64;
	/* - Send the Establish Pairing Key Resume command to the cartridge (UM 6.3 - 2)*/
	ret = stick_Transmit(pSTICK, STICK_POLLING_RETRY_COUNT_DEFAULT);
	if (ret != STICK_OK)
	{
		stick_printf(" - Establish Pairing Key Start  : failed");
		/* - Transmitting Establish Key pairing[start] Command failed (UM 6.3 - 3) */
		if (ret > 0)
		{
			stick_powerOnReset(pSTICK);
		}
		return ret;
	}
	stick_debug(" ## Establish Pairing Key Start (send) ");
#ifndef STICK_USE_PRODUCTION_CA_KEY_ONLY
	/* - Verify target STICK Certificate using Generic CA pub. key*/
	if (pSTICK->ctx.public_key_certificate.element.ca_key_id == Generic_CA_public_key.Key_ID)
	{
		crypto_res = stick_platform_ed25519_verify(Generic_CA_public_key.Key_value,						  // Production CA public key value
												   pSTICK->ctx.public_key_certificate.raw_data,			  // Message M TBS from table
												   42,													  // Size of the Message
												   pSTICK->ctx.public_key_certificate.element.signature_r // Buffer with the signature
		);
	}
	else
	{
#endif
		/* - Verify target STICK Certificate using Production CA pub. key*/
		crypto_res = stick_platform_ed25519_verify(Production_CA_public_key.Key_value,					  // Production CA public key value
												   pSTICK->ctx.public_key_certificate.raw_data,			  // Message M TBS from table
												   42,													  // Size of the Message
												   pSTICK->ctx.public_key_certificate.element.signature_r // Buffer with the signature
		);
#ifndef STICK_USE_PRODUCTION_CA_KEY_ONLY
	}
#endif
	if (crypto_res != 0)
	{
		stick_printf("Verify cartridge certificate Failed");
	}
	else
	{
		stick_debug(" ## stick_crypto_ed25519_verify (Verify cartridge certificate) : Verified");
	}
	if (crypto_res != 0)
	{
		stick_powerOnReset(pSTICK);
		/* - Crypto Library returns an error (UM 6.4 - 2.1.2) */
		return STICK_AUTHENTICATION_ERROR;
	}
	stick_debug(" ## Establish Pairing Key Start (receive) : ");
	stick_platform_Delay_ms(STICK_EXEC_TIME_ESTABLISH_PAIRING_KEY_RESUME);
	ret = stick_Receive(pSTICK, STICK_POLLING_RETRY_COUNT_MAX);
	if (ret != STICK_OK)
	{
		stick_printf(" Error (communication) ");
		/* - Establish Key pairing[start] Command returns an error (UM 6.3 - 3) */
		if (ret > 0)
		{
			stick_powerOnReset(pSTICK);
		}
		return ret;
	}
	if (pSTICK->rsp.length != 98)
	{
		stick_debug(" Error (received frame length) ");
		/* - Establish Key pairing[start] response data length is not 98-bytes (UM 6.3 - 4) */
		stick_powerOnReset(pSTICK);
		return STICK_AUTHENTICATION_ERROR;
	}
	/* - Update Volatile attributes (UM 6.3 - 5)*/
	memcpy(pSTICK->ctx.message_digest.host_ecdh_public_key, pSTICK->cmd.payload.establish_pairing_key_start.host_ecdh_public_key, 32);
	/*   o cartridge slot number (UM 6.3 - 5.1) */
	memcpy(pSTICK->ctx.message_digest.cartridge_slot_number, pSTICK->rsp.payload.establish_pairing_key_start.cartridge_slot_number, 2);
	/*   o cartridge ECDHE public key (UM 6.3 - 5.2) */
	memcpy(pSTICK->ctx.message_digest.cartridge_ecdh_public_key, pSTICK->rsp.payload.establish_pairing_key_start.cartridge_ecdh_public_key, 32);
	/*   o signature value (UM 6.3 - 5.3) */
	memcpy(pSTICK->ctx.message_signature.r, pSTICK->rsp.payload.establish_pairing_key_start.signature.r, 64);
	pSTICK->ctx.state = STICK_PAIRING_STARTED;
	stick_debug(" \t=> STICK Key slot : 0x%02X%02X ", pSTICK->ctx.message_digest.cartridge_slot_number[0],
				pSTICK->ctx.message_digest.cartridge_slot_number[1]);
	/*- Call processEstablishKeyFinalize (UM 6.3 - 6)*/
	ret = _stick_Finalize_Pairing(pSTICK);

	/* return (UM 6.3 - 7) */
	return ret;
}

static stick_ReturnCode_t _stick_Finalize_Pairing(stick_Handler_t *pSTICK)
{
	stick_ReturnCode_t ret = STICK_INVALID_PARAMETER;
	uint32_t crypto_res = 1, shared_key_length = 0;
	uint8_t shared_secret[32] = {0};
	uint8_t aes128_cmac_plain_text[17] = {0};
	uint8_t shared_key[16];
	uint32_t null_sec_count[2] = {0};

	/* - Check parameters */
	if (pSTICK == NULL)
	{
		return ret;
	}
	/* - Send Establish Key pairing [Finalize] command (UM 6.4 - 1) */
	stick_debug(" ## ESTABLISH_PAIRING_KEY_FINALIZE (send)");
	pSTICK->cmd.header = STICK_CMD_ESTABLISH_PAIRING_KEY_FINALIZE;
	pSTICK->cmd.length = 0;
	ret = stick_Transmit(pSTICK, STICK_POLLING_RETRY_COUNT_DEFAULT);
	if (ret != STICK_OK)
	{
		stick_printf(": Failed (communication error)");
		return ret;
	}

	/* - Verify Signature (UM 6.4 - 2.2)*/
	crypto_res = stick_platform_ed25519_verify(pSTICK->ctx.public_key_certificate.element.leaf_public_key, // Public key certificate
											   pSTICK->ctx.message_digest.host_ecdh_public_key,			   // Message M
											   82,														   // Digest length
											   pSTICK->ctx.message_signature.r							   // Buffer with the signature (64 byte)
	);
	if (crypto_res != 0)
	{
		stick_printf("Verify cartridge Message signature Failed");
	}
	else
	{
		stick_debug(" ## stick_crypto_ed25519_verify (Message signature) : Verified");
	}
	if (crypto_res != 0)
	{
		/* Signature is invalid (UM 6.4 - 2.2.5) */
		return STICK_AUTHENTICATION_ERROR;
	}
	/* - Compute Shared Secret key (UM 6.4 - 2.3) */
	crypto_res = stick_platform_curve25519_ecdh(pSTICK->ctx.message_digest.cartridge_ecdh_public_key, // (UM 6.4 - 2.3.1)
												(uint8_t *)Host_ECDH_key_pair.private_key,			  // (UM 6.4 - 2.3.2)
												shared_secret);
	stick_debug(" ## Compute shared secret ");
	if (crypto_res != 0)
	{
		stick_printf(": Failed (ecdh error) ");
		return STICK_AUTHENTICATION_ERROR;
	}
	/* - Prepare AES128 CMAC plain text (Label || 0x00 || Context in big-endian) */
	aes128_cmac_plain_text[0] = 0x00;
	aes128_cmac_plain_text[1] = 0x01;
	memcpy(&aes128_cmac_plain_text[2], APP_STICK_A100, 4);
	aes128_cmac_plain_text[6] = KEY_TYPE_SHARED_SECRET;
	aes128_cmac_plain_text[7] = KEY_TYPE_PAIRING;
	aes128_cmac_plain_text[8] = 0x00;
	memcpy(&aes128_cmac_plain_text[9], pSTICK->ctx.message_digest.cartridge_slot_number, 2);
	aes128_cmac_plain_text[11] = KDF_CONTEXT_DUMMY;
	memcpy(&aes128_cmac_plain_text[12], &pSTICK->ctx.message_digest.cartridge_ecdh_public_key[0], 3);
	aes128_cmac_plain_text[15] = 0x00;
	aes128_cmac_plain_text[16] = 0x80;
	crypto_res = stick_platform_aes_cmac_enc(aes128_cmac_plain_text,
											 17,
											 shared_secret, // key derivation key
											 16,			// key derivation key size
											 16,			// expected tag length
											 shared_key,
											 &shared_key_length);
	if (crypto_res != 0)
	{
		stick_printf(": Failed (aes128 key derivation error)");
		/* Error in crypto function call  */
		return STICK_AUTHENTICATION_ERROR;
	}
	stick_debug(" ## ESTABLISH_PAIRING_KEY_FINALIZE (send)");
	stick_platform_Delay_ms(STICK_EXEC_TIME_ESTABLISH_PAIRING_KEY_FINALIZE);
	ret = stick_Receive(pSTICK, STICK_POLLING_RETRY_COUNT_MAX);
	if (ret != STICK_OK)
	{
		stick_printf(": Failed (communication error)");
		/* - Establish Key pairing[Finalize] Command returns an error (UM 6.4 - 1.2) */
		if (ret > 0)
		{
			stick_powerOnReset(pSTICK);
		}
		return ret;
	}

	/* - Store cartridge information in temporary STICK_KEY_TABLE */
	memcpy(nvm_attribute_copy.STICK_KEY_TABLE[nvm_attribute_copy.STICK_KEY_TABLE_INDEX].cartridge_ID, pSTICK->ctx.public_key_certificate.element.unic_identifier, 8);
	memcpy(nvm_attribute_copy.STICK_KEY_TABLE[nvm_attribute_copy.STICK_KEY_TABLE_INDEX].cartridge_slot_number, pSTICK->ctx.message_digest.cartridge_slot_number, 2);
	memcpy(nvm_attribute_copy.STICK_KEY_TABLE[nvm_attribute_copy.STICK_KEY_TABLE_INDEX].key_value, shared_key, 16);
	memcpy(nvm_attribute_copy.STICK_KEY_TABLE[nvm_attribute_copy.STICK_KEY_TABLE_INDEX].Sequence_counter, null_sec_count, 2);
	nvm_attribute_copy.STICK_KEY_TABLE_INDEX++;

	return STICK_OK;
}

static bool is_session_allowed = false;

static stick_ReturnCode_t _stick_open_session(stick_Handler_t *pSTICK)
{
	uint8_t iKEY;
	stick_ReturnCode_t ret = STICK_INVALID_PARAMETER;
	uint8_t Known_ID = 0;
	uint32_t *pRandom;
	uint64_t stick_id;
	uint64_t table_id;

	/* - Check stick handler initialization */
	if (pSTICK == NULL)
	{
		return ret;
	}

	if (!is_session_allowed)
	{
		stick_printf("%s is not allowed in kernel implementation due to no storage for coupons\n", __func__);
		return STICK_COMMAND_NOT_AUTHORIZED;
	}

	/* - Copy the Flash page containing STICK_KEY_TABLE and index in RAM */
	memcpy(&nvm_attribute_copy.STICK_KEY_TABLE[0], &NVM_ATTRIBUTES.STICK_KEY_TABLE[0], sizeof(NVM_ATTRIBUTES));

	/* - Get STICK Unic ID */
	pSTICK->cmd.header = STICK_CMD_GET_DATA;
	pSTICK->cmd.payload.get_data.tag = 0x01;
	pSTICK->cmd.length = 1;

	ret = stick_Transfer(pSTICK, STICK_EXEC_TIME_GET_DATA);
	stick_id = (uint64_t)pSTICK->rsp.payload.get_data.data[0];

	/* - Look for target STICK ID in the Host STICK_KEY_TABLE */
	for (iKEY = 0; iKEY < nvm_attribute_copy.STICK_KEY_TABLE_INDEX; iKEY++)
	{

		table_id = (uint64_t)nvm_attribute_copy.STICK_KEY_TABLE[iKEY].cartridge_ID[0];
		if (stick_id == table_id)
		{
			Known_ID = 1;
			break;
		}
	}
	pSTICK->ctx.pairing_key_slot = iKEY;
	if (Known_ID == 0)
	{
		/* - Get STICK Public Key certificate */
		pSTICK->cmd.header = STICK_CMD_GET_PUBLIC_KEY_CERTIFICATE;
		pSTICK->cmd.length = 0;
		ret = stick_Transfer(pSTICK, STICK_EXEC_TIME_GET_PUBLIC_KEY_CERTIFICATE);
		if (ret != STICK_OK) // (UM 5.3 - 2)
		{
			return ret;
		}
		if ((pSTICK->rsp.length != 106) ||
			(pSTICK->rsp.payload.get_public_key_certificate.element.format != 0x03) ||
			((pSTICK->rsp.payload.get_public_key_certificate.element.ca_key_id != Production_CA_public_key.Key_ID)
#ifndef STICK_USE_PRODUCTION_CA_KEY_ONLY
			 && (pSTICK->rsp.payload.get_public_key_certificate.element.ca_key_id != Generic_CA_public_key.Key_ID)
#endif
				 ))
		{
			stick_powerOnReset(pSTICK);
			return STICK_AUTHENTICATION_ERROR;
		}

		if (pSTICK->ctx.state != 0)
		{
			if (memcmp(pSTICK->rsp.payload.get_public_key_certificate.raw_data, pSTICK->ctx.public_key_certificate.raw_data, 106) != 0)
			{
				stick_powerOnReset(pSTICK);
				/* - Store certificate in STICK handler context (UM 5.3 - 6.1) */
				memcpy(pSTICK->ctx.public_key_certificate.raw_data, pSTICK->rsp.payload.get_public_key_certificate.raw_data, 106);
			}
		}

		/* Check Pairing Context and Start or Resume pairing (anti tearing)*/
		switch (pSTICK->ctx.state)
		{
		case 0:
			ret = _stick_Start_Pairing(pSTICK);
			break;
		case 1:
			ret = _stick_Resume_Pairing(pSTICK);
			break;
		case 2:
			ret = _stick_Finalize_Pairing(pSTICK);
			break;
		default:
			return STICK_HANDLER_NOT_INITIALISED;
		}
		if (ret != STICK_OK)
		{
			/* Error in pairing process */
			return ret;
		}
		else
		{
			/* - Store new cartridge information in NVM */
			stick_platform_nvm_write((uint8_t *)&NVM_ATTRIBUTES.STICK_KEY_TABLE[0],
									 (uint8_t *)&nvm_attribute_copy.STICK_KEY_TABLE[0],
									 sizeof(NVM_ATTRIBUTES));
		}
	}

	stick_debug(" ## Local Slot Num : %d", pSTICK->ctx.pairing_key_slot);

	/* - Generate and store a random session ID (UM 5.3 - 6.5.2) */
	pRandom = (uint32_t *)pSTICK->cmd.payload.select_pairing_key.session_identifier;
	*(pRandom++) = stick_platform_Random();
	*(pRandom++) = stick_platform_Random();
	*(pRandom++) = stick_platform_Random();
	*(pRandom++) = stick_platform_Random();
	memcpy(pSTICK->ctx.session_ID, pSTICK->cmd.payload.select_pairing_key.session_identifier, 16);
	/* - Send Select Pairing key slot command  (UM 5.3 - 6.5.2) */
	pSTICK->cmd.header = STICK_CMD_SELECT_PAIRING_KEY;
	memcpy(pSTICK->cmd.payload.select_pairing_key.slot_number, &NVM_ATTRIBUTES.STICK_KEY_TABLE[pSTICK->ctx.pairing_key_slot].cartridge_slot_number, 2);
	pSTICK->cmd.length = 18;
	ret = stick_Transfer(pSTICK, STICK_EXEC_TIME_SELECT_PAIRING_KEY);
	if (ret != STICK_OK) // (UM 5.3 - 6.5.4, 6.5.5)
	{
		stick_powerOnReset(pSTICK);
		return ret;
	}
	/* verify response payload length*/
	if ((pSTICK->rsp.length != 2))
	{
		stick_powerOnReset(pSTICK);
		return STICK_AUTHENTICATION_ERROR;
	}
	/* - Store cartridge sequence counter (stored in little-endian) */
	pSTICK->ctx.sequence_counter[0] = pSTICK->rsp.payload.select_pairing_key.sequence_counter[1];
	pSTICK->ctx.sequence_counter[1] = pSTICK->rsp.payload.select_pairing_key.sequence_counter[0];

	return STICK_OK;
}

static stick_ReturnCode_t _stick_open_sigma_key_session(stick_Handler_t *pSTICK)
{
	stick_ReturnCode_t ret = STICK_INVALID_PARAMETER;
	uint32_t *pRandom;

	/* - Check stick handler initialization */
	if (pSTICK == NULL)
	{
		return ret;
	}

	/* Select Sigma key Slot */
	pSTICK->ctx.pairing_key_slot = 0x00;

	stick_debug(" ## Local Slot Num : Sigma Key ");

	/* - Generate and store a random session ID */
	pRandom = (uint32_t *)pSTICK->cmd.payload.select_pairing_key.session_identifier;
	*(pRandom++) = stick_platform_Random();
	*(pRandom++) = stick_platform_Random();
	*(pRandom++) = stick_platform_Random();
	*(pRandom++) = stick_platform_Random();
	memcpy(pSTICK->ctx.session_ID, pSTICK->cmd.payload.select_pairing_key.session_identifier, 16);

	/* - Send Select Pairing key slot command  */
	pSTICK->cmd.header = STICK_CMD_SELECT_PAIRING_KEY;
	pSTICK->cmd.payload.select_pairing_key.slot_number[0] = 0x00;
	pSTICK->cmd.payload.select_pairing_key.slot_number[1] = 0x00;
	pSTICK->cmd.length = 18;

	ret = stick_Transfer(pSTICK, STICK_EXEC_TIME_SELECT_PAIRING_KEY);
	if (ret != STICK_OK)
	{
		stick_powerOnReset(pSTICK);
		return ret;
	}

	/* verify response payload length*/
	if ((pSTICK->rsp.length != 2))
	{
		stick_powerOnReset(pSTICK);
		return STICK_AUTHENTICATION_ERROR;
	}

	/* - Store cartridge sequence counter (stored in little-endian) */
	pSTICK->ctx.sequence_counter[0] = pSTICK->rsp.payload.select_pairing_key.sequence_counter[1];
	pSTICK->ctx.sequence_counter[1] = pSTICK->rsp.payload.select_pairing_key.sequence_counter[0];

	return STICK_OK;
}

/* Public Functions Definition ------------------------------------------------------*/

/*########################################################*/
/*                  STICK- Generic APIs                   */
/*########################################################*/

// uint32_t stick_get_lib_version(void) __attribute__((visibility("default")));
EXPORT_SYMBOL_GPL(stick_get_lib_version);
uint32_t stick_get_lib_version(void)
{
	return (int32_t)STICK_LIB_VERSION;
}

//uint32_t stick_get_handler_size(void) __attribute__((visibility("default")));
EXPORT_SYMBOL_GPL(stick_get_handler_size);
uint32_t stick_get_handler_size(void)
{
	return sizeof(stick_Handler_t);
}

#define __tostr(arg) #arg
#define _stringize(arg) __tostr(arg)

//stick_ReturnCode_t stick_init(stick_Handler_t *pSTICK) __attribute__((visibility("default")));
EXPORT_SYMBOL_GPL(stick_init);
stick_ReturnCode_t stick_init(stick_Handler_t *pSTICK)
{
	// Show version number in log.
	stick_printf("libstick '" _stringize(RELEASE_NAME) "' (" _stringize(RELEASE_TYPE) ") version: %08X initializing", STICK_LIB_VERSION);

	// default to ST1Wire
	if ((pSTICK != NULL) && (pSTICK->io.BusSend == NULL) && (pSTICK->io.BusRecv == NULL) && (pSTICK->io.BusWake == NULL) && (pSTICK->io.BusRecovery == NULL))
	{
		pSTICK->io.BusSend = st1wire_SendFrame;
		pSTICK->io.BusRecv = st1wire_ReceiveFrame;
		pSTICK->io.BusWake = st1wire_wake;
		pSTICK->io.BusRecovery = st1wire_recovery;
		if (st1wire_init()< 0)
		{
			return STICK_INVALID_PARAMETER;
		};
		// st1wire_wake(0);
	}

	/* - Check stick handler initialization */
	if ((pSTICK != NULL) && (pSTICK->io.BusRecv != NULL) && (pSTICK->io.BusSend != NULL))
	{
		if (0 < stick_platform_init())
		{
			return STICK_INVALID_PARAMETER;
		};
		/* Recover the bus to ensure recovery after tearing*/
		if (pSTICK->io.BusRecovery != NULL)
		{
			pSTICK->io.BusRecovery(pSTICK->io.addr, pSTICK->io.speed);
		}

	  	/* - Wait for device to start */
	  	stick_platform_Delay_ms(300);

#ifndef CHECK_REPEAT_SUPPORT
		pSTICK->io.repeat_support = 1;
#else // CHECK_REPEAT_SUPPORT
		/* exchange one ECHO to bootstrap REPEAT */
		do {
			stick_ReturnCode_t ret = STICK_INVALID_PARAMETER;
		    uint8_t echo1_cmd[] = { 0x00, 0x00, 0x0F, 0x47};

			ret = pSTICK->io.BusSend(pSTICK->io.addr,pSTICK->io.speed,echo1_cmd,sizeof(echo1_cmd));
			if (ret != ST1WIRE_OK) {
				break;
			}

			usleep(6000);

			ret = pSTICK->io.BusRecv(pSTICK->io.addr,
									 pSTICK->io.speed,
									 &pSTICK->rsp.header,
									 &pSTICK->rsp.length);
			if ((ret != ST1WIRE_OK) || (pSTICK->rsp.header != 0x00)) {
				break;
			}

			/* check if REPEAT is supported */
			ret = stick_core_repeat (pSTICK,0);
			if ((ret == STICK_OK) && (pSTICK->rsp.header == STICK_OK))
			{
				pSTICK->io.repeat_support = 1;
			} else {
				pSTICK->io.repeat_support = 0;
			}
		} while (0);
#endif // CHECK_REPEAT_SUPPORT
	}
	else
	{
		return STICK_INVALID_PARAMETER;
	}
	return STICK_OK;
}

//stick_ReturnCode_t stick_open_session(stick_Handler_t * pSTICK) __attribute__((visibility("default")));
EXPORT_SYMBOL_GPL(stick_open_session);
stick_ReturnCode_t stick_open_session(stick_Handler_t *pSTICK)
{
	uint8_t retry_count = 0;
	stick_ReturnCode_t ret = STICK_INVALID_PARAMETER;

	/* - Check stick handler initialization */
	if (pSTICK == NULL)
	{
		return STICK_HANDLER_NOT_INITIALISED;
	}

	/* - Perform and retry  open session process until STICK_OK or MAX retry exceeded*/
	ret = _stick_open_session(pSTICK);
	while ((retry_count < STICK_MAX_API_RETRY) && (ret != STICK_OK) && (ret != STICK_PROT_OK))
	{
		/* - Recover STICK device */
		if (pSTICK->io.BusRecovery != NULL)
		{
			pSTICK->io.BusRecovery(pSTICK->io.addr, pSTICK->io.speed);
		}
		/* - Retry open_session */
		ret = _stick_open_session(pSTICK);
		retry_count++;
	}

	return ret;
}

// stick_ReturnCode_t stick_reset(stick_Handler_t * pSTICK) __attribute__((visibility("default")));
EXPORT_SYMBOL_GPL(stick_reset);
stick_ReturnCode_t stick_reset(stick_Handler_t *pSTICK)
{
	stick_ReturnCode_t ret = STICK_INVALID_PARAMETER;

	/* - Check stick handler initialization */
	if (pSTICK == NULL)
	{
		return STICK_HANDLER_NOT_INITIALISED;
	}

	/*- Prepare reset command*/
	pSTICK->cmd.header = STICK_CMD_RESET;
	pSTICK->cmd.length = 0;

	/*- Transfer command/response */
	ret = stick_Transfer(pSTICK, STICK_EXEC_TIME_RESET);

	/*- Check if Transfer successful and format response data */
	if (ret != STICK_OK)
	{
		/* - Return communication error code */
		return ret;
	}

	/* - Return STICK Status code */
	return (stick_ReturnCode_t)pSTICK->rsp.header;
}

// stick_ReturnCode_t stick_decrement_counter(stick_Handler_t * pSTICK,
// 										   uint8_t isprotected,
// 										   uint8_t zone_index,
// 										   stick_decrement_option_t option,
// 										   uint32_t amount,
// 										   uint16_t offset,
// 										   uint8_t *data,
// 										   uint8_t  data_length,
// 										   uint32_t * new_counter_value) __attribute__((visibility("default")));
EXPORT_SYMBOL_GPL(stick_decrement_counter);
stick_ReturnCode_t stick_decrement_counter(stick_Handler_t *pSTICK,
										   uint8_t isprotected,
										   uint8_t zone_index,
										   stick_decrement_option_t option,
										   uint32_t amount,
										   uint16_t offset,
										   uint8_t *data,
										   uint8_t data_length,
										   uint32_t *new_counter_value)
{
	stick_ReturnCode_t ret = STICK_INVALID_PARAMETER;
	uint8_t current_counter_val[3], failed_counter_val[3];
	uint32_t *pNewCntVal, *pCurCntVal, *pFailCntVal;
	uint16_t rsp_len;
	stick_read_option_t read_option;
	uint8_t retry_count = 0;

	/* - Check stick handler initialization */
	if (pSTICK == NULL)
	{
		return STICK_HANDLER_NOT_INITIALISED;
	}

	/* - Read initial counter value */
	read_option.change_ac_indicator = 0;
	read_option.filler = 0;
	read_option.new_read_ac = 0;
	read_option.new_read_ac_change_right = 0;
	ret = stick_read_zone(pSTICK,
						  isprotected,
						  zone_index,
						  read_option,
						  0,
						  current_counter_val,
						  0,
						  &rsp_len);
	if ((ret != STICK_OK) && (ret != STICK_PROT_OK))
	{
		return ret;
	}

	_change_buffer_endianess(current_counter_val, 3);
	pCurCntVal = (uint32_t *)&current_counter_val[0];

	/*- Prepare decrement command based on function parameters */
	if (isprotected == 1)
	{
		pSTICK->cmd.header = STICK_CMD_PROT_DECREMENT;
	}
	else
	{
		pSTICK->cmd.header = STICK_CMD_DECREMENT;
	}

	pSTICK->cmd.payload.decrement.filler = 0x00;
	pSTICK->cmd.payload.decrement.change_ac_indicator = option.change_ac_indicator;
	pSTICK->cmd.payload.decrement.new_update_ac_change_right = option.new_update_ac_change_right;
	pSTICK->cmd.payload.decrement.new_update_ac = option.new_update_ac;
	pSTICK->cmd.payload.decrement.zone_index = zone_index;
	pSTICK->cmd.payload.decrement.offset[0] = ((offset & 0xFF00) >> 8);
	pSTICK->cmd.payload.decrement.offset[1] = (offset & 0xFF);
	pSTICK->cmd.payload.decrement.amount[0] = ((amount & 0xFF0000) >> 16);
	pSTICK->cmd.payload.decrement.amount[1] = ((amount & 0xFF00) >> 8);
	pSTICK->cmd.payload.decrement.amount[2] = (amount & 0xFF);
	if (data_length != 0)
	{
		memcpy(pSTICK->cmd.payload.decrement.data, data, data_length);
	}
	pSTICK->cmd.length = 7 + data_length;

	/*- Transfer command/response */
	ret = stick_Transfer(pSTICK, isprotected ? STICK_EXEC_TIME_PROT_DECREMENT : STICK_EXEC_TIME_DECREMENT);

	/*- Check if Transfer successful and format response data */
	if ((ret != STICK_OK) && (ret != STICK_PROT_OK))
	{
		while ((retry_count <= STICK_MAX_API_RETRY) && (ret != STICK_OK) && (ret != STICK_PROT_OK))
		{
			/* - Read counter value */
			ret = stick_read_zone(pSTICK,
								  isprotected,
								  zone_index,
								  read_option,
								  0,
								  failed_counter_val,
								  0,
								  &rsp_len);
			if ((ret != STICK_OK) && (ret != STICK_PROT_OK))
			{
				return ret;
			}

			_change_buffer_endianess(failed_counter_val, 3);
			pFailCntVal = (uint32_t *)&failed_counter_val[0];

			if (*pFailCntVal == ((*pCurCntVal) + amount))
			{
				*new_counter_value = *pFailCntVal & 0xFFFFFF;
				return STICK_OK;
			}
			else
			{

				retry_count++;
				ret = STICK_INVALID_PARAMETER;
				/*- Prepare decrement command based on function parameters */
				if (isprotected == 1)
				{
					pSTICK->cmd.header = STICK_CMD_PROT_DECREMENT;
				}
				else
				{
					pSTICK->cmd.header = STICK_CMD_DECREMENT;
				}

				pSTICK->cmd.payload.decrement.filler = 0x00;
				pSTICK->cmd.payload.decrement.change_ac_indicator = option.change_ac_indicator;
				pSTICK->cmd.payload.decrement.new_update_ac_change_right = option.new_update_ac_change_right;
				pSTICK->cmd.payload.decrement.new_update_ac = option.new_update_ac;
				pSTICK->cmd.payload.decrement.zone_index = zone_index;
				pSTICK->cmd.payload.decrement.offset[0] = ((offset & 0xFF00) >> 8);
				pSTICK->cmd.payload.decrement.offset[1] = (offset & 0xFF);
				pSTICK->cmd.payload.decrement.amount[0] = ((amount & 0xFF0000) >> 16);
				pSTICK->cmd.payload.decrement.amount[1] = ((amount & 0xFF00) >> 8);
				pSTICK->cmd.payload.decrement.amount[2] = (amount & 0xFF);
				if (data_length != 0)
				{
					memcpy(pSTICK->cmd.payload.decrement.data, data, data_length);
				}
				pSTICK->cmd.length = 7 + data_length;

				/*- Transfer command/response */
				ret = stick_Transfer(pSTICK, STICK_EXEC_TIME_DECREMENT);
			}
		}
	}

	/*- Format response data */
	if (((pSTICK->rsp.header == 0x00) || (pSTICK->rsp.header == 0x80)) && (new_counter_value != NULL))
	{
		_change_buffer_endianess(pSTICK->rsp.payload.decrement.one_way_counter, 3);
		pNewCntVal = (uint32_t *)(pSTICK->rsp.payload.decrement.one_way_counter);
		*new_counter_value = *pNewCntVal & 0xFFFFFF;

		return STICK_OK;
	}

	/* - Return STICK Status code */
	return (stick_ReturnCode_t)pSTICK->rsp.header;
}

// stick_ReturnCode_t stick_read_zone(stick_Handler_t * pSTICK,
// 						           uint8_t isprotected,
// 								   uint32_t zone_index,
// 								   stick_read_option_t  option,
// 								   uint16_t read_offset,
// 								   uint8_t *pReadBuffer,
// 								   uint16_t read_length,
// 								   uint16_t *rsp_length) __attribute__((visibility("default")));
EXPORT_SYMBOL_GPL(stick_read_zone);
stick_ReturnCode_t stick_read_zone(stick_Handler_t *pSTICK,
								   uint8_t isprotected,
								   uint32_t zone_index,
								   stick_read_option_t option,
								   uint16_t read_offset,
								   uint8_t *pReadBuffer,
								   uint16_t read_length,
								   uint16_t *rsp_length)
{
	stick_ReturnCode_t ret = STICK_INVALID_PARAMETER;
	uint16_t length = read_length;
	uint16_t offset = read_offset;
	uint8_t counter_read = 0;
	uint8_t retry_count = 0;

	/* - Check stick handler initialization */
	if (pSTICK == NULL)
	{
		return STICK_HANDLER_NOT_INITIALISED;
	}

	/* - Clear response length value*/
	*rsp_length = 0;

	/*- Set CMD header to READ or PROTECTED READ */
	if (isprotected == 1)
	{
		pSTICK->cmd.header = STICK_CMD_PROT_READ;
	}
	else
	{
		pSTICK->cmd.header = STICK_CMD_READ;
	}

	/*- Prepare Read command payload */
	pSTICK->cmd.payload.read.filler = 0x00;
	pSTICK->cmd.payload.read.change_ac_indicator = option.change_ac_indicator;
	pSTICK->cmd.payload.read.new_read_ac_change_right = option.new_read_ac_change_right;
	pSTICK->cmd.payload.read.new_read_ac = option.new_read_ac;
	pSTICK->cmd.payload.read.zone_index = zone_index;

	/*- Perform single read operation (COUNTER read)*/
	if (length == 0)
	{
		pSTICK->cmd.payload.read.offset[0] = (uint8_t)(offset >> 8);
		pSTICK->cmd.payload.read.offset[1] = (uint8_t)(offset & 0xFF);
		pSTICK->cmd.payload.read.length[0] = (uint8_t)(0x00);
		pSTICK->cmd.payload.read.length[1] = (uint8_t)(0x00);
		pSTICK->cmd.length = 6;

		/* - Perform read operation until STICK_OK or MAX retry exceeded*/
		retry_count = 0;
		ret = STICK_COMMUNICATION_ERROR;
		while ((retry_count <= STICK_MAX_API_RETRY) && (ret != STICK_OK) && (ret != STICK_PROT_OK))
		{
			/*- Transfer command/response */
			ret = stick_Transfer(pSTICK, STICK_EXEC_TIME_PROT_READ);
			if ((ret != STICK_OK) && (ret != STICK_PROT_OK) && (isprotected == 1))
			{
				/*Re-open session in case of Protected read ERROR*/
				_stick_open_session(pSTICK);
			}
			retry_count++;
		}

		/*- Check if Transfer successful and format response data */
		if ((ret != STICK_OK) && (ret != STICK_PROT_OK))
		{
			/* - (ERROR) Return communication error code */
			return ret;
		}

		/* - (SUCCESS) Copy counter value into applicative read buffer */
		memcpy((pReadBuffer + (offset - read_offset)), pSTICK->rsp.payload.read.data, pSTICK->rsp.length);
		*rsp_length = *rsp_length + pSTICK->rsp.length;
	}
	else
	{
		/*- Perform multiple read operation (zone Counter + zone DATA read)*/
		/* - Read STICK zone by block of 0x40 data length */
		while (length >= 0x40)
		{
			pSTICK->cmd.payload.read.offset[0] = (uint8_t)(offset >> 8);
			pSTICK->cmd.payload.read.offset[1] = (uint8_t)(offset & 0xFF);
			pSTICK->cmd.payload.read.length[0] = (uint8_t)(0x00);
			pSTICK->cmd.payload.read.length[1] = (uint8_t)(0x40);
			pSTICK->cmd.length = 6;

			/* - Perform read operation until STICK_OK or MAX retry exceeded*/
			retry_count = 0;
			ret = stick_Transfer(pSTICK, STICK_EXEC_TIME_PROT_READ);
			while ((retry_count < STICK_MAX_API_RETRY) && (ret != STICK_OK) && (ret != STICK_PROT_OK))
			{
				if ((ret != STICK_OK) && (ret != STICK_PROT_OK) && (isprotected == 1))
				{
					/*Re-open session in case of Protected read ERROR*/
					_stick_open_session(pSTICK);
				}
				/*- Transfer command/response */
				ret = stick_Transfer(pSTICK, STICK_EXEC_TIME_PROT_READ);

				retry_count++;
			}

			/*- Check if Transfer successful and format response data */
			if ((ret != STICK_OK) && (ret != STICK_PROT_OK))
			{
				/* - (ERROR) Return communication error code */
				return ret;
			}

			if (pSTICK->rsp.length > 0x40)
			{
				if (counter_read == 0)
				{
					/* - Read remaining data (length < 0x40) */
					memcpy((pReadBuffer + (offset - read_offset)), pSTICK->rsp.payload.read.data, pSTICK->rsp.length);
					*rsp_length = (*rsp_length) + pSTICK->rsp.length;
					counter_read = 1;
				}
				else
				{
					/* - Read remaining data (length < 0x40) */
					memcpy((pReadBuffer + (offset - read_offset)), &pSTICK->rsp.payload.read.data[3], (pSTICK->rsp.length - 3));
					*rsp_length = (*rsp_length) + (pSTICK->rsp.length - 3);
				}
			}
			else
			{

				/* - Read remaining data (length < 0x40) */
				memcpy((pReadBuffer + (offset - read_offset)), pSTICK->rsp.payload.read.data, pSTICK->rsp.length);
				*rsp_length = (*rsp_length) + pSTICK->rsp.length;
			}
			/* - Decrement Length value */
			length = (length - 0x40);
			offset = (offset + 0x40);
		}
		if (length != 0)
		{
			pSTICK->cmd.payload.read.offset[0] = (uint8_t)(offset >> 8);
			pSTICK->cmd.payload.read.offset[1] = (uint8_t)(offset & 0xFF);
			pSTICK->cmd.payload.read.length[0] = (uint8_t)(length >> 8);
			pSTICK->cmd.payload.read.length[1] = (uint8_t)(length & 0xFF);
			pSTICK->cmd.length = 6;

			/* - Perform read operation until STICK_OK or MAX retry exceeded*/
			retry_count = 0;
			ret = stick_Transfer(pSTICK, STICK_EXEC_TIME_PROT_READ);
			while ((retry_count <= STICK_MAX_API_RETRY) && (ret != STICK_OK) && (ret != STICK_PROT_OK))
			{
				/*- Transfer command/response */
				ret = stick_Transfer(pSTICK, STICK_EXEC_TIME_PROT_READ);
				if ((ret != STICK_OK) && (ret != STICK_PROT_OK) && (isprotected == 1))
				{
					/*Re-open session in case of Protected read ERROR*/
					_stick_open_session(pSTICK);
				}
				retry_count++;
			}

			/*- Check if Transfer successful and format response data */
			if ((ret != STICK_OK) && (ret != STICK_PROT_OK))
			{
				/* - Return communication error code */
				return ret;
			}

			if (pSTICK->rsp.length > length)
			{
				if (counter_read == 0)
				{
					/* - Read remaining data (length < 0x40) */
					memcpy((pReadBuffer + (offset - read_offset)), pSTICK->rsp.payload.read.data, pSTICK->rsp.length);
					*rsp_length = (*rsp_length) + pSTICK->rsp.length;
				}
				else
				{
					/* - Read remaining data (length < 0x40) */
					memcpy((pReadBuffer + (offset - read_offset)), &pSTICK->rsp.payload.read.data[3], (pSTICK->rsp.length - 3));
					*rsp_length = (*rsp_length) + (pSTICK->rsp.length - 3);
				}
			}
			else
			{
				/* - Read remaining data (length < 0x40) */
				memcpy((pReadBuffer + (offset - read_offset)), pSTICK->rsp.payload.read.data, pSTICK->rsp.length);
				*rsp_length = (*rsp_length) + pSTICK->rsp.length;
			}
		}
	}

	/* - Return STICK Status code */
	return (stick_ReturnCode_t)(pSTICK->rsp.header & ~(0x80));
}

// stick_ReturnCode_t stick_update_zone(stick_Handler_t * pSTICK,
// 		                             uint8_t isprotected,
// 									 uint32_t zone_index,
// 									 stick_update_option_t option,
// 									 uint16_t offset,
// 									 uint8_t *data ,
// 									 uint32_t data_length) __attribute__((visibility("default")));
EXPORT_SYMBOL_GPL(stick_update_zone);
stick_ReturnCode_t stick_update_zone(stick_Handler_t *pSTICK,
									 uint8_t isprotected,
									 uint32_t zone_index,
									 stick_update_option_t option,
									 uint16_t offset,
									 uint8_t *data,
									 uint32_t data_length)
{
	stick_ReturnCode_t ret = STICK_INVALID_PARAMETER;
	uint8_t retry_count = 0;

	/* - Check stick handler initialization */
	if (pSTICK == NULL)
	{
		return STICK_HANDLER_NOT_INITIALISED;
	}

	/*- Prepare Read command*/
	if (isprotected == 1)
	{
		pSTICK->cmd.header = STICK_CMD_PROT_UPDATE;
	}
	else
	{
		pSTICK->cmd.header = STICK_CMD_UPDATE;
	}
	pSTICK->cmd.payload.update.atomicity = option.atomicity;
	pSTICK->cmd.payload.update.filler = option.filler;
	pSTICK->cmd.payload.update.change_ac_indicator = option.change_ac_indicator;
	pSTICK->cmd.payload.update.new_update_ac_change_right = option.new_update_ac_change_right;
	pSTICK->cmd.payload.update.new_update_ac = option.new_update_ac;
	pSTICK->cmd.payload.update.zone_index = zone_index;
	pSTICK->cmd.payload.update.offset[0] = (uint8_t)(offset >> 8);
	pSTICK->cmd.payload.update.offset[1] = (uint8_t)(offset & 0xFF);
	memcpy(pSTICK->cmd.payload.update.data, data, data_length);
	pSTICK->cmd.length = 4 + data_length;

	/* - Perform and retry update operation until STICK_OK or MAX retry exceeded*/
	while ((retry_count <= STICK_MAX_API_RETRY) && (ret != STICK_OK) && (ret != STICK_PROT_OK))
	{
		/*- Transfer command/response */
		ret = stick_Transfer(pSTICK, STICK_EXEC_TIME_UPDATE);
		retry_count++;
	}

	/*- Check if Transfer successful and format response data */
	if ((ret != STICK_OK) && (ret != STICK_PROT_OK))
	{
		/* - Return communication error code */
		return ret;
	}
	else
	{
		return STICK_OK;
	}
}

//stick_ReturnCode_t stick_authenticate(stick_Handler_t * pSTICK) __attribute__((visibility("default")));
EXPORT_SYMBOL_GPL(stick_authenticate);
stick_ReturnCode_t stick_authenticate(stick_Handler_t *pSTICK)
{
	stick_ReturnCode_t ret = STICK_INVALID_PARAMETER;
	uint32_t *pRandom;
	uint8_t retry_count = 0;

	/* - Check stick handler initialization */
	if (pSTICK == NULL)
	{
		return STICK_HANDLER_NOT_INITIALISED;
	}

	/*- Prepare authenticate command*/
	pSTICK->cmd.header = STICK_CMD_AUTHENTICATE;
	pRandom = (uint32_t *)pSTICK->cmd.payload.authenticate.host_chalenge;
	*(pRandom++) = stick_platform_Random();
	*(pRandom++) = stick_platform_Random();
	pSTICK->cmd.length = 8;
	/*- Transfer command/response */
	ret = stick_Transfer(pSTICK, STICK_EXEC_TIME_AUTHENTICATE);

	/* - Perform and retry authentication session process until STICK_OK or MAX retry exceeded*/
	while ((retry_count < STICK_MAX_API_RETRY) && (ret != STICK_OK) && (ret != STICK_PROT_OK))
	{
		/*- Prepare authenticate command*/
		pSTICK->cmd.header = STICK_CMD_AUTHENTICATE;
		pRandom = (uint32_t *)pSTICK->cmd.payload.authenticate.host_chalenge;
		*(pRandom++) = stick_platform_Random();
		*(pRandom++) = stick_platform_Random();
		pSTICK->cmd.length = 8;
		/*- Transfer command/response */
		ret = stick_Transfer(pSTICK, STICK_EXEC_TIME_AUTHENTICATE);
		if ((ret != STICK_OK) && (ret != STICK_PROT_OK))
		{
			stick_open_session(pSTICK);
		}
		retry_count++;
	}
	return (ret != STICK_PROT_OK) ? ret : STICK_OK;
}

// stick_ReturnCode_t stick_get_traceability(stick_Handler_t * pSTICK,
// 										  stick_traceability_data_t * pTraceData) __attribute__((visibility("default")));
EXPORT_SYMBOL_GPL(stick_get_traceability);
stick_ReturnCode_t stick_get_traceability(stick_Handler_t *pSTICK,
										  stick_traceability_data_t *pTraceData)
{
	stick_ReturnCode_t ret = STICK_INVALID_PARAMETER;
	uint8_t retry_count = 0;

	/* - Check stick handler initialization */
	if (pSTICK == NULL)
	{
		return STICK_HANDLER_NOT_INITIALISED;
	}

	/* - Prepare STVAP tx Frame */
	pSTICK->cmd.header = STICK_CMD_GET_TRACEABILITY;
	pSTICK->cmd.length = 0;

	/* - Perform and retry authentication session process until STICK_OK or MAX retry exceeded*/
	while ((retry_count <= STICK_MAX_API_RETRY) && (ret != STICK_OK) && (ret != STICK_PROT_OK))
	{
		ret = stick_Transfer(pSTICK, STICK_EXEC_TIME_GET_TRACEABILITY);
		retry_count++;
	}

	/*- Check if Transfer successful and format response data */
	if ((ret != STICK_OK) && (ret != STICK_PROT_OK))
	{
		/* - Return communication error code */
		return ret;
	}

	/*Change reception buffer endianess */
	_change_buffer_endianess(pSTICK->rsp.payload.get_traceability.product_type, sizeof(pSTICK->rsp.payload.get_traceability.product_type));
	_change_buffer_endianess(pSTICK->rsp.payload.get_traceability.cpsn, sizeof(pSTICK->rsp.payload.get_traceability.cpsn));
	_change_buffer_endianess(pSTICK->rsp.payload.get_traceability.remaining_pairing_slots, sizeof(pSTICK->rsp.payload.get_traceability.remaining_pairing_slots));
	_change_buffer_endianess(pSTICK->rsp.payload.get_traceability.remaining_ECDHE, sizeof(pSTICK->rsp.payload.get_traceability.remaining_ECDHE));
	_change_buffer_endianess(pSTICK->rsp.payload.get_traceability.remaining_signatures, sizeof(pSTICK->rsp.payload.get_traceability.remaining_signatures));
	_change_buffer_endianess(pSTICK->rsp.payload.get_traceability.total_pairing_slots, sizeof(pSTICK->rsp.payload.get_traceability.total_pairing_slots));

	memcpy(pTraceData->cpsn, &pSTICK->rsp.payload.get_traceability.cpsn, sizeof(pSTICK->rsp.payload.get_traceability.cpsn));
	memcpy(pTraceData->product_type, &pSTICK->rsp.payload.get_traceability.product_type, sizeof(pSTICK->rsp.payload.get_traceability.product_type));
	memcpy(&pTraceData->remaining_pairing_slots, &pSTICK->rsp.payload.get_traceability.remaining_pairing_slots, sizeof(pSTICK->rsp.payload.get_traceability.remaining_pairing_slots));
	memcpy(&pTraceData->remaining_ECDHE, &pSTICK->rsp.payload.get_traceability.remaining_ECDHE, sizeof(pSTICK->rsp.payload.get_traceability.remaining_ECDHE));
	memcpy(&pTraceData->remaining_signatures, &pSTICK->rsp.payload.get_traceability.remaining_signatures, sizeof(pSTICK->rsp.payload.get_traceability.remaining_signatures));
	memcpy(&pTraceData->total_pairing_slots, &pSTICK->rsp.payload.get_traceability.total_pairing_slots, sizeof(pSTICK->rsp.payload.get_traceability.total_pairing_slots));

	/* - Return STICK Status code */
	return pSTICK->rsp.header;
}

// stick_ReturnCode_t stick_echo(stick_Handler_t * pSTICK,
// 							  uint8_t* message,
// 							  uint8_t* echoed_message,
// 							  uint8_t length) __attribute__((visibility("default")));
EXPORT_SYMBOL_GPL(stick_echo);
stick_ReturnCode_t stick_echo(stick_Handler_t *pSTICK,
							  uint8_t *message,
							  uint8_t *echoed_message,
							  uint8_t length)
{
	stick_ReturnCode_t ret = STICK_INVALID_PARAMETER;
	uint8_t retry_count = 0;

	/* - Check stick handler initialization */
	if (pSTICK == NULL)
	{
		return STICK_HANDLER_NOT_INITIALISED;
	}

	if (length > 252)
	{
		// Too long, we won t be able to send it....
		return STICK_INVALID_PARAMETER;
	}

	/* - Prepare STICK echo Frame */
	pSTICK->cmd.header = STICK_CMD_ECHO;
	memcpy(pSTICK->cmd.payload.echo.message, message, length);
	pSTICK->cmd.length = length;

	/* - Perform echo operation until STICK_OK or MAX retry exceeded*/
	ret = stick_Transfer(pSTICK, STICK_EXEC_TIME_ECHO);
	while ((retry_count < STICK_MAX_API_RETRY) && (ret != STICK_OK))
	{
		/* - Recover STICK device */
		if (pSTICK->io.BusRecovery != NULL)
		{
			pSTICK->io.BusRecovery(pSTICK->io.addr, pSTICK->io.speed);
		}
		/* - Prepare and re-send STICK echo Frame */
		pSTICK->cmd.header = STICK_CMD_ECHO;
		memcpy(pSTICK->cmd.payload.echo.message, message, length);
		pSTICK->cmd.length = length;
		/* - Transfer command/response */
		ret = stick_Transfer(pSTICK, STICK_EXEC_TIME_ECHO);
		/* - Update retry count */
		retry_count++;
	}
	/*- Copy received payload into applicative buffer */
	if (ret == STICK_OK)
	{
		memcpy(echoed_message, pSTICK->rsp.payload.echo.message, length);
	}
	return ret;
}

// stick_ReturnCode_t stick_remove_information(stick_Handler_t * pSTICK) __attribute__((visibility("default")));
EXPORT_SYMBOL_GPL(stick_remove_information);
stick_ReturnCode_t stick_remove_information(stick_Handler_t *pSTICK)
{
	uint8_t known_cartridge = 0;
	uint8_t iKEY;
	uint64_t stick_id;
	uint64_t table_id;
	stick_ReturnCode_t ret = STICK_INVALID_PARAMETER;

	/* - Check stick handler initialization */
	if (pSTICK == NULL)
	{
		return STICK_HANDLER_NOT_INITIALISED;
	}

	/* - Copy the Flash page containing pairing_KEY_TABLE and index in RAM */
	memcpy(&nvm_attribute_copy.STICK_KEY_TABLE[0], &NVM_ATTRIBUTES.STICK_KEY_TABLE[0], sizeof(NVM_ATTRIBUTES));

	/* - Get STICK Unic ID */
	pSTICK->cmd.header = STICK_CMD_GET_DATA;
	pSTICK->cmd.payload.get_data.tag = 0x01;
	pSTICK->cmd.length = 1;

	ret = stick_Transfer(pSTICK, STICK_EXEC_TIME_GET_DATA);
	stick_id = (uint64_t)pSTICK->rsp.payload.get_data.data[0];

	/* - Look for target STICK ID in the Host STICK_KEY_TABLE */
	for (iKEY = 0; iKEY < nvm_attribute_copy.STICK_KEY_TABLE_INDEX; iKEY++)
	{
		table_id = (uint64_t)nvm_attribute_copy.STICK_KEY_TABLE[iKEY].cartridge_ID[0];
		if (stick_id == table_id)
		{
			known_cartridge = 1;
			break;
		}
	}
	if (known_cartridge == 1)
	{
		stick_remove_key(iKEY);
		/* - Store update STICK Key table in Flash */
		stick_platform_nvm_write((uint8_t *)&NVM_ATTRIBUTES.STICK_KEY_TABLE[0], (uint8_t *)&nvm_attribute_copy.STICK_KEY_TABLE[0], sizeof(NVM_ATTRIBUTES));
		ret = STICK_OK;
	}
	return ret;
}

// stick_ReturnCode_t stick_close_session(stick_Handler_t * pSTICK) __attribute__((visibility("default")));
EXPORT_SYMBOL_GPL(stick_close_session);
stick_ReturnCode_t stick_close_session(stick_Handler_t *pSTICK)
{
	/* - Check stick handler initialization */
	if (pSTICK == NULL)
	{
		return STICK_HANDLER_NOT_INITIALISED;
	}

	/* send reset command (no response check needed)*/
	stick_reset(pSTICK);

	/* Erase session context */
	stick_erase_context(pSTICK);

	return STICK_OK;
}

// stick_ReturnCode_t stick_hibernate(stick_Handler_t * pSTICK) __attribute__((visibility("default")));
EXPORT_SYMBOL_GPL(stick_hibernate);
stick_ReturnCode_t stick_hibernate(stick_Handler_t *pSTICK)
{
	stick_ReturnCode_t ret = STICK_INVALID_PARAMETER;
	uint8_t retry_count = 0;

	/* - Check stick handler initialization */
	if (pSTICK == NULL)
	{
		return STICK_HANDLER_NOT_INITIALISED;
	}

	/* - Prepare hibernate command */
	pSTICK->cmd.header = STICK_CMD_HIBERNATE;
	pSTICK->cmd.length = 0;

	/* - Perform and retry hibernate operation until STICK_OK or MAX retry exceeded*/
	while ((retry_count <= STICK_MAX_API_RETRY) && (ret != STICK_OK) && (ret != STICK_PROT_OK))
	{
		/*- Transfer command/response */
		ret = stick_Transfer(pSTICK, STICK_EXEC_TIME_HIBERNATE);
		retry_count++;
	}

	return ret;
}

// stick_ReturnCode_t stick_wakeup(stick_Handler_t * pSTICK) __attribute__((visibility("default")));
EXPORT_SYMBOL_GPL(stick_wakeup);
stick_ReturnCode_t stick_wakeup(stick_Handler_t *pSTICK)
{
	/* - Check stick handler initialization */
	if (pSTICK == NULL)
	{
		return STICK_HANDLER_NOT_INITIALISED;
	}
	pSTICK->io.BusWake(pSTICK->io.addr);
	stick_platform_Delay_ms(10);

	return STICK_OK;
}

/*########################################################*/
/*                  STICK-OSv2.x add-on                   */
/*########################################################*/

/* JSE : retry to be implemented*/

// stick_ReturnCode_t stick_put_data(stick_Handler_t * pSTICK,
// 								  uint8_t tag,
// 								  uint8_t *pvalue,
// 								  uint16_t value_length) __attribute__((visibility("default")));
EXPORT_SYMBOL_GPL(stick_put_data);
stick_ReturnCode_t stick_put_data(stick_Handler_t *pSTICK,
								  uint8_t tag,
								  uint8_t *pvalue,
								  uint16_t value_length)
{
	stick_ReturnCode_t ret = STICK_INVALID_PARAMETER;

	/* - Check stick handler initialization */
	if (pSTICK == NULL)
	{
		return STICK_HANDLER_NOT_INITIALISED;
	}

	/*- Prepare Read command*/
	pSTICK->cmd.header = STICK_CMD_PUT_DATA;
	pSTICK->cmd.payload.put_data.tag = tag;
	memcpy(pSTICK->cmd.payload.put_data.value, pvalue, value_length);
	pSTICK->cmd.length = value_length + 1;

	/*- Transfer command/response */
	ret = stick_Transfer(pSTICK, STICK_EXEC_TIME_PUT_DATA);

	/*- Check if Transfer successful and format response data */
	if ((ret != STICK_OK) && (ret != STICK_PROT_OK))
	{
		/* - Return communication error code */
		return ret;
	}

	/* - Return STICK Status code */
	return (stick_ReturnCode_t)pSTICK->rsp.header;
}

// stick_ReturnCode_t stick_get_data(stick_Handler_t * pSTICK,
// 								  uint8_t tag,
// 								  uint8_t additional_data,
// 								  uint8_t *pDataBuffer,
// 								  uint16_t *DataLength) __attribute__((visibility("default")));
EXPORT_SYMBOL_GPL(stick_get_data);
stick_ReturnCode_t stick_get_data(stick_Handler_t *pSTICK,
								  uint8_t tag,
								  uint8_t additional_data,
								  uint8_t *pDataBuffer,
								  uint16_t *DataLength)
{
	stick_ReturnCode_t ret = STICK_INVALID_PARAMETER;
	uint8_t retry_count = 0;

	*DataLength = 0;

	/* - Check stick handler initialization */
	if (pSTICK == NULL)
	{
		return STICK_HANDLER_NOT_INITIALISED;
	}

	/*- Prepare Read command*/
	pSTICK->cmd.header = STICK_CMD_GET_DATA;
	pSTICK->cmd.payload.get_data.tag = tag;
	if (tag == 5)
	{
		pSTICK->cmd.payload.get_data.additonal_data = additional_data;
		pSTICK->cmd.length = 2;
	}
	else
	{
		pSTICK->cmd.length = 1;
	}

	/* - Perform and retry get_data operation until STICK_OK or MAX retry exceeded */
	while ((retry_count <= STICK_MAX_API_RETRY) && (ret != STICK_OK))
	{
		/*- Transfer command/response */
		ret = stick_Transfer(pSTICK, STICK_EXEC_TIME_GET_DATA);
		retry_count++;
	}

	/*- Check if Transfer successful and format response data */
	if (ret != STICK_OK)
	{
		/* - Return communication error code */
		return ret;
	}

	memcpy(pDataBuffer, pSTICK->rsp.payload.get_data.data, pSTICK->rsp.length);
	*DataLength = pSTICK->rsp.length;

	/* - Return STICK Status code */
	return (stick_ReturnCode_t)pSTICK->rsp.header;
}

// stick_ReturnCode_t stick_eddsa_authenticate(stick_Handler_t * pSTICK,
// 		                                    uint8_t certificate_zone_index) __attribute__((visibility("default")));
/* N17 code for HQ-292265 by tongjiacheng at 20230515 start */
extern uint8_t page0_data[2];
/* N17 code for HQ-292265 by tongjiacheng at 20230515 end */
EXPORT_SYMBOL_GPL(stick_eddsa_authenticate);
stick_ReturnCode_t stick_eddsa_authenticate(stick_Handler_t *pSTICK,
											uint8_t certificate_zone_index)
{
	stick_ReturnCode_t ret = STICK_INVALID_PARAMETER;
	uint16_t rsp_length = 0;
	uint32_t crypto_res = 1;
	uint32_t *pRandom;
	stick_read_option_t read_option;
	uint8_t retry_count = 0;

	/* - Check stick handler initialization */
	if (pSTICK == NULL)
	{
		return STICK_HANDLER_NOT_INITIALISED;
	}

	/* - Read Public Key Certificate from specified STICK zone */
	read_option.change_ac_indicator = 0;
	read_option.filler = 0;
	read_option.new_read_ac = 0;
	read_option.new_read_ac_change_right = 0;
	ret = stick_read_zone(pSTICK,
						  0x00,										   // Not protected
						  certificate_zone_index,					   // Index = certificate zone index
						  read_option,								   // Read Option
						  0x00,										   // Read Offset
						  pSTICK->ctx.public_key_certificate.raw_data, // pointer to Read buffer
						  106,										   // read length
						  &rsp_length);								   // response length

	while ((retry_count < STICK_MAX_API_RETRY) &&
		   ((ret != STICK_OK) ||
			(rsp_length != 106) ||
			(pSTICK->ctx.public_key_certificate.element.format != 04) ||
			((pSTICK->ctx.public_key_certificate.element.ca_key_id != Production_CA_public_key.Key_ID)
#ifndef STICK_USE_PRODUCTION_CA_KEY_ONLY
			 && (pSTICK->ctx.public_key_certificate.element.ca_key_id != Generic_CA_public_key.Key_ID)
#endif
				 )))
	{
		retry_count++;
		ret = stick_read_zone(pSTICK,
							  0x00,
							  certificate_zone_index,
							  read_option,
							  0x00,
							  pSTICK->ctx.public_key_certificate.raw_data,
							  106,
							  &rsp_length);
	}
	if ((ret != STICK_OK) ||
		(rsp_length != 106) ||
		(pSTICK->ctx.public_key_certificate.element.format != 04) ||
		((pSTICK->ctx.public_key_certificate.element.ca_key_id != Production_CA_public_key.Key_ID)
#ifndef STICK_USE_PRODUCTION_CA_KEY_ONLY
		 && (pSTICK->ctx.public_key_certificate.element.ca_key_id != Generic_CA_public_key.Key_ID)
#endif
			 ))
	{
		stick_powerOnReset(pSTICK);
		return STICK_AUTHENTICATION_ERROR;
	}

#ifndef STICK_USE_PRODUCTION_CA_KEY_ONLY
	/* - Verify target STICK Certificate using Generic CA pub. key*/
	if (pSTICK->ctx.public_key_certificate.element.ca_key_id == Generic_CA_public_key.Key_ID)
	{
		crypto_res = stick_platform_ed25519_verify(Generic_CA_public_key.Key_value,						  // Production CA public key value
												   pSTICK->ctx.public_key_certificate.raw_data,			  // Message M TBS from table
												   42,													  // Size of the Message
												   pSTICK->ctx.public_key_certificate.element.signature_r // Buffer with the signature
		);
	}
	else
	{
#endif
		/* - Verify target STICK Certificate using Production CA pub. key*/
		crypto_res = stick_platform_ed25519_verify(Production_CA_public_key.Key_value,					  // Production CA public key value
												   pSTICK->ctx.public_key_certificate.raw_data,			  // Message M TBS from table
												   42,													  // Size of the Message
												   pSTICK->ctx.public_key_certificate.element.signature_r // Buffer with the signature
		);
#ifndef STICK_USE_PRODUCTION_CA_KEY_ONLY
	}
#endif
	if (crypto_res != 0)
	{
		stick_powerOnReset(pSTICK);
		return STICK_AUTHENTICATION_ERROR;
	}

	/*- Prepare Generate signature command*/
	pSTICK->cmd.header = STICK_CMD_GENERATE_SIGNATURE;
	pRandom = (uint32_t *)pSTICK->cmd.payload.generate_signature.challenge;
	*(pRandom++) = stick_platform_Random();
	*(pRandom++) = stick_platform_Random();
	*(pRandom++) = stick_platform_Random();
	*(pRandom++) = stick_platform_Random();
	pSTICK->cmd.payload.generate_signature.reference = 0xFF;
	pSTICK->cmd.length = 17;

	retry_count = 0;
	ret = STICK_COMMUNICATION_ERROR;
	/* - Perform and retry hibernate operation until STICK_OK or MAX retry exceeded*/
	while ((retry_count <= STICK_MAX_API_RETRY) && (ret != STICK_OK) && (ret != STICK_PROT_OK))
	{
		/*- Transfer command/response */
		ret = stick_Transfer(pSTICK, STICK_EXEC_TIME_GENERATE_SIGNATURE_EDDSA);
		retry_count++;
	}
	/*- Check if Transfer successful and format response data */
	if ((pSTICK->rsp.length != 64))
	{
		stick_powerOnReset(pSTICK);
		return STICK_AUTHENTICATION_ERROR;
	}

	/* - Verify Signature  */
	crypto_res = stick_platform_ed25519_verify(pSTICK->ctx.public_key_certificate.element.leaf_public_key, // CA public key value (UM 6.4 - 2.1.1)
											   pSTICK->cmd.payload.generate_signature.challenge,		   // Message M : Challenge
											   16,														   // Size of the Message
											   pSTICK->rsp.payload.generate_signature.signature.element.R  // Buffer with the signature
	);

	if (crypto_res != 0)
	{
		stick_powerOnReset(pSTICK);
		return STICK_AUTHENTICATION_ERROR;
	}
/* N17 code for HQ-292265 by tongjiacheng at 20230515 start */
	ret = stick_read_zone(pSTICK,
						  0x00,										   // Not protected
						  0x0f,					   // Index = certificate zone index
						  read_option,								   // Read Option
						  0x00,										   // Read Offset
						  pSTICK->ctx.public_key_certificate.raw_data, // pointer to Read buffer
						  2,										   // read length
						  &rsp_length);								   // response length
	if (ret != STICK_OK) {
		stick_powerOnReset(pSTICK);
		return STICK_AUTHENTICATION_ERROR;
	}
	memcpy(page0_data, pSTICK->ctx.public_key_certificate.raw_data, sizeof(page0_data));
/* N17 code for HQ-292265 by tongjiacheng at 20230515 end */
	return STICK_OK;
}

// stick_ReturnCode_t stick_eddsa_authenticate_zone(stick_Handler_t * pSTICK,
//                                                  uint8_t certificate_zone_index,
// 												 uint8_t data_zone_index,
// 												 uint16_t data_zone_length,
// 												 uint8_t *pdatabuffer) __attribute__((visibility("default")));
EXPORT_SYMBOL_GPL(stick_eddsa_authenticate_zone);
stick_ReturnCode_t stick_eddsa_authenticate_zone(stick_Handler_t *pSTICK,
												 uint8_t certificate_zone_index,
												 uint8_t data_zone_index,
												 uint16_t data_zone_length,
												 uint8_t *pdatabuffer)
{
	stick_ReturnCode_t ret = STICK_INVALID_PARAMETER;
	uint16_t rsp_length = 0;
	uint32_t crypto_res = 1;
	uint32_t *pRandom;
	stick_read_option_t read_option;
	// uint8_t eddsa_authenticate_message[data_zone_length + 16 + 3];
	uint8_t *eddsa_authenticate_message;
	uint8_t retry_count = 0;

	/* - Check stick handler initialization */
	if (pSTICK == NULL)
	{
		return STICK_HANDLER_NOT_INITIALISED;
	}

	eddsa_authenticate_message = (uint8_t *)kmalloc(data_zone_length + 16 + 3, GFP_KERNEL);
	if(!eddsa_authenticate_message)
		return -ENOMEM;

	/* - Configure read option for read zone function calls*/
	read_option.filler = 0;
	read_option.new_read_ac = 0;
	read_option.new_read_ac_change_right = 0;
	read_option.change_ac_indicator = 0;

	/* - Read Public Key Certificate from specified STICK zone */
	ret = stick_read_zone(pSTICK,
						  0x00,										   // Not protected
						  certificate_zone_index,					   // Zone index
						  read_option,								   // Option
						  0x00,										   // Offset
						  pSTICK->ctx.public_key_certificate.raw_data, // Buffer
						  106,										   // Length
						  &rsp_length);								   // rsp_length

	while ((retry_count < STICK_MAX_API_RETRY) &&
		   ((ret != STICK_OK) ||
			(rsp_length != 106) ||
			(pSTICK->ctx.public_key_certificate.element.format != 04) ||
			((pSTICK->ctx.public_key_certificate.element.ca_key_id != Production_CA_public_key.Key_ID)
#ifndef STICK_USE_PRODUCTION_CA_KEY_ONLY
			 && (pSTICK->ctx.public_key_certificate.element.ca_key_id != Generic_CA_public_key.Key_ID)
#endif
				 )))
	{
		retry_count++;
		ret = stick_read_zone(pSTICK,
							  0x00,
							  certificate_zone_index,
							  read_option,
							  0x00,
							  pSTICK->ctx.public_key_certificate.raw_data,
							  106,
							  &rsp_length);
	}

	if ((ret != STICK_OK) ||
		(rsp_length != 106) ||
		(pSTICK->ctx.public_key_certificate.element.format != 04) ||
		((pSTICK->ctx.public_key_certificate.element.ca_key_id != Production_CA_public_key.Key_ID)
#ifndef STICK_USE_PRODUCTION_CA_KEY_ONLY
		 && (pSTICK->ctx.public_key_certificate.element.ca_key_id != Generic_CA_public_key.Key_ID)
#endif
			 ))
	{
		stick_powerOnReset(pSTICK);
		kfree(eddsa_authenticate_message);
		return STICK_AUTHENTICATION_ERROR;
	}

#ifndef STICK_USE_PRODUCTION_CA_KEY_ONLY
	/* - Verify target STICK Certificate using Generic CA pub. key*/
	if (pSTICK->ctx.public_key_certificate.element.ca_key_id == Generic_CA_public_key.Key_ID)
	{
		crypto_res = stick_platform_ed25519_verify(Generic_CA_public_key.Key_value,						  // Production CA public key value
												   pSTICK->ctx.public_key_certificate.raw_data,			  // Message M TBS from table
												   42,													  // Size of the Message
												   pSTICK->ctx.public_key_certificate.element.signature_r // Buffer with the signature
		);
	}
	else
	{
#endif
		/* - Verify target STICK Certificate using Production CA pub. key*/
		crypto_res = stick_platform_ed25519_verify(Production_CA_public_key.Key_value,					  // Production CA public key value
												   pSTICK->ctx.public_key_certificate.raw_data,			  // Message M TBS from table
												   42,													  // Size of the Message
												   pSTICK->ctx.public_key_certificate.element.signature_r // Buffer with the signature
		);
#ifndef STICK_USE_PRODUCTION_CA_KEY_ONLY
	}
#endif
	if (crypto_res != 0)
	{
		stick_powerOnReset(pSTICK);
		kfree(eddsa_authenticate_message);
		return STICK_AUTHENTICATION_ERROR;
	}

	/* - Read zone to be authenticated  */
	ret = stick_read_zone(pSTICK,
						  0x00,
						  data_zone_index,
						  read_option,
						  0x00,
						  pdatabuffer,
						  data_zone_length,
						  &rsp_length);

	if ((ret != STICK_OK) || (rsp_length == 0))
	{
		stick_powerOnReset(pSTICK);
		kfree(eddsa_authenticate_message);
		return STICK_AUTHENTICATION_ERROR;
	}
	if (rsp_length != data_zone_length)
	{
		data_zone_length = rsp_length;
	}
	memcpy(&eddsa_authenticate_message[16], pdatabuffer, data_zone_length);

	/*- Prepare Generate signature command*/
	pSTICK->cmd.header = STICK_CMD_GENERATE_SIGNATURE;
	pRandom = (uint32_t *)pSTICK->cmd.payload.generate_signature.challenge;
	*(pRandom++) = stick_platform_Random();
	*(pRandom++) = stick_platform_Random();
	*(pRandom++) = stick_platform_Random();
	*(pRandom++) = stick_platform_Random();
	memcpy(eddsa_authenticate_message, pSTICK->cmd.payload.generate_signature.challenge, 16);
	pSTICK->cmd.payload.generate_signature.reference = data_zone_index;
	pSTICK->cmd.length = 17;

	retry_count = 0;
	ret = STICK_COMMUNICATION_ERROR;
	while ((retry_count <= STICK_MAX_API_RETRY) && (ret != STICK_OK) && (ret != STICK_PROT_OK))
	{
		/*- Transfer command/response */
		ret = stick_Transfer(pSTICK, STICK_EXEC_TIME_GENERATE_SIGNATURE_EDDSA_ZONE);
		retry_count++;
	}

	/*- Check if Transfer successful and format response data */
	if (pSTICK->rsp.length != 64)
	{
		stick_powerOnReset(pSTICK);
		kfree(eddsa_authenticate_message);
		return STICK_AUTHENTICATION_ERROR;
	}

	/* - Verify Signature  */
	crypto_res = stick_platform_ed25519_verify(pSTICK->ctx.public_key_certificate.element.leaf_public_key, // CA public key value (UM 6.4 - 2.1.1)
											   eddsa_authenticate_message,								   // Message M : Challenge + Get Traceability Data
											   data_zone_length + 16,									   // Size of the Message
											   pSTICK->rsp.payload.generate_signature.signature.element.R  // Buffer with the signature
	);

	if (crypto_res != 0)
	{
		stick_powerOnReset(pSTICK);
		kfree(eddsa_authenticate_message);
		return STICK_AUTHENTICATION_ERROR;
	}

	kfree(eddsa_authenticate_message);
	return STICK_OK;
}

// stick_ReturnCode_t stick_eddsa_authenticate_traceability(stick_Handler_t * pSTICK,
// 														 uint8_t certificate_zone_index ,
// 														 stick_traceability_data_t * pTraceData) __attribute__((visibility("default")));
EXPORT_SYMBOL_GPL(stick_eddsa_authenticate_traceability);
stick_ReturnCode_t stick_eddsa_authenticate_traceability(stick_Handler_t *pSTICK,
														 uint8_t certificate_zone_index,
														 stick_traceability_data_t *pTraceData)
{
	stick_ReturnCode_t ret = STICK_INVALID_PARAMETER;
	uint16_t rsp_length = 0;
	uint8_t crypto_res = 1;
	uint32_t *pRandom;
	uint8_t message[39];
	stick_read_option_t read_option;
	uint8_t retry_count = 0;

	/* - Check stick handler initialization */
	if (pSTICK == NULL)
	{
		return STICK_HANDLER_NOT_INITIALISED;
	}

	/* - Configure read option for read zone function calls*/
	read_option.filler = 0;
	read_option.new_read_ac = 0;
	read_option.new_read_ac_change_right = 0;
	read_option.change_ac_indicator = 0;

	/* - Read Public Key Certificate from specified STICK zone */
	read_option.filler = 0;
	read_option.new_read_ac = 0;
	read_option.new_read_ac_change_right = 0;
	read_option.change_ac_indicator = 0;
	ret = stick_read_zone(pSTICK,
						  0x00,
						  certificate_zone_index,
						  read_option,
						  0x00,
						  pSTICK->ctx.public_key_certificate.raw_data,
						  106,
						  &rsp_length);

	while ((retry_count < STICK_MAX_API_RETRY) &&
		   ((ret != STICK_OK) ||
			(rsp_length != 106) ||
			(pSTICK->ctx.public_key_certificate.element.format != 04) ||
			((pSTICK->ctx.public_key_certificate.element.ca_key_id != Production_CA_public_key.Key_ID)
#ifndef STICK_USE_PRODUCTION_CA_KEY_ONLY
			 && (pSTICK->ctx.public_key_certificate.element.ca_key_id != Generic_CA_public_key.Key_ID)
#endif
				 )))
	{
		retry_count++;
		ret = stick_read_zone(pSTICK,
							  0x00,
							  certificate_zone_index,
							  read_option,
							  0x00,
							  pSTICK->ctx.public_key_certificate.raw_data,
							  106,
							  &rsp_length);
	}
	if ((ret != STICK_OK) ||
		(rsp_length != 106) ||
		(pSTICK->ctx.public_key_certificate.element.format != 04) ||
		((pSTICK->ctx.public_key_certificate.element.ca_key_id != Production_CA_public_key.Key_ID)
#ifndef STICK_USE_PRODUCTION_CA_KEY_ONLY
		 && (pSTICK->ctx.public_key_certificate.element.ca_key_id != Generic_CA_public_key.Key_ID)
#endif
			 ))
	{
		stick_debug("ERROR : certificate read/integrity error\n\t - rsp_length : %d\n\t - format : %x\n\t - ca_key_id : %x\n",
					rsp_length,
					pSTICK->ctx.public_key_certificate.element.format,
					pSTICK->ctx.public_key_certificate.element.ca_key_id);
		stick_powerOnReset(pSTICK);
		return STICK_AUTHENTICATION_ERROR;
	}

#ifndef STICK_USE_PRODUCTION_CA_KEY_ONLY
	/* - Verify target STICK Certificate using Generic CA pub. key*/
	if (pSTICK->ctx.public_key_certificate.element.ca_key_id == Generic_CA_public_key.Key_ID)
	{
		crypto_res = stick_platform_ed25519_verify(Generic_CA_public_key.Key_value,						  // Production CA public key value
												   pSTICK->ctx.public_key_certificate.raw_data,			  // Message M TBS from table
												   42,													  // Size of the Message
												   pSTICK->ctx.public_key_certificate.element.signature_r // Buffer with the signature
		);
	}
	else
	{
#endif
		/* - Verify target STICK Certificate using Production CA pub. key*/
		crypto_res = stick_platform_ed25519_verify(Production_CA_public_key.Key_value,					  // Production CA public key value
												   pSTICK->ctx.public_key_certificate.raw_data,			  // Message M TBS from table
												   42,													  // Size of the Message
												   pSTICK->ctx.public_key_certificate.element.signature_r // Buffer with the signature
		);
#ifndef STICK_USE_PRODUCTION_CA_KEY_ONLY
	}
#endif
	if (crypto_res != 0)
	{
		stick_powerOnReset(pSTICK);
		return STICK_AUTHENTICATION_ERROR;
	}

	/* - Get traceability data */
	ret = stick_get_data(pSTICK,
						 0x00,
						 0x00,
						 (uint8_t *)pTraceData,
						 &rsp_length);

	memcpy(&message[16], pTraceData, rsp_length);

	/*- Prepare Generate signature command*/
	pSTICK->cmd.header = STICK_CMD_GENERATE_SIGNATURE;
	pRandom = (uint32_t *)pSTICK->cmd.payload.generate_signature.challenge;
	*(pRandom++) = stick_platform_Random();
	*(pRandom++) = stick_platform_Random();
	*(pRandom++) = stick_platform_Random();
	*(pRandom++) = stick_platform_Random();
	memcpy(message, pSTICK->cmd.payload.generate_signature.challenge, 16);
	pSTICK->cmd.payload.generate_signature.reference = 0xFE;
	pSTICK->cmd.length = 17;

	retry_count = 0;
	ret = stick_Transfer(pSTICK, STICK_EXEC_TIME_GENERATE_SIGNATURE_EDDSA_TRACEABILITY);
	while ((retry_count < STICK_MAX_API_RETRY) && (ret != STICK_OK) && (ret != STICK_PROT_OK))
	{
		/*- Transfer command/response */
		ret = stick_Transfer(pSTICK, STICK_EXEC_TIME_GENERATE_SIGNATURE_EDDSA_TRACEABILITY);
		retry_count++;
	}

	/*- Check if Transfer successful and format response data */
	if (pSTICK->rsp.length != 64)
	{
		stick_powerOnReset(pSTICK);
		return STICK_AUTHENTICATION_ERROR;
	}

	/* - Verify Signature  */
	crypto_res = stick_platform_ed25519_verify(pSTICK->ctx.public_key_certificate.element.leaf_public_key, // CA public key value (UM 6.4 - 2.1.1)
											   message,													   // Message M : Challenge + Get Traceability Data
											   39,														   // Size of the Message
	pSTICK->rsp.payload.generate_signature.signature.element.R  // Buffer with the signature
	);

	if (crypto_res != 0)
	{
		stick_powerOnReset(pSTICK);
		return STICK_AUTHENTICATION_ERROR;
	}

	return STICK_OK;
}

// stick_ReturnCode_t stick_regenerate(stick_Handler_t * pSTICK,
// 									uint8_t *pPassword) __attribute__((visibility("default")));
EXPORT_SYMBOL_GPL(stick_regenerate);
stick_ReturnCode_t stick_regenerate(stick_Handler_t *pSTICK,
									uint8_t *pPassword)
{
	stick_ReturnCode_t ret = STICK_INVALID_PARAMETER;

	/* - Check stick handler initialization */
	if (pSTICK == NULL)
	{
		return STICK_HANDLER_NOT_INITIALISED;
	}

	/*- Prepare regenerate command*/
	pSTICK->cmd.header = STICK_CMD_REGENERATE;
	memcpy(pSTICK->cmd.payload.regenerate.password, pPassword, 16);
	pSTICK->cmd.length = 16;

	/*- Transfer command/response */
	ret = stick_Transfer(pSTICK, STICK_EXEC_TIME_REGENERATE);

	/*- Check if Transfer successful and format response data */
	if (ret != STICK_OK)
	{
		/* - Return communication error code */
		return ret;
	}

	/* - Return STICK Status code */
	return (stick_ReturnCode_t)pSTICK->rsp.header;
}

// stick_ReturnCode_t stick_open_sigma_key_session(stick_Handler_t * pSTICK) __attribute__((visibility("default")));
EXPORT_SYMBOL_GPL(stick_open_sigma_key_session);
stick_ReturnCode_t stick_open_sigma_key_session(stick_Handler_t *pSTICK)
{
	stick_ReturnCode_t ret = STICK_INVALID_PARAMETER;
	uint8_t retry_count = 0;

	/* - Check stick handler initialization */
	if (pSTICK == NULL)
	{
		return STICK_HANDLER_NOT_INITIALISED;
	}

	/* - Perform and retry open session process until STICK_OK or MAX retry exceeded*/
	while ((retry_count <= STICK_MAX_API_RETRY) && (ret != STICK_OK) && (ret != STICK_PROT_OK))
	{
		/*- Transfer command/response */
		ret = _stick_open_sigma_key_session(pSTICK);
		retry_count++;
	}

	return ret;
}

// stick_ReturnCode_t stick_kill(stick_Handler_t * pSTICK) __attribute__((visibility("default")));
EXPORT_SYMBOL_GPL(stick_kill);
stick_ReturnCode_t stick_kill(stick_Handler_t *pSTICK)
{
	stick_ReturnCode_t ret = STICK_INVALID_PARAMETER;
	uint8_t retry_count = 0;

	/* - Check stick handler initialization */
	if (pSTICK == NULL)
	{
		return ret;
	}

	/*- Prepare regenerate command*/
	pSTICK->cmd.header = STICK_CMD_KILL;
	pSTICK->cmd.length = 0;

	/* - Perform and retry kill process until STICK_OK or MAX retry exceeded*/
	while ((retry_count <= STICK_MAX_API_RETRY) && (ret != STICK_OK) && (ret != STICK_PROT_OK))
	{
		/*- Transfer command/response */
		ret = stick_Transfer(pSTICK, STICK_EXEC_TIME_KILL);
		retry_count++;
	}

	/*- Check if Transfer successful and format response data */
	if ((ret != STICK_OK) && (ret != STICK_PROT_OK))
	{
		/* - Return communication error code */
		return ret;
	}

	/* - Return STICK Status code */
	return (stick_ReturnCode_t)pSTICK->rsp.header;
}

// stick_ReturnCode_t stick_reset_zone_ac(stick_Handler_t * pSTICK) __attribute__((visibility("default")));
EXPORT_SYMBOL_GPL(stick_reset_zone_ac);
stick_ReturnCode_t stick_reset_zone_ac(stick_Handler_t *pSTICK, uint8_t zone_index, uint8_t option)
{
	stick_ReturnCode_t ret = STICK_INVALID_PARAMETER;
	uint8_t retry_count = 0;

	/* - Check stick handler initialization */
	if (pSTICK == NULL)
	{
		return ret;
	}

	/*- Prepare reset zone AC command*/
	pSTICK->cmd.header = STICK_CMD_RESET_ZONE_AC;
	pSTICK->cmd.payload.reset_zone_ac.index = zone_index;
	pSTICK->cmd.payload.reset_zone_ac.filler = (option & 0b11111100) >> 2;
	pSTICK->cmd.payload.reset_zone_ac.reset_read_ac = (option & 0b00000010) >> 1;
	pSTICK->cmd.payload.reset_zone_ac.reset_update_ac = (option & 0b00000001);
	pSTICK->cmd.length = 2;

	/* - Perform and retry kill process until STICK_OK or MAX retry exceeded*/
	while ((retry_count <= STICK_MAX_API_RETRY) && (ret != STICK_OK) && (ret != STICK_PROT_OK))
	{
		/*- Transfer command/response */
		ret = stick_Transfer(pSTICK, STICK_EXEC_TIME_RESET_ZONE_AC);
		retry_count++;
	}

	/*- Check if Transfer successful and format response data */
	if ((ret != STICK_OK) && (ret != STICK_PROT_OK))
	{
		/* - Return communication error code */
		return ret;
	}

	/* - Return STICK Status code */
	return pSTICK->rsp.header;
}

stick_ReturnCode_t stick_repeat(stick_Handler_t *pSTICK)
{
	stick_ReturnCode_t ret = STICK_INVALID_PARAMETER;

	/* - Perform Repeat */
	ret = stick_core_repeat(pSTICK, 0);

	/* - Return communication error code */
	return ret;
}

//------
// Kernel driver specific logic
void stickapi_exit(void)
{
	stick_printf("Unloading module");
	st1wire_deinit();
}
EXPORT_SYMBOL_GPL(stickapi_exit);

void stickapi_allow_sessions(void)
{
	stick_printf("Allowing sessons. USE ONLY WITH REGEN CHIPS");
	is_session_allowed = true;
}
EXPORT_SYMBOL_GPL(stickapi_allow_sessions);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("STMicroelectronics");
MODULE_DESCRIPTION("STICK API driver");
MODULE_VERSION("0.0.1");
