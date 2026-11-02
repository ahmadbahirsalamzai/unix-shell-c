#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAX_ARGS 64   // Maximum number of arguments allowed in one command, including the command name. For example, ls -l /tmp has three arguments.
#define MAX_PIDS 128  // Maximum number of external commands that can be launched in parallel from one input line. The shell uses this limit to store child process IDs and wait for them later.
#define INTERACTIVE 0 // Represents interactive mode, where the shell reads commands from the keyboard and displays the wish> prompt.
#define BATCH 1       // Represents batch mode, where the shell reads commands from a file instead of standard input.

char *shell_name = "wish";
int mode = INTERACTIVE;

char initial_path[] = "/bin";
char *paths[MAX_ARGS] = {initial_path};
int path_count = 1;

/*
 * Print the required error message.
 */
void print_error(void)
{
    char *error_msg = "An error has occurred\n";
    write(STDERR_FILENO, error_msg, strlen(error_msg));
}
// return the pid or -1 for built-in commands and errors
pid_t run_command(char **args, int arg_count, char *redir_path)
{
    // exit command
    if (strcmp(args[0], "exit") == 0)
    {
        if (arg_count != 1)
        {
            print_error();
            return -1;
        }
        exit(0);
    }

    // cd command
    if (strcmp(args[0], "cd") == 0)
    {
        if (arg_count != 2)
        {
            print_error();
            return -1;
        }

        // change the current directory
        if (chdir(args[1]) != 0)
        {
            print_error();
            return -1;
        }

        return -1;
    }

    // path command
    if (strcmp(args[0], "path") == 0)
    {
        // freeing the old paths to avoid memory leaks before storing new ones
        for (int i = 0; i < path_count; i++)
        {
            if (paths[i] != NULL && paths[i] != initial_path)
            {
                free(paths[i]);
            }
        }

        path_count = 0;

        for (int i = 1; i < arg_count; i++)
        {
            paths[path_count] = strdup(args[i]);
            if (paths[path_count] == NULL)
            {
                print_error();
                return -1;
            }
            path_count++;
        }
        return -1;
    }

    char executable[4096];
    int found = 0;

    for (int i = 0; i < path_count; i++)
    {
        // checking if the full executable fits in the buffer -> avoiding buffer overflow
        if (strlen(paths[i]) + strlen(args[0]) + 2 > sizeof(executable))
        {
            continue;
        }

        // building the executable path
        strcpy(executable, paths[i]);
        strcat(executable, "/");
        strcat(executable, args[0]);

        // checking if executable exists
        if (access(executable, X_OK) == 0)
        {
            found = 1;
            break;
        }
    }

    if (found == 0)
    {
        print_error();
        return -1;
    }

    // create a child process
    pid_t pid = fork();

    if (pid < 0)
    {
        print_error();
        return -1;
    }

    if (pid == 0)
    {
        // redirect output if a filename was provided.
        if (redir_path != NULL)
        {
            int fd = open(redir_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);

            if (fd < 0)
            {
                print_error();
                exit(1);
            }

            if (dup2(fd, STDOUT_FILENO) < 0 || dup2(fd, STDERR_FILENO) < 0)
            {
                print_error();
                close(fd);
                exit(1);
            }

            close(fd);
        }

        execv(executable, args);

        // this only runs if execv() fails.
        print_error();
        exit(1);
    }

    return pid;
}

void process_line(char *line)
{
    pid_t pid_array[MAX_PIDS];
    int active_pids = 0;

    char *command;
    char *line_ptr = line;

    while ((command = strsep(&line_ptr, "&")) != NULL)
    {
        char *args[MAX_ARGS];
        int arg_count = 0;
        char *redir_path = NULL;

        char *redirect = strchr(command, '>');
        if (redirect != NULL)
        {
            // isolating the command from the output file name
            *redirect = '\0';

            // text after '>' will have the output file name
            redir_path = redirect + 1;

            // checking for another '>'
            if (strchr(redir_path, '>') != NULL)
            {
                print_error();
                continue;
            }

            // skipping spaces/tabs before the filename
            while (*redir_path == ' ' || *redir_path == '\t')
            {
                redir_path++;
            }

            // checking for missing file name
            if (*redir_path == '\0')
            {
                print_error();
                continue;
            }

            // checking the end of the filename
            char *end = redir_path;
            while (*end != '\0' && *end != ' ' && *end != '\t')
            {
                end++;
            }

            // checking if there's something after the filename
            if (*end != '\0')
            {
                *end = '\0';
                end++;

                while (*end == ' ' || *end == '\t')
                {
                    end++;
                }

                if (*end != '\0')
                {
                    print_error();
                    continue;
                }
            }
        }

        char *token;
        char *command_ptr = command;

        while ((token = strsep(&command_ptr, " \t")) != NULL)
        {
            // ignoring the empty tokens caused by extra whitespaces
            if (*token == '\0')
            {
                continue;
            }

            // arg_count shouldn't exceed MAX_ARGS
            if (arg_count >= MAX_ARGS - 1)
            {
                print_error();
                arg_count = 0;
                redir_path = NULL;
                break;
            }

            args[arg_count++] = token;
        }

        args[arg_count] = NULL;

        // skip empty commands and check for invalid redirection
        if (arg_count == 0)
        {
            if (redir_path != NULL)
            {
                print_error();
            }

            continue;
        }

        if (active_pids >= MAX_PIDS && strcmp(args[0], "exit") != 0 && strcmp(args[0], "cd") != 0 && strcmp(args[0], "path") != 0)
        {
            print_error();
            continue;
        }

        // execute the command and get the child PID
        pid_t pid = run_command(args, arg_count, redir_path);

        if (pid > 0)
        {
            if (active_pids < MAX_PIDS)
            {
                pid_array[active_pids++] = pid;
            }
            else
            {
                print_error();
            }
        }
    }

    // waiting for child processes to finish
    for (int i = 0; i < active_pids; i++)
    {
        waitpid(pid_array[i], NULL, 0);
    }
}

int main(int argc, char **argv)
{
    char *buffer = NULL;
    size_t buffer_size = 0;
    FILE *input = stdin;

    // checking if too many args were provided
    if (argc > 2)
    {
        print_error();
        exit(1);
    }

    // open batch file if one was provided
    if (argc == 2)
    {
        input = fopen(argv[1], "r");

        if (input == NULL)
        {
            print_error();
            exit(1);
        }

        mode = BATCH;
    }

    while (1)
    {
        // display shell name in interactive mode
        if (mode == INTERACTIVE)
        {
            printf("%s> ", shell_name);
            fflush(stdout);
        }

        if (getline(&buffer, &buffer_size, input) == -1)
        {
            break;
        }

        buffer[strcspn(buffer, "\n")] = '\0';

        process_line(buffer);
    }

    free(buffer);

    if (mode == BATCH)
    {
        fclose(input);
    }

    return 0;
}
