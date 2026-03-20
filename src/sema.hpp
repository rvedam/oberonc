#pragma once
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

// -----------------------------------------------------------------------
// Oberon-07 semantic type representation.
// Used by the code generator; not part of the parser.
// -----------------------------------------------------------------------

enum class TypeKind {
    Integer,   // INTEGER  (i64)
    Real,      // REAL     (double)
    Boolean,   // BOOLEAN  (i1)
    Char,      // CHAR     (i8)
    Byte,      // BYTE     (i8)
    Set,       // SET      (i64 bitmask)
    Nil,       // type of NIL literal
    String,    // string literal / ARRAY OF CHAR constant
    Array,     // ARRAY [N] OF T  (isOpen=true when ARRAY OF T)
    Record,    // RECORD … END
    Pointer,   // POINTER TO T
    Procedure, // PROCEDURE [FormalParameters]
};

struct OberonType;
using OberonTypePtr = std::shared_ptr<OberonType>;

struct RecordField {
    std::string   name;
    OberonTypePtr type;
};

struct ProcParam {
    bool          isVar = false;
    std::string   name;
    OberonTypePtr type;
};

struct OberonType {
    TypeKind    kind;
    std::string name;          // human-readable / debug name

    // ---- Array ----
    bool          isOpen = false; // ARRAY OF T (no length)
    int64_t       length = 0;
    OberonTypePtr elem;

    // ---- Record ----
    std::vector<RecordField> fields;
    std::string              baseName;   // record extension base
    std::string              baseModule;

    // ---- Pointer ----
    OberonTypePtr ptrBase;
    std::string   ptrBaseName;   // forward-reference (resolved later)

    // ---- Procedure ----
    std::vector<ProcParam> params;
    OberonTypePtr          retType; // nullptr = void

    // Helper: index of field by name (-1 if not found)
    int fieldIndex(const std::string& n) const {
        for (int i = 0; i < static_cast<int>(fields.size()); ++i)
            if (fields[i].name == n) return i;
        return -1;
    }

    bool isScalar() const {
        return kind == TypeKind::Integer || kind == TypeKind::Real  ||
               kind == TypeKind::Boolean || kind == TypeKind::Char  ||
               kind == TypeKind::Byte    || kind == TypeKind::Set;
    }
};
