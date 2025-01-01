#include <cplot.h>

int main() {
	int a[] = {0, 3, -1, 2, 4};
	cplot_destroy(cplot_show(cplot_y(a, 5)));
}
