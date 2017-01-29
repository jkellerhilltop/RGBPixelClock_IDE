#ifndef FIXFFT_H
#define FIXFFT_H

#include "Particle.h"
#include <math.h>

//#define pgm_read_byte_near(_addr) (pgm_read_byte(_addr))
//#define pgm_read_byte_far(_addr)	(pgm_read_byte(_addr))
//#define pgm_read_word(_addr) (*(const uint16_t *)(_addr))
//#define pgm_read_word_near(_addr) (pgm_read_word(_addr))


/*
 fix_fft() - perform forward/inverse fast Fourier transform.
 fr[n],fi[n] are real and imaginary arrays, both INPUT AND
 RESULT (in-place FFT), with 0 <= n < 2**m; set inverse to
 0 for forward transform (FFT), or 1 for iFFT.
*/
int16_t fix_fft(int8_t fr[], int8_t fi[], int16_t m, int16_t inverse);


/*
 fix_fftr() - forward/inverse FFT on array of real numbers.
 Real FFT/iFFT using half-size complex FFT by distributing
 even/odd samples into real/imaginary arrays respectively.
 In order to save data space (i.e. to avoid two arrays, one
 for real, one for imaginary samples), we proceed in the
 following two steps: a) samples are rearranged in the real
 array so that all even samples are in places 0-(N/2-1) and
 all imaginary samples in places (N/2)-(N-1), and b) fix_fft
 is called with fr and fi pointing to index 0 and index N/2
 respectively in the original array. The above guarantees
 that fix_fft "sees" consecutive real samples as alternating
 real and imaginary samples in the complex array.
*/
int16_t fix_fftr(int8_t f[], int16_t m, int16_t inverse);


#endif