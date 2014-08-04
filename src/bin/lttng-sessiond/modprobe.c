/*
 * Copyright (C) 2011 - David Goulet <dgoulet@efficios.com>
 *               2014 - Jan Glauber <jan.glauber@gmail.com>
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
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

#include <common/common.h>
#include <common/utils.h>

#include "modprobe.h"
#include "kern-modules.h"

#define LTTNG_MOD_REQUIRED	1
#define LTTNG_MOD_OPTIONAL	0

/* LTTng kernel tracer mandatory core modules list */
struct kern_modules_param kern_modules_control_core[] = {
	{ "lttng-tracer" },	/* MUST be loaded first so keep at top */
	{ "lttng-lib-ring-buffer" },
	{ "lttng-ring-buffer-client-discard" },
	{ "lttng-ring-buffer-client-overwrite" },
	{ "lttng-ring-buffer-metadata-client" },
	{ "lttng-ring-buffer-client-mmap-discard" },
	{ "lttng-ring-buffer-client-mmap-overwrite" },
	{ "lttng-ring-buffer-metadata-mmap-client" },
};

/* LTTng kernel tracer optional base modules list */
struct kern_modules_param kern_modules_control_opt[] = {
	{ "lttng-types" },
	{ "lttng-ftrace" },
	{ "lttng-kprobes" },
	{ "lttng-kretprobes" },
};

/* LTTng kernel tracer probe modules list */
struct kern_modules_param kern_modules_probes_default[] = {
	{ "lttng-probe-asoc" },
	{ "lttng-probe-block" },
	{ "lttng-probe-btrfs" },
	{ "lttng-probe-compaction" },
	{ "lttng-probe-ext3" },
	{ "lttng-probe-ext4" },
	{ "lttng-probe-gpio" },
	{ "lttng-probe-irq" },
	{ "lttng-probe-jbd" },
	{ "lttng-probe-jbd2" },
	{ "lttng-probe-kmem" },
	{ "lttng-probe-kvm" },
	{ "lttng-probe-kvm-x86" },
	{ "lttng-probe-kvm-x86-mmu" },
	{ "lttng-probe-lock" },
	{ "lttng-probe-module" },
	{ "lttng-probe-napi" },
	{ "lttng-probe-net" },
	{ "lttng-probe-power" },
	{ "lttng-probe-printk" },
	{ "lttng-probe-random" },
	{ "lttng-probe-rcu" },
	{ "lttng-probe-regmap" },
	{ "lttng-probe-regulator" },
	{ "lttng-probe-rpm" },
	{ "lttng-probe-sched" },
	{ "lttng-probe-scsi" },
	{ "lttng-probe-signal" },
	{ "lttng-probe-skb" },
	{ "lttng-probe-sock" },
	{ "lttng-probe-statedump" },
	{ "lttng-probe-sunrpc" },
	{ "lttng-probe-timer" },
	{ "lttng-probe-udp" },
	{ "lttng-probe-vmscan" },
	{ "lttng-probe-v4l2" },
	{ "lttng-probe-workqueue" },
	{ "lttng-probe-writeback" },
};

/* dynamic probe modules list */
static struct kern_modules_param *probes;
static int nr_probes;

void modprobe_remove_lttng(const struct kern_modules_param *modules,
			   int entries, int required)
{
	int ret = 0, i;
	char modprobe[256];

	for (i = entries - 1; i >= 0; i--) {
		ret = snprintf(modprobe, sizeof(modprobe),
				"/sbin/modprobe -r -q %s",
				modules[i].name);
		if (ret < 0) {
			PERROR("snprintf modprobe -r");
			return;
		}
		modprobe[sizeof(modprobe) - 1] = '\0';
		ret = system(modprobe);
		if (ret == -1) {
			ERR("Unable to launch modprobe -r for module %s",
					modules[i].name);
		} else if (required && WEXITSTATUS(ret) != 0) {
			ERR("Unable to remove module %s",
					modules[i].name);
		} else {
			DBG("Modprobe removal successful %s",
					modules[i].name);
		}
		if (probes)
			free(probes[i].name);
	}
}

/*
 * Remove control kernel module(s) in reverse load order.
 */
void modprobe_remove_lttng_control(void)
{
	modprobe_remove_lttng(kern_modules_control_opt,
			      ARRAY_SIZE(kern_modules_control_opt),
			      LTTNG_MOD_OPTIONAL);
	modprobe_remove_lttng(kern_modules_control_core,
			      ARRAY_SIZE(kern_modules_control_core),
			      LTTNG_MOD_REQUIRED);
}

/*
 * Remove data kernel modules in reverse load order.
 */
void modprobe_remove_lttng_data(void)
{
	if (probes) {
		modprobe_remove_lttng(probes, nr_probes, LTTNG_MOD_OPTIONAL);
		free(probes);
		probes = NULL;
	} else
		modprobe_remove_lttng(kern_modules_probes_default,
				      ARRAY_SIZE(kern_modules_probes_default),
				      LTTNG_MOD_OPTIONAL);
}

/*
 * Remove all kernel modules in reverse order.
 */
void modprobe_remove_lttng_all(void)
{
	modprobe_remove_lttng_data();
	modprobe_remove_lttng_control();
}

static int modprobe_lttng(struct kern_modules_param *modules,
			  int entries, int required)
{
	int ret = 0, i;
	char modprobe[256];

	for (i = 0; i < entries; i++) {
		ret = snprintf(modprobe, sizeof(modprobe),
				"/sbin/modprobe %s%s",
				required ? "" : "-q ",
				modules[i].name);
		if (ret < 0) {
			PERROR("snprintf modprobe");
			goto error;
		}
		modprobe[sizeof(modprobe) - 1] = '\0';
		ret = system(modprobe);
		if (ret == -1) {
			ERR("Unable to launch modprobe for module %s",
					modules[i].name);
		} else if (required && WEXITSTATUS(ret) != 0) {
			ERR("Unable to load module %s", modules[i].name);
		} else {
			DBG("Modprobe successfully %s", modules[i].name);
		}
	}

error:
	return ret;
}

/*
 * Load control kernel module(s).
 */
int modprobe_lttng_control(void)
{
	int ret;

	ret = modprobe_lttng(kern_modules_control_core,
			     ARRAY_SIZE(kern_modules_control_core),
			     LTTNG_MOD_REQUIRED);
	if (ret != 0)
		return ret;
	ret = modprobe_lttng(kern_modules_control_opt,
			     ARRAY_SIZE(kern_modules_control_opt),
			     LTTNG_MOD_OPTIONAL);
	return ret;
}

/*
 * Load data kernel module(s).
 */
int modprobe_lttng_data(void)
{
	int i, ret;
	int entries = ARRAY_SIZE(kern_modules_probes_default);
	char *list, *next;

	/*
	 * First take command line option, if not available take environment
	 * variable.
	 */
	if (kmod_probes_list) {
		list = kmod_probes_list;
	} else {
		list = utils_get_kmod_probes_list();
	}
	/* The default is to load ALL probes */
	if (!list) {
		return modprobe_lttng(kern_modules_probes_default, entries,
				LTTNG_MOD_OPTIONAL);
	}

	/*
	 * A probe list is available, so use it.
	 * The number of probes is limited by the number of probes in the
	 * default list.
	 */
	probes = zmalloc(sizeof(struct kern_modules_param *) * entries);
	if (!probes) {
		PERROR("malloc probe list");
		return -ENOMEM;
	}

	for (i = 0; i < entries; i++) {
		size_t name_len;

		next = strtok(list, ",");
		if (!next) {
			goto out;
		}
		list = NULL;

		/* filter leading spaces */
		while (*next == ' ') {
			next++;
		}

		/* Length 13 is "lttng-probe-" + \0 */
		name_len = strlen(next) + 13;

		probes[i].name = zmalloc(name_len);
		if (!probes[i].name) {
			PERROR("malloc probe list");
			return -ENOMEM;
		}

		ret = snprintf(probes[i].name, name_len, "lttng-probe-%s", next);
		if (ret < 0) {
			PERROR("snprintf modprobe name");
			goto out;
		}
	}

out:
	nr_probes = i;
	return modprobe_lttng(probes, nr_probes, LTTNG_MOD_OPTIONAL);
}
