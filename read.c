#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

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

	ret = read(fd, str, 5);
	if(ret == -1)
	{
		perror("Not able to open device file.");
		return ret;
	}
	close(fd);
	printf("Read string is %ld = %s\n", ret, str);

	return 0;
}
