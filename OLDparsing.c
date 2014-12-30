/*********************************************************
	HEADER SECTION - CHECKING FOR WINDOWS COMPATIBILITY ISSUES
*********************************************************/

#include "mpc.h"

/*if compiling on windows, compile these functions*/
#ifdef _WIN32

static char buffer[2048];

/*fake readline function*/
char* readline(char* prompt) {
    fputs(prompt, stdout);
    fgets(buffer, 2048, stdin);
    char* cpy = malloc(strlen(buffer) + 1);
    strcpy(cpy, buffer);
    cpy[strlen(cpy) - 1] = '\0';
    return cpy;
}

/*fake add_history function*/
void add_history(char* unused) {}

/*Otherwise include the editline headers*/
#else
#include <editline/readline.h>
#include <histedit.h>
#endif

/*********************************************************
	lval struct, with constructors
*********************************************************/

/*forward declarations*/
struct lval;
struct lenv;
typedef struct lval lval;
typedef struct lenv lenv;

/*Create enumeration of possible lval types*/
enum { LVAL_NUM, LVAL_ERR, 	 LVAL_SYM, 
	   LVAL_FUN, LVAL_SEXPR, LVAL_QEXPR };
	   
/*enumeration to string*/
char* ltype_name(int t) {
	switch (t) {
		case LVAL_FUN:   return "Function";
		case LVAL_NUM:   return "Number";
		case LVAL_ERR:   return "Error";
		case LVAL_SYM:   return "Symbol";
		case LVAL_SEXPR: return "S-Expression";
		case LVAL_QEXPR: return "Q-Expression";
		default: return "Unknown";
	}
}

/*Function pointer type lbuiltin*/
typedef lval* (*lbuiltin)(lenv*, lval*);

/*Declare New lval Struct*/
struct lval {
	int type;
	
	/*Basic*/
	long num;
	char* err;
	char* sym;
	
	/*function*/
	lbuiltin builtin;
	lenv* env;
	lval* formals;
	lval* body;
	
	/*expression*/
	int count;
	lval** cell;
};

/*Create a pointer to a new number lval*/
lval* lval_num(long x) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_NUM;
	v->num = x;
	return v;
}

/*Create a pointer to a new error lval*/
lval* lval_err(char* fmt, ...) {
	
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_ERR;
	
	/*Create a va list and initialize it*/
	va_list va;
	va_start(va, fmt);
	
	/*Allocate 512 bytes of space*/
	v->err = malloc(512);
	
	/*printf the error string with a maximum of 511 characters*/
	vsnprintf(v->err, 511, fmt, va);
	
	/*Reallocate to number of bytes actually used*/
	v->err = realloc(v->err, strlen(v->err) + 1);
	
	/*cleanup va list*/
	va_end(va);
	
	return v;
}

/*Create a pointer to a new Symbol lval*/
lval* lval_sym(char* s) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_SYM;
	v->sym = malloc(strlen(s) + 1);
	strcpy(v->sym, s);
	return v;
}

/*Create a pointer to a new empty Sexpr lval*/
lval* lval_sexpr(void) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_SEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

/*Create a pointer to a new empty Qexpr lval*/
lval* lval_qexpr(void) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_QEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

lval* lval_builtin(lbuiltin func) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_FUN;
	v->builtin = func;
	return v;
}

lenv* lenv_new(void);

lval* lval_lambda(lval* formals, lval* body) {
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_FUN;
	
	/*set builtin to null*/
	v->builtin = NULL;
	
	/*Build new environment*/
	v->env = lenv_new();
	
	/*set formals and body*/
	v->formals = formals;
	v->body = body;
	
	return v;
}

/*********************************************************
	lval_del, lval_add, lval_pop, lval_take, lval_print,
	lval_expr_print, lval_copy
*********************************************************/

void lenv_del(lenv* e);

/*delete an lval*/
void lval_del(lval* v) {
	switch (v->type) {
		/*nothing special for the number type*/
		case LVAL_NUM:
		case LVAL_FUN:
			if (!v->builtin) {
				lenv_del(v->env);
				lval_del(v->formals);
				lval_del(v->body);
			}
			break;
		
		/*for err or sym, free string data*/
		case LVAL_ERR: free(v->err); break;
		case LVAL_SYM: free(v->sym); break;
		
		/*for sexpr, qexpr, delete all inside elements*/
		case LVAL_QEXPR:
		case LVAL_SEXPR:
			for (int i = 0; i < v->count; i++) {
				lval_del(v->cell[i]);
			}
			/* and the memory allocated to the pointers*/
			free(v->cell);
			break;
		}
	/*free the memory allocated for the "lval" struct itself*/
	free(v);
}

/*add an lval to the lval cell list sexpr qexpr only*/
lval* lval_add(lval* v, lval* x) {
	v->count++;
	v->cell = realloc(v->cell, sizeof(lval*) * v->count);
	v->cell[v->count - 1] = x;
	return v;
}

/*pop an lval off of a list, risizing*/
lval* lval_pop(lval* v, int i) {
	/*find the item at i*/
	lval* x = v->cell[i];
	
	/*shift memory after the item at i over the top*/
	memmove(&v->cell[i], &v->cell[i + 1],
			sizeof(lval*) * (v->count - i - 1));
	
	/*decrease the count of items in the list*/
	v->count--;
	
	/*reallocate the memory used*/
	v->cell = realloc(v->cell, sizeof(lval*) * v->count);
	return x;
}

/*take - pop off lval at i*/
lval* lval_take(lval* v, int i) {
	lval* x = lval_pop(v, i);
	lval_del(v);
	return x;
}

/*forward declare lval_print*/
void lval_print(lval* v);

void lval_expr_print(lval* v, char open, char close) {
	putchar(open);
	for (int i = 0; i < v->count; i++) {
		
		/*print value contained within*/
		lval_print(v->cell[i]);
		
		/*dont print trailing space if last element*/
		if (i != (v->count - 1)) {
			putchar(' ');
		}
	}
	putchar(close);
}


void lval_print(lval* v) {
	switch (v->type) {
		
		case LVAL_FUN:
			if (v->builtin) {
				printf("<builtin>");
			} else {
				printf("(\\ "); lval_print(v->formals);
				putchar(' '); lval_print(v->body); putchar(')');
			}
			break;
		
		case LVAL_NUM:	 printf("%li", v->num); 	   break;
		case LVAL_ERR: 	 printf("Error: %s", v->err);  break;
		case LVAL_SYM:	 printf("%s", v->sym); 		   break;
		case LVAL_SEXPR: lval_expr_print(v, '(', ')'); break;
		case LVAL_QEXPR: lval_expr_print(v, '{', '}'); break;
	}
}

void lval_println(lval* v) { lval_print(v); putchar('\n'); }

lenv* lenv_copy(lenv* e);

lval* lval_copy(lval* v) {
	
	lval* x = malloc(sizeof(lval));
	x->type = v->type;
	
	switch (v->type) {
		
		/*Copy functions and numbers directly*/
		case LVAL_FUN: 
			if (v->builtin) {
				x->builtin = v->builtin;
			} else {
				x->builtin = NULL;
				x->env = lenv_copy(v->env);
				x->formals = lval_copy(v->formals);
				x->body = lval_copy(v->body);
			}
			break;
		
		case LVAL_NUM: x->num = v->num; break;
		
		/*copy strings using malloc and strcpy*/
		case LVAL_ERR:
			x->err = malloc(strlen(v->err) + 1);
			strcpy(x->err, v->err); break;
			
		case LVAL_SYM:
			x->sym = malloc(strlen(v->sym) + 1);
			strcpy(x->sym, v->sym); break;
			
		/*copy lists by copying each sub-expression*/
		case LVAL_SEXPR:
		case LVAL_QEXPR:
			x->count = v->count;
			x-> cell = malloc(sizeof(lval*) * x->count);
			for (int i = 0; i < x->count; i++) {
				x->cell[i] = lval_copy(v->cell[i]);
			}
		break;
	}
	return x;
}

/*********************************************************
	EVIRONMENT SETUP (CONSIDER MOVING?)
*********************************************************/

struct lenv {
	int count;
	char** syms;
	lval** vals;
};

lenv* lenv_new(void) {
	lenv* e = malloc(sizeof(lenv));
	e->count = 0;
	e->syms = NULL;
	e->vals = NULL;
	return e;
}

void lenv_del(lenv* e) {
	for (int i = 0; i < e->count; i++) {
		free(e->syms[i]);
		lval_del(e->vals[i]);
	}
	free(e->syms);
	free(e->vals);
	free(e);
}

lval* lenv_get(lenv* e, lval* k) {
	
	/*Iterate over all items in environment*/
	for (int i = 0; i < e->count; i++) {
		
		/*check if stored string matches the symbol string
		if it does, return a copy of the value*/
		if (strcmp(e->syms[i], k->sym) == 0) {
			return lval_copy(e->vals[i]);
		}
	}
	/*if no symbol found return error*/
	return lval_err("unbound symbol '%s'", k->sym);
}

void lenv_put(lenv* e, lval* k, lval* v) {
	
	/*iterate over all the items in environment
	this is to see if variable already exists*/
	for (int i = 0; i < e->count; i++) {
		
		/*if variable is found, delete item at that position
		and replace with variable supplied*/
		if (strcmp(e->syms[i], k->sym) == 0) {
			lval_del(e->vals[i]);
			e->vals[i] = lval_copy(v);
			return;
		}
	}
	
	/*if no existing entry found, allocate space for new entry*/
	e->count++;
	e->vals = realloc(e->vals, sizeof(lval*) * e->count);
	e->syms = realloc(e->syms, sizeof(lval*) * e->count);
	
	/*copy contents of lval and symbol string into new location*/
	e->vals[e->count - 1] = lval_copy(v);
	e->syms[e->count - 1] = malloc(strlen(k->sym) + 1);
	strcpy(e->syms[e->count - 1], k->sym);
}

/*********************************************************
lval functions for doing things
	LASSERT,
	builtin_head, builtin_tail, builtin_list, builtin_join, 
	builtin_op, builtin_eval, lval_join, builtin
*********************************************************/

/*forward declare lval_eval*/
lval* lval_eval(lenv* e, lval* v);

#define LASSERT(args, cond, fmt, ...)						\
			if (!(cond)) { 						 	  		\
				lval* err = lval_err(fmt, ##__VA_ARGS__); 	\
				lval_del(args); 						  	\
				return err; 							  	\
			}

#define LASSERT_TYPE(func, args, index, expect) 							\
			LASSERT(args, args->cell[index]->type == expect,				\
				"Function '%s' passed incorrect type for argument %i. " 	\
				"Got %i, Expected %i.", 									\
				func, index, 												\
				ltype_name(args->cell[index]->type), ltype_name(expect))
	
#define LASSERT_NUM(func, args, num)										\
			LASSERT(args, args->count == num,								\
				"Function '%s' passed incorrect number of arguments. " 		\
				"Got %i, Expected %i.",										\
				func, args->count, num)
		
#define LASSERT_NOT_EMPTY(func, args, index) 								\
			LASSERT(args, args->cell[index]->count != 0,					\
				"Function '%s' passed {} for argument %i."					\
				, func, index));
	

lval* builtin_head(lenv* e, lval* a) {
	
	LASSERT(a, a->count == 1, 
			"Function 'head' passed too many arguments! "
			"Got %i, Expected %i.",
			a->count, 1);
		
	LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
			"Function 'head' passed incorrect type for argument 0",
			"Got %s, Expected %s",
			ltype_name(a->cell[0]->type), ltype_name(LVAL_QEXPR));
		
	LASSERT(a , a->cell[0]->count != 0,
			"Function 'head' passed {}!",
			"Expected %i", 1);
	
	lval* v = lval_take(a, 0);
	
	while (v->count > 1) { 
		lval_del(lval_pop(v, 1)); 
	}
	
	return v;
}

lval* builtin_tail(lenv* e, lval* a) {
	LASSERT(a, a->count == 1, 
		"Function 'tail' passed too many arguments!");
	LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
		"Function 'tail' passed incorrect type");
	LASSERT(a , a->cell[0]->count != 0,
		"Function 'tail' passed {}!");
		
	lval* v = lval_take(a, 0);
	lval_del(lval_pop(v, 0));
	return v;
}

lval* builtin_list(lenv* e, lval* a) {
	a->type = LVAL_QEXPR;
	return a;
}

lval* builtin_eval(lenv* e, lval* a) {
	LASSERT(a, a->count == 1,
		"Function 'eval' passed too many arguments!");
	LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
		"Function 'eval' passed incorrect type");
	
	lval* x = lval_take(a, 0);
	x->type = LVAL_SEXPR;
	return lval_eval(e, x);
}

lval* lval_join(lval* x, lval* y) {
	
	/*for each cell in 'y' add it to 'x'*/
	while (y->count) {
		x = lval_add(x, lval_pop(y, 0));
	}
	
	/*deletet empty y and return x*/
	lval_del(y);
	return x;
}

lval* builtin_join(lenv* e, lval* a) {
	for (int i = 0; i < a->count; i++) {
		LASSERT(a, a->cell[i]->type == LVAL_QEXPR,
			"Function 'join' passed incorrect type!");
	}
	
	lval* x = lval_pop(a, 0);
	
	while (a->count) {
		x = lval_join(x, lval_pop(a, 0));
	}
	
	lval_del(a);
	return x;
}

/*builtin op*/
lval* builtin_op(lenv* e, lval* a, char* op) {
	
	/*ensure all arguments are numbers*/
	for (int i = 0; i < a->count; i++) {
		if (a->cell[i]->type != LVAL_NUM) {
			lval_del(a);
			return lval_err("Cannot operate on non-number!");
		}
	}
	
	/*pop the first element*/
	lval* x = lval_pop(a, 0);
	
	/*if no arguments and sub then perform unary negation*/
	if ((strcmp(op, "-") == 0) && a->count == 0) {
		x->num = -x->num;
	}
	
	/*while there are still elements remaining*/
	while (a->count > 0) {
		
		/*pop the next element*/
		lval* y = lval_pop(a, 0);
		if (strcmp(op, "+") == 0) { x->num += y->num; }
		if (strcmp(op, "-") == 0) { x->num -= y->num; }
		if (strcmp(op, "*") == 0) { x->num *= y->num; }
		if (strcmp(op, "/") == 0) {
			if (y->num == 0) {
				lval_del(x); lval_del(y);
				x = lval_err("Division By Zero!");
			}
			x->num /= y->num;
		}
		lval_del(y);
	}
	lval_del(a); 
	return(x);
}

lval* builtin_add(lenv* e, lval* a) {
	return builtin_op(e, a, "+");
}

lval* builtin_sub(lenv* e, lval* a) {
	return builtin_op(e, a, "-");
}

lval* builtin_mul(lenv* e, lval* a) {
	return builtin_op(e, a, "*");
}

lval* builtin_div(lenv* e, lval* a) {
	return builtin_op(e, a, "/");
}

lval* builtin_def(lenv* e, lval* a) {
	LASSERT(a, a->cell[0]->type == LVAL_QEXPR,
		"Function 'def' passed incorrect type!");
		
	/*first argument is symbol list*/
	lval* syms = a->cell[0];
	
	/*Ensure all elements of first list are symbols*/
	for (int i = 0; i < syms->count; i++) {
		LASSERT(a, syms->cell[i]->type == LVAL_SYM,
			"Function 'def' cannont define non-symbol");
	}
	
	/*Check correct number of symbols and values*/
	LASSERT(a, syms->count == a->count - 1,
		"Function 'def' cannont define incorrect "
		"number of values to symbols");
		
	/*Assign copies of values to symbols*/
	for (int i = 0; i < syms->count; i++) {
		lenv_put(e, syms->cell[i], a->cell[i+1]);
	}
	
	lval_del(a);
	return lval_sexpr();
}

lval* builtin_lambda(lenv* e, lval* a) {
	/*check two arguments, each of which are Q-expressions*/
	LASSERT_NUM("\\", a, 2);
	LASSERT_TYPE("\\", a, 0, LVAL_QEXPR);
	LASSERT_TYPE("\\", a, 1, LVAL_QEXPR);
	
	/*check first Q-expression contains only symbols*/
	for (int i = 0; i < a->cell[0]->count; i++) {
		LASSERT(a, (a->cell[0]->cell[i]->type == LVAL_SYM),
		"Cannot define non-symbol. Got %s, Expected %s.",
		ltype_name(a->cell[0]->cell[i]->type), ltype_name(LVAL_SYM));
	}
	
	/*pop first two arguments and pass them to lval_lambda*/
	lval* formals = lval_pop(a, 0);
	lval* body = lval_pop(a, 0);
	lval_del(a);
	
	return lval_lambda(formals, body);
}


void lenv_add_builtin(lenv* e, char* name, lbuiltin func) {
	lval* k = lval_sym(name);
	lval* v = lval_builtin(func);
	lenv_put(e, k, v);
	lval_del(k); lval_del(v);
}

void lenv_add_builtins(lenv* e) {
	/*Variable Functions*/
	lenv_add_builtin(e, "def", builtin_def);
	
	/*list functions*/
	lenv_add_builtin(e, "list", builtin_list);
	lenv_add_builtin(e, "head", builtin_head);
	lenv_add_builtin(e, "tail", builtin_tail);
	lenv_add_builtin(e, "eval", builtin_eval);
	lenv_add_builtin(e, "join", builtin_join);
	
	/*mathematical functions*/
	lenv_add_builtin(e, "+", builtin_add);
	lenv_add_builtin(e, "-", builtin_sub);
	lenv_add_builtin(e, "*", builtin_mul);
	lenv_add_builtin(e, "/", builtin_div);
}


/*********************************************************
	lval_eval_sexpr, lval_eval, lval_read_num, lval_read
**********************************************************/

/*lval_eval_sexpr*/
lval* lval_eval_sexpr(lenv* e, lval* v) {
	/*evaluate children*/
	for (int i = 0; i < v->count; i++) {
		v->cell[i] = lval_eval(e, v->cell[i]);
	}
	
	/*error checking*/
	for (int i = 0; i < v->count; i++) {
		if (v->cell[i]->type == LVAL_ERR) { return lval_take(v, i); }
	}
	
	/*empty expression*/
	if (v->count == 0) { return v; }
	
	/*single expression*/
	if (v->count == 1) {return lval_take(v, 0); }
	
	/*ensure first element is function after evaluation*/
	lval* f = lval_pop(v, 0);
	if (f->type != LVAL_FUN) {
		lval* err = lval_err(
			"S-expression starts with incorrect type. "
			"Got %s, Expected %s.",
			ltype_name(f->type), ltype_name(LVAL_FUN));
		lval_del(f); lval_del(v);
		return err;
	}
	
	/*call builtin with operator*/
	lval* result = f->builtin(e, v);
	lval_del(f);
	return result;
}

/*lval_eval*/
lval* lval_eval(lenv* e, lval* v) {
	
	if (v->type == LVAL_SYM) {
		lval* x = lenv_get(e, v);
		lval_del(v);
		return x;
	}
	if (v->type == LVAL_SEXPR) { return lval_eval_sexpr(e, v); }
	return v;
}

lval* lval_read_num(mpc_ast_t* t) {
	errno = 0;
	long x = strtol(t->contents, NULL, 10);
	return errno != ERANGE
		? lval_num(x)
		: lval_err("invalid number");
}

lval* lval_read(mpc_ast_t* t) {
	
	/*if symbol or number return conversion to that type*/
	if (strstr(t->tag, "number")) { return lval_read_num(t); }
	if (strstr(t->tag, "symbol")) { return lval_sym(t->contents); }
	
	/*if root (>) or sexpr then create empty list*/
	lval* x = NULL;
	if (strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
	if (strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }
	if (strstr(t->tag, "qexpr"))  { x = lval_qexpr(); }
	
	/*fill this list with any valid expression contained within*/
	for ( int i = 0; i < t->children_num; i++) {
		if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
		if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
		if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
		if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
		if (strcmp(t->children[i]->tag, "regex")  == 0) { continue; }
		x = lval_add(x, lval_read(t->children[i]));
	}
	return x;
}


int main(int argc, char** argv) {
    
    /*Create some parsers*/
    mpc_parser_t* Number    = mpc_new("number");
    mpc_parser_t* Symbol	= mpc_new("symbol");
    mpc_parser_t* Sexpr  	= mpc_new("sexpr");
    mpc_parser_t* Qexpr		= mpc_new("qexpr");
    mpc_parser_t* Expr      = mpc_new("expr");
    mpc_parser_t* Lispy     = mpc_new("lispy");

    /*Define them with the following language*/
    mpca_lang(MPCA_LANG_DEFAULT,                    
      "                                                       \
        number   : /-?[0-9]+/ ;                               \
        symbol   : /[a-zA-Z0-9_+\\-*\\/\\\\=<>!&]+/ ;         \
	    sexpr	 : '(' <expr>* ')' ;						  \
	    qexpr	 : '{' <expr>* '}' ;						  \
        expr     : <number> | <symbol> | <sexpr> | <qexpr>;   \
        lispy    : /^/ <expr>* /$/ ;		                  \
      ",
      Number, Symbol, Sexpr, Qexpr, Expr, Lispy);


    /*print version and exit info*/
    puts("Lispy Version 0.0.0.0.7");
    puts("Type exit to exit\n");
    
    lenv* e = lenv_new();
    lenv_add_builtins(e);
    int exitcode = 0;
    
    /*never ending loop*/
    while (exitcode == 0) {
        
        char* input = readline("lispy> ");
        if(strcmp(input, "exit") == 0) {
        	return exitcode = 1;
        }
        add_history(input);
        
        /*Attempt to parse user input*/
        mpc_result_t r;
        if (mpc_parse("<stdin>", input, Lispy, &r)) {
            lval* x = lval_eval(e, lval_read(r.output));
            lval_println(x);
            lval_del(x);
            mpc_ast_delete(r.output);
            
        } else {
            /*otherwise print the error*/
            mpc_err_print(r.error);
            mpc_err_delete(r.error);
        }
         
        free(input);
    }
    
    lenv_del(e);

    /*Undefine and Delete our Parsers*/
    mpc_cleanup(5, Number, Symbol, Sexpr, Qexpr, Expr, Lispy);

    return 0;
}