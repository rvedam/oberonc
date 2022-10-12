grammar Oberon;

qualIdent : (ID PERIOD)? ID  # QualifiedIdentifier
          ;
identDef : ID (TIMES)?          # ExportedIdentifier
         ;

imprt : ID ASSIGN ID;
importList : IMPORT imprt (COMMA imprt)* SEMICOLON;
module : MODULE ID SEMICOLON (importList)? BEGIN END ID PERIOD;

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
