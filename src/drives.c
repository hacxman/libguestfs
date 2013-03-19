/* libguestfs
 * Copyright (C) 2013 Red Hat Inc.
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

/* Drives added are stored in an array in the handle.  Code here
 * manages that array and the individual 'struct drive' data.
 */

#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <inttypes.h>
#include <unistd.h>
#include <fcntl.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <assert.h>

#include <pcre.h>

#include "c-ctype.h"
#include "ignore-value.h"

#include "guestfs.h"
#include "guestfs-internal.h"
#include "guestfs-internal-actions.h"
#include "guestfs_protocol.h"

/* Compile all the regular expressions once when the shared library is
 * loaded.  PCRE is thread safe so we're supposedly OK here if
 * multiple threads call into the libguestfs API functions below
 * simultaneously.
 */
static pcre *re_hostname_port;

static void compile_regexps (void) __attribute__((constructor));
static void free_regexps (void) __attribute__((destructor));

static void
compile_regexps (void)
{
  const char *err;
  int offset;

#define COMPILE(re,pattern,options)                                     \
  do {                                                                  \
    re = pcre_compile ((pattern), (options), &err, &offset, NULL);      \
    if (re == NULL) {                                                   \
      ignore_value (write (2, err, strlen (err)));                      \
      abort ();                                                         \
    }                                                                   \
  } while (0)

  COMPILE (re_hostname_port, "(.*):(\\d+)$", 0);
}

static void
free_regexps (void)
{
  pcre_free (re_hostname_port);
}

/* Create and free the 'drive' struct. */
static struct drive *
create_drive_file (guestfs_h *g, const char *path,
                   bool readonly, const char *format,
                   const char *iface, const char *name,
                   const char *disk_label,
                   bool use_cache_none)
{
  struct drive *drv = safe_calloc (g, 1, sizeof *drv);

  drv->src.protocol = drive_protocol_file;
  drv->src.u.path = safe_strdup (g, path);

  drv->readonly = readonly;
  drv->format = format ? safe_strdup (g, format) : NULL;
  drv->iface = iface ? safe_strdup (g, iface) : NULL;
  drv->name = name ? safe_strdup (g, name) : NULL;
  drv->disk_label = disk_label ? safe_strdup (g, disk_label) : NULL;
  drv->use_cache_none = use_cache_none;

  drv->priv = drv->free_priv = NULL;

  return drv;
}

static struct drive *
create_drive_nbd (guestfs_h *g,
                  struct drive_server *servers, size_t nr_servers,
                  const char *exportname,
                  bool readonly, const char *format,
                  const char *iface, const char *name,
                  const char *disk_label,
                  bool use_cache_none)
{
  struct drive *drv = safe_calloc (g, 1, sizeof *drv);

  /* Should have been checked by the calling code. */
  assert (nr_servers == 1);

  drv->src.protocol = drive_protocol_nbd;
  drv->src.servers = servers;
  drv->src.nr_servers = nr_servers;
  drv->src.u.exportname = safe_strdup (g, exportname);

  drv->readonly = readonly;
  drv->format = format ? safe_strdup (g, format) : NULL;
  drv->iface = iface ? safe_strdup (g, iface) : NULL;
  drv->name = name ? safe_strdup (g, name) : NULL;
  drv->disk_label = disk_label ? safe_strdup (g, disk_label) : NULL;
  drv->use_cache_none = use_cache_none;

  drv->priv = drv->free_priv = NULL;

  return drv;
}

/* Traditionally you have been able to use /dev/null as a filename, as
 * many times as you like.  Ancient KVM (RHEL 5) cannot handle adding
 * /dev/null readonly.  qemu 1.2 + virtio-scsi segfaults when you use
 * any zero-sized file including /dev/null.  Therefore, we replace
 * /dev/null with a non-zero sized temporary file.  This shouldn't
 * make any difference since users are not supposed to try and access
 * a null drive.
 */
static struct drive *
create_drive_dev_null (guestfs_h *g, bool readonly, const char *format,
                       const char *iface, const char *name,
                       const char *disk_label)
{
  CLEANUP_FREE char *tmpfile = NULL;
  int fd = -1;

  if (format && STRNEQ (format, "raw")) {
    error (g, _("for device '/dev/null', format must be 'raw'"));
    return NULL;
  }

  if (guestfs___lazy_make_tmpdir (g) == -1)
    return NULL;

  /* Because we create a special file, there is no point forcing qemu
   * to create an overlay as well.  Save time by setting readonly = false.
   */
  readonly = false;

  tmpfile = safe_asprintf (g, "%s/devnull%d", g->tmpdir, ++g->unique);
  fd = open (tmpfile, O_WRONLY|O_CREAT|O_NOCTTY|O_CLOEXEC, 0600);
  if (fd == -1) {
    perrorf (g, "open: %s", tmpfile);
    close (fd);
    return NULL;
  }
  if (ftruncate (fd, 4096) == -1) {
    perrorf (g, "truncate: %s", tmpfile);
    close (fd);
    return NULL;
  }
  if (close (fd) == -1) {
    perrorf (g, "close: %s", tmpfile);
    return NULL;
  }

  return create_drive_file (g, tmpfile, readonly, format, iface, name,
                            disk_label, 0);
}

static struct drive *
create_drive_dummy (guestfs_h *g)
{
  /* A special drive struct that is used as a dummy slot for the appliance. */
  return create_drive_file (g, "", 0, NULL, NULL, NULL, NULL, 0);
}

static void
free_drive_servers (struct drive_server *servers, size_t nr_servers)
{
  if (servers) {
    size_t i;

    for (i = 0; i < nr_servers; ++i)
      free (servers[i].u.hostname);
    free (servers);
  }
}

static void
free_drive_struct (struct drive *drv)
{
  guestfs___free_drive_source (&drv->src);
  free (drv->format);
  free (drv->iface);
  free (drv->name);
  free (drv->disk_label);

  if (drv->priv && drv->free_priv)
    drv->free_priv (drv->priv);

  free (drv);
}

/* Convert a struct drive to a string for debugging.  The caller
 * must free this string.
 */
static char *
drive_to_string (guestfs_h *g, const struct drive *drv)
{
  CLEANUP_FREE char *p = NULL;

  p = guestfs___drive_source_qemu_param (g, &drv->src);

  return safe_asprintf
    (g, "%s%s%s%s%s%s%s%s%s%s%s",
     p,
     drv->readonly ? " readonly" : "",
     drv->format ? " format=" : "",
     drv->format ? : "",
     drv->iface ? " iface=" : "",
     drv->iface ? : "",
     drv->name ? " name=" : "",
     drv->name ? : "",
     drv->disk_label ? " label=" : "",
     drv->disk_label ? : "",
     drv->use_cache_none ? " cache=none" : "");
}

/* Add struct drive to the g->drives vector at the given index. */
static void
add_drive_to_handle_at (guestfs_h *g, struct drive *d, size_t drv_index)
{
  if (drv_index >= g->nr_drives) {
    g->drives = safe_realloc (g, g->drives,
                              sizeof (struct drive *) * (drv_index + 1));
    while (g->nr_drives <= drv_index) {
      g->drives[g->nr_drives] = NULL;
      g->nr_drives++;
    }
  }

  assert (g->drives[drv_index] == NULL);

  g->drives[drv_index] = d;
}

/* Add struct drive to the end of the g->drives vector in the handle. */
static void
add_drive_to_handle (guestfs_h *g, struct drive *d)
{
  add_drive_to_handle_at (g, d, g->nr_drives);
}

/* Called during launch to add a dummy slot to g->drives. */
void
guestfs___add_dummy_appliance_drive (guestfs_h *g)
{
  struct drive *drv;

  drv = create_drive_dummy (g);
  add_drive_to_handle (g, drv);
}

/* Free up all the drives in the handle. */
void
guestfs___free_drives (guestfs_h *g)
{
  struct drive *drv;
  size_t i;

  ITER_DRIVES (g, i, drv) {
    free_drive_struct (drv);
  }

  free (g->drives);

  g->drives = NULL;
  g->nr_drives = 0;
}

/* cache=none improves reliability in the event of a host crash.
 *
 * However this option causes qemu to try to open the file with
 * O_DIRECT.  This fails on some filesystem types (notably tmpfs).
 * So we check if we can open the file with or without O_DIRECT,
 * and use cache=none (or not) accordingly.
 *
 * Notes:
 *
 * (1) In qemu, cache=none and cache=off are identical.
 *
 * (2) cache=none does not disable caching entirely.  qemu still
 * maintains a writeback cache internally, which will be written out
 * when qemu is killed (with SIGTERM).  It disables *host kernel*
 * caching by using O_DIRECT.  To disable caching entirely in kernel
 * and qemu we would need to use cache=directsync but there is a
 * performance penalty for that.
 *
 * (3) This function is only called on the !readonly path.  We must
 * try to open with O_RDWR to test that the file is readable and
 * writable here.
 */
static int
test_cache_none (guestfs_h *g, const char *filename)
{
  int fd = open (filename, O_RDWR|O_DIRECT);
  if (fd >= 0) {
    close (fd);
    return 1;
  }

  fd = open (filename, O_RDWR);
  if (fd >= 0) {
    close (fd);
    return 0;
  }

  perrorf (g, "%s", filename);
  return -1;
}

/* Check string parameter matches ^[-_[:alnum:]]+$ (in C locale). */
static int
valid_format_iface (const char *str)
{
  size_t len = strlen (str);

  if (len == 0)
    return 0;

  while (len > 0) {
    char c = *str++;
    len--;
    if (c != '-' && c != '_' && !c_isalnum (c))
      return 0;
  }
  return 1;
}

/* Check the disk label is reasonable.  It can't contain certain
 * characters, eg. '/', ','.  However be stricter here and ensure it's
 * just alphabetic and <= 20 characters in length.
 */
static int
valid_disk_label (const char *str)
{
  size_t len = strlen (str);

  if (len == 0 || len > 20)
    return 0;

  while (len > 0) {
    char c = *str++;
    len--;
    if (!c_isalpha (c))
      return 0;
  }
  return 1;
}

/* Check the server hostname is reasonable. */
static int
valid_hostname (const char *str)
{
  size_t len = strlen (str);

  if (len == 0 || len > 255)
    return 0;

  while (len > 0) {
    char c = *str++;
    len--;
    if (!c_isalnum (c) &&
        c != '-' && c != '.' && c != ':' && c != '[' && c != ']')
      return 0;
  }
  return 1;
}

/* Check the port number is reasonable. */
static int
valid_port (int port)
{
  if (port <= 0 || port > 65535)
    return 0;
  return 1;
}

static int
parse_one_server (guestfs_h *g, const char *server, struct drive_server *ret)
{
  char *hostname;
  char *port_str;
  int port;

  ret->transport = drive_transport_none;

  if (STRPREFIX (server, "tcp:")) {
    /* Explicit tcp: prefix means to skip the unix test. */
    server += 4;
    ret->transport = drive_transport_tcp;
    goto skip_unix;
  }

  if (STRPREFIX (server, "unix:")) {
    if (strlen (server) == 5) {
      error (g, _("missing Unix domain socket path"));
      return -1;
    }
    ret->transport = drive_transport_unix;
    ret->u.socket = safe_strdup (g, server+5);
    ret->port = 0;
    return 0;
  }
 skip_unix:

  if (match2 (g, server, re_hostname_port, &hostname, &port_str)) {
    if (sscanf (port_str, "%d", &port) != 1 || !valid_port (port)) {
      error (g, _("invalid port number '%s'"), port_str);
      free (hostname);
      free (port_str);
      return -1;
    }
    free (port_str);
    if (!valid_hostname (hostname)) {
      error (g, _("invalid hostname '%s'"), hostname);
      free (hostname);
      return -1;
    }
    ret->u.hostname = hostname;
    ret->port = port;
    return 0;
  }

  /* Doesn't match anything above, so assume it's a bare hostname. */
  if (!valid_hostname (server)) {
    error (g, _("invalid hostname or server string '%s'"), server);
    return -1;
  }

  ret->u.hostname = safe_strdup (g, server);
  ret->port = 0;
  return 0;
}

static ssize_t
parse_servers (guestfs_h *g, char *const *strs,
               struct drive_server **servers_rtn)
{
  size_t i;
  size_t n = guestfs___count_strings (strs);
  struct drive_server *servers;

  if (n == 0) {
    *servers_rtn = NULL;
    return 0;
  }

  servers = safe_calloc (g, n, sizeof (struct drive_server));

  for (i = 0; i < n; ++i) {
    if (parse_one_server (g, strs[i], &servers[i]) == -1) {
      if (i > 0)
        free_drive_servers (servers, i-1);
      return -1;
    }
  }

  *servers_rtn = servers;
  return n;
}

static int
nbd_port (void)
{
  struct servent *servent;

  servent = getservbyname ("nbd", "tcp");
  if (servent)
    return ntohs (servent->s_port);
  else
    return 10809;
}

int
guestfs__add_drive_opts (guestfs_h *g, const char *filename,
                         const struct guestfs_add_drive_opts_argv *optargs)
{
  bool readonly;
  const char *format;
  const char *iface;
  const char *name;
  const char *disk_label;
  const char *protocol;
  size_t nr_servers = 0;
  struct drive_server *servers = NULL;
  int use_cache_none;
  struct drive *drv;
  size_t i, drv_index;

  if (strchr (filename, ':') != NULL) {
    error (g, _("filename cannot contain ':' (colon) character. "
                "This is a limitation of qemu."));
    return -1;
  }

  readonly = optargs->bitmask & GUESTFS_ADD_DRIVE_OPTS_READONLY_BITMASK
    ? optargs->readonly : false;
  format = optargs->bitmask & GUESTFS_ADD_DRIVE_OPTS_FORMAT_BITMASK
    ? optargs->format : NULL;
  iface = optargs->bitmask & GUESTFS_ADD_DRIVE_OPTS_IFACE_BITMASK
    ? optargs->iface : NULL;
  name = optargs->bitmask & GUESTFS_ADD_DRIVE_OPTS_NAME_BITMASK
    ? optargs->name : NULL;
  disk_label = optargs->bitmask & GUESTFS_ADD_DRIVE_OPTS_LABEL_BITMASK
    ? optargs->label : NULL;
  protocol = optargs->bitmask & GUESTFS_ADD_DRIVE_OPTS_PROTOCOL_BITMASK
    ? optargs->protocol : "file";
  if (optargs->bitmask & GUESTFS_ADD_DRIVE_OPTS_SERVER_BITMASK) {
    ssize_t r = parse_servers (g, optargs->server, &servers);
    if (r == -1)
      return -1;
    nr_servers = r;
  }

  if (format && !valid_format_iface (format)) {
    error (g, _("%s parameter is empty or contains disallowed characters"),
           "format");
    free_drive_servers (servers, nr_servers);
    return -1;
  }
  if (iface && !valid_format_iface (iface)) {
    error (g, _("%s parameter is empty or contains disallowed characters"),
           "iface");
    free_drive_servers (servers, nr_servers);
    return -1;
  }
  if (disk_label && !valid_disk_label (disk_label)) {
    error (g, _("label parameter is empty, too long, or contains disallowed characters"));
    free_drive_servers (servers, nr_servers);
    return -1;
  }

  if (STREQ (protocol, "file")) {
    if (servers != NULL) {
      error (g, _("you cannot specify a server with file-backed disks"));
      free_drive_servers (servers, nr_servers);
      return -1;
    }

    if (STREQ (filename, "/dev/null"))
      drv = create_drive_dev_null (g, readonly, format, iface, name,
                                   disk_label);
    else {
      /* For writable files, see if we can use cache=none.  This also
       * checks for the existence of the file.  For readonly we have
       * to do the check explicitly.
       */
      use_cache_none = readonly ? false : test_cache_none (g, filename);
      if (use_cache_none == -1)
        return -1;

      if (readonly) {
        if (access (filename, R_OK) == -1) {
          perrorf (g, "%s", filename);
          return -1;
        }
      }

      drv = create_drive_file (g, filename, readonly, format, iface, name,
                               disk_label, use_cache_none);
    }
  }
  else if (STREQ (protocol, "nbd")) {
    if (nr_servers == 0 || nr_servers > 1) {
      error (g, _("protocol nbd: you must specify exactly one server"));
      free_drive_servers (servers, nr_servers);
      return -1;
    }

    if (servers[0].port == 0)
      servers[0].port = nbd_port ();

    drv = create_drive_nbd (g, servers, nr_servers, filename,
                            readonly, format, iface, name,
                            disk_label, false);
  }
  else {
    error (g, _("unknown protocol '%s'"), protocol);
    free_drive_servers (servers, nr_servers);
    return -1;
  }

  if (drv == NULL) {
    free_drive_servers (servers, nr_servers);
    return -1;
  }

  /* Add the drive. */
  if (g->state == CONFIG) {
    /* Not hotplugging, so just add it to the handle. */
    add_drive_to_handle (g, drv); /* drv is now owned by the handle */
    return 0;
  }

  /* ... else, hotplugging case. */
  if (!g->attach_ops || !g->attach_ops->hot_add_drive) {
    error (g, _("the current attach-method does not support hotplugging drives"));
    free_drive_struct (drv);
    return -1;
  }

  if (!drv->disk_label) {
    error (g, _("'label' is required when hotplugging drives"));
    free_drive_struct (drv);
    return -1;
  }

  /* Get the first free index, or add it at the end. */
  drv_index = g->nr_drives;
  for (i = 0; i < g->nr_drives; ++i)
    if (g->drives[i] == NULL)
      drv_index = i;

  /* Hot-add the drive. */
  if (g->attach_ops->hot_add_drive (g, drv, drv_index) == -1) {
    free_drive_struct (drv);
    return -1;
  }

  add_drive_to_handle_at (g, drv, drv_index);
  /* drv is now owned by the handle */

  /* Call into the appliance to wait for the new drive to appear. */
  if (guestfs_internal_hot_add_drive (g, drv->disk_label) == -1)
    return -1;

  return 0;
}

int
guestfs__add_drive_ro (guestfs_h *g, const char *filename)
{
  const struct guestfs_add_drive_opts_argv optargs = {
    .bitmask = GUESTFS_ADD_DRIVE_OPTS_READONLY_BITMASK,
    .readonly = true,
  };

  return guestfs__add_drive_opts (g, filename, &optargs);
}

int
guestfs__add_drive_with_if (guestfs_h *g, const char *filename,
                            const char *iface)
{
  const struct guestfs_add_drive_opts_argv optargs = {
    .bitmask = GUESTFS_ADD_DRIVE_OPTS_IFACE_BITMASK,
    .iface = iface,
  };

  return guestfs__add_drive_opts (g, filename, &optargs);
}

int
guestfs__add_drive_ro_with_if (guestfs_h *g, const char *filename,
                               const char *iface)
{
  const struct guestfs_add_drive_opts_argv optargs = {
    .bitmask = GUESTFS_ADD_DRIVE_OPTS_IFACE_BITMASK
             | GUESTFS_ADD_DRIVE_OPTS_READONLY_BITMASK,
    .iface = iface,
    .readonly = true,
  };

  return guestfs__add_drive_opts (g, filename, &optargs);
}

int
guestfs__add_cdrom (guestfs_h *g, const char *filename)
{
  if (strchr (filename, ':') != NULL) {
    error (g, _("filename cannot contain ':' (colon) character. "
                "This is a limitation of qemu."));
    return -1;
  }

  if (access (filename, F_OK) == -1) {
    perrorf (g, "%s", filename);
    return -1;
  }

  return guestfs__config (g, "-cdrom", filename);
}

/* Depending on whether we are hotplugging or not, this function
 * does slightly different things: If not hotplugging, then the
 * drive just disappears as if it had never been added.  The later
 * drives "move up" to fill the space.  When hotplugging we have to
 * do some complex stuff, and we usually end up leaving an empty
 * (NULL) slot in the g->drives vector.
 */
int
guestfs__remove_drive (guestfs_h *g, const char *label)
{
  size_t i;
  struct drive *drv;

  ITER_DRIVES (g, i, drv) {
    if (drv->disk_label && STREQ (label, drv->disk_label))
      goto found;
  }
  error (g, _("disk with label '%s' not found"), label);
  return -1;

 found:
  if (g->state == CONFIG) {     /* Not hotplugging. */
    free_drive_struct (drv);

    g->nr_drives--;
    for (; i < g->nr_drives; ++i)
      g->drives[i] = g->drives[i+1];

    return 0;
  }
  else {                        /* Hotplugging. */
    if (!g->attach_ops || !g->attach_ops->hot_remove_drive) {
      error (g, _("the current attach-method does not support hotplugging drives"));
      return -1;
    }

    if (guestfs_internal_hot_remove_drive_precheck (g, label) == -1)
      return -1;

    if (g->attach_ops->hot_remove_drive (g, drv, i) == -1)
      return -1;

    free_drive_struct (drv);
    g->drives[i] = NULL;
    if (i == g->nr_drives-1)
      g->nr_drives--;

    if (guestfs_internal_hot_remove_drive (g, label) == -1)
      return -1;

    return 0;
  }
}

/* Checkpoint and roll back drives, so that groups of drives can be
 * added atomicly.  Only used by guestfs_add_domain.
 */
size_t
guestfs___checkpoint_drives (guestfs_h *g)
{
  return g->nr_drives;
}

void
guestfs___rollback_drives (guestfs_h *g, size_t old_i)
{
  size_t i;

  for (i = old_i; i < g->nr_drives; ++i) {
    if (g->drives[i])
      free_drive_struct (g->drives[i]);
  }
  g->nr_drives = old_i;
}

/* Internal command to return the list of drives. */
char **
guestfs__debug_drives (guestfs_h *g)
{
  size_t i, count;
  char **ret;
  struct drive *drv;

  count = 0;
  ITER_DRIVES (g, i, drv) {
    count++;
  }

  ret = safe_malloc (g, sizeof (char *) * (count + 1));

  count = 0;
  ITER_DRIVES (g, i, drv) {
    ret[count++] = drive_to_string (g, drv);
  }

  ret[count] = NULL;

  return ret;                   /* caller frees */
}

/* The drive_source struct is also used in the attach methods, so we
 * also have these utility functions.
 */
void
guestfs___copy_drive_source (guestfs_h *g,
                             const struct drive_source *src,
                             struct drive_source *dest)
{
  size_t i;

  dest->protocol = src->protocol;
  dest->u.path = safe_strdup (g, src->u.path);
  dest->nr_servers = src->nr_servers;
  dest->servers = safe_calloc (g, src->nr_servers,
                               sizeof (struct drive_server));
  for (i = 0; i < src->nr_servers; ++i) {
    dest->servers[i].transport = src->servers[i].transport;
    if (src->servers[i].u.hostname)
      dest->servers[i].u.hostname = safe_strdup (g, src->servers[i].u.hostname);
    dest->servers[i].port = src->servers[i].port;
  }
}

char *
guestfs___drive_source_qemu_param (guestfs_h *g, const struct drive_source *src)
{
  /* Note that the qemu parameter is the bit after "file=".  It is not
   * escaped here, but would usually be escaped if passed to qemu as
   * part of a full -drive parameter (but not for qemu-img).
   */
  switch (src->protocol) {
  case drive_protocol_file:
    return safe_strdup (g, src->u.path);

  case drive_protocol_nbd: {
    CLEANUP_FREE char *p = NULL;
    char *ret;

    switch (src->servers[0].transport) {
    case drive_transport_none:
    case drive_transport_tcp:
      p = safe_asprintf (g, "nbd:%s:%d",
                         src->servers[0].u.hostname, src->servers[0].port);
      break;
    case drive_transport_unix:
      p = safe_asprintf (g, "nbd:unix:%s", src->servers[0].u.socket);
      break;
    }
    assert (p);

    if (STREQ (src->u.exportname, ""))
      ret = safe_strdup (g, p);
    else
      ret = safe_asprintf (g, "%s:exportname=%s", p, src->u.exportname);

    return ret;
  }
  }

  abort ();
}

void
guestfs___free_drive_source (struct drive_source *src)
{
  if (src) {
    free (src->u.path);
    free_drive_servers (src->servers, src->nr_servers);
  }
}