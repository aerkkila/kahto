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

const char fun_alku[] =
"#include <math.h>\n"
"#include <stdlib.h>\n" // at least rand() might be wanted
"double funktio(double x) {\n"
"	return ";

const char fun_loppu[] = ";\n}";

void* käännä(const char *sisälmys) {
	int pitsis = strlen(sisälmys);
	int tekstipit = sizeof(fun_alku)-1 + pitsis + sizeof(fun_loppu);

	/* tehdään lähdetiedoston sisältö */
	char *teksti = malloc(tekstipit);
	int nyt = 0;
	memcpy(teksti+nyt, fun_alku, sizeof(fun_alku)-1);
	nyt += sizeof(fun_alku)-1;
	memcpy(teksti+nyt, sisälmys, pitsis);
	nyt += pitsis;
	memcpy(teksti+nyt, fun_loppu, sizeof(fun_loppu));

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

	fwrite(teksti, tekstipit-1, 1, prog);
	pclose(prog);
	free(teksti);

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
	int n = 512,
		opt;

	while ((opt = getopt(argc, argv, "a:b:n:")) >= 0)
		switch (opt) {
			case 'a': alku = atof(optarg); break;
			case 'b': loppu = atof(optarg); break;
			case 'n': n = atoi(optarg); break;
		}

	int käyriä = argc - optind;
	if (käyriä <= 0)
		return 1;

	double y[käyriä][n], x[n], xväli = (loppu - alku) / (n - 1);
	for (int i=0; i<n; i++)
		x[i] = alku + i * xväli;

	void *kahva;
	for (int ikäyrä=0; ikäyrä<käyriä; ikäyrä++) {
		kahva = käännä(argv[optind+ikäyrä]);
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
	kahto_show(figure);
}
