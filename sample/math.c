#include <stdint.h>
#include <stdio.h>
#include <time.h>

#define TIMES 1000000
#define RUNS 10
#define METHOD run_fsqrt
#define STRINGIFY1(x) #x
#define STRINGIFY(x) STRINGIFY1(x) // Add this line

clock_t run_add() {
	float one = 1.0f;
	float two = 2.0f;

	clock_t start = clock();

	// Run addition many times to get measurable time
	float three;
	for (int i = 0; i < TIMES; i++) {
		three = one + two;
	}

	clock_t end = clock();
	clock_t time_spent = (end - start);

	printf("Result: %x\n", *(uint32_t *)&three);
	return time_spent;
}

clock_t run_div() {
	float one = 1.0f;
	float two = 2.0f;

	clock_t start = clock();

	// Run division many times to get measurable time
	float three;
	for (int i = 0; i < TIMES; i++) {
		three = one / two;
	}

	clock_t end = clock();
	clock_t time_spent = (end - start);

	printf("Result: %x\n", *(uint32_t *)&three);
	return time_spent;
}

clock_t run_mul() {
	float one = 1.0f;
	float two = 2.0f;

	clock_t start = clock();

	// Run division many times to get measurable time
	float three;
	for (int i = 0; i < TIMES; i++) {
		three = one * two;
	}

	clock_t end = clock();
	clock_t time_spent = (end - start);

	printf("Result: %x\n", *(uint32_t *)&three);
	return time_spent;
}

clock_t run_fsqrt() {
	float sixteen = 16.0f;

	clock_t start = clock();

	// Run fsqrt many times to get measurable time
	float four;
	for (int i = 0; i < TIMES; i++) {
		four = __builtin_sqrtf(sixteen);
	}

	clock_t end = clock();
	clock_t time_spent = (end - start);

	printf("Result: %x\n", *(uint32_t *)&four);
	return time_spent;
}

int main() {
	clock_t times[RUNS];
	clock_t sum = 0;

	printf("benchmark %s\n", STRINGIFY(METHOD));

	// Perform multiple runs
	for (int i = 0; i < RUNS; i++) {
		times[i] = METHOD();
		sum += times[i];
		printf("Run %d time: %lu ticks\n", i + 1, times[i]);
	}

	// Calculate average using integer math
	clock_t avg = sum / RUNS;
	printf("\nAverage time: %lu ticks\n", avg);

	return 0;
}
