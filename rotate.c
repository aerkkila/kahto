void rotate25(unsigned *to, int tw, const unsigned *from, int fw, int fh) {
    for (int j=0; j<fh; j++)
	for (int i=0; i<fw; i++)
	    to[i*tw + fh-1-j] = from[j*fw + i];
}

void rotate(
    unsigned *to, int tw, int th,
    const unsigned *from, int fw, int fh,
    float rot100)
{
    if (rot100 == 25)
	return rotate25(to, tw, from, fw, fh);
}
