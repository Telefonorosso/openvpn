/* Run the existing native SANA-II probe on the minimal AROS ROM. Its DOS-only
 * entry/logging avoids libc printf's unrelated mathieeedoubbas.library need.
 * The device calls and every pass/fail condition remain the original probe's. */
/* A HUNK starts at the first byte, not an ELF entry symbol. Put the branch
 * before NDK headers, whose variadic DOS helpers may emit out-of-line code. */
__asm__(".text\n.globl _aovpn_probe_entry\n_aovpn_probe_entry:\n jmp __start\n");
#include <exec/types.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include <stdio.h>
#include <stdarg.h>

struct ExecBase *SysBase;
struct DosLibrary *DOSBase;
int aovpn_probe_main(void);
static int probe_printf(const char *format, ...);
static int probe_puts(const char *text);

int __attribute__((no_reorder)) _start(void)
{
    BPTR log, previous;
    int result;
    SysBase = *(struct ExecBase **)4;
    DOSBase = (struct DosLibrary *)OpenLibrary((STRPTR)"dos.library", 0);
    if (!DOSBase)
        return 20;
    log = Open((STRPTR)"RunProg:vpn-device-guest.log", MODE_NEWFILE);
    if (!log) {
        Printf((STRPTR)"FAIL: guest log Open() returned DOS error %ld\n", IoErr());
        CloseLibrary((struct Library *)DOSBase);
        return 20;
    }
    previous = SelectOutput(log);
    result = aovpn_probe_main();
    SelectOutput(previous);
    if (!Close(log))
        result = 20;
    CloseLibrary((struct Library *)DOSBase);
    return result;
}

static int probe_printf(const char *format, ...)
{
    LONG result;
    char dos_format[256];
    unsigned i = 0;
    va_list args;
    /* DOS %d is 16-bit; the original C probe passes 32-bit promoted ints. */
    while (*format) {
        if (i + 4 >= sizeof(dos_format))
            return -1;
        if (format[0] == '%' && (format[1] == 'd' || format[1] == 'u')) {
            dos_format[i++] = *format++;
            dos_format[i++] = 'l';
        }
        dos_format[i++] = *format++;
    }
    dos_format[i] = 0;
    va_start(args, format);
    result = VPrintf((STRPTR)dos_format, (LONG *)args);
    va_end(args);
    return (int)result;
}

static int probe_puts(const char *text)
{
    LONG length = 0;
    while (text[length])
        length++;
    if (Write(Output(), (APTR)text, length) != length)
        return -1;
    return Write(Output(), (APTR)"\n", 1) == 1 ? 0 : -1;
}

#define printf probe_printf
#define puts probe_puts
#define main aovpn_probe_main
#include "../../src/vpn-device/tools/vpn_probe.c"
