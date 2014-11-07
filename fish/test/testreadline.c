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


/* This is the function to call when the word to complete is in a position
   where a command word can be found.  It grovels $PATH, looking for commands
   that match.  It also scans aliases, function names, and the shell_builtin
   table. */
char *
command_word_completion_function (hint_text, state)
     const char *hint_text;
     int state;
{
  static char *hint = (char *)NULL;
  static char *path = (char *)NULL;
  static char *val = (char *)NULL;
  static char *filename_hint = (char *)NULL;
  static char *dequoted_hint = (char *)NULL;
  static char *directory_part = (char *)NULL;
  static char **glob_matches = (char **)NULL;
  static int path_index, hint_len, dequoted_len, istate, igncase;
  static int mapping_over, local_index, searching_path, hint_is_dir;
  static int old_glob_ignore_case, globpat;
  static SHELL_VAR **varlist = (SHELL_VAR **)NULL;
#if defined (ALIAS)
  static alias_t **alias_list = (alias_t **)NULL;
#endif /* ALIAS */
  char *temp;

  /* We have to map over the possibilities for command words.  If we have
     no state, then make one just for that purpose. */
  if (state == 0)
    {
      if (dequoted_hint && dequoted_hint != hint)
	free (dequoted_hint);
      if (hint)
	free (hint);

      mapping_over = searching_path = 0;
      hint_is_dir = CMD_IS_DIR (hint_text);
      val = (char *)NULL;

      temp = rl_variable_value ("completion-ignore-case");
      igncase = RL_BOOLEAN_VARIABLE_VALUE (temp);

      if (glob_matches)
	{
	  free (glob_matches);
	  glob_matches = (char **)NULL;
	}

      globpat = glob_pattern_p (hint_text);

      /* If this is an absolute program name, do not check it against
	 aliases, reserved words, functions or builtins.  We must check
	 whether or not it is unique, and, if so, whether that filename
	 is executable. */
      if (globpat || absolute_program (hint_text))
	{
	  /* Perform tilde expansion on what's passed, so we don't end up
	     passing filenames with tildes directly to stat(). */
	  if (*hint_text == '~')
	    {
	      hint = bash_tilde_expand (hint_text, 0);
	      directory_part = savestring (hint_text);
	      temp = strchr (directory_part, '/');
	      if (temp)
		*temp = 0;
	      else
		{
		  free (directory_part);
		  directory_part = (char *)NULL;
		}
	    }
	  else
	    hint = savestring (hint_text);

	  dequoted_hint = hint;
	  /* If readline's completer found a quote character somewhere, but
	     didn't set the quote character, there must have been a quote
	     character embedded in the filename.  It can't be at the start of
	     the filename, so we need to dequote the filename before we look
	     in the file system for it. */
	  if (rl_completion_found_quote && rl_completion_quote_character == 0)
	    {
	      dequoted_hint = bash_dequote_filename (hint, 0);
	      free (hint);
	      hint = dequoted_hint;
	    }
	  dequoted_len = hint_len = strlen (hint);

	  if (filename_hint)
	    free (filename_hint);

	  filename_hint = savestring (hint);

	  istate = 0;

	  if (globpat)
	    {
	      mapping_over = 5;
	      goto globword;
	    }
	  else
	    {
	      mapping_over = 4;
	      goto inner;
	    }
	}

      dequoted_hint = hint = savestring (hint_text);
      dequoted_len = hint_len = strlen (hint);

      if (rl_completion_found_quote && rl_completion_quote_character == 0)
	{
	  dequoted_hint = bash_dequote_filename (hint, 0);
	  dequoted_len = strlen (dequoted_hint);
	}
      
      path = get_string_value ("PATH");
      path_index = dot_in_path = 0;

      /* Initialize the variables for each type of command word. */
      local_index = 0;

      if (varlist)
	free (varlist);

      varlist = all_visible_functions ();

#if defined (ALIAS)
      if (alias_list)
	free (alias_list);

      alias_list = all_aliases ();
#endif /* ALIAS */
    }

  /* mapping_over says what we are currently hacking.  Note that every case
     in this list must fall through when there are no more possibilities. */

  switch (mapping_over)
    {
    case 0:			/* Aliases come first. */
#if defined (ALIAS)
      while (alias_list && alias_list[local_index])
	{
	  register char *alias;

	  alias = alias_list[local_index++]->name;

	  if (STREQN (alias, hint, hint_len))
	    return (savestring (alias));
	}
#endif /* ALIAS */
      local_index = 0;
      mapping_over++;

    case 1:			/* Then shell reserved words. */
      {
	while (word_token_alist[local_index].word)
	  {
	    register char *reserved_word;

	    reserved_word = word_token_alist[local_index++].word;

	    if (STREQN (reserved_word, hint, hint_len))
	      return (savestring (reserved_word));
	  }
	local_index = 0;
	mapping_over++;
      }

    case 2:			/* Then function names. */
      while (varlist && varlist[local_index])
	{
	  register char *varname;

	  varname = varlist[local_index++]->name;

	  if (STREQN (varname, hint, hint_len))
	    return (savestring (varname));
	}
      local_index = 0;
      mapping_over++;

    case 3:			/* Then shell builtins. */
      for (; local_index < num_shell_builtins; local_index++)
	{
	  /* Ignore it if it doesn't have a function pointer or if it
	     is not currently enabled. */
	  if (!shell_builtins[local_index].function ||
	      (shell_builtins[local_index].flags & BUILTIN_ENABLED) == 0)
	    continue;

	  if (STREQN (shell_builtins[local_index].name, hint, hint_len))
	    {
	      int i = local_index++;

	      return (savestring (shell_builtins[i].name));
	    }
	}
      local_index = 0;
      mapping_over++;
    }

globword:
  /* Limited support for completing command words with globbing chars.  Only
     a single match (multiple matches that end up reducing the number of
     characters in the common prefix are bad) will ever be returned on
     regular completion. */
  if (globpat)
    {
      if (state == 0)
	{
	  glob_ignore_case = igncase;
	  glob_matches = shell_glob_filename (hint);
	  glob_ignore_case = old_glob_ignore_case;

	  if (GLOB_FAILED (glob_matches) || glob_matches == 0)
	    {
	      glob_matches = (char **)NULL;
	      return ((char *)NULL);
	    }

	  local_index = 0;
		
	  if (glob_matches[1] && rl_completion_type == TAB)	/* multiple matches are bad */
	    return ((char *)NULL);
	}

      while (val = glob_matches[local_index++])
        {
	  if (executable_or_directory (val))
	    {
	      if (*hint_text == '~')
		{
		  temp = restore_tilde (val, directory_part);
		  free (val);
		  val = temp;
		}
	      return (val);
	    }
	  free (val);
        }

      glob_ignore_case = old_glob_ignore_case;
      return ((char *)NULL);
    }

  /* If the text passed is a directory in the current directory, return it
     as a possible match.  Executables in directories in the current
     directory can be specified using relative pathnames and successfully
     executed even when `.' is not in $PATH. */
  if (hint_is_dir)
    {
      hint_is_dir = 0;	/* only return the hint text once */
      return (savestring (hint_text));
    }
    
  /* Repeatedly call filename_completion_function while we have
     members of PATH left.  Question:  should we stat each file?
     Answer: we call executable_file () on each file. */
 outer:

  istate = (val != (char *)NULL);

  if (istate == 0)
    {
      char *current_path;

      /* Get the next directory from the path.  If there is none, then we
	 are all done. */
      if (path == 0 || path[path_index] == 0 ||
	  (current_path = extract_colon_unit (path, &path_index)) == 0)
	return ((char *)NULL);

      searching_path = 1;
      if (*current_path == 0)
	{
	  free (current_path);
	  current_path = savestring (".");
	}

      if (*current_path == '~')
	{
	  char *t;

	  t = bash_tilde_expand (current_path, 0);
	  free (current_path);
	  current_path = t;
	}

      if (current_path[0] == '.' && current_path[1] == '\0')
	dot_in_path = 1;

      if (filename_hint)
	free (filename_hint);

      filename_hint = sh_makepath (current_path, hint, 0);
      free (current_path);		/* XXX */
    }

 inner:
  val = rl_filename_completion_function (filename_hint, istate);
  istate = 1;

  if (val == 0)
    {
      /* If the hint text is an absolute program, then don't bother
	 searching through PATH. */
      if (absolute_program (hint))
	return ((char *)NULL);

      goto outer;
    }
  else
    {
      int match, freetemp;

      if (absolute_program (hint))
	{
	  if (igncase == 0)
	    match = strncmp (val, hint, hint_len) == 0;
	  else
	    match = strncasecmp (val, hint, hint_len) == 0;

	  /* If we performed tilde expansion, restore the original
	     filename. */
	  if (*hint_text == '~')
	    temp = restore_tilde (val, directory_part);
	  else
	    temp = savestring (val);
	  freetemp = 1;
	}
      else
	{
	  temp = strrchr (val, '/');

	  if (temp)
	    {
	      temp++;
	      if (igncase == 0)
		freetemp = match = strncmp (temp, hint, hint_len) == 0;
	      else
		freetemp = match = strncasecmp (temp, hint, hint_len) == 0;
	      if (match)
		temp = savestring (temp);
	    }
	  else
	    freetemp = match = 0;
	}

#if 0
      /* If we have found a match, and it is an executable file or a
	 directory name, return it. */
      if (match && executable_or_directory (val))
#else
      /* If we have found a match, and it is an executable file, return it.
	 We don't return directory names when searching $PATH, since the
	 bash execution code won't find executables in directories which
	 appear in directories in $PATH when they're specified using
	 relative pathnames. */
      if (match && (searching_path ? executable_file (val) : executable_or_directory (val)))
#endif
	{
	  free (val);
	  val = "";		/* So it won't be NULL. */
	  return (temp);
	}
      else
	{
	  if (freetemp)
	    free (temp);
	  free (val);
	  goto inner;
	}
    }
}

static int
filename_completion_ignore (names)
     char **names;
{
  return 0;
}

static int
bash_complete_filename_internal (what_to_do)
     int what_to_do;
{
  rl_compentry_func_t *orig_func;
  rl_completion_func_t *orig_attempt_func;
  rl_icppfunc_t *orig_dir_func;
  rl_compignore_func_t *orig_ignore_func;
  /*const*/ char *orig_rl_completer_word_break_characters;
  int r;

  orig_func = rl_completion_entry_function;
  orig_attempt_func = rl_attempted_completion_function;
  orig_dir_func = rl_directory_rewrite_hook;
  orig_ignore_func = rl_ignore_some_completions_function;
  orig_rl_completer_word_break_characters = rl_completer_word_break_characters;
  rl_completion_entry_function = rl_filename_completion_function;
  rl_attempted_completion_function = (rl_completion_func_t *)NULL;
  rl_directory_rewrite_hook = (rl_icppfunc_t *)NULL;
  rl_ignore_some_completions_function = filename_completion_ignore;
  rl_completer_word_break_characters = " \t\n\"\'";

  r = rl_complete_internal (what_to_do);

  rl_completion_entry_function = orig_func;
  rl_attempted_completion_function = orig_attempt_func;
  rl_directory_rewrite_hook = orig_dir_func;
  rl_ignore_some_completions_function = orig_ignore_func;
  rl_completer_word_break_characters = orig_rl_completer_word_break_characters;

  return r;
}

static int
bash_possible_filename_completions (ignore, ignore2)
     int ignore, ignore2;
{
  return bash_complete_filename_internal ('?');
}


static int
bash_complete_filename (ignore, ignore2)
     int ignore, ignore2;
{
  return bash_complete_filename_internal (rl_completion_mode (bash_complete_filename));
}

static int
bash_specific_completion (what_to_do, generator)
     int what_to_do;
     rl_compentry_func_t *generator;
{
  rl_compentry_func_t *orig_func;
  rl_completion_func_t *orig_attempt_func;
  rl_compignore_func_t *orig_ignore_func;
  int r;

  orig_func = rl_completion_entry_function;
  orig_attempt_func = rl_attempted_completion_function;
  orig_ignore_func = rl_ignore_some_completions_function;
  rl_completion_entry_function = generator;
  rl_attempted_completion_function = NULL;
  rl_ignore_some_completions_function = orig_ignore_func;

  r = rl_complete_internal (what_to_do);

  rl_completion_entry_function = orig_func;
  rl_attempted_completion_function = orig_attempt_func;
  rl_ignore_some_completions_function = orig_ignore_func;

  return r;
}


static int
bash_complete_command_internal (what_to_do)
     int what_to_do;
{
  return bash_specific_completion (what_to_do, command_word_completion_function);
}

static int
bash_complete_command (ignore, ignore2)
     int ignore, ignore2;
{
  return bash_complete_command_internal (rl_completion_mode (bash_complete_command));
}

static int
bash_possible_command_completions (ignore, ignore2)
     int ignore, ignore2;
{
  return bash_complete_command_internal ('?');
}


void initialize_readline() {
  rl_add_defun ("complete-filename", bash_complete_filename, -1);
  rl_add_defun ("possible-filename-completions", bash_possible_filename_completions, -1);

  rl_add_defun ("complete-command", bash_complete_command, -1);
  rl_add_defun ("possible-command-completions", bash_possible_command_completions, -1);


  if (RL_ISSTATE(RL_STATE_INITIALIZED) == 0)
      rl_initialize ();

  rl_completer_quote_characters = "'\"";

  /* This sets rl_completer_word_break_characters and rl_special_prefixes
     to the appropriate values, depending on whether or not hostname
     completion is enabled. */
//  enable_hostname_completion (perform_hostname_completion);

  /* characters that need to be quoted when appearing in filenames. */
  rl_filename_quote_characters = " \t\n\\\"'@<>=;|&()#$`?*[!:{~"; /*}*/

//  rl_filename_quoting_function = bs_escape_filename;
//  rl_filename_dequoting_function = bs_unescape_filename;
//  rl_char_is_quoted_p = char_is_quoted;
}

void cleanup_readline() {
}

int main(void) {
  initialize_readline ();

  cleanup_readline();
  return EXIT_SUCCESS;
}
