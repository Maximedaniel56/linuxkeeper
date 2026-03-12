/*
 * Minimal TOML parser for linuxkeeper configuration.
 * Single-header implementation - define TOML_IMPLEMENTATION in one .c file.
 *
 * Supports: strings, integers, booleans, arrays of strings, [sections]
 * Does NOT support: nested tables, inline tables, dates, multiline strings
 */
#ifndef LK_TOML_H
#define LK_TOML_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define TOML_MAX_KEY    128
#define TOML_MAX_VAL    1024
#define TOML_MAX_SECT   64
#define TOML_MAX_ARR    32

typedef enum {
    TOML_STRING,
    TOML_INTEGER,
    TOML_BOOLEAN,
    TOML_STRING_ARRAY
} TomlType;

typedef struct {
    char section[TOML_MAX_SECT];
    char key[TOML_MAX_KEY];
    TomlType type;
    char str_val[TOML_MAX_VAL];
    long int_val;
    int bool_val;
    char arr_vals[TOML_MAX_ARR][TOML_MAX_VAL];
    int arr_count;
} TomlEntry;

typedef struct {
    TomlEntry *entries;
    int count;
    int capacity;
} TomlTable;

static inline TomlTable *toml_create(void) {
    TomlTable *t = (TomlTable *)calloc(1, sizeof(TomlTable));
    if (!t) return NULL;
    t->capacity = 64;
    t->entries = (TomlEntry *)calloc((size_t)t->capacity, sizeof(TomlEntry));
    if (!t->entries) { free(t); return NULL; }
    return t;
}

static inline void toml_free(TomlTable *t) {
    if (t) { free(t->entries); free(t); }
}

static inline void toml_add_entry(TomlTable *t, TomlEntry *e) {
    if (t->count >= t->capacity) {
        t->capacity *= 2;
        t->entries = (TomlEntry *)realloc(t->entries, (size_t)t->capacity * sizeof(TomlEntry));
    }
    t->entries[t->count++] = *e;
}

static inline char *toml_strip(char *s) {
    while (*s && isspace((unsigned char)*s)) s++;
    char *end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) *end-- = '\0';
    return s;
}

static inline int toml_parse_array(const char *val, TomlEntry *e) {
    e->type = TOML_STRING_ARRAY;
    e->arr_count = 0;
    const char *p = val;
    while (*p && *p != '[') p++;
    if (*p != '[') return -1;
    p++;

    while (*p && *p != ']' && e->arr_count < TOML_MAX_ARR) {
        while (*p && (isspace((unsigned char)*p) || *p == ',')) p++;
        if (*p == ']') break;
        if (*p == '"') {
            p++;
            const char *start = p;
            while (*p && *p != '"') p++;
            size_t len = (size_t)(p - start);
            if (len >= TOML_MAX_VAL) len = TOML_MAX_VAL - 1;
            memcpy(e->arr_vals[e->arr_count], start, len);
            e->arr_vals[e->arr_count][len] = '\0';
            e->arr_count++;
            if (*p == '"') p++;
        } else {
            /* unquoted value */
            const char *start = p;
            while (*p && *p != ',' && *p != ']') p++;
            size_t len = (size_t)(p - start);
            while (len > 0 && isspace((unsigned char)start[len-1])) len--;
            if (len >= TOML_MAX_VAL) len = TOML_MAX_VAL - 1;
            memcpy(e->arr_vals[e->arr_count], start, len);
            e->arr_vals[e->arr_count][len] = '\0';
            e->arr_count++;
        }
    }
    return 0;
}

static inline TomlTable *toml_parse_file(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return NULL;

    TomlTable *t = toml_create();
    if (!t) { fclose(fp); return NULL; }

    char section[TOML_MAX_SECT] = "";
    char line[2048];

    while (fgets(line, sizeof(line), fp)) {
        char *s = toml_strip(line);

        /* Skip empty lines and comments */
        if (!*s || *s == '#') continue;

        /* Section header */
        if (*s == '[') {
            s++;
            char *end = strchr(s, ']');
            if (end) {
                *end = '\0';
                char *sect = toml_strip(s);
                strncpy(section, sect, TOML_MAX_SECT - 1);
                section[TOML_MAX_SECT - 1] = '\0';
            }
            continue;
        }

        /* Key = value */
        char *eq = strchr(s, '=');
        if (!eq) continue;
        *eq = '\0';

        char *key = toml_strip(s);
        char *val = toml_strip(eq + 1);

        TomlEntry e;
        memset(&e, 0, sizeof(e));
        snprintf(e.section, TOML_MAX_SECT, "%s", section);
        snprintf(e.key, TOML_MAX_KEY, "%s", key);

        if (val[0] == '[') {
            /* Array */
            toml_parse_array(val, &e);
        } else if (val[0] == '"') {
            /* String */
            e.type = TOML_STRING;
            val++;
            char *endq = strrchr(val, '"');
            if (endq) *endq = '\0';
            strncpy(e.str_val, val, TOML_MAX_VAL - 1);
        } else if (strcmp(val, "true") == 0 || strcmp(val, "false") == 0) {
            e.type = TOML_BOOLEAN;
            e.bool_val = (strcmp(val, "true") == 0) ? 1 : 0;
        } else {
            /* Integer */
            e.type = TOML_INTEGER;
            e.int_val = strtol(val, NULL, 10);
        }

        toml_add_entry(t, &e);
    }

    fclose(fp);
    return t;
}

static inline const TomlEntry *toml_find(const TomlTable *t, const char *section, const char *key) {
    for (int i = 0; i < t->count; i++) {
        if (strcmp(t->entries[i].section, section) == 0 &&
            strcmp(t->entries[i].key, key) == 0) {
            return &t->entries[i];
        }
    }
    return NULL;
}

static inline const char *toml_get_string(const TomlTable *t, const char *section,
                                           const char *key, const char *def) {
    const TomlEntry *e = toml_find(t, section, key);
    if (!e || e->type != TOML_STRING) return def;
    return e->str_val;
}

static inline long toml_get_int(const TomlTable *t, const char *section,
                                 const char *key, long def) {
    const TomlEntry *e = toml_find(t, section, key);
    if (!e || e->type != TOML_INTEGER) return def;
    return e->int_val;
}

static inline int toml_get_bool(const TomlTable *t, const char *section,
                                 const char *key, int def) {
    const TomlEntry *e = toml_find(t, section, key);
    if (!e || e->type != TOML_BOOLEAN) return def;
    return e->bool_val;
}

#endif /* LK_TOML_H */
