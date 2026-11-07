#define _GNU_SOURCE
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/tty.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
static int stopping;
static volatile sig_atomic_t reload_desktop = 0;
static void reload_session(int sig) {
    (void)sig;
    reload_desktop = 1;
}
static void stop(int sig) {
    stopping = sig;
}
static int run(char *const argv[]) {
    int p = fork();
    if (!p) {
        execvp(argv[0], argv);
        perror(argv[0]);
        _exit(127);
    }
    int s;
    if (p < 0)
        return -1;
    while (waitpid(p, &s, 0) < 0 && errno == EINTR) {
    }
    return WIFEXITED(s) ? WEXITSTATUS(s) : 128;
}
static int dhcp(int argc, char **argv) {
    if (argc < 2)
        return 1;
    char *iface = getenv("interface");
    if (!iface)
        return 1;
    if (!strcmp(argv[1], "deconfig")) {
        char *a[] = {"ip", "link", "set", iface, "up", NULL};
        return run(a);
    }
    if (strcmp(argv[1], "bound") && strcmp(argv[1], "renew"))
        return 0;
    char *ip = getenv("ip"), *mask = getenv("subnet"), *router = getenv("router"),
         *dns = getenv("dns");
    if (!ip)
        return 1;
    unsigned a = 255, b = 255, c = 255, d = 0;
    if (mask)
        sscanf(mask, "%u.%u.%u.%u", &a, &b, &c, &d);
    unsigned bits = (a << 24) | (b << 16) | (c << 8) | d;
    char addr[80];
    snprintf(addr, sizeof addr, "%s/%d", ip, __builtin_popcount(bits));
    char *up[] = {"ip", "link", "set", iface, "up", NULL};
    run(up);
    char *flush[] = {"ip", "addr", "flush", "dev", iface, NULL};
    run(flush);
    char *add[] = {"ip", "addr", "add", addr, "dev", iface, NULL};
    if (run(add))
        return 1;
    if (router && *router) {
        char r[128];
        snprintf(r, sizeof r, "%s", router);
        char *space = strchr(r, ' ');
        if (space)
            *space = 0;
        char *route[] = {"ip", "route", "replace", "default", "via", r, "dev", iface, NULL};
        run(route);
    }
    FILE *f = fopen("/etc/resolv.conf", "w");
    if (f) {
        if (dns) {
            char ds[512];
            snprintf(ds, sizeof ds, "%s", dns);
            char *save = NULL, *s = strtok_r(ds, " ", &save);
            while (s) {
                fprintf(f, "nameserver %s\n", s);
                s = strtok_r(NULL, " ", &save);
            }
        }
        fclose(f);
    }
    printf("MARROW_NETWORK_READY %s %s\n", iface, ip);
    return 0;
}
static pid_t shell(const char *dev) {
    pid_t p = fork();
    if (!p) {
        setsid();
        int fd = open(dev, O_RDWR);
        if (fd < 0)
            _exit(1);
        ioctl(fd, TIOCSCTTY, 0);
        dup2(fd, 0);
        dup2(fd, 1);
        dup2(fd, 2);
        if (fd > 2)
            close(fd);
        setenv("TERM", "linux", 1);
        chdir("/workspace");
        execl("/bin/sh", "sh", "-i", NULL);
        _exit(127);
    }
    return p;
}
static pid_t desktop(void) {
    pid_t p = fork();
    if (!p) {
        setsid();
        int vt = open("/dev/tty1", O_RDWR);
        if (vt >= 0) {
            ioctl(vt, TIOCSCTTY, 0);
            dup2(vt, 0);
            if (vt > 2)
                close(vt);
        }
        execl("/usr/bin/python3", "python3", "/usr/bin/marrow-ops", "session", NULL);
        perror("desktop");
        _exit(127);
    }
    return p;
}
int main(int argc, char **argv) {
    setenv("PATH", "/usr/bin:/bin:/usr/sbin:/sbin", 1);
    if (strstr(argv[0], "marrow-dhcp"))
        return dhcp(argc, argv);
    if (getpid() != 1) {
        fprintf(stderr, "Marrow init must be PID 1\n");
        return 1;
    }
    struct sigaction sa = {0};
    sa.sa_handler = stop;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGUSR2, &sa, NULL);
    struct sigaction reload_sa = {0};
    reload_sa.sa_handler = reload_session;
    sigemptyset(&reload_sa.sa_mask);
    sigaction(SIGHUP, &reload_sa, NULL);
    umask(022);
    mkdir("/dev", 0755);
    mkdir("/proc", 0555);
    mkdir("/sys", 0555);
    mount("devtmpfs", "/dev", "devtmpfs", 0, NULL);
    mount("proc", "/proc", "proc", 0, NULL);
    mount("sysfs", "/sys", "sysfs", 0, NULL);
    int console = open("/dev/console", O_RDWR);
    if (console >= 0) {
        dup2(console, 0);
        dup2(console, 1);
        dup2(console, 2);
        if (console > 2)
            close(console);
    }
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc < 2 || strcmp(argv[1], "--system")) {
        puts("MarrowOS / mounting persistent ext4 root");
        mkdir("/newroot", 0755);
        int ok = 0;
        for (int i = 0; i < 150; i++) {
            if ((!access("/dev/vda", F_OK) && !mount("/dev/vda", "/newroot", "ext4", 0, NULL)) ||
                (!access("/dev/sda", F_OK) && !mount("/dev/sda", "/newroot", "ext4", 0, NULL))) {
                ok = 1;
                break;
            }
            usleep(100000);
        }
        if (!ok) {
            perror("persistent root");
            execl("/bin/sh", "sh", NULL);
            return 1;
        }
        mkdir("/newroot/dev", 0755);
        mkdir("/newroot/proc", 0555);
        mkdir("/newroot/sys", 0555);
        mount("/dev", "/newroot/dev", NULL, MS_MOVE, NULL);
        mount("/proc", "/newroot/proc", NULL, MS_MOVE, NULL);
        mount("/sys", "/newroot/sys", NULL, MS_MOVE, NULL);
        execl("/bin/busybox", "switch_root", "/newroot", "/sbin/marrow-init", "--system", NULL);
        perror("switch_root");
        return 1;
    }
    sethostname("marrow", 6);
    mkdir("/run", 0755);
    mount("tmpfs", "/run", "tmpfs", MS_NOSUID | MS_NODEV, "mode=0755,size=32m");
    mkdir("/tmp", 01777);
    mount("tmpfs", "/tmp", "tmpfs", MS_NOSUID | MS_NODEV, "mode=1777,size=128m");
    mkdir("/dev/pts", 0755);
    mount("devpts", "/dev/pts", "devpts", 0, "mode=0620,ptmxmode=0666");
    mkdir("/dev/shm", 01777);
    mount("tmpfs", "/dev/shm", "tmpfs", MS_NOSUID | MS_NODEV, "mode=1777,size=64m");
    symlink("/proc/self/fd", "/dev/fd");
    symlink("/proc/self/fd/0", "/dev/stdin");
    symlink("/proc/self/fd/1", "/dev/stdout");
    symlink("/proc/self/fd/2", "/dev/stderr");
    setenv("HOME", "/root", 1);
    setenv("TERM", "linux", 1);
    setenv("PS1", "marrow:\\w # ", 1);
    char *drivers[] = {"sh", "-c",
                       "for f in /sys/bus/pci/devices/*/modalias /sys/bus/usb/devices/*/modalias; "
                       "do test -f \"$f\" && modprobe \"$(cat \"$f\")\" 2>/dev/null; done; mdev -s",
                       NULL};
    char *dep[] = {"depmod", "-a", NULL};
    run(dep);
    run(drivers);
    if (!fork()) {
        execlp("mdev", "mdev", "-d", "-f", NULL);
        _exit(1);
    }
    char *lo[] = {"ip", "link", "set", "lo", "up", NULL};
    run(lo);
    DIR *dir = opendir("/sys/class/net");
    if (dir) {
        struct dirent *e;
        while ((e = readdir(dir))) {
            if (e->d_name[0] == '.' || !strcmp(e->d_name, "lo") || !strncmp(e->d_name, "wl", 2))
                continue;
            char typepath[512];
            snprintf(typepath, sizeof typepath, "/sys/class/net/%s/type", e->d_name);
            FILE *tf = fopen(typepath, "r");
            int type = 0;
            if (tf) {
                if (fscanf(tf, "%d", &type) != 1)
                    type = 0;
                fclose(tf);
            }
            if (type != 1)
                continue;
            if (!fork()) {
                char *a[] = {"udhcpc", "-i", e->d_name, "-s", "/sbin/marrow-dhcp", "-t", "5",
                             "-T",     "2",  "-b",      NULL};
                execvp(a[0], a);
                _exit(127);
            }
        }
        closedir(dir);
    }
    FILE *motd = fopen("/etc/motd", "r");
    if (motd) {
        char s[256];
        while (fgets(s, sizeof s, motd))
            fputs(s, stdout);
        fclose(motd);
    }
    for (int i = 0; i < 100 && access("/dev/fb0", F_OK); i++)
        usleep(100000);
    pid_t gui = desktop(), serial = shell("/dev/ttyS0"), tty = shell("/dev/tty2");
    if (!fork()) {
        execl("/usr/bin/python3", "python3", "/usr/bin/marrow-ops", "boot", NULL);
        _exit(1);
    }
    puts("MARROW_SYSTEM_READY root=ext4 workspace=/workspace");
    while (!stopping) {
        if (reload_desktop) {
            reload_desktop = 0;
            kill(-gui, SIGTERM);
        }
        int status;
        pid_t p = waitpid(-1, &status, 0);
        if (p < 0) {
            if (errno == EINTR)
                continue;
            usleep(100000);
            continue;
        }
        if (p == gui) {
            fprintf(stderr, "Desktop exited (%d), restarting\n", status);
            sleep(1);
            gui = desktop();
        }
        if (p == serial)
            serial = shell("/dev/ttyS0");
        if (p == tty)
            tty = shell("/dev/tty2");
    }
    puts("MarrowOS / syncing persistent workspace");
    kill(-1, SIGTERM);
    sleep(1);
    sync();
    mount(NULL, "/", NULL, MS_REMOUNT | MS_RDONLY, NULL);
    reboot((stopping == SIGTERM || stopping == SIGINT) ? RB_AUTOBOOT : RB_POWER_OFF);
    return 0;
}
