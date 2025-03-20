#define _GNU_SOURCE

#include "swish_funcs.h"

#include <assert.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <errno.h>

#include "job_list.h"
#include "string_vector.h"

#define MAX_ARGS 10

int tokenize(char *s, strvec_t *tokens) {
    // Clear tokens if there is any
    strvec_clear(tokens);
    char *token = strtok(s, " ");

    if (token == NULL) {
        return 0;
    }

    // Loop through all tokens and add it to the strvec struct
    while (token != NULL) {
        if (strvec_add(tokens, token) != 0) {
            fprintf(stderr, "Error: Failed to add token\n");
            strvec_clear(tokens);
            return -1;
        }
        token = strtok(NULL, " ");
    }
    return 0;
}

int run_command(strvec_t *tokens) {
    char *args[MAX_ARGS];
    int i, arg_index = 0;
    int file_in = -1;
    int file_out = -1;
    char *input_file = NULL;
    char *output_file = NULL;
    int redirect_input = 0;
    int redirect_output = 0;
    int append_output = 0;

    for (i = 0; i < tokens->length && arg_index < MAX_ARGS - 1; i++) {
        char *token = strvec_get(tokens, i);

        if (strcmp(token, "<") == 0 && i + 1 < tokens->length) {
            redirect_input = 1;
            input_file = strvec_get(tokens, i + 1);
            i++;
        }
        else if (strcmp(token, ">") == 0 && i + 1 < tokens->length) {
            redirect_output = 1;
            output_file = strvec_get(tokens, i + 1);
            i++;
        }
        else if (strcmp(token, ">>") == 0 && i + 1 < tokens->length) {
            append_output = 1;
            output_file = strvec_get(tokens, i + 1);
            i++;
        }
        else {
            args[arg_index] = token;
            arg_index++;
        }
    }
    args[arg_index] = NULL;

    if (redirect_input == 1) {
        file_in = open(input_file, O_RDONLY);
        if (file_in == -1) {
            perror("Failed to open input file");
            return -1;
        }
        dup2(file_in, STDIN_FILENO);
        close(file_in);
    }

    if (redirect_output == 1) {
        file_out = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
        if (file_out == -1) {
            perror("Failed to open output file");
            return -1;
        }
        dup2(file_out, STDOUT_FILENO);
        close(file_out);
    }

    if (append_output == 1) {
        file_out = open(output_file, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);
        if (file_out == -1) {
            perror("Failed to open output file");
            return -1;
        }
        dup2(file_out, STDOUT_FILENO);
        close(file_out);
    }

    struct sigaction sa;
    sa.sa_handler = SIG_DFL;
    if (sigfillset(&sa.sa_mask) == -1) {
        perror("sigfillset");
        return -1;
    }

    sa.sa_flags = 0;
    if (sigaction(SIGTTIN, &sa, NULL) == -1 || sigaction(SIGTTOU, &sa, NULL) == -1) {
        perror("sigaction");
        return -1;
    }

    pid_t pid = getpid();
    if (setpgid(pid, pid) == -1) {
        perror("Failed to set process group");
        return -1;
    }

    if (execvp(args[0], args) == -1) {
        perror("exec");
        return -1;
    }

    return 0;
}

int resume_job(strvec_t *tokens, job_list_t *jobs, int is_foreground) {
    if (tokens->length < 2) {
        fprintf(stderr, "Error: Insufficient arguments\n");
        return -1;
    }

    int job_idx = atoi(strvec_get(tokens, 1));
    job_t *job = job_list_get(jobs, job_idx);

    if (!job) {
        fprintf(stderr, "Job index out of bounds\n");
        return -1;
    }

    if (is_foreground) {
        if (tcsetpgrp(STDIN_FILENO, job->pid) == -1) {
            perror("tcsetpgrp");
            return -1;
        }
    }

    if (kill(job->pid, SIGCONT) == -1) {
        perror("kill");
        return -1;
    }

    if (is_foreground) {
        int status;
        pid_t wait_pid;

        do {
            wait_pid = waitpid(job->pid, &status, WUNTRACED);
        } while (wait_pid == -1 && errno == EINTR);

        if (wait_pid == -1) {
            perror("waitpid");
            return -1;
        }

        if (tcsetpgrp(STDIN_FILENO, getpid()) == -1) {
            perror("tcsetpgrp");
            return -1;
        }

        if (WIFSTOPPED(status)) {
            job->status = STOPPED;
        } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
            job_list_remove(jobs, job_idx);
        }
    }
    else {
        job->status = BACKGROUND;
    }

    return 0;
}

int await_background_job(strvec_t *tokens, job_list_t *jobs) {
    if (tokens->length < 2) {
        fprintf(stderr, "Usage: wait-for <job_number>\n");
        return -1;
    }

    int job_idx = atoi(strvec_get(tokens, 1));
    job_t *job = job_list_get(jobs, job_idx);

    if (!job) {
        fprintf(stderr, "Job %d not found\n", job_idx);
        return -1;
    }

    int status;
    pid_t wait_pid;

    do {
        wait_pid = waitpid(job->pid, &status, WUNTRACED);
        if (wait_pid == -1 && errno == EINTR) {
            fprintf(stderr, "waitpid: Interrupted system call (retrying)\n");
        }
    } while (wait_pid == -1 && errno == EINTR);

    if (wait_pid == -1) {
        perror("waitpid");
        return -1;
    }

    if (WIFSTOPPED(status)) {
        job->status = STOPPED;
    } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
        job_list_remove(jobs, job_idx);
    }

    return 0;
}

int await_all_background_jobs(job_list_t *jobs) {
    int status;
    pid_t wait_pid;

    for (unsigned i = 0; i < jobs->length; i++) {
        job_t *job = job_list_get(jobs, i);

        if (job->status == STOPPED) {
            continue;
        }

        do {
            wait_pid = waitpid(job->pid, &status, WUNTRACED);
        } while (wait_pid == -1 && errno == EINTR);

        if (wait_pid == -1) {
            perror("waitpid");
            return -1;
        }

        if (WIFSTOPPED(status)) {
            job->status = STOPPED;
        } else if (WIFEXITED(status) || WIFSIGNALED(status)) {
            job_list_remove(jobs, i);
            i--;
        }
    }

    return 0;
}
