lexer grammar YACSLexer;

// Встраиваем кусочек C++ кода прямо в лексер для подсчета скобок
@header {
    #include <vector>
}

@members {
    std::vector<int> interpStack;
}

// -------------------
// DEFAULT MODE (Основной код)
// -------------------

// Токены интерполяции и строк
QUOTE_OPEN  : '"' -> pushMode(STRING_MODE) ;

// Перехватываем скобки, чтобы знать, когда выходить из интерполяции
LBRACE : '{' {
    if (!interpStack.empty()) interpStack.back()++;
} ;

RBRACE : '}' {
    if (!interpStack.empty()) {
        if (interpStack.back() == 0) {
            // Мы нашли ту самую закрывающую скобку для ${ ... }
            interpStack.pop_back();
            popMode();
        } else {
            interpStack.back()--;
        }
    }
} ;

// -------------------
// Ключевые слова (Keywords)
// -------------------
IMPORT    : 'import' ;
AS        : 'as' ;
NAMESPACE : 'namespace' ;
PROJECT   : 'project' ;
TARGET    : 'target' ;
IF        : 'if' ;
ELSE      : 'else' ;
RETURN    : 'return' ;
SEMAPHORE : 'semaphore' ;
EXEC      : 'exec' ;
FUNCTION  : 'function' ;
FOR       : 'for' ;
IN        : 'in' ;

ASYNC_KW : 'async' ;

// Булевы значения
TRUE_KW   : 'true' ;
FALSE_KW  : 'false' ;

// Типы данных (добавляем суффикс _KW чтобы не путать с другими токенами)
INT_KW    : 'int' ;
BOOL_KW   : 'bool' ;
STRING_KW : 'string' ;
AUTO_KW   : 'auto' ;
PATH_KW   : 'path' ;
LIST_KW   : 'list' ;
MAP_KW    : 'map' ;
TUPLE_KW  : 'tuple' ;
VOID_KW   : 'void' ;

// Спец-скобки для блоков (о которых я предупреждал в прошлом сообщении)
LBRACK_LBRACK : '[[' ;
RBRACK_RBRACK : ']]' ;

// Твои старые добрые токены
ARROW : '->' ;
DOUBLE_ARROW       : '=>' ;
IDENTIFIER  : [a-zA-Z_][a-zA-Z0-9_]* ;
NUMBER      : [0-9]+ ('.' [0-9]+)? ;
ASSIGN       : '=' ;
PLUS  : '+' ;
MINUS : '-' ;
PLUS_ASSIGN  : '+=' ;
MINUS_ASSIGN : '-=' ;
EQUAL_TO : '==' ;
NOT_EQUAL_TO : '!=' ;
LESS_THAN : '<' ;
GREATER_THAN : '>' ;
LESS_THAN_OR_EQUAL_TO: '<=' ;
GREATER_THAN_OR_EQUAL_TO: '>=' ;

// Не забываем знаки препинания, которые раньше были литералами в парсере
SEMI  : ';' ;
COMMA : ',' ;
LPAREN: '(' ;
RPAREN: ')' ;
LBRACK: '[' ;
RBRACK: ']' ;
COLON : ':' ;
DOLLAR: '$' ;

WS          : [ \t\r\n]+ -> skip ;
COMMENT     : '//' ~[\r\n]* -> skip ;
BLOCK_COMMENT : '/*' .*? '*/' -> skip ;

// -------------------
// STRING MODE (Внутри кавычек)
// -------------------
mode STRING_MODE;

INTERP_START: '${' {
    interpStack.push_back(0);
} -> pushMode(DEFAULT_MODE) ;

STRING_TEXT : ~["\\$]+ ;
ESCAPE      : '\\' . ;
QUOTE_CLOSE : '"' -> popMode ;