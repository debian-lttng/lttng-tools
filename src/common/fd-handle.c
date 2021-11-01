/*
 * Copyright (C) 2020 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 */

#include <unistd.h>
#include <urcu/ref.h>

#include "fd-handle.h"
#include <common/error.h>

struct fd_handle {
	struct urcu_ref ref;
	int fd;
};

static void fd_handle_release(struct urcu_ref *ref)
{
	int ret;
	struct fd_handle *handle = container_of(ref, struct fd_handle, ref);

	assert(handle->fd >= 0);
	ret = close(handle->fd);
	if (ret == -1) {
		PERROR("Failed to close file descriptor of fd_handle upon release: fd = %d",
				handle->fd);
	}

	free(handle);
}

LTTNG_HIDDEN
struct fd_handle *fd_handle_create(int fd)
{
	struct fd_handle *handle = NULL;

	if (fd < 0) {
		ERR("Attempted to create an fd_handle from an invalid file descriptor: fd = %d",
				fd);
		goto end;
	}

	handle = zmalloc(sizeof(*handle));
	if (!handle) {
		PERROR("Failed to allocate fd_handle");
		goto end;
	}

	urcu_ref_init(&handle->ref);
	handle->fd = fd;

end:
	return handle;
}

LTTNG_HIDDEN
void fd_handle_get(struct fd_handle *handle)
{
	if (!handle) {
		return;
	}

	urcu_ref_get(&handle->ref);
}

LTTNG_HIDDEN
void fd_handle_put(struct fd_handle *handle)
{
	if (!handle) {
		return;
	}

	urcu_ref_put(&handle->ref, fd_handle_release);
}

LTTNG_HIDDEN
int fd_handle_get_fd(struct fd_handle *handle)
{
	assert(handle);
	return handle->fd;
}

LTTNG_HIDDEN
struct fd_handle *fd_handle_copy(const struct fd_handle *handle)
{
	struct fd_handle *new_handle = NULL;
	const int new_fd = dup(handle->fd);

	if (new_fd < 0) {
		PERROR("Failed to duplicate file descriptor while copying fd_handle: fd = %d", handle->fd);
		goto end;
	}

	new_handle = fd_handle_create(new_fd);
end:
	return new_handle;
}
