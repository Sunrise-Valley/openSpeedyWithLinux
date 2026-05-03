#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <unistd.h>
#include <time.h>

int main()
{
    struct timespec start, end;
    double elapsed;

    clock_gettime(CLOCK_MONOTONIC, &start);

    printf("Sleeping for 2 seconds...\n");
    sleep(2);

    clock_gettime(CLOCK_MONOTONIC, &end);

    elapsed = (end.tv_sec - start.tv_sec) +
              (end.tv_nsec - start.tv_nsec) / 1e9;

    printf("Elapsed (real): %.3f seconds\n", elapsed);
    printf("With 2x speed: clock_gettime shows 2x elapsed (game sees 2s in 1s)\n");
    printf("Without speed:  clock_gettime shows real elapsed (~2s)\n");

    return 0;
}
