#include <stdio.h>

int main() {
	// For storing CPUID results
	unsigned int eax, ebx, ecx, edx;

	// Get vendor ID string (EAX=0)
	char vendor[13]; // 12 characters plus null terminator

	__asm__ __volatile__("cpuid"
	                   : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
	                   : "a"(0));

	// The vendor string is stored in EBX, EDX, ECX in that order
	*((unsigned int *)vendor) = ebx;
	*((unsigned int *)(vendor + 4)) = edx;
	*((unsigned int *)(vendor + 8)) = ecx;
	vendor[12] = '\0';

	printf("CPU Vendor: %s\n", vendor);

	// Get processor info and feature bits (EAX=1)
	__asm__ __volatile__("cpuid"
	                   : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
	                   : "a"(1));

	// Extract processor information from EAX
	unsigned int stepping = eax & 0xF;
	unsigned int model = (eax >> 4) & 0xF;
	unsigned int family = (eax >> 8) & 0xF;
	unsigned int type = (eax >> 12) & 0x3;
	unsigned int extModel = (eax >> 16) & 0xF;
	unsigned int extFamily = (eax >> 20) & 0xFF;

	// Print processor information
	printf("CPU Info:\n");
	printf("  Family: %d (0x%X)\n", family, family);
	printf("  Model: %d (0x%X)\n", model, model);
	printf("  Stepping: %d (0x%X)\n", stepping, stepping);
	printf("  Extended Family: %d (0x%X)\n", extFamily, extFamily);
	printf("  Extended Model: %d (0x%X)\n", extModel, extModel);

	// Print feature flags
	printf("Feature Flags:\n");
	printf("  EDX: 0x%08X\n", edx);
	printf("  ECX: 0x%08X\n", ecx);

	// Print some common CPU features
	if (edx & (1 << 25))
		printf("  - SSE: Supported\n");
	if (edx & (1 << 26))
		printf("  - SSE2: Supported\n");
	if (ecx & (1 << 0))
		printf("  - SSE3: Supported\n");
	if (ecx & (1 << 28))
		printf("  - AVX: Supported\n");

	return 0;
}
