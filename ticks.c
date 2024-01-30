int get_ticks_basic(float **out, float min, float max) {
    int n = 10;
    *out = malloc(n * sizeof(**out));
    float jump = max - min / (n-1);
    for (int i=0; i<n; i++)
	*out[i] = min + i*jump;
    return n;
}
#warning "tikkejä tai muutakaan ei vapauteta"
