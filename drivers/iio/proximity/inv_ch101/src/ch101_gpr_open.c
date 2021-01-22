// SPDX-License-Identifier: GPL-2.0
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

#include "soniclib.h"
#include "ch101_gpr_open.h"
#include "ch_common.h"

u8 ch101_gpr_open_init(struct ch_dev_t *dev_ptr, struct ch_group_t *grp_ptr,
	u8 i2c_addr, u8 io_index, u8 i2c_bus_index)

{
	if (io_index < 3)
		dev_ptr->part_number = CH101_PART_NUMBER;
	else
		dev_ptr->part_number = CH201_PART_NUMBER;

	dev_ptr->app_i2c_address = i2c_addr;
	dev_ptr->io_index = io_index;
	dev_ptr->i2c_bus_index = i2c_bus_index;

	/* Init firmware-specific function pointers */
	dev_ptr->firmware = ch101_gpr_open_fw;

	dev_ptr->fw_version_string = ch101_gpr_open_version;
	dev_ptr->ram_init = get_ram_ch101_gpr_open_init_ptr();
	dev_ptr->get_fw_ram_init_size = get_ch101_gpr_open_fw_ram_init_size;
	dev_ptr->get_fw_ram_init_addr = get_ch101_gpr_open_fw_ram_init_addr;

	dev_ptr->prepare_pulse_timer = ch_common_prepare_pulse_timer;
	dev_ptr->store_pt_result = ch101_gpr_open_store_pt_result;
	dev_ptr->store_op_freq = ch_common_store_op_freq;
	dev_ptr->store_bandwidth = ch_common_store_bandwidth;
	dev_ptr->store_scalefactor = ch_common_store_scale_factor;
	dev_ptr->get_locked_state = ch_common_get_locked_state;

	/* Init API function pointers */
	dev_ptr->api_funcs.fw_load = ch_common_fw_load;
	dev_ptr->api_funcs.set_mode = ch_common_set_mode;
	dev_ptr->api_funcs.set_sample_interval = ch_common_set_sample_interval;
	dev_ptr->api_funcs.set_num_samples = ch_common_set_num_samples;
	dev_ptr->api_funcs.set_max_range = ch_common_set_max_range;
	dev_ptr->api_funcs.set_static_range = ch_common_set_static_range;
	dev_ptr->api_funcs.get_range = ch_common_get_range;
	dev_ptr->api_funcs.get_amplitude = ch_common_get_amplitude;
	dev_ptr->api_funcs.get_iq_data = ch_common_get_iq_data;
	dev_ptr->api_funcs.samples_to_mm = ch_common_samples_to_mm;
	dev_ptr->api_funcs.mm_to_samples = ch_common_mm_to_samples;
	dev_ptr->api_funcs.set_thresholds = NULL;	// not supported
	dev_ptr->api_funcs.get_thresholds = NULL;	// not supported

	/* Init device and group descriptor linkage */
	dev_ptr->group = grp_ptr;		// set parent group pointer
	grp_ptr->device[io_index] = dev_ptr;	// add to parent group

	return 0;
}

void ch101_gpr_open_store_pt_result(struct ch_dev_t *dev_ptr)
{
	u16 rtc_cal_result;
	u16 calc_val;
	u32 count;

	chdrv_read_word(dev_ptr, CH101_GPR_OPEN_REG_CAL_RESULT,
		&rtc_cal_result);

	count = (rtc_cal_result * 1000) / dev_ptr->group->rtc_cal_pulse_ms;

	calc_val = (u16)((u32)CH101_GPR_OPEN_CTR * 16U
		* CH101_FREQCOUNTERCYCLES / count);
	chdrv_write_word(dev_ptr, CH101_GPR_OPEN_REG_CALC, calc_val);

	dev_ptr->rtc_cal_result = rtc_cal_result;
}

