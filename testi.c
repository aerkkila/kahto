#define using_cplot
#include "cplot.h"

int main() {
    struct $axes *axes = $plot(0);
    $axislabel(axes->axis[0], "testi");
    $axislabel(axes->axis[1], "ytesti");
    $show(axes);
    $free(axes);
}
