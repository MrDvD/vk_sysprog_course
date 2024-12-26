#include "parser.h"

#include <assert.h>
#include <stdio.h>
#include <unistd.h>

#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>

char **
normalize_args(char *cmd, char **args, int args_len)
{
   char **new_arr = malloc(2 * sizeof(char**) + sizeof(args));
   new_arr[0] = cmd;
   for (int i = 1; i <= args_len; i++)
   {
      new_arr[i] = args[i - 1];
   }
   new_arr[args_len + 1] = NULL;
   return new_arr;
}

static void
execute_command_line(const struct command_line *line)
{
	/* REPLACE THIS CODE WITH ACTUAL COMMAND EXECUTION */

	assert(line != NULL);
	
	const struct expr *e = line->head;
	while (e != NULL) {
		if (e->type == EXPR_TYPE_COMMAND) {
         // printf("start\n");
         if (strcmp((const char *) e->cmd.exe, "cd") == 0)
            chdir(e->cmd.args[0]);
         else
         {
            if (fork() == 0)
               execvp(e->cmd.exe, normalize_args(e->cmd.exe, e->cmd.args, e->cmd.arg_count));
         }
		} else if (e->type == EXPR_TYPE_PIPE) {
			printf("\tPIPE\n");
		} else if (e->type == EXPR_TYPE_AND) {
			printf("\tAND\n");
		} else if (e->type == EXPR_TYPE_OR) {
			printf("\tOR\n");
		} else {
			assert(false);
		}
		e = e->next;
	}
}

int
main(void)
{
	const size_t buf_size = 1024;
	char buf[buf_size];
	int rc;
	struct parser *p = parser_new();
	while ((rc = read(STDIN_FILENO, buf, buf_size)) > 0) {
		parser_feed(p, buf, rc);
		struct command_line *line = NULL;
		while (true) {
			enum parser_error err = parser_pop_next(p, &line);
			if (err == PARSER_ERR_NONE && line == NULL)
				break;
			if (err != PARSER_ERR_NONE) {
				printf("Error: %d\n", (int)err);
				continue;
			}
			execute_command_line(line);
			command_line_delete(line);
		}
	}
	parser_delete(p);
	return 0;
}
