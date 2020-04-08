/*
 * Copyright (C) 2009  Pierre-Marc Fournier
 * Copyright (C) 2011-2012  Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 */

#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <stdlib.h>

#define TRACEPOINT_DEFINE
#define TRACEPOINT_CREATE_PROBES
#include "ust_tests_daemon.h"

int main(int argc, char **argv, char *env[])
{
	int result;

	if (argc < 1) {
		fprintf(stderr, "usage: daemon\n");
		exit(1);
	}

	pid_t parent_pid = getpid();
	printf("parent_pid %d\n", parent_pid);
	tracepoint(ust_tests_daemon, before_daemon, parent_pid);

	result = daemon(0, 1);
	if (result == 0) {
		printf("child_pid %d\n", getpid());

		tracepoint(ust_tests_daemon, after_daemon_child, getpid());
	} else {
		tracepoint(ust_tests_daemon, after_daemon_parent);
		perror("daemon");
		exit(1);
	}

	return 0;
}
