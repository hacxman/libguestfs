#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
//#include <hivex.h>
#include <getopt.h>
#include <guestfs.h>

void usage(char *pname) {
  printf("%s: usage\n", pname);
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
      {"help", no_argument, 0, 'h'},
      {"version", no_argument, 0, 'v'},
      {"connect", required_argument, 0, 'c'},
      {"debug", no_argument, 0, 'd'},
      {"format", required_argument, 0, 'f'},
      {"merge", no_argument, 0, 'm'},
      {"encoding", required_argument, 0, 'e'},
      {"unsafe-printable-strings", no_argument, 0, 'u'},
      {0,      0,           0, 0}
    };
    int option_index = 0;

    int c = getopt_long(argc, argv, "hvc:df:me:u", long_options, &option_index);
    if (c == -1)
      break;

    switch (c) {
      case 0:
        printf("0\n");
        break;
      case 'h':
        usage(argv[0]);
        exit(0);
        break;
      case 'v':
        printf("version\n");
        exit(0);
        break;
      case 'c':
        printf("connect %s %s\n", long_options[option_index].name, optarg);
        strcpy(connect, optarg);
        break;
      case 'd':
        printf("debug\n");
        debug = 1;
        break;
      case 'f':
        printf("format\n");
        strcpy(format, optarg);
        break;
      case 'm':
        printf("merge\n");
        merge = 1;
        break;
      case 'e':
        printf("encoding\n");
        strcpy(encoding, optarg);
        break;
      case 'u':
        printf("unsagebvlabasl\n");
        unsafe_printable_strings = 1;
        break;
      case '?':
        printf("?\n");
        break;
      default:
        printf("default, c=%g, option_index=%g,%s\n",c
            ,option_index,long_options[option_index].name);
        break;
    }
  }

  if (argc - optind < 2) {
    //fprintf(stderr, "\n");
    usage(argv[0]);
    exit(1);
  }

  char * domname_or_image = argv[optind];
  char * key = argv[optind+1];

  guestfs_h *g = guestfs_create();
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
  } while (++osit != NULL);

}
