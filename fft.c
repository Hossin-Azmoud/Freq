#include <complex.h>
const double PI = acos(-1);

float *fft(float *in, size_t length) {
	float *odd  = (float *) malloc(sizeof(float) * (length/2));
	float *even = (float *)  malloc(sizeof(float) * (length/2));
	float *fft_odds, *fft_evens;
	if (!length) {
		return;
	}

	// Get the odd samples.
	// even = fft(x[0::2])
	for (size_t k = 0, size_t o = 0; k < length; k +=2, o++) {
		even[o] = in[k];
	}

	// get the even samples    
	fft_evens = fft(even);
	
	for (size_t k = 1, size_t o = 0; k < length; k += 2, o++) {
		even[o] = in[k];
	}
	
	fft_odds = fft(odd);
	// T= [exp(-2j*pi*k/N)*odd[k] for k in range(N//2)]
	for (size_t k = 1; k < length/2; k++) {
		fft_odds[k] = cexp(-2 * PI * k/length) * fft_odds[k];
	}
	
	float *out = (float *) malloc(sizeof (float) * length);
	size_t k = 0;
	for (int i = 0; i < length/2; i++, k++)
		out[k] = fft_evens[i] + fft_odds[i];
	
	for (int i = 0; i < length/2; i++, k++)
		out[k] = fft_evens[i] - fft_odds[i];
	
	{
		free(even);
		free(odd);
		free(fft_evens);
		free(fft_odds);
	}

	return out;
}
