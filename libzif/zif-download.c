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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

/**
 * SECTION:zif-download
 * @short_description: Generic object to download packages.
 *
 * This object is a trivial wrapper around libsoup.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <glib.h>
#include <libsoup/soup.h>

#include "zif-config.h"
#include "zif-download.h"
#include "zif-state.h"

#define ZIF_DOWNLOAD_GET_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), ZIF_TYPE_DOWNLOAD, ZifDownloadPrivate))

/**
 * ZifDownloadPrivate:
 *
 * Private #ZifDownload data
 **/
struct _ZifDownloadPrivate
{
	gchar			*proxy;
	SoupSession		*session;
	SoupMessage		*msg;
	ZifState		*state;
	ZifConfig		*config;
};

static gpointer zif_download_object = NULL;

G_DEFINE_TYPE (ZifDownload, zif_download, G_TYPE_OBJECT)

/**
 * zif_download_error_quark:
 *
 * Return value: Our personal error quark.
 *
 * Since: 0.1.0
 **/
GQuark
zif_download_error_quark (void)
{
	static GQuark quark = 0;
	if (!quark)
		quark = g_quark_from_static_string ("zif_download_error");
	return quark;
}

/**
 * zif_download_file_got_chunk_cb:
 **/
static void
zif_download_file_got_chunk_cb (SoupMessage *msg, SoupBuffer *chunk, ZifDownload *download)
{
	guint percentage;
	guint header_size;
	guint body_length;
	gboolean ret;

	/* get data */
	body_length = (guint) msg->response_body->length;
	header_size = soup_message_headers_get_content_length (msg->response_headers);

	/* size is not known */
	if (header_size < body_length)
		goto out;

	/* calulate percentage */
	percentage = (100 * body_length) / header_size;
	ret = zif_state_set_percentage (download->priv->state, percentage);
	if (ret)
		g_debug ("download: %i%% (%i, %i) - %p, %p", percentage, body_length, header_size, msg, download);
out:
	return;
}

/**
 * zif_download_file_finished_cb:
 **/
static void
zif_download_file_finished_cb (SoupMessage *msg, ZifDownload *download)
{
	g_debug ("done!");
	g_object_unref (download->priv->msg);
	download->priv->msg = NULL;
}

/**
 * zif_download_cancelled_cb:
 **/
static void
zif_download_cancelled_cb (GCancellable *cancellable, ZifDownload *download)
{
	g_return_if_fail (ZIF_IS_DOWNLOAD (download));

	/* check we have a download */
	if (download->priv->msg == NULL) {
		g_debug ("nothing to cancel");
		return;
	}

	/* cancel */
	g_warning ("cancelling download");
	soup_session_cancel_message (download->priv->session, download->priv->msg, SOUP_STATUS_CANCELLED);
}

/**
 * zif_download_file:
 * @download: the #ZifDownload object
 * @uri: the full remote URI
 * @filename: the local filename to save to
 * @state: a #ZifState to use for progress reporting
 * @error: a #GError which is used on failure, or %NULL
 *
 * Downloads a file.
 *
 * Return value: %TRUE for success, %FALSE for failure
 *
 * Since: 0.1.0
 **/
gboolean
zif_download_file (ZifDownload *download, const gchar *uri, const gchar *filename, ZifState *state, GError **error)
{
	gboolean ret = FALSE;
	SoupURI *base_uri;
	SoupMessage *msg = NULL;
	GError *error_local = NULL;
	gulong cancellable_id = 0;
	GCancellable *cancellable;

	g_return_val_if_fail (ZIF_IS_DOWNLOAD (download), FALSE);
	g_return_val_if_fail (zif_state_valid (state), FALSE);
	g_return_val_if_fail (uri != NULL, FALSE);
	g_return_val_if_fail (filename != NULL, FALSE);
	g_return_val_if_fail (download->priv->msg == NULL, FALSE);
	g_return_val_if_fail (download->priv->session != NULL, FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* save an instance of the state object */
	download->priv->state = g_object_ref (state);

	/* set up cancel */
	cancellable = zif_state_get_cancellable (state);
	if (cancellable != NULL) {
		g_cancellable_reset (cancellable);
		cancellable_id = g_cancellable_connect (cancellable, G_CALLBACK (zif_download_cancelled_cb), download, NULL);
	}

	base_uri = soup_uri_new (uri);
	if (base_uri == NULL) {
		g_set_error (error, ZIF_DOWNLOAD_ERROR, ZIF_DOWNLOAD_ERROR_FAILED,
			     "could not parse uri: %s", uri);
		goto out;
	}

	/* GET package */
	msg = soup_message_new_from_uri (SOUP_METHOD_GET, base_uri);
	if (msg == NULL) {
		g_set_error_literal (error, ZIF_DOWNLOAD_ERROR, ZIF_DOWNLOAD_ERROR_FAILED,
				     "could not setup message");
		goto out;
	}

	/* we want progress updates */
	g_signal_connect (msg, "got-chunk", G_CALLBACK (zif_download_file_got_chunk_cb), download);
	g_signal_connect (msg, "finished", G_CALLBACK (zif_download_file_finished_cb), download);

	/* we need this for cancelling */
	download->priv->msg = g_object_ref (msg);

	/* request */
//	soup_message_set_flags (msg, SOUP_MESSAGE_NO_REDIRECT);

	/* set action */
	zif_state_action_start (state, ZIF_STATE_ACTION_DOWNLOADING, filename);

	/* send sync */
	soup_session_send_message (download->priv->session, msg);

	/* find length */
	if (!SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
		g_set_error (error, ZIF_DOWNLOAD_ERROR, ZIF_DOWNLOAD_ERROR_FAILED,
			     "failed to get valid response for %s: %s", uri, soup_status_get_phrase (msg->status_code));
		goto out;
	}

	/* write file */
	ret = g_file_set_contents (filename, msg->response_body->data, msg->response_body->length, &error_local);
	if (!ret) {
		g_set_error (error, ZIF_DOWNLOAD_ERROR, ZIF_DOWNLOAD_ERROR_FAILED,
			     "failed to write file: %s",  error_local->message);
		g_error_free (error_local);
		goto out;
	}
out:
	if (cancellable_id != 0)
		g_cancellable_disconnect (cancellable, cancellable_id);
	g_object_unref (download->priv->state);
	download->priv->state = NULL;
	if (base_uri != NULL)
		soup_uri_free (base_uri);
	if (msg != NULL)
		g_object_unref (msg);
	return ret;
}

/**
 * zif_download_set_proxy:
 * @download: the #ZifDownload object
 * @http_proxy: the HTTP proxy, e.g. "http://10.0.0.1:8080"
 * @error: a #GError which is used on failure, or %NULL
 *
 * Sets the proxy used for downloading files.
 *
 * Return value: %TRUE for success, %FALSE for failure
 *
 * Since: 0.1.0
 **/
gboolean
zif_download_set_proxy (ZifDownload *download, const gchar *http_proxy, GError **error)
{
	gboolean ret = FALSE;
	SoupURI *proxy = NULL;
	guint connection_timeout;

	g_return_val_if_fail (ZIF_IS_DOWNLOAD (download), FALSE);
	g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

	/* get default value from the config file */
	connection_timeout = zif_config_get_uint (download->priv->config, "connection_timeout", NULL);
	if (connection_timeout == G_MAXUINT)
		connection_timeout = 5;

	/* setup the session */
	if (http_proxy != NULL) {
		g_debug ("using proxy %s", http_proxy);
		proxy = soup_uri_new (http_proxy);
	}
	download->priv->session = soup_session_sync_new_with_options (SOUP_SESSION_PROXY_URI, proxy,
								      SOUP_SESSION_USER_AGENT, "zif",
								      SOUP_SESSION_TIMEOUT, connection_timeout,
								      NULL);
	if (download->priv->session == NULL) {
		g_set_error_literal (error, ZIF_DOWNLOAD_ERROR, ZIF_DOWNLOAD_ERROR_FAILED,
				     "could not setup session");
		goto out;
	}
	ret = TRUE;
out:
	if (proxy != NULL)
		soup_uri_free (proxy);
	return ret;
}

/**
 * zif_download_finalize:
 **/
static void
zif_download_finalize (GObject *object)
{
	ZifDownload *download;

	g_return_if_fail (object != NULL);
	g_return_if_fail (ZIF_IS_DOWNLOAD (object));
	download = ZIF_DOWNLOAD (object);

	g_free (download->priv->proxy);
	if (download->priv->msg != NULL)
		g_object_unref (download->priv->msg);
	if (download->priv->session != NULL)
		g_object_unref (download->priv->session);
	g_object_unref (download->priv->config);

	G_OBJECT_CLASS (zif_download_parent_class)->finalize (object);
}

/**
 * zif_download_class_init:
 **/
static void
zif_download_class_init (ZifDownloadClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	object_class->finalize = zif_download_finalize;

	g_type_class_add_private (klass, sizeof (ZifDownloadPrivate));
}

/**
 * zif_download_init:
 **/
static void
zif_download_init (ZifDownload *download)
{
	download->priv = ZIF_DOWNLOAD_GET_PRIVATE (download);
	download->priv->msg = NULL;
	download->priv->session = NULL;
	download->priv->proxy = NULL;
	download->priv->state = NULL;
	download->priv->config = zif_config_new ();
}

/**
 * zif_download_new:
 *
 * Return value: A new download class instance.
 *
 * Since: 0.1.0
 **/
ZifDownload *
zif_download_new (void)
{
	if (zif_download_object != NULL) {
		g_object_ref (zif_download_object);
	} else {
		zif_download_object = g_object_new (ZIF_TYPE_DOWNLOAD, NULL);
		g_object_add_weak_pointer (zif_download_object, &zif_download_object);
	}
	return ZIF_DOWNLOAD (zif_download_object);
}

