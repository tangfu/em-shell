/**
 * @file em-shell.h
 * @brief
 *
 *	一个可以嵌入软件中的shell库
 *
 * @author tangfu - abctangfuqiang2008@163.com
 * @version 0.1
 * @date 2011-06-09
 */

#ifndef __SHELL_H__
#define	__SHELL_H__

#define _LARGEFILE_SOURCE
#define _LARGEFILE64_SOURCE
#define _FILE_OFFSET_BITS 64


#define get_cmd_len(cmd)	sizeof(cmd)/sizeof(CMD_OBJ)

#define CMD_FILENAME_LEN 512
#define CMD_OBJ_NUM 128
#define CMD_STR_LEN 64
#define CMD_INFO_LEN 128
#define CMD_HISTORY_LEN	15
#define CMD_BUFFER_LEN 1024			/** standard xterm 80*24 */
#define MAX_PATCH_NUMBER	200
#define NOTIFY_PATCH_NUMBER 70


#define MAX_PARA_NUMBER 15			/** parameter max number = 15 */
#define MAX_PIPE_LAYER  5			/** pipe layer max number */

#ifndef SH_BOOL
typedef int SH_BOOL;
#endif

#ifndef SH_TRUE
#define SH_TRUE 0
#endif

#ifndef SH_FALSE
#define SH_FALSE -1
#endif


#define SHELL_STOP_SIGNAL SIGRTMAX-1

typedef enum cmd_type_s CMD_TYPE;
typedef enum shell_start_type_s SHELL_START_TYPE;
//typedef enum match_type_s MATCH_TYPE;
typedef struct __CMD_OBJ CMD_OBJ;
typedef struct __SHELL SHELL;
typedef struct __CMD_HISTORY	CMD_HISTORY;
typedef struct __HISTORY_STACK HISTORY_STACK;
//typedef struct __CMD_OBJ_PARAM CMD_PARAM;

//enum match_type_s {COMMAND, DIRECTORY};

enum shell_start_type_s {SHELL_START_UNBLOCK = 0, SHELL_START_BLOCK};
enum cmd_type_s {INNER_CMD, EXTERN_CMD, UNKNOWN_CMD};

struct __CMD_OBJ {
        char	CmdStr[CMD_STR_LEN];
        char	*CmdInfo;
        void	( *CmdHandler )( CMD_OBJ *cmd, const char *option );
        void 	*param;
};

struct __HISTORY_STACK {
        int cur_history_number;
        struct __CMD_HISTORY *entry;
        struct __CMD_HISTORY *stack_pointer;
};

struct __CMD_HISTORY {
        char *cmd_buf;
        //	struct __CMD_HISTORY *prior;
        //	struct __CMD_HISTORY *next;
};

struct __SHELL {
        SH_BOOL( *init )( SHELL *this, CMD_OBJ cmd[] , int i,  char *prompt );
        void ( *start )( SHELL *this, SHELL_START_TYPE type );
        void ( *stop )( SHELL *this );
        void ( *close )( SHELL *this );
};

#ifdef __cplusplus
extern "C" {
#endif

        SHELL* create_shell();
        void destroy_shell( SHELL *shell );

#ifdef __cplusplus
}
#endif

#endif	/* __SHELL_H__  */
