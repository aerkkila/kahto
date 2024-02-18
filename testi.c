#define using_cplot
#include "cplot.h"
#include <stdlib.h>

int main() {
    const int pit = 10000;
    float ydata[pit];
    for (int i=0; i<pit; i++)
	ydata[i] = (double)rand() / RAND_MAX / 10 + (float)(i%500)/500;
    struct $axes *axes = $plot(.data.yxzdata[0]=ydata, .data.yxztype[0]=$type(*ydata), .data.length=pit);
    $axislabel($xaxis0(axes), "x-nimiö\n\033[4;91mtoinen rivi\033[0m");
    $axislabel($yaxis0(axes), "y-nimiö\njatkuu täällä");
    $show(axes);
    $free(axes);
}
