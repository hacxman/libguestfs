/* libguestfs
 * Copyright (C) 2009-2014 Red Hat Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include "glthread/lock.h"
#include "ignore-value.h"

#include "guestfs.h"
#include "guestfs-internal.h"
#include "guestfs-internal-actions.h"
#include "guestfs_protocol.h"

gl_lock_define_initialized (static, locks_lock);

/* items in handles array bijectively corresponds to items in per_handle_locks
 */
static guestfs_h ** handles;
static size_t n_handles;

void
guestfs___per_handle_lock_is_not_held (guestfs_h *g)
{
  // TODO factor-out for as lookup_handle
  for (size_t i = 0; i < n_handles; i++) {
    if (handles[i] == g) {
      gl_lock_lock(per_handle_locks[i]);
      gl_lock_unlock(per_handle_locks[i]);
      return;
    }
  }
  // TODO error in case handle was not found
}

void
guestfs___per_handle_lock_add (guestfs_h *g)
{
  gl_lock_lock (locks_lock);

  n_handles++;
  per_handle_locks = realloc (per_handle_locks,
      sizeof (*per_handle_locks) * n_handles);
  handles = realloc (handles, sizeof (*handles) * n_handles);

  gl_lock_init (per_handle_locks[n_handles - 1]);
  handles[n_handles - 1] = g;

  gl_lock_unlock (locks_lock);
}

void
guestfs___per_handle_lock_remove (guestfs_h *g)
{
  // locks_lock prevents per_handle_locks manipulation within this module
  gl_lock_lock (locks_lock);

  // ensure we don't get into data race when removing lock
  guestfs___per_handle_lock_is_not_held (g);

  for (size_t i = 0; i < n_handles; i++) {
    if (handles[i] == g) {
      gl_lock_destroy (per_handle_locks[i]);
      handles[i] = NULL;
    }
  }

  n_handles++;
  per_handle_locks = realloc (per_handle_locks,
      sizeof (*per_handle_locks) * n_handles);
  handles = realloc (handles, sizeof (*handles) * n_handles);


  gl_lock_unlock (locks_lock);
}
