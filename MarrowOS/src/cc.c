#define _GNU_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
static int run(char *const a[]) {
    pid_t p = fork();
    if (!p) {
        execvp(a[0], a);
        perror(a[0]);
        _exit(127);
    }
    if (p < 0)
        return 1;
    int status;
    while (waitpid(p, &status, 0) < 0)
        if (errno != EINTR)
            return 1;
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}
int main(int argc, char **argv) {
    char **args = calloc(argc + 2, sizeof *args), **temps = calloc(argc, sizeof *temps);
    if (!args || !temps)
        return 1;
    args[0] = "tcc";
    int result = 0;
    for (int i = 1; i < argc; i++) {
        args[i] = argv[i];
        size_t n = strlen(argv[i]);
        if (n > 2 && argv[i][n - 2] == '.' && (argv[i][n - 1] == 'S' || argv[i][n - 1] == 's')) {
            char name[] = "/tmp/marrow-asm-XXXXXX.o";
            int fd = mkstemps(name, 2);
            if (fd < 0) {
                perror("temporary object");
                result = 1;
                goto cleanup;
            }
            close(fd);
            temps[i] = strdup(name);
            char *as[] = {"as", "--64", "-o", name, argv[i], NULL};
            if (run(as)) {
                result = 1;
                goto cleanup;
            }
            args[i] = temps[i];
        }
    }
    result = run(args);
cleanup:
    for (int i = 1; i < argc; i++)
        if (temps[i]) {
            unlink(temps[i]);
            free(temps[i]);
        }
    free(args);
    free(temps);
    return result;
}
