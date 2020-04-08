/*
 * Copyright (C) 2019 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 */

#ifndef LTTNG_SESSION_INTERNAL_H
#define LTTNG_SESSION_INTERNAL_H

#include <lttng/constant.h>
#include <common/macros.h>

struct lttng_session_extended {
	struct {
		uint64_t value;
		uint8_t is_set;
	} creation_time LTTNG_PACKED;
} LTTNG_PACKED;

#endif /* LTTNG_SESSION_INTERNAL_H */
