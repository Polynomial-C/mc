/*
   lib/vfs - manipulations with temp files and  dirs

   Copyright (C) 2012-2025
   Free Software Foundation, Inc.

   Written by:
   Slava Zanko <slavazanko@gmail.com>, 2012, 2013

   This file is part of the Midnight Commander.

   The Midnight Commander is free software: you can redistribute it
   and/or modify it under the terms of the GNU General Public License as
   published by the Free Software Foundation, either version 3 of the License,
   or (at your option) any later version.

   The Midnight Commander is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#define TEST_SUITE_NAME "/lib/vfs"

#include "tests/mctest.h"

#include "lib/charsets.h"
#include "lib/strutil.h"
#include "lib/vfs/xdirentry.h"
#include "lib/vfs/path.h"

#include "src/vfs/local/local.c"

/* --------------------------------------------------------------------------------------------- */

/* @Before */
static void
setup (void)
{
    // Ensure that tests behave consistently irrespectively of the environment
    g_unsetenv ("MC_TMPDIR");

    str_init_strings (NULL);

    vfs_init ();
    vfs_init_localfs ();
    vfs_setup_work_dir ();
}

/* --------------------------------------------------------------------------------------------- */

/* @After */
static void
teardown (void)
{
    vfs_shut ();
    str_uninit_strings ();
}

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_mc_tmpdir)
{
    // given
    const char *tmpdir;
    const char *env_tmpdir;

    // when
    tmpdir = mc_tmpdir ();
    env_tmpdir = g_getenv ("MC_TMPDIR");

    // then
    ck_assert_msg (g_file_test (tmpdir, G_FILE_TEST_EXISTS)
                       && g_file_test (tmpdir, G_FILE_TEST_IS_DIR),
                   "\nNo such directory: %s\n", tmpdir);
    mctest_assert_str_eq (env_tmpdir, tmpdir);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

/* @Test */
START_TEST (test_mc_mkstemps)
{
    // given
    vfs_path_t *pname_vpath = NULL;
    char *begin_pname;
    int fd;

    // when
    fd = mc_mkstemps (&pname_vpath, "mctest-", NULL);
    begin_pname = g_build_filename (mc_tmpdir (), "mctest-", (char *) NULL);

    // then
    close (fd);
    ck_assert_int_ne (fd, -1);

    const char *pname = vfs_path_as_str (pname_vpath);

    ck_assert_msg (g_file_test (pname, G_FILE_TEST_EXISTS)
                       && g_file_test (pname, G_FILE_TEST_IS_REGULAR),
                   "\nNo such file: %s\n", pname);
    unlink (pname);
    ck_assert_msg (strncmp (pname, begin_pname, strlen (begin_pname)) == 0,
                   "\nstart of %s should be equal to %s\n", pname, begin_pname);
    vfs_path_free (pname_vpath, TRUE);
}
END_TEST

/* --------------------------------------------------------------------------------------------- */

int
main (void)
{
    TCase *tc_core;

    tc_core = tcase_create ("Core");

    tcase_add_checked_fixture (tc_core, setup, teardown);

    // Add new tests here: ***************
    tcase_add_test (tc_core, test_mc_tmpdir);
    tcase_add_test (tc_core, test_mc_mkstemps);
    // ***********************************

    return mctest_run_all (tc_core);
}

/* --------------------------------------------------------------------------------------------- */
