#ifndef _RUNAS_H
#define _RUNAS_H

/*
 * Copyright (C) 2011 - David Goulet <david.goulet@polymtl.ca>
 *                      Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
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

#include <unistd.h>
#include <pthread.h>

int run_as_mkdir_recursive(const char *path, mode_t mode, uid_t uid, gid_t gid);
int run_as_mkdir(const char *path, mode_t mode, uid_t uid, gid_t gid);
int run_as_open(const char *path, int flags, mode_t mode, uid_t uid, gid_t gid);

/*
 * We need to lock pthread exit, which deadlocks __nptl_setxid in the
 * clone.
 */
extern pthread_mutex_t lttng_libc_state_lock;

#endif /* _RUNAS_H */
