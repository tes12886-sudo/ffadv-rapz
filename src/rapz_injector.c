#define _GNU_SOURCE
#include <dlfcn.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#define PT_PEEKDATA 2
#define PT_POKEDATA 5
#define PT_GETREGS 12
#define PT_SETREGS 13
#define TRAP_ADDR 0x6969696969690000ULL

struct rapz_regs {
    uint64_t x[31];
    uint64_t sp;
    uint64_t pc;
    uint64_t pstate;
};

static pid_t find_pid(const char *target) {
    DIR *d = opendir("/proc");
    if (!d) return -1;
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] < '0' || e->d_name[0] > '9') continue;
        char path[256];
        snprintf(path, sizeof(path), "/proc/%s/cmdline", e->d_name);
        int fd = open(path, O_RDONLY);
        if (fd < 0) continue;
        char buf[512] = {0};
        ssize_t n = read(fd, buf, sizeof(buf) - 1);
        close(fd);
        if (n > 0 && strcmp(buf, target) == 0) {
            pid_t pid = (pid_t)atoi(e->d_name);
            closedir(d);
            return pid;
        }
    }
    closedir(d);
    return -1;
}

static int has_mapping(pid_t pid, const char *needle) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[1024];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, needle)) { found = 1; break; }
    }
    fclose(f);
    return found;
}

static uintptr_t libc_base(pid_t pid) {
    char path[64];
    snprintf(path, sizeof(path), "/proc/%d/maps", pid);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[1024];
    uintptr_t base = 0;
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "/libc.so") && strstr(line, "r--p")) {
            sscanf(line, "%lx-", (unsigned long *)&base);
            break;
        }
    }
    if (!base) {
        rewind(f);
        while (fgets(line, sizeof(line), f)) {
            if (strstr(line, "/libc.so")) {
                sscanf(line, "%lx-", (unsigned long *)&base);
                break;
            }
        }
    }
    fclose(f);
    return base;
}

static long peek_word(pid_t pid, uintptr_t addr) {
    errno = 0;
    return ptrace(PT_PEEKDATA, pid, (void *)addr, 0);
}

static void write_mem(pid_t pid, uintptr_t addr, const void *data, size_t len) {
    const unsigned char *src = (const unsigned char *)data;
    size_t off = 0;
    while (off < len) {
        uintptr_t cur = addr + off;
        size_t n = len - off;
        if (n > 8) n = 8;
        uint64_t word = 0;
        if (n < 8) {
            long old = peek_word(pid, cur & ~7ULL);
            if (old != -1 || errno == 0) word = (uint64_t)old;
        }
        memcpy(&word, src + off, n);
        ptrace(PT_POKEDATA, pid, (void *)cur, (void *)word);
        off += n;
    }
}

static uintptr_t remote_call(pid_t pid, const struct rapz_regs *original, uintptr_t fn,
                             uintptr_t a0, uintptr_t a1, uintptr_t a2,
                             uintptr_t a3, uintptr_t a4, uintptr_t a5) {
    struct rapz_regs r = *original;
    r.x[0] = a0; r.x[1] = a1; r.x[2] = a2;
    r.x[3] = a3; r.x[4] = a4; r.x[5] = a5;
    r.x[8] = 0;
    r.x[30] = TRAP_ADDR;
    r.pc = fn;
    if (ptrace(PT_SETREGS, pid, 0, &r) != 0) return 0;
    if (ptrace(PT_CONT, pid, 0, 0) != 0) return 0;
    int status = 0;
    waitpid(pid, &status, 0);
    memset(&r, 0, sizeof(r));
    ptrace(PT_GETREGS, pid, 0, &r);
    return r.x[0];
}

int main(int argc, char **argv) {
    const char *lib = argc > 1 ? argv[1] : "/data/local/tmp/rap/libsheyan.so";
    const char *pkg = "com.dts.freefireadv";

    pid_t pid = find_pid(pkg);
    if (pid < 0) {
        fprintf(stderr, "[RAPZ] game belum jalan\n");
        return 1;
    }

    for (int i = 0; i < 90; i++) {
        if (has_mapping(pid, "libil2cpp.so")) break;
        sleep(1);
    }
    if (!has_mapping(pid, "libil2cpp.so")) {
        fprintf(stderr, "[RAPZ] libil2cpp.so tidak muncul\n");
        return 2;
    }

    if (ptrace(PT_ATTACH, pid, 0, 0) != 0) {
        fprintf(stderr, "[RAPZ] ptrace attach gagal: %s\n", strerror(errno));
        return 3;
    }
    int status = 0;
    waitpid(pid, &status, 0);

    struct rapz_regs original;
    memset(&original, 0, sizeof(original));
    if (ptrace(PT_GETREGS, pid, 0, &original) != 0) {
        ptrace(PT_DETACH, pid, 0, 0);
        return 4;
    }

    uintptr_t libc_remote = libc_base(pid);
    uintptr_t libc_local = libc_base(getpid());
    if (!libc_remote || !libc_local) {
        ptrace(PT_DETACH, pid, 0, 0);
        return 5;
    }

    uintptr_t off_mmap = (uintptr_t)dlsym(RTLD_DEFAULT, "mmap") - libc_local;
    uintptr_t off_dlopen = (uintptr_t)dlsym(RTLD_DEFAULT, "dlopen") - libc_local;

    uintptr_t mem = remote_call(pid, &original, libc_remote + off_mmap,
                                0, 0x1000, 3, 0x22, (uintptr_t)-1, 0);
    if (mem < 0x10000) {
        ptrace(PT_DETACH, pid, 0, 0);
        return 6;
    }

    write_mem(pid, mem, lib, strlen(lib) + 1);

    uintptr_t handle = remote_call(pid, &original, libc_remote + off_dlopen,
                                   mem, 2 | 0x100000, 0, 0, 0, 0);

    ptrace(PT_SETREGS, pid, 0, &original);
    ptrace(PT_DETACH, pid, 0, 0);

    printf("[RAPZ] pid=%d libc=%llx handle=%llx\n", pid,
           (unsigned long long)libc_remote, (unsigned long long)handle);
    return handle ? 0 : 7;
}
