/*
 * Copyright (C) 2011 EfficiOS Inc.
 * Copyright (C) 2011 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 */

#ifndef _LTTNG_USTCONSUMER_H
#define _LTTNG_USTCONSUMER_H

#include <common/compat/errno.h>
#include <common/consumer/consumer.h>
#include <stdbool.h>

#ifdef HAVE_LIBLTTNG_UST_CTL

int lttng_ustconsumer_take_snapshot(struct lttng_consumer_stream *stream);
int lttng_ustconsumer_sample_snapshot_positions(
		struct lttng_consumer_stream *stream);

int lttng_ustconsumer_get_produced_snapshot(
		struct lttng_consumer_stream *stream, unsigned long *pos);
int lttng_ustconsumer_get_consumed_snapshot(
		struct lttng_consumer_stream *stream, unsigned long *pos);

int lttng_ustconsumer_recv_cmd(struct lttng_consumer_local_data *ctx,
		int sock, struct pollfd *consumer_sockpoll);

extern int lttng_ustconsumer_allocate_channel(struct lttng_consumer_channel *chan);
extern void lttng_ustconsumer_del_channel(struct lttng_consumer_channel *chan);
extern void lttng_ustconsumer_free_channel(struct lttng_consumer_channel *chan);
extern int lttng_ustconsumer_add_stream(struct lttng_consumer_stream *stream);
extern void lttng_ustconsumer_del_stream(struct lttng_consumer_stream *stream);

int lttng_ustconsumer_read_subbuffer(struct lttng_consumer_stream *stream,
		struct lttng_consumer_local_data *ctx);
int lttng_ustconsumer_on_recv_stream(struct lttng_consumer_stream *stream);

void lttng_ustconsumer_on_stream_hangup(struct lttng_consumer_stream *stream);

int lttng_ust_flush_buffer(struct lttng_consumer_stream *stream,
		int producer_active);
int lttng_ustconsumer_get_stream_id(struct lttng_consumer_stream *stream,
		uint64_t *stream_id);
int lttng_ustconsumer_data_pending(struct lttng_consumer_stream *stream);
void lttng_ustconsumer_close_all_metadata(struct lttng_ht *ht);
void lttng_ustconsumer_close_metadata(struct lttng_consumer_channel *metadata);
void lttng_ustconsumer_close_stream_wakeup(struct lttng_consumer_stream *stream);
int lttng_ustconsumer_recv_metadata(int sock, uint64_t key, uint64_t offset,
		uint64_t len, uint64_t version,
		struct lttng_consumer_channel *channel, bool invoked_by_timer,
		int wait);
int lttng_ustconsumer_request_metadata(struct lttng_consumer_local_data *ctx,
		struct lttng_consumer_channel *channel, bool invoked_by_timer,
		int wait);
enum sync_metadata_status lttng_ustconsumer_sync_metadata(
		struct lttng_consumer_local_data *ctx,
		struct lttng_consumer_stream *metadata);
int lttng_ustconsumer_flush_buffer(struct lttng_consumer_stream *stream,
		int producer);
int lttng_ustconsumer_clear_buffer(struct lttng_consumer_stream *stream);
int lttng_ustconsumer_get_current_timestamp(
		struct lttng_consumer_stream *stream, uint64_t *ts);
int lttng_ustconsumer_get_sequence_number(
		struct lttng_consumer_stream *stream, uint64_t *seq);
void lttng_ustconsumer_sigbus_handle(void *addr);

#else /* HAVE_LIBLTTNG_UST_CTL */

static inline
ssize_t lttng_ustconsumer_on_read_subbuffer_mmap(
		struct lttng_consumer_local_data *ctx,
		struct lttng_consumer_stream *stream, unsigned long len,
		unsigned long padding)
{
	return -ENOSYS;
}

static inline
ssize_t lttng_ustconsumer_on_read_subbuffer_splice(
		struct lttng_consumer_local_data *ctx,
		struct lttng_consumer_stream *uststream, unsigned long len,
		unsigned long padding)
{
	return -ENOSYS;
}

static inline
int lttng_ustconsumer_take_snapshot(struct lttng_consumer_stream *stream)
{
	return -ENOSYS;
}

static inline
int lttng_ustconsumer_sample_snapshot_positions(
		struct lttng_consumer_stream *stream)
{
	return -ENOSYS;
}

static inline
int lttng_ustconsumer_get_produced_snapshot(
		struct lttng_consumer_stream *stream, unsigned long *pos)
{
	return -ENOSYS;
}

static inline
int lttng_ustconsumer_get_consumed_snapshot(
		struct lttng_consumer_stream *stream, unsigned long *pos)
{
	return -ENOSYS;
}

static inline
int lttng_ustconsumer_recv_cmd(struct lttng_consumer_local_data *ctx,
		int sock, struct pollfd *consumer_sockpoll)
{
	return -ENOSYS;
}

static inline
int lttng_ustconsumer_allocate_channel(struct lttng_consumer_channel *chan)
{
	return -ENOSYS;
}

static inline
void lttng_ustconsumer_del_channel(struct lttng_consumer_channel *chan)
{
}

static inline
void lttng_ustconsumer_free_channel(struct lttng_consumer_channel *chan)
{
}

static inline
int lttng_ustconsumer_add_stream(struct lttng_consumer_stream *stream)
{
	return -ENOSYS;
}

static inline
void lttng_ustconsumer_del_stream(struct lttng_consumer_stream *stream)
{
}

static inline
int lttng_ustconsumer_read_subbuffer(struct lttng_consumer_stream *stream,
		struct lttng_consumer_local_data *ctx)
{
	return -ENOSYS;
}

static inline
int lttng_ustconsumer_on_recv_stream(struct lttng_consumer_stream *stream)
{
	return -ENOSYS;
}

static inline
void lttng_ustconsumer_on_stream_hangup(struct lttng_consumer_stream *stream)
{
}

static inline
int lttng_ustconsumer_data_pending(struct lttng_consumer_stream *stream)
{
	return -ENOSYS;
}
static inline
int lttng_ust_flush_buffer(struct lttng_consumer_stream *stream,
		int producer_active)
{
	return -ENOSYS;
}
static inline
void lttng_ustconsumer_close_all_metadata(struct lttng_ht *ht)
{
}
static inline
void lttng_ustconsumer_close_metadata(struct lttng_consumer_channel *metadata)
{
}
static inline
void lttng_ustconsumer_close_stream_wakeup(struct lttng_consumer_stream *stream)
{
}
static inline
int lttng_ustconsumer_recv_metadata(int sock, uint64_t key, uint64_t offset,
		uint64_t len, uint64_t version,
		struct lttng_consumer_channel *channel, int timer)
{
	return -ENOSYS;
}
static inline
int lttng_ustconsumer_request_metadata(struct lttng_consumer_local_data *ctx,
		struct lttng_consumer_channel *channel, int timer, int wait)
{
	return -ENOSYS;
}
static inline
enum sync_metadata_status lttng_ustconsumer_sync_metadata(struct lttng_consumer_local_data *ctx,
		struct lttng_consumer_stream *metadata)
{
	return SYNC_METADATA_STATUS_ERROR;
}
static inline
int lttng_ustconsumer_flush_buffer(struct lttng_consumer_stream *stream,
		int producer)
{
	return -ENOSYS;
}
static inline
int lttng_ustconsumer_clear_buffer(struct lttng_consumer_stream *stream)
{
	return -ENOSYS;
}
static inline
int lttng_ustconsumer_get_current_timestamp(
		struct lttng_consumer_stream *stream, uint64_t *ts)
{
	return -ENOSYS;
}
static inline
int lttng_ustconsumer_get_sequence_number(
		struct lttng_consumer_stream *stream, uint64_t *seq)
{
	return -ENOSYS;
}
static inline
int lttng_ustconsumer_get_stream_id(struct lttng_consumer_stream *stream,
		uint64_t *stream_id)
{
	return -ENOSYS;
}
static inline
void lttng_ustconsumer_sigbus_handle(void *addr)
{
}
#endif /* HAVE_LIBLTTNG_UST_CTL */

#endif /* _LTTNG_USTCONSUMER_H */
