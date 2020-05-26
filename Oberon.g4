grammar Oberon;

/* section of all identifiers in oberon */
ident : LETTER (LETTER | DIGIT)*   # Identifier
      ;

qualIdent : (ident PERIOD)? ident  # QualifiedIdentifier
          ;

identDef : ident (TIMES)?          # ExportedIdentifier
         ;

/* section on handling numbers and strings */
integer : DIGIT(DIGIT)*           # NormalInteger
        | DIGIT(HEXDIGIT)*'H'    # HexInteger
        ;

real : DIGIT(DIGIT)* PERIOD (DIGIT)*(scaleFactor)?  # RealNumber
     ;

scaleFactor : 'E'(PLUS | MINUS)?DIGIT(DIGIT)*
            ; 

number : integer | real;

string : QUOTE QUOTE QUOTE (CHARACTER)* QUOTE QUOTE QUOTE    # StringConstant
       | DIGIT (HEXDIGIT)*'X'                            # NumberStr
       ;

/* handling of different types of declarations */
constDeclaration : identDef EQUAL constExpression;
constExpression : expression;

typeDeclaration : identDef EQUAL type;
type : qualIdent 
     | arrayType
     | recordType
     | pointerType
     | procedureType
     ;
arrayType : ARRAY length (COMMA length)* OF type;
length : constExpression;
recordType : RECORD (LPAREN baseType RPAREN)? (fieldListSequence)? END;
baseType : qualIdent;
fieldListSequence : fieldList (SEMICOLON fieldList)*;
fieldList : identList COLON type;
identList : identDef (COMMA identDef)*;
pointerType : POINTER TO type;
procedureType : PROCEDURE (formalParameters)?;

variableDeclaration : identList COLON type;
expression : simpleExpression (relation simpleExpression)?;
relation : EQUAL | HASH | LESS_THAN | LEQ | GREATER_THAN | GEQ | IN | IS;
simpleExpression : (PLUS | MINUS)? term (addOperator term)*;
addOperator : PLUS | MINUS | OR;
term : factor (mulOperator factor)*;
mulOperator : TIMES | DIVIDE | DIV | MOD | BITWISEAND;
factor : number 
       | string 
       | NIL 
       | TRUE 
       | FALSE 
       | set
       | designator (actualParameters)?
       | LPAREN expression RPAREN
       | TILDE factor
       ; 

designator : qualIdent (selector)*;
selector : PERIOD ident
         | LBRACK expList RBRACK
         | POW
         | LPAREN qualIdent RPAREN
         ;
set : LBRACE (element (COMMA element)*)? RBRACE;
element : expression (ELLIPSIS expression)?;
expList : expression (COMMA expression)*;
actualParameters : LPAREN (expList)? RPAREN;

statement : assignment 
          | procedureCall
          | ifStatement
          | caseStatement
          | whileStatement
          | repeatStatement
          | forStatement
          ;
assignment : designator ASSIGN expression;
procedureCall : designator (actualParameters)?;
statementSequence : statement (SEMICOLON statement)*;
ifStatement : IF expression THEN statementSequence (ELSIF expression THEN statementSequence)* (ELSE statementSequence)? END;
caseStatement : CASE expression OF caseSequence (PIPE CASE)* END;
caseSequence : (caseLabelList COLON statementSequence)?;
caseLabelList : labelRange (COMMA labelRange)*;
labelRange : label (ELLIPSIS label);
label : integer | string | qualIdent;
whileStatement : WHILE expression DO statementSequence (ELSIF expression DO statementSequence)* END;
repeatStatement : REPEAT statementSequence UNTIL expression;
forStatement : FOR ident ASSIGN expression TO expression (BY constExpression)? DO statementSequence END;
procedureDeclaration : procedureHeading SEMICOLON procedureBody ident;
procedureHeading : PROCEDURE identDef (formalParameters)?;
procedureBody : declarationSequence (BEGIN statementSequence)? (RETURN expression)? END;
declarationSequence : (CONST (constDeclaration SEMICOLON)*)? (TYPE (typeDeclaration SEMICOLON)*)? (VAR (variableDeclaration SEMICOLON)*)? (procedureDeclaration SEMICOLON)*;
formalParameters : LPAREN (fpSection (SEMICOLON fpSection)*)? RPAREN (COLON qualIdent)?;
fpSection : (VAR)? ident (COMMA ident)* COLON formalType;
formalType : (ARRAY OF)* qualIdent;

/* modules */
module : MODULE ident SEMICOLON (importList)? declarationSequence (BEGIN statementSequence)? END ident PERIOD;
importList : IMPORT importStatement (COMMA importStatement)* SEMICOLON;
importStatement : ident (ASSIGN ident)?;


LETTER: [a-zA-Z];   // specifying letters separately due to rules of identifiers
CHARACTER : [a-zA-Z0-9] 
          | WS (CHARACTER)*
          ;
DIGIT: [0-9];       // same goes with numbers
HEXDIGIT: DIGIT | 'A' | 'B' | 'C' | 'D' | 'E' | 'F'; // hexdecimal digits
NEWLINE : '\r'? '\n';
WS : [ \t]+ -> skip;

/* section for special character */
QUOTE : '"';
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

