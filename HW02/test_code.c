#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>

int main() {
	int ret;
	while(1) {
		ret = syscall(350);
		if (ret < 0){
			perror("Unsupported FPU");
			return -1;
		}

		printf("Cpu Utilization=%.2f(%)\n", (double) (ret) / 100.0);
		sleep(1);
	}

	return 0;
}
