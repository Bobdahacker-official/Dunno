#define _GNU_SOURCE
#include "../assets/font.h"
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <linux/fb.h>
#include <linux/input.h>
#include <linux/kd.h>
#include <linux/vt.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#define W 1280
#define H 800
#define BG 0x101312
#define PANEL 0x191e1b
#define LINE 0x2b332e
#define TEXT 0xe7ede8
#define MUTED 0x92a095
#define LIME 0xc3f580
#define TEAL 0x74bca5
#define GOLD 0xe5ba7c
static uint32_t screen[W * H];
static uint8_t *fb;
static int ttyfd = -1;
static int active_tty(void) {
    struct vt_stat state;
    return ttyfd < 0 || ioctl(ttyfd, VT_GETSTATE, &state) || state.v_active == 1;
}
static struct fb_var_screeninfo vi;
static struct fb_fix_screeninfo fi;
static int tab = 0, mx = 900, my = 680, shift = 0, ctrl = 0, mouse_down = 0, dirty = 1, running = 1;
static int editor_writable = 0;
static int focus = 0, editing = 0, cmdlen = 0, child = 0, epochs = 600, job_status = 0;
static char cmd[512] = "", notice[180] = "Workspace ready. Your files live on disk.",
            terminal[65536] = "MarrowOS shell - C tools, persistent ext4 workspace\nType a command "
                              "below. Try: marrow-train info /workspace/models/spiral.weights\n";
static char cwd[512] = "/workspace", selected[1024] = "", editor[32768] = "";
static char entries[128][256];
static int entry_count = 0, selected_index = -1;
static float losses[10001], accuracies[10001];
static int metric_count = 0, last_epoch = 0;
static float loss = 0, accuracy = 0;
static char ip[64] = "Acquiring address", iface[32] = "eth0";
static int network_up = 0;
static char wifi[32] = "", ssid[64] = "", pass[128] = "";
static int netfield = 0;
static const char *names[] = {"Overview",       "Training lab", "Datasets & files",
                              "Model registry", "Network",      "Terminal"};
static const char *subtitles[] = {"Your local AI workspace",       "Train, inspect, improve",
                                  "A real filesystem. Your data.", "Weights you can keep",
                                  "Connected to your workspace",   "A shell, close to the metal"};
static void rect(int x, int y, int w, int h, uint32_t c) {
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y, x1 = x + w > W ? W : x + w,
        y1 = y + h > H ? H : y + h;
    for (int j = y0; j < y1; j++)
        for (int i = x0; i < x1; i++)
            screen[j * W + i] = c;
}
static void pixel(int x, int y, uint32_t c) {
    if (x >= 0 && y >= 0 && x < W && y < H)
        screen[y * W + x] = c;
}
static uint32_t blend(uint32_t a, uint32_t b, int t) {
    int r = (((a >> 16) & 255) * (255 - t) + ((b >> 16) & 255) * t) / 255,
        g = (((a >> 8) & 255) * (255 - t) + ((b >> 8) & 255) * t) / 255,
        bl = ((a & 255) * (255 - t) + (b & 255) * t) / 255;
    return (r << 16) | (g << 8) | bl;
}
static void line(int x, int y, int xx, int yy, uint32_t c) {
    int dx = abs(xx - x), sx = x < xx ? 1 : -1, dy = -abs(yy - y), sy = y < yy ? 1 : -1,
        e = dx + dy;
    for (;;) {
        pixel(x, y, c);
        if (x == xx && y == yy)
            break;
        int e2 = 2 * e;
        if (e2 >= dy) {
            e += dy;
            x += sx;
        }
        if (e2 <= dx) {
            e += dx;
            y += sy;
        }
    }
}
static void circle(int cx, int cy, int r, uint32_t c) {
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x * x + y * y <= r * r)
                pixel(cx + x, cy + y, c);
}
static void roundrect(int x, int y, int w, int h, int r, uint32_t c) {
    rect(x + r, y, w - 2 * r, h, c);
    rect(x, y + r, w, h - 2 * r, c);
    for (int yy = 0; yy < r; yy++)
        for (int xx = 0; xx < r; xx++)
            if ((xx - r) * (xx - r) + (yy - r) * (yy - r) <= r * r) {
                pixel(x + xx, y + yy, c);
                pixel(x + w - 1 - xx, y + yy, c);
                pixel(x + xx, y + h - 1 - yy, c);
                pixel(x + w - 1 - xx, y + h - 1 - yy, c);
            }
}
static int text(int x, int y, const Font *f, uint32_t c, const char *s) {
    int start = x;
    for (; *s; s++) {
        unsigned char ch = *s;
        if (ch == '\n') {
            y += f->height;
            x = start;
            continue;
        }
        if (ch < 32 || ch > 126)
            ch = '?';
        int k = ch - 32;
        const uint8_t *p = f->data + k * f->width * f->height;
        for (int j = 0; j < f->height; j++)
            for (int i = 0; i < f->width; i++) {
                int a = p[j * f->width + i];
                if (a && x + i >= 0 && x + i < W && y + j >= 0 && y + j < H) {
                    uint32_t *d = &screen[(y + j) * W + x + i];
                    *d = blend(*d, c, a);
                }
            }
        x += f->advance[k];
    }
    return x;
}
static void label(int x, int y, const Font *f, uint32_t c, const char *fmt, ...) {
    char b[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    text(x, y, f, c, b);
}
static void icon(int x, int y, int which, uint32_t c) {
    switch (which) {
    case 0:
        for (int i = 0; i < 4; i++)
            roundrect(x + (i % 2) * 10, y + (i / 2) * 10, 7, 7, 2, c);
        break;
    case 1:
        line(x, y + 17, x + 5, y + 9, c);
        line(x + 5, y + 9, x + 10, y + 13, c);
        line(x + 10, y + 13, x + 18, y + 1, c);
        break;
    case 2:
        rect(x, y + 3, 18, 14, c);
        rect(x + 2, y + 5, 14, 10, PANEL);
        rect(x, y, 8, 4, c);
        break;
    case 3:
        for (int j = 0; j < 3; j++) {
            roundrect(x, y + j * 7, 18, 5, 2, c);
            rect(x + 3, y + j * 7 + 2, 12, 1, PANEL);
        }
        break;
    case 4:
        circle(x + 9, y + 15, 2, c);
        line(x + 3, y + 8, x + 9, y + 5, c);
        line(x + 9, y + 5, x + 15, y + 8, c);
        line(x, y + 3, x + 9, y - 1, c);
        line(x + 9, y - 1, x + 18, y + 3, c);
        break;
    case 5:
        line(x, y + 3, x + 5, y + 8, c);
        line(x + 5, y + 8, x, y + 13, c);
        rect(x + 8, y + 14, 10, 2, c);
        break;
    }
}
static void pill(int x, int y, const char *s, uint32_t c) {
    int w = 18;
    for (const char *p = s; *p; p++)
        w += font12.advance[(unsigned char)*p - 32];
    roundrect(x, y, w, 25, 12, blend(PANEL, c, 24));
    text(x + 9, y + 4, &font12, c, s);
}
static void button(int x, int y, int w, const char *s, int primary, int active) {
    roundrect(x, y, w, 40, 7,
              active ? (primary ? LIME : 0x303a32) : (primary ? 0x42513b : 0x242a26));
    text(x + 15, y + 10, &font14, primary ? BG : TEXT, s);
}
static int inside(int x, int y, int w, int h) {
    return mx >= x && mx < x + w && my >= y && my < y + h;
}
static void append_term(const char *s) {
    size_t n = strlen(s), len = strlen(terminal);
    if (n > 20000) {
        s += n - 20000;
        n = 20000;
    }
    if (len + n >= sizeof terminal) {
        size_t trim = len + n - sizeof terminal + 10000;
        if (trim > len)
            trim = len;
        memmove(terminal, terminal + trim, len - trim + 1);
        len -= trim;
    }
    memcpy(terminal + len, s, n + 1);
}
static void notify(const char *s) {
    snprintf(notice, sizeof notice, "%s", s);
    dirty = 1;
}
static void load_entries(void) {
    DIR *d = opendir(cwd);
    entry_count = 0;
    selected_index = -1;
    selected[0] = 0;
    editor[0] = 0;
    editing = 0;
    if (!d) {
        notify(strerror(errno));
        return;
    }
    struct dirent *e;
    while ((e = readdir(d)) && entry_count < 128) {
        if (!strcmp(e->d_name, "."))
            continue;
        snprintf(entries[entry_count++], 256, "%s", e->d_name);
    }
    closedir(d);
    for (int i = 0; i < entry_count; i++)
        for (int j = i + 1; j < entry_count; j++)
            if (strcmp(entries[i], entries[j]) > 0) {
                char t[256];
                memcpy(t, entries[i], 256);
                memcpy(entries[i], entries[j], 256);
                memcpy(entries[j], t, 256);
            }
}
static void select_file(int i) {
    editor_writable = 0;
    editing = 0;
    if (i < 0 || i >= entry_count)
        return;
    selected_index = i;
    snprintf(selected, sizeof selected, "%s/%s", cwd, entries[i]);
    struct stat st;
    if (stat(selected, &st))
        return;
    if (S_ISDIR(st.st_mode)) {
        char *real = realpath(selected, NULL);
        if (real) {
            if (strlen(real) < sizeof cwd) {
                snprintf(cwd, sizeof cwd, "%s", real);
                load_entries();
            } else
                notify("Directory path is too long.");
            free(real);
        }
        return;
    }
    if (st.st_size > 32767) {
        snprintf(editor, sizeof editor,
                 "%s\n%lld bytes\nFile exceeds editor capacity (32 KiB). Use the shell for larger "
                 "files.",
                 entries[i], (long long)st.st_size);
        return;
    }
    FILE *f = fopen(selected, "r");
    if (f) {
        size_t n = fread(editor, 1, sizeof editor - 1, f);
        editor[n] = 0;
        editor_writable = !ferror(f) && fgetc(f) == EOF;
        fclose(f);
        for (size_t k = 0; k < n; k++)
            if (editor[k] == 0 || (unsigned char)editor[k] < 9) {
                editor_writable = 0;
                snprintf(editor, sizeof editor,
                         "Binary file: %s\n%lld bytes\n\nInspect weights in Model registry, or "
                         "use:\nmarrow-train info %s",
                         entries[i], (long long)st.st_size, selected);
                break;
            }
    }
}
static void save_editor(void) {
    if (!selected[0] || !editing || !editor_writable)
        return;
    char temp[1100];
    snprintf(temp, sizeof temp, "%s.edit", selected);
    FILE *f = fopen(temp, "w");
    if (!f) {
        notify(strerror(errno));
        return;
    }
    int ok = fputs(editor, f) >= 0 && fflush(f) == 0 && fsync(fileno(f)) == 0;
    if (fclose(f))
        ok = 0;
    if (ok && rename(temp, selected) == 0) {
        int fd = open(cwd, O_DIRECTORY);
        if (fd >= 0) {
            fsync(fd);
            close(fd);
        }
        editing = 0;
        notify("File saved and synced to ext4.");
    } else {
        unlink(temp);
        notify("Save failed; original file retained.");
    }
}
static void spawn_job(char *const args[], const char *title) {
    if (child) {
        notify("Wait for the current job to finish.");
        return;
    }
    int fd = open("/workspace/runs/job.log", O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        notify(strerror(errno));
        return;
    }
    child = fork();
    if (child == 0) {
        dup2(fd, 1);
        dup2(fd, 2);
        close(fd);
        if (chdir(cwd)) {
            perror(cwd);
            _exit(1);
        }
        execvp(args[0], args);
        perror(args[0]);
        _exit(127);
    }
    close(fd);
    if (child < 0) {
        child = 0;
        notify("Unable to start process.");
        return;
    }
    job_status = -1;
    notify(title);
}
static void train_job(void) {
    char e[24];
    snprintf(e, sizeof e, "%d", epochs);
    char *args[] = {"marrow-train",
                    "train",
                    "/workspace/datasets/spiral-train.csv",
                    "/workspace/models/spiral.weights",
                    e,
                    NULL};
    spawn_job(args, "Training started. Checkpoints are saved every 25 epochs.");
}
static void eval_job(void) {
    char *args[] = {"marrow-train", "eval", "/workspace/datasets/spiral-test.csv",
                    "/workspace/models/spiral.weights", NULL};
    spawn_job(args, "Evaluating saved weights on 256 held-out examples.");
}
static void terminal_job(void) {
    if (!cmd[0])
        return;
    char b[600];
    snprintf(b, sizeof b, "\nmarrow:%s # %s\n", cwd, cmd);
    append_term(b);
    if (!strcmp(cmd, "cd") || !strncmp(cmd, "cd ", 3)) {
        const char *dest = cmd[2] ? cmd + 3 : "/workspace";
        char path[1024];
        if (dest[0] == '/')
            snprintf(path, sizeof path, "%s", dest);
        else
            snprintf(path, sizeof path, "%s/%s", cwd, dest);
        char *resolved = realpath(path, NULL);
        struct stat st;
        if (resolved && strlen(resolved) < sizeof cwd && !stat(resolved, &st) &&
            S_ISDIR(st.st_mode)) {
            snprintf(cwd, sizeof cwd, "%s", resolved);
            notify("Working directory changed.");
        } else
            append_term("cd: directory does not exist or path is too long\n");
        free(resolved);
        cmdlen = 0;
        cmd[0] = 0;
        return;
    }
    char *args[] = {"/bin/sh", "-c", cmd, NULL};
    spawn_job(args, "Command running.");
    cmdlen = 0;
    cmd[0] = 0;
}
static void update_metrics(void) {
    FILE *f = fopen("/workspace/models/spiral.weights.metrics.csv", "r");
    if (!f)
        return;
    char s[256];
    metric_count = 0;
    while (fgets(s, sizeof s, f)) {
        int e;
        float l, a;
        if (sscanf(s, "%d,%f,%f", &e, &l, &a) == 3) {
            if (metric_count < 10001) {
                losses[metric_count] = l;
                accuracies[metric_count++] = a;
            }
            loss = l;
            accuracy = a;
            last_epoch = e;
        }
    }
    fclose(f);
}
static void update_network(void) {
    struct ifaddrs *a, *p;
    network_up = 0;
    snprintf(ip, sizeof ip, "No IPv4 address");
    if (!getifaddrs(&a)) {
        for (p = a; p; p = p->ifa_next) {
            if (p->ifa_addr && p->ifa_addr->sa_family == AF_INET && strcmp(p->ifa_name, "lo")) {
                inet_ntop(AF_INET, &((struct sockaddr_in *)p->ifa_addr)->sin_addr, ip, sizeof ip);
                snprintf(iface, sizeof iface, "%s", p->ifa_name);
                network_up = 1;
                break;
            }
        }
        freeifaddrs(a);
    }
    wifi[0] = 0;
    DIR *d = opendir("/sys/class/net");
    if (d) {
        struct dirent *e;
        while ((e = readdir(d))) {
            char pth[512];
            snprintf(pth, sizeof pth, "/sys/class/net/%s/wireless", e->d_name);
            if (!access(pth, F_OK)) {
                snprintf(wifi, sizeof wifi, "%.31s", e->d_name);
                break;
            }
        }
        closedir(d);
    }
}
static void connect_wifi(void) {
    if (!wifi[0]) {
        notify("Plug in an RTL8187 USB adapter or use an ath9k PCI adapter.");
        return;
    }
    if (!ssid[0] || strlen(pass) < 8) {
        notify("Enter an SSID and a password of at least 8 characters.");
        return;
    }
    FILE *f = fopen("/run/marrow-wifi.input", "w");
    if (!f)
        return;
    fputs(pass, f);
    fputc('\n', f);
    fclose(f);
    chmod("/run/marrow-wifi.input", 0600);
    char *args[] = {"/sbin/marrow-wifi", wifi, ssid, NULL};
    spawn_job(args, "Connecting to Wi-Fi using WPA2.");
    memset(pass, 0, sizeof pass);
}
static void plot(int x, int y, int w, int h) {
    for (int j = 0; j < 5; j++) {
        line(x, y + j * h / 4, x + w, y + j * h / 4, LINE);
        label(x - 36, y + j * h / 4 - 8, &font12, MUTED, "%.2f", 1.0 - j * .25);
    }
    if (metric_count > 1) {
        int px = x, py = y + h - (int)(fminf(losses[0], 1) * h);
        for (int i = 1; i < metric_count; i++) {
            int xx = x + i * w / (metric_count - 1), yy = y + h - (int)(fminf(losses[i], 1) * h);
            line(px, py, xx, yy, LIME);
            line(px, py + 1, xx, yy + 1, LIME);
            for (int a = yy + 3; a < y + h; a++)
                if ((a - yy) % 3 == 0)
                    pixel(xx, a, blend(BG, LIME, 17));
            px = xx;
            py = yy;
        }
        circle(px, py, 4, LIME);
    }
    text(x, y + h + 12, &font12, MUTED, "0");
    label(x + w / 2 - 18, y + h + 12, &font12, MUTED, "%d", last_epoch / 2);
    label(x + w - 25, y + h + 12, &font12, MUTED, "%d", last_epoch);
}
static void spiral(int x, int y, int w, int h) {
    for (int j = 0; j < 9; j++) {
        line(x + j * w / 8, y, x + j * w / 8, y + h, 0x242f27);
        line(x, y + j * h / 8, x + w, y + j * h / 8, 0x242f27);
    }
    FILE *f = fopen("/workspace/datasets/spiral-test.csv", "r");
    if (!f)
        return;
    char s[128];
    while (fgets(s, sizeof s, f)) {
        float a, b;
        int c;
        if (sscanf(s, "%f,%f,%d", &a, &b, &c) == 3)
            circle(x + w / 2 + (int)(a * w * .44), y + h / 2 + (int)(b * h * .44), 2,
                   c ? TEAL : LIME);
    }
    fclose(f);
}
static void statcard(int x, int y, int w, const char *title, const char *value, const char *detail,
                     int ico) {
    roundrect(x, y, w, 113, 9, PANEL);
    icon(x + 19, y + 19, ico, MUTED);
    text(x + 47, y + 16, &font14, MUTED, title);
    text(x + 19, y + 44, &font24, TEXT, value);
    text(x + 19, y + 82, &font12, MUTED, detail);
}
static void common(void) {
    rect(0, 0, W, H, BG);
    rect(0, 0, 218, H, 0x151a17);
    line(218, 0, 218, H, LINE);
    roundrect(24, 26, 32, 32, 9, LIME);
    line(32, 48, 32, 36, BG);
    line(32, 36, 40, 44, BG);
    line(40, 44, 48, 36, BG);
    line(48, 36, 48, 48, BG);
    text(66, 27, &font24, TEXT, "Marrow");
    text(25, 70, &font12, MUTED, "An OS for growing ideas.");
    text(25, 125, &font12, 0x637368, "WORKSPACE");
    for (int i = 0; i < 6; i++) {
        int y = 157 + i * 49;
        if (i == tab) {
            roundrect(12, y - 2, 193, 41, 7, 0x2c3829);
            rect(12, y + 8, 3, 19, LIME);
        }
        if (focus == i && focus >= 0) {
            line(18, y + 37, 195, y + 37, 0x4c6144);
        }
        icon(26, y + 9, i, i == tab ? LIME : MUTED);
        text(57, y + 8, &font14, i == tab ? LIME : MUTED, names[i]);
    }
    roundrect(16, 626, 185, 91, 8, 0x1d251e);
    circle(34, 648, 4, LIME);
    text(46, 638, &font12, TEXT, "Local by design");
    text(30, 663, &font12, MUTED, "CPU training. Your data.");
    text(30, 682, &font12, MUTED, "No account required.");
    text(25, 748, &font12, MUTED, "MarrowOS  /  0.1");
    rect(219, 0, W - 219, 65, 0x141916);
    text(248, 23, &font14, MUTED, "Workspace");
    text(335, 23, &font14, 0x536057, "/");
    text(356, 23, &font14, TEXT, names[tab]);
    circle(1041, 33, 4, network_up ? LIME : GOLD);
    text(1053, 23, &font12, MUTED, network_up ? "Ethernet / online" : "Network / offline");
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    label(1195, 23, &font14, TEXT, "%02d:%02d", tm.tm_hour, tm.tm_min);
    line(219, 64, W, 64, LINE);
    text(251, 92, &font12, LIME, "MARROW WORKSPACE");
    text(249, 117, &font36, TEXT, names[tab]);
    text(251, 166, &font14, MUTED, subtitles[tab]);
    rect(219, 760, W - 219, 40, 0x151a17);
    circle(241, 779, 3, LIME);
    text(252, 769, &font12, MUTED, notice);
    text(1077, 769, &font12, 0x657369, "F1-F6  /  Tab  /  Enter");
}
static void overview(void) {
    char a[64], b[64];
    snprintf(a, sizeof a, "%.2f%%", accuracy);
    snprintf(b, sizeof b, "%.4f", loss);
    statcard(251, 210, 230, "Training accuracy", metric_count ? a : "--",
             "Measured on the training set", 1);
    statcard(495, 210, 230, "Cross entropy", metric_count ? b : "--", "Lower is better", 0);
    statcard(739, 210, 230, "Model parameters", "122", "2 input  /  24 hidden  /  2 output", 3);
    struct statvfs v;
    char cap[64] = "ext4";
    if (!statvfs("/workspace", &v))
        snprintf(cap, sizeof cap, "%.1f GiB", v.f_bavail * (double)v.f_frsize / 1073741824.0);
    statcard(983, 210, 264, "Available on disk", cap, "Persistent ext4 workspace", 2);
    roundrect(251, 343, 640, 319, 9, PANEL);
    text(274, 362, &font18, TEXT, "A little less loss. A little more learning.");
    text(274, 394, &font12, MUTED, "spiral-classifier  /  Adam optimizer  /  float32");
    pill(764, 363, child ? "Running" : "Saved", LIME);
    plot(313, 445, 544, 154);
    text(276, 631, &font12, MUTED, "CROSS ENTROPY");
    text(744, 631, &font12, MUTED, "TRAINING EPOCH");
    roundrect(909, 343, 338, 319, 9, PANEL);
    text(931, 362, &font18, TEXT, "Two spirals. One model.");
    text(931, 393, &font12, MUTED, "Held-out dataset  /  256 examples");
    spiral(965, 430, 225, 181);
    circle(944, 638, 3, LIME);
    text(955, 629, &font12, MUTED, "Class 0");
    circle(1060, 638, 3, TEAL);
    text(1071, 629, &font12, MUTED, "Class 1");
    roundrect(251, 680, 996, 60, 9, 0x202b20);
    text(271, 692, &font16, TEXT, "Make the next experiment yours.");
    text(271, 715, &font12, MUTED,
         "Edit a dataset, run a training job, and keep every checkpoint.");
    button(1043, 690, 183, child ? "Training in progress" : "Start training  >", 1, !child);
}
static void training(void) {
    button(1036, 126, 211, child ? "Training in progress" : "Run 600 epochs  >", 1, !child);
    roundrect(251, 210, 690, 360, 9, PANEL);
    text(274, 229, &font18, TEXT, "Training loss");
    pill(803, 229, child ? "Running" : "Complete", LIME);
    text(274, 264, &font12, MUTED,
         "Live metrics from /workspace/models/spiral.weights.metrics.csv");
    plot(313, 315, 592, 195);
    roundrect(960, 210, 287, 360, 9, PANEL);
    text(982, 230, &font18, TEXT, "Experiment");
    const char *ks[] = {"Architecture", "Optimizer",     "Learning rate",
                        "Batch size",   "Training rows", "Held-out rows"};
    const char *vs[] = {"2 -> 24 -> 2", "Adam", "0.008", "32", "768", "256"};
    for (int i = 0; i < 6; i++) {
        text(982, 278 + i * 34, &font14, MUTED, ks[i]);
        text(1131, 278 + i * 34, &font14, TEXT, vs[i]);
    }
    line(982, 491, 1223, 491, LINE);
    label(982, 506, &font14, LIME, "Epoch %d", last_epoch);
    label(1110, 506, &font14, TEXT, "%.2f%%", accuracy);
    text(982, 535, &font12, MUTED, "Checkpoints every 25 epochs");
    roundrect(251, 589, 996, 151, 9, PANEL);
    text(274, 607, &font18, TEXT, "Saved weights, honest results.");
    label(274, 641, &font14, MUTED, "Loss %.6f   /   Training accuracy %.2f%%   /   %d epochs",
          loss, accuracy, last_epoch);
    text(274, 680, &font12, MUTED,
         "Evaluate the saved checkpoint against the separate test dataset.");
    text(274, 706, &font12, MUTED,
         "Results appear in Terminal. Training and evaluation run as real processes.");
    button(1055, 642, 169, "Evaluate model", 0, !child);
}
static void wrapped(int x, int y, int width, int maxlines, const char *s, const Font *font,
                    uint32_t color) {
    int start = x, lines = 0;
    for (; *s && lines < maxlines; s++) {
        char b[2] = {*s, 0};
        if (*s == '\n' || x > start + width - 16) {
            y += font->height + 1;
            x = start;
            lines++;
            if (*s == '\n')
                continue;
        }
        if (*s == '\t') {
            x += 32;
            continue;
        }
        x = text(x, y, font, color, b);
    }
}
static void files(void) {
    roundrect(251, 210, 332, 530, 9, PANEL);
    text(271, 230, &font16, TEXT, "Files");
    char path[48];
    snprintf(path, sizeof path, "%.43s", cwd);
    text(271, 263, &font12, MUTED, path);
    line(269, 297, 564, 297, LINE);
    int first = selected_index > 10 ? selected_index - 10 : 0;
    for (int row = 0; row < 11 && row + first < entry_count; row++) {
        int i = row + first;
        int y = 309 + row * 37;
        if (i == selected_index)
            roundrect(261, y - 2, 311, 33, 5, 0x2a3628);
        char p[1024];
        snprintf(p, sizeof p, "%.511s/%.255s", cwd, entries[i]);
        struct stat st;
        int isdir = !stat(p, &st) && S_ISDIR(st.st_mode);
        icon(274, y + 7, isdir ? 2 : 3, isdir ? LIME : MUTED);
        char name[37];
        snprintf(name, sizeof name, "%.32s", entries[i]);
        text(303, y + 5, &font14, TEXT, name);
    }
    roundrect(601, 210, 646, 530, 9, PANEL);
    text(623, 229, &font16, TEXT, selected[0] ? strrchr(selected, '/') + 1 : "File preview");
    pill(1090, 231, editing ? "Editing" : "On disk", editing ? GOLD : LIME);
    line(620, 278, 1225, 278, LINE);
    if (selected[0])
        wrapped(623, 297, 600, 16, editor, &font14, TEXT);
    else {
        icon(891, 379, 2, LIME);
        text(754, 425, &font24, TEXT, "Your experiments start here.");
        text(746, 469, &font14, MUTED, "Select a folder or file from the list.");
        text(768, 498, &font12, MUTED, "CSV, scripts, logs, and saved weights.");
    }
    text(623, 702, &font12, MUTED, "E  Edit text     Ctrl+S  Save     Esc  Leave editor");
}
static void models(void) {
    roundrect(251, 210, 996, 158, 9, PANEL);
    roundrect(275, 236, 73, 73, 12, 0x2a3828);
    icon(301, 261, 3, LIME);
    text(370, 233, &font24, TEXT, "spiral-classifier");
    text(371, 272, &font14, MUTED, "A two-layer classifier, trained entirely on your CPU.");
    pill(371, 311, "Float32", LIME);
    pill(461, 311, "122 parameters", TEAL);
    pill(601, 311, "Checksummed", GOLD);
    button(1052, 250, 172, "Evaluate weights", 1, !child);
    char trainacc[40];
    snprintf(trainacc, sizeof trainacc, "%.2f%%", accuracy);
    statcard(251, 389, 318, "Training accuracy", trainacc,
             "Evaluate with the separate test dataset", 1);
    char val[60];
    snprintf(val, sizeof val, "%.4f", loss);
    statcard(589, 389, 318, "Latest training loss", val, "Read from the saved metrics file", 0);
    struct stat st;
    char bytes[64] = "No checkpoint";
    if (!stat("/workspace/models/spiral.weights", &st))
        snprintf(bytes, sizeof bytes, "%lld bytes", (long long)st.st_size);
    statcard(927, 389, 320, "Checkpoint size", bytes, "Atomic save + fsync", 3);
    roundrect(251, 523, 996, 217, 9, PANEL);
    text(275, 544, &font18, TEXT, "Artifact details");
    const char *k[] = {"Location", "Architecture", "Format", "Inference"};
    const char *v[] = {"/workspace/models/spiral.weights",
                       "2 inputs  ->  24 tanh units  ->  2 softmax outputs",
                       "Marrow weights v1 / header + float32 parameters + checksum",
                       "marrow-train infer /workspace/models/spiral.weights 0.2 0.5"};
    for (int i = 0; i < 4; i++) {
        text(275, 585 + i * 33, &font14, MUTED, k[i]);
        text(430, 585 + i * 33, &font14, TEXT, v[i]);
    }
}
static void network(void) {
    roundrect(251, 210, 478, 199, 9, PANEL);
    icon(276, 235, 4, LIME);
    text(312, 230, &font18, TEXT, "Ethernet");
    pill(608, 231, network_up ? "Online" : "Offline", network_up ? LIME : GOLD);
    text(276, 282, &font14, MUTED, "Interface");
    text(432, 282, &font14, TEXT, iface);
    text(276, 317, &font14, MUTED, "IPv4 address");
    text(432, 317, &font14, TEXT, ip);
    text(276, 365, &font12, MUTED, "VirtIO / Intel e1000 / Realtek RTL8139");
    roundrect(750, 210, 497, 199, 9, PANEL);
    text(774, 230, &font18, TEXT, "Wireless hardware");
    text(774, 279, &font14, TEXT, wifi[0] ? wifi : "No wireless adapter attached");
    text(774, 315, &font14, MUTED, "Atheros ath9k PCI / PCIe");
    text(774, 344, &font14, MUTED, "Realtek RTL8187 USB");
    text(774, 376, &font12, MUTED, "WPA2 via wpa_supplicant / nl80211");
    roundrect(251, 430, 996, 310, 9, PANEL);
    text(275, 451, &font18, TEXT, "Join a wireless network");
    text(275, 489, &font14, MUTED,
         "Connect a supported adapter, then enter your network credentials.");
    text(275, 533, &font12, MUTED, "NETWORK NAME (SSID)");
    roundrect(275, 557, 445, 42, 6, netfield == 1 ? 0x34412d : 0x101512);
    text(288, 567, &font14, TEXT, ssid[0] ? ssid : "Click to enter network name");
    text(746, 533, &font12, MUTED, "PASSWORD");
    roundrect(746, 557, 472, 42, 6, netfield == 2 ? 0x34412d : 0x101512);
    char mask[128];
    memset(mask, '*', strlen(pass));
    mask[strlen(pass)] = 0;
    text(759, 567, &font14, TEXT, pass[0] ? mask : "Click to enter password");
    button(275, 626, 176, "Connect Wi-Fi", 1, !child);
    button(468, 626, 145, "Renew DHCP", 0, !child);
    text(275, 700, &font12, MUTED, "Credentials are kept in /run and are cleared on reboot.");
}
static void terminal_page(void) {
    roundrect(251, 210, 996, 530, 9, 0x0b100d);
    rect(252, 211, 994, 40, 0x202922);
    circle(273, 231, 4, LIME);
    text(286, 220, &font12, MUTED, "root@marrow  /  /workspace");
    text(1080, 220, &font12, MUTED, child ? "Process running" : "Shell ready");
    int lines = 0;
    for (char *p = terminal; *p; p++)
        if (*p == '\n')
            lines++;
    const char *start = terminal;
    while (lines > 18 && *start) {
        if (*start == '\n')
            lines--;
        start++;
    }
    wrapped(271, 269, 950, 18, start, &font14, 0xc2d9c6);
    line(271, 671, 1227, 671, LINE);
    text(271, 695, &font16, LIME, "#");
    text(293, 695, &font14, TEXT, cmd);
    int xx = 293;
    for (int i = 0; i < cmdlen; i++)
        xx += font14.advance[(unsigned char)cmd[i] - 32];
    rect(xx + 2, 699, 7, 17, LIME);
}
static void cursor(void) {
    for (int y = 0; y < 18; y++)
        for (int x = 0; x < 12; x++)
            if (x <= y * 2 / 3) {
                pixel(mx + x, my + y, 0x0a0e0b);
                if (x > 0 && x < y * 2 / 3 && y < 15)
                    pixel(mx + x, my + y, 0xe2eddf);
            }
}
static void present(void) {
    if (fb && !active_tty())
        return;
    common();
    switch (tab) {
    case 0:
        overview();
        break;
    case 1:
        training();
        break;
    case 2:
        files();
        break;
    case 3:
        models();
        break;
    case 4:
        network();
        break;
    case 5:
        terminal_page();
        break;
    }
    cursor();
    if (!fb)
        return;
    int ow = vi.xres, oh = vi.yres;
    for (int y = 0; y < oh; y++) {
        uint8_t *row =
            fb + (y + vi.yoffset) * fi.line_length + vi.xoffset * (vi.bits_per_pixel / 8);
        int sy = y * H / oh;
        for (int x = 0; x < ow; x++) {
            uint32_t c = screen[sy * W + x * W / ow];
            uint32_t v = (((c >> 16) & 255) >> (8 - vi.red.length)) << vi.red.offset |
                         (((c >> 8) & 255) >> (8 - vi.green.length)) << vi.green.offset |
                         ((c & 255) >> (8 - vi.blue.length)) << vi.blue.offset;
            if (vi.bits_per_pixel == 32)
                ((uint32_t *)row)[x] = v;
            else if (vi.bits_per_pixel == 16)
                ((uint16_t *)row)[x] = v;
        }
    }
}
static void switch_tab(int t) {
    tab = t;
    editing = 0;
    netfield = 0;
    if (t == 2)
        load_entries();
    dirty = 1;
}
static void click(void) {
    for (int i = 0; i < 6; i++)
        if (inside(12, 155 + i * 49, 193, 43)) {
            focus = i;
            switch_tab(i);
            return;
        }
    if (tab == 0 && inside(1043, 690, 183, 40))
        train_job();
    else if (tab == 1) {
        if (inside(1036, 126, 211, 40))
            train_job();
        if (inside(1055, 642, 169, 40)) {
            eval_job();
            switch_tab(5);
        }
    } else if (tab == 2) {
        int first = selected_index > 10 ? selected_index - 10 : 0;
        for (int row = 0; row < 11 && row + first < entry_count; row++)
            if (inside(261, 307 + row * 37, 311, 35)) {
                select_file(row + first);
                break;
            }
    } else if (tab == 3 && inside(1052, 250, 172, 40)) {
        eval_job();
        switch_tab(5);
    } else if (tab == 4) {
        if (inside(275, 557, 445, 42))
            netfield = 1;
        else if (inside(746, 557, 472, 42))
            netfield = 2;
        else if (inside(275, 626, 176, 40))
            connect_wifi();
        else if (inside(468, 626, 145, 40)) {
            char *a[] = {"udhcpc", "-i", iface, "-s", "/sbin/marrow-dhcp", "-n", "-q", NULL};
            spawn_job(a, "Renewing DHCP lease.");
        } else
            netfield = 0;
    }
    dirty = 1;
}
static char keychar(int c) {
    static const char keys[128] = {
        [KEY_1] = '1',          [KEY_2] = '2',           [KEY_3] = '3',
        [KEY_4] = '4',          [KEY_5] = '5',           [KEY_6] = '6',
        [KEY_7] = '7',          [KEY_8] = '8',           [KEY_9] = '9',
        [KEY_0] = '0',          [KEY_MINUS] = '-',       [KEY_EQUAL] = '=',
        [KEY_Q] = 'q',          [KEY_W] = 'w',           [KEY_E] = 'e',
        [KEY_R] = 'r',          [KEY_T] = 't',           [KEY_Y] = 'y',
        [KEY_U] = 'u',          [KEY_I] = 'i',           [KEY_O] = 'o',
        [KEY_P] = 'p',          [KEY_LEFTBRACE] = '[',   [KEY_RIGHTBRACE] = ']',
        [KEY_A] = 'a',          [KEY_S] = 's',           [KEY_D] = 'd',
        [KEY_F] = 'f',          [KEY_G] = 'g',           [KEY_H] = 'h',
        [KEY_J] = 'j',          [KEY_K] = 'k',           [KEY_L] = 'l',
        [KEY_SEMICOLON] = ';',  [KEY_APOSTROPHE] = '\'', [KEY_GRAVE] = '`',
        [KEY_BACKSLASH] = '\\', [KEY_Z] = 'z',           [KEY_X] = 'x',
        [KEY_C] = 'c',          [KEY_V] = 'v',           [KEY_B] = 'b',
        [KEY_N] = 'n',          [KEY_M] = 'm',           [KEY_COMMA] = ',',
        [KEY_DOT] = '.',        [KEY_SLASH] = '/',       [KEY_SPACE] = ' '};
    char ch = c < 128 ? keys[c] : 0;
    if (shift) {
        if (ch >= 'a' && ch <= 'z')
            ch -= 32;
        else {
            const char *normal = "1234567890-=[];'`,./\\", *upper = "!@#$%^&*()_+{}:\"~<>?|",
                       *p = strchr(normal, ch);
            if (ch && p)
                ch = upper[p - normal];
        }
    }
    return ch;
}
static void key(int c, int down) {
    if (c == KEY_LEFTSHIFT || c == KEY_RIGHTSHIFT) {
        shift = down;
        return;
    }
    if (c == KEY_LEFTCTRL || c == KEY_RIGHTCTRL) {
        ctrl = down;
        return;
    }
    if (!down)
        return;
    dirty = 1;
    if (c >= KEY_F1 && c <= KEY_F6) {
        switch_tab(c - KEY_F1);
        return;
    }
    if (c == KEY_ESC) {
        editing = 0;
        netfield = 0;
        focus = tab;
        return;
    }
    if (ctrl && c == KEY_S && editing) {
        save_editor();
        return;
    }
    if (ctrl && c == KEY_C && child) {
        kill(child, SIGTERM);
        return;
    }
    if (c == KEY_TAB && !editing) {
        if (tab == 4) {
            netfield = (netfield + 1) % 3;
            return;
        }
        focus = (focus + (shift ? 6 : 1)) % 7;
        return;
    }
    if (c == KEY_ENTER && !editing && !netfield && tab == 2) {
        if (selected_index >= 0)
            select_file(selected_index);
        return;
    }
    if (c == KEY_ENTER && !editing && !netfield && tab != 5) {
        if (focus < 6) {
            switch_tab(focus);
            return;
        }
        if (tab == 0 || tab == 1)
            train_job();
        else if (tab == 3) {
            eval_job();
            switch_tab(5);
        }
        return;
    }
    if (tab == 2 && !editing) {
        if (c == KEY_DOWN) {
            selected_index = (selected_index + 1) % (entry_count ? entry_count : 1);
            selected[0] = 0;
            editor[0] = 0;
            editor_writable = 0;
            return;
        }
        if (c == KEY_UP) {
            selected_index = (selected_index + entry_count - 1) % (entry_count ? entry_count : 1);
            selected[0] = 0;
            editor[0] = 0;
            editor_writable = 0;
            return;
        }
        if (c == KEY_E && selected[0] && editor_writable) {
            editing = 1;
            notify("Editing text. Ctrl+S saves; Esc leaves the editor.");
            return;
        }
        if (c == KEY_RIGHT && selected_index >= 0) {
            select_file(selected_index);
            return;
        }
    }
    char *dest = NULL;
    size_t cap = 0;
    if (tab == 5) {
        dest = cmd;
        cap = sizeof cmd;
    } else if (editing) {
        dest = editor;
        cap = sizeof editor;
    } else if (tab == 4 && netfield) {
        dest = netfield == 1 ? ssid : pass;
        cap = netfield == 1 ? sizeof ssid : sizeof pass;
    }
    if (!dest)
        return;
    size_t n = strlen(dest);
    if (c == KEY_BACKSPACE) {
        if (n)
            dest[n - 1] = 0;
    } else if (c == KEY_ENTER) {
        if (tab == 5)
            terminal_job();
        else if (editing && n + 1 < cap) {
            dest[n] = '\n';
            dest[n + 1] = 0;
        } else if (tab == 4)
            connect_wifi();
    } else {
        char ch = keychar(c);
        if (ch && n + 1 < cap) {
            dest[n] = ch;
            dest[n + 1] = 0;
        }
    }
    cmdlen = strlen(cmd);
}
int main(int argc, char **argv) {
    if (argc == 3 && !strcmp(argv[1], "--render")) {
        update_metrics();
        update_network();
        present();
        FILE *f = fopen(argv[2], "wb");
        if (!f)
            return 1;
        fprintf(f, "P6\n%d %d\n255\n", W, H);
        for (int i = 0; i < W * H; i++) {
            fputc(screen[i] >> 16, f);
            fputc(screen[i] >> 8, f);
            fputc(screen[i], f);
        }
        fclose(f);
        return 0;
    }
    int fd = open("/dev/fb0", O_RDWR);
    if (fd < 0) {
        perror("framebuffer");
        return 1;
    }
    if (ioctl(fd, FBIOGET_VSCREENINFO, &vi) || ioctl(fd, FBIOGET_FSCREENINFO, &fi)) {
        perror("framebuffer info");
        return 1;
    }
    if ((vi.bits_per_pixel != 32 && vi.bits_per_pixel != 16) || vi.red.length > 8 ||
        vi.green.length > 8 || vi.blue.length > 8) {
        fprintf(stderr, "Unsupported framebuffer format\n");
        return 1;
    }
    fb = mmap(NULL, fi.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (fb == MAP_FAILED) {
        perror("framebuffer mapping");
        return 1;
    }
    int tty = open("/dev/tty1", O_RDWR);
    ttyfd = tty;
    if (tty >= 0)
        ioctl(tty, KDSETMODE, KD_GRAPHICS);
    struct pollfd inputs[32];
    int nin = 0;
    for (int i = 0; i < 32; i++) {
        char p[64];
        snprintf(p, sizeof p, "/dev/input/event%d", i);
        int ev = open(p, O_RDONLY | O_NONBLOCK);
        if (ev >= 0) {
            inputs[nin].fd = ev;
            inputs[nin].events = POLLIN;
            struct input_absinfo abs;
            if (!ioctl(ev, EVIOCGABS(ABS_X), &abs) && abs.maximum > abs.minimum)
                mx = (abs.value - abs.minimum) * (W - 1) / (abs.maximum - abs.minimum);
            if (!ioctl(ev, EVIOCGABS(ABS_Y), &abs) && abs.maximum > abs.minimum)
                my = (abs.value - abs.minimum) * (H - 1) / (abs.maximum - abs.minimum);
            nin++;
        }
    }
    printf("MARROW_DESKTOP_READY %ux%u %ubpp inputs=%d\n", vi.xres, vi.yres, vi.bits_per_pixel,
           nin);
    fflush(stdout);
    load_entries();
    update_network();
    update_metrics();
    time_t last = 0;
    while (running) {
        time_t now = time(NULL);
        if (now != last) {
            last = now;
            update_metrics();
            update_network();
            if (child) {
                int status;
                int r = waitpid(child, &status, WNOHANG);
                if (r == child) {
                    job_status = WIFEXITED(status) ? WEXITSTATUS(status) : 128;
                    child = 0;
                    FILE *f = fopen("/workspace/runs/job.log", "r");
                    if (f) {
                        char b[32768];
                        size_t n = fread(b, 1, sizeof b - 1, f);
                        b[n] = 0;
                        fclose(f);
                        append_term(b);
                    }
                    notify(job_status ? "Process exited with an error. Open Terminal for details."
                                      : "Process complete. Results saved to disk.");
                }
            }
            dirty = 1;
        }
        if (dirty) {
            present();
            dirty = 0;
        }
        if (poll(inputs, nin, 80) > 0) {
            for (int i = 0; i < nin; i++)
                if (inputs[i].revents & POLLIN) {
                    struct input_event ev;
                    while (read(inputs[i].fd, &ev, sizeof ev) == sizeof ev) {
                        if (!active_tty())
                            continue;
                        if (ev.type == EV_REL) {
                            if (ev.code == REL_X)
                                mx += ev.value;
                            if (ev.code == REL_Y)
                                my += ev.value;
                            mx = mx < 0 ? 0 : mx > W - 13 ? W - 13 : mx;
                            my = my < 0 ? 0 : my > H - 19 ? H - 19 : my;
                            dirty = 1;
                        } else if (ev.type == EV_ABS) {
                            struct input_absinfo ai;
                            if (!ioctl(inputs[i].fd, EVIOCGABS(ev.code), &ai) &&
                                ai.maximum > ai.minimum) {
                                if (ev.code == ABS_X)
                                    mx = (ev.value - ai.minimum) * (W - 1) /
                                         (ai.maximum - ai.minimum);
                                if (ev.code == ABS_Y)
                                    my = (ev.value - ai.minimum) * (H - 1) /
                                         (ai.maximum - ai.minimum);
                                dirty = 1;
                            }
                        } else if (ev.type == EV_KEY) {
                            if (ev.code == BTN_LEFT) {
                                if (ev.value && !mouse_down)
                                    click();
                                mouse_down = ev.value;
                            } else
                                key(ev.code, ev.value != 0);
                        }
                    }
                }
        }
    }
    if (tty >= 0)
        ioctl(tty, KDSETMODE, KD_TEXT);
    munmap(fb, fi.smem_len);
    return 0;
}
