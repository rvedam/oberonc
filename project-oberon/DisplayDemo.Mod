MODULE DisplayDemo;
(*
 * Minimal display test: fills a rectangle and writes a serial log.
 * Used to diagnose why the Oberon display does not appear in QEMU.
 *)
IMPORT Display, Out;

BEGIN
  Out.String("DisplayDemo: base = ");
  Out.Int(Display.Base, 0);
  Out.Ln;
  Out.String("DisplayDemo: width = ");
  Out.Int(Display.Width, 0);
  Out.Ln;
  Out.String("DisplayDemo: height = ");
  Out.Int(Display.Height, 0);
  Out.Ln;

  (* Draw a white rectangle in the top-left corner *)
  Display.ReplConst(Display.white, 0, 0, 64, 64, Display.replace);
  Out.String("DisplayDemo: ReplConst done");
  Out.Ln;

  (* Draw a checkerboard in the bottom-right corner to make pixels visible *)
  Display.ReplConst(Display.white, 960, 704, 64, 64, Display.replace);
  Display.ReplConst(Display.black, 0, 704, 64, 64, Display.replace);

  Out.String("DisplayDemo: framebuffer ready — press a key to halt");
  Out.Ln
END DisplayDemo.
