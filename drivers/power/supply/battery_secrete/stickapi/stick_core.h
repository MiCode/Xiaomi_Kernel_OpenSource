  /*
  * 		COPYRIGHT 2016 STMicroelectronics
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
#ifndef STICK_CORE_H
#define STICK_CORE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include <linux/kernel.h>
#include <linux/module.h>
#include "stick_platform.h"
#include "stick_NVM_attributes.h"

/* Exported Definitions  ------------------------------------------------------------*/
//! Hardware response definitions
#define STICK_HW_OK   ( 0)
#define STICK_HW_ERR  (-1)

#define STICK_MAX_PAYLOAD 0xFF


#define STICK_CMD_ECHO                      	 0x00U  //!< STICK general purpose "Echo" command code
#define STICK_CMD_RESET                      	 0x01U  //!< STICK general purpose "Reset" command code
#define STICK_CMD_HIBERNATE                      0x02U  //!< STICK general purpose "Reset" command code
#define STICK_CMD_GET_TRACEABILITY 				 0x03U  //!< STICK general purpose "Get traceability" command code
#define STICK_CMD_GET_DATA 				 		 0x03U  //!< STICK general purpose "Get data" command code
#define STICK_CMD_GET_PUBLIC_KEY_CERTIFICATE  	 0x04U  //!< STICK general purpose "Get Public key certificate" command code
#define STICK_CMD_DECREMENT  					 0x05U  //!< STICK data partition "Decrement" command code
#define STICK_CMD_READ  						 0x06U  //!< STICK data partition "Read" command code
#define STICK_CMD_UPDATE  						 0x07U  //!< STICK data partition "Update" command code
#define STICK_CMD_ESTABLISH_PAIRING_KEY_START 	 0x08U  //!< STICK "Establish PAIRING Key [start]" command code
#define STICK_CMD_ESTABLISH_PAIRING_KEY_FINALIZE 0x09U  //!< STICK "Establish PAIRING Key [finalize]" command code
#define STICK_CMD_SELECT_PAIRING_KEY 			 0x0AU  //!< STICK "Select PAIRING key" command code
#define STICK_CMD_ESTABLISH_PAIRING_KEY_RESUME   0x0DU  //!< STICK "Establish PAIRING Key [resume]" command code
#define STICK_CMD_GENERATE_SIGNATURE             0x11U  //!< STICK "Generate Signature" command code
#define STICK_CMD_PUT_DATA                 		 0x12U  //!< STICK "Put Data" command code
#define STICK_CMD_SWITCH_PROTOCOL				 0x13U  //!< STICK "Switch Protocol" command code
#define STICK_CMD_REGENERATE                     0x14U  //!< STICK "Regenerate" command code
#define STICK_CMD_KILL							 0x90U  //!< STICK "Kill" command code
#define STICK_CMD_RESET_ZONE_AC                  0xD5U  //!< STICK "Reset zone AC" command code
#define STICK_CMD_PROT_DECREMENT  				 0xC5U  //!< STICK data partition "Decrement" command code
#define STICK_CMD_PROT_READ  					 0xC6U  //!< STICK data partition "Protected Read" command code
#define STICK_CMD_PROT_UPDATE  					 0xC7U  //!< STICK data partition "Protected Read" command code
#define STICK_CMD_AUTHENTICATE 		 			 0x4BU  //!< STICK "Authenticate" command code
#define STICK_CMD_REPEAT                         0x3EU  //!< STICK "Repeat" command code
#define STICK_EXEC_TIME_DEFAULT									10		//!< STICK default command processing time (used when specific time == 0)
#define STICK_EXEC_TIME_ECHO                      	 			6  		//!< STICK general purpose "Echo" command processing time
#define STICK_EXEC_TIME_RESET                      	 			5  		//!< ~1 STICK general purpose "Reset" command processing time
#define STICK_EXEC_TIME_HIBERNATE                      			5  		//!< ~1 STICK general purpose "Reset" command processing time
#define STICK_EXEC_TIME_GET_TRACEABILITY 						5  		//!< ~1 STICK general purpose "Get traceability" command processing time
#define STICK_EXEC_TIME_GET_PUBLIC_KEY_CERTIFICATE  			5  		//!< ~2 STICK general purpose "Get Public key certificate" command processing time
#define STICK_EXEC_TIME_DECREMENT  					 			18 		//!< STICK data partition "Decrement" command processing time
#define STICK_EXEC_TIME_READ  						 			18 		//!< STICK data partition "Read" command processing time
#define STICK_EXEC_TIME_UPDATE  								50  	//!< STICK data partition "Update" command processing time
#define STICK_EXEC_TIME_ESTABLISH_PAIRING_KEY_START 			280		//!< STICK "Establish PAIRING Key [start]" command processing time
#define STICK_EXEC_TIME_ESTABLISH_PAIRING_KEY_FINALIZE 			280		//!< STICK "Establish PAIRING Key [finalize]" command processing time
#define STICK_EXEC_TIME_SELECT_PAIRING_KEY 			 			5 		//!< ~1 STICK "Select PAIRING key" command processing time
#define STICK_EXEC_TIME_ESTABLISH_PAIRING_KEY_RESUME   			280		//!< STICK "Establish PAIRING Key [resume]" command processing time
#define STICK_EXEC_TIME_PROT_DECREMENT  						30  	//!< STICK data partition "Decrement" command processing time
#define STICK_EXEC_TIME_PROT_READ  					 			30  	//!< STICK data partition "Protected Read" command processing time
#define STICK_EXEC_TIME_PROT_UPDATE  							30  	//!< STICK data partition "Protected Update" command processing time
#define STICK_EXEC_TIME_AUTHENTICATE 		 					10  	//!< STICK "Authenticate" command processing time
#define STICK_EXEC_TIME_GET_DATA 		 			 			5  		//!< ~1 STICK "Get Data" command processing time
#define STICK_EXEC_TIME_KILL									20		//!< STICK "Kill" command processing time
#define STICK_EXEC_TIME_GENERATE_SIGNATURE_EDDSA				470		//!< STICK "Generate EDDSA Signature" command processing time
#define STICK_EXEC_TIME_GENERATE_SIGNATURE_EDDSA_TRACEABILITY   470		//!< STICK "Generate EDDSA Signature with Traceability data" command processing time
#define STICK_EXEC_TIME_GENERATE_SIGNATURE_EDDSA_ZONE   		0xFFFF  //!< STICK "Generate EDDSA Signature with zone content" command processing time
#define STICK_EXEC_TIME_PUT_DATA                 	    		15		//!< STICK "Put Data" command processing time
#define STICK_EXEC_TIME_SWITCH_PROTOCOL			     			STICK_EXEC_TIME_DEFAULT  		//!< STICK "Switch Protocol" command processing time
#define STICK_EXEC_TIME_REGENERATE					 			18000	//!< STICK "Regenerate" command processing time
#define STICK_EXEC_TIME_RESET_ZONE_AC                  			22 		//!< STICK "Reset zone AC" command processing time
#define STICK_EXEC_TIME_REPEAT                 			        6 		//!< STICK "Repeat command processing time
#define STICK_POLLING_RETRY_COUNT_DEFAULT 	5	//!< Default number of retry on frame reception error
#define STICK_POLLING_RETRY_COUNT_MAX 		10	//!< Maximum number of retry on frame reception error
#define STICK_POLLING_INTERVAL				10	//!< Time interval between two retry
/* Exported types ------------------------------------------------------------*/
#include "stick_types.h"
/**
 * \enum stick_state_t
 * \brief STICK response codes enumeration
 */
typedef enum
{
  STICK_IDLE	= 0,
  STICK_PKC_RECEIVED,
  STICK_PAIRING_STARTED,
}stick_state_t;

/**
 * \struct stick_Handler_t
 * \brief STICK Handler structure
 *
 * This handler stores all the context information related to a specific STICK traget
 * when it creates both Commands and Receive Buffers and secure context
 *
 */
typedef struct
{

	/**
	 * \struct stick_Handler_t
	 * \brief STICK Handler structure
	 *
	 * This handler stores all the context information related to a specific STICK traget
	 * when it creates both Commands and Receive Buffers and secure context
	 *
	 */
	struct
	{

		uint8_t length; /*!< Command Length */

		uint8_t header; /*!< Command Header */

		/**
		 * \union payload
		 * \brief test
		 */
		union
		{
			uint8_t raw[STICK_MAX_PAYLOAD]; /*!< Payload Raw data */

			/**
			 * \struct get_data
			 * \brief test
			 */
			struct
			{
				uint8_t tag;			/*!< Get Data Tag */
				uint8_t additonal_data; /*!< Get Data additional_data */
			} get_data;

			/**
			 * \struct Echo
			 * \brief test
			 */
			struct
			{
				uint8_t message[STICK_MAX_PAYLOAD];	/*!< Echo Message */
			} echo ;

			/**
			 * \struct Decrement
			 * \brief test
			 */
			struct
			{
				uint8_t new_update_ac:4;     /*!< new_update_ac */
				uint8_t new_update_ac_change_right:1;  /*!< new_update_ac_change_right */
				uint8_t change_ac_indicator:1;
				uint8_t filler:2;
				uint8_t zone_index;
				uint8_t offset[2];
				uint8_t amount[3];
				uint8_t data[STICK_MAX_PAYLOAD-7];
			} decrement;

			/**
			 * \struct read
			 * \brief test
			 */
			struct
			{
				uint8_t new_read_ac:4;
				uint8_t new_read_ac_change_right:1;
				uint8_t change_ac_indicator:1;
				uint8_t filler:2;
				uint8_t zone_index;
				uint8_t offset[2];
				uint8_t length[2];
			} read;

			/**
			 * \struct update
			 * \brief test
			 */
			struct
			{
				uint8_t new_update_ac:4;
				uint8_t new_update_ac_change_right:1;
				uint8_t change_ac_indicator:1;
				uint8_t filler:1;
				uint8_t atomicity:1;
				uint8_t zone_index;
				uint8_t offset[2];
				uint8_t data[STICK_MAX_PAYLOAD-4];
			}  update;

			struct /* Establish pairing key start command */
			{
				uint8_t host_ecdh_public_key[32];
				uint8_t host_challenge[16];
				uint8_t magic_value[16];
			} establish_pairing_key_start;

			struct /* Select pairing key command */
			{
				uint8_t slot_number[2];
				uint8_t session_identifier[16];
			}  select_pairing_key;


			struct /* Authenticate command */
			{
				uint8_t host_chalenge[8];
			} authenticate;

			struct /* Regenerate command */
			{
				uint8_t password[16];
			} regenerate;


			struct /* Generate signature command */
			{
				uint8_t challenge[16];
				uint8_t reference;
			} generate_signature;

			struct /* Put-Data Command */
			{
				uint8_t tag;
				uint8_t value[STICK_MAX_PAYLOAD-1];
			} put_data;

			struct /* Switch protocol Command */
			{
				uint8_t protocol_id;
			} switch_protocol;


			struct /* Reset Zone IC command */
			{
				uint8_t index;
				uint8_t reset_update_ac:1;
				uint8_t reset_read_ac:1;
				uint8_t filler:6;
			} reset_zone_ac;

		} payload;

	}cmd;

	struct /* Handler Response Frame */
	{
		uint8_t length;

		uint8_t header;

		union /* response payload */
		{
			uint8_t raw[STICK_MAX_PAYLOAD];

			struct /* Get Traceability */
			{
				uint8_t product_type[8];
				uint8_t cpsn[7];
				uint8_t remaining_signatures[2];
				uint8_t remaining_ECDHE[2];
				uint8_t total_pairing_slots[2];
				uint8_t remaining_pairing_slots[2];
			} get_traceability;

			union /* Get Public Certificate */
			{
				uint8_t raw_data[106];
				struct /*Certificate Elements*/
				{
					uint8_t format;
					uint8_t ca_key_id;
					uint8_t unic_identifier[8];
					uint8_t leaf_public_key[32];
					uint8_t signature_r[32];
					uint8_t signature_s[32];
				}element;
			} get_public_key_certificate;

			struct /* Establish Pairing Key Start */
			{
				uint8_t cartridge_slot_number[2];
				uint8_t cartridge_ecdh_public_key[32];
				struct
				{
					uint8_t r[32];
					uint8_t s[32];
				} signature;
			} establish_pairing_key_start;

			struct /* Select Pairing Key */
			{
				uint8_t sequence_counter[2];
			}select_pairing_key;

			struct /* Decrement */
			{
				uint8_t one_way_counter[3];
				uint8_t data[STICK_MAX_PAYLOAD-3];
			} decrement;

			struct /* Read */
			{
				uint8_t data[STICK_MAX_PAYLOAD];
			} read;

			struct /* Echo */
			{
				uint8_t message[STICK_MAX_PAYLOAD];
			} echo;

			struct /* Get Data */
			{
				uint8_t data[STICK_MAX_PAYLOAD];
			} get_data;

			struct /* Generate Signature */
			{
				union
				{
					uint8_t raw_data[64];
					struct
					{
						uint8_t R[32];
						uint8_t S[32];
					}element;
				} signature;
			}generate_signature;

		} payload;
	}rsp;

	struct /* Handler I/O */
	{
		int8_t (* BusRecv)(uint8_t, uint8_t, uint8_t *, uint8_t *);
		int8_t (* BusSend)(uint8_t, uint8_t, uint8_t *, uint8_t);
		void (* BusWake)(uint8_t);
		void (* BusRecovery)(uint8_t, uint8_t);
		int8_t addr;
		int8_t speed;
		int8_t repeat_support;
	} io;

	struct /* Handler Context */
	{
		stick_state_t state;

		union /* public_key_certificate */
		{
			uint8_t raw_data[106];

			struct
			{
				uint8_t format;
				uint8_t ca_key_id;
				uint8_t unic_identifier[8];
				uint8_t leaf_public_key[32];
				uint8_t signature_r[32];
				uint8_t signature_s[32];
			}element;

		}public_key_certificate;

		uint8_t  pairing_key_slot;

		uint8_t  sequence_counter[2];

		uint8_t  session_ID[16];

		uint8_t  current_R_MAC_IV[8];

		uint8_t sigma_key_value[16];

		struct /* Message Signature */  {
			uint8_t r[32];
			uint8_t s[32];
		} message_signature;

		struct /* Message Digest */
		{
			uint8_t host_ecdh_public_key[32];
			uint8_t cartridge_ecdh_public_key[32];
			uint8_t Host_challenge[16];
			uint8_t cartridge_slot_number[2];
		} message_digest;

		struct /* Wrap Frame */
		{
			uint8_t length;
			uint8_t header;
			uint8_t payload[0xFF];
		}wrap;

	} ctx;

} stick_Handler_t;



/* Exported Functions  ------------------------------------------------------------*/

/*!
  * \brief      Erase volatile attributes stored in RAM in case of Error in Authentication/pairing process
  */
void stick_erase_context (stick_Handler_t *pSTICK);

/*!
  * \brief      Erase cotext and perfor a busrecovery
  */
void stick_powerOnReset(stick_Handler_t * pSTICK);

/*!
 * \brief : Remove key from STICK_KEY_TABLE and reorder table
 * \param[in] iKEY the key index in STICK_KEY_TABLE
 * \result : 0x00 if RMAC valid ; 0x01 if invalid
 */
void stick_remove_key(uint8_t iKEY);


/*!
 * \brief : Send and receive STICK-A10x frames using information defined in the
 *          STICK handler (commands and payload defined by the handler TXBuffer
 *          content)
 * \param[in] pSTICK the pointer to STICK handler
 * \param[in] inter_frame_delay the inter Frame delay in milliseconds (Stick CMD processing time)
 * \result : STICK_OK on success
 *			 stick_ReturnCode_t error code otherwise
 */
stick_ReturnCode_t stick_Transfer(stick_Handler_t * pSTICK, uint32_t expected_response_processing_time);

/*!
 * \brief : Send STICK-A10x frames using information defined in the
 *          STICK handler (commands and payload defined by the handler TXBuffer
 *          content)
 * \param[in] pSTICK the pointer to STICK handler
 * \param[in] polling_retry_count transmit retry number
 * \result : STICK_OK on success
 *			 stick_ReturnCode_t error code otherwise
 */
stick_ReturnCode_t stick_Transmit(stick_Handler_t * pSTICK, uint16_t polling_retry_count);

/*!
 * \brief : Receive STICK-A10x frames using information defined in the
 *          STICK handler (commands and payload defined by the handler TXBuffer
 *          content)
 * \param[in] pSTICK the pointer to STICK handler
 * \param[in] polling_retry_count Receive retry number
 * \result : STICK_OK on success
 *			 stick_ReturnCode_t error code otherwise
 */
stick_ReturnCode_t stick_Receive(stick_Handler_t * pSTICK, uint16_t polling_retry_count);

/*!
 * \brief : Ask STICK-A10x to repeat previous frame sent to Host
 * \param[in] pSTICK the pointer to STICK handler
 * \param[in] select receive target buffer (rsp or wrap buffer)
 * \result : STICK_OK on success
 *			 stick_ReturnCode_t error code otherwise
 */
stick_ReturnCode_t stick_core_repeat(stick_Handler_t * pSTICK, uint8_t wrap);

void stick_get_public_key_certificate_cmd_dbg(stick_Handler_t* pSTICK);

void stick_authenticate_cmd_dbg(stick_Handler_t* pSTICK);

void stick_select_pairing_key_cmd_dbg(stick_Handler_t* pSTICK);

void stick_establish_pairing_key_start_cmd_dbg(stick_Handler_t* pSTICK);

void stick_establish_pairing_key_finalize_dbg(stick_Handler_t* pSTICK);

void stick_decrement_dbg(stick_Handler_t* pSTICK);

void stick_xfer_dbg(stick_Handler_t* pSTICK);



#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* STICK_CORE_H */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
