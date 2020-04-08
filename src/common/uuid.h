/*
 * Copyright (C) 2011 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * SPDX-License-Identifier: MIT
 *
 */

#ifndef LTTNG_UUID_H
#define LTTNG_UUID_H

#include <common/macros.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

/*
 * Includes final \0.
 */
#define LTTNG_UUID_STR_LEN	37
#define LTTNG_UUID_LEN		16
#define LTTNG_UUID_VER		4

#define LTTNG_UUID_FMT \
	"%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "-%02" SCNx8 \
	"%02" SCNx8 "-%02" SCNx8 "%02" SCNx8 "-%02" SCNx8 "%02" SCNx8 \
	"-%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "%02" SCNx8 \
	"%02" SCNx8

#define LTTNG_UUID_FMT_VALUES(uuid) \
	(uuid)[0], (uuid)[1], (uuid)[2], (uuid)[3], (uuid)[4], (uuid)[5], \
	(uuid)[6], (uuid)[7], (uuid)[8], (uuid)[9], (uuid)[10], (uuid)[11], \
	(uuid)[12], (uuid)[13], (uuid)[14], (uuid)[15]

#define LTTNG_UUID_SCAN_VALUES(uuid) \
	&(uuid)[0], &(uuid)[1], &(uuid)[2], &(uuid)[3], &(uuid)[4], &(uuid)[5], \
	&(uuid)[6], &(uuid)[7], &(uuid)[8], &(uuid)[9], &(uuid)[10], &(uuid)[11], \
	&(uuid)[12], &(uuid)[13], &(uuid)[14], &(uuid)[15]

typedef uint8_t lttng_uuid[LTTNG_UUID_LEN];

LTTNG_HIDDEN
int lttng_uuid_from_str(const char *str_in, lttng_uuid uuid_out);

/*
 * Convert a UUID to a human-readable, NULL-terminated, string of the form
 * xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx.
 *
 * Assumes uuid_str is at least LTTNG_UUID_STR_LEN byte long.
 */
LTTNG_HIDDEN
void lttng_uuid_to_str(const lttng_uuid uuid, char *uuid_str);

LTTNG_HIDDEN
bool lttng_uuid_is_equal(const lttng_uuid a, const lttng_uuid b);

LTTNG_HIDDEN
bool lttng_uuid_is_nil(const lttng_uuid uuid);

LTTNG_HIDDEN
void lttng_uuid_copy(lttng_uuid dst, const lttng_uuid src);

/*
 * Generate a random UUID according to RFC4122, section 4.4.
 */
LTTNG_HIDDEN
int lttng_uuid_generate(lttng_uuid uuid_out);

#endif /* LTTNG_UUID_H */
