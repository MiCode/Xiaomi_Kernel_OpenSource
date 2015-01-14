/*
 * INTEL CONFIDENTIAL
 *
 * Copyright (C) 2010 - 2014 Intel Corporation.
 * All Rights Reserved.
 *
 * The source code contained or described herein and all documents
 * related to the source code ("Material") are owned by Intel Corporation
 * or licensors. Title to the Material remains with Intel
 * Corporation or its licensors. The Material contains trade
 * secrets and proprietary and confidential information of Intel or its
 * licensors. The Material is protected by worldwide copyright
 * and trade secret laws and treaty provisions. No part of the Material may
 * be used, copied, reproduced, modified, published, uploaded, posted,
 * transmitted, distributed, or disclosed in any way without Intel's prior
 * express written permission.
 *
 * No License under any patent, copyright, trade secret or other intellectual
 * property right is granted to or conferred upon you by disclosure or
 * delivery of the Materials, either expressly, by implication, inducement,
 * estoppel or otherwise. Any license under such intellectual property rights
 * must be express and approved by Intel in writing.
 */

#ifndef __IA_CSS_ISYS_IRQ_H__
#define __IA_CSS_ISYS_IRQ_H__

#include <type_support.h>
#include <storage_class.h>
#include <system_local.h>

#if defined(USE_INPUT_SYSTEM_VERSION_2401)

#ifndef __INLINE_ISYS2401_IRQ__

#define STORAGE_CLASS_ISYS2401_IRQ_H STORAGE_CLASS_EXTERN
#define STORAGE_CLASS_ISYS2401_IRQ_C STORAGE_CLASS_EXTERN
#include "isys_irq_public.h"

#else  /* __INLINE_ISYS2401_IRQ__ */

#define STORAGE_CLASS_ISYS2401_IRQ_H STORAGE_CLASS_INLINE
#define STORAGE_CLASS_ISYS2401_IRQ_C STORAGE_CLASS_INLINE
#include "isys_irq_private.h"

#endif /* __INLINE_ISYS2401_IRQ__ */

#endif /* defined(USE_INPUT_SYSTEM_VERSION_2401) */

#endif	/* __IA_CSS_ISYS_IRQ_H__ */
