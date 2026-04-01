#include <kahto.h>
/* useful for making a shared legend for multiple subfigures */
int main() {
	int a = 0;
	struct kahto_figure *fig = kahto_y(&a, 0, .label="default");
	kahto_y(&a, 0, .figure=fig, .markerstyle.marker="^", 0.02, 0.6, .label="triangle");
	kahto_y(&a, 0, .figure=fig, .legend_coloronly=1, .label="color only");
	kahto_show(fig);
}
