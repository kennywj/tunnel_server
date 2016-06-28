//
// include file for buildin command
//
#define MAX_ARGS 12

struct cmd_tbl
{
	const char *name;
	void (*func)(int argc, char *argv[]);
	const char *desc;
	const char *usage;
};
extern struct cmd_tbl commands[];
extern struct cmd_tbl *curr_cmd;
extern void cmd_quit(int argc, char* argv[]);
extern void cmd_help(int argc, char* argv[]);
extern void cmd_stat(int argc, char* argv[]);
extern void cmd_cfg(int argc, char* argv[]);
extern void cmd_ver(int argc, char* argv[]);
extern void cmd_cvr(int argc, char* argv[]);