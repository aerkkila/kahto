#include "cplot.h"
#include <stdlib.h>
#include <math.h>
#include <stdio.h>
#include <time.h>

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
    struct cplot_axis *x1axis = cplot_axis_new(layout->axes[3], 'x');

    struct tm tm = {.tm_year=2005-1900, .tm_mday=130};
    time_t aika = timegm(&tm);
    long askel = 80*86400;
    long ajat[pit2];
    for (int i=0; i<pit2; i++)
	ajat[i] = aika + askel * i;

    cplot_yx(ydata2, ajat, .len=pit2, .xaxis=x1axis, .yaxis=y1axis, .linestyle.style=1, .markersize=1.0/40, .label = "iso pallo");
    x1axis->ticks->ticker.init = cplot_init_ticker_datetime;
    x1axis->ticks->rotation100 = 25;
    cplot_axislabel(x1axis, "datetime-tikkeri");
    cplot_axislabel(y1axis, "oikia");
    y1axis->text[y1axis->ntext-1]->rotation100 = 0;
    cplot_axislabel(cplot_xaxis0(layout->axes[3]), "x-akseli alhaalla");
    cplot_axislabel(cplot_yaxis0(layout->axes[3]), "yksinkertaiset tikit");
    layout->axes[3]->legend.borderstyle.style = 1;
    layout->axes[3]->legend.borderstyle.thickness = 1.0/100;
    layout->axes[3]->legend.borderstyle.color = 0xff9adf49;
    layout->axes[3]->axis[cplot_ix0axis]->ticks->ticker.tickerdata.lin.target_nticks = 4;
    layout->axes[3]->axis[cplot_iy0axis]->ticks->ticker.init = cplot_init_ticker_simple;
    layout->axes[3]->legend.fillcolor = 0xffc9b9c9;
    layout->axes[3]->legend.fill = cplot_fill_color_e;

    /* ------------------------- */

    int kohina[pit];
    {
	int pit = 21;
	float *y = malloc(pit * sizeof(float));
	for (int i=0; i<pit; i++) {
	    y[i] = i-pit/2;
	    y[i] *= y[i];
	}
	layout->axes[1] = cplot_y(y, .len=pit, .linestyle.style=1, .yxzowner[0]=1, .marker="x", .literal_marker=1, .color=0xdd0077c7, .label="paraabeli");
	struct cplot_ticker *tk = &layout->axes[1]->axis[cplot_iy0axis]->ticks->ticker;
	tk->init = cplot_init_ticker_arbitrary_relcoord;
	tk->tickerdata.arb.nticks = 5;
	tk->tickerdata.arb.ticks = (static double[]){0, 0.15, 0.5, 0.8, 1};
	tk->tickerdata.arb.labels = (static char*[]){"0", "3×5", "\033[4;38;5;207;48;5;19mkeskikohta\033[0m", "80", "100"};

	for (int i=0; i<pit; i++)
	    kohina[i] = rand() % pit + i;
	struct cplot_axis *y1axis = cplot_axis_new(layout->axes[1], 'y');
	y1axis->pos = 1;
	cplot_yz(kohina, y, .len=pit, .yaxis=y1axis, .markersize=1/60.0,
	    .label="värillinen kohina", .cmh_enum=cmh_turbo_e);
    }

    cplot_write_png(layout, "testi.png");
    cplot_show(layout);

    cplot_destroy(layout);
}
