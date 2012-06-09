#include "shell.h"
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>
#include <signal.h>


void test( CMD_OBJ *cmd, const char *p )
{
        if( p == NULL )
                printf( "test\n" );
        else
                printf( "test %s\n", p );
}

void *thread2( void *data )
{
        SHELL *sh = ( SHELL * )data;
        fprintf( stdout, "thread2\n" );
        sleep( 20 );
        sh->stop( sh );
        fprintf( stderr, "send stop command\n" );
        return NULL;
}

void *thread1( void *data )
{
        SHELL *sh = ( SHELL * )data;
        sh->start( sh, 0 );
        fprintf( stderr, "shell has already stop\n" );
        /* destroy_shell(sh); */
        return NULL;
}

int main()
{
        sigset_t sig_mask, mask;
        pthread_attr_t attr;
        sigfillset( &sig_mask );
        sigdelset( &sig_mask, SIGALRM );
        sigdelset( &sig_mask, SIGUSR1 );
        sigprocmask( SIG_SETMASK, &sig_mask, NULL );
        pthread_attr_init( &attr );
        pthread_attr_setdetachstate( &attr, PTHREAD_CREATE_DETACHED );
        SHELL *myshell;
        CMD_OBJ cmd[2] = {{"test", test, NULL}, {"help", test, NULL}};
        myshell = create_shell();
        myshell->init( myshell, cmd, get_cmd_len( cmd ), "myshell" );
        pthread_t id1, id2;
        pthread_create( &id1, NULL, thread1, ( void * )myshell );
        pthread_create( &id2, NULL, thread2, ( void * )myshell );
        sigemptyset( &mask );
        /* sigaddset(&mask,SIGINT); */
        /* sigaddset(&mask,SIGQUIT);
        sigaddset(&mask,SIGHUP);
        sigwait(&mask,&signo); */
        pthread_join( id1, NULL );
        pthread_join( id2, NULL );
        sleep( 2 );
        fprintf( stderr, "main exit\n" );
        destroy_shell( myshell );
        return 1;
}
