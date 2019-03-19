/*
 * Copyright 2014-2017 NXP Semiconductors
 * Copyright (C) 2019 XiaoMi, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
	internal functions for TFA layer (not shared with SRV and HAL layer!)
*/

#ifndef __TFA_INTERNAL_H__
#define __TFA_INTERNAL_H__

#include "tfa_dsp_fw.h"
#include "tfa_ext.h"

#if __GNUC__ >= 4
  #define TFA_INTERNAL __attribute__ ((visibility ("hidden")))
#else
  #define TFA_INTERNAL
#endif

#define TFA98XX_GENERIC_SLAVE_ADDRESS 0x1C

TFA_INTERNAL enum Tfa98xx_Error tfa98xx_check_rpc_status(struct tfa_device *tfa, int *pRpcStatus);
TFA_INTERNAL enum Tfa98xx_Error tfa98xx_wait_result(struct tfa_device *tfa, int waitRetryCount);

#endif /* __TFA_INTERNAL_H__ */

