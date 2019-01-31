/*

                  _
                 | |
   _____   ____ _| |  ___
  / _ \ \ / / _` | | / __|
 |  __/\ V / (_| | || (__
  \___| \_/ \__,_|_(_)___|

 eval.c: Expression evaluation

 ---------------------------------------------------------------------

 Copyright 2018-2019 Caian R. Ertl <hi@caian.org>

 This program is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program.  If not, see <http://www.gnu.org/licenses/>.

 */

#include "eval.h"
#include "builtin.h"
#include "fmt.h"


/*
 * ---------- PROTOTYPES ----------
 */


// built-in functions
tlenv_T* tlenv_copy   (tlenv_T* env);
void     tlenv_del    (tlenv_T* e);
tlval_T* tlenv_get    (tlenv_T* env, tlval_T* val);
void     tlenv_incb   (tlenv_T* env, const char* name, tlbtin func);
void     tlenv_init   (tlenv_T* env);
tlenv_T* tlenv_new    (void);
void     tlenv_put    (tlenv_T* env, tlval_T* var, tlval_T* value);
void     tlenv_putg   (tlenv_T* env, tlval_T* var, tlval_T* value);
char*    tltype_nrepr (int type);
tlval_T* tlval_add    (tlval_T* v, tlval_T* x);
tlval_T* tlval_copy   (tlval_T* val);
void     tlval_del    (tlval_T* v);
tlval_T* tlval_err    (const char* fmt, ...);
tlval_T* tlval_eval   (tlenv_T* env, tlval_T* value);
tlval_T* tlval_evsexp (tlenv_T* env, tlval_T* val);
tlval_T* tlval_fun    (tlbtin func);
tlval_T* tlval_join   (tlval_T* x, tlval_T* y);
tlval_T* tlval_lambda (tlval_T* formals, tlval_T* body);
tlval_T* tlval_num    (float n);
tlval_T* tlval_pop    (tlval_T* t, size_t i);
tlval_T* tlval_qexpr  (void);
tlval_T* tlval_read   (mpc_ast_t* t);
tlval_T* tlval_rnum   (mpc_ast_t* t);
tlval_T* tlval_sexpr  (void);
tlval_T* tlval_sym    (const char* s);
tlval_T* tlval_take   (tlval_T* t, size_t i);

tlval_T* btinfn_add    (tlenv_T* env, tlval_T* args);
tlval_T* btinfn_div    (tlenv_T* env, tlval_T* args);
tlval_T* btinfn_eval   (tlenv_T* env, tlval_T* qexpr);
tlval_T* btinfn_global (tlenv_T* env, tlval_T* qexpr);
tlval_T* btinfn_head   (tlenv_T* env, tlval_T* qexpr);
tlval_T* btinfn_join   (tlenv_T* env, tlval_T* qexprv);
tlval_T* btinfn_lambda (tlenv_T* env, tlval_T* qexpr);
tlval_T* btinfn_let    (tlenv_T* env, tlval_T* qexpr);
tlval_T* btinfn_list   (tlenv_T* env, tlval_T* sexpr);
tlval_T* btinfn_max    (tlenv_T* env, tlval_T* args);
tlval_T* btinfn_min    (tlenv_T* env, tlval_T* args);
tlval_T* btinfn_mod    (tlenv_T* env, tlval_T* args);
tlval_T* btinfn_mul    (tlenv_T* env, tlval_T* args);
tlval_T* btinfn_pow    (tlenv_T* env, tlval_T* args);
tlval_T* btinfn_sqrt   (tlenv_T* env, tlval_T* args);
tlval_T* btinfn_sub    (tlenv_T* env, tlval_T* args);
tlval_T* btinfn_tail   (tlenv_T* env, tlval_T* qexpr);



/*
 * ---------- TYPE REPRESENTATION ----------
 */


/**
 * tltype_nrepr - TL type name representation
 */
char* tltype_nrepr(int type)
{
    switch(type)
    {
        case TLVAL_FUN:
            return "Function";
            break;

        case TLVAL_NUM:
            return "Number";
            break;

        case TLVAL_ERR:
            return "Error";
            break;

        case TLVAL_SYM:
            return "Symbol";
            break;

        case TLVAL_SEXPR:
            return "S-Expression";
            break;

        case TLVAL_QEXPR:
            return "Q-Expression";
            break;

        default:
            return "Undefined";
            break;
    }
}


/*
 * ---------- VALUE CONSTRUCTORS ----------
 */


/**
 * tlval_fun - TL function representation
 *
 * Constructs a pointer to a new TL function representation.
 */
tlval_T* tlval_fun(tlbtin func)
{
    tlval_T* v = malloc(sizeof(struct tlval_S));
    v->type = TLVAL_FUN;
    v->builtin = func;

    return v;
}


/**
 * tlval_lambda - TL lambda representation
 *
 * Constructs a pointer to a new TL lambda representation.
 */
tlval_T* tlval_lambda(tlval_T* formals, tlval_T* body)
{
    tlval_T* v = malloc(sizeof(struct tlval_S));
    v->type = TLVAL_FUN;
    v->builtin = NULL;
    v->environ = tlenv_new();
    v->formals = formals;
    v->body = body;

    return v;
}


/**
 * tlval_num - TL number representation
 *
 * Constructs a pointer to a new TL number representation.
 */
tlval_T* tlval_num(float n)
{
    tlval_T* v = malloc(sizeof(struct tlval_S));
    v->type = TLVAL_NUM;
    v->number = n;

    return v;
}


/**
 * tlval_err - TL error representation
 *
 * Constructs a pointer to a new TL error representation.
 */
tlval_T* tlval_err(const char* fmt, ...)
{
    tlval_T* v = malloc(sizeof(struct tlval_S));
    v->type = TLVAL_ERR;

    va_list va;
    va_start(va, fmt);

    v->error = malloc(ERROR_MESSAGE_BYTE_LENGTH);
    vsnprintf(v->error, (ERROR_MESSAGE_BYTE_LENGTH - 1), fmt, va);

    v->error = realloc(v->error, strlen(v->error) + 1);
    va_end(va);

    return v;
}


/**
 * tlval_sym - TL symbol representation
 *
 * Constructs a pointer to a new TL symbol representation.
 */
tlval_T* tlval_sym(const char* s)
{
    tlval_T* v = malloc(sizeof(struct tlval_S));
    v->type = TLVAL_SYM;
    v->symbol = malloc(strlen(s) + 1);
    strcpy(v->symbol, s);

    return v;
}


/**
 * tlval_sexpr - TL S-Expression representation
 *
 * Constructs a pointer to a new TL symbolic expression representation.
 */
tlval_T* tlval_sexpr(void)
{
    tlval_T* v = malloc(sizeof(struct tlval_S));
    v->type = TLVAL_SEXPR;
    v->counter = 0;
    v->cell = NULL;

    return v;
}


/**
 * tlval_qexpr - TL Q-Expression representation
 *
 * Constructs a pointer to a new TL quoted expression representation.
 */
tlval_T* tlval_qexpr(void)
{
    tlval_T* v = malloc(sizeof(struct tlval_S));
    v->type = TLVAL_QEXPR;
    v->counter = 0;
    v->cell = NULL;

    return v;
}


/*
 * ---------- ENVIRONMENT CONSTRUCTORS ----------
 */


/**
 * tlenv_new - TL environment creation
 *
 * Constructs a pointer to a new TL environment representation.
 */
tlenv_T* tlenv_new(void)
{
    tlenv_T* e = malloc(sizeof(struct tlenv_S));

    e->counter = 0;
    e->symbols = NULL;
    e->values  = NULL;
    e->parent  = NULL;

    return e;
}


/*
 * ---------- ENVIRONMENT OPERATIONS ----------
 */


void tlenv_init(tlenv_T* env)
{
    tlenv_incb(env, "add", btinfn_add);
    tlenv_incb(env, "sub", btinfn_sub);
    tlenv_incb(env, "mul", btinfn_mul);
    tlenv_incb(env, "div", btinfn_div);
    tlenv_incb(env, "mod", btinfn_mod);
    tlenv_incb(env, "pow", btinfn_pow);
    tlenv_incb(env, "max", btinfn_max);
    tlenv_incb(env, "min", btinfn_min);
    tlenv_incb(env, "sqrt", btinfn_sqrt);

    tlenv_incb(env, "let", btinfn_let);
    tlenv_incb(env, "global", btinfn_global);

    tlenv_incb(env, "head", btinfn_head);
    tlenv_incb(env, "tail", btinfn_tail);
    tlenv_incb(env, "list", btinfn_list);
    tlenv_incb(env, "join", btinfn_join);
    tlenv_incb(env, "eval", btinfn_eval);

    tlenv_incb(env, "lambda", btinfn_lambda);
}


/**
 * tlenv_incbin - TL environment include built-in
 */
void tlenv_incb(tlenv_T* env, const char* fname, tlbtin fref)
{
    tlval_T* symb = tlval_sym(fname);
    tlval_T* func = tlval_fun(fref);
    tlenv_put(env, symb, func);

    tlval_del(symb);
    tlval_del(func);
}


/**
 * tlenv_put - Put variable to an inner environment
 */
void tlenv_put(tlenv_T* env, tlval_T* var, tlval_T* value)
{
    for(size_t i = 0; i < env->counter; i++)
    {
        if(strequ(env->symbols[i], var->symbol))
        {
            tlval_del(env->values[i]);
            env->values[i] = tlval_copy(value);

            return;
        }
    }

    env->counter++;
    env->values = realloc(env->values, (sizeof(tlval_T*) * env->counter));
    env->symbols = realloc(env->symbols, (sizeof(char*) * env->counter));

    env->values[env->counter - 1] = tlval_copy(value);
    env->symbols[env->counter - 1] = malloc(strlen(var->symbol) + 1);

    strcpy(env->symbols[env->counter - 1], var->symbol);
}


/**
 * tlenv_put - Put variable to the global environment
 */
void tlenv_putg(tlenv_T* env, tlval_T* var, tlval_T* value)
{
    while(env->parent)
        env = env->parent;

    tlenv_put(env, var, value);
}


/**
 * tlenv_del - Environment creation
 */
void tlenv_del(tlenv_T* e)
{
    for(size_t i = 0; i < e->counter; i++)
    {
        free(e->symbols[i]);
        tlval_del(e->values[i]);
    }

    free(e->symbols);
    free(e->values);
    free(e);
}


/**
 * tlenv_get - Get from environment
 *
 * Takes an environment and returns a given expression if it exists.
 */
tlval_T* tlenv_get(tlenv_T* env, tlval_T* val)
{
    for(size_t i = 0; i < env->counter; i++)
    {
        if(strequ(val->symbol, env->symbols[i]))
            return tlval_copy(env->values[i]);
    }

    if(env->parent)
        return tlenv_get(env->parent, val);
    else
        return tlval_err(TLERR_UNBOUND_SYM, val->symbol);
}


tlenv_T* tlenv_copy(tlenv_T* env)
{
    tlenv_T* nenv = malloc(sizeof(struct tlenv_S));

    nenv->parent  = env->parent;
    nenv->counter = env->counter;
    nenv->symbols = malloc(sizeof(char*) * nenv->counter);
    nenv->values  = malloc(sizeof(tlval_T*) * nenv->counter);

    for(size_t i = 0; i < nenv->counter; i++)
    {
        nenv->symbols[i] = malloc(strlen(env->symbols[i]) + 1);
        strcpy(nenv->symbols[i], env->symbols[i]);

        nenv->values[i] = tlval_copy(env->values[i]);
    }

    return nenv;
}


/*
 * ---------- PARSED REPRESENTATION ----------
 */


/**
 * tlval_read - TL value reading
 */
tlval_T* tlval_read(mpc_ast_t* t)
{
    if(strstr(t->tag, "number"))
        return tlval_rnum(t);

    if(strstr(t->tag, "symbol"))
        return tlval_sym(t->contents);

    tlval_T* x = NULL;
    if(strequ(t->tag, ">") || strstr(t->tag, "sexpr"))
        x = tlval_sexpr();

    if(strstr(t->tag, "qexpr"))
        x = tlval_qexpr();

    for(int i = 0; i < t->children_num; i++)
    {
        if(strequ(t->children[i]->contents, "(") ||
           strequ(t->children[i]->contents, ")") ||
           strequ(t->children[i]->contents, "'(") ||
           strequ(t->children[i]->tag, "regex")) continue;

        x = tlval_add(x, tlval_read(t->children[i]));
    }

    return x;
}


/**
 * tlval_rnum - TL numeric value reading
 */
tlval_T* tlval_rnum(mpc_ast_t* t)
{
    errno = 0;
    float f = strtof(t->contents, NULL);

    return (errno != ERANGE)
        ? tlval_num(f)
        : tlval_err(TLERR_BAD_NUM);
}


/*
 * ---------- EXPRESSION OPERATIONS ----------
 */


/**
 * tlval_add - TL value addition
 *
 * Adds a TL value to a S-Expression construct.
 */
tlval_T* tlval_add(tlval_T* v, tlval_T* x)
{
    v->counter++;
    v->cell = realloc(v->cell, sizeof(struct tlval_S) * v->counter);
    v->cell[v->counter - 1] = x;

    return v;
}


/**
 * tlval_del - TL value deletion
 *
 * Recursively deconstructs a TL value.
 */
void tlval_del(tlval_T* v)
{
    switch(v->type)
    {
        case TLVAL_NUM: break;

        case TLVAL_FUN:
            if(!v->builtin)
            {
                tlenv_del(v->environ);
                tlval_del(v->formals);
                tlval_del(v->body);
            }
            break;

        case TLVAL_ERR:
            free(v->error);
            break;

        case TLVAL_SYM:
            free(v->symbol);
            break;

        case TLVAL_SEXPR:
        case TLVAL_QEXPR:
            for(size_t i = 0; i < v->counter; i++)
                tlval_del(v->cell[i]);

            free(v->cell);
            break;
    }

    free(v);
}


/**
 * tlval_copy - TL value copying
 *
 * Takes an expression, copy it's content to a new memory location and returns it.
 */
tlval_T* tlval_copy(tlval_T* val)
{
    tlval_T* nval = malloc(sizeof(struct tlval_S));
    nval->type = val->type;

    switch(val->type)
    {
        case TLVAL_FUN:
            if(val->builtin)
            {
                nval->builtin = val->builtin;
            }
            else
            {
                nval->builtin = NULL;
                nval->environ = tlenv_copy(val->environ);
                nval->formals = tlval_copy(val->formals);
                nval->body = tlval_copy(val->body);
            }
            break;

        case TLVAL_NUM:
            nval->number = val->number;
            break;

        case TLVAL_ERR:
            nval->error = malloc(strlen(val->error) + 1);
            strcpy(nval->error, val->error);
            break;

        case TLVAL_SYM:
            nval->symbol = malloc(strlen(val->symbol) + 1);
            strcpy(nval->symbol, val->symbol);
            break;

        case TLVAL_SEXPR:
        case TLVAL_QEXPR:
            nval->counter = val->counter;
            nval->cell = malloc(sizeof(struct tlval_S) * nval->counter);

            for(size_t i = 0; i < nval->counter; i++)
                nval->cell[i] = tlval_copy(val->cell[i]);
            break;
    }

    return nval;
}


/**
 * tlval_pop - TL pop value operation
 *
 * Takes a S-Expression, extracts the element at index "i" from it and returns it.
 * The "pop" operation does not delete the original input list.
 */
tlval_T* tlval_pop(tlval_T* t, size_t i)
{
    tlval_T* v = t->cell[i];

    memmove(&t->cell[i], &t->cell[i + 1], (sizeof(tlval_T*) * t->counter));

    t->counter--;
    t->cell = realloc(t->cell, (sizeof(tlval_T*) * t->counter));

    return v;
}


/**
 * tlval_take - TL value take operation
 *
 * Takes a S-Expression, extracts the element at index "i" from it and return it.
 * The "take" operation deletes the original input list;
 */
tlval_T* tlval_take(tlval_T* t, size_t i)
{
    tlval_T* v = tlval_pop(t, i);
    tlval_del(t);

    return v;
}


/**
 * tlval_join - TL value join
 *
 * Takes two expressions and joins together all of its arguments.
 */
tlval_T* tlval_join(tlval_T* x, tlval_T* y)
{
    while(y->counter)
        x = tlval_add(x, tlval_pop(y, 0));

    tlval_del(y);
    return x;
}


tlval_T* tlval_call(tlenv_T* env, tlval_T* func, tlval_T* args)
{
    if(func->builtin)
        return func->builtin(env, args);

    size_t given = args->counter;
    size_t total = func->formals->counter;

    while(args->counter)
    {
        if(func->formals->counter == 0)
        {
            tlval_del(args);
            return tlval_err(
                    "function has taken too many arguments."
                    "Got %i, expected %i", given, total);
        }

        tlval_T* symbol = tlval_pop(func->formals, 0);
        tlval_T* value  = tlval_pop(args, 0);

        tlenv_put(func->environ, symbol, value);

        tlval_del(symbol);
        tlval_del(value);
    }

    tlval_del(args);

    if(func->formals->counter == 0)
    {
        func->environ->parent = env;

        return btinfn_eval(func->environ,
                tlval_add(tlval_sexpr(), tlval_copy(func->body)));
    }
    else
    {
        return tlval_copy(func);
    }
}


/*
 * ---------- EXPRESSION EVALUATION ----------
 */


/**
 * tlval_eval - TL value evaluation
 */
tlval_T* tlval_eval(tlenv_T* env, tlval_T* value)
{
    if(value->type == TLVAL_SYM)
    {
        tlval_T* nval = tlenv_get(env, value);
        tlval_del(value);

        return nval;
    }

    if(value->type == TLVAL_SEXPR)
        return tlval_evsexp(env, value);

    return value;
}


/**
 * tlval_evsexp - TL S-Expression evaluation
 */
tlval_T* tlval_evsexp(tlenv_T* env, tlval_T* val)
{
    for(size_t i = 0; i < val->counter; i++)
        val->cell[i] = tlval_eval(env, val->cell[i]);

    for(size_t i = 0; i < val->counter; i++)
    {
        if(val->cell[i]->type == TLVAL_ERR)
            return tlval_take(val, i);
    }

    if(val->counter == 0)
        return val;

    if(val->counter == 1)
        return tlval_take(val, 0);

    tlval_T* element = tlval_pop(val, 0);
    if(element->type != TLVAL_FUN)
    {
        tlval_T* err = tlval_err(
                    "S-Expression start with incorrect type. "
                    "Got '%s', expected '%s'.",
                    tltype_nrepr(element->type), tltype_nrepr(TLVAL_FUN));

        tlval_del(val);
        tlval_del(element);

        return err;
    }

    tlval_T* res = tlval_call(env, element, val);
    tlval_del(element);

    return res;
}


