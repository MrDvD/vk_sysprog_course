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
      new_arr[i] = args[i - 1];
   new_arr[args_len + 1] = (char*) NULL;
   return new_arr;
}

static void
execute_command_line(const struct command_line *line)
{
	/* REPLACE THIS CODE WITH ACTUAL COMMAND EXECUTION */
	const struct expr *e = line->head;
   int pipe_status[2] = {0, 0};
   int pipe_idx = 0;
   int fd[2][2];
	while (e != NULL) {
		if (e->type == EXPR_TYPE_COMMAND) {
         if (e->next != NULL && e->next->type == EXPR_TYPE_PIPE)
         {
            pipe(fd[pipe_idx]);
            pipe_status[pipe_idx] = 1;
            pipe_idx = (pipe_idx + 1) % 2;
         } else
         {
            if (strcmp((const char *) e->cmd.exe, "cd") == 0)
               chdir(e->cmd.args[0]);
            else if (strcmp((const char *) e->cmd.exe, "exit") == 0)
               exit(0);
         }
         int pid = fork();
         if (pid == 0) // child
         {
            for (int i = 0; i < 2; i++)
            {
               if (pipe_status[(pipe_idx + i) % 2] == 1) // now &1 points to fd[1]
               {
                  close(fd[(pipe_idx + i) % 2][0]);
                  dup2(fd[(pipe_idx + i) % 2][1], 1);
                  // close(1);
                  // dup(fd[1]);
                  close(fd[(pipe_idx + i) % 2][1]);
               }
               else if (pipe_status[(pipe_idx + i) % 2] == 2) // need to point fd[0] to &0
               {
                  close(fd[(pipe_idx + i) % 2][1]);
                  dup2(fd[(pipe_idx + i) % 2][0], 0);
                  // close(0);
                  // dup(fd[0]);
                  close(fd[(pipe_idx + i) % 2][0]);
               }
            }
            execvp(e->cmd.exe, normalize_args(e->cmd.exe, e->cmd.args, e->cmd.arg_count));
         }
         // parent
         // wait(NULL); // ISSUE: possibly top-level process is finished earlier than children ones.
         for (int i = 0; i < 2; i++)
         {
            if (pipe_status[i])
               pipe_status[i] = (pipe_status[i] + 1) % 3;
         }
		} else if (e->type == EXPR_TYPE_AND) {
			printf("\tAND\n");
		} else if (e->type == EXPR_TYPE_OR) {
			printf("\tOR\n");
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
