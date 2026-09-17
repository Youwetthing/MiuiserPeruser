#ifndef GAVELD_TEST_ENV_H
#define GAVELD_TEST_ENV_H

/*
 * gaveld_test_env.h — scratch BASE_DIR setup shared by the gaveld tests.
 *
 * The test build compiles gaveld sources with -DBASE_DIR pointing at
 * tests/build/root, so config.h derives DB_PATH / LOG_PATH / SOVEREIGNTY_LIST
 * from there. This header creates that tree and clears state between runs.
 */

#include "config.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void gt_write_file(const char *path, const char *content) {
    FILE *fp = fopen(path, "w");
    if (!fp) {
        fprintf(stderr, "test env: cannot write %s\n", path);
        return;
    }
    fputs(content, fp);
    fclose(fp);
}

/* Create BASE_DIR/{state,logs,pipes} and remove any leftover database. */
static void gt_env_init(void) {
    mkdir(BASE_DIR, 0755);
    mkdir(BASE_DIR "/state", 0755);
    mkdir(BASE_DIR "/logs", 0755);
    mkdir(BASE_DIR "/pipes", 0755);

    unlink(DB_PATH);
    unlink(DB_PATH "-wal");
    unlink(DB_PATH "-shm");
    unlink(SOVEREIGNTY_LIST);
    unlink(IA_LOCK);
}

#endif /* GAVELD_TEST_ENV_H */
