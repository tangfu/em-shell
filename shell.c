#include "shell.h"
#include "atomic.h"
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <termios.h>
#include <sched.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <pthread.h>
#include <signal.h>

static struct termios old_settings;
static struct winsize size;
static int have_back_proc = 0;
static int patch_flag = 0;
static char *patch[MAX_PATCH_NUMBER] = { NULL };
static char buf_bak[CMD_BUFFER_LEN];
static char *argv[MAX_PARA_NUMBER];
static int pipe_cnt[MAX_PIPE_LAYER] = { -1 };
static char outfile[CMD_FILENAME_LEN] = { 0 }, infile[CMD_FILENAME_LEN] = {0};

/* **************************************************
 *
 *
 *
 * *******************************************/

struct flag {
        int flag_background ;
        int flag_inredirect ;
        int flag_outredirect ;
        int flag_pipe ;
        int fd_in;
        int fd_out;
} ;

typedef struct __SHELL_INTERNAL SHELL_INTERNAL;
struct __SHELL_INTERNAL {
        SH_BOOL( *init )( SHELL * this, CMD_OBJ * cmd, int n, char *buf );
        void ( *start )( SHELL * this, SHELL_START_TYPE type );
        void ( *stop )( SHELL * this );
        void ( *close )( SHELL * this );
        struct __CMD_OBJ *cmdobj;
        int cmdobj_num;
        char *prompt;
        atomic_t flag_init;
        atomic_t flag_start;
        pthread_t id;
        HISTORY_STACK history;
        pthread_rwlock_t lock;

};

const char *path[] = {
        "./", "/bin", "/sbin", "/usr/bin", "/usr/sbin", "/usr/local/bin",
        NULL
};
/* ************************************************************************ */
void set_keypress( void );
void reset_keypress( void );
void set_nodelay_mode( void );
void get_window_size( struct winsize * );

void free_patch_buf();
void show_match_item( unsigned int nfiles );
int find_command( SHELL_INTERNAL * this, const char *command, CMD_TYPE * type );
int find_match( SHELL_INTERNAL * this, char *buf , int buf_len );
inline static void print_prompt( SHELL_INTERNAL * this );

void forkexec( struct flag *cmd_flag );
static inline void earse_line_memory( int buf_len );
static inline void do_command_buf( SHELL_INTERNAL * this, char *buf,
                                   int buf_len );
static int command( SHELL_INTERNAL * this, char *cmdStr, int cmd_len );
static void *shell_entry( void *this );
static void shell_start( SHELL * this, SHELL_START_TYPE type );
static SH_BOOL initialise( SHELL * shell, CMD_OBJ cmd[], int n, char *prompt );
static void shell_close( SHELL * this );
static void shell_stop( SHELL * this );

struct __SHELL *create_shell();
void destroy_shell( SHELL * shell );
static void catch_signal( int i );
SH_BOOL check_cmd_style( CMD_OBJ cmd[], int n );

/*-----------------------------------------------------------------------------
 *  这个暂时只支持终端字符集是zh_CN.utf8的情况
 *-----------------------------------------------------------------------------*/
int calc_str( const char *str )
{
        if( str == NULL )
                return -1;

        const char *p = str;
        size_t len = strlen( str );

        while( *p != '\0' ) {
                if( ( int )( *p ) < 0 ) {
                        p += 3;
                        --len;
                } else
                        ++p;
        }

        return ( int )len;
}

void free_patch_buf()
{
        int i = 0;

        for( ; i < MAX_PATCH_NUMBER; ++i ) {
                if( patch[i] != NULL ) {
                        free( patch[i] );
                        patch[i] = NULL;
                }
        }
}

void forkexec( struct flag *cmd_flag )
{
        int pipe_layer_var = 0, cnt = 0;
        char *tmp_template, *tmp_file;
        int fd1,  fd2,  pid, pid2;

        if( ( pid = vfork() ) == 0 ) {
                sigset_t sigmask;
                sigemptyset( &sigmask );
                sigaddset( &sigmask, SIGINT );
                pthread_sigmask( SIG_UNBLOCK, &sigmask, NULL );	//in child proc

                if( cmd_flag->flag_outredirect == 1 ) {
                        dup2( cmd_flag->fd_out, STDOUT_FILENO );
                }

                if( cmd_flag->flag_inredirect == 1 ) {
                        dup2( cmd_flag->fd_in, STDIN_FILENO );
                }

                if( cmd_flag->flag_pipe > 0 ) {
                        tmp_template = malloc( 2 * CMD_FILENAME_LEN );

                        if( tmp_template == NULL ) {
                                printf( "malloc failed in pipe prase\n" );
                                return;
                        }

                        tmp_file = tmp_template + CMD_FILENAME_LEN;
                        strncpy( tmp_template, "/tmp/myshell",
                                 CMD_FILENAME_LEN );

                        while( 1 ) {
                                if( pipe_layer_var > cmd_flag->flag_pipe )
                                        break;

                                if( ( pid2 = vfork() ) < 0 ) {
                                        printf( "vfork failed\n" );
                                        return;
                                }

                                if( pid2 == 0 ) {
                                        if( pipe_layer_var > 0 ) {
                                                snprintf( tmp_file,
                                                          CMD_FILENAME_LEN,
                                                          "%s_%d",
                                                          tmp_template,
                                                          pipe_layer_var
                                                          - 1 );
                                                fd1 =
                                                        open( tmp_file,
                                                              O_RDONLY,
                                                              0644 );
                                                dup2( fd1, 0 );
                                        }

                                        if( pipe_layer_var < cmd_flag->flag_pipe ) {
                                                snprintf( tmp_file,
                                                          CMD_FILENAME_LEN,
                                                          "%s_%d",
                                                          tmp_template,
                                                          pipe_layer_var );
                                                fd2 =
                                                        open( tmp_file,
                                                              O_WRONLY |
                                                              O_CREAT |
                                                              O_TRUNC, 0644 );
                                                dup2( fd2, 1 );
                                        }

                                        //printf("pipe_layer = %d | pipe_cnt=%d <%s>\n",pipe_layer_var,pipe_cnt[pipe_layer_var],argv[pipe_cnt[pipe_layer_var]]);
                                        execvp( argv
                                                [pipe_cnt
                                                 [pipe_layer_var]],
                                                &argv[pipe_cnt
                                                      [pipe_layer_var]] );
                                        exit( 0 );
                                } else {
                                        if( waitpid( pid2, NULL, 0 ) ==
                                            -1 )
                                                printf
                                                ( "wait child proc failed\n" );

                                        //printf("wait success %u\n",pid2);
                                        ++pipe_layer_var;
                                }
                        }

                        /*  remove tmp file */
                        for( cnt = 0; cnt < pipe_layer_var - 1; ++cnt ) {
                                snprintf( tmp_file, CMD_FILENAME_LEN,
                                          "%s_%d", tmp_template, cnt );
                                remove( tmp_file );
                        }

                        free( tmp_template );
                        exit( 0 );
                } else {
                        /* printf("exec - argv[0]:%s\n",argv[0]); */
                        execvp( argv[0], argv );
                        exit( 0 );
                }
        } else {
                if( cmd_flag->flag_background == 0 ) {
                        signal( SIGCHLD,  SIG_DFL );

                        if( waitpid( pid, NULL, 0 ) == -1 )
                                printf( "wait child proc %d failed\n",
                                        pid );
                } else {
                        printf( "[ background ] %d\n", pid );
                        signal( SIGCHLD,  SIG_IGN );
                }

                //              printf("flag_back : %d\n",flag_background);
        }
}

int find_command( SHELL_INTERNAL * this, const char *command, CMD_TYPE * type )
{
        if( this == NULL ) {
                fprintf( stderr, "find_command - SHELL OBJECT POINTER is NULL\n" );
                *type = UNKNOWN_CMD;
                return -1;
        }

        if( command == NULL ) {
                fprintf( stderr, "command equal NULL,exit find_command\n" );
                *type = UNKNOWN_CMD;
                return -1;
        } else if( strlen( command ) > CMD_BUFFER_LEN ) {
                fprintf( stderr, "command is illegal,exit find_command\n" );
                *type = UNKNOWN_CMD;
                return -1;
        }

        int i;

        for( i = 0; i < this->cmdobj_num; ++i )
                if( strlen( command ) == strlen( ( this->cmdobj + i )->CmdStr ) )
                        if( strcmp( ( this->cmdobj + i )->CmdStr, command ) == 0 ) {
                                *type = INNER_CMD;
                                return i;	/* inner command */
                        }

        DIR *dp;
        struct dirent *dirp;
        char *path[] =
        { "./", "/bin", "/sbin/", "/usr/bin", "/usr/sbin", NULL };

        /* 使当前目录下的程序可以被运行,如命令"./fork"可以被正确解释和执行 */
        if( strncmp( command, "./", 2 ) == 0 )
                command = command + 2;

        /* 分别在当前目录、/bin和/usr/bin目录查找要可执行程序 */
        i = 0;

        while( path[i] != NULL ) {
                if( ( dp = opendir( path[i] ) ) == NULL )
                        printf( "can not open bin \n" );

                while( ( dirp = readdir( dp ) ) != NULL ) {
                        if( strcmp( dirp->d_name, command ) == 0 ) {
                                closedir( dp );
                                *type = EXTERN_CMD;
                                return 0;
                        }
                }

                closedir( dp );
                i++;
        }

        *type = UNKNOWN_CMD;
        return -1;
}

void show_match_item( unsigned int nfiles )
{
        if( nfiles <= 1 )
                return;

        unsigned int i = 0;
        struct winsize size;
        get_window_size( &size );
        int tabstops = 2;
        char ch;
        unsigned ncols, nrows, row, nc, len;
        unsigned column = 0;
        unsigned nexttab = 0;
        unsigned column_width = 0;	/* used only by STYLE_COLUMNS */

        if( nfiles > NOTIFY_PATCH_NUMBER ) {
                printf( "\nDisplay all %d possibilities? (y or n)", nfiles );

                while( ( ch = getchar() ) ) {
                        if( ch == 'y' || ch == 'Y' )
                                break;
                        else if( ch == 'n' || ch == 'N' )
                                return;
                }
        }

        while( i++ < nfiles ) {
                /* find the longest file name, use that as the column width */
                if( patch[i - 1] == NULL ) {
                        fprintf( stderr, "patch[i] error\n" );
                        return;
                }

                len = strlen( patch[i - 1] );

                if( column_width < len )
                        column_width = len;
        }

        column_width += tabstops;
        ncols = ( int )( size.ws_col / column_width );

        if( ncols > 1 ) {
                nrows = nfiles / ncols;

                if( nrows * ncols < nfiles )
                        nrows++;	/* round up fractionals */
        } else {
                nrows = nfiles;
                ncols = 1;
        }

        for( row = 0; row < nrows; row++ ) {
                printf( "\n" );

                for( nc = 0; nc < ncols; nc++ ) {
                        /* reach into the array based on the column and row */
                        //          if (all_fmt & DISP_ROWS)
                        //                  i = (row * ncols) + nc;  //display across row
                        //          else
                        i = ( nc * nrows ) + row;	/* display by column */

                        if( i < nfiles ) {
                                if( column > 0 ) {
                                        nexttab -= column;
                                        printf( "%*s", nexttab, "" );
                                        column += nexttab;
                                }

                                nexttab = column + column_width;
                                //column += list_single(dn[i]);
                                //column += printf("%s", patch[i]);
                                printf( "%s", patch[i] );
                                column += calc_str( patch[i] );
                        }
                }

                column = 0;
        }

        printf( "\n" );
}

int find_match( SHELL_INTERNAL * this, char *buf , int buf_len )
{
        unsigned int i = 0, j = 0, type;
        struct stat64 stat_buf;
        int dirname_flag = 0, curr_flag = 0, result = 0;	//  没有基本名
        /* int basename_flag; */
        static char currfile[CMD_FILENAME_LEN] = { '\0' };
        static char new_match[CMD_FILENAME_LEN] = { '\0' };
        /* second , match extern command   [limitation] - do not parse enviroment var */
        char *p = buf, *new = new_match, *tp = NULL, *tmp = NULL;
        int commond_len = 0, len = 0, new_len = 0, h = 0;
        DIR *dp;
        static char dir_name[CMD_FILENAME_LEN] = { '\0' };
        static char base_name[CMD_FILENAME_LEN] = { '\0' };
        //MATCH_TYPE type;
        struct dirent *dirp;

        if( this == NULL )
                return -1;
        else
                buf[buf_len - 1] = '\0';

        while( *p == ' ' )
                ++p;

        /*-----------------------------------------------------------------------------
         *  因为匹配都是针对一行中最后的部分来讲的，前面的空格就没啥作用了
         *-----------------------------------------------------------------------------*/
        if( ( tp = strrchr( p, '>' ) ) != NULL || ( tp = strrchr( p, '<' ) ) != NULL ) {
                ++tp;

                while( *tp == ' ' )
                        ++tp;

                type = 1;
        } else if( ( tp = strrchr( p, '|' ) ) != NULL ) {
                ++tp;

                while( *tp == ' ' )
                        ++tp;

                if( ( tmp = strrchr( tp, ' ' ) ) == NULL ) {
                        p = tp;
                        type = 0;
                } else {
                        type = 1;
                        tp = ++tmp;
                        tmp = NULL;
                }
        } else if( ( tp = strrchr( p, ' ' ) ) == NULL ) {
                type = 0;	//command type
        } else {
                type = 1;	// directory type
                ++tp;
                //strncpy(dir_name,tp,CMD_FILENAME_LEN);
                //strncpy(base,tp,CMD_FILENAME_LEN);
        }

        switch( type ) {
                case 0:
                        /* first , match inner command */
                        commond_len = strlen( p );

                        for( ; i < ( unsigned int )this->cmdobj_num; ++i )
                                if( strncmp( ( this->cmdobj + i )->CmdStr, p, commond_len )
                                    == 0 ) {
                                        if( j >= MAX_PATCH_NUMBER ) {
                                                fprintf( stderr,
                                                         "\nbeyond PATCH MAX NUMBER %d\n",
                                                         MAX_PATCH_NUMBER );
                                                result = -1;
                                                goto END;
                                        }

                                        len =
                                                strlen( ( this->cmdobj + i )->CmdStr ) -
                                                commond_len;
                                        patch[j] = strdup( ( this->cmdobj + i )->CmdStr );

                                        if( patch[j] == NULL ) {
                                                fprintf( stderr, "malloc failed\n" );
                                                return -1;
                                        }

                                        /* patch[j] =
                                                malloc( strlen( ( this->cmdobj + i )->CmdStr ) +
                                                        1 );
                                        strcpy( patch[j], ( this->cmdobj + i )->CmdStr ); */
                                        ++j;
                                }

                        i = 1;

                        if( strncmp( p, "./", 2 ) == 0 ) {
                                p = p + 2;
                                i = 0;
                                curr_flag = 1;
                        }

                        while( path[i] != NULL ) {
                                if( ( dp = opendir( path[i] ) ) == NULL )
                                        printf( "can not open %s \n", path[i] );

                                while( ( dirp = readdir( dp ) ) != NULL ) {
                                        if( strncmp( dirp->d_name, p, commond_len ) == 0 ) {
                                                if( j >= MAX_PATCH_NUMBER ) {
                                                        fprintf( stderr,
                                                                 "\nbeyond PATCH MAX NUMBER %d\n",
                                                                 MAX_PATCH_NUMBER );
                                                        result = -1;
                                                        goto END;
                                                }

                                                patch[j] = strdup( dirp->d_name );

                                                if( patch[j] == NULL ) {
                                                        fprintf( stderr, "malloc failed\n" );
                                                        return -1;
                                                }

                                                /* patch[j] =
                                                        malloc( strlen( dirp->d_name ) + 1 );
                                                strcpy( patch[j], dirp->d_name ); */
                                                ++j;
                                        }
                                }

                                closedir( dp );
                                i++;

                                if( curr_flag == 1 )
                                        break;
                        }

                        if( j == 1 )
                                strcpy( p, patch[j - 1] );
                        else {
                                i = j;

                                while( j-- ) {
                                        len = strlen( patch[j] ) - commond_len;

                                        if( len == 0 )
                                                break;
                                        else {
                                                new_len = strlen( new );

                                                if( new_len == 0 ) {
                                                        strncpy( new,
                                                                 patch[j] + commond_len,
                                                                 len + 1 );
                                                        continue;
                                                } else {
                                                        new_len =
                                                                len >
                                                                new_len ? new_len : len;

                                                        for( h = 0; h < new_len; ++h ) {
                                                                if( *( char * )
                                                                    ( patch[j] +
                                                                      commond_len + h ) !=
                                                                    *( new + h ) )
                                                                        break;
                                                        }

                                                        if( h == 0 )
                                                                break;	//没有最大匹配项
                                                        else
                                                                new_len = h;

                                                        strncpy( new,
                                                                 patch[j] + commond_len,
                                                                 new_len );
                                                        new[new_len] = '\0';
                                                }
                                        }
                                }

                                if( h != 0 )
                                        strncpy( p + commond_len, new, new_len );
                                else
                                        show_match_item( i );

                                free_patch_buf();
                        }

                        break;
                case 1:

                        //parse dirname and basename
                        if( tp == '\0' ) {
                                dirname_flag = 0;
                                /* basename_flag = 0; */
                        }

                        if( strncmp( tp, "..", strlen( tp ) > 2 ? strlen( tp ) : 2 ) == 0 ) {
                                tp[strlen( tp )] = '/';
                                tp[strlen( tp ) + 1] = '\0';
                                break;
                        }

                        tmp = strrchr( tp, '/' );

                        if( tmp == NULL ) {
                                dirname_flag = 0;	// 没有指定目录，使用当前目录
                                /* basename_flag = 1;           // 含有基本文件前缀 */
                                strcpy( base_name, tp );
                                commond_len = strlen( base_name );
                        } else {
                                dirname_flag = 1;	// 已经指定目录
                                strncpy( dir_name, tp, tmp - tp + 2 );
                                dir_name[tmp - tp + 1] = '\0';

                                if( strlen( tp ) == ( size_t )( tmp - tp + 1 ) )
                                        commond_len = 0;
                                /* basename_flag = 0; */
                                else {
                                        /* basename_flag = 1; */
                                        strcpy( base_name, tmp + 1 );
                                        commond_len = strlen( base_name );
                                }
                        }

                        //printf("\ndirname = %s[%d], basename = %s[%d]\n",dir_name, dirname_flag, base_name, basename_flag);

                        if( dirname_flag ) {
                                if( strncmp( dir_name, "../", strlen( dir_name ) ) == 0 )
                                        dp = opendir( ".." );
                                else
                                        dp = opendir( dir_name );
                        } else
                                dp = opendir( "." );

                        if( dp == NULL ) {
                                fprintf( stderr, "DIR open failed" );
                                result = -1;
                                break;
                        }

                        while( ( dirp = readdir( dp ) ) ) {
                                if( !strcmp( dirp->d_name, "." )
                                    || !strcmp( dirp->d_name, ".." )
                                    || strncmp( dirp->d_name, base_name, commond_len ) )
                                        continue;

                                if( j >= MAX_PATCH_NUMBER ) {
                                        fprintf( stderr,
                                                 "\nbeyond PATCH MAX NUMBER %d\n",
                                                 MAX_PATCH_NUMBER );
                                        result = -1;
                                        goto END;
                                }

                                sprintf( currfile, "%s%s", dir_name, dirp->d_name );

                                //printf("%s\n",currfile);
                                if( lstat64( currfile, &stat_buf ) == -1 ) {
                                        perror( "\nfile_info get failed " );
                                        result = -1;
                                        goto END;
                                }

                                if( S_ISDIR( stat_buf.st_mode ) ) {
                                        patch[j++] = malloc( strlen( dirp->d_name ) + 2 );

                                        if( patch[j - 1] == NULL ) {
                                                fprintf( stderr, "malloc failed\n" );
                                                return -1;
                                        }

                                        strcpy( patch[j - 1], dirp->d_name );
                                        strcat( patch[j - 1], "/" );
                                } else {
                                        patch[j++] = strdup( dirp->d_name );

                                        if( patch[j - 1] == NULL ) {
                                                fprintf( stderr, "malloc failed\n" );
                                                return -1;
                                        }

                                        /* patch[j++] = malloc( strlen( dirp->d_name ) + 1 );
                                        strcpy( patch[j - 1], dirp->d_name ); */
                                        //cnt++;
                                }
                        }

                        closedir( dp );

                        if( j == 1 )
                                sprintf( tp, "%s%s", dir_name, patch[j - 1] );
                        else {
                                i = j;

                                while( j-- ) {
                                        len = strlen( patch[j] ) - commond_len;

                                        if( len == 0 )
                                                break;
                                        else {
                                                new_len = strlen( new );

                                                if( new_len == 0 ) {
                                                        strncpy( new,
                                                                 patch[j] + commond_len,
                                                                 len + 1 );
                                                        continue;
                                                } else {
                                                        new_len =
                                                                len >
                                                                new_len ? new_len : len;

                                                        for( h = 0; h < new_len; ++h ) {
                                                                if( *( char * )
                                                                    ( patch[j] +
                                                                      commond_len + h ) !=
                                                                    *( new + h ) )
                                                                        break;
                                                        }

                                                        if( h == 0 )
                                                                break;	//没有最大匹配项
                                                        else
                                                                new_len = h;

                                                        strncpy( new,
                                                                 patch[j] + commond_len,
                                                                 new_len );
                                                        new[new_len] = '\0';
                                                }
                                        }
                                }

                                if( h != 0 ) {
                                        sprintf( tp, "%s%s%s", dir_name, base_name, new );
                                } else
                                        show_match_item( i );

                                free_patch_buf();
                        }

                        break;
                default:
                        break;
        }

END:
        earse_line_memory( CMD_BUFFER_LEN );
        //if(result == -1)
        //memset(buf, '\0', CMD_BUFFER_LEN);
        print_prompt( this );
        printf( "%s", buf );
        return result;
}

struct __SHELL *create_shell() {
        int i;
        SHELL_INTERNAL *sh =
                ( SHELL_INTERNAL * ) malloc( sizeof( struct __SHELL_INTERNAL ) );

        if( sh == NULL ) {
                fprintf( stderr, "create shell failed\n" );
                return NULL;
        }

        pthread_rwlockattr_t attr;
        pthread_rwlockattr_init( &attr );
        pthread_rwlockattr_setpshared( &attr, PTHREAD_PROCESS_SHARED );
        pthread_rwlock_init( &sh->lock, &attr );
        pthread_rwlockattr_destroy( &attr );

        sh->init = initialise;
        sh->start = shell_start;
        sh->stop = shell_stop;
        sh->close = shell_close;

        sh->id = 0;
        sh->history.entry =
                ( CMD_HISTORY * ) malloc( CMD_HISTORY_LEN * sizeof( CMD_HISTORY ) );

        if( sh->history.entry == NULL ) {
                fprintf( stderr, "keep history data failed\n" );
                free( sh );
                return NULL;
        }

        sh->history.stack_pointer = sh->history.entry;

        for( i = 0; i < CMD_HISTORY_LEN; ++i )
                if( ( ( sh->history.entry + i )->cmd_buf =
                              malloc( CMD_BUFFER_LEN ) ) == NULL ) {
                        fprintf( stderr, "malloc failed\n" );
                        destroy_shell( ( SHELL * ) sh );
                } else
                        memset( ( sh->history.entry + i )->cmd_buf, '\0',
                                CMD_BUFFER_LEN );

        sh->history.cur_history_number = 0;
        sh->cmdobj = NULL;
        sh->cmdobj_num = 0;
        sh->prompt = NULL;
        atomic_set( &sh->flag_init, 0 );
        atomic_set( &sh->flag_start, 0 );
        signal( SHELL_STOP_SIGNAL, SIG_IGN );
        return ( SHELL * ) sh;
}

static SH_BOOL initialise( SHELL * this, CMD_OBJ cmd[], int n, char *prompt )
{
        /* if(sizeof(cmd) < sizeof(CMD_OBJ))
           {
           fprintf(stderr,"shell init failed: please pass CMD_OBJ array not pointer\n");
           return;
           } */
        SHELL_INTERNAL *shell = ( SHELL_INTERNAL * ) this;

        if( this == NULL )
                return SH_FALSE;

        pthread_rwlock_wrlock( &shell->lock );

        if( atomic_read( &shell->flag_init ) == 1
            || check_cmd_style( cmd, n ) == -1 ) {
                pthread_rwlock_unlock( &shell->lock );
                return SH_FALSE;	//已经初始化了 or cmd style is illegal
        }

        shell->cmdobj = ( CMD_OBJ * ) malloc( n * sizeof( CMD_OBJ ) );

        if( shell->cmdobj == NULL )
                return SH_FALSE;

        shell->cmdobj_num = n;

        while( n-- )
                *( shell->cmdobj + n ) = *( cmd + n );

        if( prompt != NULL ) {
                if( shell->prompt != NULL )
                        free( shell->prompt );

                /* shell->prompt = malloc( strlen( prompt ) + 1 ); */
                shell->prompt = strdup( prompt );

                if( shell->prompt == NULL )
                        return SH_FALSE;

                /* memcpy( shell->prompt, prompt, strlen( prompt ) + 1 ); */
        } else {
                if( shell->prompt != NULL )
                        free( shell->prompt );

                shell->prompt = NULL;
        }

        shell->history.cur_history_number = 0;
        atomic_set( &shell->flag_init, 1 );
        pthread_rwlock_unlock( &shell->lock );
        return SH_TRUE;
}

void destroy_shell( SHELL * shell )
{
        SHELL_INTERNAL *this = ( SHELL_INTERNAL * ) shell;
        int i;

        if( this != NULL ) {
                pthread_rwlock_wrlock( &this->lock );
                /* reset_keypress(); */
                this->stop( shell );

                if( this->history.entry != NULL ) {
                        for( i = 0; i < CMD_HISTORY_LEN; ++i )
                                if( ( this->history.entry + i )->cmd_buf != NULL )
                                        free( ( this->history.entry +
                                                i )->cmd_buf );

                        free( this->history.entry );
                }

                if( this->prompt != NULL )
                        free( this->prompt );

                free( this->cmdobj );
                pthread_rwlock_unlock( &this->lock );
                pthread_rwlock_destroy( &this->lock );
                free( this );
        }
}

void shell_close( SHELL * shell )
{
        SHELL_INTERNAL *this = ( SHELL_INTERNAL * ) shell;

        if( this != NULL ) {
                this->stop( shell );
                pthread_rwlock_wrlock( &this->lock );

                /* reset_keypress(); */
                if( this->prompt != NULL )
                        free( this->prompt );

                free( this->cmdobj );
                atomic_set( &this->flag_init, 0 );
                pthread_rwlock_unlock( &this->lock );
        }
}

static int command( SHELL_INTERNAL * this, char *cmdStr , int cmd_len )
{
        int i = 0, j = 0, flag = 0;
        int redirect_bak[2];
        struct flag cmd_flag = {0,  0,  0,  0,  0,  1};
        /* char *tmp_template = NULL, *tmp_file = NULL;
        int pipe_layer_var = 0, cnt = 0;
        int pid, pid2, fd1, fd2*/
        const char *option;
        char *next, *temp, *tmp, *buf = cmdStr;
        char *inter_ptr = NULL, *outer_ptr = NULL;
        CMD_TYPE type;

        if( this == NULL ) {
                fprintf( stderr, "command - SHELL OBJECT POINTER is NULL\n" );
                return -1;
        }

        if( cmdStr == NULL )
                return -1;

        cmdStr[cmd_len - 1] = '\0';
        strncpy( buf_bak, cmdStr, CMD_BUFFER_LEN );
        /*-----------------------------------------------------------------------------
         *  prase flag
         *-----------------------------------------------------------------------------*/
        /* process background run flag */
        tmp = strchr( cmdStr, '&' );

        if( tmp != NULL ) {
                cmd_flag.flag_background = 1;
                have_back_proc = 1;
                *tmp = '\0';
        }

        /* process in redirection */
        tmp = strchr( cmdStr, '<' );

        if( tmp != NULL ) {
                next = tmp;
                cmd_flag.flag_inredirect = 1;

                while( *++next != '\0' )
                        if( *next == ' ' )
                                continue;
                        else {
                                strncpy( infile, next, CMD_FILENAME_LEN );
                                infile[CMD_FILENAME_LEN - 1] = '\0';
                                break;
                        }

                *tmp = '\0';
                cmd_flag.fd_in = open( infile, O_RDONLY );	//输入重定向
        }

        /* process out redirection */
        tmp = strchr( cmdStr, '>' );

        if( tmp != NULL ) {
                next = tmp;
                cmd_flag.flag_outredirect = 1;

                while( *++next != '\0' )
                        if( *next == ' ' )
                                continue;
                        else {
                                strncpy( outfile, next, CMD_FILENAME_LEN );
                                outfile[CMD_FILENAME_LEN - 1] = '\0';
                                break;
                        }

                *tmp = '\0';
                cmd_flag.fd_out = open( outfile, O_RDWR | O_CREAT | O_TRUNC, 0644 );	//输出重定向
        }

        /* process pipe */
        /* tmp = strchr(cmdStr, '|');
           if(tmp != NULL)
           {
           flag_pipe = 1;
           next = tmp;
           buf_bak[next - cmdStr] = '\0';
           } */
        /*-----------------------------------------------------------------------------
         *  split up cmdStd
         *-----------------------------------------------------------------------------*/
        pipe_cnt[cmd_flag.flag_pipe] = 0;

        while( ( next = strtok_r( buf, "|", &outer_ptr ) ) != NULL ) {
                argv[j] = next;

                while( ( temp = strtok_r( next, " ", &inter_ptr ) ) != NULL ) {
                        argv[j++] = temp;
                        next = NULL;
                        flag = 1;
                }

                if( flag == 0 )
                        ++j;

                argv[j++] = NULL;
                pipe_cnt[++cmd_flag.flag_pipe] = j;	/*直接指向管道后的命令 */
                flag = 0;
                /* printf("flag %s\n",argv[pipe_cnt[flag_pipe -1]]); */
                buf = NULL;
        }

        pipe_cnt[cmd_flag.flag_pipe--] = -1;
        /* fprintf( stderr, "[%s]\n", argv[0] ); */
        i = find_command( this, argv[0], &type );

        switch( type ) {
                case INNER_CMD:	/* inner command */

                        if( j == 1 )
                                option = NULL;	// only contain cmdStr
                        /* else if( *( buf_bak + strlen( argv[0] ) + 1 ) == '-' )
                                option = buf_bak + strlen( argv[0] ) + 1;	// contain cmd parameter,too */
                        else {
                                option = buf_bak + strlen( argv[0] );

                                while( isspace( *option ) != 0 ) {
                                        option++;
                                }

                                if( *option == '\0' )
                                        option = NULL;
                        }

                        if( cmd_flag.flag_outredirect == 1 ) {
                                redirect_bak[1] = dup( STDOUT_FILENO );
                                dup2( cmd_flag.fd_out, STDOUT_FILENO );
                        } else
                                redirect_bak[1] = 0;

                        if( cmd_flag.flag_inredirect == 1 ) {
                                redirect_bak[0] = dup( STDIN_FILENO );
                                dup2( cmd_flag.fd_in, STDIN_FILENO );
                        } else
                                redirect_bak[0] = 0;

                        if( ( this->cmdobj + i )->CmdHandler != NULL )
                                ( this->cmdobj + i )->CmdHandler( this->cmdobj + i,
                                                                  option );

                        if( redirect_bak[0] != 0 )
                                dup2( redirect_bak[0], STDIN_FILENO );

                        if( redirect_bak[1] != 0 )
                                dup2( redirect_bak[1], STDOUT_FILENO );

                        return 0;
                case EXTERN_CMD:	/* extern command */
                        signal( SIGINT, SIG_IGN );
                        forkexec( &cmd_flag );
                        signal( SIGINT, SIG_DFL );
                        return 0;
                case UNKNOWN_CMD:
                default:
                        fprintf( stderr, "unknown command\n" );
                        return -1;
        }

        return 0;
}

static void shell_stop( SHELL * shell )
{
        SHELL_INTERNAL *this = ( SHELL_INTERNAL * ) shell;

        if( atomic_read( &this->flag_start ) == 0 )
                return;

        atomic_set( &this->flag_start, 0 );
        reset_keypress();

        /* if(stdin->_IO_read_ptr != NULL)
         *(stdin->_IO_read_ptr) = '\r'; */
        if( this->id != 0 ) {
                pthread_kill( this->id, SHELL_STOP_SIGNAL );
                this->id = 0;
        }
}

static void shell_start( SHELL * shell, SHELL_START_TYPE type )
{
        SHELL_INTERNAL *this = ( SHELL_INTERNAL * ) shell;
        pthread_attr_t attr;
        pthread_attr_init( &attr );

        if( atomic_read( &this->flag_init ) == 0 ) {
                fprintf( stderr, "shell have not be init, can not start\n" );
                return;
        }

        if( atomic_read( &this->flag_start ) == 0 )
                atomic_set( &this->flag_start, 1 );
        else
                goto SH_STRART;

        switch( type ) {
                case SHELL_START_UNBLOCK:
                        pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_DETACHED );

                        if( pthread_create( &this->id, &attr, shell_entry, ( void * )shell )
                            != 0 ) {
                                atomic_set( &this->flag_start, 0 );
                                goto SH_STRART;
                        }

                        break;
                case SHELL_START_BLOCK:

                        if( pthread_create( &this->id, &attr, shell_entry, ( void * )shell )
                            != 0 ) {
                                atomic_set( &this->flag_start, 0 );
                                goto SH_STRART;
                        }

                        pthread_join( this->id, NULL );
                        break;
                default:
                        break;
        }

SH_STRART:
        pthread_attr_destroy( &attr );
}

static void *shell_entry( void *shell )
{
        SHELL_INTERNAL * const this = ( SHELL_INTERNAL * const ) shell;

        if( shell == NULL ) {
                fprintf( stderr, "shell_entry - shell object pointer is NULL\n" );
                atomic_set( &this->flag_start, 0 );
                return NULL;
        }

        static char cmdbuf[CMD_BUFFER_LEN];
        /* if(setpgid(0,0) != 0)
           perror("setpgid failed"); */
        sigset_t sigmask;
        /*sigemptyset(&sigmask);
           sigaddset(&sigmask, SHELL_STOP_SIGNAL);
           pthread_sigmask(SIG_UNBLOCK,&sigmask,NULL); */
        sigfillset( &sigmask );
        sigdelset( &sigmask, SHELL_STOP_SIGNAL );
        pthread_sigmask( SIG_SETMASK, &sigmask, NULL );
        /* sigaddset(&sigmask, SIGINT);
           sigprocmask(SIG_UNBLOCK,&sigmask,NULL); */
        signal( SHELL_STOP_SIGNAL, catch_signal );
        /* signal(SIGHUP,SIG_IGN); */
        set_keypress();

        while( 1 ) {
                /* fprintf( stderr, "\rexecute one command abnormally\n" ); */
                if( !have_back_proc )
                        print_prompt( this );

                have_back_proc = 0;
                do_command_buf( this, cmdbuf, CMD_BUFFER_LEN );
                cmdbuf[CMD_BUFFER_LEN - 1] = '\0';

                if( strncmp( cmdbuf, "\0", 1 ) == 0 )
                        continue;

                command( this, cmdbuf , CMD_BUFFER_LEN );
        }

        reset_keypress();
        fprintf( stderr, "shell exit abnormally\n" );
        atomic_set( &this->flag_start, 0 );
        return NULL;
}

/**********************************helper***********************************/

static inline void do_command_buf( SHELL_INTERNAL * this, char *buf, int buf_len )
{
        CMD_HISTORY *tmp = this->history.stack_pointer;
        //      tmp->cmd_buf is current history command;
        //char ch;
        int ch;
        int i = 0, len = 0;// cnt = 0;
        memset( buf, '\0', buf_len );
        /* set_keypress(); */

        while( 1 ) {
                ch = getchar();
                /* fprintf( stderr, "\rdo with command_buf <%c : %d>\n", ch, ( int )ch ); */

                if( ch == EOF ) {
                        fprintf( stderr, "error : %m [%d]\n", strlen( buf ) );
                        /* reset_keypress();
                        set_keypress(); */
                        sleep( 1 );
                        continue;
                }

                if( ch == 27 ) {
                        getchar();
                        ch = getchar();

                        if( ch == 65 ) {	//key up and get cmd history
                                earse_line_memory( buf_len );
                                /* fprintf( stderr, "\rdo with [key_up]\n" ); */

                                if( i < this->history.cur_history_number ) {
                                        tmp =
                                                ( tmp ==
                                                  this->history.entry ) ? ( this->
                                                                            history.
                                                                            entry +
                                                                            CMD_HISTORY_LEN
                                                                            -
                                                                            1 ) : ( tmp -
                                                                                    1 );
                                        ++i;	// up numbers
                                        /*  if( *(tmp->cmd_buf) != '\0' )           // i == 5 means key down has beyond stack length */
                                        strncpy( buf, tmp->cmd_buf, buf_len );
                                        buf[buf_len - 1] = '\0';
                                        print_prompt( this );
                                        printf( "%s", buf );
                                        fflush( stdout );
                                } else {
                                        print_prompt( this );
                                        printf( "\a" );
                                }
                        } else if( ch == 66 ) {	//key down and get cmd history
                                earse_line_memory( buf_len );
                                /* fprintf( stderr, "\rdo with [key_down]\n" ); */

                                if( i > 0 ) {
                                        /*if( *(tmp->cmd_buf) != '\0' )             // i == 0 means key down has not data */
                                        strncpy( buf, tmp->cmd_buf, buf_len );
                                        buf[buf_len - 1] = '\0';
                                        print_prompt( this );
                                        printf( "%s", buf );
                                        fflush( stdout );
                                        tmp =
                                                ( tmp ==
                                                  ( this->history.entry +
                                                    CMD_HISTORY_LEN -
                                                    1 ) ) ? this->history.entry : ( tmp +
                                                                                    1 );
                                        --i;
                                } else {
                                        print_prompt( this );
                                        printf( "\a" );
                                }
                        }

                        //memset(buf, '\0', buf_len);
                        patch_flag = 1;
                } else if( ch == 127 ) {	//key backspace
                        tmp = this->history.stack_pointer;
                        i = 0;
                        len = strlen( buf );
                        /* fprintf( stderr, "\rdo with [backspace]\n" ); */

                        if( len == 0 )
                                printf( "\a" );
                        else {
                                buf[--len] = '\0';
                                earse_line_memory( buf_len );
                                print_prompt( this );
                                printf( "%s", buf );
                                fflush( stdout );
                        }
                } else if( ch == '\t' ) {	//tab match, 超过100的匹配，显示匹配项太多，不显示
                        /* fprintf( stderr, "\rdo with [tab]\n" ); */
                        if( patch_flag == 0 )
                                continue;

                        find_match( this, buf , buf_len );
                        /* ret=find_match(this, buf); */
                        //patch_flag = 1;
                        //printf("process match finished");
                        //patch_flag = 0;
                } else if( ch == 10 ) {	//enter
                        //strncat(buf, "\0", 1);
                        /* fprintf( stderr, "\rdo with [enter]\n" ); */
                        strncpy( this->history.stack_pointer->cmd_buf, buf,
                                 buf_len );
                        this->history.stack_pointer =
                                ( this->history.stack_pointer ==
                                  ( this->history.entry + CMD_HISTORY_LEN -
                                    1 ) ) ? this->history.entry : ( this->history.
                                                                    stack_pointer + 1 );
                        this->history.cur_history_number =
                                ( this->history.cur_history_number <
                                  CMD_HISTORY_LEN ) ? ( this->history.
                                                        cur_history_number +
                                                        1 ) : CMD_HISTORY_LEN;
                        printf( "\n" );
                        /* reset_keypress(); */
                        patch_flag = 0;
                        break;
                } else {	// common charactor
                        /* fprintf( stderr, "\rdo with [comm char]\n" ); */
                        tmp = this->history.stack_pointer;
                        i = 0;
                        len = strlen( buf );

                        if( len >= buf_len - 1 ) {
                                fprintf( stderr, "\rbeyond shell max_buf_len - < %d : %s> \n", len, buf );
                                continue;
                        }

                        //if (cnt++ == 0)
                        //	print_prompt(this);
                        //printf("%c", ch);
                        buf[len++] = ch;
                        buf[len] = '\0';
                        print_prompt( this );
                        printf( "%s", buf );
                        patch_flag = 1;
                }
        }
}

void set_keypress( void )
{
        struct termios new_settings;
        tcgetattr( 0, &old_settings );
        new_settings = old_settings;
        /* Disable canonical mode, and set buffer size to 1 byte */
        new_settings.c_lflag &= ( ~ICANON );
        new_settings.c_lflag &= ( ~ECHO );
        new_settings.c_cc[VTIME] = 0;
        new_settings.c_cc[VMIN] = 1;
        tcsetattr( 0, TCSANOW, &new_settings );
        return;
}

void set_nodelay_mode( void )
{
        int termflags;
        termflags = fcntl( 0, F_GETFL );
        termflags |= O_NDELAY;	//开启非阻塞模式，非阻塞就是ATM机不用一定得等待用户输入才继续运行下面的语句。这和以前的编程不一样
        fcntl( 0, F_SETFL, termflags );
}

void reset_keypress( void )
{
        tcsetattr( 0, TCSANOW, &old_settings );
        return;
}

inline static void print_prompt( SHELL_INTERNAL * this )
{
        if( this->prompt == NULL )
                printf( "\r" );
        else
                printf( "\r%s>> ", this->prompt );

        //patch_flag = 0;
}

static inline void earse_line_memory( int buf_len )
{
        int j = 0, earse_len = buf_len;
        printf( "\r" );
        get_window_size( &size );

        if( size.ws_col < buf_len )
                earse_len = size.ws_col;

        for( ; j < earse_len; ++j )	//erase line memory
                printf( " " );

        //patch_flag = 0;
}

inline void get_window_size( struct winsize *size )
{
        ioctl( STDIN_FILENO, TIOCGWINSZ, size );
}

static void catch_signal( int i )
{
        /* set_nodelay_mode(); */
        switch( i ) {
                case 63:
                        reset_keypress();
                        /* kill(getppid(),SIGHUP); */
                        /* killpg(0,SIGINT); */
                        signal( SIGINT, SIG_IGN );
                        killpg( 0, SIGINT );
                        signal( SHELL_STOP_SIGNAL, SIG_IGN );
                        signal( SIGINT, SIG_DFL );
                        pthread_exit( 0 );
                        break;
                case SIGHUP:
                        break;
                default:
                        break;
        }
}

SH_BOOL check_cmd_style( CMD_OBJ cmd[], int n )
{
        for( ; n; --n ) {
                if( cmd[n - 1].CmdHandler == NULL ) {
                        fprintf( stderr, "cmd string %d is not legal\n", n );
                        return SH_FALSE;
                }
        }

        return SH_TRUE;
}
