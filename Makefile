# SPDX-License-Identifier: GPL-3.0-or-later

CC = gcc
CFLAGS = -O3 -Wall

all: cpuidle_bench

cpuidle_bench: cpuidle_bench.c

clean:
	$(RM) cpuidle_bench
