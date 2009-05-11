/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2009 Richard Hughes <richard@hughsie.com>
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

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>

#include "egg-debug.h"
#include "dum-depend-array.h"

/**
 * dum_depend_array_new:
 **/
DumDependArray *
dum_depend_array_new (const GPtrArray *value)
{
	guint i;
	DumDependArray *array;

	array = g_new0 (DumDependArray, 1);
	array->count = 1;
	array->value = g_ptr_array_new ();

	/* nothing to copy */
	if (value == NULL)
		goto out;

	/* copy */
	for (i=0; i<value->len; i++)
		g_ptr_array_add (array->value, dum_depend_ref (g_ptr_array_index (value, i)));
out:
	return array;
}

/**
 * dum_depend_array_ref:
 **/
DumDependArray *
dum_depend_array_ref (DumDependArray *array)
{
	g_return_val_if_fail (array != NULL, NULL);
	array->count++;
	return array;
}

/**
 * dum_depend_array_unref:
 **/
DumDependArray *
dum_depend_array_unref (DumDependArray *array)
{
	if (array == NULL)
		return NULL;
	array->count--;
	if (array->count == 0) {
		g_ptr_array_foreach (array->value, (GFunc) dum_depend_unref, NULL);
		g_ptr_array_free (array->value, TRUE);
		g_free (array);
		array = NULL;
	}
	return array;
}

/**
 * dum_depend_array_add:
 **/
void
dum_depend_array_add (DumDependArray *array, DumDepend *depend)
{
	g_return_if_fail (array != NULL);
	g_ptr_array_add (array->value, dum_depend_ref (depend));
}

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef EGG_TEST
#include "egg-test.h"

void
dum_depend_array_test (EggTest *test)
{
	DumDependArray *array;

	if (!egg_test_start (test, "DumDependArray"))
		return;

	/************************************************************/
	egg_test_title (test, "create");
	array = dum_depend_array_new (NULL);
	if (array->value->len == 0 && array->count == 1)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "incorrect value %s:%i", array->value, array->count);

	/************************************************************/
	egg_test_title (test, "ref");
	dum_depend_array_ref (array);
	egg_test_assert (test, array->count == 2);

	/************************************************************/
	egg_test_title (test, "unref");
	dum_depend_array_unref (array);
	egg_test_assert (test, array->count == 1);

	/************************************************************/
	egg_test_title (test, "unref");
	array = dum_depend_array_unref (array);
	egg_test_assert (test, array == NULL);

	egg_test_end (test);
}
#endif

