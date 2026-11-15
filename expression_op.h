#ifndef __EXPRESSION_H__
#define __EXPRESSION_H__

typedef struct accumulator {
  char kind;
  unsigned char type;
  ATOM val;
  unsigned long size;
} ACUM;

#define NAME		val.dc
#define OPERATOR	val.dc
#define ADDRESS		val.dp
#define OFFSET		val.di
#define VAL			val.di
#define FUNCTION	val.di

#define MAX_STACKS 256

/* token identifier */
#define EXP_TERMINATE   0x00
#define EXP_CONST       0x01
#define EXP_OPERATOR    0x02
#define EXP_FUNCTION    0x03
#define EXP_SYSVAR      0x04
#define EXP_VARIABLE    0x05
#define EXP_ELEMENT     0x06
#define EXP_RESET       0x07

/* variable types */
#define VAR_CHAR        0x01
#define VAR_SHORT       0x02
#define VAR_INT         0x03
#define VAR_LONG        0x04
#define VAR_STR         0x05
#define VAR_OBJECT      0x08
#define VAR_OCHR        0x09
#define VAR_ITEM        0x0a
#define VAR_MAGIC       0x0b
/* variable (array) */
#define VAR_ARRAY       0x80
#define VAR_CHAR_A      (VAR_ARRAY | VAR_CHAR)
#define VAR_SHORT_A     (VAR_ARRAY | VAR_SHORT)
#define VAR_INT_A       (VAR_ARRAY | VAR_INT)
#define VAR_LONG_A      (VAR_ARRAY | VAR_LONG)
#define VAR_STR_A       (VAR_ARRAY | VAR_STR)
#define VAR_OCHR_A      (VAR_ARRAY | VAR_OCHR)
#define VAR_ITEM_A      (VAR_ARRAY | VAR_ITEM)
#define VAR_MAGIC_A     (VAR_ARRAY | VAR_MAGIC)

/* operator */
#define OP_STACKTOPCOPY	'c'
#define OP_GREATEREQUAL	'g'
#define OP_LESSEQUAL	'l'
#define OP_EQUAL		'e'
#define OP_REVERSE		'r'


/* function */
#define FUNC_RND 1

/* error message */
#define ERRM_UNMATCH_BRACKET	"swt err  : Unmatched bracket '%c'."
#define ERRM_LONG_EXPRESSION	"swt err  : Expression too long."
#define ERRM_UNDEF_SYSVAR		"swt err  : Undefined system variable."
#define ERRM_UNDEF_OBJ			"swt err  : Undefined object variable."
#define ERRM_INVALID_STRING		"swt err  : Invalid string."
#define ERRM_INVALID_ELEMENT	"swt err  : Invalid object element."

#define ERRM_STACK_EMPTY		"swt err  : Stack empty."
#define ERRM_STACK_OVERFLOW		"swt err  : Stack overflow."

#define ERRM_NULL_REFERENCE		"swt err  : Refered to null objects."

#define ERRM_INVALID_VARTYPE	"swt err  : Invalid variable type."
#define ERRM_ILLEAGAL_VALUE		"swt err  : Illeagal value."
#define ERRM_UNDEF_FUNC			"swt err  : Undefined function."
#define ERRM_ILLEAGAL_SUBST		"swt err  : Illeagal substitution."
#define ERRM_UNSUPORT_SUBST		"swt err  : Substitute unsuport variable type."
#define ERRM_DEVIDE_ZERO		"swt err  : Devide by zero."
#define ERRM_RANGEOUT			"swt err  : Out of range of array."
#define ERRM_UNDEF_ARRAY		"swt err  : Undefined array type."
#define ERRM_UNDEF_OPERATOR		"swt err  : Undefined operation."
#define ERRM_ILLEAGAL_CODE		"swt err  : Illeagal code."
#define ERRM_UNDEF_VAR			"swt err  : Undefined variable."

extern int stpt;

int get_token(char **,int *,ATOM *);
int get_arrayaffix(char **,int *,ATOM *);
int get_CHRelement(char **,int *,ATOM *);
int get_itemelement(char **,int *,ATOM *);
int get_magicelement(char **,int *,ATOM *);
int put_token(ACUM ,int *,ATOM *);
int get_sentence(char **,int *,ATOM *,char);
int get_codetoken(ACUM *,int *,ATOM *);
int get_stack(ACUM *);
int peek_stack(ACUM *);
int put_stack(ACUM);
int get_value(ACUM *);
int calc_expression(int n,int *,ATOM *);

#endif

