/*
 * Copyright (C) 2019 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 */

#ifndef SESSIOND_TRACE_CHUNK_REGISTRY_H
#define SESSIOND_TRACE_CHUNK_REGISTRY_H

#include <common/uuid.h>
#include <common/trace-chunk.h>
#include <stdint.h>

struct sessiond_trace_chunk_registry;

struct sessiond_trace_chunk_registry *
sessiond_trace_chunk_registry_create(void);

void sessiond_trace_chunk_registry_destroy(
		struct sessiond_trace_chunk_registry *sessiond_registry);

int sessiond_trace_chunk_registry_session_created(
		struct sessiond_trace_chunk_registry *sessiond_registry,
		const lttng_uuid sessiond_uuid);

int sessiond_trace_chunk_registry_session_destroyed(
		struct sessiond_trace_chunk_registry *sessiond_registry,
		const lttng_uuid sessiond_uuid);

struct lttng_trace_chunk *sessiond_trace_chunk_registry_publish_chunk(
		struct sessiond_trace_chunk_registry *sessiond_registry,
		const lttng_uuid sessiond_uuid, uint64_t session_id,
		struct lttng_trace_chunk *chunk);

struct lttng_trace_chunk *
sessiond_trace_chunk_registry_get_anonymous_chunk(
		struct sessiond_trace_chunk_registry *sessiond_registry,
		const lttng_uuid sessiond_uuid,
		uint64_t session_id);

struct lttng_trace_chunk *
sessiond_trace_chunk_registry_get_chunk(
		struct sessiond_trace_chunk_registry *sessiond_registry,
		const lttng_uuid sessiond_uuid,
		uint64_t session_id, uint64_t chunk_id);

int sessiond_trace_chunk_registry_chunk_exists(
		struct sessiond_trace_chunk_registry *sessiond_registry,
		const lttng_uuid sessiond_uuid,
		uint64_t session_id, uint64_t chunk_id, bool *chunk_exists);

#endif /* SESSIOND_TRACE_CHUNK_REGISTRY_H */
