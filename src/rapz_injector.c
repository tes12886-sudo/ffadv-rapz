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
#include <sys/syscall.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

#define PT_PEEKDATA 2
#define PT_POKEDATA 5
#define PT_GETREGS 12
#define PT_SETREGS 13
#define PTRACE_GETREGSET_VAL 0x4204
#define PTRACE_SETREGSET_VAL 0x4205
#define PTRACE_SEIZE_VAL 0x4206
#define PTRACE_INTERRUPT_VAL 0x4207
#define NT_PRSTATUS 1

#define RTLD_NOW_VAL   0x00000002UL
#define RTLD_GLOBAL_VAL 0x00000100UL
#define STUB_OFF 0x80UL

#define PROT_RWX 7
#define PROT_RX  5
#define MAP_PRIVATE_ANON 0x22

#ifndef __NR_mmap
#define __NR_mmap 222
#endif
#ifndef __NR_mprotect
#define __NR_mprotect 226
#endif
#ifndef __NR_process_vm_readv
#define __NR_process_vm_readv 270
#endif
#ifndef __NR_process_vm_writev
#define __NR_process_vm_writev 271
#endif

struct rapz_regs {
    uint64_t x[31];
    uint64_t sp;
    uint64_t pc;
    uint64_t pstate;
};

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
        case SIGBUS: return "SIGBUS";
        case SIGILL: return "SIGILL";
        default: return "SIG?";
    }
}

static int get_regs(pid_t pid, struct rapz_regs *r) {
    memset(r, 0, sizeof(*r));
    if (ptrace(PT_GETREGS, pid, 0, r) == 0) {
        logln("[RAPZ] GETREGS OK");
        return 0;
    }
    logln("[RAPZ] GETREGS gagal: %s, coba GETREGSET...", strerror(errno));

    struct iovec io = { (void *)r, sizeof(*r) };
    if (ptrace(PTRACE_GETREGSET_VAL, pid, (void *)NT_PRSTATUS, &io) == 0) {
        logln("[RAPZ] GETREGSET OK iov_len=%zu", (size_t)io.iov_len);
        return 0;
    }
    logln("[RAPZ] GETREGSET gagal: %s", strerror(errno));
    return -1;
}

static int set_regs(pid_t pid, const struct rapz_regs *r) {
    if (ptrace(PT_SETREGS, pid, 0, (void *)r) == 0) {
        logln("[RAPZ] SETREGS OK");
        return 0;
    }
    logln("[RAPZ] SETREGS gagal: %s, coba SETREGSET...", strerror(errno));

    struct iovec io = { (void *)r, sizeof(*r) };
    if (ptrace(PTRACE_SETREGSET_VAL, pid, (void *)NT_PRSTATUS, &io) == 0) {
        logln("[RAPZ] SETREGSET OK iov_len=%zu", (size_t)io.iov_len);
        return 0;
    }
    logln("[RAPZ] SETREGSET gagal: %s", strerror(errno));
    return -1;
}

static void log_status(const char *tag, int status) {
    if (WIFSTOPPED(status)) {
        logln("[RAPZ][%s] stopped: %s (%d)", tag, sig_name(WSTOPSIG(status)), WSTOPSIG(status));
    } else if (WIFEXITED(status)) {
        logln("[RAPZ][%s] exited: %d", tag, WEXITSTATUS(status));
    } else if (WIFSIGNALED(status)) {
        logln("[RAPZ][%s] killed: %s (%d)", tag, sig_name(WTERMSIG(status)), WTERMSIG(status));
    } else {
        logln("[RAPZ][%s] status=0x%x", tag, status);
    }
}

static pid_t find_pid(const char *target) {
    DIR *d = opendir("/proc");
    if (!d) {
        logln("[RAPZ] opendir(/proc) gagal: %s", strerror(errno));
        return -1;
    }

    pid_t exact = -1;
    pid_t fallback = -1;
    struct dirent *e;

    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;

        char path[256];
        snprintf(path, sizeof(path), "/proc/%s/cmdline", e->d_name);
        int fd = open(path, O_RDONLY);
        if (fd < 0) continue;

        char buf[1024] = {0};
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n <= 0) continue;
        buf[n] = '\0';

        if (strstr(buf, target) == NULL) continue;

        pid_t pid = (pid_t)atoi(e->d_name);
        logln("[RAPZ] kandidat pid=%d cmdline=%s", pid, buf);

        if (strcmp(buf, target) == 0) {
            exact = pid;
            break;
        }
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
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);
    FILE *f = fopen(path, "r");
    if (!f) {
        logln("[RAPZ] buka %s gagal: %s", path, strerror(errno));
        return 0;
    }

    char line[1024];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, needle)) {
            logln("[RAPZ] mapping: %s", line);
            found = 1;
            break;
        }
    }
    fclose(f);
    return found;
}

static void copy_map_path(const char *line, char *out, size_t cap) {
    if (!out || cap == 0) return;
    const char *p = strstr(line, "/");
    size_t i = 0;
    if (!p) { out[0] = 0; return; }
    while (*p && *p != '\n' && *p != '\r' && *p != ' ' && i + 1 < cap) {
        out[i++] = *p++;
    }
    out[i] = 0;
}

static uintptr_t libc_base(pid_t pid, const char *label, char *path_out, size_t path_cap) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);
    FILE *f = fopen(path, "r");
    if (!f) {
        logln("[RAPZ] buka %s gagal: %s", path, strerror(errno));
        return 0;
    }

    char line[1024];
    uintptr_t base = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "/libc.so") && strstr(line, "r--p") && strstr(line, " 00000000 ")) {
            if (sscanf(line, "%lx-", (unsigned long *)&base) == 1) {
                copy_map_path(line, path_out, path_cap);
                logln("[RAPZ] %s libc base=%llx path=%s (%s)", label,
                      (unsigned long long)base, path_out, line);
                break;
            }
        }
    }

    if (!base) {
        rewind(f);
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "/libc.so")) {
                if (sscanf(line, "%lx-", (unsigned long *)&base) == 1) {
                    copy_map_path(line, path_out, path_cap);
                    logln("[RAPZ] %s libc base fallback=%llx path=%s (%s)", label,
                          (unsigned long long)base, path_out, line);
                    break;
                }
            }
        }
    }

    fclose(f);
    if (!base) logln("[RAPZ] %s libc base TIDAK ditemukan", label);
    return base;
}

static uintptr_t module_base(pid_t pid, const char *label, const char *needle,
                             char *path_out, size_t path_cap) {
    char maps[64];
    snprintf(maps, sizeof(maps), "/proc/%d/maps", pid);
    FILE *f = fopen(maps, "r");
    if (!f) {
        logln("[RAPZ] buka %s gagal: %s", maps, strerror(errno));
        return 0;
    }

    char line[1024];
    uintptr_t base = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, needle) && strstr(line, "r--p") && strstr(line, " 00000000 ")) {
            if (sscanf(line, "%lx-", (unsigned long *)&base) == 1) {
                copy_map_path(line, path_out, path_cap);
                logln("[RAPZ] %s base=%llx path=%s (%s)", label,
                      (unsigned long long)base, path_out, line);
                break;
            }
        }
    }
    if (!base) {
        rewind(f);
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, needle)) {
                if (sscanf(line, "%lx-", (unsigned long *)&base) == 1) {
                    copy_map_path(line, path_out, path_cap);
                    logln("[RAPZ] %s base fallback=%llx path=%s (%s)", label,
                          (unsigned long long)base, path_out, line);
                    break;
                }
            }
        }
    }
    fclose(f);

    if (!path_out || !path_out[0]) {
        const char *candidates[] = {
            "/system/bin/linker64",
            "/apex/com.android.runtime/bin/linker64"
        };
        for (unsigned i = 0; i < 2; i++) {
            struct stat s;
            if (stat(candidates[i], &s) == 0) {
                snprintf(path_out, path_cap, "%s", candidates[i]);
                logln("[RAPZ] %s path fallback=%s", label, path_out);
                break;
            }
        }
    }
    if (!base) logln("[RAPZ] %s base TIDAK ditemukan", label);
    return base;
}

static long peek_word(pid_t pid, uintptr_t addr) {
    errno = 0;
    return ptrace(PT_PEEKDATA, pid, (void *)addr, 0);
}

static int read_mem(pid_t pid, uintptr_t addr, void *dst, size_t len) {
    struct iovec local = { (void *)dst, len };
    struct iovec remote = { (void *)addr, len };
    ssize_t n = (ssize_t)syscall(__NR_process_vm_readv, pid, &local, 1, &remote, 1, 0);
    if (n == (ssize_t)len) return 0;
    logln("[RAPZ] process_vm_readv gagal: n=%zd err=%s", n, strerror(errno));
    return -1;
}

static int write_mem(pid_t pid, uintptr_t addr, const void *data, size_t len) {
    struct iovec local = { (void *)data, len };
    struct iovec remote = { (void *)addr, len };
    ssize_t n = (ssize_t)syscall(__NR_process_vm_writev, pid, &local, 1, &remote, 1, 0);
    if (n == (ssize_t)len) {
        logln("[RAPZ] write_mem vm_writev OK addr=%llx len=%zu",
              (unsigned long long)addr, len);
        return 0;
    }

    logln("[RAPZ] process_vm_writev gagal: n=%zd err=%s, fallback ptrace",
          n, strerror(errno));

    const unsigned char *src = (const unsigned char *)data;
    size_t off = 0;
    while (off < len) {
        uintptr_t cur = addr + off;
        uintptr_t aligned = cur & ~7ULL;
        size_t in = (size_t)(cur - aligned);
        size_t chunk = 8 - in;
        if (chunk > len - off) chunk = len - off;

        uint64_t word = 0;
        long old = peek_word(pid, aligned);
        if (old != -1 || errno == 0) word = (uint64_t)old;
        memcpy((unsigned char *)&word + in, src + off, chunk);

        if (ptrace(PT_POKEDATA, pid, (void *)aligned, (void *)word) != 0) {
            logln("[RAPZ] POKEDATA gagal addr=%llx: %s",
                  (unsigned long long)aligned, strerror(errno));
            return -1;
        }
        off += chunk;
    }
    return 0;
}

static uint16_t rd16(const unsigned char *p) {
    return (uint16_t)(p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t rd32(const unsigned char *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t rd64(const unsigned char *p) {
    return (uint64_t)rd32(p) | ((uint64_t)rd32(p + 4) << 32);
}

static uintptr_t elf_symbol_offset(const char *path, const char *name) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        logln("[RAPZ] ELF buka gagal: %s (%s)", path, strerror(errno));
        return 0;
    }
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return 0; }
    long fsz = ftell(f);
    rewind(f);
    if (fsz < 64) { fclose(f); return 0; }

    unsigned char *buf = (unsigned char *)malloc((size_t)fsz);
    if (!buf) { fclose(f); return 0; }
    if (fread(buf, 1, (size_t)fsz, f) != (size_t)fsz) {
        free(buf); fclose(f); return 0;
    }
    fclose(f);

    if (buf[0] != 0x7f || buf[1] != 'E' || buf[2] != 'L' || buf[3] != 'F' || buf[4] != 2) {
        logln("[RAPZ] ELF bukan 64-bit: %s", path);
        free(buf);
        return 0;
    }

    uint64_t shoff = rd64(buf + 0x28);
    uint16_t shentsize = rd16(buf + 0x3A);
    uint16_t shnum = rd16(buf + 0x3C);
    if (!shentsize) shentsize = 64;

    const uint32_t wanted[] = { 11, 2 };
    for (unsigned w = 0; w < 2; w++) {
        for (uint16_t i = 0; i < shnum; i++) {
            uint64_t shp = shoff + (uint64_t)i * shentsize;
            if (shp + 64 > (uint64_t)fsz) continue;

            const unsigned char *sh = buf + shp;
            uint32_t type = rd32(sh + 4);
            if (type != wanted[w]) continue;

            uint32_t stridx = rd32(sh + 0x28);
            if ((uint64_t)stridx * shentsize + shoff + 64 > (uint64_t)fsz) continue;
            const unsigned char *strsh = buf + shoff + (uint64_t)stridx * shentsize;
            uint64_t stroff = rd64(strsh + 0x18);
            uint64_t strsz = rd64(strsh + 0x20);
            if (stroff + strsz > (uint64_t)fsz) continue;

            uint64_t symoff = rd64(sh + 0x18);
            uint64_t symsz = rd64(sh + 0x20);
            uint64_t entsize = rd64(sh + 0x38);
            if (!entsize) entsize = 24;
            if (symoff + symsz > (uint64_t)fsz) continue;

            uint64_t count = symsz / entsize;
            for (uint64_t j = 0; j < count; j++) {
                const unsigned char *sym = buf + symoff + j * entsize;
                uint32_t nameoff = rd32(sym);
                if (nameoff >= strsz) continue;
                const char *symname = (const char *)(buf + stroff + nameoff);
                if (strcmp(symname, name) == 0) {
                    uint64_t off = rd64(sym + 8);
                    logln("[RAPZ] ELF sym %-8s off=%llx path=%s",
                          name, (unsigned long long)off, path);
                    free(buf);
                    return (uintptr_t)off;
                }
            }
        }
    }

    logln("[RAPZ] ELF sym %s TIDAK ditemukan di %s", name, path);
    free(buf);
    return 0;
}

static uintptr_t find_svc_gadget(pid_t pid, const char *libc_path) {
    char maps[64];
    snprintf(maps, sizeof(maps), "/proc/%d/maps", pid);
    FILE *f = fopen(maps, "r");
    if (!f) {
        logln("[RAPZ] buka %s gagal: %s", maps, strerror(errno));
        return 0;
    }

    uintptr_t start = 0, end = 0;
    char line[1024];
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, libc_path) && strstr(line, "r-xp")) {
            if (sscanf(line, "%lx-%lx", (unsigned long *)&start, (unsigned long *)&end) == 2) break;
        }
    }
    if (!start) {
        rewind(f);
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "/libc.so") && strstr(line, "r-xp")) {
                sscanf(line, "%lx-%lx", (unsigned long *)&start, (unsigned long *)&end);
                break;
            }
        }
    }
    fclose(f);

    if (!start || end <= start) {
        logln("[RAPZ] segmen r-xp libc tidak ditemukan");
        return 0;
    }
    logln("[RAPZ] libc r-xp=%llx-%llx", (unsigned long long)start, (unsigned long long)end);

    const size_t CHUNK = 0x8000;
    unsigned char *buf = (unsigned char *)malloc(CHUNK + 4);
    if (!buf) return 0;

    for (uintptr_t addr = start; addr + 4 <= end; addr += CHUNK) {
        size_t len = end - addr;
        if (len > CHUNK) len = CHUNK;
        if (read_mem(pid, addr, buf, len) != 0) continue;
        for (size_t i = 0; i + 4 <= len; i += 4) {
            if (buf[i] == 0x01 && buf[i + 1] == 0x00 &&
                buf[i + 2] == 0x00 && buf[i + 3] == 0xd4) {
                uintptr_t gadget = addr + i;
                logln("[RAPZ] svc#0 gadget=%llx bytes=01 00 00 d4",
                      (unsigned long long)gadget);
                free(buf);
                return gadget;
            }
        }
    }
    free(buf);

    for (uintptr_t a = start; a + 4 <= end; a += 4) {
        long w = peek_word(pid, a);
        if ((uint32_t)w == 0xd4000001U) {
            logln("[RAPZ] svc#0 gadget fallback=%llx", (unsigned long long)a);
            return a;
        }
    }

    logln("[RAPZ] svc#0 gadget TIDAK ditemukan");
    return 0;
}

static uintptr_t remote_syscall(pid_t pid, const struct rapz_regs *original,
                                uintptr_t gadget, uint64_t nr,
                                uint64_t a0, uint64_t a1, uint64_t a2,
                                uint64_t a3, uint64_t a4, uint64_t a5) {
    struct rapz_regs r = *original;
    r.x[0] = a0; r.x[1] = a1; r.x[2] = a2;
    r.x[3] = a3; r.x[4] = a4; r.x[5] = a5;
    r.x[8] = nr;
    r.pc = gadget;

    if (set_regs(pid, &r) != 0) {
        logln("[RAPZ] set regs syscall nr=%llu gagal", (unsigned long long)nr);
        return 0;
    }
    logln("[RAPZ] remote syscall nr=%llu gadget=%llx",
          (unsigned long long)nr, (unsigned long long)gadget);

    int entry_ok = 0;
    for (int i = 0; i < 4; i++) {
        if (ptrace(PTRACE_SYSCALL, pid, 0, 0) != 0) {
            logln("[RAPZ] PTRACE_SYSCALL entry gagal: %s", strerror(errno));
            return 0;
        }
        int status = 0;
        if (waitpid(pid, &status, 0) < 0) {
            logln("[RAPZ] waitpid syscall entry gagal: %s", strerror(errno));
            return 0;
        }
        log_status("syscall-entry", status);
        if (get_regs(pid, &r) != 0) return 0;
        logln("[RAPZ] syscall-entry pc=%llx x8=%llu",
              (unsigned long long)r.pc, (unsigned long long)r.x[8]);
        if (r.x[8] == nr) { entry_ok = 1; break; }
    }
    if (!entry_ok) {
        logln("[RAPZ] syscall entry nr=%llu tidak ketemu", (unsigned long long)nr);
        return 0;
    }

    if (ptrace(PTRACE_SYSCALL, pid, 0, 0) != 0) {
        logln("[RAPZ] PTRACE_SYSCALL exit gagal: %s", strerror(errno));
        return 0;
    }
    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        logln("[RAPZ] waitpid syscall exit gagal: %s", strerror(errno));
        return 0;
    }
    log_status("syscall-exit", status);
    if (get_regs(pid, &r) != 0) return 0;

    uintptr_t result = (uintptr_t)r.x[0];
    logln("[RAPZ] syscall nr=%llu result=%llx",
          (unsigned long long)nr, (unsigned long long)result);
    return result;
}

static int is_syscall_error(uintptr_t v) {
    return ((int64_t)v < 0 && v > (uintptr_t)-4096);
}

static uintptr_t remote_mmap(pid_t pid, const struct rapz_regs *original, uintptr_t gadget) {
    uintptr_t mem = remote_syscall(pid, original, gadget, __NR_mmap,
                                   0, 0x1000, PROT_RWX, MAP_PRIVATE_ANON,
                                   (uint64_t)-1, 0);
    if (!is_syscall_error(mem) && mem >= 0x10000) {
        logln("[RAPZ] mmap RWX berhasil: %llx", (unsigned long long)mem);
        return mem;
    }

    int err = (int)-(int64_t)mem;
    logln("[RAPZ] mmap RWX gagal errno=%d, coba RW...", err);
    mem = remote_syscall(pid, original, gadget, __NR_mmap,
                         0, 0x1000, 3, MAP_PRIVATE_ANON,
                         (uint64_t)-1, 0);
    if (is_syscall_error(mem) || mem < 0x10000) {
        logln("[RAPZ] mmap RW gagal result=%llx", (unsigned long long)mem);
        return 0;
    }
    logln("[RAPZ] mmap RW berhasil: %llx", (unsigned long long)mem);
    return mem;
}

static uintptr_t remote_call(pid_t pid, const struct rapz_regs *original, uintptr_t fn,
                             uintptr_t a0, uintptr_t a1, uintptr_t a2,
                             uintptr_t a3, uintptr_t a4, uintptr_t a5,
                             uintptr_t trap) {
    struct rapz_regs r = *original;
    r.x[0] = a0; r.x[1] = a1; r.x[2] = a2;
    r.x[3] = a3; r.x[4] = a4; r.x[5] = a5;
    r.x[8] = 0;
    r.x[30] = trap;
    r.pc = fn;

    if (set_regs(pid, &r) != 0) {
        logln("[RAPZ] set regs call fn=%llx gagal", (unsigned long long)fn);
        return 0;
    }

    if (ptrace(PTRACE_CONT, pid, 0, 0) != 0) {
        logln("[RAPZ] CONT call fn=%llx gagal: %s",
              (unsigned long long)fn, strerror(errno));
        return 0;
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        logln("[RAPZ] waitpid call gagal: %s", strerror(errno));
        return 0;
    }
    log_status("remote-call", status);

    if (get_regs(pid, &r) != 0) {
        logln("[RAPZ] get regs call gagal");
        return 0;
    }
    return r.x[0];
}

static int ptrace_attach_verbose(pid_t pid, int *status) {
    if (ptrace(PTRACE_ATTACH, pid, 0, 0) == 0) {
        logln("[RAPZ] PTRACE_ATTACH berhasil");
    } else {
        logln("[RAPZ] PTRACE_ATTACH gagal: %s", strerror(errno));
        logln("[RAPZ] coba PTRACE_SEIZE...");
        if (ptrace(PTRACE_SEIZE_VAL, pid, 0, 0) != 0) {
            logln("[RAPZ] PTRACE_SEIZE gagal: %s", strerror(errno));
            return -1;
        }
        logln("[RAPZ] PTRACE_SEIZE berhasil, kirim INTERRUPT");
        if (ptrace(PTRACE_INTERRUPT_VAL, pid, 0, 0) != 0) {
            logln("[RAPZ] PTRACE_INTERRUPT gagal: %s", strerror(errno));
            return -1;
        }
    }

    if (waitpid(pid, status, 0) < 0) {
        logln("[RAPZ] waitpid attach gagal: %s", strerror(errno));
        return -1;
    }
    log_status("attach", *status);
    return 0;
}

int main(int argc, char **argv) {
    const char *lib = argc > 1 ? argv[1] : "/data/local/tmp/rap/libsheyan.so";
    const char *pkg = "com.dts.freefireadv";

    logln("[RAPZ] build=rapz-v10 === start ===");
    logln("[RAPZ] uid=%d pid_self=%d lib=%s target=%s",
          (int)getuid(), (int)getpid(), lib, pkg);

    struct stat st;
    if (stat(lib, &st) != 0) {
        logln("[RAPZ] payload tidak ada: %s (%s)", lib, strerror(errno));
        return 10;
    }
    logln("[RAPZ] payload size=%lld mode=%o",
          (long long)st.st_size, (unsigned)st.st_mode & 0777);

    pid_t pid = find_pid(pkg);
    if (pid < 0) {
        logln("[RAPZ] game belum jalan / cmdline tidak cocok");
        return 1;
    }

    for (int i = 0; i < 60; i++) {
        if (has_mapping(pid, "libil2cpp.so")) break;
        if (i % 5 == 0) logln("[RAPZ] tunggu libil2cpp.so... %d/60", i);
        sleep(1);
    }
    if (!has_mapping(pid, "libil2cpp.so")) {
        logln("[RAPZ] libil2cpp.so tidak muncul setelah 60 detik");
        return 2;
    }

    int status = 0;
    if (ptrace_attach_verbose(pid, &status) != 0) {
        logln("[RAPZ] attach gagal, kemungkinan anti-debug / SELinux / Yama");
        return 3;
    }

    struct rapz_regs original;
    if (get_regs(pid, &original) != 0) {
        logln("[RAPZ] get regs awal gagal");
        ptrace(PTRACE_DETACH, pid, 0, 0);
        return 4;
    }
    logln("[RAPZ] regs pc=%llx sp=%llx",
          (unsigned long long)original.pc, (unsigned long long)original.sp);

    char libc_path[512] = {0};
    uintptr_t libc_remote = libc_base(pid, "remote", libc_path, sizeof(libc_path));
    if (!libc_remote || !libc_path[0]) {
        ptrace(PTRACE_DETACH, pid, 0, 0);
        return 5;
    }

    uintptr_t off_mmap = elf_symbol_offset(libc_path, "mmap");
    uintptr_t off_dlopen = 0;
    uintptr_t off_dlerror = 0;
    uintptr_t dl_base = 0;

    char linker_path[512] = {0};
    uintptr_t linker_remote = module_base(pid, "linker", "linker64",
                                          linker_path, sizeof(linker_path));
    if (linker_remote && linker_path[0]) {
        off_dlopen = elf_symbol_offset(linker_path, "dlopen");
        off_dlerror = elf_symbol_offset(linker_path, "dlerror");
        if (off_dlopen) dl_base = linker_remote;
    }
    if (!off_dlopen && libc_path[0]) {
        off_dlopen = elf_symbol_offset(libc_path, "dlopen");
        off_dlerror = elf_symbol_offset(libc_path, "dlerror");
        if (off_dlopen) dl_base = libc_remote;
    }
    if (!off_mmap || !off_dlopen) {
        logln("[RAPZ] simbol ELF tidak lengkap");
        ptrace(PTRACE_DETACH, pid, 0, 0);
        return 6;
    }

    uintptr_t gadget = find_svc_gadget(pid, libc_path);
    if (!gadget) {
        ptrace(PTRACE_DETACH, pid, 0, 0);
        return 7;
    }

    uintptr_t mem = remote_mmap(pid, &original, gadget);
    logln("[RAPZ] remote mmap result=%llx", (unsigned long long)mem);
    if (!mem || mem < 0x10000) {
        ptrace(PTRACE_DETACH, pid, 0, 0);
        return 8;
    }

    size_t path_len = strlen(lib) + 1;
    if (path_len > STUB_OFF - 16) {
        logln("[RAPZ] path terlalu panjang: %zu", path_len);
        ptrace(PTRACE_DETACH, pid, 0, 0);
        return 9;
    }

    if (write_mem(pid, mem, lib, path_len) != 0) {
        ptrace(PTRACE_DETACH, pid, 0, 0);
        return 10;
    }

    char verify[512] = {0};
    if (read_mem(pid, mem, verify, path_len) == 0) {
        logln("[RAPZ] path terverifikasi: %s", verify);
    }

    unsigned char brk_insn[4] = { 0x00, 0x00, 0x20, 0xd4 };
    if (write_mem(pid, mem + STUB_OFF, brk_insn, sizeof(brk_insn)) != 0) {
        ptrace(PTRACE_DETACH, pid, 0, 0);
        return 11;
    }
    uintptr_t trap = mem + STUB_OFF;
    logln("[RAPZ] trap stub=%llx bytes=00 00 20 d4", (unsigned long long)trap);

    uintptr_t mpret = remote_syscall(pid, &original, gadget, __NR_mprotect,
                                     mem, 0x1000, PROT_RX, 0, 0, 0);
    if (is_syscall_error(mpret)) {
        logln("[RAPZ] mprotect RX gagal errno=%d (abaikan kalau RWX)",
              (int)-(int64_t)mpret);
    } else {
        logln("[RAPZ] mprotect RX OK");
    }

    unsigned long flags = RTLD_NOW_VAL | RTLD_GLOBAL_VAL;
    logln("[RAPZ] dlopen flags=%lx (NOW|GLOBAL bionic)", flags);

    uintptr_t dlopen_addr = dl_base + off_dlopen;
    logln("[RAPZ] dlopen addr=%llx base=%llx off=%llx",
          (unsigned long long)dlopen_addr, (unsigned long long)dl_base,
          (unsigned long long)off_dlopen);
    uintptr_t handle = remote_call(pid, &original, dlopen_addr,
                                   mem, flags, 0, 0, 0, 0, trap);
    logln("[RAPZ] dlopen handle=%llx", (unsigned long long)handle);

    if (!handle && off_dlerror) {
        uintptr_t err_ptr = remote_call(pid, &original, dl_base + off_dlerror,
                                        0, 0, 0, 0, 0, 0, trap);
        char errbuf[512] = {0};
        if (err_ptr && read_mem(pid, err_ptr, errbuf, sizeof(errbuf) - 1) == 0) {
            logln("[RAPZ] dlerror=%s", errbuf);
        } else {
            logln("[RAPZ] dlerror ptr=%llx (gagal baca)", (unsigned long long)err_ptr);
        }
    }

    if (set_regs(pid, &original) != 0) {
        logln("[RAPZ] restore regs gagal");
    } else {
        logln("[RAPZ] restore regs OK");
    }

    if (ptrace(PTRACE_DETACH, pid, 0, 0) != 0) {
        logln("[RAPZ] detach gagal: %s", strerror(errno));
        return 12;
    }
    logln("[RAPZ] detach OK");

    logln("[RAPZ] pid=%d libc=%llx dlbase=%llx handle=%llx result=%s",
          pid, (unsigned long long)libc_remote, (unsigned long long)dl_base,
          (unsigned long long)handle, handle ? "SUCCESS" : "FAILED");
    return handle ? 0 : 13;
}
