#include <stdio.h>
#include <kahto.h>
int main() {
	int a = 0;
	/* A standalone legend: */
	struct kahto_figure *fig = kahto_y(&a, 0, .label="default");
	kahto_y(&a, 0, .figure=fig, .markerstyle.marker="^", 0.02, 0.6, .label="triangle");
	kahto_y(&a, 0, .figure=fig, .legend_coloronly=1, .label="color only");
	kahto_show(fig);

	/*================================================================*/

	/* Use a standalone legend as a shared legend for two figures: */
	int data[3][10]; // make some data to plot
	for (int ii=0; ii<3; ii++)
		for (int iii=0; iii<10; iii++)
			if ((data[ii][iii] = ii*iii - iii/(ii+1) + ii*ii-iii*iii) <= 0)
				data[ii][iii] = -data[ii][iii] + 1;

	fig = kahto_subfigures_new(2, 1); // 2 rows, 1 column
	// share first row to two columns
	struct kahto_figure *fig0 = fig->subfigures[0] = kahto_subfigures_new(1, 2);
	// make the two figures
	for (int i=0; i<2; i++) {
		struct kahto_figure *f = fig0->subfigures[i] = kahto_figure_new();
		for (int ii=0; ii<3; ii++)
			kahto_y(data[ii], 10, .figure=f);
	}
	kahto_gly(fig0->subfigures[1])->logscale = 1;
	// make the shared legend
	struct kahto_figure *f = fig->subfigures[1] = kahto_figure_new();
	for (int ii=0; ii<3; ii++) {
		char *a;
		asprintf(&a, "name %i", ii);
		kahto_y(data[ii], 0, .figure=f, .label=a, .labelowner=1);
	}

	/* First and second row would have the same height, but second row only contains the legend,
	   which does not use the whole height.
	   The figure height is automatically made smaller to match the used size. */
	kahto_show(fig);
}
