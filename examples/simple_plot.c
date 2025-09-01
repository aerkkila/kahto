#include <kahto.h>
#include <math.h>
#define len 200

int main(int argc, char **argv) {
	float f4a[len], f4b[len];
	short i2a[len], i2b[len];
	double f8a[len];

	for (int i=0; i<len; i++) {
		i2a[i] = f4a[i] = log(i+1) * 10;
		i2b[i] = f4b[i] = pow((i+1), -0.3) * 10;
		f8a[i] = pow(i*0.01, sin(i*0.01)) * 70 - 50;
	}

	/* Drawing multiple graphs to same figure. */
	struct kahto_figure *fig0 = kahto_figure_new();
	kahto_yx(f4a, i2a, len, .figure=fig0, .label="10 log(i+1)");       // data type is automatically recognized
	kahto_yx(f4b, i2b, len, .figure=fig0, .label="\e$10 (i+1)^-0.3$"); // data type is automatically recognized
	kahto_axislabel(kahto_yaxis0(fig0), "real");
	kahto_axislabel(kahto_xaxis0(fig0), "integer");
	fig0->legend.rowheight = 1.0 / 25; // defined as the fraction of figure height

	/* The figure can also be obtained from the plotting function
	   instead of explicitely calling kahto_figure_new().
	   In that case, the figure-argument is omitted. */
	struct kahto_figure *fig1 = kahto_y(f4a, len);
	kahto_y(i2a, len, .figure=fig1);
	kahto_y(f8a, len, .figure=fig1, kahto_lineargs);
	fig1->title.text = "10 log(i+1)";

	/* Usually the methods above are sufficient but sometimes the following syntax is useful. */
	struct kahto_figure *fig2 = NULL;
	kahto_y(f4b, len, .figureptr=&fig2, .label="real");
	kahto_y(i2b, len, .figureptr=&fig2, .label="integer", .markerstyle.marker="4");
	fig2->title.text = "\e$10 (i+1)^-0.3$";

	/* We can show any figure individually. */
	if (argc == 1)
		kahto_write_png_preserve(fig2, "01a.png");
	else
		kahto_show_preserve(fig2); // shows the figure and halts the program until the window is closed

	/* The functions kahto_show and kahto_write_png destroy the figure in the end.
	   To be able to use it again, we used functions *_preserve which don't destroy the figure. */

	/* We can also use the figures as subfigures in a larger figure. */
	struct kahto_figure *fig = kahto_subfigures_new(2, 2);
	fig->subfigures[0] = fig0;
	fig->subfigures[1] = fig1;
	fig->subfigures[3] = fig2;

	if (argc == 1)
		kahto_write_png(fig, "01b.png");
	else
		kahto_show(fig);

	/* The two functions above destroy the figures in the end and thus, all memory is freed now. */
}
