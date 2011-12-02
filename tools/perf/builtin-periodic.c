/*
 * Copyright (c) 2011, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

/*
 *	A very simple perf program to periodically print the performance
 *	counter reqested on the command line to standard out at the rate
 *	specified.
 *
 *	This is valuable for showing the output in a simple plot or
 *	exporting the counter data for post processing.  No attempt
 *	to process the data is made.
 *
 *	Scaling is not supported, use only as many counters as are
 *	provided by the hardware.
 *
 *	Math functions are support to combine counter results by using
 *	the -m flag.
 *
 *	The -r -w flags supports user signalling for input. This assumes
 *	that a pipe/fifo is needed so the -rw cmd line arg is a string
 *	that is the name of the named pipe to open for read/write.  User
 *	sends data on the read pipe to the process to collect a sample.
 *	Commands are also supported on the pipe.
 *
 */

#include "perf.h"
#include "builtin.h"
#include "util/util.h"
#include "util/parse-options.h"
#include "util/parse-events.h"
#include "util/event.h"
#include "util/evsel.h"
#include "util/evlist.h"
#include "util/debug.h"
#include "util/header.h"
#include "util/cpumap.h"
#include "util/thread.h"
#include <signal.h>
#include <sys/types.h>

#define PERF_PERIODIC_ERROR -1

/* number of pieces of data on each read. */
#define DATA_SIZE 2

#define DEFAULT_FIFO_NAME "xxbadFiFo"
#define MAX_NAMELEN 50

struct perf_evlist *evsel_list;

/*
 * command line variables and settings
 * Default to current process, no_inherit, process
 */
static pid_t target_pid = -1; /* all */
static bool system_wide;
static int cpumask = -1;  /* all */
static int ncounts;
static int ms_sleep = 1000;  /* 1 second */
static char const *operations = "nnnnnnnnnnnnnnnn";  /* nop */
static bool math_enabled;
static bool calc_delta;
static double old_accum, accum;
static int math_op_index;
static char const *wfifo_name = DEFAULT_FIFO_NAME;
static char const *rfifo_name = DEFAULT_FIFO_NAME;
static bool use_fifo;
static bool is_ratio;
static FILE *fd_in, *fd_out;

static FILE *tReadFifo, *tWriteFifo;

/*
 * Raw results from perf, we track the current value and
 * the old value.
 */
struct perf_raw_results_s {
	u64 values;
	u64 old_value;
};

/*
 * Everything we need to support a perf counter across multiple
 * CPUs.  We need to support multiple file descriptors (perf_fd)
 * because perf requires a fd per counter, so 1 per core enabled.
 *
 * Raw results values are calculated across all the cores as they
 * are read.
 */
struct perf_setup_s {
	int event_index;
	struct perf_event_attr *attr;
	int perf_fd[MAX_NR_CPUS];
	pid_t pid;
	int cpu;
	int flags;
	int group;
	struct perf_raw_results_s data;
	struct perf_raw_results_s totals;
	struct perf_raw_results_s output;
};

static void do_cleanup(void)
{
	if (fd_in) {
		if (0 != fclose(fd_in))
			error("Error closing fd_in\n");
	}
	if (fd_out) {
		if (0 != fclose(fd_out))
			error("Error closing fd_out\n");
	}
	if (use_fifo) {
		if (0 != unlink(rfifo_name))
			error("Error unlinking rfifo\n");
		if (0 != unlink(wfifo_name))
			error("Error unlinking wfifo\n");
	}
}

/*
 * Unexpected signal for error indication, cleanup
 */
static int sig_dummy;
static void sig_do_cleanup(int sig)
{
	sig_dummy = sig;
	do_cleanup();
	exit(0);
}

#define PERIODIC_MAX_STRLEN 100
/*
 * Delay for either a timed period or the wait on the read_fifo
 */
static void delay(unsigned long milli)
{
	char tmp_stg[PERIODIC_MAX_STRLEN];
	int done;
	int ret;

	if (use_fifo) {
		do {
			done = true;
			ret = fscanf(tReadFifo, "%s", tmp_stg);
			if (ret == 0)
				return;
			/*
			 * Look for a command request, and if we get a command
			 * Need to process and then wait again w/o sending data.
			 */
			if (strncmp(tmp_stg, "PID", strnlen(tmp_stg,
				PERIODIC_MAX_STRLEN)) == 0) {
				fprintf(fd_out, " %u\n", getpid());
				fflush(fd_out);
				done = false;
			} else if (strncmp(tmp_stg, "EXIT",
					strnlen(tmp_stg, PERIODIC_MAX_STRLEN))
						== 0) {
				do_cleanup();
				exit(0);
			}

		} while (done != true);
	} else
		usleep(milli*1000);
}

/*
 * Create a perf counter event.
 * Some interesting behaviour that is not documented anywhere else:
 * the CPU will not work if out of range.
 * The CPU will only work for a single CPU, so to collect the counts
 * on the system in SMP based systems a counter needs to be created
 * for each CPU.
 */
static int create_perf_counter(struct perf_setup_s *p)
{
	struct cpu_map *cpus;
	int cpu;

	cpus = cpu_map__new(NULL);
	if (p == NULL)
		return PERF_PERIODIC_ERROR;
	for (cpu = 0; cpu < cpus->nr; cpu++) {
		p->perf_fd[cpu] = sys_perf_event_open(p->attr, target_pid, cpu,
					-1, 0);
		if (p->perf_fd[cpu] < 0)
			return PERF_PERIODIC_ERROR;
	}
	return 0;
}

/*
 * Perf init setup
 */
static int perf_setup_init(struct perf_setup_s *p)
{
	if (p == NULL)
		return PERF_PERIODIC_ERROR;

	bzero(p, sizeof(struct perf_setup_s));
	p->group = -1;
	p->flags = 0;

	p->output.values = 0;
	p->output.old_value = 0;
	p->data.values = 0;
	p->data.old_value = 0;
	p->totals.old_value = 0;
	p->totals.values = 0;

	return 0;
}

/*
 * Read in ALL the performance counters configured for the CPU,
 * one performance monitor per core that was configured during
 * "all" mode
 */
static int perf_setup_read(struct perf_setup_s *p)
{
	u64 data[DATA_SIZE];
	int i, status;

	p->totals.values = 0;
	p->data.values = 0;
	for (i = 0; i < MAX_NR_CPUS; i++) {
		if (p->perf_fd[i] == 0)
			continue;
		status = read(p->perf_fd[i], &data, sizeof(data));
		p->data.values += data[0];
		p->totals.values += data[0];
	}

	/*
	 * Normally we show totals, we want to support
	 * showing deltas from the previous value so external apps do not have
	 * to do this...
	 */
	if (calc_delta) {
		p->output.values = p->data.values - p->data.old_value;
		p->data.old_value = p->data.values;
	} else
		p->output.values = p->totals.values;
	return 0;
}

static int perf_setup_show(struct perf_setup_s *p)
{
	if (p == NULL)
		return PERF_PERIODIC_ERROR;
	fprintf(fd_out, " %llu", p->output.values);
	return 0;
}


static const char * const periodic_usage[] = {
	"perf periodic [<options>]",
	NULL
};

static const struct option options[] = {
	OPT_CALLBACK('e', "event", &evsel_list, "event",
	"event selector. use 'perf list' to list available events",
	 parse_events),
	OPT_STRING('m', "math-operations", &operations, "nnnnnn",
	"math operation to perform on values collected asmd in order"),
	OPT_STRING('r', "readpipe", &rfifo_name, "xxbadFiFo",
	"wait for a user input fifo - will be created"),
	OPT_STRING('w', "writepipe", &wfifo_name, "xxbadFifo",
	"write data out on this pipe - pipe is created"),
	OPT_INTEGER('i', "increment", &ncounts,
	"number of times periods to count/iterate (default 0-forever)"),
	OPT_INTEGER('p', "pid", &target_pid,
	"stat events on existing process id"),
	OPT_INTEGER('c', "cpumask", &cpumask,
	"cpumask to enable counters, default all (-1)"),
	OPT_INTEGER('s', "sleep", &ms_sleep,
	"how long to sleep in ms between each sample (default 1000)"),
	OPT_BOOLEAN('a', "all-cpus", &system_wide,
	"system-wide collection from all CPUs overrides cpumask"),
	OPT_BOOLEAN('d', "delta", &calc_delta,
	"calculate and display the delta values math funcs will use delta"),
	OPT_INCR('v', "verbose", &verbose,
	"be more verbose (show counter open errors, etc)"),
	OPT_END()
};

/*
 * After every period we reset any math that was performed.
 */
static void reset_math(void)
{
	math_op_index = 0;
	old_accum = accum;
	accum = 0;
}

static void do_math_op(struct perf_setup_s *p)
{
	if (!math_enabled)
		return;
	switch (operations[math_op_index++]) {
	case 'm':
		accum *= (double)p->output.values; break;
	case 'a':
		accum += (double)p->output.values; break;
	case 's':
		accum -= (double)p->output.values; break;
	case 'd':
		accum /= (double)p->output.values; break;
	case 'z':
		accum =  0; break;
	case 't':
		accum =  (double)p->output.values; break; /*transfer*/
	case 'T':
		accum +=  old_accum; break; /*total*/
	case 'i':	/* ignore */
	default:
		break;
	}
}

int cmd_periodic(int argc, const char **argv, const char *prefix __used)
{
	int status = 0;
	int c, i;
	struct perf_setup_s *p[MAX_COUNTERS];
	struct perf_evsel *counter;
	FILE *fp;
	int nr_counters = 0;

	evsel_list = perf_evlist__new(NULL, NULL);
	if (evsel_list == NULL)
		return -ENOMEM;

	argc = parse_options(argc, argv, options, periodic_usage,
		PARSE_OPT_STOP_AT_NON_OPTION);

	if (system_wide)
		cpumask = -1;

	/*
	 * The r & w option redirects stdout to a newly created pipe and
	 * waits for input on the read pipe before continuing
	 */
	fd_in = stdin;
	fd_out = stdout;
	if (strncmp(rfifo_name, DEFAULT_FIFO_NAME,
				strnlen(rfifo_name, MAX_NAMELEN))) {
		fp = fopen(rfifo_name, "r");
		if (fp != NULL) {
			fclose(fp);
			remove(rfifo_name);
		}
		if (mkfifo(rfifo_name, 0777) == -1) {
			error("Could not open read fifo\n");
			do_cleanup();
			return PERF_PERIODIC_ERROR;
		}
		tReadFifo = fopen(rfifo_name, "r+");
		if (tReadFifo == 0) {
			do_cleanup();
			error("Could not open read fifo file\n");
			return PERF_PERIODIC_ERROR;
		}
		use_fifo = true;
	}
	if (strncmp(wfifo_name, DEFAULT_FIFO_NAME,
				strnlen(wfifo_name, MAX_NAMELEN)))  {
		fp = fopen(wfifo_name, "r");
		if (fp != NULL) {
			fclose(fp);
			remove(wfifo_name);
		}
		if (mkfifo(wfifo_name, 0777) == -1) {
			do_cleanup();
			error("Could not open write fifo\n");
			return PERF_PERIODIC_ERROR;
		}
		fd_out = fopen(wfifo_name, "w+");
		if (fd_out == 0) {
			do_cleanup();
			error("Could not open write fifo file\n");
			return PERF_PERIODIC_ERROR;
		}
		tWriteFifo = fd_out;
	}

	math_enabled = (operations[0] != 'n');

	/*
	 * If we don't ignore SIG_PIPE then when the other side
	 * of a pipe closes we shutdown too...
	 */
	signal(SIGPIPE, SIG_IGN);
	signal(SIGINT, sig_do_cleanup);
	signal(SIGQUIT, sig_do_cleanup);
	signal(SIGKILL, sig_do_cleanup);
	signal(SIGTERM, sig_do_cleanup);

	i = 0;
	list_for_each_entry(counter, &evsel_list->entries, node) {
		p[i] = malloc(sizeof(struct perf_setup_s));
		if (p[i] == NULL) {
			error("Error allocating perf_setup_s\n");
			do_cleanup();
			return PERF_PERIODIC_ERROR;
		}
		bzero(p[i], sizeof(struct perf_setup_s));
		perf_setup_init(p[i]);
		p[i]->attr = &(counter->attr);
		p[i]->event_index = counter->idx;
		if (create_perf_counter(p[i]) < 0) {
			do_cleanup();
			die("Not all events could be opened.\n");
			return PERF_PERIODIC_ERROR;
		}
		i++;
		nr_counters++;
	}
	i = 0;
	while (1) {

		/*
		 * Wait first otherwise single sample will print w/o signal
		 * when using the -u (user signal) flag
		 */
		delay(ms_sleep);

		/*
		 * Do the collection, read and then perform any math operations
		 */
		for (c = 0; c < nr_counters; c++) {
			status = perf_setup_read(p[c]);
			do_math_op(p[c]);
		}

		/*
		 * After all collection and math, we perform one last math
		 * to allow totaling, if enabled etc, then either printout
		 * a single float value when the math is enabled or ...
		 */
		if (math_enabled) {
			do_math_op(p[c]);
			if (is_ratio)
				fprintf(fd_out, "%#f\n", accum*100);
			else
				fprintf(fd_out, "%#f\n", accum);
		} else {
			/*
			 * ... print out one integer value for each counter
			 */
			for (c = 0; c < nr_counters; c++)
				status = perf_setup_show(p[c]);
			fprintf(fd_out, "\n");
		}

		/*
		 * Did the user give us an iteration count?
		 */
		if ((ncounts != 0) && (++i >= ncounts))
			break;
		reset_math();
		fflush(fd_out); /* make sure data is flushed out the pipe*/
	}

	do_cleanup();

	return status;
}
