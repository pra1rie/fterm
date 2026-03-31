#ifndef _PARSER_H
#define _PARSER_H

#define VARIABLE_MAX 1024
#define STRING_MAX 256

enum { T_NIL, T_STR, T_INT, T_REAL };
struct value {
    char type;
    union {
        char *as_str;
        long as_int;
        double as_real;
    };
};

struct key {
    char *keyname;
    char *action;
};

struct variable {
    char *name;
    struct value value;
};

struct config {
    struct variable vars[VARIABLE_MAX];
    struct key keys[VARIABLE_MAX];
    int nvars, nkeys;
};

struct config init_config(const char *path);
void free_config(struct config *cfg);
void set_key(struct config *cfg, char *name, char *action);
void set_var(struct config *cfg, char *name, struct value val);
struct value get_var(struct config *cfg, char *name);
struct value get_var_type(struct config *cfg, char *name, char type);

#endif
