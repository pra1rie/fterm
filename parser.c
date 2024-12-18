#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "parser.h"

static struct {
    FILE *file;
} parser;

struct string {
    int sz;
    char ptr[STRING_MAX];
};

static inline void strpush(struct string *a, char b) {
    a->ptr[a->sz++] = b;
}

static char skip_space() {
    char c = fgetc(parser.file);
    while (c != EOF && isspace(c)) c = fgetc(parser.file);
    if (c == '#') {
        while (c != EOF && c != '\n') c = fgetc(parser.file);
        ungetc(c, parser.file);
        return skip_space();
    }
    return c;
}

static struct string next_token() {
    struct string str = {0};
    char c = skip_space();
    if (c == EOF) return str;
    if (c == '\"') {
        do {
            strpush(&str, c);
            c = fgetc(parser.file);
        } while (c != '\"');
        strpush(&str, c);
        return str;
    }
    while (!(isspace(c) || c == '#')) {
        strpush(&str, c);
        c = fgetc(parser.file);
    }
    ungetc(c, parser.file);
    return str;
}

static inline char *type_to_cstr(char t) {
    switch (t) {
    case T_STR:  return "str";
    case T_INT:  return "int";
    case T_REAL: return "real";
    default:     return "nil";
    }
}

static inline struct value token_to_value(struct string tok) {
    struct value val = { T_NIL };
    if (tok.ptr[0] == '\"') {
        val.type = T_STR;
        val.as_str = strndup(tok.ptr+1, tok.sz-2);
    } else if (isdigit(tok.ptr[0])) {
        if (!strstr(tok.ptr, ".")) {
            val.type = T_INT;
            val.as_int = strtol(tok.ptr, NULL, 10);
        } else {
            val.type = T_REAL;
            val.as_real = strtod(tok.ptr, NULL);
        }
    } else {
        fprintf(stderr, "error: unknown value: %.*s\n", tok.sz, tok.ptr);
        exit(1);
    }
    return val;
}

struct config init_config(char *path) {
    if (parser.file) {
        fclose(parser.file);
        fprintf(stderr, "error: attempt to open different file mid-parsing\n");
        exit(1);
    }
    parser.file = fopen(path, "r");
    if (!parser.file) {
        fprintf(stderr, "error: could not open file '%s'\n", path);
        exit(1);
    }

    struct config cfg = {0};
    struct string tok = next_token();
    while (tok.sz) {
        struct string prv = tok;
        tok = next_token();
        if (!tok.sz) break;
        if (!strncmp(tok.ptr, "=", tok.sz)) {
            tok = next_token();
            if (!tok.sz) {
                fprintf(stderr, "error: unexpected EOF\n");
                exit(1);
            }
            set_var(&cfg, strndup(prv.ptr, prv.sz), token_to_value(tok));
        }
    }

    fclose(parser.file);
    parser.file = NULL;
    return cfg;
}

void free_config(struct config *cfg) {
    for (int i = 0; i < cfg->nvars; ++i) {
        free(cfg->vars[i].name);
        if (cfg->vars[i].value.type == T_STR)
            free(cfg->vars[i].value.as_str);
    }
    cfg->nvars = 0;
}

void set_var(struct config *cfg, char *name, struct value val) {
    for (int i = 0; i < cfg->nvars; ++i) {
        struct variable *var = &cfg->vars[i];
        if (!strcmp(var->name, name)) {
            if (var->value.type == T_STR) free(var->value.as_str);
            var->value = val;
            return;
        }
    }
    cfg->vars[cfg->nvars++] = (struct variable) { name, val };
}

struct value get_var(struct config *cfg, char *name) {
    for (int i = 0; i < cfg->nvars; ++i) {
        struct variable *var = &cfg->vars[i];
        if (!strcmp(var->name, name))
            return var->value;
    }
    return (struct value) { T_NIL };
}

struct value get_var_type(struct config *cfg, char *name, char type) {
    struct value val = get_var(cfg, name);
    if (val.type != type) {
        fprintf(stderr, "error: expected '%s' to be type '%s', but got '%s'\n",
            name, type_to_cstr(type), type_to_cstr(val.type));
        exit(1);
    }
    return val;
}
