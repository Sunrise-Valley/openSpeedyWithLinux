#ifndef OPENSPEEDY_H
#define OPENSPEEDY_H

/* Shared memory protocol between speedctl and libopenspeedy.so */

#define SHM_NAME "/openspeedy_shm"

/* Speed state stored in shared memory */
struct speed_state {
    double multiplier;      /* 1.0 = normal, 2.0 = 2x speed, 0.5 = half speed */
    volatile int enabled;   /* 1 = speed mod active, 0 = bypass */
};

#endif /* OPENSPEEDY_H */
