/*
 * Copyright (C) 2017 Jérémie Galarneau <jeremie.galarneau@efficios.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 */

#include <lttng/notification/notification-internal.h>
#include <lttng/notification/channel-internal.h>
#include <lttng/condition/condition-internal.h>
#include <lttng/endpoint.h>
#include <common/defaults.h>
#include <common/error.h>
#include <common/dynamic-buffer.h>
#include <common/utils.h>
#include <common/defaults.h>
#include <common/payload.h>
#include <common/payload-view.h>
#include <common/unix.h>
#include <assert.h>
#include "lttng-ctl-helper.h"
#include <common/compat/poll.h>

static
int handshake(struct lttng_notification_channel *channel);

/*
 * Populates the reception buffer with the next complete message.
 * The caller must acquire the channel's lock.
 */
static
int receive_message(struct lttng_notification_channel *channel)
{
	ssize_t ret;
	struct lttng_notification_channel_message msg;

	lttng_payload_clear(&channel->reception_payload);

	ret = lttcomm_recv_unix_sock(channel->socket, &msg, sizeof(msg));
	if (ret <= 0) {
		ret = -1;
		goto error;
	}

	if (msg.size > DEFAULT_MAX_NOTIFICATION_CLIENT_MESSAGE_PAYLOAD_SIZE) {
		ret = -1;
		goto error;
	}

	/* Add message header at buffer's start. */
	ret = lttng_dynamic_buffer_append(&channel->reception_payload.buffer, &msg,
			sizeof(msg));
	if (ret) {
		goto error;
	}

	if (msg.size == 0) {
		goto skip_payload;
	}

	/* Reserve space for the payload. */
	ret = lttng_dynamic_buffer_set_size(&channel->reception_payload.buffer,
			channel->reception_payload.buffer.size + msg.size);
	if (ret) {
		goto error;
	}

	/* Receive message payload. */
	ret = lttcomm_recv_unix_sock(channel->socket,
			channel->reception_payload.buffer.data + sizeof(msg), msg.size);
	if (ret < (ssize_t) msg.size) {
		ret = -1;
		goto error;
	}

skip_payload:
	/* Receive message fds. */
	if (msg.fds != 0) {
		ret = lttcomm_recv_payload_fds_unix_sock(channel->socket,
				msg.fds, &channel->reception_payload);
		if (ret < sizeof(int) * msg.fds) {
			ret = -1;
			goto error;
		}
	}
	ret = 0;
end:
	return ret;
error:
	lttng_payload_clear(&channel->reception_payload);
	goto end;
}

static
enum lttng_notification_channel_message_type get_current_message_type(
		struct lttng_notification_channel *channel)
{
	struct lttng_notification_channel_message *msg;

	assert(channel->reception_payload.buffer.size >= sizeof(*msg));

	msg = (struct lttng_notification_channel_message *)
			channel->reception_payload.buffer.data;
	return (enum lttng_notification_channel_message_type) msg->type;
}

static
struct lttng_notification *create_notification_from_current_message(
		struct lttng_notification_channel *channel)
{
	ssize_t ret;
	struct lttng_notification *notification = NULL;

	if (channel->reception_payload.buffer.size <=
			sizeof(struct lttng_notification_channel_message)) {
		goto end;
	}

	{
		struct lttng_payload_view view = lttng_payload_view_from_payload(
				&channel->reception_payload,
				sizeof(struct lttng_notification_channel_message),
				-1);

		ret = lttng_notification_create_from_payload(
				&view, &notification);
	}

	if (ret != channel->reception_payload.buffer.size -
			sizeof(struct lttng_notification_channel_message)) {
		lttng_notification_destroy(notification);
		notification = NULL;
		goto end;
	}
end:
	return notification;
}

struct lttng_notification_channel *lttng_notification_channel_create(
		struct lttng_endpoint *endpoint)
{
	int fd, ret;
	bool is_in_tracing_group = false, is_root = false;
	char *sock_path = NULL;
	struct lttng_notification_channel *channel = NULL;

	if (!endpoint ||
			endpoint != lttng_session_daemon_notification_endpoint) {
		goto end;
	}

	sock_path = zmalloc(LTTNG_PATH_MAX);
	if (!sock_path) {
		goto end;
	}

	channel = zmalloc(sizeof(struct lttng_notification_channel));
	if (!channel) {
		goto end;
	}
	channel->socket = -1;
	pthread_mutex_init(&channel->lock, NULL);
	lttng_payload_init(&channel->reception_payload);
	CDS_INIT_LIST_HEAD(&channel->pending_notifications.list);

	is_root = (getuid() == 0);
	if (!is_root) {
		is_in_tracing_group = lttng_check_tracing_group();
	}

	if (is_root || is_in_tracing_group) {
		ret = lttng_strncpy(sock_path,
				DEFAULT_GLOBAL_NOTIFICATION_CHANNEL_UNIX_SOCK,
				LTTNG_PATH_MAX);
		if (ret) {
			ret = -LTTNG_ERR_INVALID;
			goto error;
		}

		ret = lttcomm_connect_unix_sock(sock_path);
		if (ret >= 0) {
			fd = ret;
			goto set_fd;
		}
	}

	/* Fallback to local session daemon. */
	ret = snprintf(sock_path, LTTNG_PATH_MAX,
			DEFAULT_HOME_NOTIFICATION_CHANNEL_UNIX_SOCK,
			utils_get_home_dir());
	if (ret < 0 || ret >= LTTNG_PATH_MAX) {
		goto error;
	}

	ret = lttcomm_connect_unix_sock(sock_path);
	if (ret < 0) {
		goto error;
	}
	fd = ret;

set_fd:
	channel->socket = fd;

	ret = handshake(channel);
	if (ret) {
		goto error;
	}
end:
	free(sock_path);
	return channel;
error:
	lttng_notification_channel_destroy(channel);
	channel = NULL;
	goto end;
}

enum lttng_notification_channel_status
lttng_notification_channel_get_next_notification(
		struct lttng_notification_channel *channel,
		struct lttng_notification **_notification)
{
	int ret;
	struct lttng_notification *notification = NULL;
	enum lttng_notification_channel_status status =
			LTTNG_NOTIFICATION_CHANNEL_STATUS_OK;
	struct lttng_poll_event events;

	if (!channel || !_notification) {
		status = LTTNG_NOTIFICATION_CHANNEL_STATUS_INVALID;
		goto end;
	}

	pthread_mutex_lock(&channel->lock);

	if (channel->pending_notifications.count) {
		struct pending_notification *pending_notification;

		assert(!cds_list_empty(&channel->pending_notifications.list));

		/* Deliver one of the pending notifications. */
		pending_notification = cds_list_first_entry(
				&channel->pending_notifications.list,
				struct pending_notification,
				node);
		notification = pending_notification->notification;
		if (!notification) {
			status = LTTNG_NOTIFICATION_CHANNEL_STATUS_NOTIFICATIONS_DROPPED;
		}
		cds_list_del(&pending_notification->node);
		channel->pending_notifications.count--;
		free(pending_notification);
		goto end_unlock;
	}

	/*
	 * Block on interruptible epoll/poll() instead of the message reception
	 * itself as the recvmsg() wrappers always restart on EINTR. We choose
	 * to wait using interruptible epoll/poll() in order to:
	 *   1) Return if a signal occurs,
	 *   2) Not deal with partially received messages.
	 *
	 * The drawback to this approach is that we assume that messages
	 * are complete/well formed. If a message is shorter than its
	 * announced length, receive_message() will block on recvmsg()
	 * and never return (even if a signal is received).
	 */
	ret = lttng_poll_create(&events, 1, LTTNG_CLOEXEC);
	if (ret < 0) {
		status = LTTNG_NOTIFICATION_CHANNEL_STATUS_ERROR;
		goto end_unlock;
	}
	ret = lttng_poll_add(&events, channel->socket, LPOLLIN | LPOLLERR);
	if (ret < 0) {
		status = LTTNG_NOTIFICATION_CHANNEL_STATUS_ERROR;
		goto end_clean_poll;
	}
	ret = lttng_poll_wait_interruptible(&events, -1);
	if (ret <= 0) {
		status = (ret == -1 && errno == EINTR) ?
			LTTNG_NOTIFICATION_CHANNEL_STATUS_INTERRUPTED :
			LTTNG_NOTIFICATION_CHANNEL_STATUS_ERROR;
		goto end_clean_poll;
	}

	ret = receive_message(channel);
	if (ret) {
		status = LTTNG_NOTIFICATION_CHANNEL_STATUS_ERROR;
		goto end_clean_poll;
	}

	switch (get_current_message_type(channel)) {
	case LTTNG_NOTIFICATION_CHANNEL_MESSAGE_TYPE_NOTIFICATION:
		notification = create_notification_from_current_message(
				channel);
		if (!notification) {
			status = LTTNG_NOTIFICATION_CHANNEL_STATUS_ERROR;
			goto end_clean_poll;
		}
		break;
	case LTTNG_NOTIFICATION_CHANNEL_MESSAGE_TYPE_NOTIFICATION_DROPPED:
		/* No payload to consume. */
		status = LTTNG_NOTIFICATION_CHANNEL_STATUS_NOTIFICATIONS_DROPPED;
		break;
	default:
		/* Protocol error. */
		status = LTTNG_NOTIFICATION_CHANNEL_STATUS_ERROR;
		goto end_clean_poll;
	}

end_clean_poll:
	lttng_poll_clean(&events);
end_unlock:
	pthread_mutex_unlock(&channel->lock);
	*_notification = notification;
end:
	return status;
}

static
int enqueue_dropped_notification(
		struct lttng_notification_channel *channel)
{
	int ret = 0;
	struct pending_notification *pending_notification;
	struct cds_list_head *last_element =
			channel->pending_notifications.list.prev;

	pending_notification = caa_container_of(last_element,
			struct pending_notification, node);
	if (!pending_notification->notification) {
		/*
		 * The last enqueued notification indicates dropped
		 * notifications; there is nothing to do as we group
		 * dropped notifications together.
		 */
		goto end;
	}

	if (channel->pending_notifications.count >=
			DEFAULT_CLIENT_MAX_QUEUED_NOTIFICATIONS_COUNT &&
			pending_notification->notification) {
		/*
		 * Discard the last enqueued notification to indicate
		 * that notifications were dropped at this point.
		 */
		lttng_notification_destroy(
				pending_notification->notification);
		pending_notification->notification = NULL;
		goto end;
	}

	pending_notification = zmalloc(sizeof(*pending_notification));
	if (!pending_notification) {
		ret = -1;
		goto end;
	}
	CDS_INIT_LIST_HEAD(&pending_notification->node);
	cds_list_add(&pending_notification->node,
			&channel->pending_notifications.list);
	channel->pending_notifications.count++;
end:
	return ret;
}

static
int enqueue_notification_from_current_message(
		struct lttng_notification_channel *channel)
{
	int ret = 0;
	struct lttng_notification *notification;
	struct pending_notification *pending_notification;

	if (channel->pending_notifications.count >=
			DEFAULT_CLIENT_MAX_QUEUED_NOTIFICATIONS_COUNT) {
		/* Drop the notification. */
		ret = enqueue_dropped_notification(channel);
		goto end;
	}

	pending_notification = zmalloc(sizeof(*pending_notification));
	if (!pending_notification) {
		ret = -1;
		goto error;
	}
	CDS_INIT_LIST_HEAD(&pending_notification->node);

	notification = create_notification_from_current_message(channel);
	if (!notification) {
		ret = -1;
		goto error;
	}

	pending_notification->notification = notification;
	cds_list_add(&pending_notification->node,
			&channel->pending_notifications.list);
	channel->pending_notifications.count++;
end:
	return ret;
error:
	free(pending_notification);
	goto end;
}

enum lttng_notification_channel_status
lttng_notification_channel_has_pending_notification(
		struct lttng_notification_channel *channel,
		bool *_notification_pending)
{
	int ret;
	enum lttng_notification_channel_status status =
			LTTNG_NOTIFICATION_CHANNEL_STATUS_OK;
	struct lttng_poll_event events;

	if (!channel || !_notification_pending) {
		status = LTTNG_NOTIFICATION_CHANNEL_STATUS_INVALID;
		goto end;
	}

	pthread_mutex_lock(&channel->lock);

	if (channel->pending_notifications.count) {
		*_notification_pending = true;
		goto end_unlock;
	}

	if (channel->socket < 0) {
		status = LTTNG_NOTIFICATION_CHANNEL_STATUS_CLOSED;
		goto end_unlock;
	}

	/*
	 * Check, without blocking, if data is available on the channel's
	 * socket. If there is data available, it is safe to read (blocking)
	 * on the socket for a message from the session daemon.
	 *
	 * Since all commands wait for the session daemon's reply before
	 * releasing the channel's lock, the protocol only allows for
	 * notifications and "notification dropped" messages to come
	 * through. If we receive a different message type, it is
	 * considered a protocol error.
	 *
	 * Note that this function is not guaranteed not to block. This
	 * will block until our peer (the session daemon) has sent a complete
	 * message if we see data available on the socket. If the peer does
	 * not respect the protocol, this may block indefinitely.
	 */
	ret = lttng_poll_create(&events, 1, LTTNG_CLOEXEC);
	if (ret < 0) {
		status = LTTNG_NOTIFICATION_CHANNEL_STATUS_ERROR;
		goto end_unlock;
	}
	ret = lttng_poll_add(&events, channel->socket, LPOLLIN | LPOLLERR);
	if (ret < 0) {
		status = LTTNG_NOTIFICATION_CHANNEL_STATUS_ERROR;
		goto end_clean_poll;
	}
	/* timeout = 0: return immediately. */
	ret = lttng_poll_wait_interruptible(&events, 0);
	if (ret == 0) {
		/* No data available. */
		*_notification_pending = false;
		goto end_clean_poll;
	} else if (ret < 0) {
		status = LTTNG_NOTIFICATION_CHANNEL_STATUS_ERROR;
		goto end_clean_poll;
	}

	/* Data available on socket. */
	ret = receive_message(channel);
	if (ret) {
		status = LTTNG_NOTIFICATION_CHANNEL_STATUS_ERROR;
		goto end_clean_poll;
	}

	switch (get_current_message_type(channel)) {
	case LTTNG_NOTIFICATION_CHANNEL_MESSAGE_TYPE_NOTIFICATION:
		ret = enqueue_notification_from_current_message(channel);
		if (ret) {
			goto end_clean_poll;
		}
		*_notification_pending = true;
		break;
	case LTTNG_NOTIFICATION_CHANNEL_MESSAGE_TYPE_NOTIFICATION_DROPPED:
		ret = enqueue_dropped_notification(channel);
		if (ret) {
			goto end_clean_poll;
		}
		*_notification_pending = true;
		break;
	default:
		/* Protocol error. */
		status = LTTNG_NOTIFICATION_CHANNEL_STATUS_ERROR;
		goto end_clean_poll;
	}

end_clean_poll:
	lttng_poll_clean(&events);
end_unlock:
	pthread_mutex_unlock(&channel->lock);
end:
	return status;
}

static
int receive_command_reply(struct lttng_notification_channel *channel,
		enum lttng_notification_channel_status *status)
{
	int ret;
	struct lttng_notification_channel_command_reply *reply;

	while (true) {
		enum lttng_notification_channel_message_type msg_type;

		ret = receive_message(channel);
		if (ret) {
			goto end;
		}

		msg_type = get_current_message_type(channel);
		switch (msg_type) {
		case LTTNG_NOTIFICATION_CHANNEL_MESSAGE_TYPE_COMMAND_REPLY:
			goto exit_loop;
		case LTTNG_NOTIFICATION_CHANNEL_MESSAGE_TYPE_NOTIFICATION:
			ret = enqueue_notification_from_current_message(
					channel);
			if (ret) {
				goto end;
			}
			break;
		case LTTNG_NOTIFICATION_CHANNEL_MESSAGE_TYPE_NOTIFICATION_DROPPED:
			ret = enqueue_dropped_notification(channel);
			if (ret) {
				goto end;
			}
			break;
		case LTTNG_NOTIFICATION_CHANNEL_MESSAGE_TYPE_HANDSHAKE:
		{
			struct lttng_notification_channel_command_handshake *handshake;

			handshake = (struct lttng_notification_channel_command_handshake *)
					(channel->reception_payload.buffer.data +
					sizeof(struct lttng_notification_channel_message));
			channel->version.major = handshake->major;
			channel->version.minor = handshake->minor;
			channel->version.set = true;
			break;
		}
		default:
			ret = -1;
			goto end;
		}
	}

exit_loop:
	if (channel->reception_payload.buffer.size <
			(sizeof(struct lttng_notification_channel_message) +
			sizeof(*reply))) {
		/* Invalid message received. */
		ret = -1;
		goto end;
	}

	reply = (struct lttng_notification_channel_command_reply *)
			(channel->reception_payload.buffer.data +
			sizeof(struct lttng_notification_channel_message));
	*status = (enum lttng_notification_channel_status) reply->status;
end:
	return ret;
}

static
int handshake(struct lttng_notification_channel *channel)
{
	ssize_t ret;
	enum lttng_notification_channel_status status =
			LTTNG_NOTIFICATION_CHANNEL_STATUS_OK;
	struct lttng_notification_channel_command_handshake handshake = {
		.major = LTTNG_NOTIFICATION_CHANNEL_VERSION_MAJOR,
		.minor = LTTNG_NOTIFICATION_CHANNEL_VERSION_MINOR,
	};
	struct lttng_notification_channel_message msg_header = {
		.type = LTTNG_NOTIFICATION_CHANNEL_MESSAGE_TYPE_HANDSHAKE,
		.size = sizeof(handshake),
	};
	char send_buffer[sizeof(msg_header) + sizeof(handshake)];

	memcpy(send_buffer, &msg_header, sizeof(msg_header));
	memcpy(send_buffer + sizeof(msg_header), &handshake, sizeof(handshake));

	pthread_mutex_lock(&channel->lock);

	ret = lttcomm_send_creds_unix_sock(channel->socket, send_buffer,
			sizeof(send_buffer));
	if (ret < 0) {
		goto end_unlock;
	}

	/* Receive handshake info from the sessiond. */
	ret = receive_command_reply(channel, &status);
	if (ret < 0) {
		goto end_unlock;
	}

	if (!channel->version.set) {
		ret = -1;
		goto end_unlock;
	}

	if (channel->version.major != LTTNG_NOTIFICATION_CHANNEL_VERSION_MAJOR) {
		ret = -1;
		goto end_unlock;
	}

end_unlock:
	pthread_mutex_unlock(&channel->lock);
	return ret;
}

static
enum lttng_notification_channel_status send_condition_command(
		struct lttng_notification_channel *channel,
		enum lttng_notification_channel_message_type type,
		const struct lttng_condition *condition)
{
	int socket;
	ssize_t ret;
	enum lttng_notification_channel_status status =
			LTTNG_NOTIFICATION_CHANNEL_STATUS_OK;
	struct lttng_payload payload;
	struct lttng_notification_channel_message cmd_header = {
		.type = (int8_t) type,
	};

	lttng_payload_init(&payload);

	if (!channel) {
		status = LTTNG_NOTIFICATION_CHANNEL_STATUS_INVALID;
		goto end;
	}

	assert(type == LTTNG_NOTIFICATION_CHANNEL_MESSAGE_TYPE_SUBSCRIBE ||
		type == LTTNG_NOTIFICATION_CHANNEL_MESSAGE_TYPE_UNSUBSCRIBE);

	pthread_mutex_lock(&channel->lock);
	socket = channel->socket;

	if (!lttng_condition_validate(condition)) {
		status = LTTNG_NOTIFICATION_CHANNEL_STATUS_INVALID;
		goto end_unlock;
	}

	ret = lttng_dynamic_buffer_append(&payload.buffer, &cmd_header,
			sizeof(cmd_header));
	if (ret) {
		status = LTTNG_NOTIFICATION_CHANNEL_STATUS_ERROR;
		goto end_unlock;
	}

	ret = lttng_condition_serialize(condition, &payload);
	if (ret) {
		status = LTTNG_NOTIFICATION_CHANNEL_STATUS_INVALID;
		goto end_unlock;
	}

	/* Update payload length. */
	((struct lttng_notification_channel_message *) payload.buffer.data)->size =
			(uint32_t) (payload.buffer.size - sizeof(cmd_header));

	{
		struct lttng_payload_view pv =
				lttng_payload_view_from_payload(
						&payload, 0, -1);
		const int fd_count =
				lttng_payload_view_get_fd_handle_count(&pv);

		/* Update fd count. */
		((struct lttng_notification_channel_message *) payload.buffer.data)->fds =
			(uint32_t) fd_count;

		ret = lttcomm_send_unix_sock(
			socket, pv.buffer.data, pv.buffer.size);
		if (ret < 0) {
			status = LTTNG_NOTIFICATION_CHANNEL_STATUS_ERROR;
			goto end_unlock;
		}

		/* Pass fds if present. */
		if (fd_count > 0) {
			ret = lttcomm_send_payload_view_fds_unix_sock(socket,
					&pv);
			if (ret < 0) {
				status = LTTNG_NOTIFICATION_CHANNEL_STATUS_ERROR;
				goto end_unlock;
			}
		}
	}

	ret = receive_command_reply(channel, &status);
	if (ret < 0) {
		status = LTTNG_NOTIFICATION_CHANNEL_STATUS_ERROR;
		goto end_unlock;
	}
end_unlock:
	pthread_mutex_unlock(&channel->lock);
end:
	lttng_payload_reset(&payload);
	return status;
}

enum lttng_notification_channel_status lttng_notification_channel_subscribe(
		struct lttng_notification_channel *channel,
		const struct lttng_condition *condition)
{
	return send_condition_command(channel,
			LTTNG_NOTIFICATION_CHANNEL_MESSAGE_TYPE_SUBSCRIBE,
			condition);
}

enum lttng_notification_channel_status lttng_notification_channel_unsubscribe(
		struct lttng_notification_channel *channel,
		const struct lttng_condition *condition)
{
	return send_condition_command(channel,
			LTTNG_NOTIFICATION_CHANNEL_MESSAGE_TYPE_UNSUBSCRIBE,
			condition);
}

void lttng_notification_channel_destroy(
		struct lttng_notification_channel *channel)
{
	if (!channel) {
		return;
	}

	if (channel->socket >= 0) {
		(void) lttcomm_close_unix_sock(channel->socket);
	}
	pthread_mutex_destroy(&channel->lock);
	lttng_payload_reset(&channel->reception_payload);
	free(channel);
}
