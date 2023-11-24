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

#ifndef STICK_NVM_ATTR_H
#define STICK_NVM_ATTR_H

#include <linux/kernel.h>
#include <linux/module.h>

#define DEV 1
#define PROD 2
#if (RELEASE_TYPE == DEV)
#undef STICK_USE_PRODUCTION_CA_KEY_ONLY
#elif (RELEASE_TYPE == PROD)
#define STICK_USE_PRODUCTION_CA_KEY_ONLY 1
#else
#error "RELEASE_TYPE value unexpected"
#endif

#ifdef __cplusplus
extern "C" {
#endif


#ifdef STM32G031xx
#include "stm32g0xx.h"
#endif

#ifdef STM32L452xx
#include "stm32l4xx.h"
#endif

//#define NV_ATTRIBUTE __attribute__((section (".Non_Volatile_Attribute")))
#define NV_ATTRIBUTE

typedef struct {
	uint8_t cartridge_ID[8];
	uint8_t cartridge_slot_number[2];
	uint8_t key_value[16];
	uint8_t Sequence_counter[2];
}stick_key_slot_t;

typedef struct {
	uint8_t Key_ID;
	uint8_t Key_value[32];
}CA_public_key_t;

typedef struct {
	uint8_t private_key[32];
	uint8_t public_key[32];
}host_ecdh_key_pair_t;

typedef struct {
	stick_key_slot_t STICK_KEY_TABLE [70]; 	 // 73 x 28bytes =   1400 Bytes
	uint8_t STICK_KEY_TABLE_INDEX;        	 // 1 byte
}Non_Volatile_Attributes_t;

extern NV_ATTRIBUTE Non_Volatile_Attributes_t NVM_ATTRIBUTES;

/* Private constant (NVM attributes- UM Table 5) ------------------------------------ */
extern const uint8_t APP_STICK_A100[4];
extern const uint8_t KEY_TYPE_SHARED_SECRET;
extern const uint8_t KEY_TYPE_PAIRING;
extern const uint8_t KEY_TYPE_PHI_2;
extern const uint8_t KEY_TYPE_PWD;
extern const uint8_t KEY_TYPE_DUMMY;
extern const uint8_t KDF_LABEL_STATIC[9];
extern const uint8_t KDF_LABEL_COUPON_ED25519[9];
extern const uint8_t KDF_LABEL_COUPON_X25519[9];
extern const uint8_t KDF_CONTEXT_DUMMY;
extern const uint8_t KDF_CONTEXT_PHI_1;
extern const uint8_t KDF_CONTEXT_PHI_2;
extern const CA_public_key_t Generic_CA_public_key;
extern const CA_public_key_t Production_CA_public_key;
extern const host_ecdh_key_pair_t Host_ECDH_key_pair;

/* Private typedef ------------------------------------------------------------------*/
extern Non_Volatile_Attributes_t nvm_attribute_copy;

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* STICK_NVM_ATTR_H */
