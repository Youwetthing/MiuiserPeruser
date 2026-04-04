// foot_jobs.c — Footrunner job registry
// Maps job types to actual job functions.

#include <stdio.h>
#include <string.h>

// Job function prototypes
void foot_job_thermal(const char *job_id);
void foot_job_process(const char *job_id);
void foot_job_battery(const char *job_id);
void foot_job_power(const char *job_id);
void foot_job_cpu(const char *job_id);
void foot_job_wakelock(const char *job_id);

// ------------------------------------------------------------
// Job lookup table
// ------------------------------------------------------------
struct foot_job_entry {
    const char *name;
    void (*fn)(const char *job_id);
};

static struct foot_job_entry JOBS[] = {
    { "THERMAL",   foot_job_thermal   },
    { "PROCESS",   foot_job_process   },
    { "BATTERY",   foot_job_battery   },
    { "POWER",     foot_job_power     },
    { "CPU",       foot_job_cpu       },
    { "WAKELOCK",  foot_job_wakelock  },
    { NULL,        NULL }
};

// ------------------------------------------------------------
// Dispatcher: called by Footrunner main loop
// ------------------------------------------------------------
void foot_dispatch_job(const char *job_id, const char *job_type) {
    for (int i = 0; JOBS[i].name; i++) {
        if (strcmp(JOBS[i].name, job_type) == 0) {
            JOBS[i].fn(job_id);
            return;
        }
    }

    // Unknown job
    printf("FOOT RESULT %s ERR=UNKNOWN_JOB TYPE=%s\n", job_id, job_type);
}
