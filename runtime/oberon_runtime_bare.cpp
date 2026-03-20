// Oberon-07 freestanding runtime — no libc, no OS.
//
// Implements Out.*, In.*, and Oberon_NEW using only the HAL.
// BSS is guaranteed zeroed by the boot stub; heap[] lives in BSS.

#include <cstdint>
#include <cstddef>
#include "platform/hal.hpp"

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static void bare_puts(const char* s) {
    while (*s) hal_putc(*s++);
}

// Blocking character read (spin until a byte arrives).
static char blocking_getc() {
    int c;
    do {
        c = hal_getc();
#if defined(__x86_64__)
        asm volatile("pause");
#endif
    } while (c == -1);
    return static_cast<char>(c);
}

// pow10 table: pow10_table[i] = 10^i for i in [0, 44]
// Used by Out_Real and In_Real without calling libm.
static const double pow10_table[45] = {
    1e0,  1e1,  1e2,  1e3,  1e4,  1e5,  1e6,  1e7,  1e8,  1e9,
    1e10, 1e11, 1e12, 1e13, 1e14, 1e15, 1e16, 1e17, 1e18, 1e19,
    1e20, 1e21, 1e22, 1e23, 1e24, 1e25, 1e26, 1e27, 1e28, 1e29,
    1e30, 1e31, 1e32, 1e33, 1e34, 1e35, 1e36, 1e37, 1e38, 1e39,
    1e40, 1e41, 1e42, 1e43, 1e44,
};

extern "C" {

// ---------------------------------------------------------------------------
// Out module
// ---------------------------------------------------------------------------

void Out_String(const char* s) {
    if (s) bare_puts(s);
}

void Out_Char(char c) {
    hal_putc(c);
}

void Out_Ln() {
    hal_putc('\n');
}

void Out_Int(int64_t n, int64_t w) {
    char buf[21]; // INT64_MIN needs 20 digits + sign
    int  len = 0;
    bool neg = (n < 0);

    // Handle INT64_MIN safely via unsigned arithmetic
    uint64_t u = neg ? static_cast<uint64_t>(-(n + 1)) + 1u
                     : static_cast<uint64_t>(n);

    if (u == 0) {
        buf[len++] = '0';
    } else {
        while (u > 0) {
            buf[len++] = static_cast<char>('0' + u % 10);
            u /= 10;
        }
        if (neg) buf[len++] = '-';
    }

    // Reverse
    for (int i = 0, j = len - 1; i < j; ++i, --j) {
        char t = buf[i]; buf[i] = buf[j]; buf[j] = t;
    }

    // Left-pad with spaces to width w
    for (int64_t i = len; i < w; ++i) hal_putc(' ');
    for (int i = 0; i < len; ++i) hal_putc(buf[i]);
}

// Out_Real(x, w): format as D.DDDDDDEsEE (7 significant digits, Oberon style).
void Out_Real(double x, int64_t w) {
    // Build the formatted string into a local buffer, then pad + emit.
    char buf[32];
    int  blen = 0;

    auto emit = [&](char c) { if (blen < 31) buf[blen++] = c; };

    // Handle special cases via bit patterns
    uint64_t bits = 0;
    __builtin_memcpy(&bits, &x, 8);
    uint64_t exp_bits  = (bits >> 52) & 0x7FF;
    uint64_t mant_bits = bits & ((1ULL << 52) - 1);
    bool     sign      = (bits >> 63) != 0;

    if (exp_bits == 0x7FF) {
        // NaN or Infinity
        if (sign) emit('-');
        if (mant_bits != 0) { emit('N'); emit('a'); emit('N'); }
        else                { emit('I'); emit('n'); emit('f'); }
        goto pad_and_emit;
    }

    {
        if (sign) { emit('-'); x = -x; }

        if (x == 0.0) {
            // 0.000000E+00
            emit('0'); emit('.');
            for (int i = 0; i < 6; ++i) emit('0');
            emit('E'); emit('+'); emit('0'); emit('0');
            goto pad_and_emit;
        }

        // Compute decimal exponent: log10(x) ≈ (binary_exp * 30103) / 100000
        int bin_exp = static_cast<int>(exp_bits) - 1023;
        int dec_exp = static_cast<int>(
            (static_cast<int64_t>(bin_exp) * 30103) / 100000);

        // Scale x into [1.0, 10.0)
        double scaled = x;
        if (dec_exp >= 0 && dec_exp < 45) {
            scaled /= pow10_table[dec_exp];
        } else if (dec_exp < 0 && -dec_exp < 45) {
            scaled *= pow10_table[-dec_exp];
        }

        // Adjust by at most ±1 if scaling overshot
        if (scaled >= 10.0) { scaled /= 10.0; ++dec_exp; }
        if (scaled < 1.0 && scaled > 0.0) { scaled *= 10.0; --dec_exp; }

        // Extract 7 digits
        char digits[8];
        for (int i = 0; i < 7; ++i) {
            int d = static_cast<int>(scaled);
            if (d < 0) d = 0;
            if (d > 9) d = 9;
            digits[i] = static_cast<char>('0' + d);
            scaled = (scaled - static_cast<double>(d)) * 10.0;
        }

        // Format: D.DDDDDDEsEE
        emit(digits[0]); emit('.');
        for (int i = 1; i < 7; ++i) emit(digits[i]);
        emit('E');
        if (dec_exp >= 0) emit('+'); else { emit('-'); dec_exp = -dec_exp; }
        if (dec_exp < 10) {
            emit('0');
            emit(static_cast<char>('0' + dec_exp));
        } else {
            emit(static_cast<char>('0' + dec_exp / 10));
            emit(static_cast<char>('0' + dec_exp % 10));
        }
    }

pad_and_emit:
    for (int64_t i = blen; i < w; ++i) hal_putc(' ');
    for (int i = 0; i < blen; ++i) hal_putc(buf[i]);
}

// ---------------------------------------------------------------------------
// In module
// ---------------------------------------------------------------------------

void In_Char(char* c) {
    // Skip whitespace, return first non-whitespace character.
    char ch;
    do { ch = blocking_getc(); } while (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r');
    *c = ch;
}

void In_Int(int64_t* n) {
    // Skip whitespace
    char ch;
    do { ch = blocking_getc(); } while (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r');

    bool neg = (ch == '-');
    if (ch == '-' || ch == '+') ch = blocking_getc();

    int64_t val = 0;
    while (ch >= '0' && ch <= '9') {
        val = val * 10 + (ch - '0');
        ch = blocking_getc();
    }
    *n = neg ? -val : val;
}

void In_Real(double* x) {
    // Skip whitespace
    char ch;
    do { ch = blocking_getc(); } while (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r');

    bool neg = (ch == '-');
    if (ch == '-' || ch == '+') ch = blocking_getc();

    // Integer part
    double val = 0.0;
    while (ch >= '0' && ch <= '9') {
        val = val * 10.0 + static_cast<double>(ch - '0');
        ch = blocking_getc();
    }

    // Fractional part
    if (ch == '.') {
        ch = blocking_getc();
        double frac = 0.1;
        while (ch >= '0' && ch <= '9') {
            val += static_cast<double>(ch - '0') * frac;
            frac *= 0.1;
            ch = blocking_getc();
        }
    }

    // Exponent
    if (ch == 'E' || ch == 'e') {
        ch = blocking_getc();
        bool eneg = (ch == '-');
        if (ch == '-' || ch == '+') ch = blocking_getc();
        int exp = 0;
        while (ch >= '0' && ch <= '9') {
            exp = exp * 10 + (ch - '0');
            ch = blocking_getc();
        }
        if (exp < 45) {
            if (eneg) val /= pow10_table[exp];
            else      val *= pow10_table[exp];
        }
    }

    *x = neg ? -val : val;
}

// ---------------------------------------------------------------------------
// Heap allocator
// ---------------------------------------------------------------------------

static uint8_t heap[1 << 20]; // 1 MiB; zeroed by boot stub (BSS)
static size_t  heap_ptr = 0;

void* Oberon_NEW(int64_t size) {
    if (size <= 0) size = 1;
    size_t aligned = (static_cast<size_t>(size) + 15u) & ~15u;
    if (heap_ptr + aligned > sizeof(heap)) {
        bare_puts("Oberon runtime: out of memory\n");
        hal_halt();
    }
    void* p = heap + heap_ptr;
    heap_ptr += aligned;
    return p; // BSS is zeroed by boot stub
}

} // extern "C"
