MODULE TypeExporter;
  (* Exported types — these should be importable by other modules. *)
  TYPE
    Point* = RECORD
      x*, y*: INTEGER
    END;
    PointPtr* = POINTER TO Point;
    Point3D* = RECORD (Point)
      z*: INTEGER
    END;

    (* Non-exported (private) type — must NOT be visible in importing modules. *)
    PrivateHelper = RECORD val: INTEGER END;
END TypeExporter.
