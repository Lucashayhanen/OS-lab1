/*
 * Main source code file for lsh shell program
 *
 * You are free to add functions to this file.
 * If you want to add functions in a separate file(s)
 * you will need to modify the CMakeLists.txt to compile
 * your additional file(s).
 *
 * Add appropriate comments in your code to make it
 * easier for us while grading your assignment.
 *
 * Using assert statements in your code is a great way to catch errors early and make debugging easier.
 * Think of them as mini self-checks that ensure your program behaves as expected.
 * By setting up these guardrails, you're creating a more robust and maintainable solution.
 * So go ahead, sprinkle some asserts in your code; they're your friends in disguise!
 *
 * All the best!
 */
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>

// The <unistd.h> header is your gateway to the OS's process management facilities.
#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include "parse.h"
#include <sys/types.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <errno.h>


#define BUFFER_SIZE 25
#define READ_END 0
#define WRITE_END 1
#define MAX_BG 64
pid_t bg_children[MAX_BG];
int bg_count = 0;

static void Change_in(Command *cmd_list);
static void Change_out(Command *cmd_list);
static void print_cmd(Command *cmd);
static void print_pgm(Pgm *p);
void sigchld_handler(int signo);
void close_handler();
void stripwhite(char *);

static bool run_process = true; // Kills the process when ctrl+c

int main(void)
{
  signal(SIGINT, close_handler); // DENNA KALLAR PÅ FUNKTIONEN NÄR MAN TRYCKER CTRL+C
  signal(SIGCHLD, sigchld_handler); // HANDLER FÖR BAKGRUNDSPROCESSER
  for (;;)
  {
    char *line;
    line = readline("> ");

    if (line == NULL) {
      // EOF detected (Ctrl+D), exit cleanly
      printf("\n");
      exit(0);   
    }
    // Remove leading and trailing whitespace from the line
    stripwhite(line);

    // If stripped line not blank
    if (*line)
    {
      add_history(line);
      Command cmd;

      if (parse(line, &cmd) == 1)
      {
        //print_cmd(&cmd);
        int fd[2];

        if (cmd.pgm->next != NULL)
        {
          if (pipe(fd) == -1)
          {
            fprintf(stderr, "Pipe failed");
            return 1;
          }

          pid_t pid1, pid2;
          pid1 = fork();

          if (pid1 < 0)
          {
            printf("FORK ERROR\n");
          }
          else if (pid1 == 0)
          { // Child process 1 - SKA SKRIVA
            close(fd[READ_END]);
            dup2(fd[WRITE_END], STDOUT_FILENO);
            close(fd[WRITE_END]);
            Change_in(&cmd);
            execvp(cmd.pgm->next->pgmlist[0], cmd.pgm->next->pgmlist);
          }
          else // Parent process
          {
            pid2 = fork();
            if (pid2 < 0)
            {
              printf("FORK ERROR 2\n");
            }
            else if (pid2 == 0)
            { // Child process 2 - SKA LÄSA
              // waitpid(pid1, NULL, 0);
              close(fd[WRITE_END]);
              dup2(fd[READ_END], STDIN_FILENO);
              close(fd[READ_END]);
              Change_out(&cmd);
              execvp(cmd.pgm->pgmlist[0], cmd.pgm->pgmlist);
            }
            else
            {
              close(fd[READ_END]);
              close(fd[WRITE_END]);
              waitpid(pid1, NULL, 0);
              waitpid(pid2, NULL, 0);
            }
          }
        }
        else
        {
          if (cmd.pgm->pgmlist[0] && strcmp(cmd.pgm->pgmlist[0], "cd") == 0) // Changing directory.
          {
            if (chdir(cmd.pgm->pgmlist[1]) != 0)
            {
              perror("Changing directory failed\n");
            }
            else
            {
              printf("Changed directory\n");
            }
          }
          else if (strcmp(line, "exit") == 0)
          {
            exit(0);
          }
          else
          {
            pid_t pid;
            pid = fork();

            if (pid < 0)
            {
              printf("FORK ERROR\n");
            }
            else if (pid == 0)
            {
              while (run_process)
              {
                Change_in(&cmd);
                Change_out(&cmd);
                execvp(cmd.pgm->pgmlist[0], cmd.pgm->pgmlist);
              }
              _Exit(EXIT_SUCCESS);
            }
            else
            {
              if (!cmd.background)
              {
                waitpid(pid, NULL, 0);
                run_process = true;
              }
              else
              {
                if (bg_count < MAX_BG) {
                  bg_children[bg_count++] = pid;
                printf("BAKGRUND PROCESS\n");
                }
              }
            }
          }
        }
      }
      else
      {
        printf("Parse ERROR\n");
      }
    }
    // Clear memory
    free(line);
  }
  return 0;
}

void close_handler()
{
  run_process = false;
}

void sigchld_handler(int signo) {
    int saved_errno = errno;
    int status;
    pid_t pid;

    for (int i = 0; i < bg_count; i++) {
        pid = waitpid(bg_children[i], &status, WNOHANG);
        if (pid > 0) {
            // Remove reaped child from array
            bg_children[i] = bg_children[bg_count - 1];
            bg_count--;
            i--; // check the swapped-in PID
        }
    }

    errno = saved_errno;
}

static void Change_in(Command *cmd_list)
{ // Changes input to file if '<'
  if (cmd_list->rstdin)
  {
    int file_fd = open(cmd_list->rstdin, O_RDONLY);
    if (file_fd < 0)
    {
      perror("open stdin");
      exit(1);
    }
    dup2(file_fd, STDIN_FILENO);
    close(file_fd);
  }
}

static void Change_out(Command *cmd_list)
{ // Changes output to file if '>'
  if (cmd_list->rstdout)
  {
    int file_fd = open(cmd_list->rstdout, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (file_fd < 0)
    {
      perror("open");
      exit(1);
    }
    dup2(file_fd, STDOUT_FILENO); // redirect stout to read from the file
    close(file_fd);               // close the original fd
  }
}

/*
 * Print a Command structure as returned by parse on stdout.
 *
 * Helper function, no need to change. Might be useful to study as inspiration.
 */
static void print_cmd(Command *cmd_list)
{
  printf("------------------------------\n");
  printf("Parse OK\n");
  printf("stdin:      %s\n", cmd_list->rstdin ? cmd_list->rstdin : "<none>");
  printf("stdout:     %s\n", cmd_list->rstdout ? cmd_list->rstdout : "<none>");
  printf("background: %s\n", cmd_list->background ? "true" : "false");
  printf("Pgms:\n");
  print_pgm(cmd_list->pgm);
  printf("------------------------------\n");
}

/* Print a (linked) list of Pgm:s.
 *
 * Helper function, no need to change. Might be useful to study as inpsiration.
 */
static void print_pgm(Pgm *p)
{
  if (p == NULL)
  {
    return;
  }
  else
  {
    char **pl = p->pgmlist;

    /* The list is in reversed order so print
     * it reversed to get right
     */
    print_pgm(p->next);
    printf("            * [ ");
    while (*pl)
    {
      printf("%s ", *pl++);
    }
    printf("]\n");
  }
}

/* Strip whitespace from the start and end of a string.
 *
 * Helper function, no need to change.
 */
void stripwhite(char *string)
{
  size_t i = 0;

  while (isspace(string[i]))
  {
    i++;
  }

  if (i)
  {
    memmove(string, string + i, strlen(string + i) + 1);
  }

  i = strlen(string) - 1;
  while (i > 0 && isspace(string[i]))
  {
    i--;
  }

  string[++i] = '\0';
}
