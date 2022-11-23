grammar Oberon;

qualIdent : (ID PERIOD)? ID
          ;
identDef : ID (TIMES)?          # ExportedIdentifier
         ;

decimalInteger: (DIGIT)+;
hexInteger: decimalInteger(HEXDIGIT)*'H';

integer : decimalInteger
        | hexInteger
        ;

real : decimalInteger decimalPart (scaleFactor)?
     ;

decimalPart : PERIOD decimalInteger?;

sign : PLUS | MINUS;

scaleFactor : 'E' (sign)? decimalInteger
            ;

string : DOUBLE_QUOTE (CHARACTER)* DOUBLE_QUOTE
        | decimalInteger(HEXDIGIT)+'X'
       ;

comment : LPAREN TIMES .*? TIMES RPAREN
        ;

identifierAssignment : identDef EQUAL
                    ;

/* parsing constants */
constDefinition : CONST (constDeclaration SEMICOLON)*;
constDeclaration : identifierAssignment expression
                 ;

/* parsing expressions */
expression : simpleExpression (relation simpleExpression)?
           ;
simpleExpression : (sign)? term (addOperator term)*
                 ;
term : factor (multOperator factor)*
     ;
factor : integer | real | string | NIL | TRUE | FALSE
       ;
relation : (EQUAL | LESS_THAN | LEQ | GREATER_THAN | GEQ | IN | IS)
         ;
addOperator : (PLUS | MINUS | OR)   # ParseAddOperator
            ;
multOperator : TIMES | DIVIDE | DIV | MOD | BITWISEAND;

/* parsing imports (libraries)*/
imprt : ID (ASSIGN ID)?;
importStatement: IMPORT imprt (COMMA imprt)* SEMICOLON;
importList : (importStatement)+;

/* parsing PROCEDUREs */
procedureDeclaration: procedureHeading procedureBody ID SEMICOLON;
procedureHeading: PROCEDURE identDef (formalParameters)? SEMICOLON;
procedureBody: declarationSequence (BEGIN)? (RETURN)? END;
formalParameters: LPAREN (fpSection (SEMICOLON fpSection)*)? RPAREN (COLON qualIdent)?;
fpSection: (VAR)? ID (COMMA ID)* COLON formalType;
formalType: (ARRAY OF)* qualIdent;

/* parsing variables */
identList : identDef (COMMA identDef)*;
type : qualIdent;
varDeclaration : identList COLON type;
varDefinition : VAR (varDeclaration SEMICOLON)*;

declarationSequence: constDefinition? varDefinition? procedureDeclaration*;

module : MODULE ID SEMICOLON (comment)* importList? declarationSequence (comment)* (BEGIN)? END ID PERIOD;

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
DIGIT: [0-9];       // same goes with numbers
HEXDIGIT: 'A' | 'B' | 'C' | 'D' | 'E' | 'F'; // hexdecimal digits
LETTER: [a-zA-Z];   // specifying letters separately due to rules of identifiers
CHARACTER : LETTER
          | DIGIT
          | DOUBLE_QUOTE
          | SINGLE_QUOTE
          ;
WS : [ \t\r\n]+ -> skip;
