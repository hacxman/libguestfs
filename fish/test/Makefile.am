# libguestfs
# Copyright (C) 2009-2014 Red Hat Inc.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

include $(top_srcdir)/subdir-rules.mk

check_PROGRAMS = testquoting testreadline

testquoting_SOURCES = \
	testquoting.c \
	$(top_srcdir)/fish/rl.c

testquoting_CPPFLAGS = \
	-I$(top_srcdir)/src -I$(top_builddir)/src \
	-I$(top_srcdir)/fish -I$(top_builddir)/fish \
	-I$(top_srcdir)/gnulib/lib -I$(top_builddir)/gnulib/lib

#testquoting_LDADD = $(top_builddir)/fish/rl.o

testreadline_SOURCES = \
	testreadline.c \
	$(top_srcdir)/fish/rl.c

testreadline_CPPFLAGS = \
	-I$(top_srcdir)/src -I$(top_builddir)/src \
	-I$(top_srcdir)/fish -I$(top_builddir)/fish \
	-I$(top_srcdir)/gnulib/lib -I$(top_builddir)/gnulib/lib

testreadline_LDADD = -lreadline
#	$(top_builddir)/fish/rl.o -lreadline

TESTS_ENVIRONMENT = $(top_builddir)/run --test

TESTS = \
	testquoting

# testquoting_CFLAGS = -I$(top_srcdir)
