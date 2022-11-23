// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2019 MediaTek Inc.
 */
#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/module.h>
#include "tee_client_api.h"
#include "tee_impl_api.h"

/**
 * the boundary check value to recognize struct out-of-bounds write
 */
static const uint32_t BOUNDARY = 0xb0daad0bu;

/**
 * varailbes to records TEE implementations.
 */
static DEFINE_MUTEX(g_pool_mutex);
#define TEE_IMPL_MAX_SIZE 16u
static const struct gp_api_impl_info *g_tee_impl_pool[TEE_IMPL_MAX_SIZE] = {};


static bool check_impl_info(const struct gp_api_impl_info *info)
{
	bool ret = true;

	if (!info || !info->name || !info->name[0]) {
		pr_notice("ERROR: TEE impl name is empty\n");
		return false;
	}

	if (info->size.sharedmemory_max < TEEC_CONFIG_SHAREDMEM_MAX_SIZE) {
		pr_notice("WARN: TEEC_CONFIG_SHAREDMEM_MAX_SIZE greater than %zx of \"%s\", need to change\n",
			info->size.sharedmemory_max,
			info->name);
	}
	if (info->size.context > offsetof(struct TEEC_Context, boundary)) {
		pr_notice("ERROR: TEEC_Context size less than %zu of \"%s\", need to increase size of reserved\n",
			info->size.context,
			info->name);
		ret = false;
	}
	if (info->size.session > offsetof(struct TEEC_Session, boundary)) {
		pr_notice("ERROR: TEEC_Session size less than %zu of \"%s\", need to increase size of reserved\n",
			info->size.session,
			info->name);
		ret = false;
	}
	if (info->size.sharedmemory > offsetof(struct TEEC_SharedMemory, boundary)) {
		pr_notice("ERROR: TEEC_SharedMemory size less than %zu of \"%s\", need to increase size of reserved\n",
			info->size.sharedmemory,
			info->name);
		ret = false;
	}
	if (info->size.operation > offsetof(struct TEEC_Operation, boundary)) {
		pr_notice("ERROR: TEEC_Operation size less than %zu of \"%s\", need to increase size of reserved\n",
			info->size.operation,
			info->name);
		ret = false;
	}

	if (!info->ops.initializecontext) {
		pr_notice("ERROR: TEEC_InitializeContext of \"%s\" is mssing\n",
			info->name);
		ret = false;
	}
	if (!info->ops.finalizecontext) {
		pr_notice("ERROR: TEEC_FinalizeContext of \"%s\" is mssing\n",
			info->name);
		ret = false;
	}
	if (!info->ops.registersharedmemory) {
		pr_notice("TEEC_RegisterSharedMemory of \"%s\" is mssing\n",
			info->name);
		ret = false;
	}
	if (!info->ops.allocatesharedmemory) {
		pr_notice("ERROR: TEEC_AllocateSharedMemory of \"%s\" is mssing\n",
			info->name);
		ret = false;
	}
	if (!info->ops.releasesharedmemory) {
		pr_notice("ERROR: TEEC_ReleaseSharedMemory of \"%s\" is mssing\n",
			info->name);
		ret = false;
	}
	if (!info->ops.opensession) {
		pr_notice("ERROR: TEEC_OpenSession of \"%s\" is mssing\n",
			info->name);
		ret = false;
	}
	if (!info->ops.closesession) {
		pr_notice("ERROR: TEEC_CloseSession of \"%s\" is mssing\n",
			info->name);
		ret = false;
	}
	if (!info->ops.invokecommand) {
		pr_notice("ERROR: TEEC_InvokeCommand of \"%s\" is mssing\n",
			info->name);
		ret = false;
	}
	if (!info->ops.requestcancellation) {
		pr_notice("ERROR: TEEC_RequestCancellation of \"%s\" is mssing\n",
			info->name);
		ret = false;
	}

	return ret;
}

bool gp_api_impl_add(const struct gp_api_impl_info *info)
{
	size_t idx;

	if (!check_impl_info(info))
		return false;

	mutex_lock(&g_pool_mutex);
	for (idx = 0; idx < TEE_IMPL_MAX_SIZE; idx++) {
		if (!g_tee_impl_pool[idx]) {
			g_tee_impl_pool[idx] = info;
			mutex_unlock(&g_pool_mutex);
			pr_info("add TEE impl[%zu] \"%s\" successfully\n", idx, info->name);
			return true;
		}
	}
	mutex_unlock(&g_pool_mutex);

	pr_notice("ERROR: TEE impl count have exceeded %zu\n",
			TEE_IMPL_MAX_SIZE);
	return false;
}
EXPORT_SYMBOL(gp_api_impl_add);

TEEC_Result __nocfi TEEC_InitializeContext(
	const char   *name,
	struct TEEC_Context *context)
{
	size_t idx;
	TEEC_Result ret;
	const struct gp_api_impl_info *impl = NULL;
	typeof(&TEEC_InitializeContext) func = NULL;

	if (!context) {
		pr_notice("ERROR: param context is NULL\n");
		return TEEC_ERROR_BAD_PARAMETERS;
	}

	mutex_lock(&g_pool_mutex);
	for (idx = 0; idx < TEE_IMPL_MAX_SIZE; idx++) {
		if (g_tee_impl_pool[idx] && (!name || !name[0] ||
			strstr(g_tee_impl_pool[idx]->name, name))) {
			impl = g_tee_impl_pool[idx];
			break;
		}
	}
	mutex_unlock(&g_pool_mutex);
	if (!impl) {
		pr_notice("ERROR: Can not get valid TEE impl \"%s\"\n", name ? name : "<any>");
		return TEEC_ERROR_COMMUNICATION;
	}
	pr_info("TEEC_InitializeContext select TEE impl \"%s\"\n", impl->name);
	func = (typeof(&TEEC_InitializeContext))impl->ops.initializecontext;

	context->boundary = BOUNDARY;
	context->priv = NULL;
	ret = func(name, context);
	if (WARN_ON(context->boundary != BOUNDARY)) {
		pr_notice("ERROR: context data may be destroyed by TEEC_InitializeContext\n");
		return TEEC_ERROR_GENERIC;
	}
	if (ret == TEEC_SUCCESS)
		context->priv = (const void *)impl;
	return ret;
}
EXPORT_SYMBOL(TEEC_InitializeContext);

void __nocfi TEEC_FinalizeContext(struct TEEC_Context *context)
{
	const struct gp_api_impl_info *impl = NULL;
	typeof(&TEEC_FinalizeContext) func = NULL;

	if (!context || context->boundary != BOUNDARY || !context->priv)
		return;
	impl = (const struct gp_api_impl_info *)context->priv;
	func = (typeof(&TEEC_FinalizeContext))impl->ops.finalizecontext;

	func(context);
	context->priv = NULL;
}
EXPORT_SYMBOL(TEEC_FinalizeContext);

TEEC_Result __nocfi TEEC_RegisterSharedMemory(
	struct TEEC_Context      *context,
	struct TEEC_SharedMemory *sharedMem)
{
	TEEC_Result ret;
	const struct gp_api_impl_info *impl = NULL;
	typeof(&TEEC_RegisterSharedMemory) func = NULL;

	if (!context || context->boundary != BOUNDARY || !context->priv)
		return TEEC_ERROR_BAD_PARAMETERS;
	if (!sharedMem)
		return TEEC_ERROR_BAD_PARAMETERS;
	impl = (const struct gp_api_impl_info *)context->priv;
	func = (typeof(&TEEC_RegisterSharedMemory))impl->ops.registersharedmemory;

	sharedMem->boundary = BOUNDARY;
	sharedMem->priv = NULL;
	ret = func(context, sharedMem);
	if (WARN_ON(context->boundary != BOUNDARY)) {
		pr_notice("ERROR: context data may be destroyed by TEEC_RegisterSharedMemory\n");
		return TEEC_ERROR_GENERIC;
	}
	if (WARN_ON(sharedMem->boundary != BOUNDARY)) {
		pr_notice("ERROR: sharedMem data may be destroyed by TEEC_RegisterSharedMemory\n");
		return TEEC_ERROR_GENERIC;
	}
	if (ret == TEEC_SUCCESS)
		sharedMem->priv = context->priv;
	return ret;
}
EXPORT_SYMBOL(TEEC_RegisterSharedMemory);

TEEC_Result __nocfi TEEC_AllocateSharedMemory(
	struct TEEC_Context      *context,
	struct TEEC_SharedMemory *sharedMem)
{
	TEEC_Result ret;
	const struct gp_api_impl_info *impl = NULL;
	typeof(&TEEC_AllocateSharedMemory) func = NULL;

	if (!context || context->boundary != BOUNDARY || !context->priv)
		return TEEC_ERROR_BAD_PARAMETERS;
	if (!sharedMem)
		return TEEC_ERROR_BAD_PARAMETERS;
	impl = (const struct gp_api_impl_info *)context->priv;
	func = (typeof(&TEEC_AllocateSharedMemory))impl->ops.allocatesharedmemory;

	sharedMem->boundary = BOUNDARY;
	sharedMem->priv = NULL;
	ret = func(context, sharedMem);
	if (WARN_ON(context->boundary != BOUNDARY)) {
		pr_notice("ERROR: sharedMem data may be destroyed by TEEC_AllocateSharedMemory\n");
		return TEEC_ERROR_GENERIC;
	}
	if (WARN_ON(sharedMem->boundary != BOUNDARY)) {
		pr_notice("ERROR: sharedMem data may be destroyed by TEEC_AllocateSharedMemory\n");
		return TEEC_ERROR_GENERIC;
	}
	if (ret == TEEC_SUCCESS)
		sharedMem->priv = context->priv;
	return ret;
}
EXPORT_SYMBOL(TEEC_AllocateSharedMemory);

void __nocfi TEEC_ReleaseSharedMemory(struct TEEC_SharedMemory *sharedMem)
{
	const struct gp_api_impl_info *impl = NULL;
	typeof(&TEEC_ReleaseSharedMemory) func = NULL;

	if (!sharedMem || sharedMem->boundary != BOUNDARY || !sharedMem->priv)
		return;
	impl = (const struct gp_api_impl_info *)sharedMem->priv;
	func = (typeof(&TEEC_ReleaseSharedMemory))impl->ops.releasesharedmemory;

	func(sharedMem);
	sharedMem->boundary = 0;
	sharedMem->priv = NULL;
}
EXPORT_SYMBOL(TEEC_ReleaseSharedMemory);

TEEC_Result __nocfi TEEC_OpenSession(
	struct TEEC_Context    *context,
	struct TEEC_Session    *session,
	const struct TEEC_UUID *destination,
	uint32_t               connectionMethod,
	const void             *connectionData,
	struct TEEC_Operation  *operation,
	uint32_t               *returnOrigin)
{
	TEEC_Result ret;
	const struct gp_api_impl_info *impl = NULL;
	typeof(&TEEC_OpenSession) func = NULL;

	if (!context || context->boundary != BOUNDARY || !context->priv)
		return TEEC_ERROR_BAD_PARAMETERS;
	if (!session || !destination)
		return TEEC_ERROR_BAD_PARAMETERS;
	impl = (const struct gp_api_impl_info *)context->priv;
	func = (typeof(&TEEC_OpenSession))impl->ops.opensession;

	session->boundary = BOUNDARY;
	session->priv = NULL;
	if (operation) {
		operation->boundary = BOUNDARY;
		operation->priv = NULL;
	}
	ret = func(context, session, destination,
		connectionMethod, connectionData,
		operation, returnOrigin);
	if (WARN_ON(context->boundary != BOUNDARY)) {
		pr_notice("ERROR: context data may be destroyed by TEEC_OpenSession\n");
		return TEEC_ERROR_GENERIC;
	}
	if (WARN_ON(session->boundary != BOUNDARY)) {
		pr_notice("ERROR: sharedMem data may be destroyed by TEEC_OpenSession\n");
		return TEEC_ERROR_GENERIC;
	}
	if (operation) {
		if (WARN_ON(operation->boundary != BOUNDARY)) {
			pr_notice("ERROR: operation data may be destroyed by TEEC_OpenSession\n");
			return TEEC_ERROR_GENERIC;
		}
	}
	if (ret == TEEC_SUCCESS) {
		session->priv = context->priv;
		if (operation)
			operation->priv = context->priv;
	}
	return ret;
}
EXPORT_SYMBOL(TEEC_OpenSession);

void __nocfi TEEC_CloseSession(
	struct TEEC_Session *session)
{
	const struct gp_api_impl_info *impl = NULL;
	typeof(&TEEC_CloseSession) func = NULL;

	if (!session || session->boundary != BOUNDARY || !session->priv)
		return;
	impl = (const struct gp_api_impl_info *)session->priv;
	func = (typeof(&TEEC_CloseSession))impl->ops.closesession;

	func(session);
	session->boundary = 0;
	session->priv = NULL;
}
EXPORT_SYMBOL(TEEC_CloseSession);

TEEC_Result __nocfi TEEC_InvokeCommand(
	struct TEEC_Session  *session,
	uint32_t              commandID,
	struct TEEC_Operation *operation,
	uint32_t              *returnOrigin)
{
	TEEC_Result ret;
	const struct gp_api_impl_info *impl = NULL;
	typeof(&TEEC_InvokeCommand) func = NULL;

	if (!session || session->boundary != BOUNDARY || !session->priv)
		return TEEC_ERROR_BAD_PARAMETERS;
	impl = (const struct gp_api_impl_info *)session->priv;
	func = (typeof(&TEEC_InvokeCommand))impl->ops.invokecommand;

	if (operation) {
		operation->boundary = BOUNDARY;
		operation->priv = NULL;
	}
	ret = func(session, commandID, operation, returnOrigin);
	if (WARN_ON(session->boundary != BOUNDARY)) {
		pr_notice("ERROR: session data may be destroyed by TEEC_InvokeCommand\n");
		return TEEC_ERROR_GENERIC;
	}
	if (operation) {
		if (WARN_ON(operation->boundary != BOUNDARY)) {
			pr_notice("ERROR: operation data may be destroyed by TEEC_InvokeCommand\n");
			return TEEC_ERROR_GENERIC;
		}
	}
	if (ret == TEEC_SUCCESS && operation)
		operation->priv = session->priv;
	return ret;
}
EXPORT_SYMBOL(TEEC_InvokeCommand);

void __nocfi TEEC_RequestCancellation(
	struct TEEC_Operation *operation)
{
	const struct gp_api_impl_info *impl = NULL;
	typeof(&TEEC_RequestCancellation) func = NULL;

	if (!operation || operation->boundary != BOUNDARY || !operation->priv)
		return;
	impl = (const struct gp_api_impl_info *)operation->priv;
	func = (typeof(&TEEC_RequestCancellation))impl->ops.requestcancellation;

	func(operation);
	operation->boundary = 0;
	operation->priv = NULL;
}
EXPORT_SYMBOL(TEEC_RequestCancellation);


static int __init mobicore_init(void)
{
	pr_info("GPAPI coordinator driver init\n");
	return 0;
}
module_init(mobicore_init);

MODULE_AUTHOR("MediaTek Inc.");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("GPAPI Coordinator Driver");
