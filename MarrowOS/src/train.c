#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#define H 24
#define MAX_ROWS 65536
#define PARAMS (H * 2 + H + 2 * H + 2)
typedef struct {
    float w1[H][2], b1[H], w2[2][H], b2[2];
} Model;
typedef struct {
    float x[2];
    int y;
} Row;
static Row rows[MAX_ROWS];
static uint32_t rng = 42;
extern float marrow_dot(const float *, const float *, size_t);
static uint32_t random32(void) {
    rng ^= rng << 13;
    rng ^= rng >> 17;
    rng ^= rng << 5;
    return rng;
}
static float uniform(void) {
    return (random32() >> 8) * (1.0f / 16777216.0f);
}
static uint32_t checksum(const void *v, size_t n) {
    const unsigned char *p = v;
    uint32_t h = 2166136261u;
    while (n--)
        h = (h ^ *p++) * 16777619u;
    return h;
}
static void parent_sync(const char *path) {
    char b[512];
    snprintf(b, sizeof b, "%s", path);
    char *s = strrchr(b, '/');
    if (s) {
        *s = 0;
        int fd = open(*b ? b : "/", O_DIRECTORY | O_RDONLY);
        if (fd >= 0) {
            fsync(fd);
            close(fd);
        }
    }
}
static int save(const char *path, const Model *m, int epoch) {
    char tmp[600];
    snprintf(tmp, sizeof tmp, "%s.tmp", path);
    FILE *f = fopen(tmp, "wb");
    if (!f) {
        perror(path);
        return -1;
    }
    uint32_t header[8] = {
        0x4d52574d, 1, 2, H, 2, (uint32_t)epoch, sizeof *m, checksum(m, sizeof *m)};
    int ok = fwrite(header, sizeof header, 1, f) == 1 && fwrite(m, sizeof *m, 1, f) == 1;
    ok = ok && fflush(f) == 0;
    ok = ok && fsync(fileno(f)) == 0;
    if (fclose(f))
        ok = 0;
    if (!ok || rename(tmp, path)) {
        perror("checkpoint");
        unlink(tmp);
        return -1;
    }
    parent_sync(path);
    return 0;
}
static int load(const char *path, Model *m) {
    uint32_t h[8];
    FILE *f = fopen(path, "rb");
    if (!f) {
        perror(path);
        return -1;
    }
    int ok = fread(h, sizeof h, 1, f) == 1 && h[0] == 0x4d52574d && h[1] == 1 && h[2] == 2 &&
             h[3] == H && h[4] == 2 && h[6] == sizeof *m && fread(m, sizeof *m, 1, f) == 1 &&
             checksum(m, sizeof *m) == h[7] && fgetc(f) == EOF;
    fclose(f);
    if (!ok) {
        fprintf(stderr, "Invalid or damaged Marrow weights: %s\n", path);
        return -1;
    }
    const float *p = (const float *)m;
    for (int i = 0; i < PARAMS; i++)
        if (!isfinite(p[i]))
            return -1;
    return 0;
}
static int readrows(const char *path) {
    FILE *f = fopen(path, "r");
    if (!f) {
        perror(path);
        return -1;
    }
    char b[512];
    int n = 0, line = 0;
    while (fgets(b, sizeof b, f)) {
        line++;
        if (b[0] == '#' || !strncmp(b, "x1,", 3) || b[0] == '\n')
            continue;
        Row r;
        char extra;
        if (sscanf(b, "%f,%f,%d %c", &r.x[0], &r.x[1], &r.y, &extra) != 3 || !isfinite(r.x[0]) ||
            !isfinite(r.x[1]) || fabsf(r.x[0]) > 100 || fabsf(r.x[1]) > 100 || r.y < 0 || r.y > 1 ||
            n == MAX_ROWS) {
            fprintf(stderr, "Invalid CSV row %d (expected x1,x2,label; label 0 or 1)\n", line);
            fclose(f);
            return -1;
        }
        rows[n++] = r;
    }
    fclose(f);
    if (n < 2) {
        fprintf(stderr, "Dataset needs at least 2 rows\n");
        return -1;
    }
    return n;
}
static void forward(const Model *m, const float *x, float *a, float *p) {
    for (int j = 0; j < H; j++)
        a[j] = tanhf(marrow_dot(m->w1[j], x, 2) + m->b1[j]);
    float z0 = marrow_dot(m->w2[0], a, H) + m->b2[0], z1 = marrow_dot(m->w2[1], a, H) + m->b2[1];
    float z = fmaxf(z0, z1);
    p[0] = expf(z0 - z);
    p[1] = expf(z1 - z);
    float s = p[0] + p[1];
    p[0] /= s;
    p[1] /= s;
}
static void metrics(Model *m, int n, float *loss, float *acc) {
    float a[H], p[2];
    int correct = 0;
    *loss = 0;
    for (int i = 0; i < n; i++) {
        forward(m, rows[i].x, a, p);
        *loss -= logf(fmaxf(p[rows[i].y], 1e-9f));
        correct += (p[1] > p[0]) == rows[i].y;
    }
    *loss /= n;
    *acc = 100.0f * correct / n;
}
static int generate(const char *dir) {
    char path[512];
    mkdir(dir, 0755);
    for (int split = 0; split < 2; split++) {
        snprintf(path, sizeof path, "%s/%s.csv", dir, split ? "spiral-test" : "spiral-train");
        FILE *f = fopen(path, "w");
        if (!f) {
            perror(path);
            return 1;
        }
        fputs("x1,x2,label\n", f);
        int count = split ? 128 : 384;
        for (int c = 0; c < 2; c++)
            for (int i = 0; i < count; i++) {
                float r = (i + 0.5f) / count;
                float t = r * 7.0f + c * 3.14159265f + (uniform() - .5f) * .15f;
                fprintf(f, "%.7f,%.7f,%d\n", r * sinf(t), r * cosf(t), c);
            }
        if (fflush(f) || fsync(fileno(f)) || fclose(f)) {
            perror(path);
            return 1;
        }
    }
    puts("Created spiral-train.csv (768 rows) and spiral-test.csv (256 rows)");
    return 0;
}
static int train(const char *data, const char *weights, int epochs, const char *resume) {
    Model m = {0}, first = {0}, second = {0};
    int n = readrows(data);
    if (n < 0)
        return 1;
    if (resume) {
        if (load(resume, &m))
            return 1;
    } else {
        for (int j = 0; j < H; j++) {
            for (int k = 0; k < 2; k++)
                m.w1[j][k] = (uniform() - .5f) * 1.5f;
            m.b1[j] = (uniform() - .5f) * .3f;
            for (int k = 0; k < 2; k++)
                m.w2[k][j] = (uniform() - .5f) * .5f;
        }
    }
    char logpath[600];
    snprintf(logpath, sizeof logpath, "%s.metrics.csv", weights);
    FILE *log = fopen(logpath, "w");
    if (!log) {
        perror(logpath);
        return 1;
    }
    setvbuf(log, NULL, _IOLBF, 0);
    setvbuf(stdout, NULL, _IOLBF, 0);
    fputs("epoch,loss,accuracy\n", log);
    int order[MAX_ROWS];
    for (int i = 0; i < n; i++)
        order[i] = i;
    float beta1 = 1, beta2 = 1;
    int steps = 0;
    printf("Marrow Train | 2 -> %d tanh -> 2 softmax | %d parameters\nDataset %s | %d rows | Adam "
           "lr=0.008 batch=32\n",
           H, PARAMS, data, n);
    for (int e = 1; e <= epochs; e++) {
        for (int i = n - 1; i > 0; i--) {
            int j = random32() % (i + 1), q = order[i];
            order[i] = order[j];
            order[j] = q;
        }
        for (int b = 0; b < n; b += 32) {
            Model g = {0};
            int count = n - b < 32 ? n - b : 32;
            for (int i = 0; i < count; i++) {
                Row *r = &rows[order[b + i]];
                float a[H], p[2], da[H] = {0};
                forward(&m, r->x, a, p);
                p[r->y] -= 1;
                for (int k = 0; k < 2; k++) {
                    g.b2[k] += p[k];
                    for (int j = 0; j < H; j++) {
                        g.w2[k][j] += p[k] * a[j];
                        da[j] += p[k] * m.w2[k][j];
                    }
                }
                for (int j = 0; j < H; j++) {
                    float d = da[j] * (1 - a[j] * a[j]);
                    g.b1[j] += d;
                    for (int k = 0; k < 2; k++)
                        g.w1[j][k] += d * r->x[k];
                }
            }
            beta1 *= .9f;
            beta2 *= .999f;
            steps++;
            float *w = (float *)&m, *v = (float *)&first, *s = (float *)&second, *d = (float *)&g;
            for (int i = 0; i < PARAMS; i++) {
                float q = d[i] / count;
                v[i] = .9f * v[i] + .1f * q;
                s[i] = .999f * s[i] + .001f * q * q;
                w[i] -= .008f * (v[i] / (1 - beta1)) / (sqrtf(s[i] / (1 - beta2)) + 1e-8f);
            }
        }
        float loss, acc;
        metrics(&m, n, &loss, &acc);
        fprintf(log, "%d,%.6f,%.3f\n", e, loss, acc);
        if (e == 1 || e % 25 == 0 || e == epochs) {
            printf("epoch %4d/%d  loss %.6f  accuracy %.2f%%\n", e, epochs, loss, acc);
            if (save(weights, &m, e)) {
                fclose(log);
                return 1;
            }
            fsync(fileno(log));
        }
        if (!isfinite(loss)) {
            fprintf(stderr, "Non-finite loss; training stopped\n");
            fclose(log);
            return 1;
        }
    }
    if (fclose(log)) {
        perror(logpath);
        return 1;
    }
    printf("Saved %s (%zu bytes, checksummed, synced to disk)\n", weights, sizeof m + 32);
    return 0;
}
int main(int argc, char **argv) {
    if (argc == 3 && !strcmp(argv[1], "generate"))
        return generate(argv[2]);
    if (argc >= 4 && !strcmp(argv[1], "train")) {
        int e = argc > 4 ? atoi(argv[4]) : 600;
        if (e < 1 || e > 100000) {
            fprintf(stderr, "Epochs must be 1..100000\n");
            return 1;
        }
        return train(argv[2], argv[3], e, argc > 5 ? argv[5] : NULL);
    }
    if (argc == 4 && !strcmp(argv[1], "eval")) {
        Model m;
        if (load(argv[3], &m))
            return 1;
        int n = readrows(argv[2]);
        if (n < 0)
            return 1;
        float l, a;
        metrics(&m, n, &l, &a);
        int cm[2][2] = {{0}};
        for (int i = 0; i < n; i++) {
            float h[H], p[2];
            forward(&m, rows[i].x, h, p);
            cm[rows[i].y][p[1] > p[0]]++;
        }
        printf("Evaluation | %s\nRows: %d\nCross entropy: %.6f\nAccuracy: %.2f%%\nConfusion matrix "
               "(rows=true, columns=predicted):\n  %d %d\n  %d %d\n",
               argv[2], n, l, a, cm[0][0], cm[0][1], cm[1][0], cm[1][1]);
        return 0;
    }
    if (argc == 5 && !strcmp(argv[1], "infer")) {
        Model m;
        if (load(argv[2], &m))
            return 1;
        float x[2];
        char *end;
        for (int i = 0; i < 2; i++) {
            x[i] = strtof(argv[i + 3], &end);
            if (*end || !isfinite(x[i]))
                return 1;
        }
        float h[H], p[2];
        forward(&m, x, h, p);
        printf("class=%d p0=%.6f p1=%.6f\n", p[1] > p[0], p[0], p[1]);
        return 0;
    }
    if (argc == 3 && !strcmp(argv[1], "info")) {
        Model m;
        if (load(argv[2], &m))
            return 1;
        printf("Marrow weights v1\nArchitecture: 2 -> %d tanh -> 2 softmax\nParameters: %d "
               "(float32)\nChecksum: %08x\n",
               H, PARAMS, checksum(&m, sizeof m));
        return 0;
    }
    fprintf(stderr, "marrow-train generate DIR\nmarrow-train train DATA.csv WEIGHTS [EPOCHS] "
                    "[RESUME.weights]\nmarrow-train eval DATA.csv WEIGHTS\nmarrow-train infer "
                    "WEIGHTS X1 X2\nmarrow-train info WEIGHTS\n");
    return 1;
}
