#include <kahto.h>
#include <string.h>
#include <stdio.h>

int main() {
	struct kahto_figure *fig = kahto_subfigures_new(3,4);
	for (int i=0; i<12; i++) {
		fig->subfigures[i] = kahto_figure_void_new();
		fig->subfigures[i]->background = kahto_colorschemes[0][i%kahto_ncolors[0]];
		char title[30];
		sprintf(title, "figure %i", i);
		fig->subfigures[i]->title.text = strdup(title);
		fig->subfigures[i]->title.owner = 1;
	}
	fig->title.text = "suptitle";
	kahto_show(fig);
}
