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
#define CMD_AUTO_COMP_MAX		(16)
#define for_each_cmd(i1, i2, cmd) \
	for (i1 = 0; i1 < CMD_TAB_MAX && g_cmds[i1].tab != NULL; i1++) \
		for (i2 = 0, cmd = &g_cmds[i1].tab[i2]; i2 < g_cmds[i].size; i2++, cmd = &g_cmds[i1].tab[i2]) 


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
typedef enum BTermExpandStat
{
	BTS_Normal,
	BTS_Quote,
	BTS_DoubleQuote,
	BTS_Finding,
	BTS_Expanding,
} BTermExpandStat_t;

/* **************************************************************************** */
/* data                                                                         */
/* **************************************************************************** */
/* **************************************************************************** */
/* struct                                                                       */
/* **************************************************************************** */

typedef struct BTermCmdHistory_str
{
	char cmds[HISTORY_MAX][BUFSZ];
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
SIMP_TERM_CMDHDL_DEFINE(var);
SIMP_TERM_CMDHDL_DEFINE(echo);
SIMP_TERM_CMDHDL_DEFINE(dbg);


static int history_cnt(void);
static int history_start_idx(void);
static int move_left(int n);
static int move_right(int n);

/* **************************************************************************** */
/* local variable                                                               */
/* **************************************************************************** */
static char ch;				// current character
static char buf[BUFSZ];			// buffer of command line
static char buf_copy[BUFSZ];		// buffer for copy
static int idx = 0;			// current index of buffer
static int len = 0;			// length of current command
static int ctrl = 0;			// control seqency flag
static int printing_cand = 0;		// printing candicate flag
static int g_ret;			// return val of last command
static int g_var_cnt;			// variable count
static char g_var_nm[VAR_MAX][VAR_NMSZ];	// variable name
static char g_var_val[VAR_MAX][VAR_VALSZ];	// variable value

BTermCmdHist_t		hist = { 0 };

static BTermCmdTab_t g_cmds[CMD_TAB_MAX] = { NULL };

BTermCmd_t cmd_basic[] =
{
	SIMP_TERM_CMDTAB_ENTRY(exit,		"exit BTerm"),
	SIMP_TERM_CMDTAB_ENTRY(help,		"show help information"),
	SIMP_TERM_CMDTAB_ENTRY(hist,		"show history commands"),
	SIMP_TERM_CMDTAB_ENTRY(var,		"show all variables"),
	SIMP_TERM_CMDTAB_ENTRY(echo,		"echo all args"),
	SIMP_TERM_CMDTAB_ENTRY(dbg,		"for debug"),
};


/* **************************************************************************** */
/* funcitons for basic commands                                                 */
/* **************************************************************************** */
SIMP_TERM_CMDHDL_DEFINE(exit)
{
	print_newline();
	BTerm_Stop();
	
	return 0;
}

SIMP_TERM_CMDHDL_DEFINE(help)
{
	int i, j;
	BTermCmd_t *cmd;

	PRINT("Help Information%s", STR_NL);
	for_each_cmd(i, j, cmd) {
		PRINT("    %-8s: %s%s", cmd->name, cmd->help, STR_NL);
	}

	return 0;
}

SIMP_TERM_CMDHDL_DEFINE(hist)
{
	int i, j, cnt;

	cnt = history_cnt();


	PRINT("Command History (time order)%s", STR_NL);
	j = history_start_idx();
	for (i = 0; i < cnt; i++) {
//	DBG("cnt:%d, i:%d, j:%d\r\n", cnt, i, j);
		PRINT("    %s%s", hist.cmds[j], STR_NL);
		j = (j + 1) % HISTORY_MAX;
	}

	return 0;
}

SIMP_TERM_CMDHDL_DEFINE(dbg)
{

	return 0;
}

SIMP_TERM_CMDHDL_DEFINE(var)
{
	int i, j;

	if (g_var_cnt == 0) {
		PRINT("No Variable%s", STR_NL);
		return 0;
	}

	PRINT("All Variable%s", STR_NL);
	for (i = 0; i < g_var_cnt; i++) {
		PRINT("%-16s = %s%s", g_var_nm[i], g_var_val[i], STR_NL);
	}
	return 0;
}

SIMP_TERM_CMDHDL_DEFINE(echo)
{
	int i;
	DBG("argc:%d\r\n", argc);
	for (i = 1; i < argc; i++) {
		PRINT("%s ", argv[i]);
	}
	print_newline();
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
	//DBG("idx:%d, len:%d, n:%d\r\n", idx, len, n);
	if (n == 0 || idx == len) return 0;

	if (n > len - idx) n = len - idx;

	PRINT("\x1b[%dC", n);

	return n;
}

static int move_left(int n)
{
	if (n == 0 || idx == 0) return 0;

	if (n > idx) n = idx;

	PRINT("\x1b[%dD", n);

	return n;
}

static void move_sub_buf(int to, int from)
{
	int i, head, offset, movlen, dir;

	if (from == len)
		return;

	if (from == to)
		return;

//	DBG("buf:%s, i:%d, len:%d, from:%d, to:%d\r\n", buf, idx, len, from, to);
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
//	DBG("from:%d, to:%d, head:%d, len:%d, off:%d, dir:%d\r\n", from, to, head, movlen, offset, dir);
	for (i = 0; i < movlen; i++) {
		buf[head] = buf[head + (dir * offset)];
		head += dir;
	}
	buf[head] = 0;
//	DBG("buf:%s, i:%d, len:%d, from:%d, to:%d\r\n", buf, idx, len, from, to);
}

static void print_buf_c(char c)
{
	print_c(c);

//	DBG("idx:%d, len:%d, buf:%s\r\n", idx, len, buf);
	/* move chars behind idx */
	move_sub_buf(idx + 1, idx);

	buf[idx] = c;
	idx++;
	len++;

//	DBG("idx:%d, len:%d, buf:%s, PRINTED_STR:%s\r\n", idx, len, buf, buf + idx);
	if (idx < len) {
		PRINT("%s", buf + idx);
		move_left_force(len - idx);
	}
}

static void print_buf_str(char *str)
{
	int slen = strlen(str);

	if (slen <= 0)
		return;

	//DBG("buf:%s, slen:%d, str:%s, idx:%d, len:%d\r\n", buf, slen, str, idx, len);
	print_str(str);

	/* move chars behind idx */
	move_sub_buf(idx + slen, idx);

	memcpy(buf + idx, str, slen);
	idx += slen;
	len += slen;

	if (idx < len) {
		PRINT("%s", buf + idx);
		move_left_force(len - idx);
	}
}

static void delete_n_left(int n)
{
	int i;
	
	for (i = 0; i < n; i++)
		PRINT("\b");
	
	for (i = idx; i < len; i++)
		PRINT("%c", buf[i]);

	for (i = 0; i < (n - (len - idx)); i++)
		PRINT(" ");
	
	for (i = 0; i < n; i++)
		PRINT("\b");

	move_sub_buf(idx - n, idx);
}

static void delete_n_right(int n)
{
	int i;
	
	for (i = idx + n; i < len; i++)
		PRINT("%c", buf[i]);
	
	for (i = 0; i < n; i++)
		PRINT(" ");
	
	for (i = 0; i < len - idx; i++)
		PRINT("\b");

	move_sub_buf(idx, idx + n);
}

static void delete_n(int n)
{
	//DBG("idx:%d, len:%d, n:%d\r\n", idx, len, n);
	if (n == 0) {
		return;
	} else if (n > 0) {

		/* no char left */
		if (idx == 0)
			return;
		if (n > idx)
			n = idx;
		delete_n_left(n);
		idx -= n;
	} else {
		/* no char left */
		if (idx == len)
			return;

		n = 0 - n;
		if (n > (len - idx))
			n = len - idx;
		delete_n_right(n);
	}

	len -= n;
}

static void delete_all(void)
{
//	DBG("idx:%d, len:%d\r\n", idx, len);
	idx -= move_left(idx);
	delete_n(-len);

	idx = len = 0;
}

static void kill_buf_from_i(void)
{
	int clen = len - idx;
	memcpy(buf_copy, buf + idx, clen);
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
	idx += move_right(1);
}

static void process_key_left(void)
{
	ctrl = 0;
	idx -= move_left(1);
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

static int find_same_prefix(char **cmdstr, int size)
{
	int ret, i, j;

	for(i = 0; i < BUFSZ; i++) {
		if (buf[i] == 0)
			break;
		for(j = 0; j < size; j++) {
			if (!cmdstr[j] ||
				cmdstr[j][i] == 0 ||
				cmdstr[j][i] != buf[i])
			break;
		}

		if (j < size)
			break;
	}

	return i;
}

static void print_complete_cand_end(void)
{
	printing_cand = 0;
	PRINT("%s%s", STR_NL STR_PROM, buf);
}

static void autocomplete(void)
{
	int i, j, cnt = 0, prefix;
	BTermCmd_t *cmd;
	char *fcmds[CMD_AUTO_COMP_MAX];

	for_each_cmd(i, j, cmd) {
//		DBG("compared \"%s\" and \"%s\", idx:%d, result:%d %s", cmd->name, buf, idx, strncmp(cmd->name, buf, idx + 1), STR_NL);
		if (!strncmp(cmd->name, buf, len)) {
			fcmds[cnt++] = cmd->name;

			if (cnt >= CMD_AUTO_COMP_MAX - 1)
				break;
		}
	}

	prefix = find_same_prefix(fcmds, cnt);
				print_complete_cand(cmd);
	/* only one candicate */
	if (cnt == 1) {
		delete_all();
		print_buf_str(found_cmd->name);
	} else if (cnt > 1) {
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
		idx -= move_left(idx);
		break;
		
	case KEY_CTRL_B:
		idx -= move_left(1);
		break;
		
	case KEY_CTRL_C:
		break;
		
	case KEY_CTRL_D:
		print_newline();
		BTerm_Stop();
		break;
		
	case KEY_CTRL_E:
		idx += move_right(len - idx);
		break;
		
	case KEY_CTRL_F:
		idx += move_right(1);
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
	
	memset(buf, 0, BUFSZ);
	idx = len = 0;
	while (idx < BUFSZ)
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
static int var_index(char *var)
{
	int i, j;

	DBG("a\r\n");
	for (i = 0; i < g_var_cnt; i++) {
		DBG("i:%d, var:%p, nm:%p\r\n", i, var, g_var_nm[i]);
		if (!strncmp(var, g_var_nm[i], VAR_NMSZ))
			return i;
	}

	return -1;
}

static char *var_value(char *var)
{
	int i = var_index(var);

	DBG("i:%d\r\n", i);
	if (i < 0) return NULL;
	DBG("i:%d\r\n", i);

	return g_var_val[i];
}

static int is_space(char c)
{
	if (c == ' ' || c == '\t')
		return 1;

	return 0;
}

static int is_special(char c)
{
	if (is_space(c))
		return 1;

	if (c == '\'' || c == '\"' || c == 0)
		return 1;

	return 0;
}

static int set_var(void)
{
	int i, q = 0;
	char c, name[VAR_NMSZ];
	int namei, vali, var_idx;

	/* skip space */
	for (i = 0; i < len && is_space(buf[i]); i++) {
		if (buf[i] == 0) return 0;
	}

	namei = i;

	/* find '=' */
	for (; i < len && buf[i] != '='; i++) {
		if (buf[i] == 0) return 0;
	}
	if (i >= len) return 0;

	vali = i + 1;

	/* skip '=' */
	i--;
	/* ignore ending space of var name */
	while (is_space(buf[i])) i--;

	memcpy(name, buf + namei, i - namei + 1);
	name[i - namei + 1] = 0;

	var_idx = var_index(name);
	if (var_idx < 0) {
		var_idx = g_var_cnt++;
		strncpy(g_var_nm[var_idx], name, VAR_NMSZ);
	}

	strncpy(g_var_val[var_idx], buf + vali, VAR_VALSZ);

	return 1;
}

static int expand_var(int start)
{
	int i, j, q, varlen;
	BTermExpandStat_t stat = BTS_Normal;
	char name[VAR_NMSZ];
	int name_starti, name_endi;
	int var_starti, var_endi;
	char *value;

	for (i = start; i < BUFSZ; i++) {
		switch (stat) {
		case BTS_Normal:
			if (buf[i] == 0) return 0;
			if (buf[i] == '$') {
				stat = BTS_Finding;
				var_starti = i;
			}
			break;

		/* find next signle quote */
		case BTS_Quote:
			if (buf[i] == 0) {
				ERR("Not found ending quote\r\n");
				return 0;
			}
			for (; i < len && buf[i] != '\''; i++);

			if (i >= len) return 0;

			stat = BTS_Normal;

			break;

		case BTS_Finding:
			if (buf[i] == '{') {
				q = 1;
				i++;
			} else {
				q = 0;
			}

			name_starti = i;
			for (j = i; j < len && !is_special(buf[j]); j++) {
				if (buf[j] == '}') {
					if (q == 1) {
						name_endi = j - 1;
						var_endi = j;
						stat = BTS_Expanding;
						break;
					} else {
						ERR("Expanding Variable ERROR, buf:%s, index:%d\r\n",
								buf, var_starti);
						return 0;
					}
				}
			}
			if (stat == BTS_Expanding) break;

			if (q == 1) {
				ERR("Expanding Variable ERROR, "
					"Cannot find ending \'}\'. buf:%s, starti:%d, endi:%d\r\n",
					buf, var_starti, j);
				return 0;
			}

			DBG("i:%d, len:%d, buf:%s, j:%d\r\n", i, len, buf, j);
			name_endi = var_endi = j - 1;
			stat = BTS_Expanding;
			break;

		case BTS_Expanding:
			memcpy(name, buf + name_starti, name_endi - name_starti + 1);
			DBG("name start:%d, end:%d, var start:%d, end:%d\r\n", name_starti, name_endi, var_starti, var_endi);
			name[name_endi - name_starti + 1] = 0;

			value = var_value(name);
			if (value) {
				varlen = strlen(value);
				strcpy(buf + var_starti, value);
			} else {
				varlen = 0;
			}

			move_sub_buf(var_starti + varlen, var_endi + 1);
			DBG("expand over!, buf:%s, var:%s, move sbu buf %d to %d\r\n",
					buf, value, var_endi + 1, var_starti + varlen);
			return 1;
		}
	}

	return 0;
}

static void split_with_space(char *str, int *argc, char **argv)
{
	char *pos = str;
	int i = 0;

//	DBG("str:%s, argc:%p, argv:%p, argv2:%p\r\n", pos, argc, argv, *argv);
	while (*pos) {
//		DBG("i:%d, ch:%c\r\n", i, *pos);
		while (*pos == ' ') { *pos = 0; pos++; }
//		DBG("i:%d, ch:%c\r\n", i, *pos);
		if (*pos == 0) break;
//		DBG("i:%d, ch:%c\r\n", i, *pos);

		argv[i++] = pos;
//		DBG("i:%d, ch:%c\r\n", i, *pos);
		while (*pos != ' ' && *pos != 0) pos++; 
//		DBG("i:%d, ch:%c\r\n", i, *pos);
	}

	*argc = i;
}


static int cmd_exists(BTermCmd_t *cmd_array, int array_size)
{
	return 0;
}

void BTerm_RegisterCmds(BTermCmd_t *cmd_array, int array_size)
{
	int i;
	
	if (!cmd_array || !array_size)
		return;
	
	if (cmd_exists(cmd_array, array_size))
	{
		PRINT("%s() failed!(table exists)%s", __FUNCTION__, STR_NL);
		return;
	}
	
	for (i = 0; i < CMD_TAB_MAX; i++)
	{
		if (g_cmds[i].tab)
			continue;
		
		g_cmds[i].tab = cmd_array;
		g_cmds[i].size = array_size;
		break;
	}
	
	if (i == CMD_TAB_MAX)
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
	int n, i, j, found;
	BTermCmd_t *cmd;
	char *argv[ARG_MAX];
	
	memset(g_var_nm, 0, VAR_MAX * VAR_NMSZ);
	memset(g_var_val, 0, VAR_MAX * VAR_VALSZ);
	g_var_cnt = 0;
	BTerm_RegisterCmds(cmd_basic, ARYSIZ(cmd_basic));

	while (1)
	{
		PRINT(STR_PROM);
		readline();
		if (len <= 0) continue;

		if (set_var()) continue;

		while(expand_var(0));
		
		split_with_space(buf, &argc, argv);
//		for (k = 0; k < argc; k++)
//			PRINT("argv[%d]:%s\r\n", k, argv[k]);
		found = 0;
//		DBG("Get line:%s%s", buf, STR_NL);
		for_each_cmd(i, j, cmd) {
			if (!strncmp(cmd->name, argv[0], BUFSZ)) {
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



