/* **************************************************************************** */
/* include                                                                      */
/* **************************************************************************** */
#include <string.h>
#include <stdio.h>

/* **************************************************************************** */
/* define                                                                       */
/* **************************************************************************** */
#define __DBG 1

#if	(__DBG == 1)
#define DBG(f, x...) \
	printf("Term: %-8d, %s(): " f, __LINE__, __FUNCTION__,## x)
#define INF					printf
#define PRINT					printf
#elif	(__DBG == 2)
#define DBG(f, x...) \
	dbg_printf("Term: %-8d, %s(): " f, __LINE__, __FUNCTION__,## x)
#define INF					dbg_printf
#define PRINT					dbg_printf
#else
#define DBG(f, x...) 
#endif

#define IP(d,c,b,a)			((a << 0) | (b << 8) | (c << 16) | (d << 24))
#define UART_BUFSZ			64
#define HISTORY_MAX			32
#define ARYSIZ(a)		(sizeof(a) / sizeof(a[0]))

#define SIMP_TERM_CMDTAB_ENTRY(cmd, help) {cmd_hdl_ ## cmd, #cmd, help}
#define SIMP_TERM_CMDHDL_DEFINE(cmd) static int cmd_hdl_ ## cmd (int argc, char**argv)


/* **************************************************************************** */
/* enum                                                                         */
/* **************************************************************************** */
/* **************************************************************************** */
/* data                                                                         */
/* **************************************************************************** */
/* **************************************************************************** */
/* struct                                                                       */
/* **************************************************************************** */
typedef struct BTermCmd_str
{
	int (*func)(int argc, char **argv);
	char *name;
	char *help;
} BTermCmd_t;



/* **************************************************************************** */
/* extern                                                                       */
/* **************************************************************************** */
extern int dbg_printf( char *fmt, ... );

void print_newline(void);
void print_c(char c);
void print_str(char *str);
char read_c(void);

void BTerm_Main(int p);
int BTerm_Start(void);
int BTerm_Stop(void);
void BTerm_RegisterCmds(BTermCmd_t *cmd_array, int array_size);

