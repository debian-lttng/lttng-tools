# SPDX-License-Identifier: GPL-2.0-only

SUBDIRS =

AM_CPPFLAGS += -I$(srcdir) -I$(builddir)

lib_LTLIBRARIES = liblttng-ctl.la

liblttng_ctl_la_SOURCES = lttng-ctl.c snapshot.c lttng-ctl-helper.h \
		lttng-ctl-health.c save.c load.c deprecated-symbols.c \
		channel.c rotate.c event.c destruction-handle.c clear.c \
		tracker.c

liblttng_ctl_la_LDFLAGS = \
		$(LT_NO_UNDEFINED)

liblttng_ctl_la_LIBADD = \
		$(top_builddir)/src/common/sessiond-comm/libsessiond-comm.la \
		$(top_builddir)/src/common/libcommon-lgpl.la

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = lttng-ctl.pc
