#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

// Link to your registry
extern void foot_dispatch_job(const char *job_id, const char *job_type);

int main(int argc, char **argv) {
    if (argc < 3) {
        printf("FOOT RESULT 0 OK=0 ERROR=bad_args\n");
        return 1;
    }

    const char *job_id = argv[1];
    const char *job_type = argv[2];

    // Hand the mission to the registry
    foot_dispatch_job(job_id, job_type);

    return 0;
}
