#include "april_table.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void ensure_capacity(april_table_t *t) {
    if (t->count < t->capacity)
        return;

    uint32_t newcap = (t->capacity == 0) ? 8 : t->capacity * 2;
    t->entries = realloc(t->entries, newcap * sizeof(april_table_entry_t));
    t->capacity = newcap;
}

april_table_t *april_table_create(april_table_mode_t mode) {
    april_table_t *t = calloc(1, sizeof(april_table_t));
    t->mode = mode;
    return t;
}

void april_table_add(april_table_t *t, const char *key, const char *value) {
    ensure_capacity(t);
    t->entries[t->count].key = key;
    t->entries[t->count].value = value;
    t->count++;
}

static uint32_t longest_key(const april_table_t *t) {
    uint32_t max = 0;
    for (uint32_t i = 0; i < t->count; i++) {
        uint32_t len = strlen(t->entries[i].key);
        if (len > max)
            max = len;
    }
    return max;
}

static void print_normal(const april_table_t *t) {
    uint32_t w = longest_key(t);

    printf("+-%-*s-+---------+\n", w, "----------------");
    printf("| %-*s | Value   |\n", w, "Metric");
    printf("+-%-*s-+---------+\n", w, "----------------");

    for (uint32_t i = 0; i < t->count; i++) {
        printf("| %-*s | %-7s |\n",
               w,
               t->entries[i].key,
               t->entries[i].value);
    }

    printf("+-%-*s-+---------+\n", w, "----------------");
}

static void print_color(const april_table_t *t) {
    uint32_t w = longest_key(t);

    printf("\033[36m+-%-*s-+---------+\033[0m\n", w, "----------------");
    printf("\033[36m| %-*s | Value   |\033[0m\n", w, "Metric");
    printf("\033[36m+-%-*s-+---------+\033[0m\n", w, "----------------");

    for (uint32_t i = 0; i < t->count; i++) {
        printf("\033[32m| %-*s | %-7s |\033[0m\n",
               w,
               t->entries[i].key,
               t->entries[i].value);
    }

    printf("\033[36m+-%-*s-+---------+\033[0m\n", w, "----------------");
}

static void print_json(const april_table_t *t) {
    printf("{\n");
    for (uint32_t i = 0; i < t->count; i++) {
        printf("  \"%s\": \"%s\"%s\n",
               t->entries[i].key,
               t->entries[i].value,
               (i + 1 < t->count) ? "," : "");
    }
    printf("}\n");
}

static void print_quiet(const april_table_t *t) {
    for (uint32_t i = 0; i < t->count; i++) {
        printf("%s%s", t->entries[i].value,
               (i + 1 < t->count) ? " " : "");
    }
    printf("\n");
}

void april_table_print(const april_table_t *t) {
    switch (t->mode) {
    case APRIL_TABLE_COLOR:  print_color(t); break;
    case APRIL_TABLE_JSON:   print_json(t); break;
    case APRIL_TABLE_QUIET:  print_quiet(t); break;
    case APRIL_TABLE_NORMAL:
    default:
        print_normal(t);
        break;
    }
}

void april_table_free(april_table_t *t) {
    if (!t) return;
    free(t->entries);
    free(t);
}
