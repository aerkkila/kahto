#include <sys/stat.h>

static int mkdir_file(const char *restrict name) {
	int len = strlen(name);
	char k1[len+1];
	memcpy(k1, name, len+1);
	char k2[len+2];
	if (name[0] == '/')
		k2[0] = '/', len = 1;
	else
		len = 0;
	char *str = strtok(k1, "/"), *str1;
	while (1) {
		if (!(str1 = strtok(NULL, "/")))
			return 0;
		while (*str) k2[len++] = *str++;
		str = str1;
		k2[len++] = '/';
		k2[len] = 0;
		if (mkdir(k2, 0755) && errno != EEXIST)
			return 1;
	}
}
