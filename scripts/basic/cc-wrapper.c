// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2021, The Linux Foundation. All rights reserved.
 */

#define _GNU_SOURCE
#include <poll.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <regex.h>
#include <limits.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/wait.h>

/*
 * Usage: cc-wrapper [COMMAND]
 *
 *  Invoke [COMMAND] and scan for lines matching C compiler "warning:" lines
 *  If a warning is found, check if in a database whether it should be ignored.
 *  If that warning isn't ignored, bail return an error and delete the output file
 *  so that the overall Make environment fails.
 */

/*
 * 1. match as much as possible which ends with a slash '/'
 * 2. match as many characters as possible ending with .
 * 3. match the filetype
 * 4. Optionally match some line and column info of the style :123:456, etc.
 * 5. Match ': warning:'
 *
 * Ex: msm-kernel/drivers/firmware/qcom_scm.c:929:11: warning: unused variable 'unused' [-Wunused-variable]
 *     (           1              )(   2   )3(   4  )(   5   )
 */
#define REGEXPR \
	"^\\(.*\\/\\)\\([^\\/][^\\/]*\\.[a-zA-Z0-9][a-zA-Z0-9]*\\(:[0-9,-][0-9,-]*\\)*\\): warning:[^\\n]*"

static regex_t reg;
static int reg_compiled;

struct ignored_warning {
	const char * const warning;
	const size_t len;
};
#define IGNORE_WARNING(str) { (str), sizeof((str)) - 1 }

const struct ignored_warning ignored_warnings[] = {
	IGNORE_WARNING("signal.c:51"),
	IGNORE_WARNING("signal.c:95"),
	{ NULL, 0 }
};

static bool is_ignored_warning(const char * const text, size_t len)
{
	const struct ignored_warning *el = &ignored_warnings[0];

	while (el->warning) {
		/* Check that printed warning location is at least as long as
		 * the ignored one. If not, no point in strncmp */
		if (len >= el->len && !strncmp(text, el->warning, el->len))
			return true;
		el++;
	}
	return false;
}

static int process_buffer(const char *buf)
{
	int ret = 0;
	regmatch_t m[3];

	if (!buf)
		return 0;

	/* Avoid compiling the regex since it takes a little bit of time -- why not avoid */
	/* Check if there is ': warning:' in the line, if so, then we need to use regex */
	if (!reg_compiled && strcmp(buf, ": warning:")) {
		ret = regcomp(&reg, REGEXPR, 0);
		if (ret)
			return ret;
		reg_compiled = 1;
	}

	if (reg_compiled) {
		while (!regexec(&reg, buf, 3, m, 0)) {
			if (!is_ignored_warning(buf + m[2].rm_so, m[2].rm_eo - m[2].rm_so)) {
				fprintf(stderr, "\nerror, forbidden warning: %.*s\n",
					m[2].rm_eo - m[2].rm_so,
					buf + m[2].rm_so);
				ret = 1;
			}
			buf += m[0].rm_eo; /* Skip past everything that got matched */
		}
	}

	return ret;
}

int main(int argc, char **argv)
{
	int pipes[2];
	int ret, status, len;
	int i, have_error = 0;
	pid_t pid;
	char buf[PIPE_BUF];

	/*
	 * linebuf is used only when encountering a line split over multiple reads
	 * Use a fixed linebuf size to avoid malloc.
	 * The assumption here is that warning line will be a path, followed by
	 * some numbers, followed by ": warning:" text. Thus, we can make a rough estimate
	 * of the max size a warning line could be in order to capture that it's a warning
	 * line.
	 */
	size_t linebuf_len = 0;
	char linebuf[PATH_MAX + 0x100];

	if (argc < 2)
		return 0;

	ret = pipe(pipes);
	if (ret)
		return ret;

	pid = fork();
	if (pid == 0) {
		close(pipes[0]);

		dup2(pipes[1], STDERR_FILENO);

		return execvp(argv[1], argv + 1);
	}

	close(pipes[1]);

	if (pid < 0) {
		ret = pid;
		goto done;
	}

	while ((len = read(pipes[0], buf, sizeof(buf))) > 0) {
		char *next = buf;

		write(STDERR_FILENO, buf, len);
		buf[len] = '\0';

		if (linebuf_len) {
			/* Process incomplete line buffered from prev iteration */
			int buf_line_len;

			/* Find first newline in the newly read buffer (or end of buffer) */
			next = memchr(buf, '\n', len);
			if (!next)
				buf_line_len = len;
			else
				buf_line_len = next + 1 - buf; /* Add one to get back the newline */

			if (buf_line_len + linebuf_len >= sizeof(linebuf))
				buf_line_len = sizeof(linebuf) - linebuf_len - 1;

			/* copy it to the linebuf */
			memcpy(linebuf + linebuf_len, buf, buf_line_len);
			linebuf_len += buf_line_len;
			linebuf[linebuf_len] = '\0';

			/* check for warnings */
			if (process_buffer(linebuf)) {
				have_error = 1;
				linebuf_len = 0;
			} else if (linebuf[linebuf_len - 1] == '\n' ||
				   linebuf_len >= sizeof(linebuf) - 1) {
				/*
				 * reset the linebuf if we found a complete line or if the linebuf
				 * overflowed. We don't care about lines longer than linebuf because
				 * linebuf is highly unlikely to contain the ": warning:" text.
				 * This is because linebuf is PATH_MAX + 256 bytes
				 */
				linebuf_len = 0;
			}

			linebuf[linebuf_len] = '\0';
		}

		if (next) {
			/* continue processing rest of buffer */
			int last_line_len;

			/* check for warnings */
			if (process_buffer(next))
				have_error = 1;

			/* Find last newline in the buffer */
			next = memrchr(next, '\n', len - (next - buf));
			if (!next) {
				next = buf;
				last_line_len = len;
			} else {
				last_line_len = len - (next - buf);
			}

			if (last_line_len + linebuf_len >= sizeof(linebuf))
				last_line_len = sizeof(linebuf) - linebuf_len - 1;

			/*
			 * copy everything after the last newline to linebuf.
			 * there may be 0 bytes to copy if buffer ended with newline
			 *   no special handling needed for that case
			 */
			memcpy(linebuf + linebuf_len, next, last_line_len);
			linebuf_len += last_line_len;
			linebuf[linebuf_len] = '\0';
		}
	}

	/* We've finished reading from pipe, process anything leftover, just in case */
	if (process_buffer(linebuf))
		have_error = 1;

done:
	if (have_error) {
		/*
		 * Found an error? Scan the args passed to compiler and see if
		 * there is "-o out.o". If so, remove the file so that Make
		 * will attempt to recompile it next time around. Otherwise, next
		 * build will go through since out.o was created.
		 */
		for (i = 1 ; i < argc - 1 ; i++) {
			if (strcmp(argv[i], "-o") == 0) {
				remove(argv[i + 1]);
				break;
			}
		}
	}

	if (reg_compiled)
		regfree(&reg);
	close(pipes[0]);
	waitpid(pid, &status, 0);
	/* Did we have an error? */
	if (ret)
		return ret;
	/* Did cc exit gracefully? If not, then something went wrong with cc */
	if (!WIFEXITED(status))
		return 1;
	/* Did cc exit with success? If not, did we find an error? */
	return WEXITSTATUS(status) ?: have_error;
}
