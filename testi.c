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
    struct cplot_axes *axes = cplot_yx(ydata, xdata, .len=pit, .linestyle.style=cplot_line_normal_e,);

    const int pit1 = 45;
    float ydata1[pit1], xdata1[pit1];
    for (int i=0; i<pit1; i++) {
	ydata1[i] = cos(2*π/pit1*5 * i) * i * 0.0001;
	xdata1[i] = sin(2*π/pit1*5 * i) * i * 0.1;
    }
    cplot_yx(ydata1, xdata1, .len=pit1, .linestyle.style=1, .label="toinen nimi", .linestyle.thickness=1.0/800, .axes=axes);

    const int pit2 = 20;
    float xdata2[pit2], ydata2[pit2];
    for (int i=0; i<pit2; i++) {
	ydata2[i] = cos(i*0.1) * i * 0.0001;
	xdata2[i] = i*0.1;
    }
    cplot_yx(ydata2, xdata2, .len=pit2, .marker="+", .literal_marker=1, .axes=axes, .markersize=1.0/50);

    cplot_axislabel(cplot_xaxis0(axes), "x-nimiö\n\033[4;91mtoinen rivi\033[0m");
    cplot_axislabel(cplot_yaxis0(axes), "y-nimiö\njatkuu täällä");

    /* ----------------------- */

    struct cplot_layout *layout = cplot_layout_new(2, 2);
    layout->background = 0;
    layout->axes[0] = axes;
    layout->axes[3] = cplot_y(ydata1, .linestyle.style=2, .len=pit1, .label = "testi");

    struct cplot_axis *y1axis = cplot_axis_new(layout->axes[3], 'y');
    y1axis->pos = 1;
    y1axis->ticks = cplot_ticks_new(y1axis);
    struct cplot_axis *x1axis = cplot_axis_new(layout->axes[3], 'x');
    x1axis->ticks = cplot_ticks_new(x1axis);
    cplot_yx(ydata2, xdata2, .len=pit2, .xaxis=x1axis, .yaxis=y1axis, .linestyle.style=1, .markersize=1.0/40, .label = "iso pallo");
    cplot_axislabel(x1axis, "x-akseli ylhäällä");
    cplot_axislabel(y1axis, "oikia");
    y1axis->text[y1axis->ntext-1]->rotation100 = 0;
    cplot_axislabel(cplot_xaxis0(layout->axes[3]), "x-akseli alhaalla");
    cplot_axislabel(cplot_yaxis0(layout->axes[3]), "y-akseli vasemmalla");

    /* ------------------------- */

    {
	int pit = 21;
	float *y = malloc(pit * sizeof(float));
	for (int i=0; i<pit; i++) {
	    y[i] = i-pit/2;
	    y[i] *= y[i];
	}
	layout->axes[1] = cplot_y(y, .len=pit, .linestyle.style=1, .yxzowner[0]=1);
    }

    cplot_write_png(layout, "testi.png");
    cplot_show(layout);

    cplot_destroy(layout);
    cplot_fini();
}
