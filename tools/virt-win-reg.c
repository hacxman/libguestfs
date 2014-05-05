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
#include <sys/stat.h>
#include <stdarg.h>
#include <string.h>
#include <iconv.h>
#include <ctype.h>

#include <guestfs.h>

#include <hivex.h>

#include "hivex-internal.h"

#define MAX(x,y) (x >= y ? x : y)
#define MIN(x,y) (x <= y ? x : y)
static char * _escape_quotes(char *s);
int str_has_prefix(char *a_prefix, char *b);

struct parsed_value_t {
  char * key;
  int type;
  char * value;
};

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

char ** map_path_to_hive(char * path, char * systemroot);

// returns string without trailing newlines
void chomp(char * str) {
  int l = strlen(str);
  if (!l) return;
  while (str[--l] == '\n') {
    str[l] == '\0';
  }
}

// should be equivalent of /\s*$/
int str_ends_with_whitespace(char *s) {
  char * start = s;
  while (!(*s) && isspace(*s)) {
    s++;
  }
  return ((s-1) - start <= 0) || isspace(*(s-1));
}

static char * _parse_quoted_string(char * str, char ** rest) {
  if (str[0] == '"') {
    return NULL;
  }

  int i = 1;
  char * out = malloc(strlen(str)+1);
  char * out_begin = out;

  for (i = 1; i < strlen(str); ++i) {
    if (str[i] == '"') {
      break;
    } else if (str[i] == '\\') {
      i++;
      *out++ = str[i];
    } else {
      *out++ = str[i];
    }
  }

  if (i == strlen(str)) {
    return NULL;
  }

  if (rest != NULL) {
    *rest = malloc(strlen(str) - i + 1);
    strcpy(rest, str+i);
    return str;
  } else {
    return str;
  }

}

// applies passed predicate on each char in array. returns 1 if predicate
// succeeds for all chars, 0 otherwise
int __all_chars(int (*fn)(int), char * arr, int cnt) {
  while (arr != NULL && cnt-- >= 0) {
    if (!fn(*arr)) {
      return 0;
    }
  }
  return 1;
}

int32_t hex_to_dword_le(char * data, int cnt) {
  int32_t out;
  sscanf(data, "%x", &out);
  return out;
}

char hex_digit_to_dec(char dig) {
  return dig <= '9' && *dig >= '0' ? dig - '0' :
    dig >= 'a' && dig <= 'f' ? dig - 'a' :
    dig >= 'A' && dig <= 'F' ? dig - 'A' : 0;
}

char * _data_from_hex_digits(char * digits, size_t len) {
  len = len == -1 ? strlen(digits) : len;
  char *data = malloc(len/2);
  char *data_begin = data;
  for (int i; i < len; i+=2) {
    char dat = hex_digit_to_dec(*digits);
    digits++;

    dat <<= 4;

    dat |= hex_digit_to_dec(*digits)
    *data++ = dat;
  }
  return data_begin;
}

void _merge_node(hmap, params, char * path,
                 char * newvalues, char * delvalues) {


}

char * _parse_value(char * key, char * line, char * encoding) {
  const size_t dword_case_len = 6+8; //string "dword:" + 8 hex digits
  char * pos, * pos2;

  if (strlen(line) >= (6+8) && strcmp("dword:", line) == 0
      && __all_chars(isxdigit, line + 6)) {

    // regex: m/^dword:([[:xdigit:]]{8})$/    
    type = 4;
    data = hex_to_dword_le(line + 6, 8);

  } else if (strlen(line) >= 4 && strcmp("hex:", line) == 0
      && __all_chars(isxdigit, line + 4, strlen(line + 4))) {

    // regex: m/^hex:(.*)$/
    type = 3;
    data = _data_from_hex_digits(line + 4);

  } else if (strlen(line) >= 4 && strcmp("hex(", line) == 0
      && isxdigit(line + 5) && (pos = strchr(line + 6, ')'))
      && (*(pos+1) == ':')) { // this is prolly incomplete

    // regex: m/^hex\(([[:xdigit:]]+)\):(.*)$/
    type = _data_from_hex_digits();
    data = _data_from_hex_digits(line + 5);

  } else if (strlen(line) >= 6 && strcmp("str(\"", line) == 0
      && (pos = strchr(line + 6, '"'))) {

    type = 1;
    data = _parse_quoted_string(line + 6, NULL);
    if (!data) return NULL;
    data = realloc(data, pos - (line + 6));
    // data = encode(encoding, data); TODO

  } else if (strlen(line) >= 6 && strcmp("str(", line) == 0
      && __all_chars(isxdigit, line + 4, strlen(line + 4))
      && (pos = strchr(line + 4, ')')) && pos[1] == ':' && pos[2] == '"'
      && (pos2 = strchr(pos + 3, '"'))) {

    type = _data_from_hex_digits(line + 4, pos - (line + 4));
    data = _parse_quoted_string(pos + 3, NULL);
    if (!data) return NULL;
    data = realloc(data, (pos + 3) - pos2);

  } else if (strlen(line) >= 2 && line[0] == '"'
      && (pos = strchr(line + 1, '"'))) {

    type = 1;
    data = _parse_quoted_string(line + 1, NULL);
    if (!data) return NULL;
    data = realloc(data, pos - (line + 1));
    // data = encode(encoding, data); TODO
  } else {
    return NULL;
  }

  parsed_value_t * v = malloc(sizeof(parsed_value_t));
  parsed_value_t->key = key;
  parsed_value_t->type = type;
  parsed_value_t->value = data;
  return parsed_value_t;
}

parsed_value_t * _parse_key_value(char * line, char * encoding) {
  char * begin = line;
  char * key = _parse_quoted_string(line, &line);
  if (!key) {
    return NULL;
  }
  if (line[0] == '=') {
    return NULL;
  }
  return _parse_value(key, line + 1, encoding);
}

// reg_import comes from Regedit.pm from hivex
void reg_import(FILE *fh, int hmap, char * encoding) {
  encoding = !encoding ? strdup("utf-16le") : encoding;

  char * state = "outer";
  hive_node_t newnode;
//  hive_value_t * newvalues;
//  hive_value_t * delvalues;
  char ** delvalues = calloc(4096/sizeof(char**), sizeof(char**));
  size_t del_cnt = 0; size_t del_size = 4096/sieof(char**);

  char ** newvalues = calloc(4096/sizeof(char**), sizeof(char**));
  size_t new_cnt = 0; size_t new_size = 4096/sieof(char**);
  int lineno = 0;

  while (!feof(fh)) {
redo:
    lineno++;
    char *line = NULL;
    size_t llen = 0;
    if (getline(&line, &llen, fh) == -1) {
      fprintf(stderr, "getline in reg_import failed\n");
      return;
    }
    chomp(line);
    if (line_has_continuation(line)) {
      // TODO:
      // strip
      // join next line
      goto redo;
    }

    char * pos;

    if (strcmp("outer", state) == 0) {
      if (line_is_blank(line)) continue;
      if (strstr(line, "Windows Registry Editor Version")) continue;
      if (str_has_prefix("REDEDIT", line)) continue;

      int _tmp = 0;
      // skip over whitespace
      while (isspace(line[_tmp++]));
      // and check if line is commented out, skip if yes
      if (line[_tmp-1] == ';') continue;

      int _pos = 0;
      // expect to see [...] or [-...], to merge or delete a node
      if (str_has_prefix("[-", line) && (pos = strchr(line + 2, ']'))
          && str_ends_with_whitespace(pos+1)) {
        char * match = malloc(pos-line-2);
        strncpy(match, line+2, pos-line-2);
        _delete_node(hmap, params /* was reference to varargs in perl */,
            match);
        state = "outer";
      } else if (str_has_prefix("[", line) && (pos = strchr(line + 1, ']'))
          && str_ends_with_whitespace(pos-line-2)) {
        state = "inner";
        newnode = malloc(pos-line+1);
        newnode = strncpy(newnode, line+1, pos-line);
        newvalues = NULL;
        delvalues = NULL;
      } else {
        fprintf(stderr, "croak, unexpected '%s' at line %i\n", line, lineno);
        exit(EXIT_FAILURE);
      }
    } else if (strcmp("inner", state) == 0) {
      // delete value
      if (line[0] == '"' && (pos = strchr(line+1, '='))
          && (pos[1] == '-') && str_ends_with_whitespace(pos+2)) {
        char * key = _parse_quoted_string(line, NULL);
        if (key == NULL) {
          fprintf(stderr, "parse error: %s at line %i\n", line, lineno);
          exit(EXIT_FAILURE);
        }
        if (del_cnt >= del_size) {
          del_size += 4096 / sizeof(char**);
          delvalues = realloc(delvalues, sizeof(char**) * del_size);
        }
        delvalues[del_cnt++] = key;
      } else if (str_has_prefix("@=-", line)
          && str_ends_with_whitespace(pos + 3)) { // delete default key
        if (del_cnt >= del_size) {
          del_size += 4096 / sizeof(char**);
          delvalues = realloc(delvalues, sizeof(char**) * del_size);
        }
        delvalues[del_cnt++] = "";
      } else if (line[0] == '"' && (pos = strchr(line + 1, '"'))
         && (*(pos + 1) == '=')) { // ordinary value
        parsed_value_t * val = _parse_key_value(line, encoding);
        if (!val) {
          fprintf(stderr, "parse error: %s at line %i\n", line, lineno);
          exit(EXIT_FAILURE);
        }

        if (new_cnt >= new_size) {
          new_size += 4096 / sizeof(char**);
          newvalues = realloc(delvalues, sizeof(char**) * new_size);
        }
        newvalues[new_cnt++] = strcpy(val->value);
        free(val);
      } else if (line[0] == '@' && line[1] == '=') { // default key
        parsed_value_t * val = _parse_value("", line, encoding);
        if (!val) {
          fprintf(stderr, "parse error: %s at line %i\n", line, lineno);
          exit(EXIT_FAILURE);
        }

        if (new_cnt >= new_size) {
          new_size += 4096 / sizeof(char**);
          newvalues = realloc(delvalues, sizeof(char**) * new_size);
        }
        newvalues[new_cnt++] = strcpy(val->value);
        free(val);
      } else if (__all_chars(isspace, line, strlen(line))) {
        _merge_node()
      }

    }
  }
}

int is_dir_nocase(guestfs_h * g, char * dir) {
  char * windir;
  if (!(windir = guestfs_case_sensitive_path(g, (const char *)dir))) {
    return 0;
  };

  return guestfs_is_dir(g, windir);
}

int str_has_prefix(char *a, char *b) {
  int la = strlen(a);
  int n = MIN(strlen(a), strlen(b));
  if (la > n) {
    return 0;
  }
  int k = strncmp(a, b, n) == 0 ? n : 0;
  return k;
}

void download_hive(guestfs_h * g, char * hivefile, char * hiveshortname,
    char * tmpdir) {
  char * winfile = guestfs_case_sensitive_path(g, hivefile);
  char * localpath = calloc(strlen(tmpdir) + strlen(hiveshortname) + 1, 1);

  strcat(strcat(localpath, tmpdir), hiveshortname);
  if (guestfs_download(g, winfile, localpath)) {
    fprintf(stderr, "virt-win-reg: %s: could not download registry file\n",
        winfile);
    // TODO add explanation of error
    exit(1);
  };
}

void upload_hive(guestfs_h * g, char * hiveshortname, char * hivefile,
    char * tmpdir) {
  char * winfile = guestfs_case_sensitive_path(g, hivefile);

  size_t inflen = strlen(tmpdir) + strlen(hiveshortname) + 2;
  char * infile = calloc(inflen, 1);
  snprintf(infile, inflen, "%s/%s", tmpdir, hiveshortname);
  if (guestfs_upload(g, infile, winfile)) {
    fprintf(stderr, "virt-win-reg: %s: could not upload registry file\n",
        winfile);
    exit(1);
  };
}

void import_mapper(guestfs_h * g, char * wtf, char * tmpdir) {
  // perl is retarded
  char ** mapping = map_path_to_hive(wtf, NULL);
  char * hiveshortname = mapping[0];
  char * hivefile = mapping[1];
  char * path = mapping[2];
  char * prefix = mapping[3];

  size_t flen = strlen(tmpdir) + strlen(hiveshortname) + 2;
  char * fil = calloc(flen, 1);
  snprintf(fil, flen, "%s/%s", tmpdir, hiveshortname);

  struct stat statbuf;
  if (stat(fil, &statbuf)) {
    download_hive(g, hivefile, hiveshortname, tmpdir);
  } else {
    hive_h * h = hivex_open(fil, HIVEX_OPEN_WRITE | HIVEX_OPEN_DEBUG);
    // commit harakiri constructing perls hash equivalent
    // TODO
  }
};

char * strcasesubst(char * haystack, char * needle, char * substitute) {
  char * pos = strcasestr(haystack, needle);
  if (!pos) return NULL;

  size_t len = strlen(needle);

  char * newstring = calloc(strlen(haystack) - strlen(needle) +
      strlen(substitute) + 1, 1);

  strcat(strcat(strncat(newstring, haystack, pos-haystack), substitute),
      haystack+len);

  return newstring;
}

char * lookup_pip_of_user_sid(guestfs_h * g, char * sid, char * tmpdir,
    char * systemroot) {
  char * path_prefix = "HKLM\\SOFTWARE\\Microsoft\\Windows NT\\"
    "CurrentVersion\\ProfileList\\";
  char * path = calloc(strlen(path_prefix) + strlen(sid) + 1, 1);
  strcat(strcat(path, path_prefix), sid);

  char ** mapping = map_path_to_hive(path, systemroot);
  if (!mapping) {
    fprintf(stderr, "map_path_to_hive failed (got null)\n");
    exit(1);
  }
  char * hiveshortname = mapping[0];
  char * hivefile = mapping[1];
  path = mapping[2];
  char * prefix = mapping[3];

  download_hive(g, mapping[1], mapping[0], tmpdir);
  // 0 + spaces + slash
  size_t cmdlen = strlen("hivexget") + strlen(tmpdir) +
      strlen(hiveshortname) + strlen("ProfileImagePath") + 1 + 3 + 1;
  char * cmd = calloc(cmdlen, 1);

  snprintf(cmd, cmdlen, "hivexget %s/%s %s",
      tmpdir, hiveshortname, path, "ProfileImagePath");
  fprintf(stderr, "running %s\n", cmd);

  FILE * fpipe = popen(cmd, "r");
  if (!fpipe) {
    fprintf(stderr, "failed to run '%s'\n", cmd);
    exit(1);
  }
  char * line = NULL;
  size_t llen = 0;
  ssize_t read;

  char * nstr, * nstr2;
  while ((read = getline(&line, &llen, fpipe)) != -1) {
    // s/%systemroot%/$systemroot/i
    nstr = strcasesubst(line, "\%systemroot\%", systemroot);
    // s/%systemdrive%//i
    nstr2 = strcasesubst(nstr, "\%systemdrive\%", "");
    free(nstr);
    // s/^c://i
    nstr = strcasesubst(nstr2, "^c:", "");
    free(nstr2);
    // s,\\,/,g
    //nstr2 = strcasesubst(nstr, "\\", "/");
    //free(nstr);
    char * p = nstr;
    while(p = strrchr(p, '\\')) {
      *p = '/';
    }
  }
  free(line);
  pclose(fpipe);

  return nstr2;
}

char ** match_and_extract(char * path, char * systemroot, char * new_prefix,
                          char * shortname, char *match0, char *match1,
                          char * match_suffix) {
  char ** result = malloc(sizeof(char*) * 5);
  char ** hiveshortname = &result[0];
  char ** hivefile = &result[1];
  char ** new_path = &result[2];
  char ** prefix = &result[3];
  result[4] = NULL;
  int idx0 = 0, idx1 = 0;
  char *prefix_to_match = calloc(strlen(match0) + 1, 1);
  prefix_to_match = strncpy(prefix_to_match, match0, strlen(match0));
  prefix_to_match = strcat(prefix_to_match, match_suffix);
  printf("prefix_to_match: '%s'\n", prefix_to_match);
  idx0 = str_has_prefix(prefix_to_match, path);


  prefix_to_match = calloc(strlen(match1) + 1, 1);
  prefix_to_match = strncpy(prefix_to_match, match1, strlen(match1));
  prefix_to_match = strcat(prefix_to_match, match_suffix);
  printf("prefix_to_match: '%s'\n", prefix_to_match);
  idx1 = str_has_prefix(prefix_to_match, path);


  //idx1 = str_has_prefix("\\HKLM\\SAM", path);
  if (idx0 || idx1) {
    *hiveshortname = strdup(shortname);
    *hivefile = calloc(strlen(systemroot) + strlen("/system32/config/") +
        strlen(*hiveshortname) + 1, 1);
    *hivefile = strcat(strcat(strcat(*hivefile, systemroot),
          "/system32/config/"),
        *hiveshortname);
    *new_path = strdup(path + MAX(idx0, idx1));
    if (*new_path == 0) {
      *new_path = strdup("\\");
    }
    *prefix = strdup(new_prefix);
    printf("match_and_extract for %s at %s SUCCEEDS.\n", path, systemroot);
  } else {
    printf("match_and_extract for %s at %s failed.\n", path, systemroot);
    return NULL;
  }

  return result;
}

char ** map_path_to_hive(char * path, char * systemroot) {
  char ** result = malloc(sizeof(char*) * 5);
  char ** hiveshortname = &result[0];
  char ** hivefile = &result[1];
  char ** new_path = &result[2];
  char ** prefix = &result[3];
  result[4] = NULL;

  int idx0 = 0, idx1 = 0;
  idx0 = str_has_prefix("\\HKEY_LOCAL_MACHINE\\SAM", path);
  idx1 = str_has_prefix("\\HKLM\\SAM", path);
  if (idx0 || idx1) {
    *hiveshortname = strdup("sam");
    *hivefile = calloc(strlen(systemroot) + strlen("/system32/config/") +
        strlen(*hiveshortname) + 1, 1);
    *hivefile = strcat(strcat(strcat(*hivefile, systemroot),
          "/system32/config/"),
        *hiveshortname);
    *new_path = strdup(path + MAX(idx0, idx1));
    if (*new_path == 0) {
      *new_path = strdup("\\");
    }
    *prefix = strdup("HKEY_LOCAL_MACHINE\\SAM");
  }

  char ** other_result;
  if (other_result = match_and_extract(path, systemroot,
      "HKEY_LOCAL_MACHINE\\SAM", "sam", "\\HKEY_LOCAL_MACHINE\\",
      "\\HKLM\\", "SAM")) {

    for (int k=0; k<4; k++) {
      printf("%s vs. %s\n", result[k], other_result[k]);
    }
    return other_result;
  }
  if (other_result = match_and_extract(path, systemroot,
      "HKEY_LOCAL_MACHINE\\SECURITY", "security", "\\HKEY_LOCAL_MACHINE\\",
      "\\HKLM\\", "SECURITY")) {

    for (int k=0; k<4; k++) {
      printf("lala: %s\n", other_result[k]);
    }
    return other_result;
  }
  if (other_result = match_and_extract(path, systemroot,
      "HKEY_LOCAL_MACHINE\\SOFTWARE", "software", "\\HKEY_LOCAL_MACHINE\\",
      "\\HKLM\\", "SOFTWARE")) {

    for (int k=0; k<4; k++) {
      printf("lala: %s\n", other_result[k]);
    }
    return other_result;
  }
  if (other_result = match_and_extract(path, systemroot,
      "HKEY_LOCAL_MACHINE\\SYSTEM", "system", "\\HKEY_LOCAL_MACHINE\\",
      "\\HKLM\\", "SYSTEM")) {

    for (int k=0; k<4; k++) {
      printf("lala: %s\n", other_result[k]);
    }
    return other_result;
  }
  if (other_result = match_and_extract(path, systemroot,
      "HKEY_LOCAL_MACHINE\\.DEFAULT", "default", "\\HKEY_USERS\\",
      "\\HKU\\", ".DEFAULT")) {

    for (int k=0; k<4; k++) {
      printf("lala: %s\n", other_result[k]);
    }
    return other_result;
  }
  // TODO, implement next case. now we need lookup_pip_of_user_sid. DONE(?)

//  printf("new_path=%s\n", *new_path);
  printf("idx=%i %i\n", idx0, idx1);
  return other_result;
}

char * _node_canonical_path(hive_h * h, hive_node_h node) {
  if (node == hivex_root(h)) {
    return strdup("\\");
  }

  char * nejm = hivex_node_name(h, node);
  hive_node_h parent = hivex_node_parent(h, node);
  char * path = _node_canonical_path(h, parent);
  if (!strcmp("\\", path)) {
    int l = strlen(path) + strlen(nejm) + 1;
    char * r = malloc(l);
    snprintf(r, l, "%s%s", path, nejm);
    return r;
  } else {
    int l = strlen(path) + strlen(nejm) + 2;
    char * r = malloc(l);
    snprintf(r, l, "%s\\%s", path, nejm);
    return r;
  }

}

static int cmp_hive_value_keysp(const void *p1, const void *p2,
    void *arg_hive) {
  hive_h *h = (hive_h *) arg_hive;
  return strcmp(hivex_value_key(h, *(hive_value_h const *) p1),
      hivex_value_key(h, *(hive_value_h const *) p2));
}

static int cmp_hive_node_namesp(const void *p1, const void *p2,
    void *arg_hive) {
  hive_h *h = (hive_h *) arg_hive;
  return strcmp(hivex_node_name(h, *(hive_node_h const *) p1),
      hivex_node_name(h, *(hive_node_h const *) p2));
}

static char * _escape_quotes(char *s) {
  char * s_it = s;
  char * new_s = malloc(strlen(s) * 2);
  char * n_it = new_s;

  while (s_it) {
    *n_it = *s_it;
    n_it++;
    if (*s_it != '\\') {
      *n_it = '\\';
      n_it++;
    } else {
      *n_it = '"';
      n_it++;
    }
    s_it++;
  }
  *n_it = 0;
  new_s = realloc(new_s, strlen(n_it) + 1);
  return new_s;
}

char * data_as_hex(char * data, size_t size) {
  char * begin = data;
  char * obuf = malloc(size * 3);
  while (data - begin < size) {
    obuf += sprintf(obuf, "%02x,", *data);
    data++;
  }
  *obuf = 0; // overwrite last comman and zero terminate
  return obuf;
}

void reg_export_node(hive_h * h, hive_node_h node, FILE * fh, char * prefix,
    int unsafe_printable_strings) {
  char * path = _node_canonical_path(h, node);
  fprintf(fh, "[");
  if (prefix) {
    if (prefix[strlen(prefix)-1] == '\\') {
      fprintf(fh, "%s", prefix);
    }
  }
  fprintf(fh, "%s", path);
  fprintf(fh, "]\n", path);
  // TODO unsage_printable_strings

  hive_value_h * values = hivex_node_values(h, node);
  hive_value_h * v_it = values;
  while(*v_it++); // count values
/*  while (values) {
    hivex_value_key *key = 
    values++;
  }*/
  qsort_r(values, v_it - values, sizeof(hive_value_h),
      cmp_hive_value_keysp, h);

  // print the values.
  v_it = values;
  while (v_it) {
    char * key = hivex_value_key(h, *v_it);
    hive_type _t; size_t _ts;
    int type = hivex_value_type(h, *v_it, &_t, &_ts);

    if (!strcmp(key, "")) {
      fprintf(fh, "@=");
    } else {
      fprintf(fh, "\"%s\"=", _escape_quotes(key));
    }

    if (type == 4 && hivex_value_struct_length(h, *v_it)) {
      int32_t dword = hivex_value_dword(h, *v_it);
      fprintf(fh, "dword:%08x\n", dword);
    } else if (unsafe_printable_strings && (type == 1 || type == 2)) {
      hive_type __t; size_t __ts; size_t o_len;
      char * val = hivex_value_value(h, *v_it, &__t, &__ts);
      char * o = _escape_quotes(_hivex_recode("UTF16LE", val, __ts,
            "UTF8", &o_len));
      fprintf(fh, "str(%x):\"%s\"\n", type, o);
    } else {
      hive_type __t; size_t __ts;
      char * val = hivex_value_value(h, *v_it, &__t, &__ts);
      fprintf(fh, "hex(%x):", type);
      char * hexout = data_as_hex(val, __ts);
      fprintf(fh, "%s\n", hexout);
    }
    v_it++;
  }
  fprintf(fh, "\n");

  hive_node_h * children = hivex_node_children(h, node);
  hive_node_h * c_it = children;
  while (c_it++);
  qsort_r(children, c_it - children, sizeof(hive_node_h),
      cmp_hive_node_namesp, h);

  while (children) {
    reg_export_node(h, *children, fh, prefix, unsafe_printable_strings);
    children++;
  }

}

char ** strsplit(char * str, char delim) {
  char * it = str;
  char ** result = malloc(sizeof(char **) * strlen(str));
  char ** rit = result;
  char * item_rit = *rit;
  char * last_found = it;
  while (*it) {
    if (*it == delim) {
      // end last string
      *item_rit = '\0';
      // reallocate, so we don't use more than we need
      *rit = realloc(*rit, strlen(*rit)+1);
      // alloc place for new item and advance pointer
      *(++rit) = malloc(strlen(it));
      // set current item
      item_rit = *rit;
      // advance
      it++;
      // and start over
      continue;
    }
    // copy current char
    *item_rit++ = *it++;
  }
  // use only space we should
  *rit = realloc(*rit, strlen(*rit) + 1);
  // zero terminate
  *item_rit = '\0';
  result = realloc(result, sizeof(char **) * (1 + rit - result));
  result[rit - result] = NULL;
  return result;
}

size_t parray_length(void ** ar) {
  int c = 0;
  while (*(char **)ar != NULL) {
    c++;
  }
  return c;
}

hive_node_h _node_lookup(hive_h * h, char * path) {
  char ** _path = strsplit(path, '\\');
  // advance if first path component is empty
  if (strcmp("", _path[0]) == 0 && parray_length((void**)_path) > 0) {
    _path++;
  }

  hive_node_h n = hivex_root(h);

  for (char * p = *_path; *_path != NULL; path++) {
    n = hivex_node_get_child(h, n, p);
    if (n == 0) {
      return 0;
    }
  }

  return n;
}

void reg_export(hive_h * h, char * key, char * prefix,
    int unsafe_printable_strings) {
  hive_node_h node;
  if (!(node = _node_lookup(h, key))) {
    fprintf(stderr, "[reg_export]: %s: path not found in this hive", key);
    exit(1);
  }

  FILE * fh = fopen("konskypenis.reg", "w+");

  reg_export_node(h, node, fh, prefix, unsafe_printable_strings);

  fclose(fh);

}

void export_mode(guestfs_h *g, char *path, char *name, char *systemroot, char *tmpdir) {
  char ** mapping = map_path_to_hive(path, systemroot);
  char * hiveshortname = mapping[0];
  char * hivefile = mapping[1];
  char * new_path = mapping[2];
  char * prefix = mapping[3];

  size_t flen = strlen(tmpdir) + strlen(hiveshortname) + 2;
  char * fil = calloc(flen, 1);
  snprintf(fil, flen, "%s/%s", tmpdir, hiveshortname);

  download_hive(g, hivefile, hiveshortname, tmpdir);
  hive_h * h = hivex_open(fil, HIVEX_OPEN_DEBUG);

  // TODO
  if (!name) { // @name was undef in perl
    reg_export(h, new_path, prefix, 1);
  } else {
    size_t cmdsize = strlen("hivexget") + strlen(tmpdir) + strlen(path) +
      strlen(name) + 4 + 1; // 3 spaces, 1 slash, 1 ZERO
    char * args = malloc(cmdsize);
    snprintf(args, cmdsize, "hivexget %s/%s %s", tmpdir, path, name);
    FILE * pf = popen(args, "r");
    if (!pf) {
      fprintf(stderr, "hivexget failed\n");
      perror(NULL);
      return EXIT_FAILURE;
    }
  }

};

void import_mode() {
  reg_import(stdin, );
}

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

  while (*osit != NULL) {
    printf("%s\n", *osit);
    osit++;
  };

  char ** mps = guestfs_inspect_get_mountpoints(g, oses[0]);
  char ** mpit = mps;

  if (!mps) {
    printf("guestfs_inspect_get_mountpoints failed\n");
    exit(1);
  }

  while (*mpit != NULL) {
    printf("%s\n", *mpit);
//    guestfs_mount_options(g, merge ? "" : "ro", *mpit, *(++mpit));
    mpit++; mpit++;
  }

  fprintf(stderr, "inspecting system root for windows\n");
  char * systemroot = guestfs_inspect_get_windows_systemroot(g, oses[0]);
  if (!systemroot) {
    fprintf(stderr, "system root neni windows pico\n");
    exit(1);
  }
  fprintf(stderr, "%s\n", systemroot);

  // create a tempdir
  fprintf(stderr, "creating tempdir!\n");
  char * tmpdir = malloc(strlen("virt-win-reg.XXXXXX") + 1);
  strcpy(tmpdir, "virt-win-reg.XXXXXX");
  tmpdir = mkdtemp(tmpdir);
  fprintf(stderr, "%s\n", tmpdir);

  fprintf(stderr, "shutting down!\n");

  char ** res = map_path_to_hive("\\HKU\\.DEFAULT\\chuj", systemroot);
  char ** resit = res;
  while (*resit != NULL) {
    printf("%s [%d]\n", *resit, resit - res);
    resit++;
    printf("picu\n");
  }
//  if (!merge) {
//    export_mode();
//  } else {
//    import_mode();
//  }


  printf("shutting down\n");
  guestfs_shutdown (g);
  guestfs_close (g);

  rmdir(tmpdir);

  return 0;
}
