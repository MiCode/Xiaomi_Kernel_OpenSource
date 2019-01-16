/*
 * Copyright (C) 2013 LG Electironics, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */



/****************************************************************************
* Debugging Macros
****************************************************************************/
#define TPD_TAG                  "[S3320] "
#define TPD_FUN(f)               printk(KERN_ERR TPD_TAG"%s\n", __FUNCTION__)
#define TPD_ERR(fmt, args...)    printk(KERN_ERR TPD_TAG"%s %d : "fmt, __FUNCTION__, __LINE__, ##args)
#define TPD_LOG(fmt, args...)    printk(KERN_ERR TPD_TAG fmt, ##args)


struct synaptics_ts_f12_query_5 
{	
union {		
	struct {		
		unsigned char size_of_query_6;	
		struct {			
			unsigned char ctrl_00_is_present:1;			
			unsigned char ctrl_01_is_present:1;			
			unsigned char ctrl_02_is_present:1;			
			unsigned char ctrl_03_is_present:1;			
			unsigned char ctrl_04_is_present:1;			
			unsigned char ctrl_05_is_present:1;			
			unsigned char ctrl_06_is_present:1;			
			unsigned char ctrl_07_is_present:1;		
			} __packed;			
		struct {			
			unsigned char ctrl_08_is_present:1;		
			unsigned char ctrl_09_is_present:1;			
			unsigned char ctrl_10_is_present:1;			
			unsigned char ctrl_11_is_present:1;	
			unsigned char ctrl_12_is_present:1;			
			unsigned char ctrl_13_is_present:1;			
			unsigned char ctrl_14_is_present:1;			
			unsigned char ctrl_15_is_present:1;		
			} __packed;		
		struct {			
			unsigned char ctrl_16_is_present:1;			
			unsigned char ctrl_17_is_present:1;		
			unsigned char ctrl_18_is_present:1;				
			unsigned char ctrl_19_is_present:1;			
			unsigned char ctrl_20_is_present:1;			
			unsigned char ctrl_21_is_present:1;		
			unsigned char ctrl_22_is_present:1;		
			unsigned char ctrl_23_is_present:1;		
			} __packed;			
		struct {		
			unsigned char ctrl_24_is_present:1;		
			unsigned char ctrl_25_is_present:1;		
			unsigned char ctrl_26_is_present:1;		
			unsigned char ctrl_27_is_present:1;		
			unsigned char ctrl_28_is_present:1;		
			unsigned char ctrl_29_is_present:1;			
			unsigned char ctrl_30_is_present:1;		
			unsigned char ctrl_31_is_present:1;		
			} __packed;		
		};		
	unsigned char data[5];	
	};
};
struct synaptics_ts_f12_query_8 
{	
union {		
	struct {	
		unsigned char size_of_query_9;	
		struct {		
			unsigned char data_00_is_present:1;	
			unsigned char data_01_is_present:1;		
			unsigned char data_02_is_present:1;		
			unsigned char data_03_is_present:1;		
			unsigned char data_04_is_present:1;			
			unsigned char data_05_is_present:1;			
			unsigned char data_06_is_present:1;				
			unsigned char data_07_is_present:1;		
			} __packed;		
		struct {			
			unsigned char data_08_is_present:1;		
			unsigned char data_09_is_present:1;		
			unsigned char data_10_is_present:1;		
			unsigned char data_11_is_present:1;			
			unsigned char data_12_is_present:1;			
			unsigned char data_13_is_present:1;			
			unsigned char data_14_is_present:1;			
			unsigned char data_15_is_present:1;		
			} __packed;	
		};	
	unsigned char data[3];	
	};
};


