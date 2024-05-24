#include <complex.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <time.h>
const float PI = acos(-1);
#define N 48000

void dft(float in[], float complex out[], size_t n)
{
	for (size_t z = 0; z < n; z++) {
		out[z] = 0;
		for (size_t i = 0; i < n; ++i) {
			float t = (float)i/n;
			out[z] += in[i] * cexp(2*I*PI*z*t);
		}
	}
}

void genf(float out[], size_t n) 
{
	for (size_t i = 0; i < n; ++i) {
		float t = (float)i/n;
		out[i] =sinf(t*PI*2*1);
	}
}

void visualize_samples(float complex *samples, size_t n)
{
	for (int i = 0; i < n; ++i) {
		printf("%i: R: %.2f I: %.2f\n", i, creal(samples[i]), cimag(samples[i]));
	}
}
/*
int main() {
	float freqs[N] = {0};
	float complex freqs_dft[N] = {0};
	
	printf("PI -> %f\n", PI);
	printf("GENERATING SAMPLES\n");
	genf(freqs, N);
	printf("GENERATED SAMPLES\n");
	printf("APPLYING DFT\n");
	time_t a = time(NULL);
	dft(freqs, freqs_dft, N);
	time_t p = time(NULL) - a;
	printf("APPLIED DFT\n");
	visualize_samples(freqs_dft, N);	
	printf("DFT TOKK - %zus - sample count=%i\n", p, N);
	printf("DFT TOKK - %zus - sample count=%i\n", p*10, N*10);

	return 0;
}
*/
