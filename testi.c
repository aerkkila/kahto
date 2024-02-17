#define using_cplot
#include "cplot.h"

int main() {
    struct $axes *axes = $plot(0);
    $axislabel($xaxis0(axes), "x-nimiö\n\033[4;91mtoinen rivi\033[0m");
    $axislabel($yaxis0(axes), "y-nimiö\njatkuu täällä");
    $show(axes);
    $free(axes);
}
