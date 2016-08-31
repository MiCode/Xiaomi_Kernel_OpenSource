/*
 * Copyright (c) 2013, NVIDIA CORPORATION.  All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms and conditions of the GNU General Public License,
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/workqueue.h>
#include <linux/slab.h>
#include <linux/printk.h>
#include <linux/export.h>
#include "nvshm_rpc_dispatcher.h"

struct global_data {
	bool cleaning_up;
	struct workqueue_struct *wq;
	struct nvshm_rpc_program *programs[NVSHM_RPC_PROGRAMS_MAX];
};

struct work_data {
	struct nvshm_rpc_message *request;
	struct work_struct work;
};

static struct global_data global;

/* Meaningful Sun RPC protocol errors */
static const char * const protocol_errors[] = {
	"success",
	"program unavailable",
	"program version mismatch",
	"procedure unavailable",
	"garbage arguments",
	"system error",
};

/*
 * This function is called in a dedicated thread and calls the function
 * managers, which in turn call the real function.
 */
static void nvshm_rpc_dispatcher(struct work_struct *work)
{
	struct work_data *data = container_of(work, struct work_data, work);
	struct nvshm_rpc_message *request = data->request;
	struct nvshm_rpc_message *response = NULL;
	struct nvshm_rpc_procedure procedure;
	struct nvshm_rpc_program *program = NULL;
	nvshm_rpc_function_t function;
	enum rpc_accept_stat rc;

	/* Try to find a function to call */
	nvshm_rpc_utils_decode_procedure(request, &procedure);
	/* Find program */
	if (procedure.program >= NVSHM_RPC_PROGRAMS_MAX) {
		rc = RPC_PROG_UNAVAIL;
		goto done;
	}
	program = global.programs[procedure.program];
	if (!program) {
		rc = RPC_PROG_UNAVAIL;
		goto done;
	}
	/* Check version */
	if ((procedure.version < program->version_min) ||
			(procedure.version > program->version_max)) {
		rc = RPC_PROG_MISMATCH;
		goto done;
	}
	/* Find function */
	if (procedure.procedure >= program->procedures_size) {
		rc = RPC_PROC_UNAVAIL;
		goto done;
	}
	function = program->procedures[procedure.procedure];
	if (!function) {
		rc = RPC_PROC_UNAVAIL;
		goto done;
	}
	rc = function(procedure.version, request, &response);
done:
	/* Check we still have someone to reply to */
	if (!global.cleaning_up) {
		if (rc == RPC_PROG_MISMATCH) {
			/* Create version mismatch error message */
			struct nvshm_rpc_datum_in vers[] = {
				NVSHM_RPC_IN_UINT(program->version_min),
				NVSHM_RPC_IN_UINT(program->version_max),
			};
			u32 n = ARRAY_SIZE(vers);

			pr_err("failed to reply to %d:%d:%d: %s\n",
			       procedure.program, procedure.version,
			       procedure.procedure, protocol_errors[rc]);
			response = nvshm_rpc_utils_prepare_response(request, rc,
								    vers, n);
		} else if (rc != RPC_SUCCESS) {
			/* Create other error message */
			pr_err("failed to reply to %d:%d:%d: %s\n",
			       procedure.program, procedure.version,
			       procedure.procedure, protocol_errors[rc]);
			response = nvshm_rpc_utils_prepare_response(request, rc,
								    NULL, 0);
		}
		if (response) {
			pr_debug("send response\n");
			nvshm_rpc_send(response);
		}
	} else if (response)
		nvshm_rpc_free(response);
	nvshm_rpc_free(request);
	kfree(data);
}

/*
 * This function receives the requests and creates a thread to do the work.
 */
static void nvshm_rpc_dispatch(struct nvshm_rpc_message *request, void *context)
{
	struct work_data *data;

	data = kmalloc(sizeof(struct work_data), GFP_KERNEL);
	data->request = request;
	INIT_WORK(&data->work, nvshm_rpc_dispatcher);
	queue_work(global.wq, &data->work);
}

int nvshm_rpc_dispatcher_init(void)
{
	global.wq = alloc_workqueue("nvshm_rpc_work", WQ_UNBOUND|WQ_HIGHPRI, 0);
	if (!global.wq) {
		pr_err("failed to create workqueue\n");
		return -ENOMEM;
	}
	global.cleaning_up = false;
	nvshm_rpc_setdispatcher(nvshm_rpc_dispatch, NULL);
	return 0;
}

void nvshm_rpc_dispatcher_cleanup(void)
{
	/* This call likely means that the modem is gone */
	global.cleaning_up = true;
	nvshm_rpc_setdispatcher(NULL, NULL);
	flush_workqueue(global.wq);
	destroy_workqueue(global.wq);
}

int nvshm_rpc_program_register(enum nvshm_rpc_programs index,
			       struct nvshm_rpc_program *program)
{
	if (index >= NVSHM_RPC_PROGRAMS_MAX)
		return -EINVAL;
	if (global.programs[index])
		return -EBUSY;
	global.programs[index] = program;
	pr_info("program #%d registered\n", index);
	return 0;
}
EXPORT_SYMBOL_GPL(nvshm_rpc_program_register);

void nvshm_rpc_program_unregister(enum nvshm_rpc_programs index)
{
	if (index >= NVSHM_RPC_PROGRAMS_MAX)
		return;
	global.programs[index] = NULL;
	pr_info("program #%d unregistered\n", index);
}
EXPORT_SYMBOL_GPL(nvshm_rpc_program_unregister);
