/* asn1-parse.y - ASN.1 grammar
 *      Copyright (C) 2000,2001 Fabio Fiorina
 *      Copyright (C) 2001, 2010 Free Software Foundation, Inc.
 *
 * This file is part of GNUTLS.
 *
 * GNUTLS is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * GNUTLS is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see see <http://www.gnu.org/licenses/>.
 */


/*****************************************************/
/* File: x509_ASN.y                                  */
/* Description: input file for 'bison' program.      */
/*   The output file is a parser (in C language) for */
/*   ASN.1 syntax                                    */
/*****************************************************/


%{
#ifndef BUILD_GENTOOLS
# include <config.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>

#ifdef BUILD_GENTOOLS
# include "gen-help.h"
#else
# include "util.h"
# include "ksba.h"
#endif

#include "asn1-parse.h"
#include "asn1-func.h"

/* It would be better to make yyparse static but there is no way to do
   this.  Let's hope that this macros works. */
#define yyparse _ksba_asn1_yyparse

/*#define YYDEBUG 1*/
#define YYERROR_VERBOSE = 1
#define MAX_STRING_LENGTH 129

/* constants used in the grammar */
enum {
  CONST_EXPLICIT = 1,
  CONST_IMPLICIT
};

struct parser_control_s {
  FILE *fp;
  int lineno;
  int debug;
  int result_parse;
  AsnNode parse_tree;
  AsnNode all_nodes;
};
#define PARSECTL ((struct parser_control_s *)parm)
#define YYPARSE_PARAM parm
#define YYLEX_PARAM parm

%}


%pure_parser
%expect 1

%union {
  unsigned int constant;
  char str[MAX_STRING_LENGTH];
  AsnNode node;
}

%{
static AsnNode new_node (struct parser_control_s *parsectl, node_type_t type);
#define NEW_NODE(a)  (new_node (PARSECTL, (a)))
static void set_name (AsnNode node, const char *name);
static void set_str_value (AsnNode node, const char *text);
static void set_ulong_value (AsnNode node, const char *text);
static void set_right (AsnNode node, AsnNode right);
static void append_right (AsnNode node, AsnNode right);
static void set_down (AsnNode node, AsnNode down);


static int yylex (YYSTYPE *lvalp, void *parm);
static void yyerror (const char *s);
%}

%token ASSIG "::=" 
%token <str> NUM
%token <str> IDENTIFIER
%token OPTIONAL
%token INTEGER
%token SIZE
%token OCTET
%token STRING
%token SEQUENCE
%token BIT
%token UNIVERSAL
%token PRIVATE
%token APPLICATION
%token OPTIONAL
%token DEFAULT
%token CHOICE
%token OF
%token OBJECT
%token STR_IDENTIFIER
%token BOOLEAN
%token TRUE
%token FALSE
%token TOKEN_NULL
%token ANY
%token DEFINED
%token BY
%token SET
%token EXPLICIT
%token IMPLICIT
%token DEFINITIONS
%token TAGS
%token BEGIN
%token END
%token UTCTime 
%token GeneralizedTime
%token FROM
%token IMPORTS
%token ENUMERATED
%token UTF8STRING
%token NUMERICSTRING
%token PRINTABLESTRING
%token TELETEXSTRING  
%token IA5STRING
%token UNIVERSALSTRING
%token BMPSTRING



%type <node> octet_string_def constant constant_list type_assig_right 
%type <node> integer_def type_assig type_assig_list sequence_def type_def
%type <node> bit_string_def default size_def choise_def object_def 
%type <node> boolean_def any_def size_def2 obj_constant obj_constant_list
%type <node> constant_def type_constant type_constant_list definitions
%type <node> definitions_id Time bit_element bit_element_list set_def
%type <node> identifier_list imports_def tag_type tag type_assig_right_tag
%type <node> type_assig_right_tag_default enumerated_def string_def
%type <node> utf8_string_def numeric_string_def printable_string_def
%type <node> teletex_string_def ia5_string_def universal_string_def
%type <node> bmp_string_def
%type <str>  pos_num neg_num pos_neg_num pos_neg_identifier num_identifier 
%type <constant> class explicit_implicit


%%

input:  /* empty */  
       | input definitions
;

pos_num :   NUM       { strcpy($$,$1); }
          | '+' NUM   { strcpy($$,$2); }
;

neg_num : '-' NUM    
                {
                  strcpy($$,"-");
                  strcat($$,$2);
                }
;

pos_neg_num :  pos_num  { strcpy($$,$1); }
             | neg_num  { strcpy($$,$1); }
;

num_identifier :  NUM            {strcpy($$,$1);}
                | IDENTIFIER     {strcpy($$,$1);}
;

pos_neg_identifier :  pos_neg_num    {strcpy($$,$1);}
                    | IDENTIFIER     {strcpy($$,$1);}
;

constant: '(' pos_neg_num ')'  
                        {
                          $$ = NEW_NODE (TYPE_CONSTANT); 
                          set_str_value ($$, $2);
                        }
          | IDENTIFIER'('pos_neg_num')'
                        {
                          $$ = NEW_NODE (TYPE_CONSTANT); 
                          set_name ($$, $1); 
                          set_str_value ($$, $3);
                        }
;

constant_list:  constant   { $$=$1; }
              | constant_list ',' constant
                  {
                    $$ = $1;
                    append_right ($1, $3);
                  }
;

identifier_list  :  IDENTIFIER  
                        {
                          $$ = NEW_NODE (TYPE_IDENTIFIER);
                          set_name($$,$1);
                        }
                 | identifier_list IDENTIFIER  
                        {
                          AsnNode node;

                          $$=$1;
                          node = NEW_NODE (TYPE_IDENTIFIER);
                          set_name (node, $2);
                          append_right ($$, node);
                        }
;

obj_constant:  num_identifier   
                 { 
                   $$ = NEW_NODE (TYPE_CONSTANT); 
                   set_str_value ($$, $1);
                 }
             | IDENTIFIER '(' NUM ')'
                 {
                   $$ = NEW_NODE (TYPE_CONSTANT);
                   set_name ($$, $1); 
                   set_str_value ($$, $3);
                 }
;

obj_constant_list:  obj_constant   
                        { $$=$1;}
                  | obj_constant_list obj_constant 
                        {
                          $$=$1;
                          append_right ($$, $2);
                        }
;

class :  UNIVERSAL    { $$ = CLASS_UNIVERSAL;   }
       | PRIVATE      { $$ = CLASS_PRIVATE;     }
       | APPLICATION  { $$ = CLASS_APPLICATION; }
;

tag_type :  '[' NUM ']'   
                {
                  $$ = NEW_NODE (TYPE_TAG); 
                  $$->flags.class = CLASS_CONTEXT;
                  set_ulong_value ($$, $2);
                }
          | '[' class NUM ']' 
                {
                  $$ = NEW_NODE (TYPE_TAG);
                  $$->flags.class = $2;
                  set_ulong_value ($$, $3);
                }
;

tag :  tag_type
         { $$ = $1; }
     | tag_type EXPLICIT  
         {
           $$ = $1;
           $$->flags.explicit = 1;
         }
     | tag_type IMPLICIT 
         {
           $$ = $1;
           $$->flags.implicit = 1;
         }
;

default :  DEFAULT pos_neg_identifier 
               {
                 $$ = NEW_NODE (TYPE_DEFAULT); 
                 set_str_value ($$, $2);
               }
         | DEFAULT TRUE 
               {
                 $$ = NEW_NODE (TYPE_DEFAULT);
                 $$->flags.is_true = 1;
               }
         | DEFAULT FALSE 
               {
                 $$ = NEW_NODE (TYPE_DEFAULT);
                 $$->flags.is_false = 1;
               }
;

integer_def: INTEGER   
               {
                 $$ = NEW_NODE (TYPE_INTEGER);
               }
           | INTEGER '{' constant_list '}' 
               {
                 $$ = NEW_NODE (TYPE_INTEGER);
                 $$->flags.has_list = 1;
                 set_down ($$, $3);
               }
           | integer_def '(' num_identifier '.' '.' num_identifier ')'
               {
                 $$ = NEW_NODE (TYPE_INTEGER);
                 $$->flags.has_min_max = 1;
                 /* the following is wrong.  Better use a union for the value*/
                 set_down ($$, NEW_NODE (TYPE_SIZE) ); 
                 set_str_value ($$->down, $6); 
                 set_name ($$->down, $3);
               }
;

boolean_def: BOOLEAN  
              {
                $$ = NEW_NODE (TYPE_BOOLEAN);
              }
;

Time:   UTCTime       
          {
            $$ = NEW_NODE (TYPE_UTC_TIME);
          } 
      | GeneralizedTime  
          { 
            $$ = NEW_NODE (TYPE_GENERALIZED_TIME);
          } 
;

size_def2: SIZE '(' num_identifier ')' 
             {
               $$ = NEW_NODE (TYPE_SIZE);
               $$->flags.one_param = 1;
               set_str_value ($$, $3);
             }
        | SIZE '(' num_identifier '.' '.' num_identifier ')'  
             {
               $$ = NEW_NODE (TYPE_SIZE);
               $$->flags.has_min_max = 1;
               set_str_value ($$, $3);
               set_name ($$, $6);
             }
;

size_def:   size_def2          
             {
               $$=$1;
             }
          | '(' size_def2 ')'  
             {
               $$=$2;
             }
;

octet_string_def : OCTET STRING 
                     {
                       $$ = NEW_NODE (TYPE_OCTET_STRING);
                     }
                 | OCTET STRING size_def
                     {
                       $$ = NEW_NODE (TYPE_OCTET_STRING);
                       $$->flags.has_size = 1;
                       set_down ($$,$3);
                     }
;

utf8_string_def:   UTF8STRING      { $$ = NEW_NODE (TYPE_UTF8_STRING); }
                 | UTF8STRING size_def
                     {
                       $$ = NEW_NODE (TYPE_UTF8_STRING);
                       $$->flags.has_size = 1;
                       set_down ($$,$2);
                     }
;
numeric_string_def:   NUMERICSTRING  { $$ = NEW_NODE (TYPE_NUMERIC_STRING); }
                    | NUMERICSTRING size_def
                     {
                       $$ = NEW_NODE (TYPE_NUMERIC_STRING);
                       $$->flags.has_size = 1;
                       set_down ($$,$2);
                     }
;
printable_string_def:  PRINTABLESTRING 
                        { $$ = NEW_NODE (TYPE_PRINTABLE_STRING); }
                     | PRINTABLESTRING size_def
                        { 
                          $$ = NEW_NODE (TYPE_PRINTABLE_STRING);
                          $$->flags.has_size = 1;
                          set_down ($$,$2);
                        }
;
teletex_string_def:   TELETEXSTRING
                       { $$ = NEW_NODE (TYPE_TELETEX_STRING); }
                   | TELETEXSTRING size_def
                     {
                       $$ = NEW_NODE (TYPE_TELETEX_STRING);
                       $$->flags.has_size = 1;
                       set_down ($$,$2);
                     }
;
ia5_string_def:   IA5STRING      { $$ = NEW_NODE (TYPE_IA5_STRING); }
                | IA5STRING size_def
                     {
                       $$ = NEW_NODE (TYPE_IA5_STRING);
                       $$->flags.has_size = 1;
                       set_down ($$,$2);
                     }
;
universal_string_def:   UNIVERSALSTRING
                         { $$ = NEW_NODE (TYPE_UNIVERSAL_STRING); }
                      | UNIVERSALSTRING size_def
                         {
                           $$ = NEW_NODE (TYPE_UNIVERSAL_STRING);
                           $$->flags.has_size = 1;
                           set_down ($$,$2);
                         }
;
bmp_string_def:    BMPSTRING      { $$ = NEW_NODE (TYPE_BMP_STRING); }
                 | BMPSTRING size_def
                     {
                       $$ = NEW_NODE (TYPE_BMP_STRING);
                       $$->flags.has_size = 1;
                       set_down ($$,$2);
                     }
;


string_def:  utf8_string_def
           | numeric_string_def
           | printable_string_def
           | teletex_string_def
           | ia5_string_def
           | universal_string_def
           | bmp_string_def
;




bit_element :  IDENTIFIER'('NUM')'
                 {
                   $$ = NEW_NODE (TYPE_CONSTANT);
                   set_name ($$, $1); 
                   set_str_value ($$, $3);
                 }
;

bit_element_list :  bit_element 
                      {
                        $$=$1;
                      }
                  | bit_element_list ',' bit_element 
                      {
                        $$=$1;
                        append_right ($$, $3);
                      }
;

bit_string_def : BIT STRING 
                   {
                     $$ = NEW_NODE (TYPE_BIT_STRING);
                   }
               | BIT STRING '{' bit_element_list '}' 
                   {
                     $$ = NEW_NODE (TYPE_BIT_STRING);
                     $$->flags.has_list = 1;
                     set_down ($$, $4);
                   }
;

enumerated_def : ENUMERATED '{' bit_element_list '}' 
                   {
                     $$ = NEW_NODE (TYPE_ENUMERATED);
                     $$->flags.has_list = 1;
                     set_down ($$, $3);
                   }
;

object_def :  OBJECT STR_IDENTIFIER 
                   {
                     $$ = NEW_NODE (TYPE_OBJECT_ID);
                   }
;

type_assig_right: IDENTIFIER  
                    {
                      $$ = NEW_NODE (TYPE_IDENTIFIER);
                      set_str_value ($$, $1);
                    }
                | IDENTIFIER size_def
                    {
                      $$ = NEW_NODE (TYPE_IDENTIFIER);
                      $$->flags.has_size = 1;
                      set_str_value ($$, $1);
                      set_down ($$, $2);
                    }
                | integer_def        {$$=$1;}
                | enumerated_def     {$$=$1;}
                | boolean_def        {$$=$1;}
                | string_def         {$$=$1;}
                | Time            
                | octet_string_def   {$$=$1;}
                | bit_string_def     {$$=$1;}
                | sequence_def       {$$=$1;}
                | object_def         {$$=$1;}
                | choise_def         {$$=$1;}
                | any_def            {$$=$1;}
                | set_def            {$$=$1;}
                | TOKEN_NULL  
                    {
                      $$ = NEW_NODE(TYPE_NULL);
                    }
;

type_assig_right_tag :   type_assig_right  
                           {
                             $$ = $1;
                           }
                       | tag type_assig_right
                           {
/*                               $2->flags.has_tag = 1; */
/*                               $$ = $2; */
/*                               set_right ($1, $$->down ); */
/*                               set_down ($$, $1); */
                             $$ = $1;
                             set_down ($$, $2);
                           }
;

type_assig_right_tag_default : type_assig_right_tag  
                                 {
                                   $$ = $1;
                                 }
                             | type_assig_right_tag default  
                                 {
                                   $1->flags.has_default = 1;
                                   $$ = $1;
                                   set_right ($2, $$->down); 
                                   set_down ($$, $2);
                                 }
                             | type_assig_right_tag OPTIONAL  
                                 {
                                   $1->flags.is_optional = 1;
                                   $$ = $1;
                                 }
;
 
type_assig : IDENTIFIER type_assig_right_tag_default
               {
                 set_name ($2, $1); 
                 $$ = $2;
               }
;

type_assig_list : type_assig         
                    { $$=$1; }
                | type_assig_list ',' type_assig 
                    {
                      $$=$1;
                      append_right ($$, $3);
                    }
;

sequence_def : SEQUENCE '{' type_assig_list '}'
                 {
                   $$ = NEW_NODE (TYPE_SEQUENCE);
                   set_down ($$, $3);
                 }
             | SEQUENCE OF type_assig_right 
                 {
                   $$ = NEW_NODE (TYPE_SEQUENCE_OF);
                   set_down ($$, $3);
                 }
             | SEQUENCE size_def OF type_assig_right
                 {
                   $$ = NEW_NODE (TYPE_SEQUENCE_OF);
                   $$->flags.has_size = 1;
                   set_right ($2,$4);
                   set_down ($$,$2);
                 }
; 

set_def :  SET '{' type_assig_list '}'
             {
               $$ = NEW_NODE (TYPE_SET);
               set_down ($$, $3);
             }
         | SET OF type_assig_right
             {
               $$ = NEW_NODE (TYPE_SET_OF);
               set_down ($$, $3);
             }
         | SET size_def OF type_assig_right
             {
               $$ = NEW_NODE (TYPE_SET_OF);
               $$->flags.has_size = 1;
               set_right ($2, $4);
               set_down ($$, $2);
             }
; 

choise_def :  CHOICE '{' type_assig_list '}'
                {
                  $$ = NEW_NODE (TYPE_CHOICE);
                  set_down ($$, $3);
                }
;

any_def :  ANY   
             {
               $$ = NEW_NODE (TYPE_ANY);
             }
         | ANY DEFINED BY IDENTIFIER  
             {
               AsnNode node;

               $$ = NEW_NODE (TYPE_ANY);
               $$->flags.has_defined_by = 1;
               node = NEW_NODE (TYPE_CONSTANT);
               set_name (node, $4);
               set_down($$, node);
             }
;

type_def : IDENTIFIER "::=" type_assig_right_tag 
             {
               set_name ($3, $1);
               $$ = $3;
             }
;

constant_def : IDENTIFIER OBJECT STR_IDENTIFIER "::=" '{' obj_constant_list '}'
                 {
                   $$ = NEW_NODE (TYPE_OBJECT_ID);
                   $$->flags.assignment = 1;
                   set_name ($$, $1);  
                   set_down ($$, $6);
                 }
             | IDENTIFIER IDENTIFIER "::=" '{' obj_constant_list '}'
                 {
                   $$ = NEW_NODE (TYPE_OBJECT_ID);
                   $$->flags.assignment = 1;
                   $$->flags.one_param = 1;
                   set_name ($$, $1);  
                   set_str_value ($$, $2);
                   set_down ($$, $5);
                 }
             | IDENTIFIER INTEGER "::=" NUM
                 {
                   $$ = NEW_NODE (TYPE_INTEGER);
                   $$->flags.assignment = 1;
                   set_name ($$, $1);  
                   set_str_value ($$, $4);
                 }
;

type_constant:   type_def     { $$ = $1; }
               | constant_def { $$ = $1; }
;

type_constant_list : type_constant  
                       { $$ = $1; }
                   | type_constant_list type_constant 
                       { 
                         $$ = $1;
                         append_right ($$, $2);
                       }
;

definitions_id : IDENTIFIER  '{' obj_constant_list '}' 
                   {
                     $$ = NEW_NODE (TYPE_OBJECT_ID);
                     set_down ($$, $3);
                     set_name ($$, $1);
                   }
;

imports_def :  /* empty */ 
                { $$=NULL;}
            | IMPORTS identifier_list FROM IDENTIFIER obj_constant_list 
                {
                  AsnNode node;

                  $$ = NEW_NODE (TYPE_IMPORTS);
                  node = NEW_NODE (TYPE_OBJECT_ID);
                  set_name (node, $4);  
                  set_down (node, $5);
                  set_down ($$, node);
                  set_right ($$, $2);
                }
;

explicit_implicit :  EXPLICIT  { $$ = CONST_EXPLICIT; }
                   | IMPLICIT  { $$ = CONST_IMPLICIT; }
;

definitions: definitions_id
             DEFINITIONS explicit_implicit TAGS "::=" BEGIN imports_def 
             type_constant_list END
               {
                 AsnNode node;
                 
                 $$ = node = NEW_NODE (TYPE_DEFINITIONS);

                 if ($3 == CONST_EXPLICIT)
                   node->flags.explicit = 1;
                 else if ($3 == CONST_IMPLICIT)
                   node->flags.implicit = 1;

                 if ($7)
                   node->flags.has_imports = 1;

                 set_name ($$, $1->name);
                 set_name ($1, "");

                 if (!node->flags.has_imports) 
                   set_right ($1,$8);
                 else
                   {
                     set_right ($7,$8);
                     set_right ($1,$7);
                   }

                 set_down ($$, $1);

                 _ksba_asn_set_default_tag ($$);
                 _ksba_asn_type_set_config ($$);
                 PARSECTL->result_parse = _ksba_asn_check_identifier($$);
                 PARSECTL->parse_tree=$$;
               }
;

%%

const char *key_word[]={
  "::=","OPTIONAL","INTEGER","SIZE","OCTET","STRING"
  ,"SEQUENCE","BIT","UNIVERSAL","PRIVATE","OPTIONAL"
  ,"DEFAULT","CHOICE","OF","OBJECT","IDENTIFIER"
  ,"BOOLEAN","TRUE","FALSE","APPLICATION","ANY","DEFINED"
  ,"SET","BY","EXPLICIT","IMPLICIT","DEFINITIONS","TAGS"
  ,"BEGIN","END","UTCTime","GeneralizedTime","FROM"
  ,"IMPORTS","NULL","ENUMERATED"
  ,"UTF8String","NumericString","PrintableString","TeletexString"
  ,"IA5String","UniversalString","BMPString"
};
const int key_word_token[]={
   ASSIG,OPTIONAL,INTEGER,SIZE,OCTET,STRING
  ,SEQUENCE,BIT,UNIVERSAL,PRIVATE,OPTIONAL
  ,DEFAULT,CHOICE,OF,OBJECT,STR_IDENTIFIER
  ,BOOLEAN,TRUE,FALSE,APPLICATION,ANY,DEFINED
  ,SET,BY,EXPLICIT,IMPLICIT,DEFINITIONS,TAGS
  ,BEGIN,END,UTCTime,GeneralizedTime,FROM
  ,IMPORTS,TOKEN_NULL,ENUMERATED
  ,UTF8STRING,NUMERICSTRING,PRINTABLESTRING,TELETEXSTRING  
  ,IA5STRING,UNIVERSALSTRING,BMPSTRING
};      


/*************************************************************/
/*  Function: yylex                                          */
/*  Description: looks for tokens in file_asn1 pointer file. */
/*  Return: int                                              */
/*    Token identifier or ASCII code or 0(zero: End Of File) */
/*************************************************************/
static int 
yylex (YYSTYPE *lvalp, void *parm)
{
  int c,counter=0,k;
  char string[MAX_STRING_LENGTH];
  FILE *fp = PARSECTL->fp;

  if (!PARSECTL->lineno)
    PARSECTL->lineno++; /* start with line one */

  while (1)
    {
      while ( (c=fgetc (fp))==' ' || c=='\t')
        ;
      if (c =='\n')
        {
          PARSECTL->lineno++;
          continue;
        }
      if(c==EOF)
        return 0;

      if ( c=='(' || c==')' || c=='[' || c==']' 
           || c=='{' || c=='}' || c==',' || c=='.' || c=='+')
        return c;

      if (c=='-')
        {
          if ( (c=fgetc(fp))!='-')
            {
              ungetc(c,fp);
              return '-';
            }
          else
            {
              /* A comment finishes at the end of line */
              counter=0;
              while ( (c=fgetc(fp))!=EOF && c != '\n' )
                ;
              if (c==EOF)
                return 0;
              else 
                continue; /* repeat the search */
            }
        }
      
      do
        { 
          if (counter >= DIM (string)-1 )
            {
              fprintf (stderr,"%s:%d: token too long\n", "myfile:",
                       PARSECTL->lineno);
              return 0; /* EOF */
            }
          string[counter++]=c;
        }
      while ( !((c=fgetc(fp))==EOF
                || c==' '|| c=='\t' || c=='\n' 
                || c=='(' || c==')' || c=='[' || c==']' 
                || c=='{' || c=='}' || c==',' || c=='.'));
      
      ungetc (c,fp);
      string[counter]=0;
      /*fprintf (stderr, "yylex token `%s'\n", string);*/

      /* Is STRING a number? */
      for (k=0; k<counter; k++) 
        {
          if(!isdigit(string[k])) 
            break;
        }
      if (k>=counter)
        {
          strcpy (lvalp->str,string);  
          if (PARSECTL->debug)
            fprintf (stderr,"%d: yylex found number `%s'\n",
                     PARSECTL->lineno, string);
          return NUM;
        }
      
      /* Is STRING a keyword? */
      for (k=0; k<(sizeof(key_word)/sizeof(char*));k++ )
        {
          if (!strcmp(string,key_word[k])) 
            {
              if (PARSECTL->debug)
                fprintf (stderr,"%d: yylex found keyword `%s'\n",
                         PARSECTL->lineno, string);
              return key_word_token[k]; 
            }
        }
      
      /* STRING is an IDENTIFIER */
      strcpy(lvalp->str,string);
      if (PARSECTL->debug)
        fprintf (stderr,"%d: yylex found identifier `%s'\n",
                 PARSECTL->lineno, string);
      return IDENTIFIER;
    }
}

static void 
yyerror (const char *s)
{
  /* Sends the error description to stderr */
  fprintf (stderr, "%s\n", s); 
  /* Why doesn't bison provide a way to pass the parm to yyerror ??*/
}


 
static AsnNode
new_node (struct parser_control_s *parsectl, node_type_t type)
{
  AsnNode node;

  node = xcalloc (1, sizeof *node);
  node->type = type;
  node->off = -1;
  node->link_next = parsectl->all_nodes;
  parsectl->all_nodes = node;

  return node;
}

static void
release_all_nodes (AsnNode node)
{
  AsnNode node2;

  for (; node; node = node2)
    {
      node2 = node->link_next;
      xfree (node->name);
      
      if (node->valuetype == VALTYPE_CSTR)
        xfree (node->value.v_cstr);
      else if (node->valuetype == VALTYPE_MEM)
        xfree (node->value.v_mem.buf);

      xfree (node);
    }
}

static void
set_name (AsnNode node, const char *name)
{
  _ksba_asn_set_name (node, name);
}

static void
set_str_value (AsnNode node, const char *text)
{
  if (text && *text)
    _ksba_asn_set_value (node, VALTYPE_CSTR, text, 0);
  else
    _ksba_asn_set_value (node, VALTYPE_NULL, NULL, 0);
}

static void
set_ulong_value (AsnNode node, const char *text)
{
  unsigned long val;

  if (text && *text)
    val = strtoul (text, NULL, 10);
  else
    val = 0;
  _ksba_asn_set_value (node, VALTYPE_ULONG, &val, sizeof(val));
}

static void
set_right (AsnNode node, AsnNode right)
{
  return_if_fail (node);

  node->right = right;
  if (right)
    right->left = node;
}

static void
append_right (AsnNode node, AsnNode right)
{
  return_if_fail (node);

  while (node->right)
    node = node->right;

  node->right = right;
  if (right)
    right->left = node;
}


static void
set_down (AsnNode node, AsnNode down)
{
  return_if_fail (node);

  node->down = down;
  if (down)
    down->left = node;
}


/**
 * ksba_asn_parse_file:
 * @file_name: Filename with the ASN module
 * @pointer: Returns the syntax tree
 * @debug: Enable debug output
 *
 * Parse an ASN.1 file and return an syntax tree.
 * 
 * Return value: 0 for okay or an ASN_xx error code
 **/
int 
ksba_asn_parse_file (const char *file_name, ksba_asn_tree_t *result, int debug)
{
  struct parser_control_s parsectl;
     
  *result = NULL;
  
  parsectl.fp = file_name? fopen (file_name, "r") : NULL;
  if ( !parsectl.fp )
    return gpg_error_from_syserror ();

  parsectl.lineno = 0;
  parsectl.debug = debug;
  parsectl.result_parse = gpg_error (GPG_ERR_SYNTAX);
  parsectl.parse_tree = NULL;
  parsectl.all_nodes = NULL;
  /*yydebug = 1;*/
  if ( yyparse ((void*)&parsectl) || parsectl.result_parse )
    { /* error */
      fprintf (stderr, "%s:%d: parse error\n",
               file_name?file_name:"-", parsectl.lineno ); 
      release_all_nodes (parsectl.all_nodes);
      parsectl.all_nodes = NULL;
    }
  else 
    { /* okay */
      ksba_asn_tree_t tree;

      _ksba_asn_change_integer_value (parsectl.parse_tree);
      _ksba_asn_expand_object_id (parsectl.parse_tree);
      tree = xmalloc ( sizeof *tree + (file_name? strlen (file_name):1) );
      tree->parse_tree = parsectl.parse_tree;
      tree->node_list = parsectl.all_nodes;
      strcpy (tree->filename, file_name? file_name:"-");
      *result = tree;
    }

  if (file_name)
    fclose (parsectl.fp);
  return parsectl.result_parse;
}

void
ksba_asn_tree_release (ksba_asn_tree_t tree)
{
  if (!tree)
    return;
  release_all_nodes (tree->node_list);
  tree->node_list = NULL;
  xfree (tree);
}


void
_ksba_asn_release_nodes (AsnNode node)
{
  /* FIXME: it does not work yet because the allocation function in
     asn1-func.c does not link all nodes together */
  release_all_nodes (node);
}
