/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "3.0.4"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1




/* Copy the first part of user declarations.  */
#line 1 "filter-parser.y" /* yacc.c:339  */

/*
 * filter-parser.y
 *
 * LTTng filter expression parser
 *
 * Copyright 2012 Mathieu Desnoyers <mathieu.desnoyers@efficios.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-only
 *
 * Grammar inspired from http://www.quut.com/c/ANSI-C-grammar-y.html
 */

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <inttypes.h>
#include "common/bytecode/bytecode.h"
#include "filter-ast.h"
#include "filter-parser.h"
#include "memstream.h"

#include <common/compat/errno.h>
#include <common/macros.h>

#define WIDTH_u64_SCANF_IS_A_BROKEN_API	"20"
#define WIDTH_o64_SCANF_IS_A_BROKEN_API	"22"
#define WIDTH_x64_SCANF_IS_A_BROKEN_API	"17"
#define WIDTH_lg_SCANF_IS_A_BROKEN_API	"4096"	/* Hugely optimistic approximation */

#ifdef DEBUG
static const int print_xml = 1;
#define dbg_printf(fmt, args...)	\
	printf("[debug filter_parser] " fmt, ## args)
#else
static const int print_xml = 0;
#define dbg_printf(fmt, args...)				\
do {								\
	/* do nothing but check printf format */		\
	if (0)							\
		printf("[debug filter_parser] " fmt, ## args);	\
} while (0)
#endif

LTTNG_HIDDEN
int yydebug;
LTTNG_HIDDEN
int filter_parser_debug = 0;

LTTNG_HIDDEN
int yyparse(struct filter_parser_ctx *parser_ctx, yyscan_t scanner);
LTTNG_HIDDEN
int yylex(union YYSTYPE *yyval, yyscan_t scanner);
LTTNG_HIDDEN
int yylex_init_extra(struct filter_parser_ctx *parser_ctx, yyscan_t * ptr_yy_globals);
LTTNG_HIDDEN
int yylex_destroy(yyscan_t yyparser_ctx);
LTTNG_HIDDEN
void yyrestart(FILE * in_str, yyscan_t parser_ctx);

struct gc_string {
	struct cds_list_head gc;
	size_t alloclen;
	char s[];
};

static const char *node_type_to_str[] = {
	[ NODE_UNKNOWN ] = "NODE_UNKNOWN",
	[ NODE_ROOT ] = "NODE_ROOT",
	[ NODE_EXPRESSION ] = "NODE_EXPRESSION",
	[ NODE_OP ] = "NODE_OP",
	[ NODE_UNARY_OP ] = "NODE_UNARY_OP",
};

LTTNG_HIDDEN
const char *node_type(struct filter_node *node)
{
	if (node->type < NR_NODE_TYPES)
		return node_type_to_str[node->type];
	else
		return NULL;
}

static struct gc_string *gc_string_alloc(struct filter_parser_ctx *parser_ctx,
					 size_t len)
{
	struct gc_string *gstr;
	size_t alloclen;

	/* TODO: could be faster with find first bit or glib Gstring */
	/* sizeof long to account for malloc header (int or long ?) */
	for (alloclen = 8; alloclen < sizeof(long) + sizeof(*gstr) + len;
	     alloclen *= 2);

	gstr = zmalloc(alloclen);
	if (!gstr) {
		goto end;
	}
	cds_list_add(&gstr->gc, &parser_ctx->allocated_strings);
	gstr->alloclen = alloclen;
end:
	return gstr;
}

/*
 * note: never use gc_string_append on a string that has external references.
 * gsrc will be garbage collected immediately, and gstr might be.
 * Should only be used to append characters to a string literal or constant.
 */
static
struct gc_string *gc_string_append(struct filter_parser_ctx *parser_ctx,
				   struct gc_string *gstr,
				   struct gc_string *gsrc)
{
	size_t newlen = strlen(gsrc->s) + strlen(gstr->s) + 1;
	size_t alloclen;

	/* TODO: could be faster with find first bit or glib Gstring */
	/* sizeof long to account for malloc header (int or long ?) */
	for (alloclen = 8; alloclen < sizeof(long) + sizeof(*gstr) + newlen;
	     alloclen *= 2);

	if (alloclen > gstr->alloclen) {
		struct gc_string *newgstr;

		newgstr = gc_string_alloc(parser_ctx, newlen);
		strcpy(newgstr->s, gstr->s);
		strcat(newgstr->s, gsrc->s);
		cds_list_del(&gstr->gc);
		free(gstr);
		gstr = newgstr;
	} else {
		strcat(gstr->s, gsrc->s);
	}
	cds_list_del(&gsrc->gc);
	free(gsrc);
	return gstr;
}

LTTNG_HIDDEN
void setstring(struct filter_parser_ctx *parser_ctx, YYSTYPE *lvalp, const char *src)
{
	lvalp->gs = gc_string_alloc(parser_ctx, strlen(src) + 1);
	strcpy(lvalp->gs->s, src);
}

static struct filter_node *make_node(struct filter_parser_ctx *scanner,
				  enum node_type type)
{
	struct filter_ast *ast = filter_parser_get_ast(scanner);
	struct filter_node *node;

	node = zmalloc(sizeof(*node));
	if (!node)
		return NULL;
	memset(node, 0, sizeof(*node));
	node->type = type;
	cds_list_add(&node->gc, &ast->allocated_nodes);

	switch (type) {
	case NODE_ROOT:
		fprintf(stderr, "[error] %s: trying to create root node\n", __func__);
		break;

	case NODE_EXPRESSION:
		break;
	case NODE_OP:
		break;
	case NODE_UNARY_OP:
		break;

	case NODE_UNKNOWN:
	default:
		fprintf(stderr, "[error] %s: unknown node type %d\n", __func__,
			(int) type);
		break;
	}

	return node;
}

static struct filter_node *make_op_node(struct filter_parser_ctx *scanner,
			enum op_type type,
			struct filter_node *lchild,
			struct filter_node *rchild)
{
	struct filter_ast *ast = filter_parser_get_ast(scanner);
	struct filter_node *node;

	node = zmalloc(sizeof(*node));
	if (!node)
		return NULL;
	memset(node, 0, sizeof(*node));
	node->type = NODE_OP;
	cds_list_add(&node->gc, &ast->allocated_nodes);
	node->u.op.type = type;
	node->u.op.lchild = lchild;
	node->u.op.rchild = rchild;
	return node;
}

static
void yyerror(struct filter_parser_ctx *parser_ctx, yyscan_t scanner, const char *str)
{
	fprintf(stderr, "error %s\n", str);
}

#define parse_error(parser_ctx, str)				\
do {								\
	yyerror(parser_ctx, parser_ctx->scanner, YY_("parse error: " str "\n"));	\
	YYERROR;						\
} while (0)

static void free_strings(struct cds_list_head *list)
{
	struct gc_string *gstr, *tmp;

	cds_list_for_each_entry_safe(gstr, tmp, list, gc)
		free(gstr);
}

static struct filter_ast *filter_ast_alloc(void)
{
	struct filter_ast *ast;

	ast = zmalloc(sizeof(*ast));
	if (!ast)
		return NULL;
	memset(ast, 0, sizeof(*ast));
	CDS_INIT_LIST_HEAD(&ast->allocated_nodes);
	ast->root.type = NODE_ROOT;
	return ast;
}

static void filter_ast_free(struct filter_ast *ast)
{
	struct filter_node *node, *tmp;

	cds_list_for_each_entry_safe(node, tmp, &ast->allocated_nodes, gc)
		free(node);
	free(ast);
}

LTTNG_HIDDEN
int filter_parser_ctx_append_ast(struct filter_parser_ctx *parser_ctx)
{
	return yyparse(parser_ctx, parser_ctx->scanner);
}

LTTNG_HIDDEN
struct filter_parser_ctx *filter_parser_ctx_alloc(FILE *input)
{
	struct filter_parser_ctx *parser_ctx;
	int ret;

	yydebug = filter_parser_debug;

	parser_ctx = zmalloc(sizeof(*parser_ctx));
	if (!parser_ctx)
		return NULL;
	memset(parser_ctx, 0, sizeof(*parser_ctx));

	ret = yylex_init_extra(parser_ctx, &parser_ctx->scanner);
	if (ret) {
		fprintf(stderr, "yylex_init error\n");
		goto cleanup_parser_ctx;
	}
	/* Start processing new stream */
	yyrestart(input, parser_ctx->scanner);

	parser_ctx->ast = filter_ast_alloc();
	if (!parser_ctx->ast)
		goto cleanup_lexer;
	CDS_INIT_LIST_HEAD(&parser_ctx->allocated_strings);

	if (yydebug)
		fprintf(stdout, "parser_ctx input is a%s.\n",
			isatty(fileno(input)) ? "n interactive tty" :
						" noninteractive file");

	return parser_ctx;

cleanup_lexer:
	ret = yylex_destroy(parser_ctx->scanner);
	if (!ret)
		fprintf(stderr, "yylex_destroy error\n");
cleanup_parser_ctx:
	free(parser_ctx);
	return NULL;
}

LTTNG_HIDDEN
void filter_parser_ctx_free(struct filter_parser_ctx *parser_ctx)
{
	int ret;

	ret = yylex_destroy(parser_ctx->scanner);
	if (ret)
		fprintf(stderr, "yylex_destroy error\n");

	filter_ast_free(parser_ctx->ast);
	free_strings(&parser_ctx->allocated_strings);
	filter_ir_free(parser_ctx);
	free(parser_ctx->bytecode);
	free(parser_ctx->bytecode_reloc);

	free(parser_ctx);
}

LTTNG_HIDDEN
int filter_parser_ctx_create_from_filter_expression(
		const char *filter_expression, struct filter_parser_ctx **ctxp)
{
	int ret;
	struct filter_parser_ctx *ctx = NULL;
	FILE *fmem = NULL;

	assert(filter_expression);
	assert(ctxp);

	/*
	 * Casting const to non-const, as the underlying function will use it in
	 * read-only mode.
	 */
	fmem = lttng_fmemopen((void *) filter_expression,
			strlen(filter_expression), "r");
	if (!fmem) {
		fprintf(stderr, "Error opening memory as stream\n");
		ret = -LTTNG_ERR_FILTER_NOMEM;
		goto error;
	}
	ctx = filter_parser_ctx_alloc(fmem);
	if (!ctx) {
		fprintf(stderr, "Error allocating parser\n");
		ret = -LTTNG_ERR_FILTER_NOMEM;
		goto filter_alloc_error;
	}
	ret = filter_parser_ctx_append_ast(ctx);
	if (ret) {
		fprintf(stderr, "Parse error\n");
		ret = -LTTNG_ERR_FILTER_INVAL;
		goto parse_error;
	}
	if (print_xml) {
		ret = filter_visitor_print_xml(ctx, stdout, 0);
		if (ret) {
			fflush(stdout);
			fprintf(stderr, "XML print error\n");
			ret = -LTTNG_ERR_FILTER_INVAL;
			goto parse_error;
		}
	}

	dbg_printf("Generating IR... ");
	fflush(stdout);
	ret = filter_visitor_ir_generate(ctx);
	if (ret) {
		fprintf(stderr, "Generate IR error\n");
		ret = -LTTNG_ERR_FILTER_INVAL;
		goto parse_error;
	}
	dbg_printf("done\n");

	dbg_printf("Validating IR... ");
	fflush(stdout);
	ret = filter_visitor_ir_check_binary_op_nesting(ctx);
	if (ret) {
		ret = -LTTNG_ERR_FILTER_INVAL;
		goto parse_error;
	}

	/* Normalize globbing patterns in the expression. */
	ret = filter_visitor_ir_normalize_glob_patterns(ctx);
	if (ret) {
		ret = -LTTNG_ERR_FILTER_INVAL;
		goto parse_error;
	}

	/* Validate strings used as literals in the expression. */
	ret = filter_visitor_ir_validate_string(ctx);
	if (ret) {
		ret = -LTTNG_ERR_FILTER_INVAL;
		goto parse_error;
	}

	/* Validate globbing patterns in the expression. */
	ret = filter_visitor_ir_validate_globbing(ctx);
	if (ret) {
		ret = -LTTNG_ERR_FILTER_INVAL;
		goto parse_error;
	}

	dbg_printf("done\n");

	dbg_printf("Generating bytecode... ");
	fflush(stdout);
	ret = filter_visitor_bytecode_generate(ctx);
	if (ret) {
		fprintf(stderr, "Generate bytecode error\n");
		ret = -LTTNG_ERR_FILTER_INVAL;
		goto parse_error;
	}
	dbg_printf("done\n");
	dbg_printf("Size of bytecode generated: %u bytes.\n",
			bytecode_get_len(&ctx->bytecode->b));

	/* No need to keep the memory stream. */
	if (fclose(fmem) != 0) {
		fprintf(stderr, "fclose (%d) \n", errno);
		ret = -LTTNG_ERR_FILTER_INVAL;
	}

	*ctxp = ctx;
	return 0;

parse_error:
	filter_ir_free(ctx);
	filter_parser_ctx_free(ctx);
filter_alloc_error:
	if (fclose(fmem) != 0) {
		fprintf(stderr, "fclose (%d) \n", errno);
	}
error:
	return ret;
}


#line 496 "filter-parser.c" /* yacc.c:339  */

# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 0
#endif

/* In a future release of Bison, this section will be replaced
   by #include "y.tab.h".  */
#ifndef YY_YY_FILTER_PARSER_H_INCLUDED
# define YY_YY_FILTER_PARSER_H_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 1
#endif
#if YYDEBUG
extern int yydebug;
#endif

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    CHARACTER_CONSTANT_START = 258,
    SQUOTE = 259,
    STRING_LITERAL_START = 260,
    DQUOTE = 261,
    ESCSEQ = 262,
    CHAR_STRING_TOKEN = 263,
    DECIMAL_CONSTANT = 264,
    OCTAL_CONSTANT = 265,
    HEXADECIMAL_CONSTANT = 266,
    FLOAT_CONSTANT = 267,
    LSBRAC = 268,
    RSBRAC = 269,
    LPAREN = 270,
    RPAREN = 271,
    LBRAC = 272,
    RBRAC = 273,
    RARROW = 274,
    STAR = 275,
    PLUS = 276,
    MINUS = 277,
    MOD_OP = 278,
    DIV_OP = 279,
    RIGHT_OP = 280,
    LEFT_OP = 281,
    EQ_OP = 282,
    NE_OP = 283,
    LE_OP = 284,
    GE_OP = 285,
    LT_OP = 286,
    GT_OP = 287,
    AND_OP = 288,
    OR_OP = 289,
    NOT_OP = 290,
    ASSIGN = 291,
    COLON = 292,
    SEMICOLON = 293,
    DOTDOTDOT = 294,
    DOT = 295,
    EQUAL = 296,
    COMMA = 297,
    XOR_BIN = 298,
    AND_BIN = 299,
    OR_BIN = 300,
    NOT_BIN = 301,
    IDENTIFIER = 302,
    GLOBAL_IDENTIFIER = 303,
    ERROR = 304
  };
#endif
/* Tokens.  */
#define CHARACTER_CONSTANT_START 258
#define SQUOTE 259
#define STRING_LITERAL_START 260
#define DQUOTE 261
#define ESCSEQ 262
#define CHAR_STRING_TOKEN 263
#define DECIMAL_CONSTANT 264
#define OCTAL_CONSTANT 265
#define HEXADECIMAL_CONSTANT 266
#define FLOAT_CONSTANT 267
#define LSBRAC 268
#define RSBRAC 269
#define LPAREN 270
#define RPAREN 271
#define LBRAC 272
#define RBRAC 273
#define RARROW 274
#define STAR 275
#define PLUS 276
#define MINUS 277
#define MOD_OP 278
#define DIV_OP 279
#define RIGHT_OP 280
#define LEFT_OP 281
#define EQ_OP 282
#define NE_OP 283
#define LE_OP 284
#define GE_OP 285
#define LT_OP 286
#define GT_OP 287
#define AND_OP 288
#define OR_OP 289
#define NOT_OP 290
#define ASSIGN 291
#define COLON 292
#define SEMICOLON 293
#define DOTDOTDOT 294
#define DOT 295
#define EQUAL 296
#define COMMA 297
#define XOR_BIN 298
#define AND_BIN 299
#define OR_BIN 300
#define NOT_BIN 301
#define IDENTIFIER 302
#define GLOBAL_IDENTIFIER 303
#define ERROR 304

/* Value type.  */
#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED

union YYSTYPE
{
#line 458 "filter-parser.y" /* yacc.c:355  */

	long long ll;
	char c;
	struct gc_string *gs;
	struct filter_node *n;

#line 641 "filter-parser.c" /* yacc.c:355  */
};

typedef union YYSTYPE YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define YYSTYPE_IS_DECLARED 1
#endif



int yyparse (struct filter_parser_ctx *parser_ctx, yyscan_t scanner);
/* "%code provides" blocks.  */
#line 432 "filter-parser.y" /* yacc.c:355  */

#include "common/macros.h"

LTTNG_HIDDEN
void setstring(struct filter_parser_ctx *parser_ctx, YYSTYPE *lvalp, const char *src);

#line 660 "filter-parser.c" /* yacc.c:355  */

#endif /* !YY_YY_FILTER_PARSER_H_INCLUDED  */

/* Copy the second part of user declarations.  */

#line 666 "filter-parser.c" /* yacc.c:358  */

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef YY_ATTRIBUTE
# if (defined __GNUC__                                               \
      && (2 < __GNUC__ || (__GNUC__ == 2 && 96 <= __GNUC_MINOR__)))  \
     || defined __SUNPRO_C && 0x5110 <= __SUNPRO_C
#  define YY_ATTRIBUTE(Spec) __attribute__(Spec)
# else
#  define YY_ATTRIBUTE(Spec) /* empty */
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# define YY_ATTRIBUTE_PURE   YY_ATTRIBUTE ((__pure__))
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# define YY_ATTRIBUTE_UNUSED YY_ATTRIBUTE ((__unused__))
#endif

#if !defined _Noreturn \
     && (!defined __STDC_VERSION__ || __STDC_VERSION__ < 201112)
# if defined _MSC_VER && 1200 <= _MSC_VER
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn YY_ATTRIBUTE ((__noreturn__))
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif


#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
         || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYSIZE_T yynewbytes;                                            \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / sizeof (*yyptr);                          \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  65
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   77

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  50
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  24
/* YYNRULES -- Number of rules.  */
#define YYNRULES  63
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  94

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   304

#define YYTRANSLATE(YYX)                                                \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47,    48,    49
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   492,   492,   494,   499,   501,   510,   512,   517,   519,
     526,   535,   546,   555,   564,   570,   576,   582,   591,   597,
     606,   610,   619,   623,   632,   636,   642,   651,   653,   655,
     663,   668,   673,   678,   686,   688,   692,   696,   703,   705,
     709,   716,   718,   722,   729,   731,   738,   740,   747,   749,
     756,   758,   762,   766,   770,   777,   779,   783,   790,   792,
     799,   801,   808,   813
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 0
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "CHARACTER_CONSTANT_START", "SQUOTE",
  "STRING_LITERAL_START", "DQUOTE", "ESCSEQ", "CHAR_STRING_TOKEN",
  "DECIMAL_CONSTANT", "OCTAL_CONSTANT", "HEXADECIMAL_CONSTANT",
  "FLOAT_CONSTANT", "LSBRAC", "RSBRAC", "LPAREN", "RPAREN", "LBRAC",
  "RBRAC", "RARROW", "STAR", "PLUS", "MINUS", "MOD_OP", "DIV_OP",
  "RIGHT_OP", "LEFT_OP", "EQ_OP", "NE_OP", "LE_OP", "GE_OP", "LT_OP",
  "GT_OP", "AND_OP", "OR_OP", "NOT_OP", "ASSIGN", "COLON", "SEMICOLON",
  "DOTDOTDOT", "DOT", "EQUAL", "COMMA", "XOR_BIN", "AND_BIN", "OR_BIN",
  "NOT_BIN", "IDENTIFIER", "GLOBAL_IDENTIFIER", "ERROR", "$accept",
  "c_char_sequence", "c_char", "s_char_sequence", "s_char",
  "primary_expression", "identifiers", "prefix_expression_rec",
  "prefix_expression", "postfix_expression", "unary_expression",
  "unary_operator", "multiplicative_expression", "additive_expression",
  "shift_expression", "and_expression", "exclusive_or_expression",
  "inclusive_or_expression", "relational_expression",
  "equality_expression", "logical_and_expression", "logical_or_expression",
  "expression", "translation_unit", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,   303,   304
};
# endif

#define YYPACT_NINF -42

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-42)))

#define YYTABLE_NINF -1

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int8 yypact[] =
{
      -3,     8,    20,   -42,   -42,   -42,   -42,    -3,   -42,   -42,
     -42,   -42,   -42,   -42,   -42,    -8,   -42,   -15,   -42,    -3,
     -10,     1,    16,   -41,   -32,    18,     4,    22,    25,    17,
     -42,    64,   -42,   -42,    13,   -42,   -42,   -42,   -42,    40,
     -42,    49,    -3,   -42,     5,     5,   -42,    -3,    -3,    -3,
      -3,    -3,    -3,    -3,    -3,    -3,    -3,    -3,    -3,    -3,
      -3,    -3,    -3,    -3,    -3,   -42,   -42,   -42,   -42,   -42,
     -42,    52,   -42,   -42,   -42,   -42,   -42,   -10,   -10,     1,
       1,    16,   -41,   -32,    18,    18,    18,    18,     4,     4,
      22,    25,    -8,   -42
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       0,     0,     0,    10,    11,    12,    13,     0,    30,    31,
      32,    33,    18,    19,    28,    22,    24,    27,    34,     0,
      38,    41,    44,    46,    48,    50,    55,    58,    60,    62,
      63,     0,     5,     4,     0,     2,    14,     9,     8,     0,
       6,     0,     0,    23,     0,     0,    29,     0,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,     1,    16,     3,    15,     7,
      17,     0,    26,    25,    35,    37,    36,    39,    40,    43,
      42,    45,    47,    49,    53,    54,    51,    52,    56,    57,
      59,    61,    20,    21
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int8 yypgoto[] =
{
     -42,   -42,    33,   -42,    29,   -42,   -42,   -23,    10,   -42,
     -18,   -42,     6,     7,    19,    15,    21,   -20,     0,     9,
      11,   -42,    67,   -42
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int8 yydefgoto[] =
{
      -1,    34,    35,    39,    40,    14,    15,    43,    16,    17,
      18,    19,    20,    21,    22,    23,    24,    25,    26,    27,
      28,    29,    30,    31
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_uint8 yytable[] =
{
       1,    46,     2,    54,    44,    42,     3,     4,     5,     6,
      47,    55,     7,    48,    49,    32,    33,    66,     8,     9,
      32,    33,    50,    51,    71,    45,    36,    37,    38,    74,
      75,    76,    10,    57,    58,    59,    60,    84,    85,    86,
      87,    52,    53,    11,    12,    13,    68,    37,    38,    61,
      62,    64,    12,    13,    72,    73,    77,    78,    63,    79,
      80,    88,    89,    56,    65,    70,    92,    67,    69,    93,
      82,     0,    90,    81,    41,    91,     0,    83
};

static const yytype_int8 yycheck[] =
{
       3,    19,     5,    44,    19,    13,     9,    10,    11,    12,
      20,    43,    15,    23,    24,     7,     8,     4,    21,    22,
       7,     8,    21,    22,    42,    40,     6,     7,     8,    47,
      48,    49,    35,    29,    30,    31,    32,    57,    58,    59,
      60,    25,    26,    46,    47,    48,     6,     7,     8,    27,
      28,    34,    47,    48,    44,    45,    50,    51,    33,    52,
      53,    61,    62,    45,     0,    16,    14,    34,    39,    92,
      55,    -1,    63,    54,     7,    64,    -1,    56
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,     3,     5,     9,    10,    11,    12,    15,    21,    22,
      35,    46,    47,    48,    55,    56,    58,    59,    60,    61,
      62,    63,    64,    65,    66,    67,    68,    69,    70,    71,
      72,    73,     7,     8,    51,    52,     6,     7,     8,    53,
      54,    72,    13,    57,    19,    40,    60,    20,    23,    24,
      21,    22,    25,    26,    44,    43,    45,    29,    30,    31,
      32,    27,    28,    33,    34,     0,     4,    52,     6,    54,
      16,    60,    58,    58,    60,    60,    60,    62,    62,    63,
      63,    64,    65,    66,    67,    67,    67,    67,    68,    68,
      69,    70,    14,    57
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    50,    51,    51,    52,    52,    53,    53,    54,    54,
      55,    55,    55,    55,    55,    55,    55,    55,    56,    56,
      57,    57,    58,    58,    59,    59,    59,    60,    60,    60,
      61,    61,    61,    61,    62,    62,    62,    62,    63,    63,
      63,    64,    64,    64,    65,    65,    66,    66,    67,    67,
      68,    68,    68,    68,    68,    69,    69,    69,    70,    70,
      71,    71,    72,    73
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     1,     2,     1,     1,     1,     2,     1,     1,
       1,     1,     1,     1,     2,     3,     3,     3,     1,     1,
       3,     4,     1,     2,     1,     3,     3,     1,     1,     2,
       1,     1,     1,     1,     1,     3,     3,     3,     1,     3,
       3,     1,     3,     3,     1,     3,     1,     3,     1,     3,
       1,     3,     3,     3,     3,     1,     3,     3,     1,     3,
       1,     3,     1,     1
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
do                                                              \
  if (yychar == YYEMPTY)                                        \
    {                                                           \
      yychar = (Token);                                         \
      yylval = (Value);                                         \
      YYPOPSTACK (yylen);                                       \
      yystate = *yyssp;                                         \
      goto yybackup;                                            \
    }                                                           \
  else                                                          \
    {                                                           \
      yyerror (parser_ctx, scanner, YY_("syntax error: cannot back up")); \
      YYERROR;                                                  \
    }                                                           \
while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256



/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value, parser_ctx, scanner); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, struct filter_parser_ctx *parser_ctx, yyscan_t scanner)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  YYUSE (parser_ctx);
  YYUSE (scanner);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, struct filter_parser_ctx *parser_ctx, yyscan_t scanner)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep, parser_ctx, scanner);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, int yyrule, struct filter_parser_ctx *parser_ctx, yyscan_t scanner)
{
  unsigned long int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[yyssp[yyi + 1 - yynrhs]],
                       &(yyvsp[(yyi + 1) - (yynrhs)])
                                              , parser_ctx, scanner);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule, parser_ctx, scanner); \
} while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif


#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
static YYSIZE_T
yystrlen (const char *yystr)
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char *
yystpcpy (char *yydest, const char *yysrc)
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
        switch (*++yyp)
          {
          case '\'':
          case ',':
            goto do_not_strip_quotes;

          case '\\':
            if (*++yyp != '\\')
              goto do_not_strip_quotes;
            /* Fall through.  */
          default:
            if (yyres)
              yyres[yyn] = *yyp;
            yyn++;
            break;

          case '"':
            if (yyres)
              yyres[yyn] = '\0';
            return yyn;
          }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, struct filter_parser_ctx *parser_ctx, yyscan_t scanner)
{
  YYUSE (yyvaluep);
  YYUSE (parser_ctx);
  YYUSE (scanner);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/*----------.
| yyparse.  |
`----------*/

int
yyparse (struct filter_parser_ctx *parser_ctx, yyscan_t scanner)
{
/* The lookahead symbol.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

    /* Number of syntax errors so far.  */
    int yynerrs;

    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
        /* Give user a chance to reallocate the stack.  Use copies of
           these so that the &'s don't force the real ones into
           memory.  */
        YYSTYPE *yyvs1 = yyvs;
        yytype_int16 *yyss1 = yyss;

        /* Each stack pointer address is followed by the size of the
           data in use in that stack, in bytes.  This used to be a
           conditional around just the two extra args, but that might
           be undefined if yyoverflow is a macro.  */
        yyoverflow (YY_("memory exhausted"),
                    &yyss1, yysize * sizeof (*yyssp),
                    &yyvs1, yysize * sizeof (*yyvsp),
                    &yystacksize);

        yyss = yyss1;
        yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
        goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
        yystacksize = YYMAXDEPTH;

      {
        yytype_int16 *yyss1 = yyss;
        union yyalloc *yyptr =
          (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
        if (! yyptr)
          goto yyexhaustedlab;
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
#  undef YYSTACK_RELOCATE
        if (yyss1 != yyssa)
          YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;

      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
                  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
        YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex (&yylval, scanner);
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 2:
#line 493 "filter-parser.y" /* yacc.c:1646  */
    {	(yyval.gs) = (yyvsp[0].gs);					}
#line 1826 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 3:
#line 495 "filter-parser.y" /* yacc.c:1646  */
    {	(yyval.gs) = gc_string_append(parser_ctx, (yyvsp[-1].gs), (yyvsp[0].gs));		}
#line 1832 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 4:
#line 500 "filter-parser.y" /* yacc.c:1646  */
    {	(yyval.gs) = yylval.gs;					}
#line 1838 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 5:
#line 502 "filter-parser.y" /* yacc.c:1646  */
    {
			parse_error(parser_ctx, "escape sequences not supported yet");
		}
#line 1846 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 6:
#line 511 "filter-parser.y" /* yacc.c:1646  */
    {	(yyval.gs) = (yyvsp[0].gs);					}
#line 1852 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 7:
#line 513 "filter-parser.y" /* yacc.c:1646  */
    {	(yyval.gs) = gc_string_append(parser_ctx, (yyvsp[-1].gs), (yyvsp[0].gs));		}
#line 1858 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 8:
#line 518 "filter-parser.y" /* yacc.c:1646  */
    {	(yyval.gs) = yylval.gs;					}
#line 1864 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 9:
#line 520 "filter-parser.y" /* yacc.c:1646  */
    {
			parse_error(parser_ctx, "escape sequences not supported yet");
		}
#line 1872 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 10:
#line 527 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_node(parser_ctx, NODE_EXPRESSION);
			(yyval.n)->u.expression.type = AST_EXP_CONSTANT;
			if (sscanf(yylval.gs->s, "%" WIDTH_u64_SCANF_IS_A_BROKEN_API SCNu64,
					&(yyval.n)->u.expression.u.constant) != 1) {
				parse_error(parser_ctx, "cannot scanf decimal constant");
			}
		}
#line 1885 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 11:
#line 536 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_node(parser_ctx, NODE_EXPRESSION);
			(yyval.n)->u.expression.type = AST_EXP_CONSTANT;
			if (!strcmp(yylval.gs->s, "0")) {
				(yyval.n)->u.expression.u.constant = 0;
			} else if (sscanf(yylval.gs->s, "0%" WIDTH_o64_SCANF_IS_A_BROKEN_API SCNo64,
					&(yyval.n)->u.expression.u.constant) != 1) {
				parse_error(parser_ctx, "cannot scanf octal constant");
			}
		}
#line 1900 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 12:
#line 547 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_node(parser_ctx, NODE_EXPRESSION);
			(yyval.n)->u.expression.type = AST_EXP_CONSTANT;
			if (sscanf(yylval.gs->s, "0x%" WIDTH_x64_SCANF_IS_A_BROKEN_API SCNx64,
					&(yyval.n)->u.expression.u.constant) != 1) {
				parse_error(parser_ctx, "cannot scanf hexadecimal constant");
			}
		}
#line 1913 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 13:
#line 556 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_node(parser_ctx, NODE_EXPRESSION);
			(yyval.n)->u.expression.type = AST_EXP_FLOAT_CONSTANT;
			if (sscanf(yylval.gs->s, "%" WIDTH_lg_SCANF_IS_A_BROKEN_API "lg",
					&(yyval.n)->u.expression.u.float_constant) != 1) {
				parse_error(parser_ctx, "cannot scanf float constant");
			}
		}
#line 1926 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 14:
#line 565 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_node(parser_ctx, NODE_EXPRESSION);
			(yyval.n)->u.expression.type = AST_EXP_STRING;
			(yyval.n)->u.expression.u.string = "";
		}
#line 1936 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 15:
#line 571 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_node(parser_ctx, NODE_EXPRESSION);
			(yyval.n)->u.expression.type = AST_EXP_STRING;
			(yyval.n)->u.expression.u.string = (yyvsp[-1].gs)->s;
		}
#line 1946 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 16:
#line 577 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_node(parser_ctx, NODE_EXPRESSION);
			(yyval.n)->u.expression.type = AST_EXP_STRING;
			(yyval.n)->u.expression.u.string = (yyvsp[-1].gs)->s;
		}
#line 1956 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 17:
#line 583 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_node(parser_ctx, NODE_EXPRESSION);
			(yyval.n)->u.expression.type = AST_EXP_NESTED;
			(yyval.n)->u.expression.u.child = (yyvsp[-1].n);
		}
#line 1966 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 18:
#line 592 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_node(parser_ctx, NODE_EXPRESSION);
			(yyval.n)->u.expression.type = AST_EXP_IDENTIFIER;
			(yyval.n)->u.expression.u.identifier = yylval.gs->s;
		}
#line 1976 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 19:
#line 598 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_node(parser_ctx, NODE_EXPRESSION);
			(yyval.n)->u.expression.type = AST_EXP_GLOBAL_IDENTIFIER;
			(yyval.n)->u.expression.u.identifier = yylval.gs->s;
		}
#line 1986 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 20:
#line 607 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = (yyvsp[-1].n);
		}
#line 1994 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 21:
#line 611 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = (yyvsp[-2].n);
			(yyval.n)->u.expression.pre_op = AST_LINK_BRACKET;
			(yyval.n)->u.expression.prev = (yyvsp[0].n);
		}
#line 2004 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 22:
#line 620 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = (yyvsp[0].n);
		}
#line 2012 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 23:
#line 624 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = (yyvsp[-1].n);
			(yyval.n)->u.expression.pre_op = AST_LINK_BRACKET;
			(yyval.n)->u.expression.next_bracket = (yyvsp[0].n);
		}
#line 2022 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 24:
#line 633 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = (yyvsp[0].n);
		}
#line 2030 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 25:
#line 637 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = (yyvsp[0].n);
			(yyval.n)->u.expression.post_op = AST_LINK_DOT;
			(yyval.n)->u.expression.prev = (yyvsp[-2].n);
		}
#line 2040 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 26:
#line 643 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = (yyvsp[0].n);
			(yyval.n)->u.expression.post_op = AST_LINK_RARROW;
			(yyval.n)->u.expression.prev = (yyvsp[-2].n);
		}
#line 2050 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 27:
#line 652 "filter-parser.y" /* yacc.c:1646  */
    {	(yyval.n) = (yyvsp[0].n);					}
#line 2056 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 28:
#line 654 "filter-parser.y" /* yacc.c:1646  */
    {	(yyval.n) = (yyvsp[0].n);					}
#line 2062 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 29:
#line 656 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = (yyvsp[-1].n);
			(yyval.n)->u.unary_op.child = (yyvsp[0].n);
		}
#line 2071 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 30:
#line 664 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_node(parser_ctx, NODE_UNARY_OP);
			(yyval.n)->u.unary_op.type = AST_UNARY_PLUS;
		}
#line 2080 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 31:
#line 669 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_node(parser_ctx, NODE_UNARY_OP);
			(yyval.n)->u.unary_op.type = AST_UNARY_MINUS;
		}
#line 2089 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 32:
#line 674 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_node(parser_ctx, NODE_UNARY_OP);
			(yyval.n)->u.unary_op.type = AST_UNARY_NOT;
		}
#line 2098 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 33:
#line 679 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_node(parser_ctx, NODE_UNARY_OP);
			(yyval.n)->u.unary_op.type = AST_UNARY_BIT_NOT;
		}
#line 2107 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 34:
#line 687 "filter-parser.y" /* yacc.c:1646  */
    {	(yyval.n) = (yyvsp[0].n);					}
#line 2113 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 35:
#line 689 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_op_node(parser_ctx, AST_OP_MUL, (yyvsp[-2].n), (yyvsp[0].n));
		}
#line 2121 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 36:
#line 693 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_op_node(parser_ctx, AST_OP_DIV, (yyvsp[-2].n), (yyvsp[0].n));
		}
#line 2129 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 37:
#line 697 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_op_node(parser_ctx, AST_OP_MOD, (yyvsp[-2].n), (yyvsp[0].n));
		}
#line 2137 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 38:
#line 704 "filter-parser.y" /* yacc.c:1646  */
    {	(yyval.n) = (yyvsp[0].n);					}
#line 2143 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 39:
#line 706 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_op_node(parser_ctx, AST_OP_PLUS, (yyvsp[-2].n), (yyvsp[0].n));
		}
#line 2151 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 40:
#line 710 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_op_node(parser_ctx, AST_OP_MINUS, (yyvsp[-2].n), (yyvsp[0].n));
		}
#line 2159 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 41:
#line 717 "filter-parser.y" /* yacc.c:1646  */
    {	(yyval.n) = (yyvsp[0].n);					}
#line 2165 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 42:
#line 719 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_op_node(parser_ctx, AST_OP_BIT_LSHIFT, (yyvsp[-2].n), (yyvsp[0].n));
		}
#line 2173 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 43:
#line 723 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_op_node(parser_ctx, AST_OP_BIT_RSHIFT, (yyvsp[-2].n), (yyvsp[0].n));
		}
#line 2181 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 44:
#line 730 "filter-parser.y" /* yacc.c:1646  */
    {	(yyval.n) = (yyvsp[0].n);					}
#line 2187 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 45:
#line 732 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_op_node(parser_ctx, AST_OP_BIT_AND, (yyvsp[-2].n), (yyvsp[0].n));
		}
#line 2195 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 46:
#line 739 "filter-parser.y" /* yacc.c:1646  */
    {	(yyval.n) = (yyvsp[0].n);					}
#line 2201 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 47:
#line 741 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_op_node(parser_ctx, AST_OP_BIT_XOR, (yyvsp[-2].n), (yyvsp[0].n));
		}
#line 2209 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 48:
#line 748 "filter-parser.y" /* yacc.c:1646  */
    {	(yyval.n) = (yyvsp[0].n);					}
#line 2215 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 49:
#line 750 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_op_node(parser_ctx, AST_OP_BIT_OR, (yyvsp[-2].n), (yyvsp[0].n));
		}
#line 2223 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 50:
#line 757 "filter-parser.y" /* yacc.c:1646  */
    {	(yyval.n) = (yyvsp[0].n);					}
#line 2229 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 51:
#line 759 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_op_node(parser_ctx, AST_OP_LT, (yyvsp[-2].n), (yyvsp[0].n));
		}
#line 2237 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 52:
#line 763 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_op_node(parser_ctx, AST_OP_GT, (yyvsp[-2].n), (yyvsp[0].n));
		}
#line 2245 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 53:
#line 767 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_op_node(parser_ctx, AST_OP_LE, (yyvsp[-2].n), (yyvsp[0].n));
		}
#line 2253 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 54:
#line 771 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_op_node(parser_ctx, AST_OP_GE, (yyvsp[-2].n), (yyvsp[0].n));
		}
#line 2261 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 55:
#line 778 "filter-parser.y" /* yacc.c:1646  */
    {	(yyval.n) = (yyvsp[0].n);					}
#line 2267 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 56:
#line 780 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_op_node(parser_ctx, AST_OP_EQ, (yyvsp[-2].n), (yyvsp[0].n));
		}
#line 2275 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 57:
#line 784 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_op_node(parser_ctx, AST_OP_NE, (yyvsp[-2].n), (yyvsp[0].n));
		}
#line 2283 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 58:
#line 791 "filter-parser.y" /* yacc.c:1646  */
    {	(yyval.n) = (yyvsp[0].n);					}
#line 2289 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 59:
#line 793 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_op_node(parser_ctx, AST_OP_AND, (yyvsp[-2].n), (yyvsp[0].n));
		}
#line 2297 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 60:
#line 800 "filter-parser.y" /* yacc.c:1646  */
    {	(yyval.n) = (yyvsp[0].n);					}
#line 2303 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 61:
#line 802 "filter-parser.y" /* yacc.c:1646  */
    {
			(yyval.n) = make_op_node(parser_ctx, AST_OP_OR, (yyvsp[-2].n), (yyvsp[0].n));
		}
#line 2311 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 62:
#line 809 "filter-parser.y" /* yacc.c:1646  */
    {	(yyval.n) = (yyvsp[0].n);					}
#line 2317 "filter-parser.c" /* yacc.c:1646  */
    break;

  case 63:
#line 814 "filter-parser.y" /* yacc.c:1646  */
    {
			parser_ctx->ast->root.u.root.child = (yyvsp[0].n);
		}
#line 2325 "filter-parser.c" /* yacc.c:1646  */
    break;


#line 2329 "filter-parser.c" /* yacc.c:1646  */
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (parser_ctx, scanner, YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (parser_ctx, scanner, yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
         error, discard it.  */

      if (yychar <= YYEOF)
        {
          /* Return failure if at end of input.  */
          if (yychar == YYEOF)
            YYABORT;
        }
      else
        {
          yydestruct ("Error: discarding",
                      yytoken, &yylval, parser_ctx, scanner);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
        {
          yyn += YYTERROR;
          if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
            {
              yyn = yytable[yyn];
              if (0 < yyn)
                break;
            }
        }

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
        YYABORT;


      yydestruct ("Error: popping",
                  yystos[yystate], yyvsp, parser_ctx, scanner);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (parser_ctx, scanner, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, parser_ctx, scanner);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  yystos[*yyssp], yyvsp, parser_ctx, scanner);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  return yyresult;
}
