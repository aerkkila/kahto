#include <stdio.h>
#define nticks 10

int get_nticks_basic(float min, float max) {
    return nticks;
}

float get_tick_basic(int ind, float min, float max, char *out, int sizeout) {
    float väli = (max-min) / (nticks-1);
    snprintf(out, sizeout, "%.1f", ind*väli + min);
    out[sizeout-1] = '\0';
    return ind * väli - min;
}

#undef nticks
