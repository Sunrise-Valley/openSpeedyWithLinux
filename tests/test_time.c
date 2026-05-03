#define _POSIX_C_SOURCE 199309L
#include <stdio.h>
#include <unistd.h>
#include <time.h>

int main()
{
    struct timespec t1, t2, t3, t4;
    double elapsed_mono, elapsed_real;

    /* Measure 1 second of real sleep */
    clock_gettime(CLOCK_MONOTONIC, &t1);
    clock_gettime(CLOCK_REALTIME, &t3);
    sleep(1);
    clock_gettime(CLOCK_MONOTONIC, &t2);
    clock_gettime(CLOCK_REALTIME, &t4);

    elapsed_mono = (t2.tv_sec - t1.tv_sec) +
                   (t2.tv_nsec - t1.tv_nsec) / 1e9;
    elapsed_real = (t4.tv_sec - t3.tv_sec) +
                   (t4.tv_nsec - t3.tv_nsec) / 1e9;

    printf("CLOCK_MONOTONIC elapsed: %.3f seconds\n", elapsed_mono);
    printf("CLOCK_REALTIME elapsed:  %.3f seconds\n", elapsed_real);
    printf("With 2x speed, MONOTONIC should be ~2.0s, REALTIME ~1.0s\n");

    return 0;
}
