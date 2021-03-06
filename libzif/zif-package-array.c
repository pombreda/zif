/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2010 Richard Hughes <richard@hughsie.com>
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/**
 * SECTION:zif-package-array
 * @short_description: Arrays of packages
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <string.h>

#include "zif-package-array-private.h"
#include "zif-package-remote.h"
#include "zif-utils.h"

/**
 * zif_package_array_new:
 *
 * Return value: (element-type ZifPackage) (transfer container): A new #GPtrArray instance.
 *
 * Since: 0.1.3
 **/
GPtrArray *
zif_package_array_new (void)
{
	return g_ptr_array_new_with_free_func ((GDestroyNotify) g_object_unref);
}

/**
 * zif_package_array_find:
 * @array: (element-type ZifPackage): Array of %ZifPackage's
 * @package_id: A #GError, or %NULL
 * @error: A #GError, or %NULL
 *
 * Finds a package from an array.
 *
 * Return value: (transfer full): A single %ZifPackage, or %NULL in the case of an error.
 * The returned object should be freed with g_object_unref() when no
 * longer needed.
 *
 * Since: 0.2.1
 **/
ZifPackage *
zif_package_array_find (GPtrArray *array,
			const gchar *package_id,
			GError **error)
{
	guint i;
	ZifPackage *package = NULL;
	ZifPackage *package_tmp;

	g_return_val_if_fail (array != NULL, NULL);
	g_return_val_if_fail (package_id != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	for (i = 0; i < array->len; i++) {
		package_tmp = g_ptr_array_index (array, i);
		if (g_strcmp0 (zif_package_get_id (package_tmp), package_id) == 0) {
			package = g_object_ref (package_tmp);
			break;
		}
	}
	if (package == NULL) {
		g_set_error (error,
			     ZIF_PACKAGE_ERROR,
			     ZIF_PACKAGE_ERROR_FAILED,
			     "failed to find %s",
			     package_id);
	}
	return package;
}

/**
 * zif_package_array_get_newest:
 * @array: (element-type ZifPackage): Array of %ZifPackage's
 * @error: A #GError, or %NULL
 *
 * Returns the newest package from a list.
 * The package name is not used when calculating the newest package.
 *
 * Return value: (transfer full): A single %ZifPackage, or %NULL in the case of an error.
 * The returned object should be freed with g_object_unref() when no
 * longer needed.
 *
 * Since: 0.1.0
 **/
ZifPackage *
zif_package_array_get_newest (GPtrArray *array, GError **error)
{
	ZifPackage *package_newest;
	ZifPackage *package_tmp;
	ZifPackage *package = NULL;
	guint i;
	gint retval;

	g_return_val_if_fail (array != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* no results */
	if (array->len == 0) {
		g_set_error_literal (error,
				     ZIF_PACKAGE_ERROR,
				     ZIF_PACKAGE_ERROR_FAILED,
				     "nothing in array");
		goto out;
	}

	/* start with the first package being the newest */
	package_newest = g_ptr_array_index (array, 0);

	/* find newest in rest of the array */
	for (i=1; i < array->len; i++) {
		package_tmp = g_ptr_array_index (array, i);
		retval = zif_package_compare_full (package_tmp,
						   package_newest,
						   ZIF_PACKAGE_COMPARE_FLAG_CHECK_VERSION |
						   ZIF_PACKAGE_COMPARE_FLAG_CHECK_ARCH);
		/* if the packages are identical in every way, just
		 * choose the 1st package that was added to the array */
		if (retval == 0)
			continue;
		if (retval > 0)
			package_newest = package_tmp;
	}

	/* return reference so we can unref the list */
	package = g_object_ref (package_newest);
out:
	return package;
}

/**
 * zif_package_array_get_oldest:
 * @array: (element-type ZifPackage): Array of %ZifPackage's
 * @error: A #GError, or %NULL
 *
 * Returns the oldest package from a list.
 *
 * Return value: (transfer full): A single %ZifPackage, or %NULL in the case of an error.
 * The returned object should be freed with g_object_unref() when no
 * longer needed.
 *
 * Since: 0.1.3
 **/
ZifPackage *
zif_package_array_get_oldest (GPtrArray *array, GError **error)
{
	ZifPackage *package_oldest;
	ZifPackage *package = NULL;
	guint i;
	gint retval;

	g_return_val_if_fail (array != NULL, NULL);
	g_return_val_if_fail (error == NULL || *error == NULL, NULL);

	/* no results */
	if (array->len == 0) {
		g_set_error_literal (error, ZIF_PACKAGE_ERROR, ZIF_PACKAGE_ERROR_FAILED,
				     "nothing in array");
		goto out;
	}

	/* start with the first package being the oldest */
	package_oldest = g_ptr_array_index (array, 0);

	/* find oldest in rest of the array */
	for (i=1; i < array->len; i++) {
		package = g_ptr_array_index (array, i);
		retval = zif_package_compare (package, package_oldest);
		if (retval < 0)
			package_oldest = package;
	}

	/* return reference so we can unref the list */
	package = g_object_ref (package_oldest);
out:
	return package;
}

/**
 * zif_package_array_percentage_changed_cb:
 **/
static void
zif_package_array_percentage_changed_cb (ZifState *state,
					 guint percentage,
					 ZifPackage *package)
{
	/* update any UI */
	g_debug ("%s is DOWNLOADING @%i%%",
		 zif_package_get_id_basic (package),
		 percentage);
	zif_state_set_package_progress (state,
					zif_package_get_id_basic (package),
					ZIF_STATE_ACTION_DOWNLOADING,
					percentage);
}

/**
 * zif_package_array_download:
 * @packages: array of %ZifPackage's
 * @directory: A local directory to save to, or %NULL to use the package cache
 * @state: A #ZifState to use for progress reporting
 * @error: A #GError, or %NULL
 *
 * Downloads a list of packages.
 *
 * Return value: %TRUE for success, %FALSE otherwise
 *
 * Since: 0.2.5
 **/
gboolean
zif_package_array_download (GPtrArray *packages,
                            const gchar *directory,
                            ZifState *state,
                            GError **error)
{
	gboolean ret = TRUE;
	GError *error_local = NULL;
	guint i;
	guint percentage_id;
	ZifPackage *package;
	ZifState *state_loop;

	g_return_val_if_fail (packages != NULL, FALSE);
	g_return_val_if_fail (state != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	zif_state_set_number_steps (state, packages->len);
	for (i = 0; i < packages->len; i++) {
		package = g_ptr_array_index (packages, i);
		state_loop = zif_state_get_child (state);
		g_debug ("downloading %s",
			 zif_package_get_id (package));
		zif_state_action_start (state,
					ZIF_STATE_ACTION_DOWNLOADING,
					zif_package_get_id (package));
		percentage_id = g_signal_connect (state_loop, "percentage-changed",
						  G_CALLBACK (zif_package_array_percentage_changed_cb),
						  package);
		ret = zif_package_remote_download (ZIF_PACKAGE_REMOTE (package),
						   directory,
						   state_loop,
						   &error_local);
		g_signal_handler_disconnect (state_loop, percentage_id);
		if (!ret) {
			g_propagate_prefixed_error (error, error_local,
						    "cannot download %s: ",
						    zif_package_get_printable (package));
			goto out;
		}

		/* done */
		ret = zif_state_done (state, error);
		if (!ret)
			goto out;
	}
out:
	return ret;
}

/**
 * zif_package_array_filter_newest:
 * @packages: array of %ZifPackage's
 *
 * Filters the list so that only the newest version of a package remains.
 *
 * Return value: %TRUE if the array was modified
 *
 * Since: 0.1.0
 **/
gboolean
zif_package_array_filter_newest (GPtrArray *packages)
{
	const gchar *key;
	gboolean ret = FALSE;
	GHashTable *hash_keep;
	GHashTable *hash_namearch;
	gint retval;
	gpointer tmp;
	GPtrArray *array_new;
	guint i;
	ZifPackage *package;
	ZifPackage *package_tmp;

	g_return_val_if_fail (packages != NULL, FALSE);

	/* first, filter out any duplicates */
	zif_package_array_filter_duplicates (packages);

	/* use a hash so it's O(n) not O(n^2) */
	hash_namearch = g_hash_table_new_full (g_str_hash, g_str_equal,
					       g_free, g_object_unref);
	hash_keep = g_hash_table_new_full (g_str_hash, g_str_equal,
					   NULL, NULL);
	for (i = 0; i < packages->len; i++) {
		package = ZIF_PACKAGE (g_ptr_array_index (packages, i));
		key = zif_package_get_name (package);
		package_tmp = g_hash_table_lookup (hash_namearch, key);

		/* does not already exist */
		if (package_tmp == NULL) {
			g_hash_table_insert (hash_namearch,
					     g_strdup (key),
					     g_object_ref (package));
			g_hash_table_insert (hash_keep,
					     (gchar*)zif_package_get_id_basic (package),
					     (gchar*)"<keep>");
			continue;
		}

		/* the new package is the same */
		retval = zif_package_compare_full (package,
						   package_tmp,
						   ZIF_PACKAGE_COMPARE_FLAG_CHECK_ARCH |
						   ZIF_PACKAGE_COMPARE_FLAG_CHECK_VERSION |
						   ZIF_PACKAGE_COMPARE_FLAG_CHECK_INSTALLED |
						   ZIF_PACKAGE_COMPARE_FLAG_CHECK_DATA);
		if (retval == 0) {
			g_warning ("failed to compare %s : %s",
				   zif_package_get_id_basic (package),
				   zif_package_get_id_basic (package_tmp));
			continue;
		}

		/* the new package is older */
		if (retval < 0) {
			ret = TRUE;
			continue;
		}

		/* remove the old one */
		g_hash_table_remove (hash_namearch,
				     zif_package_get_name_arch (package_tmp));
		g_hash_table_insert (hash_namearch,
				     g_strdup (key),
				     g_object_ref (package));

		/* track packages */
		g_hash_table_remove (hash_keep,
				     zif_package_get_id_basic (package_tmp));
		g_hash_table_insert (hash_keep,
				     (gchar*)zif_package_get_id_basic (package),
				     (gchar*)"<keep>");
	}

	/* only add packages to array_new in hash_keep */
	array_new = g_ptr_array_sized_new (packages->len);
	for (i = 0; i < packages->len; i++) {
		package = ZIF_PACKAGE (g_ptr_array_index (packages, i));
		key = zif_package_get_id_basic (package);
		tmp = g_hash_table_lookup (hash_keep, key);
		if (tmp != NULL) {
			g_ptr_array_add (array_new,
					 g_object_ref (package));
		}
	}

	/* just copy the contents of the new array into the old array */
	g_ptr_array_set_size (packages, 0);
	for (i = 0; i < array_new->len; i++) {
		package = ZIF_PACKAGE (g_ptr_array_index (array_new, i));
		g_ptr_array_add (packages, package);
	}
	g_ptr_array_unref (array_new);
	g_hash_table_unref (hash_namearch);
	g_hash_table_unref (hash_keep);
	return ret;
}

/**
 * zif_package_array_filter_duplicates:
 * @packages: array of %ZifPackage's
 *
 * Filters the list for duplicates.
 *
 * Since: 0.2.1
 **/
void
zif_package_array_filter_duplicates (GPtrArray *packages)
{
	const gchar *key;
	GHashTable *hash_keep;
	gpointer tmp;
	guint i;
	ZifPackage *package;
	GPtrArray *array_new;

	g_return_if_fail (packages != NULL);

	/* use a hash so it's O(n) not O(n^2) */
	array_new = g_ptr_array_sized_new (packages->len);
	hash_keep = g_hash_table_new_full (g_str_hash, g_str_equal,
					   NULL, NULL);
	for (i = 0; i < packages->len; i++) {
		package = ZIF_PACKAGE (g_ptr_array_index (packages, i));
		key = zif_package_get_id_basic (package);
		tmp = g_hash_table_lookup (hash_keep, key);
		if (tmp == NULL) {
			g_hash_table_insert (hash_keep,
					     (gchar*) key,
					     (gchar*) "<keep>");
			g_ptr_array_add (array_new,
					 g_object_ref (package));
		}
	}

	/* just copy the contents of the new array into the old array */
	g_ptr_array_set_size (packages, 0);
	for (i = 0; i < array_new->len; i++) {
		package = ZIF_PACKAGE (g_ptr_array_index (array_new, i));
		g_ptr_array_add (packages, package);
	}
	g_ptr_array_unref (array_new);
	g_hash_table_unref (hash_keep);
}

/**
 * zif_package_array_filter_best_arch32:
 * @array: (element-type ZifPackage): Array of %ZifPackage's
 *
 * Filters the array so that only the best version of a package remains.
 **/
static void
zif_package_array_filter_best_arch32 (GPtrArray *array)
{
	ZifPackage *package;
	guint i;
	const gchar *arch;
	const gchar *best_arch = NULL;

	/* find the best arch */
	for (i = 0; i < array->len; i++) {
		package = g_ptr_array_index (array, i);
		arch = zif_package_get_arch (package);
		if (g_strcmp0 (arch, "x86_64") == 0 ||
		    g_strcmp0 (arch, "noarch") == 0)
			continue;
		if (g_strcmp0 (arch, best_arch) > 0) {
			best_arch = arch;
		}
	}

	/* if no obvious best, skip */
	g_debug ("best 32 bit arch=%s", best_arch);
	if (best_arch == NULL) {
		zif_package_array_filter_arch (array, "noarch");
		return;
	}

	/* remove any that are not best */
	for (i = 0; i < array->len;) {
		package = g_ptr_array_index (array, i);
		arch = zif_package_get_arch (package);
		if (g_strcmp0 (arch, best_arch) != 0 &&
		    g_strcmp0 (arch, "noarch") != 0) {
			g_ptr_array_remove_index_fast (array, i);
			continue;
		}

		/* not compatible with i386 */
		if (g_strcmp0 (arch, "x86_64") == 0) {
			g_ptr_array_remove_index_fast (array, i);
			continue;
		}
		i++;
	}
}

/**
 * zif_package_array_filter_best_arch:
 * @array: (element-type ZifPackage): Array of %ZifPackage's
 *
 * Filters the array so that only the best version of a package remains.
 *
 * If we have the following packages:
 *  - glibc.i386
 *  - hal.i386
 *  - glibc.i686
 *
 * Then we output:
 *  - glibc.i686
 *
 * Since: 0.2.1
 **/
void
zif_package_array_filter_best_arch (GPtrArray *array, const gchar *arch)
{
	g_return_if_fail (array != NULL);
	g_return_if_fail (arch != NULL);

	/* only x86_64 can be installed on x86_64 */
	if (g_strcmp0 (arch, "x86_64") == 0) {
		zif_package_array_filter_arch (array, arch);
		return;
	}

	/* just filter to the best 32 bit arch */
	zif_package_array_filter_best_arch32 (array);
}

/**
 * zif_package_array_filter_arch:
 * @array: (element-type ZifPackage): Array of %ZifPackage's
 * @arch: architecture string, e.g. "i486"
 *
 * Filters the array so that only the matching arch of a package remains.
 *
 * Since: 0.1.3
 **/
void
zif_package_array_filter_arch (GPtrArray *array, const gchar *arch)
{
	const gchar *arch_tmp;
	guint i;
	ZifPackage *package;

	/* remove any that are not best */
	for (i = 0; i < array->len;) {
		package = g_ptr_array_index (array, i);
		arch_tmp = zif_package_get_arch (package);
		if (g_strcmp0 (arch_tmp, "noarch") == 0) {
			i++;
			continue;
		}
		if (!zif_arch_is_native (arch, arch_tmp)) {
			g_ptr_array_remove_index_fast (array, i);
			continue;
		}
		i++;
	}
}

/**
 * zif_package_array_filter_provide:
 * @array: (element-type ZifPackage): Array of %ZifPackage's
 * @depends: (element-type ZifDepend): an array of #ZifDepend's
 * @state: A #ZifState to use for progress reporting
 * @error: A #GError, or %NULL
 *
 * Filters the list by the dependency satisfiability.
 *
 * Return value: %TRUE if the array was searched successfully
 *
 * Since: 0.1.3
 **/
gboolean
zif_package_array_filter_provide (GPtrArray *array,
				  GPtrArray *depends,
				  ZifState *state,
				  GError **error)
{
	guint i, j;
	gboolean ret = TRUE;
	ZifPackage *package;
	ZifDepend *satisfies = NULL;
	ZifDepend *depend_tmp;
	ZifState *state_local;
	ZifState *state_loop;

	g_return_val_if_fail (array != NULL, FALSE);
	g_return_val_if_fail (depends != NULL, FALSE);
	g_return_val_if_fail (state != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* remove entries that do not satisfy the dep */
	zif_state_set_number_steps (state, array->len);
	for (i = 0; i < array->len;) {
		package = g_ptr_array_index (array, i);
		state_local = zif_state_get_child (state);
		zif_state_set_number_steps (state_local, depends->len);

		/* try each depend as 'OR' */
		for (j = 0; j < depends->len; j++) {
			depend_tmp = g_ptr_array_index (depends, j);
			state_loop = zif_state_get_child (state_local);
			ret = zif_package_provides (package,
						    depend_tmp,
						    &satisfies,
						    state_loop,
						    error);
			if (!ret)
				goto out;
			if (satisfies != NULL) {
				ret = zif_state_finished (state_local, error);
				if (!ret)
					goto out;
				break;
			}

			/* done */
			ret = zif_state_done (state_local, error);
			if (!ret)
				goto out;
		}
		ret = zif_state_done (state, error);
		if (!ret)
			goto out;
		if (satisfies == NULL) {
			g_ptr_array_remove_index_fast (array, i);
			continue;
		}
		g_object_unref (satisfies);
		i++;
	}
out:
	return ret;
}

/**
 * zif_package_array_filter_require:
 * @array: (element-type ZifPackage): Array of %ZifPackage's
 * @depends: (element-type ZifDepend): an array of #ZifDepend's
 * @state: A #ZifState to use for progress reporting
 * @error: A #GError, or %NULL
 *
 * Filters the list by the dependency satisfiability.
 *
 * Return value: %TRUE if the array was searched successfully
 *
 * Since: 0.1.3
 **/
gboolean
zif_package_array_filter_require (GPtrArray *array,
				  GPtrArray *depends,
				  ZifState *state,
				  GError **error)
{
	guint i, j;
	gboolean ret = TRUE;
	ZifPackage *package;
	ZifDepend *satisfies = NULL;
	ZifDepend *depend_tmp;
	ZifState *state_local;
	ZifState *state_loop;

	g_return_val_if_fail (array != NULL, FALSE);
	g_return_val_if_fail (depends != NULL, FALSE);
	g_return_val_if_fail (state != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* remove entries that do not satisfy the dep */
	zif_state_set_number_steps (state, array->len);
	for (i = 0; i < array->len;) {
		package = g_ptr_array_index (array, i);
		state_local = zif_state_get_child (state);
		zif_state_set_number_steps (state_local, depends->len);

		/* try each depend as 'OR' */
		for (j = 0; j < depends->len; j++) {
			depend_tmp = g_ptr_array_index (depends, j);
			state_loop = zif_state_get_child (state_local);
			ret = zif_package_requires (package,
						    depend_tmp,
						    &satisfies,
						    state_loop,
						    error);
			if (!ret)
				goto out;
			if (satisfies != NULL) {
				ret = zif_state_finished (state_local, error);
				if (!ret)
					goto out;
				break;
			}

			/* done */
			ret = zif_state_done (state_local, error);
			if (!ret)
				goto out;
		}
		ret = zif_state_done (state, error);
		if (!ret)
			goto out;
		if (satisfies == NULL) {
			g_ptr_array_remove_index_fast (array, i);
			continue;
		}
		g_object_unref (satisfies);
		i++;
	}
out:
	return ret;
}

/**
 * zif_package_array_filter_conflict:
 * @array: (element-type ZifPackage): Array of %ZifPackage's
 * @depends: (element-type ZifDepend): an array of #ZifDepend's
 * @state: A #ZifState to use for progress reporting
 * @error: A #GError, or %NULL
 *
 * Filters the list by the dependency satisfiability.
 *
 * Return value: %TRUE if the array was searched successfully
 *
 * Since: 0.1.3
 **/
gboolean
zif_package_array_filter_conflict (GPtrArray *array,
				   GPtrArray *depends,
				   ZifState *state,
				   GError **error)
{
	guint i, j;
	gboolean ret = TRUE;
	ZifPackage *package;
	ZifDepend *satisfies = NULL;
	ZifDepend *depend_tmp;
	ZifState *state_local;
	ZifState *state_loop;

	g_return_val_if_fail (array != NULL, FALSE);
	g_return_val_if_fail (depends != NULL, FALSE);
	g_return_val_if_fail (state != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* remove entries that do not satisfy the dep */
	zif_state_set_number_steps (state, array->len);
	for (i = 0; i < array->len;) {
		package = g_ptr_array_index (array, i);
		state_local = zif_state_get_child (state);
		zif_state_set_number_steps (state_local, depends->len);

		/* try each depend as 'OR' */
		for (j = 0; j < depends->len; j++) {
			depend_tmp = g_ptr_array_index (depends, j);
			state_loop = zif_state_get_child (state_local);
			ret = zif_package_conflicts (package,
						     depend_tmp,
						     &satisfies,
						     state_loop,
						     error);
			if (!ret)
				goto out;
			if (satisfies != NULL) {
				ret = zif_state_finished (state_local, error);
				if (!ret)
					goto out;
				break;
			}

			/* done */
			ret = zif_state_done (state_local, error);
			if (!ret)
				goto out;
		}
		ret = zif_state_done (state, error);
		if (!ret)
			goto out;
		if (satisfies == NULL) {
			g_ptr_array_remove_index_fast (array, i);
			continue;
		}
		g_object_unref (satisfies);
		i++;
	}
out:
	return ret;
}

/**
 * zif_package_array_filter_obsolete:
 * @array: (element-type ZifPackage): Array of %ZifPackage's
 * @depends: (element-type ZifDepend): an array of #ZifDepend's
 * @state: A #ZifState to use for progress reporting
 * @error: A #GError, or %NULL
 *
 * Filters the list by the dependency satisfiability.
 *
 * Return value: %TRUE if the array was searched successfully
 *
 * Since: 0.1.3
 **/
gboolean
zif_package_array_filter_obsolete (GPtrArray *array,
				   GPtrArray *depends,
				   ZifState *state,
				   GError **error)
{
	guint i, j;
	gboolean ret = TRUE;
	ZifPackage *package;
	ZifDepend *satisfies = NULL;
	ZifDepend *depend_tmp;
	ZifState *state_local;
	ZifState *state_loop;

	g_return_val_if_fail (array != NULL, FALSE);
	g_return_val_if_fail (depends != NULL, FALSE);
	g_return_val_if_fail (state != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* remove entries that do not satisfy the dep */
	zif_state_set_number_steps (state, array->len);
	for (i = 0; i < array->len;) {
		package = g_ptr_array_index (array, i);
		state_local = zif_state_get_child (state);
		zif_state_set_number_steps (state_local, depends->len);

		/* try each depend as 'OR' */
		for (j = 0; j < depends->len; j++) {
			depend_tmp = g_ptr_array_index (depends, j);
			state_loop = zif_state_get_child (state_local);
			ret = zif_package_obsoletes (package,
						     depend_tmp,
						     &satisfies,
						     state_loop,
						     error);
			if (!ret)
				goto out;
			if (satisfies != NULL) {
				ret = zif_state_finished (state_local, error);
				if (!ret)
					goto out;
				break;
			}

			/* done */
			ret = zif_state_done (state_local, error);
			if (!ret)
				goto out;
		}
		ret = zif_state_done (state, error);
		if (!ret)
			goto out;
		if (satisfies == NULL) {
			g_ptr_array_remove_index_fast (array, i);
			continue;
		}
		g_object_unref (satisfies);
		i++;
	}
out:
	return ret;
}

/**
 * zif_package_array_depend:
 **/
static gboolean
zif_package_array_depend (GPtrArray *array,
			   ZifDepend *depend,
			   ZifDepend **best_depend,
			   GPtrArray **provides,
			   ZifPackageEnsureType type,
			   ZifState *state,
			   GError **error)
{
	gboolean ret = TRUE;
	guint i;
	ZifPackage *package_tmp;
	ZifDepend *satisfies = NULL;
	ZifDepend *best_depend_tmp = NULL;

	g_return_val_if_fail (array != NULL, FALSE);
	g_return_val_if_fail (depend != NULL, FALSE);
	g_return_val_if_fail (state != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* create results array */
	if (provides != NULL)
		*provides = zif_package_array_new ();

	/* interate through the array */
	for (i = 0; i < array->len; i++) {
		package_tmp = g_ptr_array_index (array, i);

		/* does this match */
		switch (type) {
		case ZIF_PACKAGE_ENSURE_TYPE_PROVIDES:
			ret = zif_package_provides (package_tmp,
						    depend,
						    &satisfies,
						    state,
						    error);
			break;
		case ZIF_PACKAGE_ENSURE_TYPE_REQUIRES:
			ret = zif_package_requires (package_tmp,
						    depend,
						    &satisfies,
						    state,
						    error);
			break;
		case ZIF_PACKAGE_ENSURE_TYPE_CONFLICTS:
			ret = zif_package_conflicts (package_tmp,
						    depend,
						    &satisfies,
						    state,
						    error);
			break;
		case ZIF_PACKAGE_ENSURE_TYPE_OBSOLETES:
			ret = zif_package_obsoletes (package_tmp,
						    depend,
						    &satisfies,
						    state,
						    error);
			break;
		default:
			g_assert_not_reached ();
		}
		if (!ret)
			goto out;

		/* gotcha, but keep looking */
		if (satisfies != NULL) {
			if (provides != NULL)
				g_ptr_array_add (*provides, g_object_ref (package_tmp));

			/* ensure we track the best depend */
			if (best_depend != NULL &&
			    (best_depend_tmp == NULL ||
			     zif_depend_compare (satisfies, best_depend_tmp) > 0)) {
				if (best_depend_tmp != NULL)
					g_object_unref (best_depend_tmp);
				best_depend_tmp = g_object_ref (satisfies);
			}

			g_object_unref (satisfies);
		}
	}

	/* if we supplied an address for it, return it */
	if (best_depend != NULL) {
		if (best_depend_tmp == NULL) {
			*best_depend = NULL;
		} else {
			*best_depend = g_object_ref (best_depend_tmp);
		}
	}
out:
	if (best_depend_tmp != NULL)
		g_object_unref (best_depend_tmp);
	return ret;
}

/**
 * zif_package_array_provide:
 * @array: (element-type ZifPackage): Array of %ZifPackage's
 * @depend: A dependency to try and satisfy
 * @best_depend: The best matched dependency if not %NULL
 * @results: A matched dependencies if not %NULL
 * @state: A #ZifState to use for progress reporting
 * @error: A #GError, or %NULL
 *
 * Gets the package dependencies that satisfy the supplied dependency.
 *
 * Return value: %TRUE if the array was searched.
 * Use @results->len == 0 to detect a missing dependency.
 *
 * Since: 0.1.3
 **/
gboolean
zif_package_array_provide (GPtrArray *array,
			   ZifDepend *depend,
			   ZifDepend **best_depend,
			   GPtrArray **results,
			   ZifState *state,
			   GError **error)
{
	return zif_package_array_depend (array,
					 depend,
					 best_depend,
					 results,
					 ZIF_PACKAGE_ENSURE_TYPE_PROVIDES,
					 state,
					 error);
}

/**
 * zif_package_array_require:
 * @array: (element-type ZifPackage): Array of %ZifPackage's
 * @depend: A dependency to try and satisfy
 * @best_depend: The best matched dependency if not %NULL
 * @results: A matched dependencies if not %NULL
 * @state: A #ZifState to use for progress reporting
 * @error: A #GError, or %NULL
 *
 * Gets the package dependencies that satisfy the supplied dependency.
 *
 * Return value: %TRUE if the array was searched.
 * Use @results->len == 0 to detect a missing dependency.
 *
 * Since: 0.1.3
 **/
gboolean
zif_package_array_require (GPtrArray *array,
			   ZifDepend *depend,
			   ZifDepend **best_depend,
			   GPtrArray **results,
			   ZifState *state,
			   GError **error)
{
	return zif_package_array_depend (array,
					 depend,
					 best_depend,
					 results,
					 ZIF_PACKAGE_ENSURE_TYPE_REQUIRES,
					 state,
					 error);
}

/**
 * zif_package_array_conflict:
 * @array: (element-type ZifPackage): Array of %ZifPackage's
 * @depend: A dependency to try and satisfy
 * @best_depend: The best matched dependency if not %NULL
 * @results: A matched dependencies if not %NULL
 * @state: A #ZifState to use for progress reporting
 * @error: A #GError, or %NULL
 *
 * Gets the package dependencies that satisfy the supplied dependency.
 *
 * Return value: %TRUE if the array was searched.
 * Use @results->len == 0 to detect a missing dependency.
 *
 * Since: 0.1.3
 **/
gboolean
zif_package_array_conflict (GPtrArray *array,
			    ZifDepend *depend,
			    ZifDepend **best_depend,
			    GPtrArray **results,
			    ZifState *state,
			    GError **error)
{
	return zif_package_array_depend (array,
					 depend,
					 best_depend,
					 results,
					 ZIF_PACKAGE_ENSURE_TYPE_CONFLICTS,
					 state,
					 error);
}

/**
 * zif_package_array_obsolete:
 * @array: (element-type ZifPackage): Array of %ZifPackage's
 * @depend: A dependency to try and satisfy
 * @best_depend: The best matched dependency if not %NULL
 * @results: A matched dependencies if not %NULL
 * @state: A #ZifState to use for progress reporting
 * @error: A #GError, or %NULL
 *
 * Gets the package dependencies that satisfy the supplied dependency.
 *
 * Return value: %TRUE if the array was searched.
 * Use @results->len == 0 to detect a missing dependency.
 *
 * Since: 0.1.3
 **/
gboolean
zif_package_array_obsolete (GPtrArray *array,
			    ZifDepend *depend,
			    ZifDepend **best_depend,
			    GPtrArray **results,
			    ZifState *state,
			    GError **error)
{
	return zif_package_array_depend (array,
					 depend,
					 best_depend,
					 results,
					 ZIF_PACKAGE_ENSURE_TYPE_OBSOLETES,
					 state,
					 error);
}
