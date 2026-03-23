MODULE SysTest;

IMPORT Out, SYSTEM;

VAR
    x, y: INTEGER;
    b: BOOLEAN;

BEGIN
    (* SIZE(T): INTEGER is 8 bytes on x86-64 *)
    Out.String("SIZE(INTEGER) = ");
    Out.Int(SYSTEM.SIZE(INTEGER), 0);
    Out.Ln;

    Out.String("SIZE(BOOLEAN) = ");
    Out.Int(SYSTEM.SIZE(BOOLEAN), 0);
    Out.Ln;

    (* PUT / GET round-trip: write 42 to x via address, read back via y *)
    x := 0;
    SYSTEM.PUT(SYSTEM.ADR(x), 42);
    SYSTEM.GET(SYSTEM.ADR(x), y);
    Out.String("PUT/GET round-trip = ");
    Out.Int(y, 0);
    Out.Ln;

    (* ADR: address of x should be non-zero *)
    Out.String("ADR(x) # 0: ");
    Out.Int(SYSTEM.ADR(x), 0);
    Out.Ln;

    (* VAL: reinterpret INTEGER as SET, then back to INTEGER *)
    x := 255;
    Out.String("VAL(INTEGER, VAL(SET, 255)) = ");
    Out.Int(SYSTEM.VAL(INTEGER, SYSTEM.VAL(SET, x)), 0);
    Out.Ln;

    (* BIT: bit 0 of 255 is TRUE, bit 8 is FALSE *)
    Out.String("BIT(ADR(x), 0) [expect 1] = ");
    b := SYSTEM.BIT(SYSTEM.ADR(x), 0);
    IF b THEN Out.Int(1, 0) ELSE Out.Int(0, 0) END;
    Out.Ln

END SysTest.
