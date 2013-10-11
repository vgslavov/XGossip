#include <stdio.h>

/* sizeof: display sizes of basic types */
int main(void)
{
	printf("char %d, short %d, int %d, long %d,", sizeof(char),
	    sizeof(short), sizeof(int), sizeof(long));
	printf(" float %d, double %d, void* %d, char* %d\n", sizeof(float),
	    sizeof(double), sizeof(void *), sizeof(char *));
	return 0;
}
