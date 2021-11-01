/*
 * Copyright (C) 2021 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 */

#ifndef _LTTNG_LOGLEVEL_UTILS_H
#define _LTTNG_LOGLEVEL_UTILS_H

#include <lttng/lttng.h>
#include <common/macros.h>

LTTNG_HIDDEN
int loglevel_name_to_value(const char *name, enum lttng_loglevel *loglevel);

LTTNG_HIDDEN
bool loglevel_parse_range_string(const char *str,
		enum lttng_loglevel *min,
		enum lttng_loglevel *max);

LTTNG_HIDDEN
int loglevel_log4j_name_to_value(
		const char *name, enum lttng_loglevel_log4j *loglevel);

LTTNG_HIDDEN
bool loglevel_log4j_parse_range_string(const char *str,
		enum lttng_loglevel_log4j *min,
		enum lttng_loglevel_log4j *max);

LTTNG_HIDDEN
int loglevel_jul_name_to_value(
		const char *name, enum lttng_loglevel_jul *loglevel);

LTTNG_HIDDEN
bool loglevel_jul_parse_range_string(const char *str,
		enum lttng_loglevel_jul *min,
		enum lttng_loglevel_jul *max);

LTTNG_HIDDEN
int loglevel_python_name_to_value(
		const char *name, enum lttng_loglevel_python *loglevel);

LTTNG_HIDDEN
bool loglevel_python_parse_range_string(const char *str,
		enum lttng_loglevel_python *min,
		enum lttng_loglevel_python *max);

LTTNG_HIDDEN
const char *loglevel_value_to_name(int loglevel);

LTTNG_HIDDEN
const char *loglevel_log4j_value_to_name(int loglevel);

LTTNG_HIDDEN
const char *loglevel_jul_value_to_name(int loglevel);

LTTNG_HIDDEN
const char *loglevel_python_value_to_name(int loglevel);

#endif /* _LTTNG_LOGLEVEL_UTILS_H */
