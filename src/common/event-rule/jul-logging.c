/*
 * Copyright (C) 2019 Jonathan Rajotte <jonathan.rajotte-julien@efficios.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 */

#include <assert.h>
#include <common/credentials.h>
#include <common/error.h>
#include <common/hashtable/hashtable.h>
#include <common/hashtable/utils.h>
#include <common/macros.h>
#include <common/mi-lttng.h>
#include <common/optional.h>
#include <common/payload-view.h>
#include <common/payload.h>
#include <common/runas.h>
#include <common/string-utils/string-utils.h>
#include <lttng/event-rule/event-rule-internal.h>
#include <lttng/event-rule/jul-logging-internal.h>
#include <lttng/event.h>
#include <lttng/log-level-rule.h>

#define IS_JUL_LOGGING_EVENT_RULE(rule) \
	(lttng_event_rule_get_type(rule) == LTTNG_EVENT_RULE_TYPE_JUL_LOGGING)

static void lttng_event_rule_jul_logging_destroy(struct lttng_event_rule *rule)
{
	struct lttng_event_rule_jul_logging *jul_logging;

	if (rule == NULL) {
		return;
	}

	jul_logging = container_of(
			rule, struct lttng_event_rule_jul_logging, parent);

	lttng_log_level_rule_destroy(jul_logging->log_level_rule);
	free(jul_logging->pattern);
	free(jul_logging->filter_expression);
	free(jul_logging->internal_filter.filter);
	free(jul_logging->internal_filter.bytecode);
	free(jul_logging);
}

static bool lttng_event_rule_jul_logging_validate(
		const struct lttng_event_rule *rule)
{
	bool valid = false;
	struct lttng_event_rule_jul_logging *jul_logging;

	if (!rule) {
		goto end;
	}

	jul_logging = container_of(
			rule, struct lttng_event_rule_jul_logging, parent);

	/* Required field. */
	if (!jul_logging->pattern) {
		ERR("Invalid jul_logging event rule: a pattern must be set.");
		goto end;
	}

	valid = true;
end:
	return valid;
}

static int lttng_event_rule_jul_logging_serialize(
		const struct lttng_event_rule *rule,
		struct lttng_payload *payload)
{
	int ret;
	size_t pattern_len, filter_expression_len, header_offset;
	size_t size_before_log_level_rule;
	struct lttng_event_rule_jul_logging *jul_logging;
	struct lttng_event_rule_jul_logging_comm jul_logging_comm;
	struct lttng_event_rule_jul_logging_comm *header;

	if (!rule || !IS_JUL_LOGGING_EVENT_RULE(rule)) {
		ret = -1;
		goto end;
	}

	header_offset = payload->buffer.size;

	DBG("Serializing jul_logging event rule.");
	jul_logging = container_of(
			rule, struct lttng_event_rule_jul_logging, parent);

	pattern_len = strlen(jul_logging->pattern) + 1;

	if (jul_logging->filter_expression != NULL) {
		filter_expression_len =
				strlen(jul_logging->filter_expression) + 1;
	} else {
		filter_expression_len = 0;
	}

	jul_logging_comm.pattern_len = pattern_len;
	jul_logging_comm.filter_expression_len = filter_expression_len;

	ret = lttng_dynamic_buffer_append(&payload->buffer, &jul_logging_comm,
			sizeof(jul_logging_comm));
	if (ret) {
		goto end;
	}

	ret = lttng_dynamic_buffer_append(
			&payload->buffer, jul_logging->pattern, pattern_len);
	if (ret) {
		goto end;
	}

	ret = lttng_dynamic_buffer_append(&payload->buffer, jul_logging->filter_expression,
			filter_expression_len);
	if (ret) {
		goto end;
	}

	size_before_log_level_rule = payload->buffer.size;

	ret = lttng_log_level_rule_serialize(jul_logging->log_level_rule, payload);
	if (ret < 0) {
		goto end;
	}

	header = (typeof(header)) ((char *) payload->buffer.data + header_offset);
	header->log_level_rule_len =
			payload->buffer.size - size_before_log_level_rule;

end:
	return ret;
}

static bool lttng_event_rule_jul_logging_is_equal(
		const struct lttng_event_rule *_a,
		const struct lttng_event_rule *_b)
{
	bool is_equal = false;
	struct lttng_event_rule_jul_logging *a, *b;

	a = container_of(_a, struct lttng_event_rule_jul_logging, parent);
	b = container_of(_b, struct lttng_event_rule_jul_logging, parent);

	/* Quick checks. */

	if (!!a->filter_expression != !!b->filter_expression) {
		goto end;
	}

	/* Long check. */
	assert(a->pattern);
	assert(b->pattern);
	if (strcmp(a->pattern, b->pattern)) {
		goto end;
	}

	if (a->filter_expression && b->filter_expression) {
		if (strcmp(a->filter_expression, b->filter_expression)) {
			goto end;
		}
	} else if (!!a->filter_expression != !!b->filter_expression) {
		/* One is set; not the other. */
		goto end;
	}

	if (!lttng_log_level_rule_is_equal(
				a->log_level_rule, b->log_level_rule)) {
		goto end;
	}

	is_equal = true;
end:
	return is_equal;
}

/*
 * On success ret is 0;
 *
 * On error ret is negative.
 *
 * An event with NO loglevel and the name is * will return NULL.
 */
static int generate_agent_filter(
		const struct lttng_event_rule *rule, char **_agent_filter)
{
	int err;
	int ret = 0;
	char *agent_filter = NULL;
	const char *pattern;
	const char *filter;
	const struct lttng_log_level_rule *log_level_rule = NULL;
	enum lttng_event_rule_status status;

	assert(rule);
	assert(_agent_filter);

	status = lttng_event_rule_jul_logging_get_name_pattern(rule, &pattern);
	if (status != LTTNG_EVENT_RULE_STATUS_OK) {
		ret = -1;
		goto end;
	}

	status = lttng_event_rule_jul_logging_get_filter(rule, &filter);
	if (status == LTTNG_EVENT_RULE_STATUS_UNSET) {
		filter = NULL;
	} else if (status != LTTNG_EVENT_RULE_STATUS_OK) {
		ret = -1;
		goto end;
	}


	/* Don't add filter for the '*' event. */
	if (strcmp(pattern, "*") != 0) {
		if (filter) {
			err = asprintf(&agent_filter,
					"(%s) && (logger_name == \"%s\")",
					filter, pattern);
		} else {
			err = asprintf(&agent_filter, "logger_name == \"%s\"",
					pattern);
		}

		if (err < 0) {
			PERROR("Failed to format agent filter string");
			ret = -1;
			goto end;
		}
	}

	status = lttng_event_rule_jul_logging_get_log_level_rule(
			rule, &log_level_rule);
	if (status == LTTNG_EVENT_RULE_STATUS_OK) {
		enum lttng_log_level_rule_status llr_status;
		const char *op;
		int level;

		switch (lttng_log_level_rule_get_type(log_level_rule))
		{
		case LTTNG_LOG_LEVEL_RULE_TYPE_EXACTLY:
			llr_status = lttng_log_level_rule_exactly_get_level(
					log_level_rule, &level);
			op = "==";
			break;
		case LTTNG_LOG_LEVEL_RULE_TYPE_AT_LEAST_AS_SEVERE_AS:
			llr_status = lttng_log_level_rule_at_least_as_severe_as_get_level(
					log_level_rule, &level);
			op = ">=";
			break;
		default:
			abort();
		}

		if (llr_status != LTTNG_LOG_LEVEL_RULE_STATUS_OK) {
			ret = -1;
			goto end;
		}

		if (filter || agent_filter) {
			char *new_filter;

			err = asprintf(&new_filter,
					"(%s) && (int_loglevel %s %d)",
					agent_filter ? agent_filter : filter,
					op, level);
			if (agent_filter) {
				free(agent_filter);
			}
			agent_filter = new_filter;
		} else {
			err = asprintf(&agent_filter, "int_loglevel %s %d", op,
					level);
		}

		if (err < 0) {
			PERROR("Failed to format agent filter string");
			ret = -1;
			goto end;
		}
	}

	*_agent_filter = agent_filter;
	agent_filter = NULL;

end:
	free(agent_filter);
	return ret;
}

static enum lttng_error_code
lttng_event_rule_jul_logging_generate_filter_bytecode(
		struct lttng_event_rule *rule,
		const struct lttng_credentials *creds)
{
	int ret;
	enum lttng_error_code ret_code;
	struct lttng_event_rule_jul_logging *jul_logging;
	enum lttng_event_rule_status status;
	const char *filter;
	struct lttng_bytecode *bytecode = NULL;
	char *agent_filter;

	assert(rule);

	jul_logging = container_of(
			rule, struct lttng_event_rule_jul_logging, parent);

	status = lttng_event_rule_jul_logging_get_filter(rule, &filter);
	if (status == LTTNG_EVENT_RULE_STATUS_UNSET) {
		filter = NULL;
	} else if (status != LTTNG_EVENT_RULE_STATUS_OK) {
		ret_code = LTTNG_ERR_FILTER_INVAL;
		goto end;
	}

	if (filter && filter[0] == '\0') {
		ret_code = LTTNG_ERR_FILTER_INVAL;
		goto error;
	}

	ret = generate_agent_filter(rule, &agent_filter);
	if (ret) {
		ret_code = LTTNG_ERR_FILTER_INVAL;
		goto error;
	}

	jul_logging->internal_filter.filter = agent_filter;

	if (jul_logging->internal_filter.filter == NULL) {
		ret_code = LTTNG_OK;
		goto end;
	}

	ret = run_as_generate_filter_bytecode(
			jul_logging->internal_filter.filter, creds,
			&bytecode);
	if (ret) {
		ret_code = LTTNG_ERR_FILTER_INVAL;
		goto end;
	}

	jul_logging->internal_filter.bytecode = bytecode;
	bytecode = NULL;
	ret_code = LTTNG_OK;

error:
end:
	free(bytecode);
	return ret_code;
}

static const char *lttng_event_rule_jul_logging_get_internal_filter(
		const struct lttng_event_rule *rule)
{
	struct lttng_event_rule_jul_logging *jul_logging;

	assert(rule);
	jul_logging = container_of(
			rule, struct lttng_event_rule_jul_logging, parent);
	return jul_logging->internal_filter.filter;
}

static const struct lttng_bytecode *
lttng_event_rule_jul_logging_get_internal_filter_bytecode(
		const struct lttng_event_rule *rule)
{
	struct lttng_event_rule_jul_logging *jul_logging;

	assert(rule);
	jul_logging = container_of(
			rule, struct lttng_event_rule_jul_logging, parent);
	return jul_logging->internal_filter.bytecode;
}

static enum lttng_event_rule_generate_exclusions_status
lttng_event_rule_jul_logging_generate_exclusions(
		const struct lttng_event_rule *rule,
		struct lttng_event_exclusion **_exclusions)
{
	/* Unsupported. */
	*_exclusions = NULL;
	return LTTNG_EVENT_RULE_GENERATE_EXCLUSIONS_STATUS_NONE;
}

static unsigned long lttng_event_rule_jul_logging_hash(
		const struct lttng_event_rule *rule)
{
	unsigned long hash;
	struct lttng_event_rule_jul_logging *tp_rule =
			container_of(rule, typeof(*tp_rule), parent);

	hash = hash_key_ulong((void *) LTTNG_EVENT_RULE_TYPE_JUL_LOGGING,
			lttng_ht_seed);
	hash ^= hash_key_str(tp_rule->pattern, lttng_ht_seed);

	if (tp_rule->filter_expression) {
		hash ^= hash_key_str(tp_rule->filter_expression, lttng_ht_seed);
	}

	if (tp_rule->log_level_rule) {
		hash ^= lttng_log_level_rule_hash(tp_rule->log_level_rule);
	}

	return hash;
}

static struct lttng_event *lttng_event_rule_jul_logging_generate_lttng_event(
		const struct lttng_event_rule *rule)
{
	int ret;
	const struct lttng_event_rule_jul_logging *jul_logging;
	struct lttng_event *local_event = NULL;
	struct lttng_event *event = NULL;
	enum lttng_loglevel_type loglevel_type;
	int loglevel_value = 0;
	enum lttng_event_rule_status status;
	const struct lttng_log_level_rule *log_level_rule;

	jul_logging = container_of(
			rule, const struct lttng_event_rule_jul_logging, parent);

	local_event = zmalloc(sizeof(*local_event));
	if (!local_event) {
		goto error;
	}

	local_event->type = LTTNG_EVENT_TRACEPOINT;
	ret = lttng_strncpy(local_event->name, jul_logging->pattern,
			    sizeof(local_event->name));
	if (ret) {
		ERR("Truncation occurred when copying event rule pattern to `lttng_event` structure: pattern = '%s'",
				jul_logging->pattern);
		goto error;
	}


	/* Map the log level rule to an equivalent lttng_loglevel. */
	status = lttng_event_rule_jul_logging_get_log_level_rule(
			rule, &log_level_rule);
	if (status == LTTNG_EVENT_RULE_STATUS_UNSET) {
		loglevel_type = LTTNG_EVENT_LOGLEVEL_ALL;
		loglevel_value = 0;
	} else if (status == LTTNG_EVENT_RULE_STATUS_OK) {
		enum lttng_log_level_rule_status llr_status;

		switch (lttng_log_level_rule_get_type(log_level_rule)) {
		case LTTNG_LOG_LEVEL_RULE_TYPE_EXACTLY:
			llr_status = lttng_log_level_rule_exactly_get_level(
					log_level_rule, &loglevel_value);
			loglevel_type = LTTNG_EVENT_LOGLEVEL_SINGLE;
			break;
		case LTTNG_LOG_LEVEL_RULE_TYPE_AT_LEAST_AS_SEVERE_AS:
			llr_status = lttng_log_level_rule_at_least_as_severe_as_get_level(
					log_level_rule, &loglevel_value);
			loglevel_type = LTTNG_EVENT_LOGLEVEL_RANGE;
			break;
		default:
			abort();
			break;
		}

		if (llr_status != LTTNG_LOG_LEVEL_RULE_STATUS_OK) {
			goto error;
		}
	} else {
		goto error;
	}

	local_event->loglevel_type = loglevel_type;
	local_event->loglevel = loglevel_value;

	event = local_event;
	local_event = NULL;
error:
	free(local_event);
	return event;
}

static enum lttng_error_code lttng_event_rule_jul_logging_mi_serialize(
		const struct lttng_event_rule *rule, struct mi_writer *writer)
{
	int ret;
	enum lttng_error_code ret_code;
	enum lttng_event_rule_status status;

	const char *filter = NULL;
	const char *name_pattern = NULL;
	const struct lttng_log_level_rule *log_level_rule = NULL;

	assert(rule);
	assert(writer);
	assert(IS_JUL_LOGGING_EVENT_RULE(rule));

	status = lttng_event_rule_jul_logging_get_name_pattern(
			rule, &name_pattern);
	assert(status == LTTNG_EVENT_RULE_STATUS_OK);
	assert(name_pattern);

	status = lttng_event_rule_jul_logging_get_filter(rule, &filter);
	assert(status == LTTNG_EVENT_RULE_STATUS_OK ||
			status == LTTNG_EVENT_RULE_STATUS_UNSET);

	status = lttng_event_rule_jul_logging_get_log_level_rule(
			rule, &log_level_rule);
	assert(status == LTTNG_EVENT_RULE_STATUS_OK ||
			status == LTTNG_EVENT_RULE_STATUS_UNSET);

	/* Open event rule jul logging element. */
	ret = mi_lttng_writer_open_element(
			writer, mi_lttng_element_event_rule_jul_logging);
	if (ret) {
		goto mi_error;
	}

	/* Name pattern. */
	ret = mi_lttng_writer_write_element_string(writer,
			mi_lttng_element_event_rule_name_pattern, name_pattern);
	if (ret) {
		goto mi_error;
	}

	/* Filter expression. */
	if (filter != NULL) {
		ret = mi_lttng_writer_write_element_string(writer,
				mi_lttng_element_event_rule_filter_expression,
				filter);
		if (ret) {
			goto mi_error;
		}
	}

	/* Log level rule. */
	if (log_level_rule) {
		ret_code = lttng_log_level_rule_mi_serialize(
				log_level_rule, writer);
		if (ret_code != LTTNG_OK) {
			goto end;
		}
	}

	/* Close event rule jul logging element. */
	ret = mi_lttng_writer_close_element(writer);
	if (ret) {
		goto mi_error;
	}

	ret_code = LTTNG_OK;
	goto end;

mi_error:
	ret_code = LTTNG_ERR_MI_IO_FAIL;
end:
	return ret_code;
}

struct lttng_event_rule *lttng_event_rule_jul_logging_create(void)
{
	struct lttng_event_rule *rule = NULL;
	struct lttng_event_rule_jul_logging *tp_rule;
	enum lttng_event_rule_status status;

	tp_rule = zmalloc(sizeof(struct lttng_event_rule_jul_logging));
	if (!tp_rule) {
		goto end;
	}

	rule = &tp_rule->parent;
	lttng_event_rule_init(&tp_rule->parent, LTTNG_EVENT_RULE_TYPE_JUL_LOGGING);
	tp_rule->parent.validate = lttng_event_rule_jul_logging_validate;
	tp_rule->parent.serialize = lttng_event_rule_jul_logging_serialize;
	tp_rule->parent.equal = lttng_event_rule_jul_logging_is_equal;
	tp_rule->parent.destroy = lttng_event_rule_jul_logging_destroy;
	tp_rule->parent.generate_filter_bytecode =
			lttng_event_rule_jul_logging_generate_filter_bytecode;
	tp_rule->parent.get_filter =
			lttng_event_rule_jul_logging_get_internal_filter;
	tp_rule->parent.get_filter_bytecode =
			lttng_event_rule_jul_logging_get_internal_filter_bytecode;
	tp_rule->parent.generate_exclusions =
			lttng_event_rule_jul_logging_generate_exclusions;
	tp_rule->parent.hash = lttng_event_rule_jul_logging_hash;
	tp_rule->parent.generate_lttng_event =
			lttng_event_rule_jul_logging_generate_lttng_event;
	tp_rule->parent.mi_serialize = lttng_event_rule_jul_logging_mi_serialize;

	tp_rule->log_level_rule = NULL;

	/* Default pattern is '*'. */
	status = lttng_event_rule_jul_logging_set_name_pattern(rule, "*");
	if (status != LTTNG_EVENT_RULE_STATUS_OK) {
		lttng_event_rule_destroy(rule);
		rule = NULL;
	}

end:
	return rule;
}

LTTNG_HIDDEN
ssize_t lttng_event_rule_jul_logging_create_from_payload(
		struct lttng_payload_view *view,
		struct lttng_event_rule **_event_rule)
{
	ssize_t ret, offset = 0;
	enum lttng_event_rule_status status;
	const struct lttng_event_rule_jul_logging_comm *jul_logging_comm;
	const char *pattern;
	const char *filter_expression = NULL;
	struct lttng_buffer_view current_buffer_view;
	struct lttng_event_rule *rule = NULL;
	struct lttng_log_level_rule *log_level_rule = NULL;

	if (!_event_rule) {
		ret = -1;
		goto end;
	}

	current_buffer_view = lttng_buffer_view_from_view(
			&view->buffer, offset, sizeof(*jul_logging_comm));
	if (!lttng_buffer_view_is_valid(&current_buffer_view)) {
		ERR("Failed to initialize from malformed event rule jul_logging: buffer too short to contain header.");
		ret = -1;
		goto end;
	}

	jul_logging_comm = (typeof(jul_logging_comm)) current_buffer_view.data;

	rule = lttng_event_rule_jul_logging_create();
	if (!rule) {
		ERR("Failed to create event rule jul_logging.");
		ret = -1;
		goto end;
	}

	/* Skip to payload. */
	offset += current_buffer_view.size;

	/* Map the pattern. */
	current_buffer_view = lttng_buffer_view_from_view(
			&view->buffer, offset, jul_logging_comm->pattern_len);

	if (!lttng_buffer_view_is_valid(&current_buffer_view)) {
		ret = -1;
		goto end;
	}

	pattern = current_buffer_view.data;
	if (!lttng_buffer_view_contains_string(&current_buffer_view, pattern,
			jul_logging_comm->pattern_len)) {
		ret = -1;
		goto end;
	}

	/* Skip after the pattern. */
	offset += jul_logging_comm->pattern_len;

	if (!jul_logging_comm->filter_expression_len) {
		goto skip_filter_expression;
	}

	/* Map the filter_expression. */
	current_buffer_view = lttng_buffer_view_from_view(&view->buffer, offset,
			jul_logging_comm->filter_expression_len);
	if (!lttng_buffer_view_is_valid(&current_buffer_view)) {
		ret = -1;
		goto end;
	}

	filter_expression = current_buffer_view.data;
	if (!lttng_buffer_view_contains_string(&current_buffer_view,
			filter_expression,
			jul_logging_comm->filter_expression_len)) {
		ret = -1;
		goto end;
	}

	/* Skip after the pattern. */
	offset += jul_logging_comm->filter_expression_len;

skip_filter_expression:
	if (!jul_logging_comm->log_level_rule_len) {
		goto skip_log_level_rule;
	}

	{
		/* Map the log level rule. */
		struct lttng_payload_view current_payload_view =
				lttng_payload_view_from_view(view, offset,
						jul_logging_comm->log_level_rule_len);

		ret = lttng_log_level_rule_create_from_payload(
				&current_payload_view, &log_level_rule);
		if (ret < 0) {
			ret = -1;
			goto end;
		}

		assert(ret == jul_logging_comm->log_level_rule_len);
	}

	/* Skip after the log level rule. */
	offset += jul_logging_comm->log_level_rule_len;

skip_log_level_rule:

	status = lttng_event_rule_jul_logging_set_name_pattern(rule, pattern);
	if (status != LTTNG_EVENT_RULE_STATUS_OK) {
		ERR("Failed to set event rule jul_logging pattern.");
		ret = -1;
		goto end;
	}

	if (filter_expression) {
		status = lttng_event_rule_jul_logging_set_filter(
				rule, filter_expression);
		if (status != LTTNG_EVENT_RULE_STATUS_OK) {
			ERR("Failed to set event rule jul_logging pattern.");
			ret = -1;
			goto end;
		}
	}

	if (log_level_rule) {
		status = lttng_event_rule_jul_logging_set_log_level_rule(
				rule, log_level_rule);
		if (status != LTTNG_EVENT_RULE_STATUS_OK) {
			ERR("Failed to set event rule jul_logging log level rule.");
			ret = -1;
			goto end;
		}
	}

	*_event_rule = rule;
	rule = NULL;
	ret = offset;
end:
	lttng_log_level_rule_destroy(log_level_rule);
	lttng_event_rule_destroy(rule);
	return ret;
}

enum lttng_event_rule_status lttng_event_rule_jul_logging_set_name_pattern(
		struct lttng_event_rule *rule, const char *pattern)
{
	char *pattern_copy = NULL;
	struct lttng_event_rule_jul_logging *jul_logging;
	enum lttng_event_rule_status status = LTTNG_EVENT_RULE_STATUS_OK;

	if (!rule || !IS_JUL_LOGGING_EVENT_RULE(rule) || !pattern ||
			strlen(pattern) == 0) {
		status = LTTNG_EVENT_RULE_STATUS_INVALID;
		goto end;
	}

	jul_logging = container_of(
			rule, struct lttng_event_rule_jul_logging, parent);
	pattern_copy = strdup(pattern);
	if (!pattern_copy) {
		status = LTTNG_EVENT_RULE_STATUS_ERROR;
		goto end;
	}

	/* Normalize the pattern. */
	strutils_normalize_star_glob_pattern(pattern_copy);

	free(jul_logging->pattern);

	jul_logging->pattern = pattern_copy;
	pattern_copy = NULL;
end:
	return status;
}

enum lttng_event_rule_status lttng_event_rule_jul_logging_get_name_pattern(
		const struct lttng_event_rule *rule, const char **pattern)
{
	struct lttng_event_rule_jul_logging *jul_logging;
	enum lttng_event_rule_status status = LTTNG_EVENT_RULE_STATUS_OK;

	if (!rule || !IS_JUL_LOGGING_EVENT_RULE(rule) || !pattern) {
		status = LTTNG_EVENT_RULE_STATUS_INVALID;
		goto end;
	}

	jul_logging = container_of(
			rule, struct lttng_event_rule_jul_logging, parent);
	if (!jul_logging->pattern) {
		status = LTTNG_EVENT_RULE_STATUS_UNSET;
		goto end;
	}

	*pattern = jul_logging->pattern;
end:
	return status;
}

enum lttng_event_rule_status lttng_event_rule_jul_logging_set_filter(
		struct lttng_event_rule *rule, const char *expression)
{
	char *expression_copy = NULL;
	struct lttng_event_rule_jul_logging *jul_logging;
	enum lttng_event_rule_status status = LTTNG_EVENT_RULE_STATUS_OK;

	if (!rule || !IS_JUL_LOGGING_EVENT_RULE(rule) || !expression ||
			strlen(expression) == 0) {
		status = LTTNG_EVENT_RULE_STATUS_INVALID;
		goto end;
	}

	jul_logging = container_of(
			rule, struct lttng_event_rule_jul_logging, parent);
	expression_copy = strdup(expression);
	if (!expression_copy) {
		PERROR("Failed to copy filter expression");
		status = LTTNG_EVENT_RULE_STATUS_ERROR;
		goto end;
	}

	if (jul_logging->filter_expression) {
		free(jul_logging->filter_expression);
	}

	jul_logging->filter_expression = expression_copy;
	expression_copy = NULL;
end:
	return status;
}

enum lttng_event_rule_status lttng_event_rule_jul_logging_get_filter(
		const struct lttng_event_rule *rule, const char **expression)
{
	struct lttng_event_rule_jul_logging *jul_logging;
	enum lttng_event_rule_status status = LTTNG_EVENT_RULE_STATUS_OK;

	if (!rule || !IS_JUL_LOGGING_EVENT_RULE(rule) || !expression) {
		status = LTTNG_EVENT_RULE_STATUS_INVALID;
		goto end;
	}

	jul_logging = container_of(
			rule, struct lttng_event_rule_jul_logging, parent);
	if (!jul_logging->filter_expression) {
		status = LTTNG_EVENT_RULE_STATUS_UNSET;
		goto end;
	}

	*expression = jul_logging->filter_expression;
end:
	return status;
}

static bool log_level_rule_valid(const struct lttng_log_level_rule *rule)
{
	/*
	 * For both JUL and LOG4J custom log level are possible and can
	 * span the entire int32 range.
	 */
	return true;
}

enum lttng_event_rule_status lttng_event_rule_jul_logging_set_log_level_rule(
		struct lttng_event_rule *rule,
		const struct lttng_log_level_rule *log_level_rule)
{
	struct lttng_event_rule_jul_logging *jul_logging;
	enum lttng_event_rule_status status = LTTNG_EVENT_RULE_STATUS_OK;
	struct lttng_log_level_rule *copy = NULL;

	if (!rule || !IS_JUL_LOGGING_EVENT_RULE(rule)) {
		status = LTTNG_EVENT_RULE_STATUS_INVALID;
		goto end;
	}

	jul_logging = container_of(
			rule, struct lttng_event_rule_jul_logging, parent);

	if (!log_level_rule_valid(log_level_rule)) {
		status = LTTNG_EVENT_RULE_STATUS_INVALID;
		goto end;
	}

	copy = lttng_log_level_rule_copy(log_level_rule);
	if (copy == NULL) {
		status = LTTNG_EVENT_RULE_STATUS_ERROR;
		goto end;
	}

	if (jul_logging->log_level_rule) {
		lttng_log_level_rule_destroy(jul_logging->log_level_rule);
	}

	jul_logging->log_level_rule = copy;

end:
	return status;
}

enum lttng_event_rule_status lttng_event_rule_jul_logging_get_log_level_rule(
		const struct lttng_event_rule *rule,
		const struct lttng_log_level_rule **log_level_rule
		)
{
	struct lttng_event_rule_jul_logging *jul_logging;
	enum lttng_event_rule_status status = LTTNG_EVENT_RULE_STATUS_OK;

	if (!rule || !IS_JUL_LOGGING_EVENT_RULE(rule) || !log_level_rule) {
		status = LTTNG_EVENT_RULE_STATUS_INVALID;
		goto end;
	}

	jul_logging = container_of(
			rule, struct lttng_event_rule_jul_logging, parent);
	if (jul_logging->log_level_rule == NULL) {
		status = LTTNG_EVENT_RULE_STATUS_UNSET;
		goto end;
	}

	*log_level_rule = jul_logging->log_level_rule;
end:
	return status;
}
