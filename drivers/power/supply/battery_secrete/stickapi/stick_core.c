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
#include "stick_core.h"
#include "stick_NVM_attributes.h"

#define stick_debug(FMT, ...) pr_info("stickapi_COR " FMT, ## __VA_ARGS__)

/* Private constants -----------------------------------------------------------*/
const uint8_t SIGMA_KEY[16] = {0x72,0x91,0xF0,0xDE,0x50,0xD8,0x75,0x52,0x66,0xD3,0x02,0x25,0xC2,0x00,0x1B,0xB0};


/* Static functions prototypes -----------------------------------------------*/
static stick_ReturnCode_t _stick_unwrap_rsp (stick_Handler_t * pSTICK);
static stick_ReturnCode_t _stick_wrap_cmd (stick_Handler_t * pSTICK);

/* Static Functions Definition ------------------------------------------------*/

static stick_ReturnCode_t _stick_unwrap_rsp (stick_Handler_t * pSTICK)
{
	uint8_t i = 0 ,ret = 0;
	uint8_t session_ID_empty = 1;
	uint8_t nonce[13] = {0};
	uint8_t associated_data[255] = {0};
	uint16_t* pcount;

	/* - Check parameters */
	if (pSTICK == NULL)
	{
		return STICK_HANDLER_NOT_INITIALISED;
	}
	/* - Check memory attributes (UM 6.6 - 1) */
	for (i=0;i<8;i++)
	{
		if (pSTICK->ctx.session_ID[i] != 0)
		{
			session_ID_empty = 0;
			break;
		}
	}
	if(session_ID_empty == 1)
	{
		return STICK_SESSION_ERROR;
	}
	/*- Prepare Nonce */
	nonce[0] = 0b1<<7; // R-MAC
	memcpy(&nonce[1],&pSTICK->ctx.current_R_MAC_IV[0],8);
	/*- Increment Sequence counter */
	pcount = (uint16_t*) &pSTICK->ctx.sequence_counter[0];
	*pcount = *pcount+1;
    nonce[11] = pSTICK->ctx.sequence_counter[1];
    nonce[12] = pSTICK->ctx.sequence_counter[0];
	/*- Prepare Associated Data */
	associated_data[0] = pSTICK->cmd.header;           								// Command Header
	memcpy(&associated_data[1],&pSTICK->cmd.payload.raw[0] ,pSTICK->cmd.length);    // || plaintext command data (optional)  || CMAC (optional)
	associated_data[pSTICK->cmd.length+1] = pSTICK->ctx.wrap.header; 		        // || Rx Header
	memcpy(&associated_data[pSTICK->cmd.length+2],pSTICK->ctx.session_ID,16);       // || Session ID


	if(pSTICK->ctx.pairing_key_slot != 0)
	{
		/*- Call CCM decrypt function */
		ret = stick_platform_aes_ccm_dec( &pSTICK->ctx.wrap.payload[0], // Encrypted payload
										  pSTICK->ctx.wrap.length - 8, // Length of encrypted payload = Rx frame length - R-MAC
										  &pSTICK->ctx.wrap.payload[(pSTICK->ctx.wrap.length-8)], // Tag (R-MAC)
										  8, // Tag length (8)
										  &nvm_attribute_copy.STICK_KEY_TABLE[pSTICK->ctx.pairing_key_slot].key_value[0], // Key
										  16, // Key length = 16
										  &nonce[0], // Nonce
										  13, // Nonce length = 13
										  &associated_data[0], // Associated data
										  pSTICK->cmd.length + 18, // Associated data length
										  &pSTICK->rsp.payload.raw[0], // Decrypted payload
										  pSTICK->ctx.wrap.length-8 // Decrypted payload length
		);
	} else {
		ret = stick_platform_aes_ccm_dec( &pSTICK->ctx.wrap.payload[0], // Encrypted payload
										  pSTICK->ctx.wrap.length - 8, // Length of encrypted payload = Rx frame length - R-MAC
										  &pSTICK->ctx.wrap.payload[(pSTICK->ctx.wrap.length-8)], // Tag (R-MAC)
										  8, // Tag length (8)
										  SIGMA_KEY, //Sigma Key
										  16, // Key length = 16
										  &nonce[0], // Nonce
										  13, // Nonce length = 13
										  &associated_data[0], // Associated data
										  pSTICK->cmd.length + 18, // Associated data length
										  &pSTICK->rsp.payload.raw[0], // Decrypted payload
										  pSTICK->ctx.wrap.length-8 // Decrypted payload length
		);

	}

	memcpy(&pSTICK->ctx.current_R_MAC_IV[0],&pSTICK->ctx.wrap.payload[(pSTICK->ctx.wrap.length-8)],8);
	pSTICK->rsp.header = pSTICK->ctx.wrap.header;
	pSTICK->rsp.length = pSTICK->ctx.wrap.length - 8;
	if (ret != 0)
	{
		return STICK_AUTHENTICATION_ERROR;
	}

	return STICK_OK ;
}

static stick_ReturnCode_t _stick_wrap_cmd (stick_Handler_t * pSTICK)
{
	uint8_t i = 0, ret = 0;
	uint8_t session_ID_empty = 1;
	uint8_t nonce[13] = {0};
	uint8_t associated_data[17] = {0};
	uint16_t* pcount;
	int32_t cmd_length;

	/* - Check parameters */
	if (pSTICK == NULL)
	{
		return STICK_HANDLER_NOT_INITIALISED;
	}
	/* - Check memory attributes (UM 6.6 - 1) */
	for (i=0;i<8;i++)
	{
		if (pSTICK->ctx.session_ID[i] != 0)
		{
			session_ID_empty = 0;
			break;
		}
	}
	if(session_ID_empty == 1)
	{
		return ret = STICK_SESSION_ERROR;
	}
	/*- Increment Sequence counter */
	pcount = (uint16_t*) &pSTICK->ctx.sequence_counter[0];
	*pcount = *pcount+1;
    nonce[11] = pSTICK->ctx.sequence_counter[1];
    nonce[12] = pSTICK->ctx.sequence_counter[0];
	/*- Prepare Associated Data (COMMAND HEADER || SESSION ID) */
	associated_data[0] = pSTICK->cmd.header;
	memcpy(&associated_data[1],pSTICK->ctx.session_ID,16);
	/*- Call CCM encrypt function */
	cmd_length = (int32_t)pSTICK->cmd.length;

	if(pSTICK->ctx.pairing_key_slot != 0)
	{
		ret = stick_platform_aes_ccm_enc( &pSTICK->cmd.payload.raw[0], // Plaintext payload
									  	  pSTICK->cmd.length, // Length of the Plaintext
										  &nvm_attribute_copy.STICK_KEY_TABLE[pSTICK->ctx.pairing_key_slot].key_value[0], // Key
										  16, // Key length = 16
										  nonce, // Nonce
										  13, // Nonce length
										  associated_data, //Associated Data
										  17, //Associated Data length
										  &pSTICK->ctx.wrap.payload[0], //pointer to encrypted Data buffer
										  &cmd_length, //Encrypted Data buffer length
										  &pSTICK->ctx.wrap.payload[pSTICK->cmd.length], // Tag : C-MAC
										  8); // TAG : C-MAC length
	} else {
		ret = stick_platform_aes_ccm_enc( &pSTICK->cmd.payload.raw[0], // Plaintext payload
									  	  pSTICK->cmd.length, // Length of the Plaintext
										  SIGMA_KEY, // Key
										  16, // Key length = 16
										  nonce, // Nonce
										  13, // Nonce length
										  associated_data, //Associated Data
										  17, //Associated Data length
										  &pSTICK->ctx.wrap.payload[0], //pointer to encrypted Data buffer
										  &cmd_length, //Encrypted Data buffer length
										  &pSTICK->ctx.wrap.payload[pSTICK->cmd.length], // Tag : C-MAC
										  8); // TAG : C-MAC length

	}
	pSTICK->cmd.length = (uint8_t)cmd_length;
	if (ret != 0)
	{
		return STICK_SESSION_ERROR;
	}
	/* wrapped_tx_buffer (C-MAC inclusion) */
	pSTICK->ctx.wrap.header = pSTICK->cmd.header;
	pSTICK->ctx.wrap.length = pSTICK->cmd.length + 8;
	/* Include C-MAC at the end of command payload (required in case of  R-MAC verification) */
	memcpy(&pSTICK->cmd.payload.raw[pSTICK->cmd.length],&pSTICK->ctx.wrap.payload[pSTICK->cmd.length],8);
	pSTICK->cmd.length = pSTICK->cmd.length + 8;

	return STICK_OK;
}

/* Public Functions Definition ------------------------------------------------------*/

void stick_remove_key(uint8_t iKEY)
{
	uint8_t i;

	/*Shift table by one index starting from iKEY */
	if (iKEY != nvm_attribute_copy.STICK_KEY_TABLE_INDEX )
	{
		for (i = iKEY ; i < (nvm_attribute_copy.STICK_KEY_TABLE_INDEX-1) ;i++)
		{
			memcpy(&nvm_attribute_copy.STICK_KEY_TABLE[i],&nvm_attribute_copy.STICK_KEY_TABLE[i+1],28);
		}
	}
	/* Clear last table index */
	nvm_attribute_copy.STICK_KEY_TABLE[nvm_attribute_copy.STICK_KEY_TABLE_INDEX].Sequence_counter[0]=0xFF;
	nvm_attribute_copy.STICK_KEY_TABLE[nvm_attribute_copy.STICK_KEY_TABLE_INDEX].Sequence_counter[1]=0xFF;
	for(i=0;i<8;i++)
	{
		nvm_attribute_copy.STICK_KEY_TABLE[nvm_attribute_copy.STICK_KEY_TABLE_INDEX].cartridge_ID[i] = 0xFF;
	}
	nvm_attribute_copy.STICK_KEY_TABLE[nvm_attribute_copy.STICK_KEY_TABLE_INDEX].cartridge_slot_number[0] = 0xFF;
	nvm_attribute_copy.STICK_KEY_TABLE[nvm_attribute_copy.STICK_KEY_TABLE_INDEX].cartridge_slot_number[1] = 0xFF;
	for(i=0;i<16;i++)
	{
		nvm_attribute_copy.STICK_KEY_TABLE[nvm_attribute_copy.STICK_KEY_TABLE_INDEX].key_value[i] = 0xFF;
	}
	/* Decrement STICK index  */
	if(nvm_attribute_copy.STICK_KEY_TABLE_INDEX !=0)
	{
		nvm_attribute_copy.STICK_KEY_TABLE_INDEX = nvm_attribute_copy.STICK_KEY_TABLE_INDEX - 1;
	}
}

void stick_erase_context (stick_Handler_t *pSTICK)
{
	uint8_t i;

	if (pSTICK != NULL)
	{
		pSTICK->ctx.state = 0;

	    for(i=0 ; i<16 ; i++)
	    {
	    	pSTICK->ctx.session_ID[i] = 0;
	    }
	    /* - Erase RMAC_IV  */
	    for(i=0 ; i<8 ; i++)
	    {
	    	pSTICK->ctx.current_R_MAC_IV[i] = 0;
	    }
	    /* - Erase cartridge public key certificate  */
	    for(i=0 ; i<106 ; i++)
	    {
	    	pSTICK->ctx.public_key_certificate.raw_data[i] = 0;
	    }
	    /* - Erase message signature R */
	    for(i=0 ; i<32 ; i++)
	    {
	    	pSTICK->ctx.message_signature.r[i] = 0;
	    }
	    /* - Erase message signature S */
	    for(i=0 ; i<32 ; i++)
	    {
	    	pSTICK->ctx.message_signature.s[i] = 0;
	    }
	    /* - Erase message digest */
	    for(i=0 ; i<16 ; i++)
	    {
	    	pSTICK->ctx.message_digest.Host_challenge[i] = 0;
	    }
	    for(i=0 ; i<32 ; i++)
	    {
	    	pSTICK->ctx.message_digest.cartridge_ecdh_public_key[i] = 0;
	    }
	    for(i=0 ; i<2 ; i++)
	    {
	    	pSTICK->ctx.message_digest.cartridge_slot_number[i] = 0;
	    }

	    for(i=0 ; i<32 ; i++)
	    {
	    	pSTICK->ctx.message_digest.host_ecdh_public_key[i] = 0;
	    }
	}
}

void stick_powerOnReset(stick_Handler_t *pSTICK)
{
	if (pSTICK != NULL)
	{
		/* Erase context with target STICK */
			stick_erase_context(pSTICK);

		    /* Perform a Bus recovery / Reset the device */
			if (pSTICK->io.BusRecovery != NULL)
			{
				pSTICK->io.BusRecovery(pSTICK->io.addr,pSTICK->io.speed);
			}
	}
}

stick_ReturnCode_t stick_core_repeat(stick_Handler_t *pSTICK, uint8_t wrap)
{
	stick_ReturnCode_t ret = STICK_INVALID_PARAMETER;
	uint8_t backup_header;
	uint8_t backup_length;
	uint8_t backup_raw[2]; // REPEAT consumes 2 bytes in the raw buffer.

	/* - Check stick handler initialization */
	if (pSTICK == NULL)
	{
		return ret;
	}

	backup_header = pSTICK->cmd.header;
	backup_length = pSTICK->cmd.length;
	backup_raw[0] = pSTICK->cmd.payload.raw[0];
	backup_raw[1] = pSTICK->cmd.payload.raw[1];

	/*- Prepare repeat command */
	pSTICK->cmd.header = STICK_CMD_REPEAT;
	pSTICK->cmd.length = 0;

	/*- Transfer command/response */
	ret = stick_Transmit(pSTICK, STICK_POLLING_RETRY_COUNT_MAX);

	/*- Check if Transfer successful and format response data */
	if (ret == STICK_OK)
	{
		if (wrap == 0)
		{
			/* - Receive Frame from STICK device */
			ret = pSTICK->io.BusRecv(pSTICK->io.addr,
									 pSTICK->io.speed,
									 &pSTICK->rsp.header,
									 &pSTICK->rsp.length);
			/* - Return STICK Status code */
			ret = (stick_ReturnCode_t)pSTICK->rsp.header;
		}
		else
		{
			/* - Receive Frame from STICK device */
			ret = pSTICK->io.BusRecv(pSTICK->io.addr,
									 pSTICK->io.speed,
									 &pSTICK->ctx.wrap.header,
									 &pSTICK->ctx.wrap.length);
			/* - Return STICK Status code */
			ret = (stick_ReturnCode_t)pSTICK->ctx.wrap.header;
		}
	}

	// Restore pSTICK cmd data
	pSTICK->cmd.header = backup_header;
	pSTICK->cmd.length = backup_length;
	pSTICK->cmd.payload.raw[0] = backup_raw[0];
	pSTICK->cmd.payload.raw[1] = backup_raw[1];

	return ret;
}

stick_ReturnCode_t stick_Transfer(stick_Handler_t * pSTICK, uint32_t expected_response_processing_time)
{
	stick_ReturnCode_t ret = STICK_HANDLER_NOT_INITIALISED;
	uint16_t polling_retry_count = STICK_POLLING_RETRY_COUNT_DEFAULT;

	if (pSTICK != NULL)
	{
		/* - Transmit command to the target STICK device */
		ret = stick_Transmit(pSTICK,polling_retry_count);

		if (ret == STICK_OK)
		{
			/* - Add processing delay if different from 0*/
			if (expected_response_processing_time == 0xFFFF) {
				/* - (long command) set polling retry count to maximum */
				polling_retry_count = STICK_POLLING_RETRY_COUNT_MAX;
				/* - (long command) perform a processing delay of 1s*/
				stick_platform_Delay_ms(1000);
			} else if (expected_response_processing_time != 0)
			{
				stick_platform_Delay_ms(expected_response_processing_time);
			}

			/* - Receive response from the target STICK device */
			ret = stick_Receive(pSTICK, polling_retry_count);
		}

/* N17 code for HQ-299575 by tongjiacheng at 20230616 start */
		if (ret < 0) {
			// Return comm error
			return ret;
		} else {
			// Return other error
			return pSTICK->rsp.header;
		}
	}
	else {
		return STICK_HANDLER_NOT_INITIALISED;
	}
/* N17 code for HQ-299575 by tongjiacheng at 20230616 end */
}

stick_ReturnCode_t stick_Transmit(stick_Handler_t * pSTICK, uint16_t polling_retry_count)
{
	stick_ReturnCode_t ret = STICK_INVALID_PARAMETER;
	uint16_t crc = 0 ;

	for(; polling_retry_count>0; polling_retry_count--)
	{
		if (pSTICK != NULL)
		{
			/* - Wrap command if requested */
			if (pSTICK->cmd.header & 0x80)
			{
				ret = _stick_wrap_cmd(pSTICK);
				if (ret == STICK_OK)
				{
					/* - Compute & concatenate CRC to wrapped frame  */
					crc =  stick_platform_Crc16(&pSTICK->ctx.wrap.header,pSTICK->ctx.wrap.length+1);
					pSTICK->ctx.wrap.payload[pSTICK->ctx.wrap.length] = (uint8_t) (crc >> 8);
					pSTICK->ctx.wrap.payload[pSTICK->ctx.wrap.length+1] = (uint8_t) crc & 0xFF;
					/* - Send wrapped frame + header to STICK device */
					ret = pSTICK->io.BusSend(pSTICK->io.addr,pSTICK->io.speed,(uint8_t *)&pSTICK->ctx.wrap.header,pSTICK->ctx.wrap.length + 3);
				}
			} else {
				/* - Compute & concatenate CRC to buffer */
				crc =  stick_platform_Crc16(&pSTICK->cmd.header,pSTICK->cmd.length + 1);
				pSTICK->cmd.payload.raw[pSTICK->cmd.length] = (uint8_t) (crc >> 8);
				pSTICK->cmd.payload.raw[pSTICK->cmd.length+1] = (uint8_t) crc & 0xFF ;
				/* - Send Frame + header to STICK device */
				ret = pSTICK->io.BusSend(pSTICK->io.addr,pSTICK->io.speed,(uint8_t *)&pSTICK->cmd.header,pSTICK->cmd.length + 3);
			}
		}

		/* - Retry only if there is a BUS_ERROR, other status are not subject to retry */
		if((ret != STICK_BUS_ERR) && (ret != STICK_INVALID_PARAMETER))
		{
			break;
		}

		/* Wait for STICK_POLLING_INTERVAL before retry */
		stick_platform_Delay_ms(STICK_POLLING_INTERVAL);
	}

	return ret;
}

stick_ReturnCode_t stick_Receive(stick_Handler_t * pSTICK, uint16_t polling_retry_count)
{
	stick_ReturnCode_t ret = STICK_INVALID_PARAMETER;
	uint16_t crc, rsp_crc = 0, repeat_count = 0;

	if (pSTICK == NULL)
	{
	 return ret;
	}
	/* Protected response expected */
	if (pSTICK->cmd.header & 0x40)
	{
		/* - Receive Frame from STICK device */
		ret = pSTICK->io.BusRecv(pSTICK->io.addr, pSTICK->io.speed, &pSTICK->ctx.wrap.header, &pSTICK->ctx.wrap.length);
		/* Check correct reception and compute CRC */
		if (ret == STICK_OK)
		{
			if ((pSTICK->io.repeat_support == 1) && (pSTICK->cmd.header != STICK_CMD_HIBERNATE))
			{
				ret = STICK_BUS_ERR;

				while ((repeat_count < STICK_MAX_REPEAT) && (ret != STICK_OK))
				{
					/* - Compute received frame CRC */
					pSTICK->ctx.wrap.length--;
					rsp_crc = (uint16_t)(pSTICK->ctx.wrap.payload[pSTICK->ctx.wrap.length - 2] << 8);
					rsp_crc = rsp_crc + pSTICK->ctx.wrap.payload[pSTICK->ctx.wrap.length - 1];
					pSTICK->ctx.wrap.length = pSTICK->ctx.wrap.length - 2;
					crc = stick_platform_Crc16(&pSTICK->ctx.wrap.header, pSTICK->ctx.wrap.length + 1);
					if (rsp_crc != crc)
					{
						/* CRC error detected */
						ret = STICK_BUS_ERR;
						if (stick_core_repeat(pSTICK, 1))
						{
							break;
						}
						repeat_count++;
					}
					else
					{
						ret = _stick_unwrap_rsp(pSTICK);
					}
				}
			}
			else
			{
				/* - Compute received frame CRC */
				pSTICK->ctx.wrap.length--;
				rsp_crc = (uint16_t)(pSTICK->ctx.wrap.payload[pSTICK->ctx.wrap.length - 2] << 8);
				rsp_crc = rsp_crc + pSTICK->ctx.wrap.payload[pSTICK->ctx.wrap.length - 1];
				pSTICK->ctx.wrap.length = pSTICK->ctx.wrap.length - 2;
				crc = stick_platform_Crc16(&pSTICK->ctx.wrap.header, pSTICK->ctx.wrap.length + 1);
				if (rsp_crc != crc)
				{
					/* CRC error detected */
					ret = STICK_BUS_ERR;
				}
				else
				{
					ret = _stick_unwrap_rsp(pSTICK);
				}
			}
		}
	}
	else
	{
		/* - Receive Frame from STICK device */
		ret = pSTICK->io.BusRecv(pSTICK->io.addr, pSTICK->io.speed, &pSTICK->rsp.header, &pSTICK->rsp.length);
		/* - Verify Frame CRC */
		if (ret == STICK_OK)
		{
			if ((pSTICK->io.repeat_support == 1) && (pSTICK->cmd.header != STICK_CMD_HIBERNATE))
			{
				ret = STICK_BUS_ERR;
				while ((repeat_count < STICK_MAX_REPEAT) && (ret != STICK_OK))
				{
					pSTICK->rsp.length--;
					rsp_crc = (uint16_t)(pSTICK->rsp.payload.raw[pSTICK->rsp.length - 2] << 8);
					rsp_crc = rsp_crc + pSTICK->rsp.payload.raw[pSTICK->rsp.length - 1];
					pSTICK->rsp.length = pSTICK->rsp.length - 2;
					crc = stick_platform_Crc16(&pSTICK->rsp.header, pSTICK->rsp.length + 1);
					if (rsp_crc != crc)
					{
						/* CRC error detected */
						ret = STICK_BUS_ERR;
						if (stick_core_repeat(pSTICK, 0))
						{
							break;
						}
						repeat_count++;
					}
					else
					{
						ret = STICK_OK;
					}
				}
			}
			else
			{
				pSTICK->rsp.length--;
				rsp_crc = (uint16_t)(pSTICK->rsp.payload.raw[pSTICK->rsp.length - 2] << 8);
				rsp_crc = rsp_crc + pSTICK->rsp.payload.raw[pSTICK->rsp.length - 1];
				pSTICK->rsp.length = pSTICK->rsp.length - 2;
				crc = stick_platform_Crc16(&pSTICK->rsp.header, pSTICK->rsp.length + 1);
				if (rsp_crc != crc)
				{
					/* CRC error detected */
					ret = STICK_BUS_ERR;
				}
			}
		}
	}

	return ret;
}


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
