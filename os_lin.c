/* **************************************************************************** */
/* include                                                                      */
/* **************************************************************************** */
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <term.h>
#include <sys/ioctl.h>
#include <termios.h>


/* **************************************************************************** */
/* define                                                                       */
/* **************************************************************************** */
#define SPBUFSZ			512
/* **************************************************************************** */
/* enum                                                                         */
/* **************************************************************************** */
/* **************************************************************************** */
/* data                                                                         */
/* **************************************************************************** */
/* **************************************************************************** */
/* struct                                                                       */
/* **************************************************************************** */
/* **************************************************************************** */
/* extern                                                                       */
/* **************************************************************************** */
/* **************************************************************************** */
/* local variable                                                               */
/* **************************************************************************** */
static char stty_param[SPBUFSZ];

int PRINT(const char *format, ...)
{
	va_list arg;
	int done;

	va_start (arg, format);
	done = vfprintf (stdout, format, arg);
	va_end (arg);
	return done;
}

int PRINT_ERR(const char *format, ...)
{
	va_list arg;
	int done;

	va_start (arg, format);
	done = vfprintf (stderr, format, arg);
	va_end (arg);
	return done;
}

void print_newline(void)
{
	printf("\r\n");
}

void print_c(char c)
{
	putchar(c);
}


void print_str(char *str)
{
	printf("%s", str);
}

char read_c(void)
{
	return getchar();
}

void os_wait(int ms)
{
	usleep(ms * 1000);
}

int BTerm_Start(void)
{
	FILE *fp;
	char buf[SPBUFSZ];
	int len;
	char *p;
	struct termios io_conf;
	tcgetattr(0, &io_conf);

	io_conf.c_lflag &= ~(ECHO);  // disabled echo
	//io_conf.c_lflag &= ~(ICANON);  // disabled canonical
	io_conf.c_cc[VMIN]  = 0;
	io_conf.c_cc[VTIME] = 1;
	tcsetattr( 0 , TCSAFLUSH , &io_conf );

	fp = popen("stty -g", "r");

	p = stty_param;
	memset(p, 0, SPBUFSZ);
	if(!fp){
		fprintf(stderr, "Could not open pipe for output.\n");
		return 1;
	}

	while (fgets(buf, SPBUFSZ , fp)) {
		len = strlen(buf);
		if (buf[len - 1] == '\n') {
			buf[len - 1] = 0;
			len--;
		}
		//printf("got stty param:%s, len:%d\n", buf, len);
		if (p + len + 1 > stty_param + SPBUFSZ) {
			fprintf(stderr, "buffer size (%d) is too small\n", SPBUFSZ);
			goto err;
		}
		memcpy(p, buf, len);
		p[len] = ' ';
		p += (len + 1);
	}

	system ("stty raw -echo");

err:
	if (pclose(fp) != 0) {
		fprintf(stderr," Error: Failed to close command stream \n");
	}

	BTerm_Main(0);

	return 0;
}

int BTerm_Stop(void)
{
	char cmd[SPBUFSZ];
	sprintf(cmd, "stty %s", stty_param);
	//printf("restore stty cmd:%s\n", cmd);
	system(cmd);
	exit(0);

	return 0;
}


void term_register_test(void);
int main(void)
{

	term_register_test();
	return BTerm_Start();
}
