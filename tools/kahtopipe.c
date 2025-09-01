#include <kahto.h>
#include <stdio.h>
#include <unistd.h>

int main(int argc, char **argv) {
	int opt, separator = 0;
	while ((opt = getopt(argc, argv, "s")) >= 0)
		switch (opt) {
			case 's': separator = 1; break;
		}

	char *format = separator ? "%lf %*c" : "%lf ";
	long len = 2000;
	double *data = malloc(len*sizeof(double));
	long pos = -1;
	while (1) {
		while (++pos < len)
			if (scanf(format, data+pos) != 1)
				goto done;
		len *= 1.5;
		data = realloc(data, len*sizeof(double));
	}
done:
	kahto_show(kahto_y(data, pos+1));
	free(data);
}
