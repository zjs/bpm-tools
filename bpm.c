/*
 * Copyright (C) 2011 Mark Hills <mark@pogo.org.uk>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License version 2 for more details.
 *
 * You should have received a copy of the GNU General Public License
 * version 2 along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 *
 */

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <sysexits.h>
#include <unistd.h>

#define BANNER "tempo 0.1 (C) Copyright 2011 Mark Hills <mark@pogo.org.uk>"
#define NAME "tempo"

#define RATE 44100 /* of input data */

#define BLOCK 4096
#define INTERVAL 128

#define drand() drand48()

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(*(x)))

/*
 * Sample from the metered energy
 *
 * No need to interpolate and it makes a tiny amount of difference; we
 * take a random sample of samples, any errors are averaged out.
 */

static double sample(float nrg[], size_t len, double offset)
{
	double n;
	size_t i;

	n = floor(offset);
	i = (size_t)n;

	return (n >= 0.0 && n < (double)len) ? nrg[i] : 0.0;
}

/*
 * Test an autodifference for the given interval
 */

double autodifference(float nrg[], size_t len, double interval)
{
	size_t n;
	double mid, v, diff;
	static const double beats[] = { -32, -16, -8, -4, -2, -1, 1, 2, 4, 8, 16, 32 },
			nobeats[] = { -0.5, -0.5, 0.5, 0.5 };

	mid = drand() * len;
	v = sample(nrg, len, mid);

	diff = 0.0;
	for (n = 0; n < ARRAY_SIZE(beats); n++) {
		double y;

		y = sample(nrg, len, mid + beats[n] * interval);
		diff += fabs(y - v);
	}

	for (n = 0; n < ARRAY_SIZE(nobeats); n++) {
		double y;

		y = sample(nrg, len, mid + nobeats[n] * interval);
		diff -= fabs(y - v);
	}

	return diff / (ARRAY_SIZE(beats) + ARRAY_SIZE(nobeats));
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
		unsigned int samples,
		bool graph)
{
	double step, interval, trough, height;
	unsigned int s;

	slowest = bpm_to_interval(slowest);
	fastest = bpm_to_interval(fastest);
	step = (slowest - fastest) / steps;

	height = INFINITY;

	for (interval = fastest; interval <= slowest; interval += step) {
		double t;

		t = 0.0;
		for (s = 0; s < samples; s++)
			t += autodifference(nrg, len, interval);

		if (graph) {
			printf("%lf\t%lf\n",
				interval_to_bpm(interval),
				t / samples);
		}

		/* Track the lowest value */

		if (t < height) {
			trough = interval;
			height = t;
		}
	}

	return interval_to_bpm(trough - (step / 2));
}

void usage(FILE *f)
{
	fprintf(f, "Usage: " NAME " [options]\n"
		"Analyse the tempo (in beats-per-minute, BPM) of incoming audio\n\n"
		"  -g   Output autodifference graph to stdout\n"
		"  -f   Print format for final BPM value (default \"%%0.1f\")\n"
		"  -v   Print progress information to stderr\n"
		"  -h   Display this help message and exit\n\n");

	fprintf(f, "Incoming audio is raw audio on stdin at %dHz, mono, 32-bit float; eg.\n"
		"  $ sox file.mp3 -t raw -r %d -e float -c 1 | ./" NAME "\n\n",
		RATE, RATE);

	fprintf(f, "To view autodifference graph:\n"
		"  $ sox [...] | ./tempo -g > file.dat\n"
		"  $ gnuplot\n"
		"  gnuplot> plot \"file.dat\"\n\n");
}

int main(int argc, char *argv[])
{
	float *nrg = NULL;
	size_t len = 0, buf = 0;
	off_t n = 0;
	float bpm, v = 0.0;
	const char *format = "%0.1f";

	for (;;) {
		int c;

		c = getopt(argc, argv, "vf:gh");
		if (c == -1)
			break;

		switch (c) {
		case 'f':
			format = optarg;
			break;

		case 'g':
			format = NULL;
			break;

		case 'h':
			usage(stdout);
			return 0;

		default:
			return EX_USAGE;
		}
	}

	argv += optind;
	argc -= optind;

	if (argc > 0) {
		fprintf(stderr, "%s: Too many arguments\n", NAME);
		return EX_USAGE;
	}

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

		/* At regular intervals, sample the energy to give a
		 * low-resolution overview of the track */

		n++;
		if (n != INTERVAL)
			continue;

		n = 0;

		if (len == buf) {
			size_t n;

			n = buf + BLOCK;
			nrg = realloc(nrg, n * sizeof(*nrg));
			if (nrg == NULL) {
				perror("realloc");
				return -1;
			}
			buf = n;
		}

		nrg[len++] = v;
	}

	bpm = scan_for_bpm(nrg, len, 60.0, 170.0, 800, 1000, (format == NULL));

	if (format != NULL) {
		printf(format, bpm);
		putc('\n', stdout);
	}

	free(nrg);

	return 0;
}
