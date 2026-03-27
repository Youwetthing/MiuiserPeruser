#ifndef APRIL_TABLE_H
#define APRIL_TABLE_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    APRIL_TABLE_NORMAL = 0,
    APRIL_TABLE_COLOR,
    APRIL_TABLE_JSON,
    APRIL_TABLE_QUIET
} april_table_mode_t;

typedef struct {
    const char *key;
    const char *value;
} april_table_entry_t;

typedef struct {
    april_table_entry_t *entries;
    uint32_t count;
    uint32_t capacity;
    april_table_mode_t mode;
} april_table_t;

april_table_t *april_table_create(april_table_mode_t mode);
void april_table_add(april_table_t *t, const char *key, const char *value);
void april_table_print(const april_table_t *t);
void april_table_free(april_table_t *t);

#endif /* APRIL_TABLE_H */
