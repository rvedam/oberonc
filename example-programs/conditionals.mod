MODULE Conditionals;
IMPORT Out;

PROCEDURE Do*;
BEGIN
  IF (5 > 4) THEN
  BEGIN
    Out.String("Of course, 5 is greater than 4.");
  END
  ELSE
  BEGIN
    Out.String("Error: your compiler thinks 5 is less than 4");
  END
END Do;

BEGIN
  Do();
END Conditionals;
