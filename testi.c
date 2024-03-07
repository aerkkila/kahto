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
	ydata[i] = cos(2*π/pit*10 * i) * i * 0.0001;
	xdata[i] = sin(2*π/pit*10 * i) * i * 0.1;
    }
    struct $axes *axes = cplot_yx(ydata, xdata, .len=pit, .linestyle="-", .line_thickness=1.0/500, .label="nimiö");

    const int pit1 = 45;
    float ydata1[pit1], xdata1[pit1];
    for (int i=0; i<pit1; i++) {
	ydata1[i] = cos(2*π/pit1*5 * i) * i * 0.0001;
	xdata1[i] = sin(2*π/pit1*5 * i) * i * 0.1;
    }
    cplot_yx(ydata1, xdata1, .len=pit1, .linestyle="-", .label="toinen", .line_thickness=1.0/800, .axes=axes);

    const int pit2 = 20;
    float xdata2[pit2], ydata2[pit2];
    for (int i=0; i<pit2; i++) {
	ydata2[i] = cos(i*0.1) * i * 0.0001;
	xdata2[i] = i*0.1;
    }
    cplot_yx(ydata2, xdata2, .len=pit2, .marker="+", .literal_marker=1, .axes=axes, .markersize=1.0/50);

    $axislabel($xaxis0(axes), "x-nimiö\n\033[4;91mtoinen rivi\033[0m");
    $axislabel($yaxis0(axes), "y-nimiö\njatkuu täällä");

    cplot_write_png(axes, "testi.png");
    cplot_show(axes);

    $free(axes);
}
