// FULL RAPZ arm64 ptrace injector — pola rapzz-ffth (libdl.so + return-to-zero trap).
// Usage: rapzinj </absolute/path/libsheyan.so>
// Method: register hijack -> mmap() RW -> write path -> dlopen(), symbols from
// target's on-disk libc.so and libdl.so so addresses match the device exactly.
#define _GNU_SOURCE
#include <dlfcn.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

#define NT_PRSTATUS_VAL 1
#ifndef __NR_process_vm_writev
#define __NR_process_vm_writev 271
#endif

struct a64_regs {
    uint64_t regs[31];
    uint64_t sp;
    uint64_t pc;
    uint64_t pstate;
};

static pid_t g_pid = 0;

static void logln(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
    fflush(stderr);
}

static const char *sig_name(int sig) {
    switch (sig) {
        case SIGTRAP: return "SIGTRAP";
        case SIGSEGV: return "SIGSEGV";
        case SIGSTOP: return "SIGSTOP";
        case SIGBUS:  return "SIGBUS";
        case SIGILL:  return "SIGILL";
        case SIGABRT: return "SIGABRT";
        case SIGCHLD: return "SIGCHLD";
        default: return "SIG?";
    }
}

static int module_base(pid_t pid, const char *name, char *path_out, size_t path_sz) {
    char maps[64], line[1024];
    snprintf(maps, sizeof maps, "/proc/%d/maps", pid);
    FILE *f = fopen(maps, "r");
    if (!f) {
        logln("[RAPZ] buka %s gagal: %s", maps, strerror(errno));
        return 0;
    }
    uintptr_t base = 0;
    while (fgets(line, sizeof line, f)) {
        if (strstr(line, name)) {
            unsigned long long start_ll = 0, off_ll = 0;
            char perms[8], path[512] = {0};
            if (sscanf(line, "%llx-%*llx %4s %llx %*s %*s %511s",
                       &start_ll, perms, &off_ll, path) >= 3) {
                uintptr_t start = (uintptr_t)start_ll;
                uintptr_t off = (uintptr_t)off_ll;
                if (off == 0 && strstr(perms, "r")) {
                    base = start;
                    if (path_out && path[0] == '/') snprintf(path_out, path_sz, "%s", path);
                    logln("[RAPZ] module %-10s base=%llx path=%s (%s)",
                          name, (unsigned long long)base, path_out ? path_out : "-", line);
                    break;
                }
            }
        }
    }
    fclose(f);
    if (!base) logln("[RAPZ] module %s TIDAK ditemukan", name);
    return (uintptr_t)base;
}

static uintptr_t symbol_offset(const char *elf_path, const char *sym) {
    int fd = open(elf_path, O_RDONLY);
    if (fd < 0) {
        logln("[RAPZ] ELF buka gagal: %s (%s)", elf_path, strerror(errno));
        return 0;
    }
    off_t sz = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);
    if (sz <= 0 || sz > (off_t)64 * 1024 * 1024) { close(fd); return 0; }
    uint8_t *buf = (uint8_t *)malloc((size_t)sz);
    if (!buf) { close(fd); return 0; }
    ssize_t rd = read(fd, buf, (size_t)sz);
    close(fd);
    if (rd != sz) { free(buf); return 0; }

    if (buf[0] != 0x7f || buf[1] != 'E' || buf[2] != 'L' || buf[3] != 'F' || buf[4] != 2) {
        logln("[RAPZ] ELF bukan 64-bit: %s", elf_path);
        free(buf);
        return 0;
    }

    uint64_t shoff = 0;
    uint16_t shentsize = 64, shnum = 0, shstrndx = 0;
    memcpy(&shoff, buf + 0x28, 8);
    memcpy(&shentsize, buf + 0x3A, 2);
    memcpy(&shnum, buf + 0x3C, 2);
    memcpy(&shstrndx, buf + 0x3E, 2);
    if (!shentsize) shentsize = 64;

    uintptr_t result = 0;
    for (uint16_t i = 0; i < shnum; i++) {
        uint64_t p = shoff + (uint64_t)i * shentsize;
        if (p + 64 > (uint64_t)sz) continue;

        uint32_t sh_type = 0, sh_link = 0;
        uint64_t sh_off = 0, sh_size = 0, sh_entsize = 0;
        memcpy(&sh_type, buf + p + 4, 4);
        memcpy(&sh_off, buf + p + 0x18, 8);
        memcpy(&sh_size, buf + p + 0x20, 8);
        memcpy(&sh_link, buf + p + 0x28, 4);
        memcpy(&sh_entsize, buf + p + 0x38, 8);

        if (sh_type != 11 && sh_type != 2) continue;
        if (!sh_entsize) sh_entsize = 24;
        if (sh_off + sh_size > (uint64_t)sz) continue;
        if ((uint64_t)sh_link * shentsize + shoff + 64 > (uint64_t)sz) continue;

        uint64_t stroff = 0, strsz = 0;
        memcpy(&stroff, buf + shoff + (uint64_t)sh_link * shentsize + 0x18, 8);
        memcpy(&strsz, buf + shoff + (uint64_t)sh_link * shentsize + 0x20, 8);
        if (stroff + strsz > (uint64_t)sz) continue;

        size_t n = (size_t)(sh_size / sh_entsize);
        for (size_t j = 0; j < n; j++) {
            uint64_t sp = sh_off + (uint64_t)j * sh_entsize;
            if (sp + 24 > (uint64_t)sz) break;
            uint32_t nameoff = 0;
            uint64_t value = 0;
            memcpy(&nameoff, buf + sp, 4);
            memcpy(&value, buf + sp + 8, 8);
            if (!nameoff || !value) continue;
            if (nameoff >= strsz) continue;
            const char *symname = (const char *)(buf + stroff + nameoff);
            if (strcmp(symname, sym) == 0) {
                result = (uintptr_t)value;
                logln("[RAPZ] ELF sym %-8s off=%llx path=%s",
                      sym, (unsigned long long)result, elf_path);
                free(buf);
                return result;
            }
        }
    }
    free(buf);
    logln("[RAPZ] ELF sym %s TIDAK ditemukan di %s", sym, elf_path);
    return 0;
}

static int ptrace_getregs(struct a64_regs *r) {
    struct iovec io = { (void *)r, sizeof(*r) };
    return ptrace(PTRACE_GETREGSET, g_pid, (void *)NT_PRSTATUS_VAL, &io);
}

static int ptrace_setregs(struct a64_regs *r) {
    struct iovec io = { (void *)r, sizeof(*r) };
    return ptrace(PTRACE_SETREGSET, g_pid, (void *)NT_PRSTATUS_VAL, &io);
}

static int wait_call_stop(void) {
    int st = 0;
    for (;;) {
        if (waitpid(g_pid, &st, __WALL) < 0) {
            logln("[RAPZ] waitpid call gagal: %s", strerror(errno));
            return -1;
        }
        if (!WIFSTOPPED(st)) {
            logln("[RAPZ] call tidak stopped status=0x%x", st);
            return -1;
        }
        int sig = WSTOPSIG(st);
        logln("[RAPZ] call stopped: %s (%d)", sig_name(sig), sig);
        if (sig == SIGSEGV || sig == SIGTRAP || sig == SIGABRT ||
            sig == SIGILL || sig == SIGBUS) return sig;
        if (ptrace(PTRACE_CONT, g_pid, 0, (void *)(long)sig) != 0) {
            logln("[RAPZ] forward %s gagal: %s", sig_name(sig), strerror(errno));
            return -1;
        }
    }
}

static uint64_t call_func(uint64_t func, uint64_t a0, uint64_t a1, uint64_t a2,
                          uint64_t a3, uint64_t a4, uint64_t a5,
                          struct a64_regs *orig) {
    struct a64_regs r = *orig;
    r.pc = func;
    r.regs[0] = a0; r.regs[1] = a1; r.regs[2] = a2;
    r.regs[3] = a3; r.regs[4] = a4; r.regs[5] = a5;
    r.regs[30] = 0;
    r.sp &= ~0xFULL;

    if (ptrace_setregs(&r) != 0) {
        logln("[RAPZ] set regs call=%llx gagal: %s",
              (unsigned long long)func, strerror(errno));
        return 0;
    }
    if (ptrace(PTRACE_CONT, g_pid, 0, 0) != 0) {
        logln("[RAPZ] CONT call=%llx gagal: %s",
              (unsigned long long)func, strerror(errno));
        return 0;
    }
    if (wait_call_stop() < 0) return 0;

    struct a64_regs out;
    if (ptrace_getregs(&out) != 0) {
        logln("[RAPZ] get regs call=%llx gagal", (unsigned long long)func);
        return 0;
    }
    logln("[RAPZ] call %llx -> x0=%llx",
          (unsigned long long)func, (unsigned long long)out.regs[0]);
    return out.regs[0];
}

static void detach_run(void) {
    ptrace(PTRACE_CONT, g_pid, 0, 0);
    ptrace(PTRACE_DETACH, g_pid, 0, 0);
}

static pid_t find_pid(const char *target) {
    DIR *d = opendir("/proc");
    if (!d) return -1;
    pid_t exact = -1, fallback = -1;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
        char path[256];
        snprintf(path, sizeof path, "/proc/%s/cmdline", e->d_name);
        int fd = open(path, O_RDONLY);
        if (fd < 0) continue;
        char buf[1024] = {0};
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n <= 0) continue;
        buf[n] = 0;
        if (!strstr(buf, target)) continue;
        pid_t pid = (pid_t)atoi(e->d_name);
        logln("[RAPZ] kandidat pid=%d cmdline=%s", pid, buf);
        if (!strcmp(buf, target)) { exact = pid; break; }
        if (fallback < 0) fallback = pid;
    }
    closedir(d);
    pid_t chosen = exact > 0 ? exact : fallback;
    logln("[RAPZ] pid terpilih=%d (%s)", chosen,
          exact > 0 ? "exact" : (fallback > 0 ? "partial" : "none"));
    return chosen;
}

static int has_mapping(pid_t pid, const char *needle) {
    char path[64];
    snprintf(path, sizeof path, "/proc/%d/maps", pid);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[1024];
    int found = 0;
    while (fgets(line, sizeof line, f)) {
        if (strstr(line, needle)) {
            logln("[RAPZ] mapping: %s", line);
            found = 1;
            break;
        }
    }
    fclose(f);
    return found;
}

int main(int argc, char **argv) {
    const char *so = argc > 1 ? argv[1] : "/data/local/tmp/rap/libsheyan.so";
    const char *pkg = "com.dts.freefireadv";
    setvbuf(stderr, NULL, _IONBF, 0);
    setvbuf(stdout, NULL, _IONBF, 0);

    logln("[RAPZ] build=rapz-v14 === start ===");
    logln("[RAPZ] uid=%d pid_self=%d lib=%s target=%s",
          (int)getuid(), (int)getpid(), so, pkg);

    struct stat st;
    if (stat(so, &st) != 0) {
        logln("[RAPZ] payload tidak ada: %s (%s)", so, strerror(errno));
        return 1;
    }
    logln("[RAPZ] payload size=%lld mode=%o",
          (long long)st.st_size, (unsigned)st.st_mode & 0777);

    if (argc > 1 && !strcmp(argv[1], "--unattach")) {
        pid_t up = find_pid(pkg);
        if (up > 0) {
            ptrace(PTRACE_DETACH, up, 0, 0);
            logln("[RAPZ] detach ptrace pid=%d (unattach)", up);
        } else {
            logln("[RAPZ] game tidak jalan, tidak ada yang di-detach");
        }
        return 0;
    }

    pid_t pid = find_pid(pkg);
    if (pid < 0) {
        logln("[RAPZ] game belum jalan / cmdline tidak cocok");
        return 1;
    }
    g_pid = pid;

    for (int i = 0; i < 60; i++) {
        if (has_mapping(pid, "libil2cpp.so")) break;
        if (i % 5 == 0) logln("[RAPZ] tunggu libil2cpp.so... %d/60", i);
        sleep(1);
    }
    if (!has_mapping(pid, "libil2cpp.so")) {
        logln("[RAPZ] libil2cpp.so tidak muncul setelah 60 detik");
        return 2;
    }

    if (ptrace(PTRACE_ATTACH, g_pid, 0, 0) < 0) {
        logln("[RAPZ] attach gagal: %s", strerror(errno));
        return 5;
    }
    { int st2; waitpid(g_pid, &st2, __WALL); }
    logln("[RAPZ] attached");

    // Baca semua base DARI SATU snapshot maps yang sama, setelah attach.
    char libc_path[512] = {0}, libdl_path[512] = {0};
    uintptr_t libc_base = module_base(pid, "libc.so", libc_path, sizeof libc_path);
    uintptr_t libdl_base = module_base(pid, "libdl.so", libdl_path, sizeof libdl_path);
    if (!libc_base || !libdl_base) {
        logln("[RAPZ] base libc/libdl tidak lengkap libc=%llx libdl=%llx",
              (unsigned long long)libc_base, (unsigned long long)libdl_base);
        detach_run();
        return 3;
    }
    if (!libc_path[0]) snprintf(libc_path, sizeof libc_path, "/system/lib64/libc.so");
    if (!libdl_path[0]) snprintf(libdl_path, sizeof libdl_path, "/system/lib64/libdl.so");

    uintptr_t off_mmap = symbol_offset(libc_path, "mmap");
    uintptr_t off_dlopen = symbol_offset(libdl_path, "dlopen");
    if (!off_mmap || !off_dlopen) {
        logln("[RAPZ] symbol resolve gagal mmap=%llx dlopen=%llx libc=%s libdl=%s",
              (unsigned long long)off_mmap, (unsigned long long)off_dlopen,
              libc_path, libdl_path);
        detach_run();
        return 4;
    }

    uint64_t mmap_addr = libc_base + off_mmap;
    uint64_t dlopen_addr = libdl_base + off_dlopen;
    logln("[RAPZ] mmap=%llx dlopen=%llx libcbase=%llx libdlbase=%llx off_mmap=%llx off_dlopen=%llx",
          (unsigned long long)mmap_addr, (unsigned long long)dlopen_addr,
          (unsigned long long)libc_base, (unsigned long long)libdl_base,
          (unsigned long long)off_mmap, (unsigned long long)off_dlopen);

    struct a64_regs orig;
    if (ptrace_getregs(&orig) < 0) {
        logln("[RAPZ] GETREGSET awal gagal: %s", strerror(errno));
        detach_run();
        return 6;
    }
    logln("[RAPZ] regs pc=%llx sp=%llx",
          (unsigned long long)orig.pc, (unsigned long long)orig.sp);

    uint64_t mem = call_func(mmap_addr, 0, 0x2000, 3, 0x22, (uint64_t)-1, 0, &orig);
    if ((int64_t)mem <= 0) {
        logln("[RAPZ] mmap target gagal: %llx", (unsigned long long)mem);
        ptrace_setregs(&orig);
        detach_run();
        return 7;
    }
    logln("[RAPZ] target mmap=%llx", (unsigned long long)mem);

    char path[512];
    snprintf(path, sizeof path, "%s", so);
    size_t plen = strlen(path) + 1;
    struct iovec local = { (void *)path, plen };
    struct iovec remote = { (void *)mem, plen };
    if (syscall(__NR_process_vm_writev, g_pid, &local, 1, &remote, 1, 0) != (ssize_t)plen) {
        logln("[RAPZ] process_vm_writev gagal: %s", strerror(errno));
        ptrace_setregs(&orig);
        detach_run();
        return 8;
    }
    char verify[512] = {0};
    struct iovec rlocal = { verify, sizeof(verify) - 1 };
    struct iovec rremote = { (void *)mem, sizeof(verify) - 1 };
    if (syscall(__NR_process_vm_readv, g_pid, &rlocal, 1, &rremote, 1, 0) > 0) {
        logln("[RAPZ] path terverifikasi: %s", verify);
    }

    uint64_t handle = call_func(dlopen_addr, mem, 2 /*RTLD_NOW*/, 0, 0, 0, 0, &orig);
    logln("[RAPZ] dlopen handle=%llx", (unsigned long long)handle);

    ptrace_setregs(&orig);
    detach_run();

    logln("[RAPZ] pid=%d handle=%llx result=%s", pid,
          (unsigned long long)handle, handle ? "SUCCESS" : "FAILED");
    return handle ? 0 : 9;
}
