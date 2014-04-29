#ifndef AWL_ASSERT_H
#define AWL_ASSERT_H

#define LASSERT(args, cond, fmt, ...) \
    if (!(cond)) { \
        lval* err = lval_err(fmt, ##__VA_ARGS__); \
        lval_del(args); \
        return err; \
    }

#define LASSERT_TYPE(args, i, expected, fname) \
    LASSERT(args, (args->cell[i]->type == expected), \
            "function '%s' passed incorrect type for arg %i; got %s, expected %s", \
            fname, i, ltype_name(args->cell[i]->type), ltype_name(expected));

#define LASSERT_ARGCOUNT(args, expected, fname) \
    LASSERT(args, (args->count == expected), \
            "function '%s' takes exactly %i argument(s); %i given", fname, expected, args->count);

#define LASSERT_MINARGCOUNT(args, min, fname) \
    LASSERT(args, (args->count >= min), \
            "function '%s' takes %i or more arguments; %i given", fname, min, args->count);

#define LASSERT_NONEMPTY(args, lval, fname) \
    LASSERT(args, (lval->count != 0), "function '%s' passed {}", fname);

#endif