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

#include "locking.h"

static gl_lock_t guestfs___lookup_lock_or_abort (guestfs_h *g, int *pos);
static void guestfs___per_handle_lock_is_not_held (guestfs_h *g);
static void ___shrink_lock_arrays (void);


gl_lock_define (static, *per_handle_locks)
gl_rwlock_define_initialized (static, locks_lock)

/* items in handles array bijectively corresponds to items in per_handle_locks
 */
static guestfs_h ** handles;
static size_t n_handles;


static gl_lock_t
guestfs___lookup_lock_or_abort (guestfs_h *g, int *pos)
{
  // this function should be called with locks_lock already held
  for (size_t i = 0; i < n_handles; i++) {
    if (handles[i] == g) {
      if (pos != NULL) {
        *pos = i;
      }
      return per_handle_locks[i];
    }
  }
  // handle not found. aborting
  error (g, _("guestfs lock handle not found"));
  abort ();
}

static void
guestfs___per_handle_lock_is_not_held (guestfs_h *g)
{
  // this function should be called with locks_lock already held

  int i;
  gl_lock_t l = guestfs___lookup_lock_or_abort (g, &i);

  gl_lock_lock (l);
  gl_lock_unlock (l);
}

void
guestfs___per_handle_lock_add (guestfs_h *g)
{
  gl_rwlock_wrlock (locks_lock);

  n_handles++;
  per_handle_locks = realloc (per_handle_locks,
      sizeof (*per_handle_locks) * n_handles);
  handles = realloc (handles, sizeof (*handles) * n_handles);

  gl_lock_init (per_handle_locks[n_handles - 1]);
  handles[n_handles - 1] = g;

  gl_rwlock_unlock (locks_lock);
}

static void
___shrink_lock_arrays (void)
{
  // locks_lock should be held

  int i;
  guestfs___lookup_lock_or_abort (NULL, &i);

  memmove (handles + i, handles + i + 1, (n_handles - i - 1)
      * sizeof (*handles));
  memmove (per_handle_locks + i, per_handle_locks + i + 1,
      (n_handles - i - 1) * sizeof (*per_handle_locks));
  if (handles == NULL || per_handle_locks == NULL) {
    abort ();
  }
}

void
guestfs___per_handle_lock_remove (guestfs_h *g)
{
  // locks_lock prevents per_handle_locks manipulation within this module
  // take write lock, since we're going to update guarded per_handle_locks
  gl_rwlock_wrlock (locks_lock);

  // ensure we don't get into data race when removing lock
  guestfs___per_handle_lock_is_not_held (g);

  int i;
  gl_lock_t l = guestfs___lookup_lock_or_abort (g, &i);
  gl_lock_destroy (l);
  handles[i] = NULL;
  // fill in hole we've created
  ___shrink_lock_arrays ();

  // and realloc
  n_handles--;
  if (n_handles > 0) {
    per_handle_locks = realloc (per_handle_locks,
        sizeof (*per_handle_locks) * n_handles);
    handles = realloc (handles, sizeof (*handles) * n_handles);
  }

  gl_rwlock_unlock (locks_lock);
}

void
guestfs___per_handle_lock_lock (guestfs_h *g)
{
  gl_rwlock_rdlock (locks_lock);

  gl_lock_t l = guestfs___lookup_lock_or_abort(g, NULL);
  gl_lock_lock(l);

  gl_rwlock_unlock (locks_lock);
}

void
guestfs___per_handle_lock_unlock (guestfs_h *g)
{
  gl_rwlock_rdlock (locks_lock);

  gl_lock_t l = guestfs___lookup_lock_or_abort(g, NULL);
  gl_lock_unlock(l);

  gl_rwlock_unlock (locks_lock);
}

