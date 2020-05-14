lexer grammar OberonLexer;

LETTER: [a-zA-Z];   // specifying letters separately due to rules of identifiers
DIGIT: [0-9];       // same goes with numbers
HEXDIGIT: DIGIT | 'A' | 'B' | 'C' | 'D' | 'E' | 'F'; // hexdecimal digits

