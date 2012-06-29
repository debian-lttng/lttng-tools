/*
 * Copyright (C) 2011 - Julien Desfossez <julien.desfossez@polymtl.ca>
 *                      Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *                      David Goulet <david.goulet@polymtl.ca>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2 only,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef _LTTNG_KERNEL_H
#define _LTTNG_KERNEL_H

#include <stdint.h>

#define LTTNG_KERNEL_SYM_NAME_LEN  256

/*
 * LTTng DebugFS ABI structures.
 *
 * This is the kernel ABI copied from lttng-modules tree.
 */

enum lttng_kernel_instrumentation {
	LTTNG_KERNEL_ALL           = -1,   /* Used within lttng-tools */
	LTTNG_KERNEL_TRACEPOINT    = 0,
	LTTNG_KERNEL_KPROBE        = 1,
	LTTNG_KERNEL_FUNCTION      = 2,
	LTTNG_KERNEL_KRETPROBE     = 3,
	LTTNG_KERNEL_NOOP          = 4,    /* not hooked */
	LTTNG_KERNEL_SYSCALL       = 5,
};

enum lttng_kernel_context_type {
	LTTNG_KERNEL_CONTEXT_PID            = 0,
	LTTNG_KERNEL_CONTEXT_PERF_COUNTER   = 1,
	LTTNG_KERNEL_CONTEXT_PROCNAME       = 2,
	LTTNG_KERNEL_CONTEXT_PRIO           = 3,
	LTTNG_KERNEL_CONTEXT_NICE           = 4,
	LTTNG_KERNEL_CONTEXT_VPID           = 5,
	LTTNG_KERNEL_CONTEXT_TID            = 6,
	LTTNG_KERNEL_CONTEXT_VTID           = 7,
	LTTNG_KERNEL_CONTEXT_PPID           = 8,
	LTTNG_KERNEL_CONTEXT_VPPID          = 9,
};

/* Perf counter attributes */
struct lttng_kernel_perf_counter_ctx {
	uint32_t type;
	uint64_t config;
	char name[LTTNG_KERNEL_SYM_NAME_LEN];
};

/* Event/Channel context */
#define LTTNG_KERNEL_CONTEXT_PADDING1  16
#define LTTNG_KERNEL_CONTEXT_PADDING2  LTTNG_KERNEL_SYM_NAME_LEN + 32
struct lttng_kernel_context {
	enum lttng_kernel_context_type ctx;
	char padding[LTTNG_KERNEL_CONTEXT_PADDING1];

	union {
		struct lttng_kernel_perf_counter_ctx perf_counter;
		char padding[LTTNG_KERNEL_CONTEXT_PADDING2];
	} u;
};

struct lttng_kernel_kretprobe {
	uint64_t addr;

	uint64_t offset;
	char symbol_name[LTTNG_KERNEL_SYM_NAME_LEN];
};

/*
 * Either addr is used, or symbol_name and offset.
 */
struct lttng_kernel_kprobe {
	uint64_t addr;

	uint64_t offset;
	char symbol_name[LTTNG_KERNEL_SYM_NAME_LEN];
};

/* Function tracer */
struct lttng_kernel_function {
	char symbol_name[LTTNG_KERNEL_SYM_NAME_LEN];
};

#define LTTNG_KERNEL_EVENT_PADDING1    16
#define LTTNG_KERNEL_EVENT_PADDING2    LTTNG_KERNEL_SYM_NAME_LEN + 32
struct lttng_kernel_event {
	char name[LTTNG_KERNEL_SYM_NAME_LEN];
	enum lttng_kernel_instrumentation instrumentation;
	char padding[LTTNG_KERNEL_EVENT_PADDING1];

	/* Per instrumentation type configuration */
	union {
		struct lttng_kernel_kretprobe kretprobe;
		struct lttng_kernel_kprobe kprobe;
		struct lttng_kernel_function ftrace;
		char padding[LTTNG_KERNEL_EVENT_PADDING2];
	} u;
};

struct lttng_kernel_tracer_version {
	uint32_t major;
	uint32_t minor;
	uint32_t patchlevel;
};

enum lttng_kernel_calibrate_type {
	LTTNG_KERNEL_CALIBRATE_KRETPROBE,
};

struct lttng_kernel_calibrate {
	enum lttng_kernel_calibrate_type type;	/* type (input) */
};

#endif /* _LTTNG_KERNEL_H */
