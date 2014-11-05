#include <config.h>

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <locale.h>
#include <langinfo.h>
#include <libintl.h>

#ifdef HAVE_LIBREADLINE
#include <readline/readline.h>
#include <readline/history.h>
#endif

#include <guestfs.h>

#include <rl.h>

void initialize_readline() {
  rl_add_defun ("complete-filename", bash_complete_filename, -1);
  rl_add_defun ("possible-filename-completions", bash_possible_filename_completions, -1);

  if (RL_ISSTATE(RL_STATE_INITIALIZED) == 0)
      rl_initialize ();

  rl_completer_quote_characters = "'\"";

  /* This sets rl_completer_word_break_characters and rl_special_prefixes
     to the appropriate values, depending on whether or not hostname
     completion is enabled. */
  enable_hostname_completion (perform_hostname_completion);

  /* characters that need to be quoted when appearing in filenames. */
  rl_filename_quote_characters = " \t\n\\\"'@<>=;|&()#$`?*[!:{~"; /*}*/

  rl_filename_quoting_function = bs_escape_filename;
  rl_filename_dequoting_function = bs_unescape_filename;
  rl_char_is_quoted_p = char_is_quoted;
}

void cleanup_readline() {
}

int main(void) {
  initialize_readline ();

  cleanup_readline();
  return EXIT_SUCCESS;
}
