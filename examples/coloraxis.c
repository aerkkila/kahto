#include <kahto.h>
#include <cmh_enum.h>

int main() {
	float y[100], z[100];
	for (int i=0; i<100; i++)
		y[i] = i%6 + 0.1*i/6,
		z[i] = i-10;
	struct kahto_figure *fig0 =
		kahto_yz(y, z, 100, .caxis_center=0, .cmh_enum=cmh_coolwarm_e);
	struct kahto_figure *fig1 =
		kahto_yz(y, z, 100, .cmh_enum=cmh_coolwarm_e);
	struct kahto_figure *fig = kahto_subfigures_new(1, 2);
	fig->wh[0] = 1000;
	fig->wh[1] = 500;
	fig->subfigures[0] = fig0;
	fig->subfigures[1] = fig1;
	fig0->margin[2] = 0.03;
	fig1->margin[0] = 0.03;
	kahto_show(fig);
}
