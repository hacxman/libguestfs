/* virt-win-reg
 * Copyright (C) 2014 Red Hat Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <config.h>

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <math.h>
#include <guestfs.h>

#include <hivex.h>

#define MAX(x,y) (x >= y ? x : y)

void usage(char *pname) {
  printf("This program can export and merge Windows Registry entries from a\n"
    "Windows guest.\n"
    "\n"
    "Usage:\n"
    "     virt-win-reg domname 'HKLM\\Path\\To\\Subkey'\n"
    "\n"
    "     virt-win-reg domname 'HKLM\\Path\\To\\Subkey' name\n"
    "\n"
    "     virt-win-reg domname 'HKLM\\Path\\To\\Subkey' @\n"
    "\n"
    "     virt-win-reg --merge domname [input.reg ...]\n"
    "\n"
    "     virt-win-reg [--options] disk.img ... # instead of domname\n"
    "\n"
    "WARNING\n"
    "\n"
    "     You must NOT use virt-win-reg with the --merge option on live\n"
    "     virtual machines.  If you do this, you WILL get irreversible disk\n"
    "     corruption in the VM.  virt-win-reg tries to stop you from doing\n"
    "     this, but doesn't catch all cases.\n"
    "\n"
    "     Modifying the Windows Registry is an inherently risky operation.  The format\n"
    "     is deliberately obscure and undocumented, and Registry changes\n"
    "     can leave the system unbootable.  Therefore when using the --merge\n"
    "     option, make sure you have a reliable backup first.\n"
    "\n"
    "Options:\n"
    "    --help\n"
    "        Display brief help.\n"
    "\n"
    "    --version\n"
    "        Display version number and exit.\n"
    "\n"
    "    --debug\n"
    "        Enable debugging messages.\n"
    "\n"
    "    -c URI\n"
    "    --connect URI\n"
    "        If using libvirt, connect to the given *URI*. If omitted, then we\n"
    "        connect to the default libvirt hypervisor.\n"
    "\n"
    "        If you specify guest block devices directly, then libvirt is not\n"
    "        used at all.\n"
    "\n"
    "    --format raw\n"
    "        Specify the format of disk images given on the command line. If this\n"
    "        is omitted then the format is autodetected from the content of the\n"
    "        disk image.\n"
    "\n"
    "        If disk images are requested from libvirt, then this program asks\n"
    "        libvirt for this information. In this case, the value of the format\n"
    "        parameter is ignored.\n"
    "\n"
    "        If working with untrusted raw-format guest disk images, you should\n"
    "        ensure the format is always specified.\n"
    "\n"
    "    --merge\n"
    "        In merge mode, this merges a textual regedit file into the Windows\n"
    "        Registry of the virtual machine. If this flag is *not* given then\n"
    "        virt-win-reg displays or exports Registry entries instead.\n"
    "\n"
    "        Note that *--merge* is *unsafe* to use on live virtual machines, and\n"
    "        will result in disk corruption. However exporting (without this\n"
    "        flag) is always safe.\n"
    "\n"
    "    --encoding UTF-16LE|ASCII\n"
    "        When merging (only), you may need to specify the encoding for\n"
    "        strings to be used in the hive file. This is explained in detail in\n"
    "        \"ENCODING STRINGS\" in Win::Hivex::Regedit(3).\n"
    "\n"
    "        The default is to use UTF-16LE, which should work with recent\n"
    "        versions of Windows.\n"
    "\n"
    "    --unsafe-printable-strings\n"
    "        When exporting (only), assume strings are UTF-16LE and print them as\n"
    "        strings instead of hex sequences. Remove the final zero codepoint\n"
    "        from strings if present.\n"
    "\n"
    "        This is unsafe and does not preserve the fidelity of strings in the\n"
    "        original Registry for various reasons:\n"
    "\n"
    "        *   Assumes the original encoding is UTF-16LE. ASCII strings and\n"
    "            strings in other encodings will be corrupted by this\n"
    "            transformation.\n"
    "\n"
    "        *   Assumes that everything which has type 1 or 2 is really a string\n"
    "            and that everything else is not a string, but the type field in\n"
    "            real Registries is not reliable.\n"
    "\n"
    "        *   Loses information about whether a zero codepoint followed the\n"
    "            string in the Registry or not.\n"
    "\n"
    "        This all happens because the Registry itself contains no information\n"
    "        about how strings are encoded (see \"ENCODING STRINGS\" in\n"
    "        Win::Hivex::Regedit(3)).\n"
    "\n"
    "        You should only use this option for quick hacking and debugging of\n"
    "        the Registry contents, and *never* use it if the output is going to\n"
    "        be passed into another program or stored in another Registry.\n"
    , pname);
}

//void export_mode(void);
//void import_mode(void);

int main(int argc, char * argv[]) {
  char *connect;
  int debug = 0;
  char *format;
  int merge = 0;
  char *encoding;
  int unsafe_printable_strings = 0;

  while (1) {
    static struct option long_options[] = {
      {"help",     no_argument,       0, 'h'},
      {"version",  no_argument,       0, 'v'},
      {"connect",  required_argument, 0, 'c'},
      {"debug",    no_argument,       0, 'd'},
      {"format",   required_argument, 0, 'f'},
      {"merge",    no_argument,       0, 'm'},
      {"encoding", required_argument, 0, 'e'},
      {"unsafe-printable-strings", no_argument, 0, 'u'},
      {0,          0,                 0,  0}
    };
    int option_index = 0;

    int c = getopt_long(argc, argv, "hvc:df:me:u", long_options, &option_index);
    if (c == -1)
      break;

    guestfs_h *g;
    struct guestfs_version *v;

    switch (c) {
      case 0:
        printf("0\n");
        break;
      case 'h':
        usage(argv[0]);
        exit(0);
        break;
      case 'v':
        g = guestfs_create();
        v = guestfs_version(g);
        printf("%d.%d.%d%s\n", v->major, v->minor, v->release, v->extra);
        guestfs_free_version(v);
        guestfs_close(g);
        exit(0);
        break;
      case 'c':
        strcpy(connect, optarg);
        break;
      case 'd':
        debug = 1;
        break;
      case 'f':
        strcpy(format, optarg);
        break;
      case 'm':
        merge = 1;
        break;
      case 'e':
        strcpy(encoding, optarg);
        break;
      case 'u':
        unsafe_printable_strings = 1;
        break;
      case '?':
        break;
      default:
        break;
    }
  }

  if (argc - optind < 2) {
    usage(argv[0]);
    exit(1);
  }

  char * domname_or_image = argv[optind];
  char * key = argv[optind+1];

  guestfs_h *g = guestfs_create();
  if (!g) {
    fprintf(stderr, "failik\n");
    exit(1);
  }
  // for now, implement only local file variant, we miss other 2
  fprintf(stderr, "domname_or_image = %s\n", domname_or_image);
  guestfs_add_drive(g, domname_or_image);
  guestfs_launch(g);

  fprintf(stderr, "inspecting guest\n");

  char ** oses = guestfs_inspect_os(g);
  char ** osit = oses;
  if (osit == NULL) {
    fprintf(stderr, "kokot\n");
    exit(1);
  }

  do {
    printf("%s\n", *osit);
  } while (*(++osit) != NULL);

  char ** mps = guestfs_inspect_get_mountpoints(g, oses[0]);
  char ** mpit = mps;

  do {
    printf("%s\n", *mpit);
//    guestfs_mount_options(g, merge ? "" : "ro", *mpit, *(++mpit));

  } while (*(++mpit) != NULL);

  fprintf(stderr, "inspecting system root for windows\n");
  char * systemroot = guestfs_inspect_get_windows_systemroot(g, oses[0]);
  if (!systemroot) {
    fprintf(stderr, "system root neni windows pico\n");
    exit(1);
  }
  fprintf(stderr, "%s\n", systemroot);

  // create a tempdir
  fprintf(stderr, "creating tempdir!\n");
  char * tmpdir = malloc(strlen("virt-win-reg.XXXXXX")+1);
  strcpy(tmpdir, "virt-win-reg.XXXXXX");
  tmpdir = mkdtemp(tmpdir);
  fprintf(stderr, "%s\n", tmpdir);

  fprintf(stderr, "shutting down!\n");

//  if (!merge) {
//    export_mode();
//  } else {
//    import_mode();
//  }


  guestfs_shutdown (g);
  guestfs_close (g);

  rmdir(tmpdir);

  return 0;
}

int str_has_prefix(char *a, char *b) {
  int n = strlen(a);
  return strncmp(a, b, n) ? n : 0;
}

char ** map_path_to_hive(char * path, char * systemroot) {
  char ** result = malloc(sizeof(char*) * 4);
  char * hiveshortname = result[0];
  char * hivefile = result[1];
  char * new_path = result[2];
  char * prefix = result[3];
  int idx0, idx1;
  if ((idx0 = str_has_prefix("\\HKEY_LOCAL_MACHINE\\SAM", path)) ||
      (idx1 = str_has_prefix("\\HKLM\\SAM", path))) {
    hiveshortname = strdup("sam");
    hivefile = malloc(strlen(systemroot) + strlen("/system32/config/") +
        strlen(hiveshortname) + 1);
    hivefile = strcat(strcat(strcat(hivefile, systemroot),
          "/system32/config/"),
        hiveshortname);
    new_path = strdup(path + MAX(idx0, idx1));
    if (*new_path == 0) {
      new_path = strdup("\\");
    }
    prefix = strdup("HKEY_LOCAL_MACHINE\\SAM");
  }

}

void export_mode(char *path, char *name) {
//  char ** mapping = map_path_to_hive(path);
};
