#include <stdio.h>
#include <stdlib.h>

#define SYS_FS		6
#define SYS_HALT	7
#define SYS_REBOOT	8

#define FS_LS		0
#define FS_PWD		1
#define FS_CD		2
#define FS_MKDIR	3
#define FS_TOUCH	4
#define FS_RM		5
#define FS_RMDIR	6
#define FS_CAT		7
#define FS_FILE		8
#define FS_MV		9
#define FS_CP		10

#define MAX_ARGS	8

struct builtin {
	const char *name;
	int op;
	int min_args;	/* including command name */
	int max_args;
};

static const struct builtin builtins[] = {
	{ "pwd",   FS_PWD,   1, 1 },
	{ "ls",    FS_LS,    1, 2 },
	{ "cd",    FS_CD,    2, 2 },
	{ "mkdir", FS_MKDIR, 2, 2 },
	{ "touch", FS_TOUCH, 2, 2 },
	{ "rm",    FS_RM,    2, 2 },
	{ "rmdir", FS_RMDIR, 2, 2 },
	{ "cat",   FS_CAT,   2, 2 },
	{ "file",  FS_FILE,  2, 2 },
	{ "mv",    FS_MV,    3, 3 },
	{ "cp",    FS_CP,    3, 3 },
};

static int
sys_fs(int op, const char *arg1, const char *arg2)
{
	int ret;

	asm volatile("int $0x80"
			: "=a"(ret)
			: "a"(SYS_FS), "b"(op), "c"(arg1), "d"(arg2)
			: "memory");
	return ret;
}

static void
sys_halt(void)
{
	asm volatile("int $0x80"
			:
			: "a"(SYS_HALT)
			: "memory");
}

static void
sys_reboot(void)
{
	asm volatile("int $0x80"
			:
			: "a"(SYS_REBOOT)
			: "memory");
}

static void
read_line(char *buf, size_t size)
{
	size_t i = 0;
	char c;

	if (!buf || size < 2)
		return;

	while (i < size - 1)
	{
		if (read(0, &c, sizeof(c)) != 1)
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

	/* Buffer full: discard until end of line. */
	if (i >= size - 1)
	{
		while (read(0, &c, sizeof(c)) == 1 && c != '\n' && c != '\r')
			;
		if (c == '\n' || c == '\r')
			printf("\n");
	}

	buf[i] = '\0';
}

static int
streq(const char *a, const char *b)
{
	while (*a && *a == *b)
	{
		a++;
		b++;
	}

	return *a == '\0' && *b == '\0';
}

/* Split line into whitespace-separated argv; returns argc. Mutates line. */
static int
tokenize(char *line, char **argv, int max_args)
{
	int argc = 0;

	while (*line && argc < max_args)
	{
		while (*line == ' ' || *line == '\t')
			line++;

		if (*line == '\0')
			break;

		argv[argc++] = line;

		while (*line && *line != ' ' && *line != '\t')
			line++;

		if (*line)
		{
			*line = '\0';
			line++;
		}
	}

	return argc;
}

static int
run_hello(void)
{
	int status;
	int pid;

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
		return -1;
	}

	wait(&status);
	return 0;
}

static int
run_builtin(int argc, char **argv)
{
	unsigned int i;
	const struct builtin *b;
	const char *a1;
	const char *a2;

	for (i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++)
	{
		b = &builtins[i];

		if (!streq(argv[0], b->name))
			continue;

		if (argc < b->min_args || argc > b->max_args)
		{
			printf("usage: %s", b->name);

			if (b->max_args >= 2)
			{
				if (b->min_args < 2)
					printf(" [path]");
				else
					printf(" <path>");
			}
			if (b->max_args >= 3)
				printf(" <dst>");

			printf("\n");
			return -1;
		}

		a1 = (argc >= 2) ? argv[1] : 0;
		a2 = (argc >= 3) ? argv[2] : 0;
		return sys_fs(b->op, a1, a2);
	}

	return 1;	/* not a builtin */
}

int
main(int argc, char **argv)
{
	char line[256];
	char *args[MAX_ARGS];
	int n;
	int rc;

	(void)argc;
	(void)argv;

	for (;;)
	{
		printf("aragveli$ ");
		read_line(line, sizeof(line));

		n = tokenize(line, args, MAX_ARGS);

		if (n == 0)
			continue;

		if (streq(args[0], "hello"))
		{
			run_hello();
			continue;
		}

		if (streq(args[0], "halt"))
		{
			sys_halt();
			continue;
		}

		if (streq(args[0], "reboot"))
		{
			sys_reboot();
			continue;
		}

		rc = run_builtin(n, args);

		if (rc == 1)
		{
			printf("unknown command: %s\n", args[0]);
			continue;
		}

		if (rc != 0)
			printf("error: %d\n", rc);
	}
}
