/*
 * Copyright (c) 2015-2017 MICROTRUST Incorporated
 * Copyright (C) 2019 XiaoMi, Inc.
 * All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "teei_fp.h"
#include "teei_client_transfer_data.h"
#define IMSG_TAG "[tz_driver]"
#include <imsg_log.h>

unsigned long fp_buff_addr;

#if defined(CONFIG_WT_FINGERPRINT_SUPPORT)
extern bool goodix_fp_exist;
extern bool fpc1022_fp_exist;
#else
bool goodix_fp_exist = false;
bool fpc1022_fp_exist = false;
#endif

struct TEEC_UUID uuid_fp = { 0x00000000, 0xc30c, 0x4dd0,
		{ 0xa3, 0x19, 0xea, 0x29, 0x64, 0x3d, 0x4d, 0x4b } };

#define FP_VENDOR_MAX 64                                                        
static DEFINE_MUTEX(fp_vendor_lock);                                            
static uint8_t __fp_vendor_id = 255;                                            
void set_fp_vendor(uint8_t fp_vendor_id)                                        
{                                                                               
    // check param                                                              
    if (fp_vendor_id>FP_VENDOR_MAX) {                                           
        IMSG_ERROR("%s:%d param error , fp_vendor_id=%d,FP_VENDOR_MAX=%d, fp_vendor_id>FP_VENDOR_MAX", __func__, __LINE__,  fp_vendor_id, FP_VENDOR_MAX);
        return;                                                                 
    }                                                                           
                                                                                
    // set only onece                                                           
    if (__fp_vendor_id==255) {                                                  
        mutex_lock(&fp_vendor_lock);                                            
        __fp_vendor_id = fp_vendor_id;                                          
        mutex_unlock(&fp_vendor_lock);                                          
        IMSG_ERROR("%s:%d set __fp_vendor_id=%d", __func__, __LINE__, __fp_vendor_id);
    } else {                                                                    
        IMSG_WARN("%s:%d __fp_vendor_id already seted __fp_vendor_id = %d, return Directly", __func__, __LINE__, __fp_vendor_id);
    }                                                                           
}          

unsigned long create_fp_fdrv(int buff_size)
{
	unsigned long addr = 0;

	if (buff_size < 1) {
		IMSG_ERROR("Wrong buffer size %d:", buff_size);
		return 0;
	}
	addr = (unsigned long) vmalloc(buff_size);
	if (addr == 0) {
		IMSG_ERROR("kmalloc buffer failed");
		return 0;
	}
	memset((void *)addr, 0, buff_size);
	return addr;
}

static struct TEEC_Context context;
static int context_initialized;
int send_fp_command(void *buffer, unsigned long size)
{
	int ret = 0;
	
	//struct TEEC_UUID uuid_ta1 = { 0x7778c03f, 0xc30c, 0x4dd0,
	//{ 0xa3, 0x19, 0xea, 0x29, 0x64, 0x3d, 0x4d, 0x4b } };
	
	//struct TEEC_UUID uuid_ta2 = { 0x8888c03f, 0xc30c, 0x4dd0,
	//{ 0xa3, 0x19, 0xea, 0x29, 0x64, 0x3d, 0x4d, 0x4b } };

	if (buffer == NULL || size < 1)
		return -1;

	if (context_initialized == 0) {
		memset(&context, 0, sizeof(context));
		ret = ut_pf_gp_initialize_context(&context);
		if (ret) {
			IMSG_ERROR("Failed to initialize fp context ,err: %x",
			ret);
			goto release_1;
		}
		context_initialized = 1;
	}
	
	#if 0
	ret = ut_pf_gp_transfer_data(&context, &uuid_ta, 1, buffer, size);
	if (ret) {
		IMSG_ERROR("Failed to transfer data,err: %x", ret);
		goto release_2;
	}
	#endif

	IMSG_ERROR("send fp command start\n");
	if(!goodix_fp_exist && fpc1022_fp_exist) {
		IMSG_ERROR("It's fpc chip vendor\n");
		//uuid_fp = { 0x7778c03f, 0xc30c, 0x4dd0,
		//{ 0xa3, 0x19, 0xea, 0x29, 0x64, 0x3d, 0x4d, 0x4b } };

		uuid_fp.timeLow = 0x7778c03f;

		ret = ut_pf_gp_transfer_data(&context, &uuid_fp, 1, buffer, size);
		if (ret) {
			IMSG_ERROR("Failed to transfer data,err: %x", ret);
			goto release_2;
		}
	} else if (goodix_fp_exist && (!fpc1022_fp_exist)) {
		IMSG_ERROR("It's goodix chip vendor\n");
		// uuid_fp = { 0x8888c03f, 0xc30c, 0x4dd0,
		//{ 0xa3, 0x19, 0xea, 0x29, 0x64, 0x3d, 0x4d, 0x4b } };

		uuid_fp.timeLow = 0x8888c03f;

		ret = ut_pf_gp_transfer_data(&context, &uuid_fp, 1, buffer, size);
		if (ret) {
			IMSG_ERROR("Failed to transfer data,err: %x", ret);
			goto release_2;
		}
	} else {
#if defined(CONFIG_WT_FINGERPRINT_SUPPORT)
		IMSG_ERROR("Can't find fp sensor\n");
#else
		IMSG_ERROR("Do not support fp\n");
#endif
		goto release_2;
	}
	IMSG_ERROR("send fp command end\n");
release_2:
	if (ret) {
		ut_pf_gp_finalize_context(&context);
		context_initialized = 0;
	}
release_1:
	return ret;
}
