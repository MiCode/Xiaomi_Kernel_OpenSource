// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include "tee_client_api.h"


/* tee_sanity TA */
static const struct TEEC_UUID TEE_SANITY_CA_UUID_STRUCT = {
	0x05150000, 0x0000, 0x0000, {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
static struct TEEC_Context  *g_context;
static struct TEEC_Session  *g_session;

#define TA_INSTANCES_MAX	(10)

struct test_result {
	uint32_t result;
	uint32_t elapsed_time; /* in unit of ms */
};

/* Command ID's for communication CA -> TA. */
#define CMD_GET_NUM_OF_TEST_CASE (1)
#define CMD_GET_TEST_CASE_DESC   (2)
#define CMD_START_TEST_CASE      (3)

/* For Memory test */
#define TST_TA_MEM_SIZE			(4 * 1024)
#define MALLOC_TEST_SZ			(0x100)
#define REALLOC_TEST_SZ			(2 * MALLOC_TEST_SZ)

#define MEMFILL_TEST_PATTERN1		(0x12)
#define MEMFILL_TEST_PATTERN2		(0x34)
#define MEMFILL_TEST_PATTERN3		(0x56)

#define TEE_SANITY_ION_FLAG_CACHED	(0x0)
#define TEE_SANITY_ION_FLAG_UNCACHED	(0x1)

#define SIZE_512B     0x00000200    /* 512B  */
#define SIZE_1K       0x00000400    /* 1K    */
#define SIZE_2K       0x00000800    /* 2K    */
#define SIZE_4K       0x00001000    /* 4K    */
#define SIZE_8K       0x00002000    /* 8K    */
#define SIZE_16K      0x00004000    /* 16K   */
#define SIZE_32K      0x00008000    /* 32K   */
#define SIZE_64K      0x00010000    /* 64K   */
#define SIZE_128K     0x00020000    /* 128K  */
#define SIZE_256K     0x00040000    /* 256K  */
#define SIZE_512K     0x00080000    /* 512K  */
#define SIZE_1M       0x00100000    /* 1M    */
#define SIZE_2M       0x00200000    /* 2M    */
#define SIZE_4M       0x00400000    /* 4M    */
#define SIZE_8M       0x00800000    /* 8M    */
#define SIZE_16M      0x01000000    /* 16M   */
#define SIZE_32M      0x02000000    /* 32M   */
#define SIZE_64M      0x04000000    /* 64M   */
#define SIZE_128M     0x08000000    /* 128M  */
#define SIZE_256M     0x10000000    /* 256M  */
#define SIZE_320M     0x14000000    /* 320M  */
#define SIZE_1G       0x40000000    /* 1G    */
#define SIZE_2G       0x80000000    /* 2G    */
#define SIZE_U32_MAX  0xFFFFFFFF    /* 4G-1  */

struct AllocParameters {
	int size;
	int alignment;
};

static struct AllocParameters alloc_test_params[] = {
	{SIZE_512B, 0},
	{SIZE_1K,   0},
	{SIZE_2K,   0},
	{SIZE_4K,   0},
	{SIZE_8K,   0},
	{SIZE_16K,  0},
	{SIZE_32K,  0},
	{SIZE_64K,  0},
	{SIZE_128K, 0},
	{SIZE_256K, 0},
	{SIZE_512K, 0},
	{SIZE_1M,   0},
	{SIZE_2M,   0},
	{SIZE_U32_MAX, 0},  // stop here, microtrust just support < 4MB
	{SIZE_4M,   0},
	{SIZE_8M,   0},
	{SIZE_16M,  0},
	{SIZE_32M,  0},
	{SIZE_64M,  0},
};

enum TEESANITY_CMD {
	CMD_TA_BASIC = 0,
	CMD_TA_PERSISTENT_OBJ,
	CMD_TA_TRANSIENT_OBJ,
	CMD_TA_WRITE,
	CMD_TA_READ,
	CMD_TA_GET_SYS_TIME,
	CMD_TA_GET_REE_TIME,
	CMD_TA_GET_SET_PERSIST_TIME,
	CMD_TA_MEMORY_ALLOC,
	CMD_TA_MEMORY_REALLOC,

	/* 10 */
	CMD_TA_MEMORY_FILL,
	CMD_TA_MEMORY_COMPARE,
	CMD_TA_MEMORY_MOVE,
	CMD_TA_GEN_RND,
	CMD_TA_CALCULATION_PERF,
	CMD_TA_CRYPTO_AES,
	CMD_TA_CRYPTO_HASH,
	CMD_TA_CRYPTO_HMAC,
	CMD_TA_CRYPTO_RSA,
	CMD_TA_CRYPTO_PBKDF2,

	/* 20 */
	CMD_DRV_BASIC,
	CMD_DRV_MEMORY,
	CMD_DRV_STRING,
	CMD_DRV_TIME,
	CMD_DRV_INTERRUPT,

	CMD_REGISTER_SHARED_MEMORY,
	CMD_REGISTER_ION_SHARED_MEMORY,

	TEESANITY_CMD_TOTAL,
};


struct CmdName {
	enum TEESANITY_CMD cmd;
	const char *name;
};

static struct CmdName TeeSanityCmdToName[] = {
	{CMD_TA_BASIC,                   "CMD_TA_BASIC"},
	{CMD_TA_PERSISTENT_OBJ,          "CMD_TA_PERSISTENT_OBJ"},
	{CMD_TA_TRANSIENT_OBJ,           "CMD_TA_TRANSIENT_OBJ"},
	{CMD_TA_WRITE,                   "CMD_TA_WRITE"},
	{CMD_TA_READ,                    "CMD_TA_READ"},
	{CMD_TA_GET_SYS_TIME,            "CMD_TA_GET_SYS_TIME"},
	{CMD_TA_GET_REE_TIME,            "CMD_TA_GET_REE_TIME"},
	{CMD_TA_GET_SET_PERSIST_TIME,    "CMD_TA_GET_SET_PERSIST_TIME"},
	{CMD_TA_MEMORY_ALLOC,            "CMD_TA_MEMORY_ALLOC"},
	{CMD_TA_MEMORY_REALLOC,          "CMD_TA_MEMORY_REALLOC"},

	/* 10 */
	{CMD_TA_MEMORY_FILL,             "CMD_TA_MEMORY_FILL"},
	{CMD_TA_MEMORY_COMPARE,          "CMD_TA_MEMORY_COMPARE"},
	{CMD_TA_MEMORY_MOVE,             "CMD_TA_MEMORY_MOVE"},
	{CMD_TA_GEN_RND,                 "CMD_TA_GEN_RND"},
	{CMD_TA_CALCULATION_PERF,        "CMD_TA_CALCULATION_PERF"},
	{CMD_TA_CRYPTO_AES,              "CMD_TA_CRYPTO_AES"},
	{CMD_TA_CRYPTO_HASH,             "CMD_TA_CRYPTO_HASH"},
	{CMD_TA_CRYPTO_HMAC,             "CMD_TA_CRYPTO_HMAC"},
	{CMD_TA_CRYPTO_RSA,              "CMD_TA_CRYPTO_RSA"},
	{CMD_TA_CRYPTO_PBKDF2,           "CMD_TA_CRYPTO_PBKDF2"},

	/* 20 */
	{CMD_DRV_BASIC,                  "CMD_DRV_BASIC"},
	{CMD_DRV_MEMORY,                 "CMD_DRV_MEMORY"},
	{CMD_DRV_STRING,                 "CMD_DRV_STRING"},
	{CMD_DRV_TIME,                   "CMD_DRV_TIME"},

	{TEESANITY_CMD_TOTAL,            "TEESANITY_CMD_TOTAL"},
};


static TEEC_Result ta_init(struct TEEC_Context **context)
{
	TEEC_Result    nError;
	struct TEEC_Context  *pContext;

	pr_info("[%s]\n", __func__);

	*context = NULL;
	pContext = kmalloc(sizeof(struct TEEC_Context), GFP_KERNEL);
	if (pContext == NULL)
		return TEEC_ERROR_BAD_PARAMETERS;

	memset(pContext, 0, sizeof(struct TEEC_Context));

	/* Create Device context  */
	nError = TEEC_InitializeContext(NULL, pContext);

	if (nError != TEEC_SUCCESS) {
		pr_notice("%s: %d: TEEC_InitializeContext failed (0x%x)\n",
				__func__, __LINE__, nError);

		if (nError == TEEC_ERROR_COMMUNICATION)
			pr_notice("%s: %d: Error communication\n",
					__func__, __LINE__);
		kfree(pContext);
	} else
		*context = pContext;

	pr_info("%s finished\n", __func__);
	return nError;
}

static TEEC_Result ta_finalize(struct TEEC_Context *context)
{
	pr_info("[%s]\n", __func__);

	if (context == NULL) {
		pr_notice("%s: %d: Device handle invalid\n", __func__, __LINE__);
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	TEEC_FinalizeContext(context);

	kfree(context);
	return TEEC_SUCCESS;
}

static TEEC_Result ta_open_session(struct TEEC_Context *context, struct TEEC_Session **session)
{
	struct TEEC_Operation sOperation;
	TEEC_Result    nError;

	pr_info("[%s]\n", __func__);

	*session = kmalloc(sizeof(struct TEEC_Session), GFP_KERNEL);
	if (*session == NULL)
		return TEEC_ERROR_OUT_OF_MEMORY;

	memset(*session, 0, sizeof(struct TEEC_Session));
	memset(&sOperation, 0, sizeof(struct TEEC_Operation));
	sOperation.paramTypes = 0;

	nError = TEEC_OpenSession(context,
			*session,                   /* OUT session */
			&TEE_SANITY_CA_UUID_STRUCT, /* destination UUID */
			TEEC_LOGIN_PUBLIC,          /* connectionMethod */
			NULL,                       /* connectionData */
			&sOperation,                /* IN OUT operation */
			NULL                        /* OUT returnOrigin, optional */
		);

	if (nError != TEEC_SUCCESS) {
		pr_notice("%s: %d: TEEC_OpenSession failed (0x%x)\n",
				__func__, __LINE__, nError);
		kfree(*session);
		*session = NULL;
		return nError;
	}

	return TEEC_SUCCESS;
}

static TEEC_Result ta_close_session(struct TEEC_Session *session)
{
	pr_info("[%s]\n", __func__);

	if (session == NULL) {
		pr_notice("%s: %d: Invalid session handle\n",
				__func__, __LINE__);
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	TEEC_CloseSession(session);
	kfree(session);

	return TEEC_SUCCESS;
}

static TEEC_Result ta_open(void)
{
	TEEC_Result nError;

	pr_info("[%s]\n", __func__);

	nError = ta_init(&g_context);
	if (nError != TEEC_SUCCESS) {
		pr_notice("%s: %d: Initialize failed (0x%x)\n",
				__func__, __LINE__, nError);
		return nError;
	}

	/* Open a session */
	nError = ta_open_session(g_context, &g_session);
	if (nError != TEEC_SUCCESS) {
		pr_notice("%s: %d: OpenSession failed (%08x)\n",
				__func__, __LINE__, nError);
		return nError;
	}

	return nError;
}

TEEC_Result ta_close(void)
{
	TEEC_Result nError;

	pr_info("[%s]\n", __func__);

	/* Close the session */
	nError = ta_close_session(g_session);
	if (nError != TEEC_SUCCESS) {
		pr_notice("%s: %d: CloseSession failed (0x%x)\n",
				__func__, __LINE__, nError);
		return nError;
	}

	/* Finalize */
	nError = ta_finalize(g_context);
	if (nError != TEEC_SUCCESS) {
		pr_notice("%s: %d: Finalize failed (0x%x)\n",
				__func__, __LINE__, nError);
	}

	return nError;
}

TEEC_Result Alloc_SharedMem_UT(uint32_t mem_size)
{
	TEEC_Result nError;
	struct TEEC_SharedMemory buf_shm = {
		.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT,
		.buffer = NULL,
		.size = mem_size
	};

	pr_info("Test TEEC_AllocateSharedMemory GP API, mem_size: 0x%x\n", mem_size);
	nError = TEEC_AllocateSharedMemory(g_context, &buf_shm);
	if (nError != TEEC_SUCCESS) {
		pr_notice("%s: %d: TEEC_AllocateSharedMemory failed (0x%x)\n",
				__func__, __LINE__, nError);
		return nError;
	}

	if (!buf_shm.buffer) {
		pr_notice("%s: %d: TEEC_AllocateSharedMemory success but memory invlaid\n",
				__func__, __LINE__);
	} else {
		memset(buf_shm.buffer, 0, buf_shm.size);
	}
	TEEC_ReleaseSharedMemory(&buf_shm);
	return TEEC_SUCCESS;
}

TEEC_Result Register_SharedMem_UT(uint32_t mem_size)
{
	unsigned char *temp_buf = NULL;
	TEEC_Result nError;
	struct TEEC_SharedMemory buf_shm = {
		.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT,
		.buffer = NULL,
		.size = mem_size
	};

	pr_info("Test TEEC_RegisterSharedMemory() GP API, mem_size: 0x%x\n", mem_size);

	temp_buf = vmalloc(mem_size);
	if (!temp_buf) {
		nError = TEEC_ERROR_CANCEL;
		goto end;
	}

	buf_shm.buffer = temp_buf;
	nError = TEEC_RegisterSharedMemory(g_context, &buf_shm);
	if (nError != TEEC_SUCCESS) {
		pr_notice("%s: %d: TEEC_RegisterSharedMemory failed (0x%x)\n",
				__func__, __LINE__, nError);
	}

	TEEC_ReleaseSharedMemory(&buf_shm);

end:
	if (temp_buf)
		vfree(temp_buf);

	return nError;
}


TEEC_Result Register_SharedMem_IT(uint32_t index, uint32_t mem_size)
{
	uint8_t *prealloc_shared_mem;
	struct TEEC_SharedMemory share_memory;
	struct TEEC_Operation sOperation;
	TEEC_Result nError;
	struct test_result test_result = {0};
	int i;

	/* try mmap as MAP_SHARED */
	prealloc_shared_mem = vmalloc(mem_size * sizeof(uint8_t));
	pr_info("prealloc_shared_mem: 0x%p, size 0x%x\n", prealloc_shared_mem, mem_size);
	if (!prealloc_shared_mem)
		return 0xFFFFFFFF;

	memset(&share_memory, 0x0, sizeof(share_memory));
	share_memory.buffer = (void *)prealloc_shared_mem;
	share_memory.size = (size_t)mem_size;
	share_memory.flags = TEEC_MEM_INPUT | TEEC_MEM_OUTPUT;

	nError = TEEC_RegisterSharedMemory(g_context, &share_memory);
	if (nError != TEEC_SUCCESS) {
		vfree(prealloc_shared_mem);
		pr_notice("%s: %d: TEEC_RegisterSharedMemory() failed (0x%x)\n",
			__func__, __LINE__, nError);
		return nError;
	}

	/* Assign test pattern1 */
	for (i = 0; i < mem_size/2; i++)
		prealloc_shared_mem[i] = MEMFILL_TEST_PATTERN1;
	pr_info("[CA] assign pattern1 done\n");

	memset(&sOperation, 0, sizeof(struct TEEC_Operation));
	sOperation.paramTypes = TEEC_PARAM_TYPES(
			TEEC_VALUE_INPUT,		/* for test case index */
			TEEC_MEMREF_TEMP_OUTPUT,	/* for test result */
			TEEC_MEMREF_WHOLE,		/* for shared memory */
			TEEC_NONE);

	sOperation.params[0].value.a = index;
	sOperation.params[0].value.b = 0;
	sOperation.params[1].tmpref.buffer = (void *)&test_result;
	sOperation.params[1].tmpref.size = sizeof(struct test_result);
	sOperation.params[2].memref.parent = &share_memory;
	sOperation.params[2].memref.size = mem_size;

	pr_info("[CA] invokecommand start\n");
	nError = TEEC_InvokeCommand(g_session,
		CMD_START_TEST_CASE,
		&sOperation,       /* IN OUT operation */
		NULL               /* OUT returnOrigin, optional */
	);
	pr_info("[CA] invokecommand done\n");

	if (nError != TEEC_SUCCESS || test_result.result != 0) {
		pr_notice("%s: %d: TEEC_InvokeCommand failed (0x%x, 0x%x)\n",
				__func__, __LINE__, nError, test_result.result);
		goto err;
	}
	nError = 0xFFFFFFFE;

	/* Compare pattern3 for block 1 */
	for (i = 0; i < mem_size/2; i++) {
		if (prealloc_shared_mem[i] != MEMFILL_TEST_PATTERN3) {
			pr_notice("[CA] Shared memory compare failed, shared_mem[%d]:0x%x, pattern:0x%x\n",
					i, prealloc_shared_mem[i], MEMFILL_TEST_PATTERN3);
			goto err;
		}
	}

	/* compare pattern2 for block 2 */
	for (i = mem_size/2; i < mem_size; i++) {
		if (prealloc_shared_mem[i] != MEMFILL_TEST_PATTERN2) {
			pr_notice("[CA] Shared memory compare failed, shared_mem[%d]:0x%x, pattern:0x%x\n",
					i, prealloc_shared_mem[i], MEMFILL_TEST_PATTERN2);
			goto err;
		}
	}

	pr_info("[CA] compare test pattern done\n");
	TEEC_ReleaseSharedMemory(&share_memory);
	vfree(prealloc_shared_mem);

	return TEEC_SUCCESS;

err:
	/* dump shared memory contents */
	for (i = 0; i < mem_size; i += 8) {
		if (i == 31)
			break;

		if (i == 0)
			pr_info("shared_mem dump: {");
		else if (i % 16 == 0)
			pr_info(" ");

		pr_info("\t\t%02x%02x%02x%02x%02x%02x%02x%02x",
				prealloc_shared_mem[i],
				prealloc_shared_mem[i+1],
				prealloc_shared_mem[i+2],
				prealloc_shared_mem[i+3],
				prealloc_shared_mem[i+4],
				prealloc_shared_mem[i+5],
				prealloc_shared_mem[i+6],
				prealloc_shared_mem[i+7]
		     );
	}
	pr_info("};\n");

	TEEC_ReleaseSharedMemory(&share_memory);
	vfree(prealloc_shared_mem);

	return nError;
}

TEEC_Result ta_cmd_UT(uint32_t index, uint32_t args)
{
	struct TEEC_Operation sOperation;
	TEEC_Result nError;
	struct test_result test_result = {0};

	memset(&sOperation, 0, sizeof(struct TEEC_Operation));
	sOperation.paramTypes = TEEC_PARAM_TYPES(
		TEEC_VALUE_INPUT, /* for test case index */
		TEEC_MEMREF_TEMP_OUTPUT, /* for test result */
		TEEC_NONE,
		TEEC_NONE);

	sOperation.params[0].value.a = index;
	sOperation.params[0].value.b = args;
	sOperation.params[1].tmpref.buffer = (void *)&test_result;
	sOperation.params[1].tmpref.size = sizeof(struct test_result);

	nError = TEEC_InvokeCommand(g_session,
		CMD_START_TEST_CASE,
		&sOperation,       /* IN OUT operation */
		NULL               /* OUT returnOrigin, optional */
	);

	if (nError != TEEC_SUCCESS || test_result.result != 0) {
		pr_notice("%s: %d: TEEC_InvokeCommand failed (0x%x, 0x%x)\n",
				__func__, __LINE__, nError, test_result.result);
		return nError;
	}

	return TEEC_SUCCESS;
}

bool Case_Open_TA_Multi_Session(void)
{
	bool result = true;
	TEEC_Result ret;
	struct TEEC_Session *session[TA_INSTANCES_MAX] = {0};
	int i;

	/* open multiple times and check maximum instances */
	for (i = 0; i < TA_INSTANCES_MAX - 1; i++) {
		ret = ta_open_session(g_context, &session[i]);
		if (ret != TEEC_SUCCESS) {
			result = false;
			pr_notice("%s:%d open session[%d] fail %u\n",
				__func__, __LINE__, i, ret);
		}
	}

	ret = ta_open_session(g_context, &session[i]);
	if (ret == TEEC_SUCCESS) {
		result = false;
		pr_notice("%s:%d open session[%d] can not success, but success\n",
			__func__, __LINE__, i);
	}

	for (i = 0; i < TA_INSTANCES_MAX - 1; i++) {
		ret = ta_close_session(session[i]);
		if (ret != TEEC_SUCCESS) {
			result = false;
			pr_notice("%s:%d close session[%d] fail %u\n",
				__func__, __LINE__, i, ret);
		}
	}

	ret = ta_close_session(NULL);
	if (ret == TEEC_SUCCESS) {
		result = false;
		pr_notice("%s:%d close session NULL can not success, but success\n",
			__func__, __LINE__);
	}

	return result;
}

bool Case_Tee_Sanity(void)
{
	bool result = true;
	TEEC_Result ret;
	int i;

	for (i = 0; TeeSanityCmdToName[i].cmd != TEESANITY_CMD_TOTAL; i++) {
		pr_info("%s:%d Test cmd %s ...\n",
			__func__, __LINE__, TeeSanityCmdToName[i].name);
		ret = ta_cmd_UT(TeeSanityCmdToName[i].cmd, 0);
		if (ret != TEEC_SUCCESS) {
			result = false;
			pr_notice("%s:%d cmd %s fail %u\n",
				__func__, __LINE__,
				TeeSanityCmdToName[i].name, ret);
		}
	}

	return result;
}

bool Case_Allocate_Shared_Memory_UT(void)
{
	bool result = true;
	TEEC_Result ret;
	int i;

	for (i = 0; alloc_test_params[i].size != SIZE_U32_MAX; i++) {
		pr_info("%s:%d Test size %u\n",
			__func__, __LINE__, alloc_test_params[i].size);
		ret = Alloc_SharedMem_UT(alloc_test_params[i].size);
		if (ret !=  TEEC_SUCCESS) {
			result = false;
			pr_notice("%s:%d size %u fail %u\n",
				__func__, __LINE__,
				alloc_test_params[i].size, ret);
		}
	}

	return result;
}

bool Case_Register_Shared_Memory_UT(void)
{
	bool result = true;
	TEEC_Result ret;
	int i;

	for (i = 0; alloc_test_params[i].size != SIZE_U32_MAX; i++) {
		pr_info("%s:%d Test size %u\n",
			__func__, __LINE__, alloc_test_params[i].size);
		ret = Register_SharedMem_UT(alloc_test_params[i].size);
		if (ret !=  TEEC_SUCCESS) {
			result = false;
			pr_notice("%s:%d size %u fail %u\n",
				__func__, __LINE__,
				alloc_test_params[i].size, ret);
		}
	}

	return result;
}

bool Case_Register_SharedMem_IT(void)
{
	bool result = true;
	TEEC_Result ret;
	int i;

	for (i = 0; alloc_test_params[i].size != SIZE_U32_MAX; i++) {
		pr_info("%s:%d Test size %u\n",
			__func__, __LINE__, alloc_test_params[i].size);
		ret = Register_SharedMem_IT(
			CMD_REGISTER_SHARED_MEMORY,
			alloc_test_params[i].size);
		if (ret !=  TEEC_SUCCESS) {
			result = false;
			pr_notice("%s:%d size %u fail %u\n",
				__func__, __LINE__,
				alloc_test_params[i].size, ret);
		}
	}

	return result;
}

void gptests_entry(const char *cmd)
{
	int i;

	ta_open();

	for (i = 0; cmd[i] != '\0' && cmd[i] != '\x0a'; i++) {
		switch (cmd[i]) {
		case '1':
			Case_Open_TA_Multi_Session();
			break;
		case '2':
			Case_Tee_Sanity();
			break;
		case '3':
			Case_Allocate_Shared_Memory_UT();
			break;
		case '4':
			Case_Register_Shared_Memory_UT();
			break;
		case '5':
			Case_Register_SharedMem_IT();
			break;
		default:
			pr_notice("%s:%d unknown test cmd 0x%x\n", __func__, __LINE__, cmd[i]);
		}
	}

	ta_close();
}


int gptests_open(
	__always_unused struct inode *inode,
	__always_unused struct file *file)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	return 0;
}

int gptests_release(
	__always_unused struct inode *ino,
	__always_unused struct file *file)
{
	pr_info("%s:%d\n", __func__, __LINE__);
	return 0;
}

ssize_t gptests_write(
	struct file *file,
	const char __user *buffer,
	size_t count,
	loff_t *data)
{
	char cmd[128] = {0};

	if (count > 127)
		count = 127;
	if (copy_from_user(cmd, buffer, count))
		return 0;
	pr_info("get command: %s\n", cmd);
	gptests_entry(cmd);
	return count;
}

static const struct proc_ops gptests_fops = {
	.proc_open = gptests_open,
	.proc_release = gptests_release,
	.proc_ioctl = NULL,
	.proc_write = gptests_write,
};


static struct proc_dir_entry *g_ent;

static int __init mobicore_init(void)
{
	g_ent = proc_create("gptests", 0664, NULL, &gptests_fops);
	if (!g_ent) {
		pr_info("GPAPI tests module init fail\n");
		return -EIO;
	}

	pr_info("GPAPI tests module inited\n");
	return 0;
}

static void __exit mobicore_exit(void)
{
	if (g_ent)
		proc_remove(g_ent);
	pr_info("GPAPI tests module bye\n");
}
module_init(mobicore_init);
module_exit(mobicore_exit);


MODULE_AUTHOR("MediaTek Inc.");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("GPAPI tests mobule");
