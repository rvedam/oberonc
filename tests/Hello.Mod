MODULE Hello;

IMPORT Out;

CONST
  Greeting* = "Hello, World!";

VAR
  x, y: INTEGER;

PROCEDURE SayHello*;
BEGIN
  Out.String(Greeting);
  Out.Ln
END SayHello;

BEGIN
  x := 42;
  y := x + 1;
  SayHello
END Hello.
