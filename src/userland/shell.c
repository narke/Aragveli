#include <stdio.h>
#include <stdlib.h>

static void
read_line(char *buf, int size)
{
	int i = 0;
	char c;

	if (size <= 0)
		return;

	while (i < size - 1)
	{
		if (read(0, &c, 1) != 1)
			break;

		if (c == '\n' || c == '\r')
		{
			printf("\n");
			break;
		}

		if (c == '\b' || c == 0x7f)
		{
			if (i > 0)
			{
				i--;
				printf("\b \b");
			}
			continue;
		}

		buf[i++] = c;
		printf("%c", c);
	}

	buf[i] = '\0';
}

static int
is_hello(const char *s)
{
	const char *p = "hello";

	while (*p && *s == *p)
	{
		s++;
		p++;
	}

	return *p == '\0' && *s == '\0';
}

int
main(int argc, char **argv)
{
	char line[256];
	int status;
	int pid;
	char *cmd;

	(void)argc;
	(void)argv;

	for (;;)
	{
		printf("aragveli$ ");
		read_line(line, sizeof(line));

		cmd = line;
		while (*cmd == ' ' || *cmd == '\t')
			cmd++;

		if (*cmd == '\0')
			continue;

		if (!is_hello(cmd))
		{
			printf("unknown command: %s\n", cmd);
			continue;
		}

		pid = fork();
		if (pid == 0)
		{
			char *child_argv[] = { "/hello.elf", 0 };

			exec("/hello.elf", child_argv);
			printf("exec failed\n");
			exit(1);
		}

		if (pid < 0)
		{
			printf("fork failed\n");
			continue;
		}

		wait(&status);
	}
}
