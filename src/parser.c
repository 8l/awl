#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "mpc.h"
#include "assert.h"
#include "util.h"

static mpc_parser_t* Integer;
static mpc_parser_t* FPoint;
static mpc_parser_t* Number;
static mpc_parser_t* Bool;
static mpc_parser_t* String;
static mpc_parser_t* Comment;
static mpc_parser_t* Symbol;
static mpc_parser_t* QSymbol;
static mpc_parser_t* Sexpr;
static mpc_parser_t* Qexpr;
static mpc_parser_t* EExpr;
static mpc_parser_t* CExpr;
static mpc_parser_t* Expr;
static mpc_parser_t* Awl;

void setup_parser(void) {
    Integer = mpc_new("integer");
    FPoint = mpc_new("fpoint");
    Number = mpc_new("number");
    Bool = mpc_new("bool");
    String = mpc_new("string");
    Comment = mpc_new("comment");
    Symbol = mpc_new("symbol");
    QSymbol = mpc_new("qsymbol");
    Sexpr = mpc_new("sexpr");
    Qexpr = mpc_new("qexpr");
    EExpr = mpc_new("eexpr");
    CExpr = mpc_new("cexpr");
    Expr = mpc_new("expr");
    Awl = mpc_new("awl");

    mpca_lang(MPCA_LANG_DEFAULT,
        "                                                                   \
        integer : /[+-]?[0-9]+/ ;                                           \
        fpoint  : /[+-]?[0-9]+\\.[0-9]*/ | /[+-]?[0-9]*\\.[0-9]+/ ;         \
        number  : <fpoint> | <integer> ;                                    \
        bool    : \"true\" | \"false\" ;                                    \
        string  : /\"(\\\\.|[^\"])*\"/ | /'(\\\\.|[^'])*'/ ;                \
        comment : /;[^\\r\\n]*/ ;                                           \
        symbol  : /[a-zA-Z0-9_+\\-*\\/=<>!\\?&%^$]+/ ;                      \
        qsymbol : ':' <symbol> ;                                            \
        sexpr   : '(' <expr>* ')' ;                                         \
        qexpr   : '{' <expr>* '}' ;                                         \
        eexpr   : '\\\\' <expr> ;                                           \
        cexpr   : '@' <expr> ;                                              \
        expr    : <number> | <bool> | <string> | <symbol> | <qsymbol> |     \
                  <comment> | <sexpr> | <qexpr> | <eexpr> | <cexpr> ;       \
        awl     : /^/ <expr>* /$/ ;                                         \
        ",
        Integer, FPoint, Number, Bool, String, Comment, Symbol, QSymbol, Sexpr, Qexpr, EExpr, CExpr, Expr, Awl);
}

void teardown_parser(void) {
    mpc_cleanup(12, Integer, FPoint, Number, Bool, String, Comment, Symbol, QSymbol, Sexpr, Qexpr, EExpr, CExpr, Expr, Awl);
}

static awlval* awlval_read_int(const mpc_ast_t* t) {
    errno = 0;
    long x = strtol(t->contents, NULL, 10);
    return errno != ERANGE ? awlval_int(x) : awlval_err("invalid number: %s", t->contents);
}

static awlval* awlval_read_float(const mpc_ast_t* t) {
    errno = 0;
    double x = strtod(t->contents, NULL);
    return errno != ERANGE ? awlval_float(x) : awlval_err("invalid float: %s", t->contents);
}

static awlval* awlval_read_bool(const mpc_ast_t* t) {
    if (streq(t->contents, "true")) {
        return awlval_bool(true);
    }
    return awlval_bool(false);
}

static awlval* awlval_read_string(const mpc_ast_t* t) {
    t->contents[strlen(t->contents) - 1] = '\0';

    char* unescaped = safe_malloc(strlen(t->contents + 1) + 1);
    strcpy(unescaped, t->contents + 1);

    unescaped = mpcf_unescape(unescaped);
    awlval* str = awlval_str(unescaped);

    free(unescaped);
    return str;
}

static awlval* awlval_read_qsym(const mpc_ast_t* t) {
    return awlval_qsym(t->children[1]->contents);
}

static awlval* awlval_read_sym(const mpc_ast_t* t) {
    return awlval_sym(t->contents);
}

static awlval* awlval_read(const mpc_ast_t* t) {
    if (strstr(t->tag, "integer")) {
        return awlval_read_int(t);
    }
    if (strstr(t->tag, "fpoint")) {
        return awlval_read_float(t);
    }
    if (strstr(t->tag, "bool")) {
        return awlval_read_bool(t);
    }
    if (strstr(t->tag, "string")) {
        return awlval_read_string(t);
    }
    if (strstr(t->tag, "qsymbol")) {
        return awlval_read_qsym(t);
    }
    if (strstr(t->tag, "symbol")) {
        return awlval_read_sym(t);
    }

    awlval* x = NULL;
    /* If root '>' */
    if (streq(t->tag, ">")) {
        x = awlval_sexpr();
    }
    else if (strstr(t->tag, "sexpr")) {
        x = awlval_sexpr();
    }
    else if (strstr(t->tag, "qexpr")) {
        x = awlval_qexpr();
    }
    else if (strstr(t->tag, "eexpr")) {
        x = awlval_eexpr();
    }
    else if (strstr(t->tag, "cexpr")) {
        x = awlval_cexpr();
    }

    for (int i = 0; i < t->children_num; i++) {
        if (streq(t->children[i]->contents, "(")) { continue; }
        if (streq(t->children[i]->contents, ")")) { continue; }
        if (streq(t->children[i]->contents, "{")) { continue; }
        if (streq(t->children[i]->contents, "}")) { continue; }
        if (streq(t->children[i]->contents, ":")) { continue; }
        if (streq(t->children[i]->contents, "\\")) { continue; }
        if (streq(t->children[i]->contents, "@")) { continue; }
        if (streq(t->children[i]->tag, "regex")) { continue; }
        if (strstr(t->children[i]->tag, "comment")) { continue; }
        x = awlval_add(x, awlval_read(t->children[i]));
    }

    return x;
}

bool awlval_parse(const char* input, awlval** v, char** err) {
    mpc_result_t r;
    if (mpc_parse("<stdin>", input, Awl, &r)) {
        *v = awlval_read(r.output);
        mpc_ast_delete(r.output);
        return true;
    } else {
        *err = mpc_err_string(r.error);
        mpc_err_delete(r.error);
        return false;
    }
}

bool awlval_parse_file(const char* file, awlval** v, char** err) {
    mpc_result_t r;
    if (mpc_parse_contents(file, Awl, &r)) {
        *v = awlval_read(r.output);
        mpc_ast_delete(r.output);
        return true;
    } else {
        *err = mpc_err_string(r.error);
        mpc_err_delete(r.error);
        return false;
    }
}
