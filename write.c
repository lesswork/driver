#include<stdio.h>
#include<unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>

int main(int argc, char *argv[])
{
	size_t ret = 0;
	int fd = 0;
	char str[64] = {0, };

	fd = open ("/dev/circular-0",O_RDWR);
	if(fd == -1)
	{
		perror("Not able to open device file.");
		return fd;
	}

	printf("\nEnter string to add in buffer : ");
	scanf("%s",str);

	ret = write(fd, str, strlen(str) + 1);
	if(ret == -1)
	{
		perror("Not able to wrote device file.");
		return ret;
	}

	return 0;
}
