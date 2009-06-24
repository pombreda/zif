/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*-
 *
 * Copyright (C) 2008 Richard Hughes <richard@hughsie.com>
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

/**
 * SECTION:zif-utils
 * @short_description: Simple utility functions useful to zif
 *
 * Common, non-object functions are declared here.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <rpm/rpmlib.h>
#include <rpm/rpmdb.h>
#include <archive.h>
#include <archive_entry.h>
#include <bzlib.h>
#include <zlib.h>
#include <packagekit-glib/packagekit.h>

#include "egg-debug.h"
#include "egg-string.h"

#include "zif-utils.h"
#include "zif-package.h"

/**
 * zif_init:
 *
 * This must be called before any of the zif_* functions are called.
 *
 * Return value: %TRUE if we initialised correctly
 **/
gboolean
zif_init (void)
{
	gint retval;

	retval = rpmReadConfigFiles (NULL, NULL);
	if (retval != 0) {
		egg_warning ("failed to read config files");
		return FALSE;
	}

	return TRUE;
}

/**
 * zif_boolean_from_text:
 * @text: the input text
 *
 * Convert a text boolean into it's enumerated boolean state
 *
 * Return value: %TRUE for positive, %FALSE for negative
 **/
gboolean
zif_boolean_from_text (const gchar *text)
{
	g_return_val_if_fail (text != NULL, FALSE);
	if (g_ascii_strcasecmp (text, "true") == 0 ||
	    g_ascii_strcasecmp (text, "yes") == 0 ||
	    g_ascii_strcasecmp (text, "1") == 0)
		return TRUE;
	return FALSE;
}

/**
 * zif_list_print_array:
 * @array: The string array to print
 *
 * Print an array of strings to %STDOUT.
 **/
void
zif_list_print_array (GPtrArray *array)
{
	guint i;
	ZifPackage *package;

	for (i=0;i<array->len;i++) {
		package = g_ptr_array_index (array, i);
		zif_package_print (package);
	}
}

/**
 * zif_package_id_from_header:
 * @name: The package name, e.g. "hal"
 * @epoch: The package epoch, e.g. "1" or %NULL
 * @version: The package version, e.g. "1.0.0"
 * @release: The package release, e.g. "2"
 * @arch: The package architecture, e.g. "i386"
 * @data: The package data, typically the repo name, or "installed"
 *
 * Formats a #PkPackageId structure from a NEVRA.
 *
 * Return value: The #PkPackageId value, or %NULL if invalid
 **/
PkPackageId *
zif_package_id_from_nevra (const gchar *name, const gchar *epoch, const gchar *version, const gchar *release, const gchar *arch, const gchar *data)
{
	gchar *version_compound;
	PkPackageId *id;

	/* do we include an epoch? */
	if (epoch == NULL || epoch[0] == '0')
		version_compound = g_strdup_printf ("%s-%s", version, release);
	else
		version_compound = g_strdup_printf ("%s:%s-%s", epoch, version, release);

	id = pk_package_id_new_from_list (name, version_compound, arch, data);
	g_free (version_compound);
	return id;
}

/**
 * zif_package_convert_evr:
 *
 * Modifies evr, so pass in copy
 **/
static gboolean
zif_package_convert_evr (gchar *evr, const gchar **epoch, const gchar **version, const gchar **release)
{
	gchar *find;

	g_return_val_if_fail (evr != NULL, FALSE);

	/* set to NULL initially */
	*version = NULL;

	/* split possible epoch */
	find = strstr (evr, ":");
	if (find != NULL) {
		*find = '\0';
		*epoch = evr;
		*version = find+1;
	} else {
		*epoch = NULL;
		*version = evr;
	}

	/* split possible release */
	find = g_strrstr (*version, "-");
	if (find != NULL) {
		*find = '\0';
		*release = find+1;
	} else {
		*release = NULL;
	}

	return TRUE;
}

/**
 * zif_compare_evr:
 * @a: the first version string
 * @b: the second version string
 *
 * Compare two [epoch:]version[-release] strings
 *
 * Return value: 1 for a>b, 0 for a==b, -1 for b>a
 **/
gint
zif_compare_evr (const gchar *a, const gchar *b)
{
	gint val = 0;
	gchar *ad = NULL;
	gchar *bd = NULL;
	const gchar *ae, *av, *ar;
	const gchar *be, *bv, *br;

	g_return_val_if_fail (a != NULL, 0);
	g_return_val_if_fail (b != NULL, 0);

	/* exactly the same, optimise */
	if (strcmp (a, b) == 0)
		goto out;

	/* copy */
	ad = g_strdup (a);
	bd = g_strdup (b);

	/* split */
	zif_package_convert_evr (ad, &ae, &av, &ar);
	zif_package_convert_evr (bd, &be, &bv, &br);

	/* compare epoch */
	if (ae != NULL && be != NULL)
		val = rpmvercmp (ae, be);
	else if (ae != NULL && atol (ae) > 0) {
		val = 1;
		goto out;
	} else if (be != NULL && atol (be) > 0) {
		val = -1;
		goto out;
	}

	/* compare version */
	val = rpmvercmp (av, bv);
	if (val != 0)
		goto out;

	/* compare release */
	if (ar != NULL && br != NULL)
		val = rpmvercmp (ar, br);

out:
	g_free (ad);
	g_free (bd);
	return val;
}

#define ZIF_BUFFER_SIZE 1024

/**
 * zif_file_decompress_zlib:
 **/
static gboolean
zif_file_decompress_zlib (const gchar *in, const gchar *out, GError **error)
{
	gboolean ret = FALSE;
	FILE *f_in = NULL;
	FILE *f_out = NULL;
	gint retval;
	guint have;
	z_stream strm;
	guchar buffer_in[ZIF_BUFFER_SIZE];
	guchar buffer_out[ZIF_BUFFER_SIZE];

	g_return_val_if_fail (in != NULL, FALSE);
	g_return_val_if_fail (out != NULL, FALSE);

	/* open file for reading */
	f_in = fopen (in, "r");
	if (f_in == NULL) {
		if (error != NULL)
			*error = g_error_new (1, 0, "cannot open %s for reading", in);
		goto out;
	}

	/* open file for writing */
	f_out = fopen (out, "w");
	if (f_out == NULL) {
		if (error != NULL)
			*error = g_error_new (1, 0, "cannot open %s for writing", out);
		goto out;
	}

	/* allocate inflate state */
	strm.zalloc = Z_NULL;
	strm.zfree = Z_NULL;
	strm.opaque = Z_NULL;
	strm.avail_in = 0;
	strm.next_in = Z_NULL;
	retval = inflateInit (&strm);
	if (retval != Z_OK) {
		if (error != NULL)
			*error = g_error_new (1, 0, "cannot initialize zlib");
		goto out;
	}

	/* decompress until deflate stream ends or end of file */
	do {
		strm.avail_in = fread (buffer_in, 1, ZIF_BUFFER_SIZE, f_in);
		if (ferror (f_in)) {
			if (error != NULL)
				*error = g_error_new (1, 0, "failed to inflate");
			goto out;
		}
egg_debug ("%i", strm.avail_in);
		/* no more to decompress */
		if (strm.avail_in == 0)
			break;
		strm.next_in = buffer_in;

		/* run inflate() on input until output buffer not full */
		do {
			strm.avail_out = ZIF_BUFFER_SIZE;
			strm.next_out = buffer_out;
			retval = inflate (&strm, Z_NO_FLUSH);
			switch (retval) {
			case Z_NEED_DICT:
				retval = Z_DATA_ERROR;	 /* and fall through */
			case Z_DATA_ERROR:
			case Z_MEM_ERROR:
				if (error != NULL)
					*error = g_error_new (1, 0, "out of memory (buffer %i bytes)", ZIF_BUFFER_SIZE);
				goto out;
			}
			have = ZIF_BUFFER_SIZE - strm.avail_out;
			if (fwrite (buffer_out, 1, have, f_out) != have || ferror (f_out)) {
				if (error != NULL)
					*error = g_error_new (1, 0, "failed to write");
				goto out;
			}
		} while (strm.avail_out == 0);

		/* done when inflate() says it's done */
	} while (retval != Z_STREAM_END);

	/* failed to read */
	if (retval != Z_STREAM_END) {
		if (error != NULL)
			*error = g_error_new (1, 0, "did not decompress file: %s", in);
		goto out;
	}

	/* success */
	ret = TRUE;
out:
	inflateEnd (&strm);
	if (f_in != NULL)
		fclose (f_in);
	if (f_out != NULL)
		fclose (f_out);
	return ret;
}

/**
 * zif_file_decompress_bz2:
 **/
static gboolean
zif_file_decompress_bz2 (const gchar *in, const gchar *out, GError **error)
{
	gboolean ret = FALSE;
	FILE *f_in = NULL;
	FILE *f_out = NULL;
	BZFILE *b = NULL;
	gint size;
	gint written;
	gchar buf[ZIF_BUFFER_SIZE];
	gint bzerror = BZ_OK;

	g_return_val_if_fail (in != NULL, FALSE);
	g_return_val_if_fail (out != NULL, FALSE);

	/* open file for reading */
	f_in = fopen (in, "r");
	if (f_in == NULL) {
		if (error != NULL)
			*error = g_error_new (1, 0, "cannot open %s for reading", in);
		goto out;
	}

	/* open file for writing */
	f_out = fopen (out, "w");
	if (f_out == NULL) {
		if (error != NULL)
			*error = g_error_new (1, 0, "cannot open %s for writing", out);
		goto out;
	}

	/* read in file */
	b = BZ2_bzReadOpen (&bzerror, f_in, 0, 0, NULL, 0);
	if (bzerror != BZ_OK) {
		if (error != NULL)
			*error = g_error_new (1, 0, "cannot open %s for bz2 reading", in);
		goto out;
	}

	/* read in all data in 1k chunks */
	while (TRUE) {
		/* read data */
		size = BZ2_bzRead (&bzerror, b, buf, 1024);
		if (bzerror != BZ_OK)
			break;

		/* write data */
		written = fwrite (buf, size, 1, f_out);
		if (written != size) {
			if (error != NULL)
				*error = g_error_new (1, 0, "failed to write %i/%i bytes", written, size);
			goto out;
		}
	}

	/* failed to read */
	if (bzerror != BZ_STREAM_END) {
		if (error != NULL)
			*error = g_error_new (1, 0, "did not decompress file: %s", in);
		goto out;
	}

	/* success */
	ret = TRUE;
out:
	if (b != NULL)
		BZ2_bzReadClose (&bzerror, b);
	if (f_in != NULL)
		fclose (f_in);
	if (f_out != NULL)
		fclose (f_out);
	return ret;
}

/**
 * zif_file_decompress:
 * @in: the filename to unpack
 * @out: the file to create
 * @error: a valid %GError
 *
 * Decompress files into a directory
 *
 * Return value: %TRUE if the file was decompressed
 **/
gboolean
zif_file_decompress (const gchar *in, const gchar *out, GError **error)
{
	gboolean ret = FALSE;

	g_return_val_if_fail (in != NULL, FALSE);
	g_return_val_if_fail (out != NULL, FALSE);

	/* bz2 */
	if (g_str_has_suffix (in, "bz2")) {
		ret = zif_file_decompress_bz2 (in, out, error);
		goto out;
	}

	/* zlib */
	if (g_str_has_suffix (in, "gz")) {
		ret = zif_file_decompress_zlib (in, out, error);
		goto out;
	}

	/* no support */
	if (error != NULL)
		*error = g_error_new (1, 0, "no support to decompress file: %s", in);
out:
	return ret;
}

/**
 * zif_file_untar:
 * @filename: the filename to unpack
 * @directory: the directory to unpack into
 * @error: a valid %GError
 *
 * Untar files into a directory
 *
 * Return value: %TRUE if the file was decompressed
 **/
gboolean
zif_file_untar (const gchar *filename, const gchar *directory, GError **error)
{
	gboolean ret = FALSE;
	struct archive *arch = NULL;
	struct archive_entry *entry;
	int r;
	int retval;
	gchar *retcwd;
	gchar buf[PATH_MAX];

	g_return_val_if_fail (filename != NULL, FALSE);
	g_return_val_if_fail (directory != NULL, FALSE);

	/* save the PWD as we chdir to extract */
	retcwd = getcwd (buf, PATH_MAX);
	if (retcwd == NULL) {
		*error = g_error_new (1, 0, "failed to get cwd");
		goto out;
	}

	/* we can only read tar achives */
	arch = archive_read_new ();
	archive_read_support_format_all (arch);
	archive_read_support_compression_all (arch);

	/* open the tar file */
	r = archive_read_open_file (arch, filename, 10240);
	if (r) {
		*error = g_error_new (1, 0, "cannot open: %s", archive_error_string (arch));
		goto out;
	}

	/* switch to our destination directory */
	retval = chdir (directory);
	if (retval != 0) {
		*error = g_error_new (1, 0, "failed chdir to %s", directory);
		goto out;
	}

	/* decompress each file */
	for (;;) {
		r = archive_read_next_header (arch, &entry);
		if (r == ARCHIVE_EOF)
			break;
		if (r != ARCHIVE_OK) {
			*error = g_error_new (1, 0, "cannot read header: %s", archive_error_string (arch));
			goto out;
		}
		r = archive_read_extract (arch, entry, 0);
		if (r != ARCHIVE_OK) {
			*error = g_error_new (1, 0, "cannot extract: %s", archive_error_string (arch));
			goto out;
		}
	}

	/* completed all okay */
	ret = TRUE;
out:
	/* close the archive */
	if (arch != NULL) {
		archive_read_close (arch);
		archive_read_finish (arch);
	}

	/* switch back to PWD */
	retval = chdir (buf);
	if (retval != 0)
		egg_warning ("cannot chdir back!");

	return ret;
}

/**
 * zif_file_uncompressed_name:
 * @filename: the filename, e.g. /lib/dave.tar.gz
 *
 * Finds the uncompressed filename.
 *
 * Return value: the uncompressed file name, e.g. /lib/dave.tar, use g_free() to free.
 **/
gchar *
zif_file_uncompressed_name (const gchar *filename)
{
	guint len;
	gchar *tmp;

	g_return_val_if_fail (filename != NULL, NULL);

	/* remove compression extension */
	tmp = g_strdup (filename);
	len = strlen (tmp);
	if (len > 4 && g_str_has_suffix (tmp, ".gz"))
		tmp[len-3] = '\0';
	else if (len > 5 && g_str_has_suffix (tmp, ".bz2"))
		tmp[len-4] = '\0';

	/* return newly allocated string */
	return tmp;
}

/***************************************************************************
 ***                          MAKE CHECK TESTS                           ***
 ***************************************************************************/
#ifdef EGG_TEST
#include "egg-test.h"

void
zif_utils_test (EggTest *test)
{
	PkPackageId *id;
	gchar *package_id;
	gboolean ret;
	gchar *evr;
	gint val;
	const gchar *e;
	const gchar *v;
	const gchar *r;
	gchar *filename;
	GError *error;

	if (!egg_test_start (test, "ZifUtils"))
		return;

	/************************************************************
	 ****************           NEVRA          ******************
	 ************************************************************/
	egg_test_title (test, "no epoch");
	id = zif_package_id_from_nevra ("kernel", NULL, "0.0.1", "1", "i386", "fedora");
	package_id = pk_package_id_to_string (id);
	if (egg_strequal (package_id, "kernel;0.0.1-1;i386;fedora"))
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "incorrect package_id '%s'", package_id);
	g_free (package_id);
	pk_package_id_free (id);

	/************************************************************/
	egg_test_title (test, "epoch zero");
	id = zif_package_id_from_nevra ("kernel", "0", "0.0.1", "1", "i386", "fedora");
	package_id = pk_package_id_to_string (id);
	if (egg_strequal (package_id, "kernel;0.0.1-1;i386;fedora"))
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "incorrect package_id '%s'", package_id);
	g_free (package_id);
	pk_package_id_free (id);

	/************************************************************/
	egg_test_title (test, "epoch value");
	id = zif_package_id_from_nevra ("kernel", "2", "0.0.1", "1", "i386", "fedora");
	package_id = pk_package_id_to_string (id);
	if (egg_strequal (package_id, "kernel;2:0.0.1-1;i386;fedora"))
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "incorrect package_id '%s'", package_id);
	g_free (package_id);
	pk_package_id_free (id);

	/************************************************************/
	egg_test_title (test, "init");
	ret = zif_init ();
	egg_test_assert (test, ret);

	/************************************************************/
	egg_test_title (test, "bool to text true (1)");
	ret = zif_boolean_from_text ("1");
	egg_test_assert (test, ret);

	/************************************************************/
	egg_test_title (test, "bool to text true (2)");
	ret = zif_boolean_from_text ("TRUE");
	egg_test_assert (test, ret);

	/************************************************************/
	egg_test_title (test, "bool to text false");
	ret = zif_boolean_from_text ("false");
	egg_test_assert (test, !ret);

	/************************************************************/
	egg_test_title (test, "bool to text blank");
	ret = zif_boolean_from_text ("");
	egg_test_assert (test, !ret);

	/************************************************************/
	egg_test_title (test, "convert evr");
	evr = g_strdup ("7:1.0.0-6");
	zif_package_convert_evr (evr, &e, &v, &r);
	if (egg_strequal (e, "7") && egg_strequal (v, "1.0.0") && egg_strequal (r, "6"))
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "incorrect evr '%s','%s','%s'", e, v, r);
	g_free (evr);

	/************************************************************/
	egg_test_title (test, "convert evr no epoch");
	evr = g_strdup ("1.0.0-6");
	zif_package_convert_evr (evr, &e, &v, &r);
	if (e == NULL && egg_strequal (v, "1.0.0") && egg_strequal (r, "6"))
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "incorrect evr '%s','%s','%s'", e, v, r);
	g_free (evr);

	/************************************************************/
	egg_test_title (test, "convert evr no epoch or release");
	evr = g_strdup ("1.0.0");
	zif_package_convert_evr (evr, &e, &v, &r);
	if (e == NULL && egg_strequal (v, "1.0.0") && r == NULL)
		egg_test_success (test, NULL);
	else
		egg_test_failed (test, "incorrect evr '%s','%s','%s'", e, v, r);
	g_free (evr);

	/************************************************************/
	egg_test_title (test, "compare same");
	val = zif_compare_evr ("1:1.0.2-3", "1:1.0.2-3");
	egg_test_assert (test, (val == 0));

	/************************************************************/
	egg_test_title (test, "compare right heavy");
	val = zif_compare_evr ("1:1.0.2-3", "1:1.0.2-4");
	egg_test_assert (test, (val == -1));

	/************************************************************/
	egg_test_title (test, "compare new release");
	val = zif_compare_evr ("1:1.0.2-4", "1:1.0.2-3");
	egg_test_assert (test, (val == 1));

	/************************************************************/
	egg_test_title (test, "compare new epoch");
	val = zif_compare_evr ("1:0.0.1-1", "1.0.2-2");
	egg_test_assert (test, (val == 1));

	/************************************************************/
	egg_test_title (test, "compare new version");
	val = zif_compare_evr ("1.0.2-1", "1.0.1-1");
	egg_test_assert (test, (val == 1));

	/************************************************************/
	egg_test_title (test, "get uncompressed name from compressed");
	filename = zif_file_uncompressed_name ("/dave/moo.sqlite.gz");
	egg_test_assert (test, (g_strcmp0 (filename, "/dave/moo.sqlite") == 0));
	g_free (filename);

	/************************************************************/
	egg_test_title (test, "get uncompressed name from uncompressed");
	filename = zif_file_uncompressed_name ("/dave/moo.sqlite");
	egg_test_assert (test, (g_strcmp0 (filename, "/dave/moo.sqlite") == 0));
	g_free (filename);

	/************************************************************/
	egg_test_title (test, "decompress gz");
	ret = zif_file_decompress ("../test/cache/fedora/cf940a26805152e5f675edd695022d890241aba057a4a4a97a0b46618a51c482-comps-rawhide.xml.gz", "/tmp/comps-rawhide.xml", &error);
	if (ret)
		egg_test_success (test, NULL);
	else {
		egg_test_failed (test, "failed: %s", error->message);
		g_error_free (error);
	}

	/************************************************************/
	egg_test_title (test, "decompress bz2");
	ret = zif_file_decompress ("../test/cache/fedora/35d817e2bac701525fa72cec57387a2e3457bf32642adeee1e345cc180044c86-primary.sqlite.bz2", "/tmp/moo.sqlite", &error);
	if (ret)
		egg_test_success (test, NULL);
	else {
		egg_test_failed (test, "failed: %s", error->message);
		g_error_free (error);
	}

	egg_test_end (test);
}
#endif

