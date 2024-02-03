#define using_cplot
#include "cplot.h"

int main() {
    struct $axes *axes = $plot(0);
    $show(axes);
    $free(axes);
}
