MODULE KernelDemo;
(*
 * Bare-metal x86-64 demo that exercises Project Oberon's Kernel module.
 * Initialises the sector map, marks two sectors, allocates a free sector,
 * and prints a summary over the serial port / VGA via Out.
 *)

IMPORT Kernel, Out;

VAR sec: INTEGER;

BEGIN
  Kernel.InitSecMap;           (* zero the sector bitmap *)
  Kernel.MarkSector(29);       (* mark logical sector 1 as used *)
  Kernel.MarkSector(58);       (* mark logical sector 2 as used *)

  Out.String("Project Oberon Kernel on x86-64");
  Out.Ln;
  Out.String("InitSecMap + MarkSector: OK");
  Out.Ln;

  Kernel.AllocSector(0, sec);  (* find first free sector *)
  Out.String("AllocSector → sec = ");
  Out.Int(sec, 0);
  Out.Ln
END KernelDemo.
