// If you are not compiling with the gcc option --std=gnu99, then
// uncomment the following line or you might get a compiler warning
//#define _GNU_SOURCE

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <fcntl.h>
#include <wait.h>
#include <signal.h>

#define MAX_CHAR_LENGTH 2048
#define MAX_ARGS 512

// Global variables
int backgroundEnabled = 0;
int foregroundEnabled = 0;
int childExitStatus = 0;
int statusNum;
int signalsNum;
int backgroundPIDS[101] = { 0 };

/*
*   Signal handler function for SIGTSTP and enables/disables foreground mode
*   and writes to the shell.
*   Inspiration taken from Exploration: Signal Handling API
*/
void handle_SIGTSTP(){
    if (foregroundEnabled == 0)
    {
        char* message = "Entering foreground mode (disabling &).\n";

        // Use write instead of printf
        write(STDOUT_FILENO, message, 41);

        backgroundEnabled = 0;
        foregroundEnabled = 1;
    }
    else if (foregroundEnabled != 0)
    {
        char* message = "Exiting foreground mode (enabling &).\n";

        // Use write instead of printf
        write(STDOUT_FILENO, message, 39);

        foregroundEnabled = 0;
    }
}

/*
*   Grabs the PID and parses through the supplied string to convert occurrences of $$
*   to the value of PID
*/
void findProcessID(char* argument)
{
    pid_t mainPID = getpid();
    char* tempStr = malloc(sizeof(char));

    // Parses through each element of the string
    for (int i = 0; i < strlen(argument); i++)
    {
        if (argument[i] == '$')
        {
            if (argument[i+1] == '$')
            {
                // Copies to a temporary value
                strcpy(tempStr, argument);
                tempStr[i] = '%', tempStr[i+1] = 'd';

                sprintf(argument, tempStr, mainPID);
            }
        }
    }
    free(tempStr);
}

/*
*   Enables/Disables background commands based on given input
*/
void toggleBackgroundForeground(char* argument, const char* nextToken)
{
    // Checks for & character
    if (strcmp(argument, "&") == 0)
    {
        // Check if foreground signal is enabled
        // Also make sure that "&" is the last element of the command
        if (foregroundEnabled == 0 && nextToken == NULL)
        {
            backgroundEnabled = 1;
        }
    }
}

/*
*   Grab valid user input that can be used for processing later on in the program
*/
char* getInput()
{
    int whileCounter = 1;
    int invalidInput = 0;

    // Loops until a value that isn't # or an empty space
    while (1)
    {
        if (invalidInput > 0)
        {
            printf(": ");
            fflush(stdout);
        }

        char* input = NULL;
        size_t max_input_size = MAX_CHAR_LENGTH;
        getline(&input, &max_input_size, stdin);

        if (input[0] != '#')
        {
            if (input[0] != ' ')
            {
                whileCounter++;
                return input;
            }
        }
        invalidInput++;
    }
}

/*
*   Processes shell-like commands inputted by the user to run
*   built-in commands or other commands as either a parent or child process
*/

int main(int argc, char *argv[])
{
    if (argc > 1)
    {
        printf("Please try again with no arguments.\n");
        printf("Example usage: ./smallsh\n");
        return EXIT_FAILURE;
    }

    while (1)
    {
        // Inspiration taken from Exploration: Signal Handling API
        // Initialize SIGINT_action struct to be empty
        struct sigaction SIGINT_action = {0};
        struct sigaction SIGTSTP_action = {0};

        // Initialize handlers for each signal handler
        SIGINT_action.sa_handler = SIG_IGN;
        SIGTSTP_action.sa_handler = handle_SIGTSTP;
        sigfillset(&SIGINT_action.sa_mask);
        sigfillset(&SIGTSTP_action.sa_mask);
        SIGINT_action.sa_flags = 0;

        // Flag set for SIGTSTP
        SIGTSTP_action.sa_flags = SA_RESTART;

        // Make the signal handler work
        sigaction(SIGINT, &SIGINT_action, NULL);
        sigaction(SIGTSTP, &SIGTSTP_action, NULL);

        backgroundEnabled = 0;

        char** argumentArr = malloc(sizeof(char) * MAX_ARGS);
        printf(": ");
        fflush(stdout);
        char* user_input = getInput();
        findProcessID(user_input);

        char* saveptr;
        char* token = strtok_r(user_input, " \n", &saveptr);
        int argCounter = 0;

        // While loop to iterate through each token
        while(token != NULL)
        {
            argumentArr[argCounter] = token;
            findProcessID(argumentArr[argCounter]);

            // Iterate to next token
            token = strtok_r(NULL, " \n", &saveptr);

            toggleBackgroundForeground(argumentArr[argCounter], token);

            if (!backgroundEnabled)
            {
                // Check for & character and if foreground-only mode is enabled to not input "&" at the end
                // to be part of the argument
                if (strcmp(argumentArr[argCounter], "&") == 0 && foregroundEnabled == 1)
                {
                    argumentArr[argCounter] = NULL;
                    token = strtok_r(NULL, " \n", &saveptr);
                }
                else
                {
                    argCounter++;
                }
            }
            else
            {
                argumentArr[argCounter] = NULL;
                token = strtok_r(NULL, " \n", &saveptr);
            }

        }

        if (strcmp(argumentArr[0], "exit") == 0)
        {
            free(user_input);
            free(argumentArr);
            return EXIT_SUCCESS;
        }

        // Taken inspiration from the tips given from the Assignment 3 page
        else if (strcmp(argumentArr[0], "cd") == 0)
        {
            if (argCounter == 2)
            {
                // Changes working directory
                int ret = chdir(argumentArr[1]);
                char cwd[256];
            }
            else
            {
                // Changes directory to the HOME environment
                chdir(getenv("HOME"));
                char cwd[256];
            }
        }

        else if (strcmp(argumentArr[0], "status") == 0)
        {
            // Inspiration from Exploration: Process API - Monitoring Child Processes
            // Get exit value from signal
            if (WIFSIGNALED(childExitStatus))
            {
                printf("exit value signal %d\n", WTERMSIG(childExitStatus));
                fflush(stdout);
            }

            // Exit value output if not from signal
            printf("exit value %d\n", statusNum);
            fflush(stdout);
        }
        else
        {
            // Inspiration taken from Exploration: Process API - Monitoring Child Processes
            pid_t spawnpid = -5;
            int childStatus;
            int childPid;

            // If fork is successful, the value of spawnpid will be 0 in the child, the child's pid in the parent
            spawnpid = fork();
            switch (spawnpid)
            {
                // If the fork doesn't work
                case -1:
                    perror("fork() failed!");
                    exit(1);
                    break;
                case 0:
                    // Commands issued for the child

                    // Child running as background process ignores SIGINT
                    if (backgroundEnabled == 0)
                    {
                        SIGINT_action.sa_handler = SIG_DFL;
                        sigaction(SIGINT, &SIGINT_action, NULL);
                    }

                    // Child running as background process ignores SIGTSTP
                    if (backgroundEnabled == 1)
                    {
                        SIGTSTP_action.sa_handler = SIG_DFL;
                        sigaction(SIGTSTP, &SIGTSTP_action, NULL);
                    }

                    spawnpid = waitpid(spawnpid, &childStatus, 0);

                    argumentArr[argCounter] = NULL;
                    int toggleRedirection = 0;

                    // Inspiration taken from Exploration: Processes and I/O
                    // Compares each element from the argument array
                    for (int i = 0; i < argCounter; i++)
                    {
                        // Check if it's output redirection
                        if (strcmp(argumentArr[i], ">") == 0)
                        {
                            int targetFD = open(argumentArr[i+1], O_WRONLY | O_CREAT | O_TRUNC, 0640);
                            if (targetFD == -1)
                            {
                                perror("open()");
                                exit(1);
                            }

                            // Use dup2 to point FD 1, i.e., standard output to targetFD
                            int result = dup2(targetFD, 1);
                            if (result == -1)
                            {
                                perror("dup2");
                                exit(2);
                            }

                            toggleRedirection = 1;
                        }

                        // Check if it's input redirection
                        if (strcmp(argumentArr[i], "<") == 0)
                        {
                            int targetFD = open(argumentArr[i+1], O_RDONLY);
                            if (targetFD == -1)
                            {
                                perror("open()");
                                exit(1);
                            }

                            // Use dup2 to point FD 1, i.e., standard output to targetFD
                            int result = dup2(targetFD, 0);
                            if (result == -1)
                            {
                                perror("dup2");
                                exit(2);
                            }

                            toggleRedirection = 1;
                        }
                    }

                    // NULL terminator for execvp if the command is a redirection
                    if (toggleRedirection == 1)
                    {
                        argumentArr[1] = NULL;
                    }

                    // Executes the command
                    execvp(argumentArr[0], argumentArr);
                    perror("execvp");
                    exit(EXIT_FAILURE);
                default:
                    // spawnpid is the pid of the child
                    // Check to see if the child process is going to be run in the background
                    if (backgroundEnabled == 1)
                    {
                        // Iterates through each element of backgroundPIDS for an empty element to store PIDS
                        for (int i = 0; (sizeof backgroundPIDS / sizeof *backgroundPIDS); i++)
                        {
                            if (backgroundPIDS[i] == 0)
                            {
                                backgroundPIDS[i] = spawnpid;
                                break;
                            }
                        }
                        printf("(start) background pid is %d\n", spawnpid);
                        //printf("Foreground: %d", foregroundEnabled);
                        //printf("Background: %d", backgroundEnabled);
                        fflush(stdout);
                    }
                    else
                    {
                        waitpid(spawnpid, &childExitStatus, 0);

                        // Store and check child status (if it's been terminated normally by a signal or not)
                        if (WIFEXITED(childExitStatus))
                        {
                            statusNum = WEXITSTATUS(childExitStatus);
                        }
                        // Store and check child status (if it's been terminated abnormally by a signal or not)
                        if (WIFSIGNALED(childExitStatus))
                        {
                            signalsNum = WTERMSIG(childExitStatus);
                            printf("exit value signal %d\n", WTERMSIG(childExitStatus));
                            fflush(stdout);
                        }
                    }

                    // Used tips and Exploration: Process API - Monitoring Child Processes
                    // Iterate through array of the backgroundPIDS
                    for (int i = 0; i < (sizeof backgroundPIDS / sizeof *backgroundPIDS); i++)
                    {
                        if(backgroundPIDS[i] != 0)
                        {
                            // Check child processes and if it's been ended or not with status
                            int childProcess = waitpid(-1, &childExitStatus, WNOHANG);
                            if (childProcess >= 1)
                            {
                                // Check background child status if it terminated normally
                                if (WIFEXITED(childExitStatus) == 1)
                                {
                                    printf("(end) background pid is %d with status %d\n", childProcess, WEXITSTATUS(childExitStatus));
                                    fflush(stdout);
                                }
                                else
                                {
                                    // Background process must have ended via signal if not
                                    printf("(end): background pid %d via signal: %d\n", childProcess, WTERMSIG(childExitStatus));
                                    fflush(stdout);
                                }
                                // Reset element in backgroundPIDS to be empty
                                backgroundPIDS[i] = 0;
                            }
                        }
                    }
                    break;
            }
        }
        // Valgrind is complaining about this so put here to free
        free(user_input);
    }
}