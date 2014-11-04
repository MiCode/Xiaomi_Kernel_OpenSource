/*
* Support for Medfield PNW Camera Imaging ISP subsystem.
*
* Copyright (c) 2010 Intel Corporation. All Rights Reserved.
*
* Copyright (c) 2010 Silicon Hive www.siliconhive.com.
*
* This program is free software; you can redistribute it and/or
* modify it under the terms of the GNU General Public License version
* 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program; if not, write to the Free Software
* Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
* 02110-1301, USA.
*
*/

#include "sh_css_accelerate.h"

#include "sh_css_hrt.h"		/* sh_css_hrt_sp_wait() */
#include "sh_css_sp_start.h"
#include "sh_css_sp.h"
#include "sh_css_internal.h"

#include "memory_access.h"
#include "mmu_device.h"

#define __INLINE_SP__
#include "sp.h"

#if !defined(C_RUN) && !defined(HRT_UNSCHED)
static const unsigned char *
upload_isp_code(const struct sh_css_fw_info *firmware)
{
	const unsigned char *binary = firmware->isp_code;
	if (!binary) {
		unsigned size = firmware->blob.size;
		const unsigned char *blob;
		const unsigned char *binary_name;
		binary_name =
			(const unsigned char *)(SH_CSS_EXT_ISP_PROG_NAME(
						firmware));
		blob = binary_name +
			strlen((const char *)binary_name) +
			1;
		binary = (const unsigned char *)HOST_ADDRESS(
			sh_css_load_blob(blob, size));
		((struct sh_css_fw_info *)firmware)->isp_code = binary;
		((struct sh_css_fw_info *)firmware)->info.isp.xmem_addr =
			(hrt_vaddress)HOST_ADDRESS(binary);
	}

	if (!binary)
		return NULL;
	return binary;
}

static const unsigned char *
sh_css_acc_upload_isp_code(const struct sh_css_acc_fw *firmware)
{
	struct sh_css_acc_fw_hdr *header
		= (struct sh_css_acc_fw_hdr *)&firmware->header;
	const unsigned char *binary = firmware->header.isp_code;

	if (!binary) {
		const unsigned char *blob = SH_CSS_ACC_ISP_CODE(firmware);
		unsigned size		  = SH_CSS_ACC_ISP_SIZE(firmware);
		binary = (const unsigned char *)HOST_ADDRESS(
			sh_css_load_blob(blob, size));
		header->isp_code = binary;
	}

	if (!binary)
		return NULL;
	sp_dmem_store(SP0_ID, HOST_ADDRESS(header->sp.isp_code),
			     &binary, sizeof(binary));
	return binary;
}
#endif

static void
upload_var(void *sp_address, void *val, size_t size)
{
	if (!sp_address)
		return;
	sp_dmem_store(SP0_ID, HOST_ADDRESS(sp_address), val, size);
}

static void
upload_int(unsigned *sp_address, unsigned *val)
{
	upload_var(sp_address, val, sizeof(unsigned));
}

/* Load the firmware into xmem */
enum sh_css_err
sh_css_acc_load(const struct sh_css_acc_fw *firmware)
{
	struct sh_css_acc_fw_hdr *header
		= (struct sh_css_acc_fw_hdr *)&firmware->header;

	header->loaded = true;
	return sh_css_success;
}

void
sh_css_acc_unload(const struct sh_css_acc_fw *firmware)
{
	struct sh_css_acc_fw_hdr *header
		= (struct sh_css_acc_fw_hdr *)&firmware->header;
	struct sh_css_acc_sp *sp = &header->sp;
	if (sp->code)
		mmgr_free(HOST_ADDRESS(sp->code));
	if (header->isp_code)
		mmgr_free(HOST_ADDRESS(header->isp_code));
	sp->code  = NULL;
	header->isp_code = NULL;
	header->loaded   = false;
}

/* Load the firmware into xmem */
enum sh_css_err
sh_css_acc_load_extension(const struct sh_css_fw_info *firmware)
{
#if !defined(C_RUN) && !defined(HRT_UNSCHED)
	const unsigned char *isp_program
			= upload_isp_code(firmware);
	if (isp_program == NULL)
		return sh_css_err_cannot_allocate_memory;
#endif

	((struct sh_css_fw_info *)firmware)->loaded = true;
	return sh_css_success;
}

void
sh_css_acc_unload_extension(const struct sh_css_fw_info *firmware)
{
	if (firmware->isp_code)
		mmgr_free(HOST_ADDRESS(firmware->isp_code));
	((struct sh_css_fw_info *)firmware)->isp_code = NULL;
	((struct sh_css_fw_info *)firmware)->loaded = false;
}

/* Set acceleration parameter to value <val> */
enum sh_css_err
sh_css_acc_set_parameter(struct sh_css_acc_fw *firmware,
			 struct sh_css_hmm_section parameters)
{
	firmware->header.parameters = parameters;
	return sh_css_success;
}

/* Set firmware parameters to value <parameters> */
enum sh_css_err
sh_css_acc_set_firmware_parameters(struct sh_css_fw_info *firmware,
			 enum sh_css_isp_memories mem,
			 struct sh_css_hmm_section parameters)
{
	firmware->memory_interface[mem].parameters = parameters;
	return sh_css_success;
}

static void
sh_css_acc_init(struct sh_css_acc_fw *firmware)
{
	struct sh_css_acc_sp *sp = &firmware->header.sp;
	unsigned sp_address = (unsigned)HOST_ADDRESS(
			sp->fw.info.sp.ddr_parameter_address);
	unsigned sp_size = (unsigned)HOST_ADDRESS(
			sp->fw.info.sp.ddr_parameter_size);
	unsigned value = firmware->header.parameters.ddr_address;
	unsigned size  = firmware->header.parameters.ddr_size;
/* MW: "sp_address" is an offset address, 0 is a legal value*/
	if (sp_address != 0) {
		sp_dmem_store(SP0_ID, sp_address, &value, sizeof(value));
		sp_dmem_store(SP0_ID, sp_size, &size, sizeof(size));
	}
}

/* Start the sp, which will start the isp.
*/
enum sh_css_err
sh_css_acc_start(struct sh_css_acc_fw *firmware)
{
	struct sh_css_acc_fw_hdr *header
		= (struct sh_css_acc_fw_hdr *)&firmware->header;
	struct sh_css_acc_sp *sp = &header->sp;
	bool is_extension = header->type != SH_CSS_ACC_STANDALONE;
	const struct sh_css_fw_info *sp_fw = &sp->fw;
	const unsigned char *sp_program;
#if !defined(C_RUN) && !defined(HRT_UNSCHED)
	const unsigned char *isp_program;
#endif

	*(const void **)&sp_fw->blob.text = SH_CSS_ACC_SP_CODE(firmware);
	*(const void **)&sp_fw->blob.data = SH_CSS_ACC_SP_DATA(firmware);

	if (!header->loaded)
		return sh_css_err_invalid_arguments;
	if (is_extension)
		return sh_css_err_invalid_arguments;

	/* NOTE: sp accelerators have their (shared buffer pointer)
	 * arguments flushed in "atomisp_acc_start()"
	 */
	if (is_extension)
		sh_css_flush(firmware);

	sp_program = (const unsigned char *)HOST_ADDRESS(
		sh_css_sp_load_program(sp_fw,
		SH_CSS_ACC_SP_PROG_NAME(firmware),
		(hrt_vaddress)HOST_ADDRESS(sp->code)));
	if (sp_program == NULL)
		return sh_css_err_cannot_allocate_memory;
	sp->code = sp_program;
#if !defined(C_RUN) && !defined(HRT_UNSCHED)
	isp_program = sh_css_acc_upload_isp_code(firmware);
	if (isp_program == NULL)
		return sh_css_err_cannot_allocate_memory;
#endif

#ifdef C_RUN
	sp->init(firmware);
#endif
	sh_css_acc_init(firmware);

	/* Start the firmware on the sp, which will start the isp */
#ifdef C_RUN
	/* No need to run the dmem_init in crun */
	sh_css_sp_do_invalidate_mmu();
	csim_processor_set_crun_func(SP, sp->entry);
	hrt_ctl_run(SP, 1);
	hrt_ctl_start(SP);
	hrt_sleep();
/* MW: sp->entry is a pointer, yet the entry points are relative addresses and thus 32-bit */
#elif defined(HRT_CSIM)
	sh_css_sp_do_invalidate_mmu();
	_hrt_cell_start(SP, HOST_ADDRESS(sp->entry));
#else
	sh_css_sp_start((unsigned int)HOST_ADDRESS(sp->entry));
#endif
	return sh_css_success;
}

/* To be called when acceleration has terminated.
*/
void
sh_css_acc_done(struct sh_css_acc_fw *firmware)
{
	(void)firmware;
}

void
sh_css_acc_wait(void)
{
	sh_css_hrt_sp_wait();
}

/* Flag abortion of acceleration */
void sh_css_acc_abort(struct sh_css_acc_fw *firmware)
{
	unsigned int t = true;
	upload_int(firmware->header.sp.css_abort, &t);
}
