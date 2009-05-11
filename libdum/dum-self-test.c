/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Richard Hughes <richard@hughsie.com>
 * Copyright (C) 2008 Shishir Goel <crazyontheedge@gmail.com>
 *
 * Licensed under the GNU General Public License Version 2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */

#include "config.h"

#include <glib.h>
#include <glib-object.h>
#include "egg-test.h"
#include "egg-debug.h"

/* prototypes */
void dum_monitor_test (EggTest *test);
void dum_config_test (EggTest *test);
void dum_utils_test (EggTest *test);
void dum_package_test (EggTest *test);
void dum_store_local_test (EggTest *test);
void dum_groups_test (EggTest *test);
void dum_store_remote_test (EggTest *test);
void dum_repo_md_master_test (EggTest *test);
void dum_repo_md_primary_test (EggTest *test);
void dum_repo_md_filelists_test (EggTest *test);
void dum_repos_test (EggTest *test);
void dum_sack_local_test (EggTest *test);
void dum_sack_remote_test (EggTest *test);
void dum_download_test (EggTest *test);
void dum_string_test (EggTest *test);
void dum_string_array_test (EggTest *test);

int
main (int argc, char **argv)
{
	EggTest *test;

	g_type_init ();
	g_thread_init (NULL);
	test = egg_test_init ();
	egg_debug_init (TRUE);

	/* tests go here */
	dum_repo_md_master_test (test);
	dum_repo_md_filelists_test (test);
	dum_repo_md_primary_test (test);
	dum_string_test (test);
	dum_string_array_test (test);
	dum_download_test (test);
	dum_monitor_test (test);
	dum_utils_test (test);
	dum_config_test (test);
	dum_package_test (test);
	dum_store_local_test (test);
	dum_groups_test (test);
	dum_store_remote_test (test);
	dum_repos_test (test);
	dum_sack_local_test (test);
	dum_sack_remote_test (test);
	
	return (egg_test_finish (test));
}

