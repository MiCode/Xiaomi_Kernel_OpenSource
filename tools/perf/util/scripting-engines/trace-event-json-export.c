/*
 * trace-event-json-export.  Export events to JSON format.
 *
 * derived from: trace-event-python.c
 *
 * Copyright (c) 2011-2012, Code Aurora Forum. All rights reserved.
 * Copyright (C) 2010 Tom Zanussi <tzanussi@gmail.com>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

#include "../../perf.h"
#include "../util.h"
#include "../trace-event.h"
#include "../event.h"
#include "../thread.h"

#define FTRACE_MAX_EVENT				\
	((1 << (sizeof(unsigned short) * 8)) - 1)

FILE *ofp;
struct event *events[FTRACE_MAX_EVENT];
static char *cur_field_name;

static void define_value(enum print_arg_type field_type,
			 int id,
			 const char *field_name,
			 const char *field_value,
			 const char *field_str)
{
	const char *handler_name = (field_type == PRINT_SYMBOL) ?
		"define_symbol" : "define_flag";
	unsigned long long value;

	value = eval_flag(field_value);

	fprintf(ofp,
		",\n[\"%s\",%d,{\"field\":\"%s\",\"value\":%llu,\"name\":\"%s\"}]",
		handler_name, id, field_name, value, field_str);
}

static void define_values(enum print_arg_type field_type,
			  struct print_flag_sym *field,
			  int id,
			  const char *field_name)
{
	define_value(field_type, id, field_name, field->value,
			 field->str);

	if (field->next)
		define_values(field_type, field->next, id, field_name);
}

static void define_field(enum print_arg_type field_type,
			 int id,
			 const char *field_name,
			 const char *delim)
{
	if (field_type == PRINT_FLAGS) {
		const char *handler_name = "define_flag_field";
		fprintf(ofp,
			",\n[\"%s\",%d,{\"field\":\"%s\",\"delim\":\"%s\"}]",
			handler_name, id, field_name, delim);
	} else {
		const char *handler_name = "define_symbol_field";
		fprintf(ofp, ",\n[\"%s\",%d,{\"field\":\"%s\"}]",
				handler_name, id, field_name);
	}
}

static void define_event_symbols(struct event *event,
				struct print_arg *args)
{
	switch (args->type) {
	case PRINT_NULL:
		break;
	case PRINT_ATOM:
		define_value(PRINT_FLAGS, event->id, cur_field_name, "0",
				 args->atom.atom);
		break;
	case PRINT_FIELD:
		cur_field_name = args->field.name;
		break;
	case PRINT_FLAGS:
		define_event_symbols(event, args->flags.field);
		define_field(PRINT_FLAGS, event->id, cur_field_name,
				 args->flags.delim);
		define_values(PRINT_FLAGS, args->flags.flags, event->id,
				  cur_field_name);
		break;
	case PRINT_SYMBOL:
		define_event_symbols(event, args->symbol.field);
		define_field(PRINT_SYMBOL, event->id, cur_field_name, NULL);
		define_values(PRINT_SYMBOL, args->symbol.symbols, event->id,
				  cur_field_name);
		break;
	case PRINT_STRING:
		break;
	case PRINT_TYPE:
		define_event_symbols(event, args->typecast.item);
		break;
	case PRINT_OP:
		define_event_symbols(event, args->op.left);
		define_event_symbols(event, args->op.right);
		break;
	default:
		/* we should warn... */
		return;
	}

	if (args->next)
		define_event_symbols(event, args->next);
}

#define prefix(indx) (indx ? "," : "")
static void define_event(struct event *event)
{
	const char *ev_system = event->system;
	const char *ev_name = event->name;
	int indx = 0;
	const char *handler_name = "define_event";
	struct format_field *field = 0;

	fprintf(ofp,
		",\n[\"%s\",%d,{\"system\":\"%s\",\"name\":\"%s\",\"args\":{",
			handler_name, event->id, ev_system, ev_name);

	fprintf(ofp, "%s\"%s\":%d", prefix(indx), "common_s", indx);
	indx++;
	fprintf(ofp, "%s\"%s\":%d", prefix(indx), "common_ns", indx);
	indx++;
	fprintf(ofp, "%s\"%s\":%d", prefix(indx), "common_cpu", indx);
	indx++;
	fprintf(ofp, "%s\"%s\":%d", prefix(indx), "common_comm", indx);
	indx++;
	for (field = event->format.common_fields; field; field = field->next) {
		fprintf(ofp, "%s\"%s\":%d", prefix(indx), field->name, indx);
		indx++;
	}
	for (field = event->format.fields; field; field = field->next) {
		fprintf(ofp, "%s\"%s\":%d", prefix(indx), field->name, indx);
		indx++;
	}
	fprintf(ofp, "}}]");
}

static inline struct event *find_cache_event(int type)
{
	struct event *event;

	if (events[type])
		return events[type];

	events[type] = event = trace_find_event(type);
	if (!event)
		return NULL;

	define_event(event);
	define_event_symbols(event, event->print_fmt.args);

	return event;
}

static void json_process_field(int indx, void *data, struct format_field *field)
{
	unsigned long long val;

	if (field->flags & FIELD_IS_STRING) {
		int offset;
		if (field->flags & FIELD_IS_DYNAMIC) {
			offset = *(int *)(data + field->offset);
			offset &= 0xffff;
		} else
			offset = field->offset;
		fprintf(ofp, "%s\"%s\"", prefix(indx), (char *)data + offset);
	} else { /* FIELD_IS_NUMERIC */
		val = read_size(data + field->offset, field->size);
		if (field->flags & FIELD_IS_SIGNED)
			fprintf(ofp, "%s%lld", prefix(indx),
					(long long int) val);
		else
			fprintf(ofp, "%s%llu", prefix(indx), val);
	}
}

static void json_process_event(union perf_event *pevent __unused,
			       struct perf_sample *sample,
			       struct perf_evsel *evsel __unused,
			       struct machine *machine __unused,
			       struct thread *thread)
{
	struct format_field *field;
	unsigned long s, ns;
	struct event *event;
	int type;
	int indx = 0;
	int cpu = sample->cpu;
	void *data = sample->raw_data;
	unsigned long long nsecs = sample->time;
	char *comm = thread->comm;

	type = trace_parse_common_type(data);

	event = find_cache_event(type);
	if (!event)
		die("ug! no event found for type %d", type);

	s = nsecs / NSECS_PER_SEC;
	ns = nsecs - s * NSECS_PER_SEC;

	fprintf(ofp, ",\n[\"event\",%d,[%lu,%lu,%d,\"%s\"",
			type, s, ns, cpu, comm);

	indx += 4;

	for (field = event->format.common_fields; field; field = field->next)
		json_process_field(indx++, data, field);

	for (field = event->format.fields; field; field = field->next)
		json_process_field(indx++, data, field);

	fprintf(ofp , "]]");
}

/*
 * Start trace script
 */
static int json_start_script(const char *script, int argc __unused,
		const char **argv __unused)
{
	int err = 0;

	if (script[0]) {
		ofp = fopen(script, "w");
		if (ofp == NULL) {
			fprintf(stderr, "couldn't open %s\n", script);
			return -EBADF;
		}
	} else
		ofp = stdout;

	fprintf(ofp, "[[\"trace_start\"]");

	return err;
}

/*
 * Stop trace script
 */
static int json_stop_script(void)
{
	int err = 0;

	fprintf(ofp, ",\n[\"trace_end\"]]");

	return err;
}

static int json_generate_script(const char *outfile)
{
	struct event *event = NULL;
	char fname[PATH_MAX];

	snprintf(fname, sizeof(fname), "%s.json", outfile);

	ofp = fopen(fname, "w");

	if (ofp == NULL) {
		fprintf(stderr, "couldn't open %s\n", fname);
		return -EBADF;
	}
	fprintf(ofp, "[[\"generate_start\"]");

	while ((event = trace_find_next_event(event))) {
		define_event(event);
		define_event_symbols(event, event->print_fmt.args);
	}

	fprintf(ofp, ",\n[\"generate_end\"]]");

	fclose(ofp);

	fprintf(stderr, "generated json script: %s\n", fname);

	return 0;
}

struct scripting_ops json_scripting_ops = {
	.name = "JSON",
	.start_script = json_start_script,
	.stop_script = json_stop_script,
	.process_event = json_process_event,
	.generate_script = json_generate_script,
};

void setup_json_export(void)
{
	int err;
	err = script_spec_register("JSON", &json_scripting_ops);
	if (err)
		die("error registering JSON export extension");
}

