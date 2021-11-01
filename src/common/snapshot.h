/*
 * Copyright (C) 2020 Simon Marchi <simon.marchi@efficios.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 */

#ifndef COMMON_SNAPSHOT_H
#define COMMON_SNAPSHOT_H

#include <common/macros.h>

#include <stdbool.h>
#include <sys/types.h>

struct lttng_payload_view;
struct lttng_payload;
struct lttng_snapshot_output;
struct mi_writer;

LTTNG_HIDDEN
bool lttng_snapshot_output_validate(const struct lttng_snapshot_output *output);

LTTNG_HIDDEN
bool lttng_snapshot_output_is_equal(
		const struct lttng_snapshot_output *a,
		const struct lttng_snapshot_output *b);

LTTNG_HIDDEN
int lttng_snapshot_output_serialize(
		const struct lttng_snapshot_output *output,
		struct lttng_payload *payload);

LTTNG_HIDDEN
ssize_t lttng_snapshot_output_create_from_payload(
		struct lttng_payload_view *view,
		struct lttng_snapshot_output **output_p);

LTTNG_HIDDEN
enum lttng_error_code lttng_snapshot_output_mi_serialize(
		const struct lttng_snapshot_output *output,
		struct mi_writer *writer);

#endif /* COMMON_SNAPSHOT_H */
