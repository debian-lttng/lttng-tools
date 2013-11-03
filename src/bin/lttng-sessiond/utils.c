/*
 * Copyright (C) 2011 - David Goulet <david.goulet@polymtl.ca>
 *                      Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2 only,
 * as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#define _GNU_SOURCE
#include <stdlib.h>
#include <unistd.h>

#include <common/error.h>

#include "utils.h"
#include "lttng-sessiond.h"

int ht_cleanup_pipe[2] = { -1, -1 };

/*
 * Write to writable pipe used to notify a thread.
 */
int notify_thread_pipe(int wpipe)
{
	int ret;

	/* Ignore if the pipe is invalid. */
	if (wpipe < 0) {
		return 0;
	}

	do {
		ret = write(wpipe, "!", 1);
	} while (ret < 0 && errno == EINTR);
	if (ret < 0) {
		PERROR("write poll pipe");
	}

	return ret;
}

void ht_cleanup_push(struct lttng_ht *ht)
{
	int ret;
	int fd = ht_cleanup_pipe[1];

	if (fd < 0)
		return;
	do {
		ret = write(fd, &ht, sizeof(ht));
	} while (ret < 0 && errno == EINTR);
	if (ret < 0 || ret != sizeof(ht)) {
		PERROR("write ht cleanup pipe %d", fd);
		if (ret < 0) {
			ret = -errno;
		}
		goto error;
	}

	/* All good. Don't send back the write positive ret value. */
	ret = 0;
error:
	assert(!ret);
}
