#include <kahto.h>
#include <math.h>

#define xlen 9
#define ylen 6
#define pi 3.14159265358979
int main() {
	float zdata[ylen][xlen];
	for (int i=0; i<ylen; i++)
		for (int ii=0; ii<xlen; ii++)
			zdata[i][ii] = cos(i * 2*pi / ylen) + cos(ii * 3*pi / xlen);

	kahto_show(
		kahto_colormesh(zdata[0], ylen, xlen));
}
