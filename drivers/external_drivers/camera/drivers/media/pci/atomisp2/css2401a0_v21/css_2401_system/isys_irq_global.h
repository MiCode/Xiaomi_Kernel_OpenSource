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

#ifndef __ISYS_IRQ_GLOBAL_H__
#define __ISYS_IRQ_GLOBAL_H__

#if defined(USE_INPUT_SYSTEM_VERSION_2401)

/* Register offset/index from base location */
#define ISYS_IRQ_EDGE_REG_IDX		(0)
#define ISYS_IRQ_MASK_REG_IDX		(ISYS_IRQ_EDGE_REG_IDX + 1)
#define ISYS_IRQ_STATUS_REG_IDX		(ISYS_IRQ_EDGE_REG_IDX + 2)
#define ISYS_IRQ_CLEAR_REG_IDX		(ISYS_IRQ_EDGE_REG_IDX + 3)
#define ISYS_IRQ_ENABLE_REG_IDX		(ISYS_IRQ_EDGE_REG_IDX + 4)
#define ISYS_IRQ_LEVEL_NO_REG_IDX	(ISYS_IRQ_EDGE_REG_IDX + 5)

/* Register values */
#define ISYS_IRQ_MASK_REG_VALUE		(0xFFFF)
#define ISYS_IRQ_CLEAR_REG_VALUE	(0xFFFF)
#define ISYS_IRQ_ENABLE_REG_VALUE	(0xFFFF)

#endif /* defined(USE_INPUT_SYSTEM_VERSION_2401) */

#endif	/* __ISYS_IRQ_GLOBAL_H__ */
