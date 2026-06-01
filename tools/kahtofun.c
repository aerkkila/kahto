/* Plots a function given as a command line argument.
   The function should be valid c-code with x as the variable.
   For example
   >>> kahtofun -a -20 -b 20 'x*x*x * log(x < 0 ? fabs(sin(x)) : x) * sin(x*x*x*0.2)'
   */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <kahto.h>
#include <dlfcn.h>
#include <string.h>
#include <err.h>
#include <time.h>

const char muoto[] =
"#include <math.h>\n"
"#include <stdlib.h>\n" // at least rand() might be wanted
"const double pi = 3.14159265358979L;\n"
"double funktio(double x) {\n"
"	%s;\n"
"	return %s;\n"
"}\n";

void* käännä(const char *sisälmys, const char *määrittele) {
	char *lähde;
	int pituus = asprintf(&lähde, muoto, määrittele, sisälmys);

	char nimi_so[128];
	srand(time(NULL));
	sprintf(nimi_so, "/tmp/funktio_%i.so", rand());

	/* kääntäjä */
	const char *ldlibs = getenv("LDLIBS");
	const char *cflags = getenv("CFLAGS");
	cflags=cflags?cflags:"";
	ldlibs=ldlibs?ldlibs:"";
	int len = snprintf(NULL, 0, "cc %s -fpic -shared -o %s -x c - %s", cflags, nimi_so, ldlibs);
	char *cmd = malloc(len+1);
	snprintf(cmd, len+1, "cc %s -fpic -shared -o %s -x c - %s", cflags, nimi_so, ldlibs);
	FILE *prog = popen(cmd, "w");
	if (!prog)
		err(1, "popen %s", cmd);
	free(cmd);

	fwrite(lähde, 1, pituus-1, prog);
	pclose(prog);
	free(lähde);

	void *kahva = dlopen(nimi_so, RTLD_NOW);
	if (!kahva) {
		warn("dlopen %s", nimi_so);
		if (unlink(nimi_so) < 0)
			warn("unlink %s", nimi_so);
		exit(3);
	}
	if (unlink(nimi_so) < 0)
		warn("unlink %s", nimi_so);
	return kahva;
}

int main(int argc, char **argv) {
	double alku = -5,
		   loppu = 5;
	int n = 512, opt;
	char ylog = 0, xlog = 0, equal_xy = 0;
	char *määrittele = "";

	while ((opt = getopt(argc, argv, "a:b:n:yxed:")) >= 0)
		switch (opt) {
			case 'a': alku = atof(optarg); break;
			case 'b': loppu = atof(optarg); break;
			case 'n': n = atoi(optarg); break;
			case 'y': ylog = 1; break;
			case 'x': xlog = 1; break;
			case 'e': equal_xy = 1; break;
			case 'd': määrittele = optarg; break;
		}

	int käyriä = argc - optind;
	if (käyriä <= 0)
		return 1;

	double y[käyriä][n], x[n], xväli = (loppu - alku) / (n - 1);
	for (int i=0; i<n; i++)
		x[i] = alku + i * xväli;

	void *kahva;
	for (int ikäyrä=0; ikäyrä<käyriä; ikäyrä++) {
		kahva = käännä(argv[optind+ikäyrä], määrittele);
		double (*funktio)(double) = dlsym(kahva, "funktio");
		if (!funktio)
			err(1, "dlsym funktio");
		for (int i=0; i<n; i++)
			y[ikäyrä][i] = funktio(x[i]);
		dlclose(kahva);
	}

	struct kahto_figure *figure = kahto_figure_new();
	for (int ikäyrä=0; ikäyrä<käyriä; ikäyrä++)
		kahto_yx(y[ikäyrä], x, n, kahto_lineargs, .figure=figure);
	if (equal_xy)
		kahto_glg(figure)->equal_scale_xy = 1;
	kahto_glx(figure)->logscale = xlog;
	kahto_gly(figure)->logscale = ylog;
	kahto_show(figure);
}
