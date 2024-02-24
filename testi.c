#define using_cplot
#include "cplot.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>

#define π 3.14159265358979

int main() {
    const int pit = 45;
    float ydata[pit], xdata[pit];
    for (int i=0; i<pit; i++) {
	ydata[i] = cos(2*π/pit*10 * i) * i * 0.1;
	xdata[i] = sin(2*π/pit*10 * i) * i * 0.1;
    }
    struct $axes *axes = cplot_yx(ydata, xdata, .len=pit, .linestyle="-", .line_thickness=1.0/500);
    $axislabel($xaxis0(axes), "x-nimiö\n\033[4;91mtoinen rivi\033[0m");
    $axislabel($yaxis0(axes), "y-nimiö\njatkuu täällä");

    cplot_show(axes);

    $free(axes);
}
