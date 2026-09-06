#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>

char *program_invocation_name = "atshogi-engine";
char *program_invocation_short_name = "atshogi-engine";

int* __errno_location(void) {
    return __errno();
}

void __assert_fail(const char *assertion, const char *file, unsigned int line, const char *function) {
    fprintf(stderr, "Assertion failed: %s (%s: %u: %s)\n", assertion, file, line, function ? function : "");
    abort();
}

int __fprintf_chk(FILE *stream, int flag, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vfprintf(stream, format, ap);
    va_end(ap);
    return ret;
}

int __printf_chk(int flag, const char *format, ...) {
    va_list ap;
    va_start(ap, format);
    int ret = vprintf(format, ap);
    va_end(ap);
    return ret;
}

int __vfprintf_chk(FILE *stream, int flag, const char *format, va_list ap) {
    return vfprintf(stream, format, ap);
}

int __fdelt_chk(int d) {
    if (d < 0 || d >= 1024) {
        fprintf(stderr, "fd_set overflow\n");
        abort();
    }
    return d / 64;
}

const unsigned short **__ctype_b_loc(void) {
    static const unsigned short ctype_b[384] = {0};
    static const unsigned short *p = ctype_b + 128;
    return &p;
}

long int __isoc23_strtol(const char *nptr, char **endptr, int base) {
    return strtol(nptr, endptr, base);
}

unsigned long long int __isoc23_strtoull(const char *nptr, char **endptr, int base) {
    return strtoull(nptr, endptr, base);
}

ssize_t __getdelim(char **lineptr, size_t *n, int delimiter, FILE *stream) {
    return getdelim(lineptr, n, delimiter, stream);
}

int dlinfo(void *handle, int request, void *info) {
    return -1;
}

// Android Bionic compatibility: Force UTF-8 codeset to prevent GHC mkTextEncoding unknown encoding crash
#include <locale.h>
#include <langinfo.h>

char *nl_langinfo(nl_item item) {
    (void)item;
    return "UTF-8";
}

char *nl_langinfo_l(nl_item item, locale_t loc) {
    (void)item;
    (void)loc;
    return "UTF-8";
}
