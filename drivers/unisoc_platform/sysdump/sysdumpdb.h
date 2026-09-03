/*
 * Copyright (C) 2018 Spreadtrum Communications Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __SYSDUMPDB_H__
#define __SYSDUMPDB_H__
#if 0 /*	only for native app*/
#include "dirent.h"
#include <sys/stat.h>
#include <sys/ioctl.h>
#ifdef CONFIG_NAND
#include <ubi-user.h>
#endif
#define DIR_MAX_NUM 5
#define MINIDUMP_FILE_PATH "/data/minidump"
#define MINIDUMP_FILE_NAME "%02d_minidump_%s"
#define EXCEPTION_FILE_NAME "exception_info_%02d"
#define EXCEPTION_FILE_NUM_MAX 3
#define RESET_COUNT_FILE_NAME "reset_count.txt"
#define REBOOT_HISTORY_FILE_NAME "reboot_history.txt"
#define PARTITION_NAME_MAX_SIZE 128
#endif /*	only for native app end */

#define PROC_DIR "/proc"
#define MINIDUMP_INFO_DIR "sprd_minidump"
#define MINIDUMP_INFO_PROC "minidump_info"
#define SYSDUMPDB_LOG_TAG "sysdumpdb"
#define SYSDUMPDB_PARTITION_NAME "sysdumpdb"
#define UBOOT_MAGIC "U2.0"
#define KERNEL_MAGIC "K2.0"
#define APP_MAGIC "A2.0"

/*	minidump description macro	*/
#define OSRELEASE_SIZE 300  		/* Linux version 4.4.83+ (builder@shhud14) (gcc version 4.9.x 20150123 (prerelease) (GCC) ) #1 SMP PREEMPT Tue Sep 11 12:47:50 CST 2018*/
#define TIME_SIZE 50 			/* 2018-12-12-20-24-24 */
#define REBOOT_REASON_SIZE 50 		/* kernel crash */
#define CONTENTS_DESC_SIZE (1024 - OSRELEASE_SIZE - TIME_SIZE - REBOOT_REASON_SIZE)

/*	minidump contents description macro	*/
#define REGS_NUM_MAX 50 		/* max dump regs num in minidump,real num in regs_info_item  */
#define SECTION_NUM_MAX 200		/* max dump section num in minidump */
#define SECTION_NAME_MAX 40
#define MINIDUMP_MEM_MAX (REGS_NUM_MAX + SECTION_NUM_MAX + 1)
#define PT_BUF_SIZE	(2 * 1024 * 1024)

#define EXTEND_STRING "extend"

enum minidump_info_type {
	MINIDUMP_INFO_PADDR,
	MINIDUMP_INFO_SIZE,
};
char *minidump_info_proc_name[] = {
	"minidump_info_paddr",
	"minidump_info_size",
};

#define GET_MINIDUMP_INFO_NAME(x) minidump_info_proc_name[(x)]
/* the struct to save minidump info description  */
struct info_desc{
	unsigned long paddr;
	int size;
};
/* the struct to save dump header info */
struct dumpdb_header{
	char uboot_magic[4];  		/* for uboot lable,type: "U2.0" ,means uboot saved minidump data*/
	char app_magic[4];    		/* for app lable,type:"A2.0" ,means app read minidump description ok */
	/*	Bit[0] : first boot indicate . 0: first boot ,not init .  1: no first boot , has init
		Bit[1] : sysdump status , 0: disable 1:enable
		Bit[2] : minidump status, 0:disable 1:enable
		Bit[3] : boot from  sysdump status , 0:not from sysdump 1:from sysdump; when undefined mode , do not check this status
		others : reserved.
	*/
	int	dump_flag;
	int	reset_mode;	/*record which reset mode  enter sysdump*/
	struct info_desc minidump_info_desc;
};

/*	the struct to save minidump contents description (1024Bytes)*/
struct minidump_data_desc{
	char osrelease[OSRELEASE_SIZE];
	char time[TIME_SIZE];
	char reboot_reason[REBOOT_REASON_SIZE];
	char minidump_data[CONTENTS_DESC_SIZE];
};


#define EXCEPTION_INFO_SIZE_SHORT 256
#define EXCEPTION_INFO_SIZE_MID 512
#define EXCEPTION_INFO_SIZE_LONG  0x2000
#define MAX_STACK_TRACE_DEPTH 32
#ifdef __aarch64__
#define reg_pc  pc
#define reg_lr  regs[30]
#define reg_sp  sp
#define reg_fp  regs[29]
#else
#define reg_pc  ARM_pc
#define reg_lr  ARM_lr
#define reg_sp  ARM_sp
#define reg_ip  ARM_ip
#define reg_fp  ARM_fp
#endif

struct exception_info_item {
	char kernel_magic[8];  /* "K2.0" :make sure excep data valid */
	char exception_serialno[EXCEPTION_INFO_SIZE_SHORT];
	char exception_kernel_version[EXCEPTION_INFO_SIZE_MID];
	char exception_reboot_reason[EXCEPTION_INFO_SIZE_SHORT];
	char exception_panic_reason[EXCEPTION_INFO_SIZE_SHORT];
	char exception_time[EXCEPTION_INFO_SIZE_SHORT];
	char exception_file_info[EXCEPTION_INFO_SIZE_SHORT];
	int  exception_task_id;
	char exception_task_family[EXCEPTION_INFO_SIZE_SHORT];
	char exception_pc_symbol[EXCEPTION_INFO_SIZE_SHORT];
	char exception_stack_info[EXCEPTION_INFO_SIZE_LONG];
};

enum reg_arch_type {
	ARM,
	ARM64,
	X86,
	X86_64
};
struct regs_info{
	int arch; 	/* 32bit reg unsigned int and 64bit unsigned long, we need use type to mark*/
	int num; 	/* the num of regs will save memery amount */
	unsigned long vaddr; 	/* struct pt_regs vaddr */
	unsigned long paddr; 	/* struct pt_regs paddr */
	int size; 	/* sizeof(struct pt_regs)  */
	int size_comp; 	/* size after compressed  */
};

struct regs_memory_info{
	unsigned long reg_vaddr[REGS_NUM_MAX - 1];	/* if vaddr invalid set it as 0*/
	unsigned long reg_paddr[REGS_NUM_MAX - 1];	/* if paddr invalid set it as 0*/
	int per_reg_memory_size; 		/* memory size amount reg */
	int per_mem_size_comp[REGS_NUM_MAX - 1]; 			/* size after compressed  */
	int valid_reg_num; 			/* maybe some regs value not a valid addr */
	int size;				/* memory size amount reg */
};
struct section_info{
	char section_name[SECTION_NAME_MAX];
	/*Get teh value in kernel use to record elfhdr info in uboot*/
	unsigned long section_start_vaddr;
	unsigned long section_end_vaddr;
	/*Get the value in kernel by __pa  use to get memory contents in uboot */
	unsigned long section_start_paddr;
	unsigned long section_end_paddr;
	int section_size;
	int section_size_comp; 			/* size after compressed  */
};

struct section_info_total{
	struct section_info section_info[SECTION_NUM_MAX];
	int total_size;
	int total_num;
};
/* the struct to save minidump all infomation  */
struct minidump_info{
	char kernel_magic[6];  				  /* make sure minidump data valid */
	struct regs_info regs_info;			  /* | struct pt_regs | 			*/
	struct regs_memory_info regs_memory_info;	  /* | memory amount regs |  , need paddr and size, if paddr invalid set it as 0  */
	struct section_info_total section_info_total;	  /* | sections | , text,rodata,page_table ....,may be logbuf in here */
	int minidump_elfhdr_size;			  /* minidump elfhdr data size: update in uboot  */
	int minidump_elfhdr_size_comp;			  /* minidump elfhdr data size,after compressed  */
	struct minidump_data_desc  desc;		  /* minidump contents description */
	int minidump_data_size;				  /* minidump data total size: regs_all_size + reg_memory_all_size + section_all_size  */
	int compressed;					  /* indicate if minidump data compressed */
	struct exception_info_item exception_info;	  /* exception info */
};

#endif /* __SYSDUMPDB_H__ */
