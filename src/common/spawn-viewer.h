#ifndef LTTNG_SPAWN_VIEWER_H
#define LTTNG_SPAWN_VIEWER_H

/*
 * Copyright (C) 2020 Francis Deslauriers <francis.deslauriers@efficios.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 */

#include <stdbool.h>

/*
 * Read the trace by `exec()ing` the provided viewer program if any. If
 * `opt_viewer` is NULL, try to read the trace with the default trace reader.
 * On success, this function doesn't return.
 * Returns -1 if the `opt_viewer` string or the default trace viewer can't be
 * `exec()`.
 */
int spawn_viewer(const char *trace_path, char *opt_viewer, bool opt_live_mode);

#endif /* ifndef LTTNG_SPAWN_VIEWER_H */
