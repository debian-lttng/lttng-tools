/*
 * Copyright (C) 2019 Jonathan Rajotte <jonathan.rajotte-julien@efficios.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 */

#define _LGPL_SOURCE
#include <popt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdbool.h>
#include <lttng/clear.h>
#include <lttng/clear-handle.h>

#include "../command.h"

#include <common/mi-lttng.h>
#include <common/sessiond-comm/sessiond-comm.h>
#include <common/utils.h>

static int opt_clear_all;

#ifdef LTTNG_EMBED_HELP
static const char help_msg[] =
#include <lttng-clear.1.h>
;
#endif

/* Mi writer */
static struct mi_writer *writer;

enum {
	OPT_HELP = 1,
	OPT_LIST_OPTIONS,
};

static struct poptOption long_options[] = {
	/* longName, shortName, argInfo, argPtr, value, descrip, argDesc */
	{"help",      'h', POPT_ARG_NONE, 0, OPT_HELP, 0, 0},
	{"all",       'a', POPT_ARG_VAL, &opt_clear_all, 1, 0, 0},
	{"list-options", 0, POPT_ARG_NONE, NULL, OPT_LIST_OPTIONS, NULL, NULL},
	{0, 0, 0, 0, 0, 0, 0}
};

/*
 * clear session
 */
static int clear_session(struct lttng_session *session)
{
	enum lttng_clear_handle_status status =
			LTTNG_CLEAR_HANDLE_STATUS_OK;
	struct lttng_clear_handle *handle = NULL;
	enum lttng_error_code ret_code;
	bool printed_wait_msg = false;
	char *session_name = NULL;
	int ret;

	ret = lttng_clear_session(session->name, &handle);
	if (ret < 0) {
		ERR("%s", lttng_strerror(ret));
		goto error;
	}

	do {
		status = lttng_clear_handle_wait_for_completion(handle,
				DEFAULT_DATA_AVAILABILITY_WAIT_TIME_US / USEC_PER_MSEC);
		switch (status) {
		case LTTNG_CLEAR_HANDLE_STATUS_TIMEOUT:
			if (!printed_wait_msg) {
				_MSG("Waiting for clear of session \"%s\"",
						session->name);
				printed_wait_msg = true;
			}
			_MSG(".");
			fflush(stdout);
			break;
		case LTTNG_CLEAR_HANDLE_STATUS_COMPLETED:
			break;
		default:
			ERR("Failed to wait for the completion of clear for session \"%s\"",
					session->name);
			ret = -1;
			goto error;
		}
	} while (status == LTTNG_CLEAR_HANDLE_STATUS_TIMEOUT);

	status = lttng_clear_handle_get_result(handle, &ret_code);
	if (status != LTTNG_CLEAR_HANDLE_STATUS_OK) {
		ERR("Failed to get the result of session clear");
		ret = -1;
		goto error;
	}
	if (ret_code != LTTNG_OK) {
		ret = -ret_code;
		goto error;
	}

	MSG("%sSession \"%s\" cleared", printed_wait_msg ? "\n" : "",
			session->name);
	printed_wait_msg = false;

	if (lttng_opt_mi) {
		ret = mi_lttng_session(writer, session, 0);
		if (ret) {
			ret = CMD_ERROR;
			goto error;
		}
	}

	ret = CMD_SUCCESS;
error:
	if (printed_wait_msg) {
		MSG("");
	}
	lttng_clear_handle_destroy(handle);
	free(session_name);
	return ret;
}

/*
 * clear all sessions
 *
 * Call clear_session for each registered sessions
 */
static int clear_all_sessions(struct lttng_session *sessions, int count)
{
	int i, ret = CMD_SUCCESS;

	if (count == 0) {
		MSG("No session found, nothing to do.");
	} else if (count < 0) {
		ERR("%s", lttng_strerror(ret));
		goto error;
	}

	for (i = 0; i < count; i++) {
		ret = clear_session(&sessions[i]);
		if (ret < 0) {
			goto error;
		}
	}
error:
	return ret;
}

/*
 * The 'clear <options>' first level command
 */
int cmd_clear(int argc, const char **argv)
{
	int opt;
	int ret = CMD_SUCCESS , i, command_ret = CMD_SUCCESS, success = 1;
	static poptContext pc;
	char *session_name = NULL;
	const char *leftover = NULL;
	bool free_session_name = false;
	struct lttng_session *sessions = NULL;
	int count;
	int found;

	pc = poptGetContext(NULL, argc, argv, long_options, 0);
	poptReadDefaultConfig(pc, 0);

	while ((opt = poptGetNextOpt(pc)) != -1) {
		switch (opt) {
		case OPT_HELP:
			SHOW_HELP();
			break;
		case OPT_LIST_OPTIONS:
			list_cmd_options(stdout, long_options);
			break;
		default:
			ret = CMD_UNDEFINED;
			break;
		}
		goto end;
	}

	/* Mi preparation */
	if (lttng_opt_mi) {
		writer = mi_lttng_writer_create(fileno(stdout), lttng_opt_mi);
		if (!writer) {
			ret = -LTTNG_ERR_NOMEM;
			goto end;
		}

		/* Open command element */
		ret = mi_lttng_writer_command_open(writer,
				mi_lttng_element_command_clear);
		if (ret) {
			ret = CMD_ERROR;
			goto end;
		}

		/* Open output element */
		ret = mi_lttng_writer_open_element(writer,
				mi_lttng_element_command_output);
		if (ret) {
			ret = CMD_ERROR;
			goto end;
		}

		/* For validation and semantic purpose we open a sessions element */
		ret = mi_lttng_sessions_open(writer);
		if (ret) {
			ret = CMD_ERROR;
			goto end;
		}
	}

	if (!opt_clear_all) {
		session_name = (char *) poptGetArg(pc);
		if (!session_name) {
			/* No session name specified, lookup default */
			session_name = get_session_name();
			if (session_name == NULL) {
				command_ret = CMD_ERROR;
				success = 0;
				goto mi_closing;
			}
			free_session_name = true;
		}
	} else {
		session_name = NULL;
	}

	leftover = poptGetArg(pc);
	if (leftover) {
		ERR("Unknown argument: %s", leftover);
		ret = CMD_ERROR;
		success = 0;
		goto mi_closing;
	}

	/* Recuperate all sessions for further operation */
	count = lttng_list_sessions(&sessions);
	if (count < 0) {
		ERR("%s", lttng_strerror(count));
		command_ret = CMD_ERROR;
		success = 0;
		goto mi_closing;
	}

	/* Ignore session name in case all sessions are to be cleaned */
	if (opt_clear_all) {
		command_ret = clear_all_sessions(sessions, count);
		if (command_ret) {
			ERR("%s", lttng_strerror(command_ret));
			success = 0;
		}
	} else {
		/* Find the corresponding lttng_session struct */
		found = 0;
		for (i = 0; i < count; i++) {
			if (strncmp(sessions[i].name, session_name, NAME_MAX) == 0) {
				found = 1;
				command_ret = clear_session(&sessions[i]);
				if (command_ret) {
					ERR("%s", lttng_strerror(command_ret));
					success = 0;
				}
			}
		}

		if (!found) {
			ERR("Session name %s not found", session_name);
			command_ret = LTTNG_ERR_SESS_NOT_FOUND;
			success = 0;
			goto mi_closing;
		}
	}

mi_closing:
	/* Mi closing */
	if (lttng_opt_mi) {
		/* Close sessions and output element element */
		ret = mi_lttng_close_multi_element(writer, 2);
		if (ret) {
			ret = CMD_ERROR;
			goto end;
		}

		/* Success ? */
		ret = mi_lttng_writer_write_element_bool(writer,
				mi_lttng_element_command_success, success);
		if (ret) {
			ret = CMD_ERROR;
			goto end;
		}

		/* Command element close */
		ret = mi_lttng_writer_command_close(writer);
		if (ret) {
			ret = CMD_ERROR;
			goto end;
		}
	}
end:
	/* Mi clean-up */
	if (writer && mi_lttng_writer_destroy(writer)) {
		/* Preserve original error code */
		ret = ret ? ret : -LTTNG_ERR_MI_IO_FAIL;
	}

	free(sessions);
	if (free_session_name) {
		free(session_name);
	}

	/* Overwrite ret if an error occurred during clear_session/all */
	ret = command_ret ? command_ret : ret;

	poptFreeContext(pc);
	return ret;
}
