# Report Lab 1

## Intoduction

In this lab some basic functions were added to a shell. This report shows our thought process for the different implementations. The order of the sections is the same order in which we did the implementations.

## Basic Commands

We started with the implementation of basic commands because it felt like a natural starting point and something we could build from. At first, we misunderstood the instructions and started implementing the basic commands on our own, for example by calling Date() if the input line was “date”. However, after some assistance from the teaching assistant we realized that using the execvp() function was the way to go. This allowed us to fork a process and let the child process handle the commands while the parent waits for the child to finish. Once the child completes, the parent continues running the shell loop and is ready for the next command. We chose the execvp() function because of its automatic PATH lookup and since it fit the way the parser gave us commands (arguments as a vector).

## Background Execution

The first thought was that we were supposed to create the ‘&&’ command, which runs two commands if the earlier of them is successful. However, after speaking with the teaching assistant, we realized what the background process meant. This discussion made us realize that the difference now from the earlier implementation for the basic commands was that the parent process does not need to wait for the child process to finish. The first implementation of background execution worked by checking the input string for an ampersand. If this were the case parent process did not wait for the child process to finish. Looking if the line included an ampersand was not a good solution, which we changed to look at the ‘background’ information of the parsed command.

```
singal(SINGINT, SIG_IGN);
```

## Piping

Our first approach was to check if we have more than one command in cmd list and if so, create a pipe between the first and second commands. The first command was to be executed by a child process and the second was to be executed by another child process. When we ran the tests we found that making a pipe between 2 commands was not enough and we needed a solution that could create many pipes for commands like ls | grep lsh | sort -r . Therefore we scrapped our first hardcoded pipe creation and created a new one that utilized a for loop over the cmd list to create both pipes and execute commands. Here we ran into the problem of executing the commands in the wrong order due to the fact that the commands need to be run in the opposite order from the cmd list. Here we chose to go for a third strategy, Recursion. With recursion we could go into the list as far as possible and execute the selected command only if it was the last in the list. Using recursion allowed us to execute the commands in the right order and saved us the hassle of having to modify the cmd variable.

## I/O Redirection

For I/O redirection we created two functions, Chamne_in and Change_out. Chamne_in and Chamne_in change the input and output from STDIN_FILENO, STDOUT_FILENO ( the standard file descriptor) to file_fd ( the file descriptor that we defined when using the < or > command)

## Built-ins

For the built-in functions cd and exit we wanted to check if the line included these strings. This was first implemented using

```
strcmp(line,”cd”)
```

This would return zero if the line contained “cd”. This gave an error when using the input “echo cd”. So it was instead changed to

```
cmd.pgm->pgmlist[0] && strcmp(cmd.pgm->pgmlist[0], "cd")
```

which fixed this issue. If this were true, the chdir() command would be used to change the directory depending on the input. The exit function however still uses the strcmp() and calls the exit(0) command. This could be improved by also using the same structure as the "cd" command.

## Ctrl-D Handling

Each time a command is entered in the shell, the line will contain information about this command. But if Ctrl+D is pressed instead, this line will be empty. This means that at the beginning of each new command, we check the line with

```
if (line == NULL){
exit(0);
}
```

If this is the case, we close the shell. Since other commands like Ctrl+C use other signal handlers, these will not cause an issue with this.

## Ctrl-C Handling

We only use Ctrl-C handling if the process in question is a foreground process which is inherited. We do not want to stop the background process if we press Ctrl-C. We suppress this with the following code :

```
​​     if (cmd->background)
     {
       signal(SIGINT, SIG_IGN);
     }
```

Here the background process ignores the normal SIGINT behaviour (stopping the process with Ctrl-C).

## Feedback

Overall, we found the lab materials straightforward and helpful, and the specifications were clearly defined. The automated tests were very useful and allowed us to confirm correctness for each step. It also helped us find different bugs and zombie processes that we might otherwise have missed.

The tests could, of course, be extended with a few more edge cases. For example running cd with an empty or invalid path, or testing whether commands like “echo cd” would trigger any errors. At the moment, the tests for pipes are limited to a maximum of three commands. Including a test with four or five commands could provide additional validation, especially for more complex pipelines. It would also be useful to include tests for combinations of input and output redirection with multiple commands.
