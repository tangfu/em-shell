/*
 * =====================================================================================
 *
 *       Filename:  test.c
 *
 *    Description:  shell库的单元测试
 *
 *        Version:  1.0
 *        Created:  2012年06月09日 08时48分27秒
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  YOUR NAME (tangfu),
 *        Company:  UESTC
 *
 * =====================================================================================
 */

#include <stdio.h>
#include "em-shell.h"
#include <stdarg.h>
#include <setjmp.h>
#include <cmockery.h>

SHELL *p;


void test( CMD_OBJ *cmd, const char *p )
{
        printf( "test\n" );
}

void test_macro( void **state )
{
        CMD_OBJ cmdx[2] = {{"test", test, NULL}, {"help", test, NULL}};
        CMD_OBJ cmdy[3] = {{"test", test, NULL}, {"help", NULL, NULL}, {"ok", test, NULL}};
        assert_int_equal( get_cmd_len( cmdx ), 2 );
        assert_int_equal( get_cmd_len( cmdy ), 3 );
}

void test_init( void **state )
{
        CMD_OBJ cmda[2] = {{"test", test, NULL}, {"help", test, NULL}};
        CMD_OBJ cmdb[3] = {{"test", test, NULL}, {"help", NULL, NULL}, {"ok", test, NULL}};
        assert_int_equal( p->init( p, cmdb , 3, NULL ), SH_FALSE );
        assert_int_equal( p->init( p, cmda , 2, NULL ), SH_TRUE );
        assert_int_equal( p->init( p, cmda, 2, NULL ), SH_FALSE );
}

int main( int argc, char *argv[] )
{
        p = create_shell();
        UnitTest TESTS[] = {
                unit_test( test_macro ),
                unit_test( test_init )
        };
        return run_tests( TESTS );
}
