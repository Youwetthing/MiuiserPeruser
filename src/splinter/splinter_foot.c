// splinter_foot.c — Splinter Foot Job Dispatcher
// Handles FOOT JOB commands from Krang and executes Footrunner.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>

#define FOOTRUNNER_PATH "/data/data/com.termux/files/home/MiuiserPeruser/build/src/footclan/footrunner"
#define BUF_SIZE 2048

// ------------------------------------------------------------
// Execute Footrunner and capture its output
// ------------------------------------------------------------
static int run_foot_job(const char *job_id, const char *job_type, char *outbuf, size_t outsz) {
    int pipefd[2];
    if (pipe(pipefd) < 0) {
        snprintf(outbuf, outsz, "FOOT RESULT %s OK=0 ERROR=pipe_failed", job_id);
        return -1;
    }

    pid_t pid = fork();
    if (pid < 0) {
        snprintf(outbuf, outsz, "FOOT RESULT %s OK=0 ERROR=fork_failed", job_id);
        return -1;
    }

    if (pid == 0) {
        // Child: exec Footrunner
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        execl(FOOTRUNNER_PATH, "footrunner", job_id, job_type, NULL);
        _exit(1);
    }

    // Parent
    close(pipefd[1]);

    ssize_t n = read(pipefd[0], outbuf, outsz - 1);
    if (n <= 0) {
        snprintf(outbuf, outsz, "FOOT RESULT %s OK=0 ERROR=no_output", job_id);
        close(pipefd[0]);
        return -1;
    }

    outbuf[n] = '\0';
    close(pipefd[0]);

    waitpid(pid, NULL, 0);
    return 0;
}

// ------------------------------------------------------------
// Handle FOOT JOB command from Krang
// ------------------------------------------------------------
int splinter_handle_foot_job(const char *line, char *response, size_t rsz) {
    // Expected format:
    // FOOT JOB <job_id> <job_type>

    char cmd[16], job_kw[16], job_id[64], job_type[128];

    int n = sscanf(line, "%15s %15s %63s %127s", cmd, job_kw, job_id, job_type);
    if (n < 4) {
        snprintf(response, rsz, "FOOT RESULT 0 OK=0 ERROR=bad_format");
        return -1;
    }

    char result[BUF_SIZE];
    run_foot_job(job_id, job_type, result, sizeof(result));

    // Forward Footrunner result back to Krang
    snprintf(response, rsz, "%s", result);
    return 0;
}
