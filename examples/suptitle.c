#include <cplot.h>
#include <string.h>
#include <stdio.h>

int main() {
	struct cplot_figure *fig = cplot_subplots_new(3,4);
	for (int i=0; i<12; i++) {
		fig->children[i] = cplot_figure_bare_new();
		fig->children[i]->background = cplot_colorschemes[0][i%cplot_ncolors[0]];
		char title[30];
		sprintf(title, "figure %i", i);
		fig->children[i]->title.text = strdup(title);
		fig->children[i]->title.owner = 1;
	}
	fig->title.text = "suptitle";
	cplot_show(fig);
}
