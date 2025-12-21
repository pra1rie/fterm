#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "parser.h"

struct _parser {
    FILE *file;
};

struct _string {
    int sz;
    char ptr[STRING_MAX];
};

static inline void strpush(struct _string *a, char b) {
    a->ptr[a->sz++] = b;
}

static char _skip_space(struct _parser *parser) {
    char c = fgetc(parser->file);
    while (c != EOF && isspace(c)) c = fgetc(parser->file);
    if (c == '#') {
        while (c != EOF && c != '\n') c = fgetc(parser->file);
        ungetc(c, parser->file);
        return _skip_space(parser);
    }
    return c;
}

static struct _string _next_token(struct _parser *parser) {
    struct _string str = {0};
    char c = _skip_space(parser);
    if (c == EOF) return str;
    if (c == '\"') {
        do {
            strpush(&str, c);
            c = fgetc(parser->file);
        } while (c != '\"');
        strpush(&str, c);
        return str;
    }
    while (!(isspace(c) || c == '#')) {
        strpush(&str, c);
        c = fgetc(parser->file);
    }
    ungetc(c, parser->file);
    return str;
}

static inline char *_type_to_cstr(char t) {
    switch (t) {
    case T_STR:  return "str";
    case T_INT:  return "int";
    case T_REAL: return "real";
    default:     return "nil";
    }
}

static inline struct value _token_to_value(struct _string tok) {
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
    struct _parser parser = { .file = fopen(path, "r") };
    if (!parser.file) {
        fprintf(stderr, "error: could not open file '%s'\n", path);
        exit(1);
    }

    struct config cfg = {0};
    struct _string tok = _next_token(&parser);
    while (tok.sz) {
        if (!strncmp("include", tok.ptr, tok.sz)) {
            tok = _next_token(&parser);
            struct value val = _token_to_value(tok);
            if (val.type != T_STR) {
                fprintf(stderr, "error: include expected path\n");
                exit(1);
            }
            struct config inc = init_config(val.as_str);
            for (int i = 0; i < inc.nvars; ++i) {
                struct value v = inc.vars[i].value;
                // here hoping the compiler won't try to optimize this out lmfao
                if (v.type == T_STR) v.as_str = strdup(inc.vars[i].value.as_str);
                set_var(&cfg, strdup(inc.vars[i].name), v);
            }
            free_config(&inc);
            free(val.as_str);
            continue;
        }
        struct _string prv = tok;
        tok = _next_token(&parser);
        if (!tok.sz) break;
        if (!strncmp(tok.ptr, "=", tok.sz)) {
            tok = _next_token(&parser);
            if (!tok.sz) {
                fprintf(stderr, "error: unexpected EOF\n");
                exit(1);
            }
            set_var(&cfg, strndup(prv.ptr, prv.sz), _token_to_value(tok));
        }
    }

    fclose(parser.file);
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
            name, _type_to_cstr(type), _type_to_cstr(val.type));
        exit(1);
    }
    return val;
}
