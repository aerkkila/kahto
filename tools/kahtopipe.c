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
	double *ptr = data - 1;
	double *end = data + len;
	while (1) {
		while (++ptr < end)
			if (scanf(format, ptr) != 1)
				goto done;
		len *= 1.5;
		long pos = ptr - data;
		data = realloc(data, len*sizeof(double));
		ptr = data + pos;
		end = data + len;
	}
done:
	kahto_show(kahto_y(data, ptr-data));
	free(data);
}
