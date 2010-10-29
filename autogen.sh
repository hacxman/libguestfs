#!/bin/bash -
# libguestfs
# Copyright (C) 2009 Red Hat Inc.
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
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
#
# Rebuild the autotools environment.

set -e
set -v

# ocaml/.depend is updated automatically. However, as it's included by
# ocaml/Makefile.am we need to bootstrap it here.
touch ocaml/.depend

# Ensure that whenever we pull in a gnulib update or otherwise change to a
# different version (i.e., when switching branches), we also rerun ./bootstrap.
curr_status=.git-module-status
t=$(git submodule status|sed 's/^[ +-]//;s/ .*//')
if test "$t" = "$(cat $curr_status 2>/dev/null)"; then
    : # good, it's up to date
else
    echo running bootstrap...
    ./bootstrap && echo "$t" > $curr_status
fi

CONFIGUREDIR=.

# Run configure in BUILDDIR if it's set
if [ ! -z "$BUILDDIR" ]; then
    mkdir -p $BUILDDIR
    cd $BUILDDIR

    CONFIGUREDIR=..
fi

# Ensure that an ocaml package is present for build-from sources.
# This is *not* for anything that is required at configure-time
# when configure is run from a distribution tarball.  From those,
# nothing ocaml-related is required.
# ocamlfind cannot detect the presence of -devel packages directly,
# so if $pkg ends in -devel, first check for the base package, and
# if that's found, check for the existence of $base.cmxa in the
# resulting directory.
require_ocaml_pkg()
{
  pkg=$1
  case $pkg in
    *-devel)
      local base=${pkg%%-devel}
      local dir=$(ocamlfind query "$base") || return 1
      test -f "$dir/$base.cmxa" || return 1
      ;;
    *) ocamlfind query "$pkg" > /dev/null 2>&1 || return 1;;
  esac
  return 0
}

{ require_ocaml_pkg xml-light && require_ocaml_pkg xml-light-devel; } \
  || { echo "you must have ocaml, ocamlfind, ocaml-xml-light" \
         "and ocaml-xml-light-devel" >&2; exit 1; }

# If no arguments were specified and configure has run before, use the previous
# arguments
if test $# == 0 && test -x ./config.status; then
    ./config.status --recheck
else
    $CONFIGUREDIR/configure "$@"
fi
