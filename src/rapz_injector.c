#define _GNU_SOURCE
#include <dlfcn.h>
#include <dirent.h>
#include <linux/ptrace.h>
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
#ifndef __NR_mmap
#define __NR_mmap 222
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

    struct iovec io = { r, sizeof(*r) };
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

static int get_syscall_info(pid_t pid, struct ptrace_syscall_info *si) {
    memset(si, 0, sizeof(*si));
    if (ptrace(PTRACE_GET_SYSCALL_INFO, pid, (void *)sizeof(*si), si) != 0) {
        logln("[RAPZ] PTRACE_GET_SYSCALL_INFO gagal: %s", strerror(errno));
        return -1;
    }

    if (si->op == PTRACE_SYSCALL_INFO_ENTRY) {
        logln("[RAPZ] syscall ENTRY nr=%llu args=%llx,%llx,%llx,%llx,%llx,%llx",
              (unsigned long long)si->entry.nr,
              (unsigned long long)si->entry.args[0],
              (unsigned long long)si->entry.args[1],
              (unsigned long long)si->entry.args[2],
              (unsigned long long)si->entry.args[3],
              (unsigned long long)si->entry.args[4],
              (unsigned long long)si->entry.args[5]);
    } else if (si->op == PTRACE_SYSCALL_INFO_EXIT) {
        logln("[RAPZ] syscall EXIT rval=%lld is_error=%u",
              (long long)si->exit.rval, (unsigned)si->exit.is_error);
    } else {
        logln("[RAPZ] syscall op=%u", (unsigned)si->op);
    }
    return 0;
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
    logln("[RAPZ] pid terpilih=%d (%s)", chosen, exact > 0 ? "exact" : (fallback > 0 ? "partial" : "none"));
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

static uintptr_t libc_base(pid_t pid, const char *label) {
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
        if (strstr(line, "/libc.so") && strstr(line, "r--p")) {
            if (sscanf(line, "%lx-", (unsigned long *)&base) == 1) {
                logln("[RAPZ] %s libc base=%llx (%s)", label,
                      (unsigned long long)base, line);
                break;
            }
        }
    }

    if (!base) {
        rewind(f);
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "/libc.so")) {
                if (sscanf(line, "%lx-", (unsigned long *)&base) == 1) {
                    logln("[RAPZ] %s libc base fallback=%llx (%s)", label,
                          (unsigned long long)base, line);
                    break;
                }
            }
        }
    }

    fclose(f);
    if (!base) logln("[RAPZ] %s libc base TIDAK ditemukan", label);
    return base;
}

static uintptr_t libc_sym_offset(const char *name, uintptr_t base) {
    void *h = dlopen("libc.so", RTLD_NOW);
    if (!h) h = dlopen("libc.so.6", RTLD_NOW);
    void *s = h ? dlsym(h, name) : dlsym(RTLD_DEFAULT, name);
    if (!s) {
        logln("[RAPZ] simbol %s tidak ditemukan: %s", name, dlerror());
        return 0;
    }

    uintptr_t off = (uintptr_t)s - base;
    logln("[RAPZ] sym %-8s local=%llx base=%llx off=%llx",
          name, (unsigned long long)(uintptr_t)s,
          (unsigned long long)base, (unsigned long long)off);
    return off;
}

static long peek_word(pid_t pid, uintptr_t addr) {
    errno = 0;
    return ptrace(PT_PEEKDATA, pid, (void *)addr, 0);
}

static int read_mem(pid_t pid, uintptr_t addr, void *dst, size_t len) {
    struct iovec local = { dst, len };
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

static uintptr_t remote_mmap(pid_t pid, const struct rapz_regs *original, uintptr_t fn) {
    struct rapz_regs r = *original;
    r.x[0] = 0;
    r.x[1] = 0x1000;
    r.x[2] = 3;
    r.x[3] = 0x22;
    r.x[4] = (uint64_t)-1;
    r.x[5] = 0;

    if (set_regs(pid, &r) != 0) {
        logln("[RAPZ] set regs mmap gagal");
        return 0;
    }
    logln("[RAPZ] remote mmap dipanggil fn=%llx nr=%d",
          (unsigned long long)fn, __NR_mmap);

    if (ptrace(PTRACE_SYSCALL, pid, 0, 0) != 0) {
        logln("[RAPZ] PTRACE_SYSCALL entry gagal: %s", strerror(errno));
        return 0;
    }

    struct ptrace_syscall_info si;
    int found_entry = 0;

    for (int i = 0; i < 8; i++) {
        int status = 0;
        if (waitpid(pid, &status, 0) < 0) {
            logln("[RAPZ] waitpid syscall gagal: %s", strerror(errno));
            return 0;
        }
        log_status("syscall", status);

        if (get_syscall_info(pid, &si) != 0) return 0;

        if (si.op == PTRACE_SYSCALL_INFO_ENTRY && si.entry.nr == __NR_mmap) {
            found_entry = 1;
            break;
        }

        if (ptrace(PTRACE_SYSCALL, pid, 0, 0) != 0) {
            logln("[RAPZ] PTRACE_SYSCALL lanjut gagal: %s", strerror(errno));
            return 0;
        }
    }

    if (!found_entry) {
        logln("[RAPZ] entry mmap tidak ketemu setelah 8 stop");
        return 0;
    }

    if (ptrace(PTRACE_SYSCALL, pid, 0, 0) != 0) {
        logln("[RAPZ] PTRACE_SYSCALL exit gagal: %s", strerror(errno));
        return 0;
    }

    int status = 0;
    if (waitpid(pid, &status, 0) < 0) {
        logln("[RAPZ] waitpid exit gagal: %s", strerror(errno));
        return 0;
    }
    log_status("mmap-exit", status);

    if (get_syscall_info(pid, &si) != 0) return 0;
    if (si.op != PTRACE_SYSCALL_INFO_EXIT) {
        logln("[RAPZ] stop bukan syscall EXIT, op=%u", (unsigned)si.op);
        return 0;
    }
    if (si.exit.is_error) {
        logln("[RAPZ] mmap syscall error: -%d", (int)-si.exit.rval);
        return 0;
    }

    return (uintptr_t)si.exit.rval;
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

    logln("[RAPZ] build=rapz-v6 === start ===");
    logln("[RAPZ] uid=%d pid_self=%d lib=%s target=%s",
          (int)getuid(), (int)getpid(), lib, pkg);

    struct stat st;
    if (stat(lib, &st) != 0) {
        logln("[RAPZ] payload tidak ada: %s (%s)", lib, strerror(errno));
        return 10;
    }
    logln("[RAPZ] payload size=%lld mode=%o", (long long)st.st_size, (unsigned)st.st_mode & 0777);

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

    uintptr_t libc_remote = libc_base(pid, "remote");
    uintptr_t libc_local = libc_base(getpid(), "local ");
    if (!libc_remote || !libc_local) {
        ptrace(PTRACE_DETACH, pid, 0, 0);
        return 5;
    }

    uintptr_t off_mmap = libc_sym_offset("mmap", libc_local);
    uintptr_t off_dlopen = libc_sym_offset("dlopen", libc_local);
    uintptr_t off_dlerror = libc_sym_offset("dlerror", libc_local);
    if (!off_mmap || !off_dlopen) {
        ptrace(PTRACE_DETACH, pid, 0, 0);
        return 6;
    }

    uintptr_t mem = remote_mmap(pid, &original, libc_remote + off_mmap);
    logln("[RAPZ] remote mmap result=%llx", (unsigned long long)mem);
    if (mem < 0x10000) {
        ptrace(PTRACE_DETACH, pid, 0, 0);
        return 7;
    }

    size_t path_len = strlen(lib) + 1;
    if (path_len > STUB_OFF - 16) {
        logln("[RAPZ] path terlalu panjang: %zu", path_len);
        ptrace(PTRACE_DETACH, pid, 0, 0);
        return 8;
    }

    if (write_mem(pid, mem, lib, path_len) != 0) {
        ptrace(PTRACE_DETACH, pid, 0, 0);
        return 9;
    }

    char verify[512] = {0};
    if (read_mem(pid, mem, verify, path_len) == 0) {
        logln("[RAPZ] path terverifikasi: %s", verify);
    }

    unsigned char brk_insn[4] = { 0x00, 0x00, 0x20, 0xd4 };
    if (write_mem(pid, mem + STUB_OFF, brk_insn, sizeof(brk_insn)) != 0) {
        ptrace(PTRACE_DETACH, pid, 0, 0);
        return 10;
    }
    uintptr_t trap = mem + STUB_OFF;
    logln("[RAPZ] trap stub=%llx bytes=00 00 20 d4", (unsigned long long)trap);

    unsigned long flags = RTLD_NOW_VAL | RTLD_GLOBAL_VAL;
    logln("[RAPZ] dlopen flags=%lx (NOW|GLOBAL bionic)", flags);

    uintptr_t handle = remote_call(pid, &original, libc_remote + off_dlopen,
                                   mem, flags, 0, 0, 0, 0, trap);
    logln("[RAPZ] dlopen handle=%llx", (unsigned long long)handle);

    if (!handle && off_dlerror) {
        uintptr_t err_ptr = remote_call(pid, &original, libc_remote + off_dlerror,
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

    logln("[RAPZ] pid=%d libc=%llx handle=%llx result=%s",
          pid, (unsigned long long)libc_remote, (unsigned long long)handle,
          handle ? "SUCCESS" : "FAILED");
    return handle ? 0 : 11;
}
