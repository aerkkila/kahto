float get_ticks_basic(int ind, float min, float max) {
    int n = 10;
    if (ind < 0)
	return n;
    float väli = (max-min) / (n-1);
    return ind * väli - min;
}
