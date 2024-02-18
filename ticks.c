#include <stdio.h>
#define nticks 10

int get_nticks_basic(double min, double max) {
    return nticks;
}

double get_tick_basic(int ind, double min, double max, char *out, int sizeout) {
    double väli = (max-min) / (nticks-1);
    snprintf(out, sizeout, "%.1f", ind*väli + min);
    out[sizeout-1] = '\0';
    return ind * väli - min;
}

#undef nticks
