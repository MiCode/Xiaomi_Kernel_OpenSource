/* SPDX-License-Identifier: GPL-2.0 */
/*! \file ch201_gprmt.h
 *
 * \brief Internal definitions for the Chirp CH201 GPR Multi-threshold sensor
 * firmware.
 *
 * This file contains register offsets and other values for use with the CH201
 * GPR Multi-threshold sensor firmware.  These values are subject to change
 * without notice.
 *
 * You should not need to edit this file or call the driver functions directly.
 * Doing so will reduce your ability to benefit from future enhancements and
 * releases from Chirp.
 */

/*
 * Copyright (c) 2016-2019, Chirp Microsystems.  All rights reserved.
 *
 * Chirp Microsystems CONFIDENTIAL
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL CHIRP MICROSYSTEMS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef CH201_GPRMT_H_
#define CH201_GPRMT_H_

#include "ch201.h"
#include "soniclib.h"

/* GPR with multi thresholds firmware registers */
#define CH201_GPRMT_REG_OPMODE		0x01
#define CH201_GPRMT_REG_TICK_INTERVAL	0x02
#define CH201_GPRMT_REG_PERIOD		0x05
#define CH201_GPRMT_REG_CAL_TRIG	0x06
#define CH201_GPRMT_REG_MAX_RANGE	0x07
#define CH201_GPRMT_REG_THRESH_LEN_0	0x08
#define CH201_GPRMT_REG_THRESH_LEN_1	0x09
#define CH201_GPRMT_REG_CAL_RESULT	0x0A
#define CH201_GPRMT_REG_THRESH_LEN_2	0x0C
#define CH201_GPRMT_REG_THRESH_LEN_3	0x0D
#define CH201_GPRMT_REG_ST_RANGE	0x12
#define CH201_GPRMT_REG_READY		0x14
#define CH201_GPRMT_REG_THRESH_LEN_4	0x15
/* start of array of six 2-byte threshold levels */
#define CH201_GPRMT_REG_THRESHOLDS	0x16
#define CH201_GPRMT_REG_TOF_SF		0x22
#define CH201_GPRMT_REG_TOF		0x24
#define CH201_GPRMT_REG_AMPLITUDE	0x26
#define CH201_GPRMT_REG_DATA		0x28

#define CMREG_READY_FREQ_LOCKED_BM	(0x02)

#define CH201_GPRMT_MAX_SAMPLES			(450)	// max number of samples
/* total number of thresholds */
#define CH201_GPRMT_NUM_THRESHOLDS	(6)

#define CH201_FW_VERS_SIZE  32

extern  char ch201_gprmt_version[CH201_FW_VERS_SIZE];
    // version string in fw .c file
extern  u8 ch201_gprmt_fw[CH201_FW_SIZE + 32];

uint16_t get_ch201_gprmt_fw_ram_init_addr(void);
uint16_t get_ch201_gprmt_fw_ram_init_size(void);

unsigned char *get_ram_ch201_gprmt_init_ptr(void);

u8 ch201_gprmt_init(struct ch_dev_t *dev_ptr, struct ch_group_t
	*grp_ptr, u8 i2c_addr, u8 dev_num, u8 i2c_bus_index);

#endif
