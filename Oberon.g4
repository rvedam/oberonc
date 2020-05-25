grammar Oberon;

/* section of all identifiers in oberon */
ident : LETTER (LETTER | DIGIT)*   # Identifier
      ;
qualident : (ident PERIOD)? ident  # QualifiedIdentifier
          ;

LETTER: [a-zA-Z];   // specifying letters separately due to rules of identifiers
DIGIT: [0-9];       // same goes with numbers
HEXDIGIT: DIGIT | 'A' | 'B' | 'C' | 'D' | 'E' | 'F'; // hexdecimal digits

/* section for special character */
LPAREN : '(';
RPAREN : ')';
LBRACK : '[';
RBRACK : ']';
LBRACE : '{';
RBRACE : '}';
PLUS : '+';
MINUS : '-';
TIMES : '*';
DIV : '/';
TILDE : '~';
BITWISEAND : '&';
PERIOD : '.';
COMMA : ',';
SEMICOLON : ';';
PIPE : '|';
ASSIGN: ':=';

