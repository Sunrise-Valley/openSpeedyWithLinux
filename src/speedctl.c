#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <errno.h>
#include "openspeedy.h"

static struct speed_state *shm = NULL;
static int shm_fd = -1;

/* Create or open shared memory */
static int shm_init(void)
{
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return -1;
    }

    if (ftruncate(shm_fd, SHM_SIZE) == -1) {
        perror("ftruncate");
        close(shm_fd);
        return -1;
    }

    shm = mmap(NULL, SHM_SIZE, PROT_READ | PROT_WRITE,
               MAP_SHARED, shm_fd, 0);
    if (shm == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        return -1;
    }

    /* Init if first time */
    if (shm->version == 0) {
        shm->version = 1;
        shm->multiplier = 1.0;
        shm->enabled = 0;
        shm->cmd_seq = 0;
        shm->controller_pid = getpid();
    }

    return 0;
}

static void shm_cleanup(void)
{
    if (shm && shm != MAP_FAILED)
        munmap(shm, SHM_SIZE);
    if (shm_fd != -1)
        close(shm_fd);
}

static void send_cmd(double multiplier, int enabled)
{
    shm->multiplier = multiplier;
    shm->enabled = enabled;
    shm->cmd_seq++;
    shm->controller_pid = getpid();
}

static void cmd_status(void)
{
    if (shm_init() != 0) {
        printf("No active speed modifier (shared memory not found)\n");
        return;
    }
    printf("Multiplier: %.2fx\n", shm->multiplier);
    printf("Status:     %s\n", shm->enabled ? "ACTIVE" : "inactive");
    printf("CmdSeq:     %lu\n", (unsigned long)shm->cmd_seq);
    shm_cleanup();
}

static void cmd_set(const char *arg)
{
    double mult = atof(arg);
    if (mult <= 0.0) {
        fprintf(stderr, "Invalid multiplier: %s (must be > 0)\n", arg);
        exit(1);
    }
    if (shm_init() != 0) exit(1);
    send_cmd(mult, (mult != 1.0) ? 1 : 0);
    printf("Speed set to %.2fx\n", mult);
    shm_cleanup();
}

static void cmd_reset(void)
{
    if (shm_init() != 0) exit(1);
    send_cmd(1.0, 0);
    printf("Speed reset to 1.0x\n");
    shm_cleanup();
}

static void cmd_off(void)
{
    if (shm_init() != 0) exit(1);
    send_cmd(shm->multiplier, 0);
    printf("Speed modifier disabled (multiplier kept at %.2fx)\n",
           shm->multiplier);
    shm_cleanup();
}

static void cmd_run(int argc, char *argv[])
{
    (void)argc; /* unused — we use argc-2 at call site */
    char lib_path[4096];
    /* Find libopenspeedy.so relative to speedctl binary location */
    const char *installed = "/usr/local/lib/libopenspeedy.so";
    if (access(installed, F_OK) == 0) {
        strcpy(lib_path, installed);
    } else {
        /* Use path relative to the binary */
        strcpy(lib_path, "build/libopenspeedy.so");
    }

    setenv("LD_PRELOAD", lib_path, 1);
    execvp(argv[0], argv);
    perror("execvp");
    exit(1);
}

static void print_usage(void)
{
    printf(
        "OpenSpeedy Linux — Game speed modifier\n"
        "\n"
        "Usage: speedctl <command> [args]\n"
        "\n"
        "Commands:\n"
        "  status         Show current speed multiplier and status\n"
        "  set <N>        Set speed multiplier (e.g. 2.0 = 2x speed)\n"
        "  reset          Reset to 1.0x normal speed\n"
        "  off            Disable speed modifier\n"
        "  run <cmd...>   Run command with LD_PRELOAD set for speed mod\n"
        "\n"
        "Examples:\n"
        "  speedctl set 3.0              # Set 3x speed\n"
        "  speedctl run ./my_game        # Run game with speed mod\n"
        "  speedctl status               # Check current state\n"
    );
}

int main(int argc, char *argv[])
{
    if (argc < 2) {
        print_usage();
        return 1;
    }

    if (strcmp(argv[1], "status") == 0) {
        cmd_status();
    } else if (strcmp(argv[1], "set") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: speedctl set <multiplier>\n");
            return 1;
        }
        cmd_set(argv[2]);
    } else if (strcmp(argv[1], "reset") == 0) {
        cmd_reset();
    } else if (strcmp(argv[1], "off") == 0) {
        cmd_off();
    } else if (strcmp(argv[1], "run") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Usage: speedctl run <program> [args...]\n");
            return 1;
        }
        cmd_run(argc - 2, &argv[2]);
    } else if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0
               || strcmp(argv[1], "-h") == 0) {
        print_usage();
    } else {
        fprintf(stderr, "Unknown command: %s\n", argv[1]);
        print_usage();
        return 1;
    }

    return 0;
}
