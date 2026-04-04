parser grammar YACSParser;

options { tokenVocab=YACSLexer; }

project_file
    : (import_stmt | namespace_block | project_block)* EOF
    ;

import_stmt
    : IMPORT IDENTIFIER (AS IDENTIFIER)? SEMI
    ;

namespace_block
    : NAMESPACE IDENTIFIER LBRACE (function_def | variable_decl)* RBRACE
    ;

project_block
    : PROJECT LBRACK_LBRACK project_body RBRACK_RBRACK
    ;

target_block
    : ASYNC_KW? TARGET IDENTIFIER LBRACK_LBRACK target_body RBRACK_RBRACK
    ;

project_body
    : (statement | target_block)*
    ;

target_body
    : statement*
    ;

statement
    : variable_decl
    | assignment
    | if_stmt
    | for_stmt
    | return_stmt
    | semaphore_decl
    | exec_stmt
    | expr SEMI
    ;

variable_decl
    : type_spec? IDENTIFIER (ASSIGN expr)? SEMI
    ;

assignment
    : qualified_id op=(ASSIGN | PLUS_ASSIGN | MINUS_ASSIGN) expr SEMI
    ;

if_stmt
    : IF LPAREN expr RPAREN LBRACE statement* RBRACE (ELSE LBRACE statement* RBRACE)?
    ;

for_stmt
    : FOR LPAREN param_list IN expr RPAREN LBRACE statement* RBRACE
    ;

return_stmt
    : RETURN expr? SEMI ;

semaphore_decl
    : SEMAPHORE IDENTIFIER LPAREN NUMBER RPAREN SEMI
    ;

exec_stmt
    : EXEC qualified_id SEMI
    ;

function_def
    : FUNCTION IDENTIFIER LPAREN param_list? RPAREN ARROW type_spec LBRACE (statement)* RBRACE
    ;

// -------------------
// Expressions
// -------------------
expr
    : primary                                       #PrimaryExpr
    | qualified_id LPAREN arg_list? RPAREN                #FunctionCall
    | qualified_id                                  #IdReference
    | DOLLAR IDENTIFIER                                #VarReference
    | left=expr op=(EQUAL_TO | NOT_EQUAL_TO | LESS_THAN | GREATER_THAN | LESS_THAN_OR_EQUAL_TO | GREATER_THAN_OR_EQUAL_TO) right=expr #ComparisonExpr
    | left=expr op=(PLUS | MINUS) right=expr        #AdditiveExpr
    | expr LBRACK expr RBRACK                             #IndexAccess
    ;

primary
    : string_literal                                #StringLiteral
    | NUMBER                                        #NumberLiteral
    | (TRUE_KW | FALSE_KW)                            #BoolLiteral
    | LBRACK map_entries? RBRACK                    #MapLiteral
    | LBRACK expr_list? RBRACK                      #ListLiteral
    | LPAREN expr_list? RPAREN                      #TupleLiteral
    ;

map_entries
    : map_entry (COMMA map_entry)*
    ;

map_entry
    : expr DOUBLE_ARROW expr
    ;

expr_list
    : expr (COMMA expr)*
    ;

arg_list
    : arg (COMMA arg)*
    ;

arg
    : (IDENTIFIER ASSIGN)? expr
    ;

qualified_id
    : IDENTIFIER (COLON IDENTIFIER)*
    ;

param_list
    : param (COMMA param)*
    ;

param
    : type_spec? IDENTIFIER
    ;

type_spec
    : INT_KW
    | BOOL_KW
    | STRING_KW
    | AUTO_KW
    | TARGET
    | PATH_KW
    | LIST_KW
    | MAP_KW
    | TUPLE_KW
    | VOID_KW
    | IDENTIFIER
    ;

string_literal
    : QUOTE_OPEN (STRING_TEXT | ESCAPE | interpolation)* QUOTE_CLOSE
    ;

interpolation
    : INTERP_START expr RBRACE
    ;
