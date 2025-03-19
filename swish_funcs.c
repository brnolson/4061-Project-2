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

    if (execvp(args[0], args) == -1) {
        perror("exec");
        return -1;
    }

    // TODO Task 4: You need to do two items of setup before exec()'ing
    // 1. Restore the signal handlers for SIGTTOU and SIGTTIN to their defaults.
    // The code in main() within swish.c sets these handlers to the SIG_IGN value.
    // Adapt this code to use sigaction() to set the handlers to the SIG_DFL value.
    // 2. Change the process group of this process (a child of the main shell).
    // Call getpid() to get its process ID then call setpgid() and use this process
    // ID as the value for the new process group ID

    return 0;
}

int resume_job(strvec_t *tokens, job_list_t *jobs, int is_foreground) {
    // TODO Task 5: Implement the ability to resume stopped jobs in the foreground
    // 1. Look up the relevant job information (in a job_t) from the jobs list
    //    using the index supplied by the user (in tokens index 1)
    //    Feel free to use sscanf() or atoi() to convert this string to an int
    // 2. Call tcsetpgrp(STDIN_FILENO, <job_pid>) where job_pid is the job's process ID
    // 3. Send the process the SIGCONT signal with the kill() system call
    // 4. Use the same waitpid() logic as in main -- don't forget WUNTRACED
    // 5. If the job has terminated (not stopped), remove it from the 'jobs' list
    // 6. Call tcsetpgrp(STDIN_FILENO, <shell_pid>). shell_pid is the *current*
    //    process's pid, since we call this function from the main shell process

    // TODO Task 6: Implement the ability to resume stopped jobs in the background.
    // This really just means omitting some of the steps used to resume a job in the foreground:
    // 1. DO NOT call tcsetpgrp() to manipulate foreground/background terminal process group
    // 2. DO NOT call waitpid() to wait on the job
    // 3. Make sure to modify the 'status' field of the relevant job list entry to BACKGROUND
    //    (as it was STOPPED before this)

    return 0;
}

int await_background_job(strvec_t *tokens, job_list_t *jobs) {
    // TODO Task 6: Wait for a specific job to stop or terminate
    // 1. Look up the relevant job information (in a job_t) from the jobs list
    //    using the index supplied by the user (in tokens index 1)
    // 2. Make sure the job's status is BACKGROUND (no sense waiting for a stopped job)
    // 3. Use waitpid() to wait for the job to terminate, as you have in resume_job() and main().
    // 4. If the process terminates (is not stopped by a signal) remove it from the jobs list

    return 0;
}

int await_all_background_jobs(job_list_t *jobs) {
    // TODO Task 6: Wait for all background jobs to stop or terminate
    // 1. Iterate through the jobs list, ignoring any stopped jobs
    // 2. For a background job, call waitpid() with WUNTRACED.
    // 3. If the job has stopped (check with WIFSTOPPED), change its
    //    status to STOPPED. If the job has terminated, do nothing until the
    //    next step (don't attempt to remove it while iterating through the list).
    // 4. Remove all background jobs (which have all just terminated) from jobs list.
    //    Use the job_list_remove_by_status() function.

    return 0;
}
