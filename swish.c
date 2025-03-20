#define _GNU_SOURCE

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "job_list.h"
#include "string_vector.h"
#include "swish_funcs.h"

#define CMD_LEN 512
#define PROMPT "@> "

int main(int argc, char **argv) {
    // Task 4: Set up shell to ignore SIGTTIN, SIGTTOU when put in background
    // You should adapt this code for use in run_command().
    struct sigaction sac;
    sac.sa_handler = SIG_IGN;
    if (sigfillset(&sac.sa_mask) == -1) {
        perror("sigfillset");
        return 1;
    }
    sac.sa_flags = 0;
    if (sigaction(SIGTTIN, &sac, NULL) == -1 || sigaction(SIGTTOU, &sac, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    strvec_t tokens;
    strvec_init(&tokens);
    job_list_t jobs;
    job_list_init(&jobs);
    char cmd[CMD_LEN];

    printf("%s", PROMPT);
    while (fgets(cmd, CMD_LEN, stdin) != NULL) {
        // Need to remove trailing '\n' from cmd. There are fancier ways.
        int i = 0;
        while (cmd[i] != '\n') {
            i++;
        }
        cmd[i] = '\0';

        if (tokenize(cmd, &tokens) != 0) {
            printf("Failed to parse command\n");
            strvec_clear(&tokens);
            job_list_free(&jobs);
            return 1;
        }
        if (tokens.length == 0) {
            printf("%s", PROMPT);
            continue;
        }
        const char *first_token = strvec_get(&tokens, 0);

        if (strcmp(first_token, "pwd") == 0) {
            // Printing out working directory
            char cwd[CMD_LEN];
            if (getcwd(cwd, sizeof(cwd)) != NULL) {
               printf("%s\n", cwd);
            }

            else {
                perror("getcwd");
            }
        }

        else if (strcmp(first_token, "cd") == 0) {
            // Changing directory based on input
            const char *dir;

            if (tokens.length > 1) {
                dir = strvec_get(&tokens, 1);
            }
            else {
                dir = getenv("HOME");

                if (dir == NULL) {
                    fprintf(stderr, "cd: HOME not set\n");
                }
            }

            if (chdir(dir) == -1) {
                perror("chdir");
            }
        }

        else if (strcmp(first_token, "exit") == 0) {
            strvec_clear(&tokens);
            break;
        }

        // Task 5: Print out current list of pending jobs
        else if (strcmp(first_token, "jobs") == 0) {
            int i = 0;
            job_t *current = jobs.head;
            while (current != NULL) {
                char *status_desc;
                if (current->status == BACKGROUND) {
                    status_desc = "background";
                } else {
                    status_desc = "stopped";
                }
                printf("%d: %s (%s)\n", i, current->name, status_desc);
                i++;
                current = current->next;
            }
        }

        // Task 5: Move stopped job into foreground
        else if (strcmp(first_token, "fg") == 0) {
            if (resume_job(&tokens, &jobs, 1) == -1) {
                printf("Failed to resume job in foreground\n");
            }
        }

        // Task 6: Move stopped job into background
        else if (strcmp(first_token, "bg") == 0) {
            if (resume_job(&tokens, &jobs, 0) == -1) {
                printf("Failed to resume job in background\n");
            }
        }

        // Task 6: Wait for a specific job identified by its index in job list
        else if (strcmp(first_token, "wait-for") == 0) {
            if (await_background_job(&tokens, &jobs) == -1) {
                printf("Failed to wait for background job\n");
            }
        }

        // Task 6: Wait for all background jobs
        else if (strcmp(first_token, "wait-all") == 0) {
            if (await_all_background_jobs(&jobs) == -1) {
                printf("Failed to wait for all background jobs\n");
            }
        }

        else {
            // Check if the current process is a background process by checking the & token is there
            int is_background = 0;
            if (tokens.length > 0 && strcmp(strvec_get(&tokens, tokens.length - 1), "&") == 0) {
                is_background = 1;
                strvec_take(&tokens, tokens.length -1 );
            }

            pid_t pid = fork();

            if (pid < 0) {
                perror("fork");
            }

            else if (pid == 0) {
                if (run_command(&tokens) == -1) {
                    return 1;
                }
            }

            else {
                if (is_background) {
                    if (job_list_add(&jobs, pid, strvec_get(&tokens, 0), BACKGROUND) == -1) {
                        fprintf(stderr, "Failed to add background job to list\n");
                    }
                }
                else {
                    int status;

                    if (tcsetpgrp(STDIN_FILENO, pid) == -1) {
                        perror("tcsetpgrp");
                        return 1;
                    }

                    if (waitpid(pid, &status, WUNTRACED) == -1) {
                        perror("waitpid");
                        return 1;
                    }

                    if (tcsetpgrp(STDIN_FILENO, getpid()) == -1) {
                        perror("tcsetpgrp");
                        return 1;
                    }

                    if (WIFSTOPPED(status)) {
                        job_t *new_job = malloc(sizeof(job_t));
                        if (!new_job) {
                            perror("malloc");
                            return 1;
                        }

                        snprintf(new_job->name, NAME_LEN, "%.*s", NAME_LEN - 1, strvec_get(&tokens, 0));
                        new_job->pid = pid;
                        new_job->status = STOPPED;

                        if (job_list_add(&jobs, pid, new_job->name, STOPPED) == -1) {
                            fprintf(stderr, "Failed to add job to list\n");
                            free(new_job);
                            return 1;
                        }
                        free(new_job);
                    }
                }
            }

            // TODO Task 6: If the last token input by the user is "&", start the current
            // command in the background.
            // 1. Determine if the last token is "&". If present, use strvec_take() to remove
            //    the "&" from the token list.
            // 2. Modify the code for the parent (shell) process: Don't use tcsetpgrp() or
            //    use waitpid() to interact with the newly spawned child process.
            // 3. Add a new entry to the jobs list with the child's pid, program name,
            //    and status BACKGROUND.
        }

        strvec_clear(&tokens);
        printf("%s", PROMPT);
    }

    job_list_free(&jobs);
    return 0;
}
