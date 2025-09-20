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
static void execute_piped_command(Command *cmd, struct c *pgm);
static void execute_command(Command *cmd);
void signal_handler(int signo);
void close_handler();
void stripwhite(char *);
int count_commands(Pgm *head);


static bool run_process = true; // Kills the process when ctrl+c

int main(void)
{
  signal(SIGINT, close_handler); // DENNA KALLAR PÅ FUNKTIONEN NÄR MAN TRYCKER CTRL+C
  signal(SIGCHLD, signal_handler); // HANDLER FÖR BAKGRUNDSPROCESSER
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
        print_cmd(&cmd);

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

          if (strcmp(line, "exit") == 0)
          {
            exit(0);
          }

          execute_command(&cmd);

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

int count_commands(Pgm *head) {
    int count = 0;
    Pgm *current = head;
    while (current != NULL) {
        count++;
        current = current->next;
    }
    return count;
}

void signal_handler(int signo) {
  if (signo == SIGCHLD) {
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
      for (int i = 0; i < bg_count; i++) {
        if (bg_children[i] == pid) {
          printf("\nBackground process %d finished\n", pid);
          // Remove the finished process from the array
          bg_children[i] = bg_children[bg_count - 1];
          bg_count--;
          break;
        }
      }
    }
  }
}

// Changes input to file if '<
static void Change_in(Command *cmd_list)
{
  int file_fd = open(cmd_list->rstdin, O_RDONLY);
  if (file_fd < 0)
  {
    perror("open stdin");
    exit(1);
  }
  dup2(file_fd, STDIN_FILENO);// redirect stout to read to the file
  close(file_fd);              // close the original fd
}

// Changes output to file if '>'
static void Change_out(Command *cmd_list)
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

static void command(Command *cmd)
{
  pid_t pid = fork();

  if (pid < 0)
  {
    perror("Fork failed");
    return;
  }
  else if (pid == 0)
  {
    printf("%d\n", count_commands(cmd->pgm));
    if (count_commands(cmd->pgm) > 1)
    {
      piped_command(cmd, cmd->pgm);
    }
    else
    {
      if (cmd->background)
      {
        signal(SIGINT, SIG_IGN);
      }
      if (cmd->rstdin)
      {
        Change_in(cmd);
      }
      if (cmd->rstdout)
      {
        Change_out(cmd);
      }
      char **args = cmd->pgm->pgmlist;
      if (execvp(args[0], args) < 0)
      {
        exit(EXIT_FAILURE);
      }
   }
  }
  else
  {
    if (!cmd->background)
    {
      waitpid(pid, NULL, 0);
    }
    else
    {
      printf("Started background process with PID: %d\n", pid);
      
    }
  }
}

// creates a fork and a pipe for each command in the piped command and executes them recursivly from the end of the list
static void piped_command(Command *cmd, struct c *pgm)
{
  int fd[2];
  if (pipe(fd) == -1)
  {
    perror("Pipe failed");
    return;
  }
  int pid = fork();

  if (pid < 0)
  {
    perror("Fork failed");
    return;
  }
  else if (pid == 0)
  {
    close(fd[0]);
    dup2(fd[1], STDOUT_FILENO);
    close(fd[1]);

    pgm = pgm->next;

    if (pgm->next == NULL)
    {
    char **args = pgm->pgmlist;
      if (execvp(args[0], args) < 0)
      {
        exit(EXIT_FAILURE);
      }
    }
    else if (pgm->next != NULL)
    {
      piped_command(cmd, pgm);
    }
  }
  else
  {

    close(fd[1]);
    dup2(fd[0], STDIN_FILENO);
    char **args = pgm->pgmlist;
    if (execvp(args[0], args) < 0)
    {
      exit(EXIT_FAILURE);
    }
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
