#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define RATE 44100 /* of input data */

#define BLOCK 4096
#define INTERVAL 128

#define drand() drand48()

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))

/*
 * Sample from the metered energy with interpolation
 */

static double sample(float nrg[], size_t len, double offset)
{
	double f, a, b;
	size_t n;

	n = (size_t)offset;

	f = offset - n;
	if (f == 0.0)
		return (n < len) ? nrg[n] : 0.0;

	/* Linear interpolation */

	a = (n < len) ? nrg[n] : 0.0;
	n++;
	b = (n < len) ? nrg[n] : 0.0;

	return (1.0 - f) * a + f * b;
}

/*
 * Test an autodifference for the given interval
 */

double autodifference(float nrg[], size_t len, double interval)
{
	size_t n;
	double mid, v, diff = 0.0;
	static const int beats[] = { -32, -16, -8, -4, -4, -4, -2, -2, -1,
				     1, 2, 2, 4, 4, 4, 8, 16, 32 };

	mid = drand() * len;
	v = sample(nrg, len, mid);

	for (n = 0; n < ARRAY_SIZE(beats); n++) {
		int beat;
		double y;

		beat = beats[n];
		y = sample(nrg, len, mid + beat * interval);
		diff += fabs(y - v);
	}

	return diff;
}

/*
 * Beats-per-minute to a sampling interval in energy space
 */

double bpm_to_interval(double bpm)
{
	double beats_per_second, samples_per_beat;

	beats_per_second = bpm / 60;
	samples_per_beat = RATE / beats_per_second;
	return samples_per_beat / INTERVAL;
}

/*
 * Sampling interval in enery space to beats-per-minute
 */

double interval_to_bpm(double interval)
{
	double samples_per_beat, beats_per_second;

	samples_per_beat = interval * INTERVAL;
	beats_per_second = (double)RATE / samples_per_beat;
	return beats_per_second * 60;
}

/*
 * Scan a range of BPM values for the one with the
 * minimum autodifference
 */

double scan_for_bpm(float nrg[], size_t len,
		double slowest, double fastest,
		unsigned int steps,
		unsigned int samples)
{
	double step, interval, trough, height;
	unsigned int s;

	slowest = bpm_to_interval(slowest);
	fastest = bpm_to_interval(fastest);
	step = (slowest - fastest) / steps;

	height = INFINITY;

	for (;;) {
		double t;

		interval += step;
		if (interval > slowest)
			break;

		t = 0.0;
		for (s = 0; s < samples; s++)
			t += autodifference(nrg, len, interval);

		/* Track the lowest value */

		if (t < height) {
			trough = interval;
			height = t;
		}
	}

	return interval_to_bpm(trough - (step / 2));
}

int main(int argc, char *argv[])
{
	float *nrg = NULL;
	size_t len = 0, buf = 0;
	off_t n = 0;
	double bpm;
	float v = 0.0;

	for (;;) {
		float z;

		if (fread(&z, sizeof z, 1, stdin) != 1)
			break;

		/* Maintain an energy meter (similar to PPM) */

		z = fabs(z);
		if (z > v) {
			v += (z - v) / 8;
		} else {
			v -= (v - z) / 512;
		}

		/* Periodically sample the energy to give a
		 * low-resolution overview of the track */

		n++;
		if (n != INTERVAL)
			continue;

		n = 0;

		if (len == buf) {
			size_t n;

			fprintf(stderr, "Reallocating at %zd\n", len);

			n = buf + BLOCK;
			nrg = realloc(nrg, n * sizeof(*nrg));
			assert(nrg != NULL);
			buf = n;
		}

		nrg[len++] = v;
	}

	bpm = scan_for_bpm(nrg, len, 60.0, 180.0, 400, 1000);
	printf("BPM = %f\n", bpm);

	free(nrg);

	return 0;
}
