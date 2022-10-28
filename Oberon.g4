grammar Oberon;

qualIdent : (ID PERIOD)? ID  # QualifiedIdentifier
          ;
identDef : ID (TIMES)?          # ExportedIdentifier
         ;

integer : DIGIT(DIGIT)*
        | DIGIT(HEXDIGIT)* 'H'
        ;

real : DIGIT(DIGIT)* decimalPart (scaleFactor)?
     ;

decimalPart : PERIOD DIGIT*;

scaleFactor : 'E' (PLUS | MINUS)? DIGIT(DIGIT)*
            ;
number : integer | real;

string : DOUBLE_QUOTE (CHARACTER)* DOUBLE_QUOTE
        | DIGIT (HEXDIGIT)* 'X'
       ;

comment : LPAREN TIMES .*? TIMES RPAREN
        ;

identifierAssignment : identDef EQUAL
                    ;

constDeclaration : identifierAssignment expression
                 ;

expression : simpleExpression relation simpleExpression # ParseExpression
           ;

simpleExpression : (PLUS | MINUS)? term (addOperator term)*     # ParseSimpleExpression
                 ;

term : factor (multOperator factor)*
     ;

factor : (number | string | NIL | TRUE | FALSE)
       ;

relation : (EQUAL | LESS_THAN | LEQ | GREATER_THAN | GEQ | IN | IS)
         ;

addOperator : (PLUS | MINUS | OR)   # ParseAddOperator
            ;

multOperator : TIMES | DIVIDE | DIV | MOD | BITWISEAND;

imprt : ID (ASSIGN ID)?;

importStatement: IMPORT imprt (COMMA imprt)* SEMICOLON;

importList : comment* importStatement+ comment*;

module : MODULE ID SEMICOLON importList? BEGIN? END ID PERIOD;

/* section for special character */
DOUBLE_QUOTE : '"';
SINGLE_QUOTE : '\'';
LPAREN : '(';
RPAREN : ')';
LBRACK : '[';
RBRACK : ']';
LBRACE : '{';
RBRACE : '}';
PLUS : '+';
MINUS : '-';
TIMES : '*';
DIVIDE : '/';
TILDE : '~';
BITWISEAND : '&';
PERIOD : '.';
COMMA : ',';
SEMICOLON : ';';
PIPE : '|';
ASSIGN: ':=';
POW : '^';
EQUAL : '=';
HASH : '#';
LESS_THAN : '<';
GREATER_THAN : '>';
LEQ : '<=';
GEQ : '>=';
ELLIPSIS : '..';
COLON : ':';
ARRAY : 'ARRAY';
BEGIN : 'BEGIN';
BY : 'BY';
CASE : 'CASE';
CONST : 'CONST';
DIV : 'DIV';
DO : 'DO';
ELSE : 'ELSE';
ELSIF : 'ELSIF';
END : 'END';
FALSE : 'FALSE';
FOR : 'FOR';
IF : 'IF';
IMPORT : 'IMPORT';
IN : 'IN';
IS : 'IS';
MOD : 'MOD';
MODULE : 'MODULE';
NIL : 'NIL';
OF : 'OF';
OR : 'OR';
POINTER : 'POINTER';
PROCEDURE : 'PROCEDURE';
RECORD : 'RECORD';
REPEAT : 'REPEAT';
RETURN : 'RETURN';
THEN : 'THEN';
TO : 'TO';
TRUE : 'TRUE';
TYPE : 'TYPE';
UNTIL : 'UNTIL';
VAR : 'VAR';
WHILE : 'WHILE';

ID: LETTER(LETTER|DIGIT)*;
LETTER: [a-zA-Z];   // specifying letters separately due to rules of identifiers
CHARACTER : LETTER
          | DIGIT
          | DOUBLE_QUOTE
          | SINGLE_QUOTE
          ;
DIGIT: [0-9];       // same goes with numbers
HEXDIGIT: DIGIT | 'A' | 'B' | 'C' | 'D' | 'E' | 'F'; // hexdecimal digits
WS : [ \t\r\n]+ -> skip;
