#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void run_cmd_to_buf(const char *cmd, char *buf, size_t len) {
    FILE *f = popen(cmd, "r");
    if (f) {
        if (fgets(buf, len, f)) {
            buf[strcspn(buf, "\n")] = '\0';
        }
        pclose(f);
    }
}

void foot_job_cpu(const char *job_id) {
    char load[64] = {0};
    char total[16] = {0};
    
    // Grab the Load line
    run_cmd_to_buf("dumpsys cpuinfo | grep 'Load:' | awk '{print $2 \" \" $4 \" \" $6}'", load, sizeof(load));
    // Grab the Total % line
    run_cmd_to_buf("dumpsys cpuinfo | grep 'TOTAL:' | awk '{print $1}'", total, sizeof(total));

    if (strlen(total) > 0) {
        printf("FOOT RESULT %s OK=1 LOAD=\"%s\" TOTAL_USAGE=\"%s\"\n", job_id, load, total);
    } else {
        printf("FOOT RESULT %s OK=0 ERR=cpu_parse_fail\n", job_id);
    }
}

void foot_job_battery(const char *job_id) {
    char level[16] = {0};
    run_cmd_to_buf("dumpsys battery | grep level | awk '{print $2}'", level, sizeof(level));
    printf("FOOT RESULT %s OK=1 BATT=%s\n", job_id, (strlen(level) > 0) ? level : "0");
}

void foot_job_thermal(const char *job_id) {
    char temp[16] = {0};
    run_cmd_to_buf("dumpsys thermalservice | grep -m1 'Current temperature' | awk '{print $3}'", temp, sizeof(temp));
    printf("FOOT RESULT %s OK=1 TEMP=%s\n", job_id, (strlen(temp) > 0) ? temp : "0");
}

void foot_job_network(const char *job_id) {
    char state[64] = {0};
    run_cmd_to_buf("cmd connectivity show | grep 'NetworkAgentInfo' | grep 'CONNECTED' | head -n 1 | sed 's/.*{[^ ]* [^ ]* \\([^ ]*\\) .*/\\1/'", state, sizeof(state));
    if (strlen(state) > 0) {
        printf("FOOT RESULT %s OK=1 NET_STATE=\"CONNECTED\" TYPE=\"%s\"\n", job_id, state);
    } else {
        printf("FOOT RESULT %s OK=1 NET_STATE=\"DISCONNECTED\"\n", job_id);
    }
}

void foot_job_process(const char *job_id) {
    char count[16] = {0};
    run_cmd_to_buf("dumpsys processinfo | grep 'Total' | awk '{print $3}'", count, sizeof(count));
    printf("FOOT RESULT %s OK=1 PROCS=%s\n", job_id, (strlen(count) > 0) ? count : "0");
}

// Stubs
void foot_job_power(const char *job_id)   { printf("FOOT RESULT %s OK=1 JOB=POWER_STUB\n", job_id); }
void foot_job_wakelock(const char *job_id){ printf("FOOT RESULT %s OK=1 JOB=WAKELOCK_STUB\n", job_id); }

struct foot_job_entry {
    const char *name;
    void (*fn)(const char *job_id);
};

static struct foot_job_entry JOBS[] = {
    { "THERMAL",   foot_job_thermal   },
    { "PROCESS",   foot_job_process   },
    { "BATTERY",   foot_job_battery   },
    { "NETWORK",   foot_job_network   },
    { "CPU",       foot_job_cpu       },
    { "POWER",     foot_job_power     },
    { "WAKELOCK",  foot_job_wakelock  },
    { NULL,        NULL }
};

void foot_dispatch_job(const char *job_id, const char *job_type) {
    for (int i = 0; JOBS[i].name; i++) {
        if (strcmp(JOBS[i].name, job_type) == 0) {
            JOBS[i].fn(job_id);
            return;
        }
    }
    printf("FOOT RESULT %s OK=0 ERR=UNKNOWN_JOB TYPE=%s\n", job_id, job_type);
}
