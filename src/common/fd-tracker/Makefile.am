# SPDX-License-Identifier: GPL-2.0-only

noinst_LTLIBRARIES = libfd-tracker.la

if COMPAT_EPOLL
COMPAT=utils-epoll.c
else
COMPAT=utils-poll.c
endif

libfd_tracker_la_SOURCES = fd-tracker.h fd-tracker.c \
                           utils.h utils.c \
                           inode.h inode.c \
                           $(COMPAT)
