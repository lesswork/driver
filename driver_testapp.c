#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#define EXIT 0
#define READ 1
#define WRITE 2

int menu()
{
	int choice=0;
	printf("press 1 for read form buffer\n");
	printf("press 2 for write in buffer\n");
	printf("press 0 for exit application\n");
	scanf("%d", &choice);
	return choice;
}

int main(int argc, char *argv[])
{
	size_t ret = 0;
	int fd = 0;
	int choice = 0;
	int read_bytes = 0;
	char write_str[64] = {0, };
	char read_str[64] = {0, };

	fd = open ("/dev/circular-0",O_RDWR);
	if(fd == -1)
	{
		perror("Not able to open device file.");
		return fd;
	}

	while((choice = menu()) , choice != EXIT)
	{
		printf("choice : %d\n",choice);
		switch(choice)
		{
			case READ:
				printf("\nHow many bytes want to read : ");
				scanf("%*c %d",&read_bytes);
				ret = read(fd, read_str, read_bytes);
				if(ret < 0)
				{
					perror("Not able to open device file.");
					return ret;
				}
				printf("Read string is %ld = %s\n", ret, read_str);
				break;

			case WRITE:
				printf("\nEnter string to add in buffer : ");
				scanf("%s",write_str);

				ret = write(fd, write_str, strlen(write_str) + 1);
				if(ret < 0)
				{
					perror("Not able to wrote device file.");
					return ret;
				}
				break;
		}
	}
	close(fd);

	return 0;
}
