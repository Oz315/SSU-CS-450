#include <stdio.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include "constants.h"
#include "parsetools.h"


int main() {

    // Buffer for reading one line of input
    char line[MAX_LINE_CHARS];
    // holds separated words based on whitespace
    char* line_words[MAX_LINE_WORDS + 1];
    // True when stdin is connected to a terminal
    int interactive = isatty(STDIN_FILENO);

    // Loop until user hits Ctrl-D (end of input)
    // or some other input error occurs
    while (1) {
        if (interactive) {
            printf("lobo> ");
            fflush(stdout);
        }
        if (fgets(line, MAX_LINE_CHARS, stdin) == NULL) {
            break;
        }

        int num_words = split_cmd_line(line, line_words);

        //looking for pipe indicators
        int first_pipe_index = -1;
        int second_pipe_index = -1;

        for (int i = 0; i < num_words; i++) {
            if (strcmp(line_words[i], "|") == 0) {
                if (first_pipe_index == -1) {
                    first_pipe_index = i;
                }
                else {
                    second_pipe_index = i;
                    break;
                }
            }
        }

        char *input_file = NULL;
        char *output_file = NULL;
        //This is so later we can decide if we do O_APPEND or O_TRUNC
        //Could also include boolean at the top of the file but I'm just using 1 or 0
        int append = 0;

        //Looking for any redirection, <, >, and >>
        for (int i = 0; i < num_words; i++) {
            if (strcmp(line_words[i], "<") == 0) {
                input_file = line_words[i + 1];
                line_words[i] = NULL;
            }
            else if (strcmp(line_words[i], ">") == 0) {
                append = 0;
                output_file = line_words[i + 1];
                line_words[i] = NULL;
            }
            else if (strcmp(line_words[i], ">>") == 0) {
                append = 1;
                output_file = line_words[i + 1];
                line_words[i] = NULL;
            }
        }
        
        //Where we split the pipe
        char **left_args = line_words;
        char **middle_args = NULL;
        char **right_args = NULL;

        // create two pipes
        if (second_pipe_index != -1) {

            line_words[first_pipe_index] = NULL;
            line_words[second_pipe_index] = NULL;

            middle_args = &line_words[first_pipe_index + 1];
            right_args = &line_words[second_pipe_index + 1];

            // create two pipes

            int pipefd1[2];
            int pipefd2[2];

            if (pipe(pipefd1) < 0) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }

            if (pipe(pipefd2) < 0) {
                perror("pipe");

                if (close(pipefd1[0]) < 0) {
                    perror("close");
                }

                if (close(pipefd1[1]) < 0) {
                    perror("close");
                }

                exit(EXIT_FAILURE);
            }

            // create left child
            pid_t left_pid = fork();
            if (left_pid < 0) {
                perror("fork");
                exit(EXIT_FAILURE);
            }
            else if (left_pid == 0) {

                // Make stdout go into first pipe
                if (dup2(pipefd1[1], STDOUT_FILENO) < 0) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }
                // We just need input redirection for the left_args
                if (input_file != NULL) {
                    int fd_in = open(input_file, O_RDONLY);
                    if (fd_in < 0){
                        perror("something wrong with input");
                        exit(EXIT_FAILURE);
                    }
                    dup2(fd_in, STDIN_FILENO);
                    close(fd_in);
                }
                // Child no longer needs original pipe descriptors
                if (close(pipefd1[0]) < 0) {
                    perror("close");
                    exit(EXIT_FAILURE);
                }
                if (close(pipefd1[1]) < 0) {
                    perror("close");
                    exit(EXIT_FAILURE);
                }
                if (close(pipefd2[0]) < 0) {
                    perror("close");
                    exit(EXIT_FAILURE);
                }
                if (close(pipefd2[1]) < 0) {
                    perror("close");
                    exit(EXIT_FAILURE);
                }

                // Run the left command
                execvp(left_args[0], left_args);

                perror("execvp");
                exit(EXIT_FAILURE);
            }

            // create middle child
            pid_t middle_pid = fork();

            if (middle_pid < 0) {
                perror("fork");
                exit(EXIT_FAILURE);
            }
            else if (middle_pid == 0) {

                // Make stdin come from first pipe
                if (dup2(pipefd1[0], STDIN_FILENO) < 0) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }

                // Make stdout go into second pipe
                if (dup2(pipefd2[1], STDOUT_FILENO) < 0) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }

                // Child no longer needs original pipe descriptors
                if (close(pipefd1[0]) < 0) {
                    perror("close");
                    exit(EXIT_FAILURE);
                }

                if (close(pipefd1[1]) < 0) {
                    perror("close");
                    exit(EXIT_FAILURE);
                }

                if (close(pipefd2[0]) < 0) {
                    perror("close");
                    exit(EXIT_FAILURE);
                }

                if (close(pipefd2[1]) < 0) {
                    perror("close");
                    exit(EXIT_FAILURE);
                }

                // Run the middle command
                execvp(middle_args[0], middle_args);

                perror("execvp");
                exit(EXIT_FAILURE);
            }


            // create right child
            pid_t right_pid = fork();

            if (right_pid < 0) {
                perror("fork");
                exit(EXIT_FAILURE);
            }
            else if (right_pid == 0) {
                
                // Make stdin come from second pipe
                if (dup2(pipefd2[0], STDIN_FILENO) < 0) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }

                // Now we apply output here
                // This applies to all outputs but you need to make clean before running
                // test 5 and 6a otherwise something buggy happens with the umask and data
                // not being overwritten correctly and subsequent runs will always fail those test
                // only the first time after running make clean will test 5 and 6a pass
                if (output_file != NULL) {
                    int output_fd;
                    if (append) {
                        output_fd = open(output_file, O_WRONLY | O_CREAT | O_APPEND, 0666);
                    } else {
                        output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                    }
                    if (output_fd < 0) {
                        perror("open output problem");
                        exit (EXIT_FAILURE);
                    }
                    dup2(output_fd, STDOUT_FILENO);
                    close(output_fd);
                }

                // Child no longer needs original pipe descriptors
                if (close(pipefd1[0]) < 0) {
                    perror("close");
                    exit(EXIT_FAILURE);
                }

                if (close(pipefd1[1]) < 0) {
                    perror("close");
                    exit(EXIT_FAILURE);
                }

                if (close(pipefd2[0]) < 0) {
                    perror("close");
                    exit(EXIT_FAILURE);
                }

                if (close(pipefd2[1]) < 0) {
                    perror("close");
                    exit(EXIT_FAILURE);
                }

                // Run the right command
                execvp(right_args[0], right_args);

                perror("execvp");
                exit(EXIT_FAILURE);
            }

            // parent does not need either pipe anymore
            if (close(pipefd1[0]) < 0) {
                perror("close");
            }

            if (close(pipefd1[1]) < 0) {
                perror("close");
            }

            if (close(pipefd2[0]) < 0) {
                perror("close");
            }

            if (close(pipefd2[1]) < 0) {
                perror("close");
            }


            // parent waits for all three children
            waitpid(left_pid, NULL, 0);
            waitpid(middle_pid, NULL, 0);
            waitpid(right_pid, NULL, 0);
        }
        else if (first_pipe_index != -1) { // one pipe

            line_words[first_pipe_index] = NULL;
            right_args = &line_words[first_pipe_index + 1];

            //pipe loop
            int pipefd[2];

            if (pipe(pipefd) < 0) {
                perror("pipe");
                exit(EXIT_FAILURE);
            }

            // create left child
            pid_t left_pid = fork();

            if (left_pid < 0) {
                perror("fork");
                exit(EXIT_FAILURE);
            }
            else if (left_pid == 0) {

                // Make stdout go into the pipe
                if (dup2(pipefd[1], STDOUT_FILENO) < 0) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }

                if (input_file != NULL) {
                    int fd_in = open(input_file, O_RDONLY);
                    if (fd_in < 0){
                        perror("something wrong with input");
                        exit(EXIT_FAILURE);
                    }
                    dup2(fd_in, STDIN_FILENO);
                    close(fd_in);
                }

                // Child no longer needs these original pipe descriptors
                if (close(pipefd[0]) < 0) {
                    perror("close");
                    exit(EXIT_FAILURE);
                }

                if (close(pipefd[1]) < 0) {
                    perror("close");
                    exit(EXIT_FAILURE);
                }

                // Run the left command
                execvp(left_args[0], left_args);

                perror("execvp");
                exit(EXIT_FAILURE);
            }

            // create right child
            pid_t right_pid = fork();

            if (right_pid < 0) {
                perror("fork");
                exit(EXIT_FAILURE);
            }
            else if (right_pid == 0) {

                // Make stdin come from the pipe
                if (dup2(pipefd[0], STDIN_FILENO) < 0) {
                    perror("dup2");
                    exit(EXIT_FAILURE);
                }

                //output redirection
                if (output_file != NULL) {
                    int output_fd;
                    if (append) {
                        output_fd = open(output_file, O_WRONLY | O_CREAT | O_APPEND, 0666);
                    } else {
                        output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                    }
                    if (output_fd < 0) {
                        perror("open output problem");
                        exit (EXIT_FAILURE);
                    }
                    dup2(output_fd, STDOUT_FILENO);
                    close(output_fd);
                }

                // Child no longer needs these original pipe descriptors
                if (close(pipefd[0]) < 0) {
                    perror("close");
                    exit(EXIT_FAILURE);
                }

                if (close(pipefd[1]) < 0) {
                    perror("close");
                    exit(EXIT_FAILURE);
                }

                // Run the right command
                execvp(right_args[0], right_args);

                perror("execvp");
                exit(EXIT_FAILURE);
            }

            // parent does not need the pipe anymore
            if (close(pipefd[0]) < 0) {
                perror("close");
            }

            if (close(pipefd[1]) < 0) {
                perror("close");
            }

            // parent waits for both children
            waitpid(left_pid, NULL, 0);
            waitpid(right_pid, NULL, 0);
        }
        else if (num_words > 0) { // no pipes
            
            pid_t pid = fork();

            // fork error check
            if (pid < 0) {
                perror("fork");
            }
            else if (pid == 0) {

                // checks if command has input redirection
                if (input_file != NULL) {

                    // opens input file for reading
                    int input_fd = open(input_file, O_RDONLY);

                    if (input_fd < 0) {
                        perror("open");
                        exit(EXIT_FAILURE);
                    }

                    // Make stdin come from the file
                    if (dup2(input_fd, STDIN_FILENO) < 0) {
                        perror("dup2");
                        exit(EXIT_FAILURE);
                    }

                    close (input_fd);
                }

                if (output_file != NULL) {
                    int output_fd;
                    if (append) {
                        output_fd = open(output_file, O_WRONLY | O_CREAT | O_APPEND, 0666);
                    } else {
                        output_fd = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
                    }
                    if (output_fd < 0) {
                        perror("open output problem");
                        exit (EXIT_FAILURE);
                    }
                    dup2(output_fd, STDOUT_FILENO);
                    close(output_fd);
                }

                // child runs the command
                execvp(line_words[0], line_words);

                // only happens if execvp fails
                perror("execvp");
                exit(EXIT_FAILURE);
            }
            else {
                // parent waits for child to finish
                waitpid(pid, NULL, 0);
            }
        }
    }
    
    
    return 0;
}


