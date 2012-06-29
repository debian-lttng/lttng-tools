/*
 * liblttngctl.c
 *
 * Linux Trace Toolkit Control Library
 *
 * Copyright (C) 2011 David Goulet <david.goulet@polymtl.ca>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License, version 2.1 only,
 * as published by the Free Software Foundation.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#define _GNU_SOURCE
#include <grp.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <common/common.h>
#include <common/defaults.h>
#include <common/sessiond-comm/sessiond-comm.h>
#include <lttng/lttng.h>

/* Socket to session daemon for communication */
static int sessiond_socket;
static char sessiond_sock_path[PATH_MAX];

/* Variables */
static char *tracing_group;
static int connected;

/* Global */

/*
 * Those two variables are used by error.h to silent or control the verbosity of
 * error message. They are global to the library so application linking with it
 * are able to compile correctly and also control verbosity of the library.
 *
 * Note that it is *not* possible to silent ERR() and PERROR() macros.
 */
int lttng_opt_quiet;
int lttng_opt_verbose;

/*
 * Copy string from src to dst and enforce null terminated byte.
 */
static void copy_string(char *dst, const char *src, size_t len)
{
	if (src && dst) {
		strncpy(dst, src, len);
		/* Enforce the NULL terminated byte */
		dst[len - 1] = '\0';
	} else if (dst) {
		dst[0] = '\0';
	}
}

/*
 * Copy domain to lttcomm_session_msg domain.
 *
 * If domain is unknown, default domain will be the kernel.
 */
static void copy_lttng_domain(struct lttng_domain *dst, struct lttng_domain *src)
{
	if (src && dst) {
		switch (src->type) {
			case LTTNG_DOMAIN_KERNEL:
			case LTTNG_DOMAIN_UST:
			/*
			case LTTNG_DOMAIN_UST_EXEC_NAME:
			case LTTNG_DOMAIN_UST_PID:
			case LTTNG_DOMAIN_UST_PID_FOLLOW_CHILDREN:
			*/
				memcpy(dst, src, sizeof(struct lttng_domain));
				break;
			default:
				memset(dst, 0, sizeof(struct lttng_domain));
				dst->type = LTTNG_DOMAIN_KERNEL;
				break;
		}
	}
}

/*
 * Send lttcomm_session_msg to the session daemon.
 *
 * On success, returns the number of bytes sent (>=0)
 * On error, returns -1
 */
static int send_session_msg(struct lttcomm_session_msg *lsm)
{
	int ret;

	if (!connected) {
		ret = -ENOTCONN;
		goto end;
	}

	ret = lttcomm_send_creds_unix_sock(sessiond_socket, lsm,
			sizeof(struct lttcomm_session_msg));

end:
	return ret;
}

/*
 * Receive data from the sessiond socket.
 *
 * On success, returns the number of bytes received (>=0)
 * On error, returns -1 (recvmsg() error) or -ENOTCONN
 */
static int recv_data_sessiond(void *buf, size_t len)
{
	int ret;

	if (!connected) {
		ret = -ENOTCONN;
		goto end;
	}

	ret = lttcomm_recv_unix_sock(sessiond_socket, buf, len);

end:
	return ret;
}

/*
 *  Check if we are in the specified group.
 *
 *  If yes return 1, else return -1.
 */
static int check_tracing_group(const char *grp_name)
{
	struct group *grp_tracing;	/* no free(). See getgrnam(3) */
	gid_t *grp_list;
	int grp_list_size, grp_id, i;
	int ret = -1;

	/* Get GID of group 'tracing' */
	grp_tracing = getgrnam(grp_name);
	if (!grp_tracing) {
		/* If grp_tracing is NULL, the group does not exist. */
		goto end;
	}

	/* Get number of supplementary group IDs */
	grp_list_size = getgroups(0, NULL);
	if (grp_list_size < 0) {
		perror("getgroups");
		goto end;
	}

	/* Alloc group list of the right size */
	grp_list = malloc(grp_list_size * sizeof(gid_t));
	if (!grp_list) {
		perror("malloc");
		goto end;
	}
	grp_id = getgroups(grp_list_size, grp_list);
	if (grp_id < 0) {
		perror("getgroups");
		goto free_list;
	}

	for (i = 0; i < grp_list_size; i++) {
		if (grp_list[i] == grp_tracing->gr_gid) {
			ret = 1;
			break;
		}
	}

free_list:
	free(grp_list);

end:
	return ret;
}

/*
 * Try connect to session daemon with sock_path.
 *
 * Return 0 on success, else -1
 */
static int try_connect_sessiond(const char *sock_path)
{
	int ret;

	/* If socket exist, we check if the daemon listens for connect. */
	ret = access(sock_path, F_OK);
	if (ret < 0) {
		/* Not alive */
		return -1;
	}

	ret = lttcomm_connect_unix_sock(sock_path);
	if (ret < 0) {
		/* Not alive */
		return -1;
	}

	ret = lttcomm_close_unix_sock(ret);
	if (ret < 0) {
		perror("lttcomm_close_unix_sock");
	}

	return 0;
}

/*
 * Set sessiond socket path by putting it in the global
 * sessiond_sock_path variable.
 * Returns 0 on success,
 * -ENOMEM on failure (the sessiond socket path is somehow too long)
 */
static int set_session_daemon_path(void)
{
	int ret;
	int in_tgroup = 0;	/* In tracing group */
	uid_t uid;

	uid = getuid();

	if (uid != 0) {
		/* Are we in the tracing group ? */
		in_tgroup = check_tracing_group(tracing_group);
	}

	if ((uid == 0) || in_tgroup) {
		copy_string(sessiond_sock_path, DEFAULT_GLOBAL_CLIENT_UNIX_SOCK,
				sizeof(sessiond_sock_path));
	}

	if (uid != 0) {
		if (in_tgroup) {
			/* Tracing group */
			ret = try_connect_sessiond(sessiond_sock_path);
			if (ret >= 0) {
				goto end;
			}
			/* Global session daemon not available... */
		}
		/* ...or not in tracing group (and not root), default */

		/*
		 * With GNU C <  2.1, snprintf returns -1 if the target buffer is too small;
		 * With GNU C >= 2.1, snprintf returns the required size (excluding closing null)
		 */
		ret = snprintf(sessiond_sock_path, sizeof(sessiond_sock_path),
				DEFAULT_HOME_CLIENT_UNIX_SOCK, getenv("HOME"));
		if ((ret < 0) || (ret >= sizeof(sessiond_sock_path))) {
			return -ENOMEM;
		}
	}
end:
	return 0;
}

/*
 *  Connect to the LTTng session daemon.
 *
 *  On success, return 0. On error, return -1.
 */
static int connect_sessiond(void)
{
	int ret;

	ret = set_session_daemon_path();
	if (ret < 0) {
		return -1; /* set_session_daemon_path() returns -ENOMEM */
	}

	/* Connect to the sesssion daemon */
	ret = lttcomm_connect_unix_sock(sessiond_sock_path);
	if (ret < 0) {
		return ret;
	}

	sessiond_socket = ret;
	connected = 1;

	return 0;
}

/*
 *  Clean disconnect from the session daemon.
 *  On success, return 0. On error, return -1.
 */
static int disconnect_sessiond(void)
{
	int ret = 0;

	if (connected) {
		ret = lttcomm_close_unix_sock(sessiond_socket);
		sessiond_socket = 0;
		connected = 0;
	}

	return ret;
}

/*
 * Ask the session daemon a specific command and put the data into buf.
 *
 * Return size of data (only payload, not header) or a negative error code.
 */
static int ask_sessiond(struct lttcomm_session_msg *lsm, void **buf)
{
	int ret;
	size_t size;
	void *data = NULL;
	struct lttcomm_lttng_msg llm;

	ret = connect_sessiond();
	if (ret < 0) {
		goto end;
	}

	/* Send command to session daemon */
	ret = send_session_msg(lsm);
	if (ret < 0) {
		goto end;
	}

	/* Get header from data transmission */
	ret = recv_data_sessiond(&llm, sizeof(llm));
	if (ret < 0) {
		goto end;
	}

	/* Check error code if OK */
	if (llm.ret_code != LTTCOMM_OK) {
		ret = -llm.ret_code;
		goto end;
	}

	size = llm.data_size;
	if (size == 0) {
		/* If client free with size 0 */
		if (buf != NULL) {
			*buf = NULL;
		}
		ret = 0;
		goto end;
	}

	data = (void*) malloc(size);

	/* Get payload data */
	ret = recv_data_sessiond(data, size);
	if (ret < 0) {
		free(data);
		goto end;
	}

	/*
	 * Extra protection not to dereference a NULL pointer. If buf is NULL at
	 * this point, an error is returned and data is freed.
	 */
	if (buf == NULL) {
		ret = -1;
		free(data);
		goto end;
	}

	*buf = data;
	ret = size;

end:
	disconnect_sessiond();
	return ret;
}

/*
 * Create lttng handle and return pointer.
 * The returned pointer will be NULL in case of malloc() error.
 */
struct lttng_handle *lttng_create_handle(const char *session_name,
		struct lttng_domain *domain)
{
	struct lttng_handle *handle;

	handle = malloc(sizeof(struct lttng_handle));
	if (handle == NULL) {
		perror("malloc handle");
		goto end;
	}

	/* Copy session name */
	copy_string(handle->session_name, session_name,
			sizeof(handle->session_name));

	/* Copy lttng domain */
	copy_lttng_domain(&handle->domain, domain);

end:
	return handle;
}

/*
 * Destroy handle by free(3) the pointer.
 */
void lttng_destroy_handle(struct lttng_handle *handle)
{
	if (handle) {
		free(handle);
	}
}

/*
 * Register an outside consumer.
 * Returns size of returned session payload data or a negative error code.
 */
int lttng_register_consumer(struct lttng_handle *handle,
		const char *socket_path)
{
	struct lttcomm_session_msg lsm;

	lsm.cmd_type = LTTNG_REGISTER_CONSUMER;
	copy_string(lsm.session.name, handle->session_name,
			sizeof(lsm.session.name));
	copy_lttng_domain(&lsm.domain, &handle->domain);

	copy_string(lsm.u.reg.path, socket_path, sizeof(lsm.u.reg.path));

	return ask_sessiond(&lsm, NULL);
}

/*
 *  Start tracing for all traces of the session.
 *  Returns size of returned session payload data or a negative error code.
 */
int lttng_start_tracing(const char *session_name)
{
	struct lttcomm_session_msg lsm;

	if (session_name == NULL) {
		return -1;
	}

	lsm.cmd_type = LTTNG_START_TRACE;

	copy_string(lsm.session.name, session_name, sizeof(lsm.session.name));

	return ask_sessiond(&lsm, NULL);
}

/*
 *  Stop tracing for all traces of the session.
 *  Returns size of returned session payload data or a negative error code.
 */
int lttng_stop_tracing(const char *session_name)
{
	struct lttcomm_session_msg lsm;

	if (session_name == NULL) {
		return -1;
	}

	lsm.cmd_type = LTTNG_STOP_TRACE;

	copy_string(lsm.session.name, session_name, sizeof(lsm.session.name));

	return ask_sessiond(&lsm, NULL);
}

/*
 * Add context to event and/or channel.
 * If event_name is NULL, the context is applied to all events of the channel.
 * If channel_name is NULL, a lookup of the event's channel is done.
 * If both are NULL, the context is applied to all events of all channels.
 *
 * Returns the size of the returned payload data or a negative error code.
 */
int lttng_add_context(struct lttng_handle *handle,
		struct lttng_event_context *ctx, const char *event_name,
		const char *channel_name)
{
	struct lttcomm_session_msg lsm;

	/* Safety check. Both are mandatory */
	if (handle == NULL || ctx == NULL) {
		return -1;
	}

	memset(&lsm, 0, sizeof(lsm));

	lsm.cmd_type = LTTNG_ADD_CONTEXT;

	/* Copy channel name */
	copy_string(lsm.u.context.channel_name, channel_name,
			sizeof(lsm.u.context.channel_name));
	/* Copy event name */
	copy_string(lsm.u.context.event_name, event_name,
			sizeof(lsm.u.context.event_name));

	copy_lttng_domain(&lsm.domain, &handle->domain);

	memcpy(&lsm.u.context.ctx, ctx, sizeof(struct lttng_event_context));

	copy_string(lsm.session.name, handle->session_name,
			sizeof(lsm.session.name));

	return ask_sessiond(&lsm, NULL);
}

/*
 *  Enable event(s) for a channel.
 *  If no event name is specified, all events are enabled.
 *  If no channel name is specified, the default 'channel0' is used.
 *  Returns size of returned session payload data or a negative error code.
 */
int lttng_enable_event(struct lttng_handle *handle,
		struct lttng_event *ev, const char *channel_name)
{
	struct lttcomm_session_msg lsm;

	if (handle == NULL || ev == NULL) {
		return -1;
	}

	memset(&lsm, 0, sizeof(lsm));

	/* If no channel name, we put the default name */
	if (channel_name == NULL) {
		copy_string(lsm.u.enable.channel_name, DEFAULT_CHANNEL_NAME,
				sizeof(lsm.u.enable.channel_name));
	} else {
		copy_string(lsm.u.enable.channel_name, channel_name,
				sizeof(lsm.u.enable.channel_name));
	}

	copy_lttng_domain(&lsm.domain, &handle->domain);

	if (ev->name[0] != '\0') {
		lsm.cmd_type = LTTNG_ENABLE_EVENT;
	} else {
		lsm.cmd_type = LTTNG_ENABLE_ALL_EVENT;
	}
	memcpy(&lsm.u.enable.event, ev, sizeof(lsm.u.enable.event));

	copy_string(lsm.session.name, handle->session_name,
			sizeof(lsm.session.name));

	return ask_sessiond(&lsm, NULL);
}

/*
 *  Disable event(s) of a channel and domain.
 *  If no event name is specified, all events are disabled.
 *  If no channel name is specified, the default 'channel0' is used.
 *  Returns size of returned session payload data or a negative error code.
 */
int lttng_disable_event(struct lttng_handle *handle, const char *name,
		const char *channel_name)
{
	struct lttcomm_session_msg lsm;

	if (handle == NULL) {
		return -1;
	}

	memset(&lsm, 0, sizeof(lsm));

	if (channel_name) {
		copy_string(lsm.u.disable.channel_name, channel_name,
				sizeof(lsm.u.disable.channel_name));
	} else {
		copy_string(lsm.u.disable.channel_name, DEFAULT_CHANNEL_NAME,
				sizeof(lsm.u.disable.channel_name));
	}

	copy_lttng_domain(&lsm.domain, &handle->domain);

	if (name != NULL) {
		copy_string(lsm.u.disable.name, name, sizeof(lsm.u.disable.name));
		lsm.cmd_type = LTTNG_DISABLE_EVENT;
	} else {
		lsm.cmd_type = LTTNG_DISABLE_ALL_EVENT;
	}

	copy_string(lsm.session.name, handle->session_name,
			sizeof(lsm.session.name));

	return ask_sessiond(&lsm, NULL);
}

/*
 *  Enable channel per domain
 *  Returns size of returned session payload data or a negative error code.
 */
int lttng_enable_channel(struct lttng_handle *handle,
		struct lttng_channel *chan)
{
	struct lttcomm_session_msg lsm;

	/*
	 * NULL arguments are forbidden. No default values.
	 */
	if (handle == NULL || chan == NULL) {
		return -1;
	}

	memset(&lsm, 0, sizeof(lsm));

	memcpy(&lsm.u.channel.chan, chan, sizeof(lsm.u.channel.chan));

	lsm.cmd_type = LTTNG_ENABLE_CHANNEL;

	copy_lttng_domain(&lsm.domain, &handle->domain);

	copy_string(lsm.session.name, handle->session_name,
			sizeof(lsm.session.name));

	return ask_sessiond(&lsm, NULL);
}

/*
 *  All tracing will be stopped for registered events of the channel.
 *  Returns size of returned session payload data or a negative error code.
 */
int lttng_disable_channel(struct lttng_handle *handle, const char *name)
{
	struct lttcomm_session_msg lsm;

	/* Safety check. Both are mandatory */
	if (handle == NULL || name == NULL) {
		return -1;
	}

	memset(&lsm, 0, sizeof(lsm));

	lsm.cmd_type = LTTNG_DISABLE_CHANNEL;

	copy_string(lsm.u.disable.channel_name, name,
			sizeof(lsm.u.disable.channel_name));

	copy_lttng_domain(&lsm.domain, &handle->domain);

	copy_string(lsm.session.name, handle->session_name,
			sizeof(lsm.session.name));

	return ask_sessiond(&lsm, NULL);
}

/*
 *  Lists all available tracepoints of domain.
 *  Sets the contents of the events array.
 *  Returns the number of lttng_event entries in events;
 *  on error, returns a negative value.
 */
int lttng_list_tracepoints(struct lttng_handle *handle,
		struct lttng_event **events)
{
	int ret;
	struct lttcomm_session_msg lsm;

	if (handle == NULL) {
		return -1;
	}

	lsm.cmd_type = LTTNG_LIST_TRACEPOINTS;
	copy_lttng_domain(&lsm.domain, &handle->domain);

	ret = ask_sessiond(&lsm, (void **) events);
	if (ret < 0) {
		return ret;
	}

	return ret / sizeof(struct lttng_event);
}

/*
 *  Returns a human readable string describing
 *  the error code (a negative value).
 */
const char *lttng_strerror(int code)
{
	/* lttcomm error codes range from -LTTCOMM_OK down to -LTTCOMM_NR */
	if (code > -LTTCOMM_OK) {
		return "Ended with errors";
	}

	return lttcomm_get_readable_code(code);
}

/*
 *  Create a brand new session using name and path.
 *  Returns size of returned session payload data or a negative error code.
 */
int lttng_create_session(const char *name, const char *path)
{
	struct lttcomm_session_msg lsm;

	lsm.cmd_type = LTTNG_CREATE_SESSION;
	copy_string(lsm.session.name, name, sizeof(lsm.session.name));
	copy_string(lsm.session.path, path, sizeof(lsm.session.path));

	return ask_sessiond(&lsm, NULL);
}

/*
 *  Destroy session using name.
 *  Returns size of returned session payload data or a negative error code.
 */
int lttng_destroy_session(const char *session_name)
{
	struct lttcomm_session_msg lsm;

	if (session_name == NULL) {
		return -1;
	}

	lsm.cmd_type = LTTNG_DESTROY_SESSION;

	copy_string(lsm.session.name, session_name, sizeof(lsm.session.name));

	return ask_sessiond(&lsm, NULL);
}

/*
 *  Ask the session daemon for all available sessions.
 *  Sets the contents of the sessions array.
 *  Returns the number of lttng_session entries in sessions;
 *  on error, returns a negative value.
 */
int lttng_list_sessions(struct lttng_session **sessions)
{
	int ret;
	struct lttcomm_session_msg lsm;

	lsm.cmd_type = LTTNG_LIST_SESSIONS;
	ret = ask_sessiond(&lsm, (void**) sessions);
	if (ret < 0) {
		return ret;
	}

	return ret / sizeof(struct lttng_session);
}

/*
 *  Ask the session daemon for all available domains of a session.
 *  Sets the contents of the domains array.
 *  Returns the number of lttng_domain entries in domains;
 *  on error, returns a negative value.
 */
int lttng_list_domains(const char *session_name,
		struct lttng_domain **domains)
{
	int ret;
	struct lttcomm_session_msg lsm;

	if (session_name == NULL) {
		return -1;
	}

	lsm.cmd_type = LTTNG_LIST_DOMAINS;

	copy_string(lsm.session.name, session_name, sizeof(lsm.session.name));

	ret = ask_sessiond(&lsm, (void**) domains);
	if (ret < 0) {
		return ret;
	}

	return ret / sizeof(struct lttng_domain);
}

/*
 *  Ask the session daemon for all available channels of a session.
 *  Sets the contents of the channels array.
 *  Returns the number of lttng_channel entries in channels;
 *  on error, returns a negative value.
 */
int lttng_list_channels(struct lttng_handle *handle,
		struct lttng_channel **channels)
{
	int ret;
	struct lttcomm_session_msg lsm;

	if (handle == NULL) {
		return -1;
	}

	lsm.cmd_type = LTTNG_LIST_CHANNELS;
	copy_string(lsm.session.name, handle->session_name,
			sizeof(lsm.session.name));

	copy_lttng_domain(&lsm.domain, &handle->domain);

	ret = ask_sessiond(&lsm, (void**) channels);
	if (ret < 0) {
		return ret;
	}

	return ret / sizeof(struct lttng_channel);
}

/*
 *  Ask the session daemon for all available events of a session channel.
 *  Sets the contents of the events array.
 *  Returns the number of lttng_event entries in events;
 *  on error, returns a negative value.
 */
int lttng_list_events(struct lttng_handle *handle,
		const char *channel_name, struct lttng_event **events)
{
	int ret;
	struct lttcomm_session_msg lsm;

	/* Safety check. An handle and channel name are mandatory */
	if (handle == NULL || channel_name == NULL) {
		return -1;
	}

	lsm.cmd_type = LTTNG_LIST_EVENTS;
	copy_string(lsm.session.name, handle->session_name,
			sizeof(lsm.session.name));
	copy_string(lsm.u.list.channel_name, channel_name,
			sizeof(lsm.u.list.channel_name));

	copy_lttng_domain(&lsm.domain, &handle->domain);

	ret = ask_sessiond(&lsm, (void**) events);
	if (ret < 0) {
		return ret;
	}

	return ret / sizeof(struct lttng_event);
}

/*
 * Sets the tracing_group variable with name.
 * This function allocates memory pointed to by tracing_group.
 * On success, returns 0, on error, returns -1 (null name) or -ENOMEM.
 */
int lttng_set_tracing_group(const char *name)
{
	if (name == NULL) {
		return -1;
	}

	if (asprintf(&tracing_group, "%s", name) < 0) {
		return -ENOMEM;
	}

	return 0;
}

/*
 * Returns size of returned session payload data or a negative error code.
 */
int lttng_calibrate(struct lttng_handle *handle,
		struct lttng_calibrate *calibrate)
{
	struct lttcomm_session_msg lsm;

	/* Safety check. NULL pointer are forbidden */
	if (handle == NULL || calibrate == NULL) {
		return -1;
	}

	lsm.cmd_type = LTTNG_CALIBRATE;
	copy_lttng_domain(&lsm.domain, &handle->domain);

	memcpy(&lsm.u.calibrate, calibrate, sizeof(lsm.u.calibrate));

	return ask_sessiond(&lsm, NULL);
}

/*
 * Set default channel attributes.
 * If either or both of the arguments are null, attr content is zeroe'd.
 */
void lttng_channel_set_default_attr(struct lttng_domain *domain,
		struct lttng_channel_attr *attr)
{
	/* Safety check */
	if (attr == NULL || domain == NULL) {
		return;
	}

	memset(attr, 0, sizeof(struct lttng_channel_attr));

	switch (domain->type) {
	case LTTNG_DOMAIN_KERNEL:
		attr->overwrite = DEFAULT_CHANNEL_OVERWRITE;
		attr->switch_timer_interval = DEFAULT_CHANNEL_SWITCH_TIMER;
		attr->read_timer_interval = DEFAULT_CHANNEL_READ_TIMER;

		attr->subbuf_size = DEFAULT_KERNEL_CHANNEL_SUBBUF_SIZE;
		attr->num_subbuf = DEFAULT_KERNEL_CHANNEL_SUBBUF_NUM;
		attr->output = DEFAULT_KERNEL_CHANNEL_OUTPUT;
		break;
	case LTTNG_DOMAIN_UST:
#if 0
	case LTTNG_DOMAIN_UST_EXEC_NAME:
	case LTTNG_DOMAIN_UST_PID:
	case LTTNG_DOMAIN_UST_PID_FOLLOW_CHILDREN:
#endif
		attr->overwrite = DEFAULT_CHANNEL_OVERWRITE;
		attr->switch_timer_interval = DEFAULT_CHANNEL_SWITCH_TIMER;
		attr->read_timer_interval = DEFAULT_CHANNEL_READ_TIMER;

		attr->subbuf_size = DEFAULT_UST_CHANNEL_SUBBUF_SIZE;
		attr->num_subbuf = DEFAULT_UST_CHANNEL_SUBBUF_NUM;
		attr->output = DEFAULT_UST_CHANNEL_OUTPUT;
		break;
	default:
		/* Default behavior: leave set to 0. */
		break;
	}
}

/*
 * Check if session daemon is alive.
 *
 * Return 1 if alive or 0 if not.
 * On error returns a negative value.
 */
int lttng_session_daemon_alive(void)
{
	int ret;

	ret = set_session_daemon_path();
	if (ret < 0) {
		/* Error */
		return ret;
	}

	if (strlen(sessiond_sock_path) == 0) {
		/* No socket path set. Weird error */
		return -1;
	}

	ret = try_connect_sessiond(sessiond_sock_path);
	if (ret < 0) {
		/* Not alive */
		return 0;
	}

	/* Is alive */
	return 1;
}

/*
 * lib constructor
 */
static void __attribute__((constructor)) init()
{
	/* Set default session group */
	lttng_set_tracing_group(DEFAULT_TRACING_GROUP);
}
