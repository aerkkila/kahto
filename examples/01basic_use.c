#include <cplot.h>
#include <math.h>
#define len 200

int main() {
	float a0[len], a1[len];
	short b0[len], b1[len];
	double c0[len];

	for (int i=0; i<len; i++) {
		b0[i] = a0[i] = log(i+1) * 10;
		b1[i] = a1[i] = pow((i+1), -0.3) * 10;
		c0[i] = pow(i*0.01, sin(i*0.01)) * 70 - 50;
	}

	/* One method to draw multiple graphs into same axes. */
	struct cplot_axes *axes0 = cplot_y(a0, len);	// data type is automatically recognized
	cplot_y(b0, len, .axes=axes0);					// data type is automatically recognized
	cplot_y(c0, len, .axes=axes0, cplot_lineargs);
	axes0->title.text = "10 log(i+1)";

	/* Another method to draw multiple graphs into same axes. */
	struct cplot_axes *axes1 = NULL;
	cplot_y(a1, len, .axesptr=&axes1, .label="real");
	cplot_y(b1, len, .axesptr=&axes1, .label="integer", .markerstyle.marker="4");
	axes1->title.text = "\e$10 (i+1)^-0.3$";

	/* Third method */
	struct cplot_axes *axes2 = cplot_axes_new();
	cplot_yx(a0, b0, len, .axes=axes2, .label="10 log(i+1)");
	cplot_yx(a1, b1, len, .axes=axes2, .label="\e$10 (i+1)^-0.3$");
	cplot_axislabel(axes2->axis[cplot_iy], "real");
	cplot_axislabel(axes2->axis[cplot_ix], "integer");
	axes2->legend.rowheight = 1.0 / 25; // defined as the fraction of figure height

	/* We can show any axes individually. */
	cplot_write_png_preserve(axes1, "01a.png");
	//cplot_show_preserve(axes1); // shows the figure and halts the program until the window is closed

	/* Or we can combine them into a single figure. */
	struct cplot_subplots *subplots = cplot_subplots_new(2, 2);
	subplots->axes[0] = axes0;
	subplots->axes[1] = axes1;
	subplots->axes[3] = axes2;
	cplot_write_png(subplots, "01b.png");
}
