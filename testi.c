#include "kahto.h"
#include <cmh_enum.h>
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
		xdata[i] = sin(2*π/pit*10 * i) * i * 0.1 + 6000;
	}
	struct kahto_axes *axes0 = kahto_yx(ydata, xdata, pit, .linestyle.style=kahto_line_normal_e,);
	axes0->title.text = "\e$nmol m^-2 s^-1 CH_{3}OH$";

	const int pit1 = 45;
	float ydata1[pit1], xdata1[pit1];
	for (int i=0; i<pit1; i++) {
		ydata1[i] = cos(2*π/pit1*5 * i) * i * 0.0001;
		xdata1[i] = sin(2*π/pit1*5 * i) * i * 0.1 + 6000;
	}
	kahto_yx(ydata1, xdata1, pit1, .label="toinen nimi", .axes=axes0,
		.linestyle.style=1, .linestyle.thickness=1.0/800);

	const int pit2 = 20;
	float xdata2[pit2], ydata2[pit2];
	for (int i=0; i<pit2; i++) {
		ydata2[i] = cos(i*0.1) * i * 0.0001;
		xdata2[i] = i*0.1 + 6000;
	}
	kahto_yx(ydata2, xdata2, pit2, .axes=axes0, .markerstyle.marker="+", 1.0/50, .markerstyle.literal=1);

	kahto_axislabel(axes0->axis[0], "x-nimiö\n\033[4;91mtoinen rivi\033[0m");
	kahto_axislabel(axes0->axis[1], "y-nimiö\njatkuu täällä");
	axes0->axis[1]->ticks->rowheight = 1./10;
	axes0->legend.visible = -1; // only if empty slot is available

	/* ----------------------- */

	struct kahto_axes *axes3 = kahto_y(ydata1, .linestyle.style=2, .len=pit1, .label = "testi");
	struct kahto_axis *y1axis = kahto_axis_new(axes3, 'y', 1);
	y1axis->pos = 1;
	struct kahto_axis *x1axis = kahto_axis_new(axes3, 'x', 0);

	struct tm tm = {.tm_year=2005-1900, .tm_mday=130};
	time_t aika = timegm(&tm);
	long askel = 80*86400;
	long ajat[pit2];
	for (int i=0; i<pit2; i++)
		ajat[i] = aika + askel * i;
	kahto_yx(ydata2, ajat, .len=pit2, .xaxis=x1axis, .yaxis=y1axis, .linestyle.style=1, .markerstyle.size=1.0/40, .label = "iso pallo");

	x1axis->ticks->init = kahto_init_ticker_datetime;
	x1axis->ticks->rotation_grad = 100;
	kahto_axislabel(x1axis, "datetime-tikkeri");
	kahto_axislabel(y1axis, "oikia");
	y1axis->text[y1axis->ntext-1]->rotation_grad = -20;
	kahto_axislabel(axes3->axis[0], "x-akseli alhaalla");
	kahto_axislabel(axes3->axis[1], "yksinkertaiset tikit")->rotation_grad = 100;
	axes3->legend.borderstyle.style = 1;
	axes3->legend.borderstyle.thickness = 1.0/100;
	axes3->legend.borderstyle.color = 0xff9adf49;
	axes3->axis[0]->ticks->tickerdata.lin.target_nticks = 4;
	axes3->axis[1]->ticks->init = kahto_init_ticker_simple;
	axes3->legend.fillcolor = 0xffc9b9c9;
	axes3->legend.fill = kahto_fill_color_e;
	/* ------------------------- */

	int kohina[pit];
	struct kahto_axes *axes1 = kahto_axes_new();
	{
		int pit = 21;
		float *y = malloc(pit * sizeof(float));
		for (int i=0; i<pit; i++) {
			y[i] = i-pit/2;
			y[i] *= y[i];
		}
		kahto_y(y, pit, .axes=axes1, .linestyle.style=1, .yxzowner[0]=1,
			.markerstyle.marker="x", .markerstyle.literal=1, .color=0xdd0077c7, .label="paraabeli");
		struct kahto_ticks *tk = axes1->axis[kahto_iy0axis]->ticks;
		tk->init = kahto_init_ticker_arbitrary_relcoord;
		tk->tickerdata.arb.nticks = 5;
		tk->tickerdata.arb.ticks = (static double[]){0, 0.15, 0.5, 0.8, 1};
		tk->tickerdata.arb.labels = (static char*[]){"0", "3×5", "\033[4;38;5;207;48;5;19mkeskikohta\033[0m", "80", "100"};

		for (int i=0; i<pit; i++)
			kohina[i] = rand() % pit + i;
		struct kahto_axis *y1axis = kahto_axis_new(axes1, 'y', 1);
		y1axis->pos = 1;
		struct kahto_axis *caxis = kahto_coloraxis_new(axes1, 'y');
		caxis->pos = 1;
		kahto_yz(kohina, y, pit, .yaxis=y1axis, .caxis=caxis, .markerstyle.size=1/60.0,
			.label="värillinen kohina", .cmh_enum=cmh_turbo_e);
	}

	struct kahto_subplots *subplots = kahto_subplots_new(2, 2);
	subplots->background = 0;
	subplots->axes[0] = axes0;
	subplots->axes[1] = axes1;
	subplots->axes[3] = axes3;

	kahto_write_png(subplots, "testi.png");
	kahto_show(subplots);

	kahto_destroy(subplots);
}
