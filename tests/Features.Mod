(* Tests all major Oberon-07 grammar features *)
MODULE Features;

IMPORT In, Out;

CONST
  MaxSize* = 100;
  Pi       = 3.14159E0;
  NullChar = 0DX;

TYPE
  Index   = INTEGER;
  Vector* = ARRAY 3 OF REAL;
  Matrix  = ARRAY 4, 4 OF REAL;
  Node    = POINTER TO NodeRec;
  NodeRec = RECORD
    value : INTEGER;
    next  : Node
  END;
  Callback = PROCEDURE (n: INTEGER): BOOLEAN;

VAR
  i, j  : INTEGER;
  flag  : BOOLEAN;
  buf   : ARRAY MaxSize OF CHAR;
  root  : Node;

PROCEDURE Gcd(a, b: INTEGER): INTEGER;
VAR t: INTEGER;
BEGIN
  WHILE b # 0 DO
    t := a MOD b;
    a := b;
    b := t
  END;
  RETURN a
END Gcd;

PROCEDURE FillArray(VAR arr: ARRAY OF INTEGER; n: INTEGER);
VAR k: INTEGER;
BEGIN
  FOR k := 0 TO n - 1 DO
    arr[k] := k * k
  END
END FillArray;

PROCEDURE TestStatements;
VAR n: INTEGER; ok: BOOLEAN;
BEGIN
  (* IF / ELSIF / ELSE *)
  n := 5;
  IF n < 0 THEN
    Out.String("negative")
  ELSIF n = 0 THEN
    Out.String("zero")
  ELSE
    Out.String("positive")
  END;
  Out.Ln;

  (* CASE *)
  CASE n OF
    1: Out.String("one")
  | 2, 3: Out.String("two or three")
  | 4..9: Out.String("four to nine")
  END;
  Out.Ln;

  (* REPEAT *)
  REPEAT
    n := n - 1
  UNTIL n = 0;

  (* WHILE with ELSIF *)
  n := 10;
  WHILE n > 5 DO
    n := n - 1
  ELSIF n > 0 DO
    n := 0
  END;

  (* Set expression *)
  ok := 3 IN {1, 2, 3, 5..8};

  (* Pointer dereference and field access *)
  NEW(root);
  root^.value := 42;
  root^.next  := NIL;
  Out.String("root.value = ");
  Out.Int(root^.value, 0);
  Out.Ln
END TestStatements;

BEGIN
  i := Gcd(48, 18);
  Out.String("Gcd(48,18) = ");
  Out.Int(i, 0);
  Out.Ln;
  TestStatements
END Features.
