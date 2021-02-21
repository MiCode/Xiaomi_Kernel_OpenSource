/* SPDX-License-Identifier: GPL-2.0 */
/*! \file ch101_gpr.h
 *
 * \brief Internal definitions for the Chirp CH101 GPR sensor firmware.
 *
 * This file contains register offsets and other values for use with the CH101
 * GPR sensor firmware.  These values are subject to change without notice.
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

#ifndef CH101_GPR_H_
#define CH101_GPR_H_

#include "ch101.h"
#include "soniclib.h"

/* GPR firmware registers */
#define CH101_GPR_REG_OPMODE		0x01
#define CH101_GPR_REG_TICK_INTERVAL	0x02
#define CH101_GPR_REG_PERIOD		0x05
#define CH101_GPR_REG_CAL_TRIG		0x06
#define CH101_GPR_REG_CAL_TRIG		0x06
#define CH101_GPR_REG_MAX_RANGE		0x07
#define CH101_GPR_REG_CALC		0x08
#define CH101_GPR_REG_ST_RANGE		0x12
#define CH101_GPR_REG_READY		0x14
#define CH101_GPR_REG_TOF_SF		0x16
#define CH101_GPR_REG_TOF		0x18
#define CH101_GPR_REG_AMPLITUDE		0x1A
#define CH101_GPR_REG_CAL_RESULT	0x0A
#define CH101_GPR_REG_DATA		0x1C

/* XXX need more values (?) */
#define CMREG_READY_FREQ_LOCKED_BM	(0x02)

#define	CH101_GPR_CTR			(0x2B368)

extern char ch101_gpr_version[CH101_FW_VERS_SIZE];    // version string
extern u8 ch101_gpr_fw[CH101_FW_SIZE + 32];

uint16_t get_ch101_gpr_fw_ram_init_addr(void);
uint16_t get_ch101_gpr_fw_ram_init_size(void);

unsigned char *get_ram_ch101_gpr_init_ptr(void);

u8 ch101_gpr_init(struct ch_dev_t *dev_ptr, struct ch_group_t *grp_ptr,
	u8 i2c_addr, u8 dev_num, u8 i2c_bus_index);

void ch101_gpr_store_pt_result(struct ch_dev_t *dev_ptr);

#endif
