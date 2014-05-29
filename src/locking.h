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

#ifndef _LOCKING_H
#define _LOCKING_H

#ifdef __cplusplus
extern "C" {
#endif

#include <config.h>

#include "glthread/lock.h"

#include "guestfs.h"
#include "guestfs-internal.h"

extern void guestfs___per_handle_lock_add (guestfs_h *g);

extern void guestfs___per_handle_lock_remove (guestfs_h *g);

extern void guestfs___per_handle_lock_lock (guestfs_h *g);

extern void guestfs___per_handle_lock_unlock (guestfs_h *g);

#ifdef __cplusplus
}
#endif

#endif /* _LOCKING_H */
