/* **************************************************************************** */
/* include                                                                      */
/* **************************************************************************** */
#include "term.h"
#include <string.h>
#include <stdio.h>


/* **************************************************************************** */
/* define                                                                       */
/* **************************************************************************** */
#define ARG_MAX				(10)
#define CMD_TAB_MAX			(10)
#define for_each_cmd(i1, i2, cmd) \
	for (i1 = 0; i1 < CMD_TAB_MAX && g_cmds[i1].tab != NULL; i1++) \
		for (i2 = 0, cmd = &g_cmds[i1].tab[i2]; i2 < g_cmds[j].size; i2++, cmd = &g_cmds[i1].tab[i2]) 


#define STR_PROM			"\x1B[31;1m>\x1B[0m "

#define KEY_CTRL_A			(0x01)
#define KEY_CTRL_B			(0x02)
#define KEY_CTRL_C			(0x03)
#define KEY_CTRL_D			(0x04)
#define KEY_CTRL_E			(0x05)
#define KEY_CTRL_F			(0x06)
#define KEY_CTRL_K			(0x0b)
#define KEY_CTRL_Y			(0x19)

#define KEY_DEL				(0x7f)
/* **************************************************************************** */
/* enum                                                                         */
/* **************************************************************************** */
/* **************************************************************************** */
/* data                                                                         */
/* **************************************************************************** */
/* **************************************************************************** */
/* struct                                                                       */
/* **************************************************************************** */

typedef struct BTermCmdHistory_str
{
	char cmds[HISTORY_MAX][UART_BUFSZ];
	int idx;
	int idx_ref;
	int loop;
} BTermCmdHist_t;

typedef struct BTermCmdTab_str
{
	BTermCmd_t *tab;
	int size;
} BTermCmdTab_t;

/* **************************************************************************** */
/* extern                                                                       */
/* **************************************************************************** */
SIMP_TERM_CMDHDL_DEFINE(exit);
SIMP_TERM_CMDHDL_DEFINE(help);
SIMP_TERM_CMDHDL_DEFINE(hist);
SIMP_TERM_CMDHDL_DEFINE(dbg);


static int history_cnt(void);
static int history_start_idx(void);
static int move_left(int n);
static int move_right(int n);

/* **************************************************************************** */
/* local variable                                                               */
/* **************************************************************************** */
static char ch;				// current character
static char buf[UART_BUFSZ];		// buffer of command line
static char buf_copy[UART_BUFSZ];	// buffer for copy
static int i = 0;			// current index of buffer
static int len = 0;			// length of current command
static int ctrl = 0;			// control seqency flag
static int printing_cand = 0;		// printing candicate flag
static int g_ret;

BTermCmdHist_t		hist = { 0 };

static BTermCmdTab_t g_cmds[CMD_TAB_MAX] = { NULL };

BTermCmd_t cmd_basic[] =
{
	SIMP_TERM_CMDTAB_ENTRY(exit,		"exit BTerm"),
	SIMP_TERM_CMDTAB_ENTRY(help,		"show help information"),
	SIMP_TERM_CMDTAB_ENTRY(hist,		"show history commands"),
	SIMP_TERM_CMDTAB_ENTRY(dbg,		"for debug"),
};


/* **************************************************************************** */
/* funcitons for basic commands                                                 */
/* **************************************************************************** */
SIMP_TERM_CMDHDL_DEFINE(exit)
{
	BTerm_Stop();
	
	return 0;
}

SIMP_TERM_CMDHDL_DEFINE(help)
{
	int j, k;
	BTermCmd_t *cmd;

	PRINT("Help Information%s", STR_NL);
	for_each_cmd(j, k, cmd) {
		PRINT("    %-8s: %s%s", cmd->name, cmd->help, STR_NL);
	}

	return 0;
}

SIMP_TERM_CMDHDL_DEFINE(hist)
{
	int j, k, cnt;

	cnt = history_cnt();


	PRINT("Command History (time order)%s", STR_NL);
	k = history_start_idx();
	for (j = 0; j < cnt; j++) {
//	DBG("cnt:%d, j:%d, k:%d\r\n", cnt, j, k);
		PRINT("    %s%s", hist.cmds[k], STR_NL);
		k = (k + 1) % HISTORY_MAX;
	}

	return 0;
}

SIMP_TERM_CMDHDL_DEFINE(dbg)
{

	return 0;
}



/* **************************************************************************** */
/* funcitons for implementation                                                 */
/* **************************************************************************** */

static void move_left_force(int n)
{
	PRINT("\x1b[%dD", n);
}

static int move_right(int n)
{
	//DBG("i:%d, len:%d, n:%d\r\n", i, len, n);
	if (n == 0 || i == len) return 0;

	if (n > len - i) n = len - i;

	PRINT("\x1b[%dC", n);

	return n;
}

static int move_left(int n)
{
	if (n == 0 || i == 0) return 0;

	if (n > i) n = i;

	PRINT("\x1b[%dD", n);

	return n;
}

static void move_sub_buf(int to, int from)
{
	int j, head, offset, movlen, dir;

	if (from == len)
		return;

	if (from == to)
		return;

	movlen = strlen(buf + from);
	if (from > to) {
		head = to;
		offset = from - to;
		dir = 1;
	} else {
		offset = to - from;
		head = len + offset - 1;
		dir = -1;
	}
	//DBG("from:%d, to:%d, head:%d, len:%d, off:%d\r\n", from, to, head, movlen, offset);
	for (j = 0; j < movlen; j++) {
		buf[head] = buf[head + (dir * offset)];
		head += dir;
	}
}

static void print_buf_c(char c)
{
	print_c(c);

//	DBG("i:%d, len:%d, buf:%s\r\n", i, len, buf);
	/* move chars behind i */
	move_sub_buf(i + 1, i);

	buf[i] = c;
	i++;
	len++;

//	DBG("i:%d, len:%d, buf:%s, PRINTED_STR:%s\r\n", i, len, buf, buf + i);
	if (i < len) {
		PRINT("%s", buf + i);
		move_left_force(len - i);
	}
}

static void print_buf_str(char *str)
{
	int slen = strlen(str);

	if (slen <= 0)
		return;

	//DBG("buf:%s, slen:%d, str:%s, i:%d, len:%d\r\n", buf, slen, str, i, len);
	print_str(str);

	/* move chars behind i */
	move_sub_buf(i + slen, i);

	memcpy(buf + i, str, slen);
	i += slen;
	len += slen;

	if (i < len) {
		PRINT("%s", buf + i);
		move_left_force(len - i);
	}
}

static void delete_n_left(int n)
{
	int j;
	
	for (j = 0; j < n; j++)
		PRINT("\b");
	
	for (j = i; j < len; j++)
		PRINT("%c", buf[j]);

	for (j = 0; j < (n - (len - i)); j++)
		PRINT(" ");
	
	for (j = 0; j < n; j++)
		PRINT("\b");

	move_sub_buf(i - n, i);
}

static void delete_n_right(int n)
{
	int j;
	
	for (j = i + n; j < len; j++)
		PRINT("%c", buf[j]);
	
	for (j = 0; j < n; j++)
		PRINT(" ");
	
	for (j = 0; j < len - i; j++)
		PRINT("\b");

	move_sub_buf(i, i + n);
}

static void delete_n(int n)
{
	//DBG("i:%d, len:%d, n:%d\r\n", i, len, n);
	if (n == 0) {
		return;
	} else if (n > 0) {

		/* no char left */
		if (i == 0)
			return;
		if (n > i)
			n = i;
		delete_n_left(n);
		i -= n;
	} else {
		/* no char left */
		if (i == len)
			return;

		n = 0 - n;
		if (n > (len - i))
			n = len - i;
		delete_n_right(n);
	}

	len -= n;
}

static void delete_all(void)
{
//	DBG("i:%d, len:%d\r\n", i, len);
	i -= move_left(i);
	delete_n(-len);

	i = len = 0;
}

static void kill_buf_from_i(void)
{
	int clen = len - i;
	memcpy(buf_copy, buf + i, clen);
	buf_copy[clen] = 0;

	delete_n(-clen);
}

static void paste_buf(void)
{
	print_buf_str(buf_copy);
}

static int history_cnt(void)
{
	int ret = 0;
	
	if (hist.loop)
	{
		ret = HISTORY_MAX;
	}
	else
	{
		ret = hist.idx;
	}
	
	return ret;
}

static int history_next_idx(void)
{
	return (hist.idx + 1) % HISTORY_MAX;
}

static int history_prev_idx(void)
{
	return (hist.idx - 1) % HISTORY_MAX;
}

static int history_start_idx(void)
{
	if (hist.loop)
		return history_next_idx();
	else
		return 0;
}

static void history_prev(void)
{
	if (hist.idx_ref == history_cnt())
		return;
	
	/* backup current buf */
	if (hist.idx_ref == 0) {
		buf[len] = 0;
		strcpy(hist.cmds[hist.idx], buf);
	}

	hist.idx_ref++;
	
	delete_all();
	print_buf_str(hist.cmds[hist.idx - hist.idx_ref]);
}

static void history_next(void)
{
	if (hist.idx_ref == 0)
		return;

	/* restore backuped current buf */
	if (hist.idx_ref == 1) {
		strcpy(buf, hist.cmds[hist.idx]);
	}

	hist.idx_ref--;

	delete_all();
	print_buf_str(hist.cmds[hist.idx - hist.idx_ref]);
}

static char *history_get_last(void)
{
	if (history_cnt() <= 0)
		return NULL;
	
	return hist.cmds[history_prev_idx()];
}


static void history_store(void)
{
	char *c_prev = history_get_last();
	char *c = hist.cmds[hist.idx];
	
	/* do nothing if last history is same as this one */
	if (c_prev)
	{
		if (!strcmp(buf, c_prev))
			return;
	}

	strcpy(c, buf);
	
	if (hist.idx == HISTORY_MAX - 1)
	{
		hist.loop = 1;
	}
	
	hist.idx = history_next_idx();

}

static void process_key_up(void)
{
	ctrl = 0;
	history_prev();
}

static void process_key_down(void)
{
	ctrl = 0;
	history_next();
}

static void process_key_right(void)
{
	ctrl = 0;
	i += move_right(1);
}

static void process_key_left(void)
{
	ctrl = 0;
	i -= move_left(1);
}

static void process_key_delete(void)
{
	ctrl = 0;
	delete_n(-1);
}

static void print_complete_cand(BTermCmd_t *cmd)
{
	if(printing_cand) {
		PRINT("%s%s", STR_NL, cmd->name);
	} else {
		printing_cand = 1;
		PRINT("%s%s", STR_NL, cmd->name);
	}
}

static void print_complete_cand_end(void)
{
	printing_cand = 0;
	PRINT("%s%s", STR_NL STR_PROM, buf);
}

static void autocomplete(void)
{
	int j, k, found_cnt = 0;
	BTermCmd_t *cmd, *found_cmd;

	for_each_cmd(j, k, cmd) {
//		DBG("compared \"%s\" and \"%s\", i:%d, result:%d %s", cmd->name, buf, i, strncmp(cmd->name, buf, i + 1), STR_NL);
		if (!strncmp(cmd->name, buf, len)) {
			found_cnt++;

			/* first time */
			if (found_cnt == 1) {
				found_cmd = cmd;

			/* second time */
			} else if (found_cnt == 2) {
				print_complete_cand(found_cmd);
				print_complete_cand(cmd);

			/* more than one cmd found */
			} else {
				print_complete_cand(cmd);
			}
		}
	}

	/* only one candicate */
	if (found_cnt == 1) {
		delete_all();
		print_buf_str(found_cmd->name);
	} else if (found_cnt > 1) {
		print_complete_cand_end();
	}
}

void process_char_ctrl(void)
{
	switch (ch)
	{
	case '[':
		break;
		
	case 'A':
		process_key_up();
		break;
		
	case 'B':
		process_key_down();
		break;
		
	case 'C':
		process_key_right();
		break;
		
	case 'D':
		process_key_left();
		break;
		
	case 0x7e:
		process_key_delete();
		break;
		
	default:
		if (ch >= 0x40 && ch <= 0x7E)
			ctrl = 0;
		break;
	}
}

static int process_char_normal(void)
{
	switch (ch)
	{
	case '\r':
	case '\n':
		print_newline();
		buf[len] = 0;
		
		if (len > 0)
		{
			history_store();
		}
		
		hist.idx_ref = 0;
		
		return 1;

		/* ESC */
	case 0x1b:
		ctrl++;
		break;
		
	case '\b':
	case KEY_DEL:
		delete_n(1);
		break;
		
	case '\t':
		buf[len] = 0;
		autocomplete();
		break;
		
	case KEY_CTRL_A:
		i -= move_left(i);
		break;
		
	case KEY_CTRL_B:
		i -= move_left(1);
		break;
		
	case KEY_CTRL_C:
		break;
		
	case KEY_CTRL_D:
		break;
		
	case KEY_CTRL_E:
		i += move_right(len - i);
		break;
		
	case KEY_CTRL_F:
		i += move_right(1);
		break;
		
	case KEY_CTRL_K:
		kill_buf_from_i();
		break;
		
	case KEY_CTRL_Y:
		paste_buf();
		break;
		
	default:
		if (ch < 0x20 || ch > 0x7F) {
			/* ignore special char */
		} else {
			print_buf_c(ch);
		}
	}
	
	return 0;
}

static int readline(void)
{
	int ret;
	
	memset(buf, 0, UART_BUFSZ);
	i = len = 0;
	while (i < UART_BUFSZ)
	{
		ch = read_c();
		
	//DBG("got char:%02x\r\n", ch);
		if (ctrl)
		{
			process_char_ctrl();
			continue;
		}
		
		if ((ret = process_char_normal()) > 0)
			return ret;
	}
	
	/* buffer overflow */
	return len;
}

#if 0
static int read_ip(const char *name, unsigned long *result)
{
	unsigned long a, b, c, d, n;
	
	DBG("%s:", name);
	readline();
	n = sscanf(buf, "%lu.%lu.%lu.%lu", &a, &b, &c, &d);
	if (n != 4)
	{
		PRINT("ip format error:%s\n", buf);
		return 1;
	}
	
	*result = IP(a, b, c, d);
	return 0;
}

static int read_int(const char *name, int *result)
{
	int n;
	
	DBG("%s:", name);
	readline();
	n = sscanf(buf, "%d", result);
	if (n != 1)
	{
		PRINT("read int format error\n");
		return 1;
	}
	return 0;
}

static int read_str(const char *name, char *result, int sz)
{
	int n;
	
	DBG("%s:", name);
	n = readline();
	if (n <= 1)
	{
		PRINT("read str format error\n");
		return 1;
	}
	
	strncpy(result, buf, sz);
	if (n > sz) result[sz - 1] = 0;
	
	return 0;
}


static void print_ip(int ip)
{
	unsigned int a, b, c, d;

	d = ip & 0xFF,
	c = (ip & 0xFF00) >> 8;
	b = (ip & 0xFF0000) >> 16;
	a = (ip & 0xFF000000) >> 24;

	PRINT("%d.%d.%d.%d", a, b, c, d);
}
#endif

static void split_with_space(char *str, int *argc, char **argv)
{
	char *pos = str;
	int j = 0;

//	DBG("str:%s, argc:%p, argv:%p, argv2:%p\r\n", pos, argc, argv, *argv);
	while (*pos) {
//		DBG("j:%d, ch:%c\r\n", j, *pos);
		while (*pos == ' ') { *pos = 0; pos++; }
//		DBG("j:%d, ch:%c\r\n", j, *pos);
		if (*pos == 0) break;
//		DBG("j:%d, ch:%c\r\n", j, *pos);

		argv[j++] = pos;
//		DBG("j:%d, ch:%c\r\n", j, *pos);
		while (*pos != ' ' && *pos != 0) pos++; 
//		DBG("j:%d, ch:%c\r\n", j, *pos);
	}

	*argc = j;
}


static int cmd_exists(BTermCmd_t *cmd_array, int array_size)
{
	return 0;
}

void BTerm_RegisterCmds(BTermCmd_t *cmd_array, int array_size)
{
	int j;
	
	if (!cmd_array || !array_size)
		return;
	
	if (cmd_exists(cmd_array, array_size))
	{
		PRINT("%s() failed!(table exists)%s", __FUNCTION__, STR_NL);
		return;
	}
	
	for (j = 0; j < CMD_TAB_MAX; j++)
	{
		if (g_cmds[j].tab)
			continue;
		
		g_cmds[j].tab = cmd_array;
		g_cmds[j].size = array_size;
		break;
	}
	
	if (j == CMD_TAB_MAX)
	{
		PRINT("Cmd table is full!%s", STR_NL);
	}
}

static int BTerm_DoCmd(BTermCmd_t *cmd, int argc, char **argv)
{
	return cmd->func(argc, argv);
}


void BTerm_Main(int p)
{
	int argc;
	int n, j, k, found;
	BTermCmd_t *cmd;
	char *argv[ARG_MAX];
	
	BTerm_RegisterCmds(cmd_basic, ARYSIZ(cmd_basic));

	while (1)
	{
		PRINT(STR_PROM);
		n = readline();
		if (n <= 0) continue;
		
		split_with_space(buf, &argc, argv);
//		for (l = 0; l < argc; l++)
//			PRINT("argv[%d]:%s\r\n", l, argv[l]);
		found = 0;
//		DBG("Get line:%s%s", buf, STR_NL);
		for_each_cmd(j, k, cmd) {
			if (!strncmp(cmd->name, argv[0], UART_BUFSZ)) {
				g_ret = BTerm_DoCmd(cmd, argc, argv);
				found = 1;
				goto next_cmd;
			}
		}
next_cmd:
		if (!found) {
			PRINT("Command Not Found%s", STR_NL);
		}
	}
}



