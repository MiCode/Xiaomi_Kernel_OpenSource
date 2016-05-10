/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */
/*
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 *
 * Airgo Networks, Inc proprietary. All rights reserved.
 * This file schSysParams.h contains scheduler parameter definitions
 *
 * Author:      Sandesh Goel
 * Date:        02/25/02
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 * 
 */
 
#ifndef __SCH_SYS_PARAMS_H__
#define __SCH_SYS_PARAMS_H__

/// Unsolicited poll period (ms) (0 to disable)
#define SCH_POLL_PERIOD                1000

/// RR timeout value (ms) (0 to disable)
#define SCH_RR_TIMEOUT_MS                40

/// Default CP:CFP ratio
#define SCH_DEFAULT_CP_TO_CFP_RATIO       0

/// Default maximum CFP duration (us)
#define SCH_DEFAULT_MAX_CFP_TIME       60000

/// Default minimum CP duration (us)
#define SCH_DEFAULT_MIN_CP_TIME         100

/// Amount of delay prior to starting CFP (us)
#define SCH_CFP_START_DELAY             100

/// Unit of Txop in micro seconds
#define TXOP_UNIT_IN_USEC                32

/// Minimum amount of time granted per instruction on average (units of txop)
#define MIN_TXOP_PER_INSTRUCTION         50

/// Maximum amount of time granted per instruction (units of txop)
#define MAX_TXOP_PER_INSTRUCTION        300 // HACK - 100

/// Maximum amount of time granted to one entire schedule (units of txop)
#define MAX_TXOP_PER_SCHEDULE           400

/// Scheduling quantum (units of TXOP)
#define SCH_QUANTUM_QUEUE                 4 

/// Maximum unused quantum allowed to be accumulated by a queue
#define MAX_ACCUMULATED_QUANTUM         500

/// Minimum allocated quantum for an uplink flow before a poll instruction is written
#define SCH_MIN_UL_ALLOC                 12

// ---- Scheduling Policy ----

/// Number of QOS classes
#define SCH_NUM_QOS_CLASSES    2

#define SCH_POLICY_STRICT_PRI  0
#define SCH_POLICY_DRR         1

/// Scheduling quantum for each class if using DRR
#define SCH_QUANTUM_CLASS      100

/// The default scheduling policy between classes
#define SCH_POLICY_DEFAULT     SCH_POLICY_STRICT_PRI

// Scheduling weights for each priority

#define SCH_QUANTUM_0    40
#define SCH_QUANTUM_1    36
#define SCH_QUANTUM_2    32
#define SCH_QUANTUM_3    28
#define SCH_QUANTUM_4    24
#define SCH_QUANTUM_5    20
#define SCH_QUANTUM_6    16
#define SCH_QUANTUM_7    12

#endif
