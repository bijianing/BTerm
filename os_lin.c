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

int PRINT(const char *format, ...)
{
	va_list arg;
	int done;

	va_start (arg, format);
	done = vfprintf (stdout, format, arg);
	va_end (arg);
	return done;
}

int ERR(const char *format, ...)
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
	struct termios io_conf;
	tcgetattr(0, &io_conf);

	io_conf.c_lflag &= ~(ECHO);  // disabled echo
	//io_conf.c_lflag &= ~(ICANON);  // disabled canonical
	io_conf.c_cc[VMIN]  = 0;
	io_conf.c_cc[VTIME] = 1;
	tcsetattr( 0 , TCSAFLUSH , &io_conf );

	system ("/bin/stty raw");

	BTerm_Main(0);

	return 0;
}

int BTerm_Stop(void)
{
	system ("/bin/stty 4500:5:bf:8a3b:3:1c:7f:15:4:0:1:0:11:13:1a:0:12:f:17:16:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0:0");
	exit(0);

	return 0;
}


void term_register_test(void);
int main(void)
{

	term_register_test();
	return BTerm_Start();
}
