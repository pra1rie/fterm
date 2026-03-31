#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <ctype.h>
#include "parser.h"

extern char default_path[4096];

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
    } else if (strcmp(tok.ptr, "true") == 0) {
        val.type = T_INT;
        val.as_int = 1;
    } else if (strcmp(tok.ptr, "false") == 0) {
        val.type = T_INT;
        val.as_int = 0;
    } else {
        fprintf(stderr, "error: unknown value: %.*s\n", tok.sz, tok.ptr);
    }
    return val;
}

static inline void _include_file(struct config *cfg, const char *path) {
    char cfg_path[4096] = {0};
    if (access(path, F_OK) != 0) sprintf(cfg_path, "%s/%s", default_path, path);
    else sprintf(cfg_path, "%s", path);
    // don't free config so its values remain allocated
    struct config inc = init_config(cfg_path);
    for (int i = 0; i < inc.nvars; ++i)
        set_var(cfg, inc.vars[i].name, inc.vars[i].value);
    for (int i = 0; i < inc.nkeys; ++i)
        set_key(cfg, inc.keys[i].keyname, inc.keys[i].action);
}

struct config init_config(const char *path) {
    struct _parser parser = { .file = fopen(path, "r") };
    struct config cfg = {0};
    if (!parser.file) {
        fprintf(stderr, "error: could not open file '%s'\n", path);
        return cfg;
    }

    struct _string tok = _next_token(&parser);
    while (tok.sz) {
        if (!strncmp("include", tok.ptr, tok.sz)) {
            tok = _next_token(&parser);
            struct value val = _token_to_value(tok);
            if (val.type != T_STR) {
                fprintf(stderr, "error: include expected path\n");
            } else {
                _include_file(&cfg, val.as_str);
                free(val.as_str);
            }
            continue;
        }
        struct _string prv = tok;
        tok = _next_token(&parser);
        if (!tok.sz) break;
        if (!strncmp(prv.ptr, "bind", prv.sz)) {
            struct _string action = _next_token(&parser);
            if (action.sz) {
                struct value key = _token_to_value(tok);
                if (key.type == T_STR)
                    set_key(&cfg, key.as_str, strndup(action.ptr, action.sz));
                else
                    fprintf(stderr, "error: bind expected a string key\n");
            } else {
                fprintf(stderr, "error: unexpected EOF\n");
            }
        } else if (!strncmp(tok.ptr, "=", tok.sz)) {
            tok = _next_token(&parser);
            if (tok.sz)
                set_var(&cfg, strndup(prv.ptr, prv.sz), _token_to_value(tok));
            else
                fprintf(stderr, "error: unexpected EOF\n");
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
    for (int i = 0; i < cfg->nkeys; ++i) {
        free(cfg->keys[i].keyname);
        free(cfg->keys[i].action);
    }
    cfg->nvars = cfg->nkeys = 0;
}

void set_key(struct config *cfg, char *name, char *action) {
    for (int i = 0; i < cfg->nkeys; ++i) {
        struct key *key = &cfg->keys[i];
        if (!strcmp(key->keyname, name)) {
            free(key->action);
            key->action = action;
            return;
        }
    }
    cfg->keys[cfg->nkeys++] = (struct key) { name, action };
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
        return (struct value) { T_NIL };
    }
    return val;
}
