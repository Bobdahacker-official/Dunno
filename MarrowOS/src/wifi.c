#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
static int wait_child(pid_t p) {
    int s;
    while (waitpid(p, &s, 0) < 0)
        if (errno != EINTR)
            return 1;
    return WIFEXITED(s) ? WEXITSTATUS(s) : 1;
}
static int run(char *const a[]) {
    pid_t p = fork();
    if (!p) {
        execvp(a[0], a);
        perror(a[0]);
        _exit(127);
    }
    return p < 0 ? 1 : wait_child(p);
}
int main(int argc, char **argv) {
    if (argc != 3) {
        fputs("marrow-wifi INTERFACE SSID (password read from /run/marrow-wifi.input)\n", stderr);
        return 1;
    }
    umask(077);
    int input = open("/run/marrow-wifi.input", O_RDONLY | O_NOFOLLOW);
    if (input < 0) {
        perror("Wi-Fi password");
        return 1;
    }
    unlink("/run/marrow-wifi.input");
    int fds[2];
    if (pipe(fds)) {
        close(input);
        return 1;
    }
    pid_t p = fork();
    if (!p) {
        dup2(input, 0);
        dup2(fds[1], 1);
        close(fds[0]);
        close(fds[1]);
        close(input);
        execlp("wpa_passphrase", "wpa_passphrase", argv[2], NULL);
        _exit(127);
    }
    close(input);
    close(fds[1]);
    if (p < 0) {
        close(fds[0]);
        return 1;
    }
    FILE *f = fdopen(fds[0], "r"), *out = fopen("/run/marrow-wpa.conf", "w");
    if (!f || !out) {
        kill(p, SIGTERM);
        return 1;
    }
    fputs("ctrl_interface=/run/wpa_supplicant\n", out);
    char b[1024];
    while (fgets(b, sizeof b, f))
        if (!strstr(b, "#psk="))
            fputs(b, out);
    fclose(f);
    int ok = fflush(out) == 0 && fsync(fileno(out)) == 0;
    if (fclose(out))
        ok = 0;
    if (wait_child(p) || !ok) {
        unlink("/run/marrow-wpa.conf");
        fputs("Unable to derive Wi-Fi key\n", stderr);
        return 1;
    }
    char *up[] = {"ip", "link", "set", argv[1], "up", NULL};
    if (run(up))
        return 1;
    char *quit[] = {"wpa_cli", "-i", argv[1], "terminate", NULL};
    run(quit);
    char *wpa[] = {"wpa_supplicant",       "-B", "-D", "nl80211", "-i", argv[1], "-c",
                   "/run/marrow-wpa.conf", NULL};
    if (run(wpa))
        return 1;
    puts("Waiting for wireless association...");
    for (int i = 0; i < 30; i++) {
        char path[128];
        snprintf(path, sizeof path, "/sys/class/net/%s/carrier", argv[1]);
        FILE *c = fopen(path, "r");
        int carrier = 0;
        if (c) {
            fscanf(c, "%d", &carrier);
            fclose(c);
        }
        if (carrier) {
            char *dhcp[] = {"udhcpc", "-i", argv[1], "-s", "/sbin/marrow-dhcp",
                            "-n",     "-q", "-t",    "5",  NULL};
            return run(dhcp);
        }
        sleep(1);
    }
    fputs("Association timed out. Check SSID, password, and signal.\n", stderr);
    return 1;
}
