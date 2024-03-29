/*
 * Copyright (C) 2011 EfficiOS Inc.
 * Copyright (C) 2011 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 * Copyright (C) 2013 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 */

#include "lttng-sessiond.h"
#include <common/uuid.h>

lttng_uuid the_sessiond_uuid;

int the_ust_consumerd64_fd = -1;
int the_ust_consumerd32_fd = -1;

long the_page_size;

struct health_app *the_health_sessiond;

struct notification_thread_handle *the_notification_thread_handle;

struct lttng_ht *the_agent_apps_ht_by_sock = NULL;
struct lttng_ht *the_trigger_agents_ht_by_domain = NULL;

struct lttng_kernel_abi_tracer_version the_kernel_tracer_version;
struct lttng_kernel_abi_tracer_abi_version the_kernel_tracer_abi_version;

int the_kernel_poll_pipe[2] = {-1, -1};

pid_t the_ppid;
pid_t the_child_ppid;

struct sessiond_config the_config;

struct consumer_data the_kconsumer_data = {
	.type = LTTNG_CONSUMER_KERNEL,
	.err_sock = -1,
	.cmd_sock = -1,
	.channel_monitor_pipe = -1,
	.pid_mutex = PTHREAD_MUTEX_INITIALIZER,
	.lock = PTHREAD_MUTEX_INITIALIZER,
};

struct consumer_data the_ustconsumer64_data = {
	.type = LTTNG_CONSUMER64_UST,
	.err_sock = -1,
	.cmd_sock = -1,
	.channel_monitor_pipe = -1,
	.pid_mutex = PTHREAD_MUTEX_INITIALIZER,
	.lock = PTHREAD_MUTEX_INITIALIZER,
};

struct consumer_data the_ustconsumer32_data = {
	.type = LTTNG_CONSUMER32_UST,
	.err_sock = -1,
	.cmd_sock = -1,
	.channel_monitor_pipe = -1,
	.pid_mutex = PTHREAD_MUTEX_INITIALIZER,
	.lock = PTHREAD_MUTEX_INITIALIZER,
};

enum consumerd_state the_ust_consumerd_state;
enum consumerd_state the_kernel_consumerd_state;

static void __attribute__((constructor)) init_sessiond_uuid(void)
{
	if (lttng_uuid_generate(the_sessiond_uuid)) {
		ERR("Failed to generate a session daemon UUID");
		abort();
	}
}
