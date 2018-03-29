/*************************************************************************/ /*!
@File           km_apphint.c
@Title          Apphint routines
@Copyright      Copyright (c) Imagination Technologies Ltd. All Rights Reserved
@Description    Device specific functions
@License        Dual MIT/GPLv2

The contents of this file are subject to the MIT license as set out below.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

Alternatively, the contents of this file may be used under the terms of
the GNU General Public License Version 2 ("GPL") in which case the provisions
of GPL are applicable instead of those above.

If you wish to allow use of your version of this file only under the terms of
GPL, and not to allow others to use your version of this file under the terms
of the MIT license, indicate your decision by deleting the provisions above
and replace them with the notice and other provisions required by GPL as set
out in the file called "GPL-COPYING" included in this distribution. If you do
not delete the provisions above, a recipient may use your version of this file
under the terms of either the MIT license or GPL.

This License is also included in this distribution in the file called
"MIT-COPYING".

EXCEPT AS OTHERWISE STATED IN A NEGOTIATED AGREEMENT: (A) THE SOFTWARE IS
PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING
BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
PURPOSE AND NONINFRINGEMENT; AND (B) IN NO EVENT SHALL THE AUTHORS OR
COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/ /**************************************************************************/

#if defined(SUPPORT_KERNEL_SRVINIT)

#include "pvr_debugfs.h"
#include "pvr_uaccess.h"
#include <linux/moduleparam.h>
#include <linux/workqueue.h>
#include <linux/string.h>

/* for action device access */
#include "pvrsrv.h"
#include "device.h"
#include "rgxdevice.h"
#include "rgxfwutils.h"
#include "debugmisc_server.h"
#include "htbserver.h"
#include "rgxutils.h"
#include "rgxapi_km.h"

#include "img_defs.h"

/* defines for default values */
#include "rgx_fwif.h"
#include "htbuffer_types.h"

#include "pvr_notifier.h"

#include "km_apphint_defs.h"
#include "km_apphint.h"

#if defined(PDUMP)
#include <stdarg.h>
#include "pdump_km.h"
#endif

/* Size of temporary buffers used to read and write AppHint data.
 * Must be large enough to contain any strings read/written
 * but no larger than 4096 with is the buffer size for the
 * kernel_param_ops .get function.
 * And less than 1024 to keep the stack frame size within bounds.
 */
#define APPHINT_BUFFER_SIZE 512

/*
*******************************************************************************
 * AppHint mnemonic data type helper tables
******************************************************************************/
struct apphint_lookup {
	char *name;
	int value;
};

struct apphint_map {
	int id;
	unsigned flag;
};

static const struct apphint_map apphint_flag_map[] = {
	{ APPHINT_ID_AssertOnHWRTrigger, RGXFWIF_INICFG_ASSERT_ON_HWR_TRIGGER },
	{ APPHINT_ID_AssertOutOfMemory,  RGXFWIF_INICFG_ASSERT_ON_OUTOFMEMORY },
	{ APPHINT_ID_CheckMList,         RGXFWIF_INICFG_CHECK_MLIST_EN },
	{ APPHINT_ID_EnableHWR,          RGXFWIF_INICFG_HWR_EN },
	{ APPHINT_ID_DisableFEDLogging,  RGXKMIF_DEVICE_STATE_DISABLE_DW_LOGGING_EN },
	{ APPHINT_ID_ZeroFreelist,       RGXKMIF_DEVICE_STATE_ZERO_FREELIST },
	{ APPHINT_ID_DustRequestInject,  RGXKMIF_DEVICE_STATE_DUST_REQUEST_INJECT_EN },
	{ APPHINT_ID_DisableClockGating, RGXFWIF_INICFG_DISABLE_CLKGATING_EN },
	{ APPHINT_ID_DisableDMOverlap,   RGXFWIF_INICFG_DISABLE_DM_OVERLAP },
};

static const struct apphint_lookup fwt_logtype_tbl[] = {
	{ "trace", 2},
	{ "tbi", 1},
	{ "none", 0}
};

static const struct apphint_lookup fwt_loggroup_tbl[] = {
	RGXFWIF_LOG_GROUP_NAME_VALUE_MAP
};

static const struct apphint_lookup htb_loggroup_tbl[] = {
#define X(a, b) { #b, HTB_LOG_GROUP_FLAG(a) },
	HTB_LOG_SFGROUPLIST
#undef X
};

static const struct apphint_lookup htb_opmode_tbl[] = {
	{ "droplatest", HTB_OPMODE_DROPLATEST},
	{ "dropoldest", HTB_OPMODE_DROPOLDEST},
	{ "block", HTB_OPMODE_BLOCK}
};

/*
*******************************************************************************
 Data types
******************************************************************************/
union apphint_value {
	IMG_UINT64 UINT64;
	IMG_UINT32 UINT32;
	IMG_BOOL BOOL;
};

struct apphint_param {
	APPHINT_DATA_TYPE data_type;
	APPHINT_ACTION  action_type;
	const void *data_type_helper;
	IMG_UINT32 helper_size;
};

struct apphint_init_data {
	IMG_UINT32 id;			/* index into AppHint Table */
	APPHINT_CLASS class;
	u16 perm;
	IMG_CHAR *name;
	union apphint_value default_value;
};

struct apphint_class_state {
	APPHINT_CLASS class;
	IMG_BOOL enabled;
};

struct apphint_work {
	struct work_struct work;
	APPHINT_ID id;
	union apphint_value value;
};

/*
*******************************************************************************
 Initialization / configuration table data
******************************************************************************/
static const struct apphint_init_data init_data_buildvar[] = {
#define X(a, b, c, d, e, f, g) \
	{APPHINT_ID_ ## a, APPHINT_CLASS_ ## e, c, #a, {f} } ,
	APPHINT_LIST_BUILDVAR
#undef X
};

static const struct apphint_init_data init_data_modparam[] = {
#define X(a, b, c, d, e, f, g) \
	{APPHINT_ID_ ## a, APPHINT_CLASS_ ## e, c, #a, {f} } ,
	APPHINT_LIST_MODPARAM
#undef X
};

static const struct apphint_init_data init_data_debugfs[] = {
#define X(a, b, c, d, e, f, g) \
	{APPHINT_ID_ ## a, APPHINT_CLASS_ ## e, c, #a, {f} } ,
	APPHINT_LIST_DEBUGFS
#undef X
};

/* Don't use the kernel ARRAY_SIZE macro here because it checks
 * __must_be_array() and we need to be able to use this safely on a NULL ptr.
 * This will return an undefined size for a NULL ptr - so should only be
 * used here.
 */
#define APPHINT_HELP_ARRAY_SIZE(a) (sizeof((a))/(sizeof((a[0]))))

static const struct apphint_param param_lookup[] = {
#define X(a, b, c, d, e, f, g) \
	{APPHINT_DATA_TYPE_ ## b, APPHINT_ACTION_ ## d, g, APPHINT_HELP_ARRAY_SIZE(g) } ,
	APPHINT_LIST_ALL
#undef X
};

#undef APPHINT_HELP_ARRAY_SIZE

static const struct apphint_class_state class_state[] = {
#define X(a) {APPHINT_CLASS_ ## a, APPHINT_ENABLED_CLASS_ ## a} ,
	APPHINT_CLASS_LIST
#undef X
};

/*
*******************************************************************************
 Global state
******************************************************************************/
/* If the union apphint_value becomes such that it is not possible to read
 * and write atomically, a mutex may be desirable to prevent a read returning
 * a partially written state.
 * This would require a statically initialized mutex outside of the
 * struct apphint_state to prevent use of an uninitialized mutex when
 * module_params are provided on the command line.
 *     static DEFINE_MUTEX(apphint_mutex);
 */
static struct apphint_state
{
	struct workqueue_struct *workqueue;
	PVR_DEBUGFS_DIR_DATA *debugfs_rootdir;
	PVR_DEBUGFS_ENTRY_DATA *debugfs_entry[APPHINT_DEBUGFS_ID_MAX];
	PVR_DEBUGFS_DIR_DATA *buildvar_rootdir;
	PVR_DEBUGFS_ENTRY_DATA *buildvar_entry[APPHINT_BUILDVAR_ID_MAX];
	int initialized;
	union apphint_value val[APPHINT_ID_MAX];
} apphint = {
/* statically initialise default values to ensure that any module_params
 * provided on the command line are not overwritten by defaults.
 */
	.val = {
#define X(a, b, c, d, e, f, g) \
	{ f },
	APPHINT_LIST_ALL
#undef X
	},
	.initialized = 0
};

/*
*******************************************************************************
 Generic modparam/debugfs AppHint write, read and action functions

   .get / .show --> AppHintWrite
   .set / .?    --> AppHintRead

******************************************************************************/
/**
 * action_fwt_param - Update the Firmware trace configuration after an AppHint
 *                       update has been requested by a UM process
 */
static int action_fwt_param(APPHINT_ID id, union apphint_value value,
			PVRSRV_DATA *driver_data)
{
	PVRSRV_ERROR eError;
	int result = 0;
	PVRSRV_DEVICE_NODE *dev_node = NULL;
	unsigned fwt_log_type = 0;

	switch (id) {
	case APPHINT_ID_EnableLogGroup:
		/* force FirmwareLogType to trace (2) if not enabled */
		/* leave alone if set to TBI (1) */
		fwt_log_type = value.UINT32;
		if (value.UINT32 &&
			1 != apphint.val[APPHINT_ID_FirmwareLogType].UINT32) {
			fwt_log_type |= RGXFWIF_LOG_TYPE_TRACE;
			apphint.val[APPHINT_ID_FirmwareLogType].UINT32 = 2;
		}
		break;
	case APPHINT_ID_FirmwareLogType:
		/* force MAIN log group if nothing enabled */
		if (value.UINT32) {
			fwt_log_type
				= apphint.val[APPHINT_ID_EnableLogGroup].UINT32;
			if (!fwt_log_type)
				fwt_log_type = RGXFWIF_LOG_TYPE_GROUP_MAIN;
			if (2 == value.UINT32)
				fwt_log_type |= RGXFWIF_LOG_TYPE_TRACE;
		} else {
			fwt_log_type = 0;
		}

		break;
	default:
		result = -EINVAL;
	}

	/* set for all devices */
	dev_node = driver_data->psDeviceNodeList;
	while (dev_node && 0 == result) {
		eError = PVRSRVRGXDebugMiscSetFWLogKM(NULL, dev_node,
				fwt_log_type);
		if (PVRSRV_OK != eError)
			result = -EACCES;
		dev_node = dev_node->psNext;
	}

	/* if EnableLogGroup was cleared, set log type to 'none'
	 * or leave if FirmwareLogType is set to TBI
	 */
	if (APPHINT_ID_EnableLogGroup == id && 0 == result && !fwt_log_type
			&& 1 != apphint.val[APPHINT_ID_FirmwareLogType].UINT32)
		apphint.val[APPHINT_ID_FirmwareLogType].UINT32 = 0;
	/* if FirmwareLogType changed, set log groups to match */
	if (APPHINT_ID_FirmwareLogType == id && 0 == result) {
		apphint.val[APPHINT_ID_EnableLogGroup].UINT32
			= (fwt_log_type & RGXFWIF_LOG_TYPE_GROUP_MASK);
	}

	return result;
}

/**
 * action_htb_param - Update an HTB parameter after an AppHint update has
 *                       been requested by a UM process
 */
static int action_htb_param(APPHINT_ID id, union apphint_value value)
{
	PVRSRV_ERROR eError;
	int result = 0;
	IMG_UINT32 ui32NumFlagGroups = 0;
	IMG_UINT32 *pui32GroupEnable = NULL;
	IMG_UINT32 ui32LogLevel = 0;
	IMG_UINT32 ui32EnablePID = 0;
	HTB_LOGMODE_CTRL eLogMode = HTB_LOGMODE_UNDEF;
	HTB_OPMODE_CTRL eOpMode = HTB_OPMODE_UNDEF;

	switch (id) {
	case APPHINT_ID_EnableHTBLogGroup:
		ui32NumFlagGroups = 1;
		pui32GroupEnable = &value.UINT32;
		break;
	case APPHINT_ID_HTBOperationMode:
		eOpMode = (HTB_OPMODE_CTRL)value.UINT32;
		break;
	default:
		result = -EINVAL;
	}

	if (0 == result) {
		eError = HTBControlKM(ui32NumFlagGroups, pui32GroupEnable,
				ui32LogLevel, ui32EnablePID, eLogMode, eOpMode);
		result = (PVRSRV_OK == eError) ? 0 : -EPERM;
	}

	return result;
}

/**
 * action_device_flag - Update a device flag after an AppHint update has
 *                       been requested by a UM process
 */
static int action_device_flag(APPHINT_ID id, union apphint_value value,
	PVRSRV_DATA *driver_data,
	PVRSRV_ERROR (*set_flags)(PVRSRV_RGXDEV_INFO*, IMG_UINT32, IMG_BOOL))
{
	int i;
	int result = 0;
	unsigned flag = 0;
	PVRSRV_ERROR eError;

	PVRSRV_DEVICE_NODE *dev_node = NULL;
	PVRSRV_RGXDEV_INFO *dev_info = NULL;

	int size = param_lookup[id].helper_size;
	struct apphint_map *map =
		(struct apphint_map *)param_lookup[id].data_type_helper;

	if (APPHINT_DATA_TYPE_FLAG != param_lookup[id].data_type) {
		result = -EINVAL;
		goto err_exit;
	}

	if (!map) {
		result = -EINVAL;
		goto err_exit;
	}

	/* EnableHWR is a special case, can only disable */
	if (APPHINT_ID_EnableHWR == id && IMG_FALSE != value.BOOL) {
		result = -EPERM;
		goto err_exit;
	}

	for (i = 0; i < size; i++) {
		if (map[i].id == id) {
			flag = map[i].flag;
			break;
		}
	}
	if (i == size) {
		result = -EINVAL;
		goto err_exit;
	}

	/* set for all devices */
	dev_node = driver_data->psDeviceNodeList;
	while (dev_node) {
		dev_info = (PVRSRV_RGXDEV_INFO *)dev_node->pvDevice;
		eError = set_flags(dev_info, flag, value.BOOL);
		if (PVRSRV_OK != eError)
			result = -EACCES;
		dev_node = dev_node->psNext;
	}

err_exit:
	return result;
}

/**
 * action_device_bool - Update a device boolean after an AppHint update has
 *                       been requested by a UM process
 */
static int action_device_bool(APPHINT_ID id, union apphint_value value,
	PVRSRV_DATA *driver_data,
	PVRSRV_ERROR (*set_bool)(PVRSRV_RGXDEV_INFO*, IMG_BOOL))
{
	int result = 0;
	PVRSRV_ERROR eError;

	PVRSRV_DEVICE_NODE *dev_node = NULL;
	PVRSRV_RGXDEV_INFO *dev_info = NULL;

	if (APPHINT_DATA_TYPE_BOOL != param_lookup[id].data_type) {
		result = -EINVAL;
		goto err_exit;
	}

	/* set for all devices */
	dev_node = driver_data->psDeviceNodeList;
	while (dev_node) {
		dev_info = (PVRSRV_RGXDEV_INFO *)dev_node->pvDevice;
		eError = set_bool(dev_info, value.BOOL);
		if (PVRSRV_OK != eError)
			result = -EACCES;
		dev_node = dev_node->psNext;
	}

err_exit:
	return result;
}

/**
 * action_apm_off - force APM off if requested by a UM process
 */
static int action_apm_off(APPHINT_ID id, union apphint_value value,
			PVRSRV_DATA *driver_data)
{
	int result = 0;
	PVRSRV_ERROR eError;

	PVRSRV_DEVICE_NODE *dev_node = NULL;

	if (APPHINT_DATA_TYPE_UINT32 != param_lookup[id].data_type) {
		result = -EINVAL;
		goto err_exit;
	}

	if (APPHINT_ID_EnableAPM != id) {
		result = -EINVAL;
		goto err_exit;
	}

	if (RGX_ACTIVEPM_FORCE_OFF != value.UINT32) {
		result = -EPERM;
		goto err_exit;
	}

	/* set for all devices */
	dev_node = driver_data->psDeviceNodeList;
	while (dev_node) {
		eError = RGXSetAPMStateOff(dev_node);
		if (PVRSRV_OK != eError)
			result = -EACCES;
		dev_node = dev_node->psNext;
	}

err_exit:
	return result;
}

static int action_hwp_filter(APPHINT_ID id, union apphint_value value)
{
	int result = 0;
	PVRSRV_ERROR eError;
	IMG_HANDLE hHWPerf;
	RGX_HWPERF_STREAM_ID eStreamId;
	IMG_UINT64 ui64Filter;

	switch (id) {
	case APPHINT_ID_HWPerfFWFilter:
		eStreamId = RGX_HWPERF_STREAM_ID0_FW;
		ui64Filter = value.UINT64;
		break;
	case APPHINT_ID_HWPerfHostFilter:
		eStreamId = RGX_HWPERF_STREAM_ID1_HOST;
		ui64Filter = (IMG_UINT64) value.UINT32;
		break;
	default:
		result = -EINVAL;
		goto err_exit;
	}

	if (RGXHWPerfLazyConnect(&hHWPerf) != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_ERROR, "Could not connect to HWPerf"));
		result = -EFAULT;
		goto err_exit;
	}

	eError = RGXHWPerfControl(hHWPerf, eStreamId, IMG_FALSE, ui64Filter);
	if (eError != PVRSRV_OK) {
		PVR_DPF((PVR_DBG_WARNING, "Failed to set filter for the GPU"));
		result = -EFAULT;
		goto err_disconnect;
	}

err_disconnect:
	(void) RGXHWPerfFreeConnection(hHWPerf); /* ignore return status */
err_exit:
	return result;
}

/**
 * check_device_initialised - check if all RGX devices have been initialised
 */
static int check_device_initialised(PVRSRV_DATA *driver_data)
{
	int result = 0;
	PVRSRV_DEVICE_NODE *dev_node = NULL;
	PVRSRV_RGXDEV_INFO *dev_info = NULL;

	if (!driver_data) {
		result = -ENODEV;
		goto err_exit;
	}

	dev_node = driver_data->psDeviceNodeList;
	while (dev_node) {
		dev_info = (PVRSRV_RGXDEV_INFO *)dev_node->pvDevice;
		if (!dev_info->bDevInit2Done)
			result = -ENODEV;
		dev_node = dev_node->psNext;
	}

err_exit:
	return result;
}

/**
 * apphint_action_worker - perform an action after an AppHint update has been
 *                    requested by a UM process
 *                    And update the record of the current active value
 */
static void apphint_action_worker(struct work_struct *work)
{
	struct apphint_work *action = container_of(work,
						struct apphint_work,
						work);
	int result = -EINVAL;
	APPHINT_ID id = action->id;
	union apphint_value value = action->value;

	APPHINT_ACTION action_type = param_lookup[id].action_type;

	PVRSRV_DATA *driver_data = PVRSRVGetPVRSRVData();

	if (0 != check_device_initialised(driver_data)) {
		/* Failure here indicates the RGX devices have not yet
		 * been initialised. This is the normal code path for module
		 * parameters provided on the command line and for any AppHint
		 * updates before the RGX devices have been opened:
		 * Update the parameter without attempting an action
		 */
		result = 0;
		goto err_exit;
	}

	if (value.UINT64 == apphint.val[id].UINT64) {
		/* Value is already set, so don't try to apply update */
		result = 0;
		goto err_exit;
	}

	switch (action_type) {
	case APPHINT_ACTION_FWSTF:
		result = action_device_flag(id, value, driver_data,
							RGXSetStateFlags);
		break;
	case APPHINT_ACTION_DEVFLG:
		result = action_device_flag(id, value, driver_data,
							RGXSetDeviceFlags);
		break;
	case APPHINT_ACTION_DEVPAR:
		result = action_device_bool(id, value, driver_data,
							RGXSetPdumpPanicEnable);
		break;
	case APPHINT_ACTION_HTBCTL:
		result = action_htb_param(id, value);
		break;
	case APPHINT_ACTION_FWTCTL:
		result = action_fwt_param(id, value, driver_data);
		break;
	case APPHINT_ACTION_APMCTL:
		result = action_apm_off(id, value, driver_data);
		break;
	case APPHINT_ACTION_HWPERF:
		result = action_hwp_filter(id, value);
		break;
	case APPHINT_ACTION_FWDMCTL:
	case APPHINT_ACTION_FW:
	case APPHINT_ACTION_HOST:
		result = 0;
		break;
	default:
		result = -EINVAL;
	}

err_exit:
	if (0 == result) {
		apphint.val[id] = value;
	} else {
		PVR_DPF((PVR_DBG_ERROR, "%s: failed (%d)", __func__, result));
	}

	kfree((void *)action);
}

static void apphint_action(APPHINT_ID id, union apphint_value value)
{
	struct apphint_work *work = kmalloc(sizeof(*work), GFP_KERNEL);

	/* queue apphint update on a serialized workqueue to avoid races */
	if (work) {
		work->id = id;
		work->value = value;
		INIT_WORK(&work->work, apphint_action_worker);
		if (0 == queue_work(apphint.workqueue, &work->work)) {
			PVR_DPF((PVR_DBG_ERROR,
				"%s: failed to queue apphint change request",
				__func__));
		}
	} else {
		PVR_DPF((PVR_DBG_ERROR,
			"%s: failed to alloc memory for apphint change request",
			__func__));
	}
}

/**
 * apphint_read - read the different AppHint data types
 * return -errno or the buffer size
 */
static int apphint_read(char *buffer, size_t count, APPHINT_ID ue,
			 union apphint_value *value)
{
	APPHINT_DATA_TYPE data_type = param_lookup[ue].data_type;
	int result = 0;

	switch (data_type) {
	case APPHINT_DATA_TYPE_UINT64:
		if (kstrtou64(buffer, 0, &value->UINT64) < 0) {
			PVR_DPF((PVR_DBG_ERROR,
				"%s: Invalid UINT64 input data for id %d: %s",
				__func__, ue, buffer));
			result = -EINVAL;
			goto err_exit;
		}
		break;
	case APPHINT_DATA_TYPE_UINT32:
		if (kstrtou32(buffer, 0, &value->UINT32) < 0) {
			PVR_DPF((PVR_DBG_ERROR,
				"%s: Invalid UINT32 input data for id %d: %s",
				__func__, ue, buffer));
			result = -EINVAL;
			goto err_exit;
		}
		break;
	case APPHINT_DATA_TYPE_BOOL:
	case APPHINT_DATA_TYPE_FLAG:
		switch (buffer[0]) {
		case '0':
		case 'n':
		case 'N':
		case 'f':
		case 'F':
			value->BOOL = IMG_FALSE;
			break;
		case '1':
		case 'y':
		case 'Y':
		case 't':
		case 'T':
			value->BOOL = IMG_TRUE;
			break;
		default:
			PVR_DPF((PVR_DBG_ERROR,
				"%s: Invalid BOOL input data for id %d: %s",
				__func__, ue, buffer));
			result = -EINVAL;
			goto err_exit;
		}
		break;
	case APPHINT_DATA_TYPE_UINT32List:
	{
		int i;
		struct apphint_lookup *lookup =
			(struct apphint_lookup *)
			param_lookup[ue].data_type_helper;
		int size = param_lookup[ue].helper_size;
		/* buffer may include '\n', remove it */
		char *arg = strsep(&buffer, "\n");

		if (!lookup) {
			result = -EINVAL;
			goto err_exit;
		}

		for (i = 0; i < size; i++) {
			if (strcasecmp(lookup[i].name, arg) == 0) {
				value->UINT32 = lookup[i].value;
				break;
			}
		}
		if (i == size) {
			if (strlen(arg) == 0) {
				PVR_DPF((PVR_DBG_ERROR,
					"%s: No value set for AppHint",
					__func__));
			} else {
				PVR_DPF((PVR_DBG_ERROR,
					"%s: Unrecognised AppHint value (%s)",
					__func__, arg));
			}
			result = -EINVAL;
		}
		break;
	}
	case APPHINT_DATA_TYPE_UINT32Bitfield:
	{
		int i;
		struct apphint_lookup *lookup =
			(struct apphint_lookup *)
			param_lookup[ue].data_type_helper;
		int size = param_lookup[ue].helper_size;
		/* buffer may include '\n', remove it */
		char *string = strsep(&buffer, "\n");
		char *token = strsep(&string, ",");

		if (!lookup) {
			result = -EINVAL;
			goto err_exit;
		}

		value->UINT32 = 0;
		/* empty string is valid to clear the bitfield */
		while (token && *token) {
			for (i = 0; i < size; i++) {
				if (strcasecmp(lookup[i].name, token) == 0) {
					value->UINT32 |= lookup[i].value;
					break;
				}
			}
			if (i == size) {
				PVR_DPF((PVR_DBG_ERROR,
					"%s: Unrecognised AppHint value (%s)",
					__func__, token));
				result = -EINVAL;
				goto err_exit;
			}
			token = strsep(&string, ",");
		}
		break;
	}
	case APPHINT_DATA_TYPE_UINT32Array:
		value->UINT32 = 0;
		break;
	default:
		result = -EINVAL;
		goto err_exit;
	}

err_exit:
	return (result < 0) ? result : count;
}

/**
 * apphint_write - write the current AppHint data to a buffer
 *
 * Returns length written or -errno
 */
static int apphint_write(char *buffer, const size_t size, const APPHINT_ID ue)
{
	const struct apphint_param *hint = &param_lookup[ue];
	union apphint_value value;
	int result = 0;

	value = apphint.val[ue];

	switch (hint->data_type) {
	case APPHINT_DATA_TYPE_UINT64:
		result += snprintf(buffer + result, size - result,
				"0x%016llx",
				value.UINT64);
		break;
	case APPHINT_DATA_TYPE_UINT32:
		result += snprintf(buffer + result, size - result,
				"0x%08x",
				value.UINT32);
		break;
	case APPHINT_DATA_TYPE_BOOL:
	case APPHINT_DATA_TYPE_FLAG:
		result += snprintf(buffer + result, size - result,
			"%s",
			value.BOOL ? "Y" : "N");
		break;
	case APPHINT_DATA_TYPE_UINT32List:
	{
		struct apphint_lookup *lookup =
			(struct apphint_lookup *) hint->data_type_helper;
		IMG_UINT32 i;

		if (!lookup) {
			result = -EINVAL;
			goto err_exit;
		}

		for (i = 0; i < hint->helper_size; i++) {
			if (lookup[i].value == value.UINT32) {
				result += snprintf(buffer + result,
						size - result,
						"%s",
						lookup[i].name);
				break;
			}
		}
		break;
	}
	case APPHINT_DATA_TYPE_UINT32Bitfield:
	{
		struct apphint_lookup *lookup =
			(struct apphint_lookup *) hint->data_type_helper;
		IMG_UINT32 i;

		if (!lookup) {
			result = -EINVAL;
			goto err_exit;
		}

		for (i = 0; i < hint->helper_size; i++) {
			if (lookup[i].value & value.UINT32) {
				result += snprintf(buffer + result,
						size - result,
						"%s,",
						lookup[i].name);
			}
		}
		if (result) {
			/* remove any trailing ',' */
			--result;
			*(buffer + result) = '\0';
		} else {
			result += snprintf(buffer + result,
					size - result, "none");
		}
		break;
	}
	case APPHINT_DATA_TYPE_UINT32Array:
		result += snprintf(buffer + result, size - result,
				"Not yet implemented");
		break;
	default:
		result = -EINVAL;
	}

err_exit:
	return result;
}

/*
*******************************************************************************
 Module parameters initialization - different from debugfs
******************************************************************************/
#define GET_APPHINT_ID_FROM_VALUE_ADDR(addr) \
	(APPHINT_ID)((union apphint_value *)addr - apphint.val)
/**
 * apphint_kparam_set - Handle an update of a module parameter
 *
 * Returns 0, or -errno.  arg is in kp->arg.
 */
static int apphint_kparam_set(const char *val, const struct kernel_param *kp)
{
	char val_copy[APPHINT_BUFFER_SIZE];
	const APPHINT_ID ue = GET_APPHINT_ID_FROM_VALUE_ADDR(kp->arg);
	union apphint_value value;
	int result;

	/* need to discard const in case of string comparison */
	result = strlcpy(val_copy, val, APPHINT_BUFFER_SIZE);
	if (result < APPHINT_BUFFER_SIZE) {
		result = apphint_read(val_copy, result,
				       ue, &value);
		if (result >= 0)
			apphint.val[ue] = value;
	} else {
		PVR_DPF((PVR_DBG_ERROR, "%s: String too long", __func__));
	}
	return (result > 0) ? 0 : result;
}

/**
 * apphint_kparam_get - handle a read of a module parameter
 *
 * Returns length written or -errno.  Buffer is 4k (ie. be short!)
 */
static int apphint_kparam_get(char *buffer, const struct kernel_param *kp)
{
	const APPHINT_ID ue = GET_APPHINT_ID_FROM_VALUE_ADDR(kp->arg);

	return apphint_write(buffer, PAGE_SIZE, ue);
}

static const struct kernel_param_ops __maybe_unused apphint_kparam_fops = {
	.set = apphint_kparam_set,
	.get = apphint_kparam_get,
};


/*
 * call module_param_cb() for all AppHints listed in APPHINT_LIST_MODPARAM
 * apphint_modparam_class_ ## resolves to apphint_modparam_enable() except for
 * AppHint classes that have been disabled.
 */

#define apphint_modparam_enable(name, number, perm) \
	module_param_cb(name, &apphint_kparam_fops, &apphint.val[number], perm);

#define X(a, b, c, d, e, f, g) \
	apphint_modparam_class_ ##e(a, APPHINT_ID_ ## a, (S_IRUSR|S_IRGRP|S_IROTH))
	APPHINT_LIST_MODPARAM
#undef X

/*
*******************************************************************************
 Debugfs get (seq file) operations - supporting functions
******************************************************************************/
static void *apphint_seq_start(struct seq_file *s, loff_t *pos)
{
	if (*pos == 0) {
		/* We want only one entry in the sequence, one call to show() */
		return (void *) 1;
	}

	PVR_UNREFERENCED_PARAMETER(s);

	return NULL;
}

static void apphint_seq_stop(struct seq_file *s, void *v)
{
	PVR_UNREFERENCED_PARAMETER(s);
	PVR_UNREFERENCED_PARAMETER(v);
}

static void *apphint_seq_next(struct seq_file *s, void *v, loff_t *pos)
{
	PVR_UNREFERENCED_PARAMETER(s);
	PVR_UNREFERENCED_PARAMETER(v);
	PVR_UNREFERENCED_PARAMETER(pos);
	return NULL;
}

static int apphint_seq_show(struct seq_file *s, void *v)
{
	APPHINT_ID ue = *(APPHINT_ID *) s->private;
	IMG_CHAR km_buffer[APPHINT_BUFFER_SIZE];
	int result;

	PVR_UNREFERENCED_PARAMETER(v);

	result = apphint_write(km_buffer, APPHINT_BUFFER_SIZE, ue);
	if (result < 0) {
		PVR_DPF((PVR_DBG_ERROR, "%s: failure", __func__));
	} else {
		/* debugfs requires a trailing \n, module_params don't */
		result += snprintf(km_buffer + result,
				APPHINT_BUFFER_SIZE - result,
				"\n");
		seq_puts(s, km_buffer);
	}

	/* have to return 0 to see output */
	return (result < 0) ? result : 0;
}

static const struct seq_operations apphint_seq_fops = {
	.start = apphint_seq_start,
	.stop  = apphint_seq_stop,
	.next  = apphint_seq_next,
	.show  = apphint_seq_show,
};

/*
*******************************************************************************
 Debugfs supporting functions
******************************************************************************/
/**
 * apphint_set - Handle a debugfs value update
 */
static ssize_t apphint_set(const char __user *buffer,
			    size_t count,
			    loff_t position,
			    void *data)
{
	APPHINT_ID id = *(APPHINT_ID *) data;
	union apphint_value value;
	char km_buffer[APPHINT_BUFFER_SIZE];
	int result = 0;

	PVR_UNREFERENCED_PARAMETER(position);

	if (count >= APPHINT_BUFFER_SIZE) {
		PVR_DPF((PVR_DBG_ERROR, "%s: String too long (%zd)",
			__func__, count));
		result = -EINVAL;
		goto err_exit;
	}

	if (pvr_copy_from_user(km_buffer, buffer, count)) {
		PVR_DPF((PVR_DBG_ERROR, "%s: Copy of user data failed",
			__func__));
		result = -EFAULT;
		goto err_exit;
	}
	km_buffer[count] = '\0';

	result = apphint_read(km_buffer, count, id, &value);
	if (result >= 0)
		apphint_action(id, value);

err_exit:
	return result;
}

/**
 * apphint_debugfs_init - Create the specified debugfs entries
 */
static int apphint_debugfs_init(char *sub_dir,
		unsigned init_data_size,
		const struct apphint_init_data *init_data,
		PVR_DEBUGFS_DIR_DATA **rootdir, PVR_DEBUGFS_ENTRY_DATA **entry)
{
	int result = 0;
	unsigned i;

	if (*rootdir) {
		PVR_DPF((PVR_DBG_WARNING,
			"AppHint DebugFS already created, skipping"));
		result = -EEXIST;
		goto err_exit;
	}

	result = PVRDebugFSCreateEntryDir(sub_dir, NULL,
					  rootdir);
	if (result < 0) {
		PVR_DPF((PVR_DBG_WARNING,
			"Failed to create \"%s\" DebugFS directory.", sub_dir));
		goto err_exit;
	}

	for (i = 0; i < init_data_size; i++) {
		if (!class_state[init_data[i].class].enabled)
			continue;

		result = PVRDebugFSCreateEntry(init_data[i].name,
				*rootdir,
				&apphint_seq_fops,
				(init_data[i].perm ? apphint_set : NULL),
				(void *) &init_data[i].id,
				&entry[i]);
		if (result < 0) {
			PVR_DPF((PVR_DBG_WARNING,
				"Failed to create \"%s/%s\" DebugFS entry.",
				sub_dir, init_data[i].name));
		}
	}

err_exit:
	return result;
}

/**
 * apphint_debugfs_deinit- destroy the debugfs entries
 */
static void apphint_debugfs_deinit(unsigned num_entries,
		PVR_DEBUGFS_DIR_DATA **rootdir, PVR_DEBUGFS_ENTRY_DATA **entry)
{
	unsigned i;

	for (i = 0; i < num_entries; i++) {
		if (entry[i]) {
			PVRDebugFSRemoveEntry(&entry[i]);
			entry[i] = NULL;
		}
	}

	if (*rootdir) {
		PVRDebugFSRemoveEntryDir(rootdir);
		*rootdir = NULL;
	}
}

/*
*******************************************************************************
 AppHint status dump implementation
******************************************************************************/
#if defined(PDUMP)
static void apphint_pdump_values(void *flags, const IMG_CHAR *format, ...)
{
	char km_buffer[APPHINT_BUFFER_SIZE];
	IMG_UINT32 ui32Flags = *(IMG_UINT32*)flags;
	va_list ap;

	va_start(ap, format);
	(void)vsnprintf(km_buffer, APPHINT_BUFFER_SIZE, format, ap);
	va_end(ap);

	PDumpCommentKM(km_buffer, ui32Flags);
}
#endif

static void apphint_dump_values(char *group_name,
			const struct apphint_init_data *group_data,
			int group_size,
			DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
			void *pvDumpDebugFile)
{
	int i, result;
	char km_buffer[APPHINT_BUFFER_SIZE];

	PVR_DUMPDEBUG_LOG("  %s", group_name);
	for (i = 0; i < group_size; i++) {
		result = apphint_write(km_buffer, APPHINT_BUFFER_SIZE,
				group_data[i].id);

		if (result <= 0) {
			PVR_DUMPDEBUG_LOG("    %s: <Error>",
				group_data[i].name);
		} else {
			PVR_DUMPDEBUG_LOG("    %s: %s",
				group_data[i].name, km_buffer);
		}
	}
}

/**
 * Callback for debug dump
 */
static void apphint_dump_state(PVRSRV_DBGREQ_HANDLE hDebugRequestHandle,
			IMG_UINT32 ui32VerbLevel,
			DUMPDEBUG_PRINTF_FUNC *pfnDumpDebugPrintf,
			void *pvDumpDebugFile)
{
	PVR_UNREFERENCED_PARAMETER(hDebugRequestHandle);

	if (DEBUG_REQUEST_VERBOSITY_HIGH == ui32VerbLevel) {
		PVR_DUMPDEBUG_LOG("------[ AppHint Settings ]------");

		apphint_dump_values("Build Vars",
			init_data_buildvar, ARRAY_SIZE(init_data_buildvar),
			pfnDumpDebugPrintf, pvDumpDebugFile);

		apphint_dump_values("Module Params",
			init_data_modparam, ARRAY_SIZE(init_data_modparam),
			pfnDumpDebugPrintf, pvDumpDebugFile);

		apphint_dump_values("Debugfs Params",
			init_data_debugfs, ARRAY_SIZE(init_data_debugfs),
			pfnDumpDebugPrintf, pvDumpDebugFile);
	}
}

/*
*******************************************************************************
 Public interface
******************************************************************************/
int pvr_apphint_init(void)
{
	int result;

	if (apphint.initialized) {
		result = -EEXIST;
		goto err_out;
	}


	/* create workqueue with strict execution ordering to ensure no
	 * race conditions when setting/updating apphints from different
	 * contexts
	 */
	apphint.workqueue = alloc_workqueue("apphint_workqueue", WQ_UNBOUND, 1);
	if (!apphint.workqueue) {
		result = -ENOMEM;
		goto err_out;
	}

	result = apphint_debugfs_init("apphint",
		ARRAY_SIZE(init_data_debugfs), init_data_debugfs,
		&apphint.debugfs_rootdir, apphint.debugfs_entry);
	if (0 != result)
		goto err_out;

	result = apphint_debugfs_init("buildvar",
		ARRAY_SIZE(init_data_buildvar), init_data_buildvar,
		&apphint.buildvar_rootdir, apphint.buildvar_entry);

	apphint.initialized = 1;

err_out:
	return result;
}

void pvr_apphint_deinit(void)
{
	if (!apphint.initialized)
		return;

	apphint_debugfs_deinit(APPHINT_DEBUGFS_ID_MAX,
			&apphint.debugfs_rootdir, apphint.debugfs_entry);
	apphint_debugfs_deinit(APPHINT_BUILDVAR_ID_MAX,
			&apphint.buildvar_rootdir, apphint.buildvar_entry);

	destroy_workqueue(apphint.workqueue);

	apphint.initialized = 0;
}

void pvr_apphint_device_register(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	(void)PVRSRVRegisterDbgRequestNotify(
			&psDeviceNode->hAppHintDbgReqNotify,
			psDeviceNode,
			apphint_dump_state,
			DEBUG_REQUEST_APPHINT,
			NULL);
}

void pvr_apphint_device_unregister(PVRSRV_DEVICE_NODE *psDeviceNode)
{
	if (psDeviceNode->hAppHintDbgReqNotify) {
		(void)PVRSRVUnregisterDbgRequestNotify(
			psDeviceNode->hAppHintDbgReqNotify);
		psDeviceNode->hAppHintDbgReqNotify = NULL;
	}
}

void pvr_apphint_dump_state(void)
{
#if defined(PDUMP)
	IMG_UINT32 ui32Flags = PDUMP_FLAGS_CONTINUOUS;
	apphint_dump_state(NULL, DEBUG_REQUEST_VERBOSITY_HIGH,
	                   apphint_pdump_values, (void*)&ui32Flags);
#endif
	apphint_dump_state(NULL, DEBUG_REQUEST_VERBOSITY_HIGH,
	                   NULL, NULL);
}

int pvr_apphint_get_uint64(APPHINT_ID ue, IMG_UINT64 *pVal)
{
	int error = -ERANGE;

	if (ue < APPHINT_ID_MAX) {
		*pVal = apphint.val[ue].UINT64;
		error = 0;
	}
	return error;
}

int pvr_apphint_get_uint32(APPHINT_ID ue, IMG_UINT32 *pVal)
{
	int error = -ERANGE;

	if (ue < APPHINT_ID_MAX) {
		*pVal = apphint.val[ue].UINT32;
		error = 0;
	}
	return error;
}

int pvr_apphint_get_bool(APPHINT_ID ue, IMG_BOOL *pVal)
{
	int error = -ERANGE;

	if (ue < APPHINT_ID_MAX) {
		error = 0;
		*pVal = apphint.val[ue].BOOL;
	}
	return error;
}

#endif /* #if defined(SUPPORT_KERNEL_SRVINIT) */
/* EOF */

