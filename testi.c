#define using_cplot
#include "cplot.h"
#include <stdlib.h>

int main() {
    const int pit = 50;
    float ydata[pit];
    for (int i=0; i<pit; i++)
	ydata[i] = rand() % (i+1) + 0.1;
    struct $axes *axes = cplot_y(ydata, .len=pit, .linestyle="-", .line_thickness=1.0/500);
    $axislabel($xaxis0(axes), "x-nimiö\n\033[4;91mtoinen rivi\033[0m");
    $axislabel($yaxis0(axes), "y-nimiö\njatkuu täällä");
    //$xaxis0(axes)->range_isset = 3;
    $show(axes);
    $free(axes);
}
