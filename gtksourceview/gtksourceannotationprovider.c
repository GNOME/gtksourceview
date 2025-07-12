#include "config.h"

#include "gtksourceannotation.h"
#include "gtksourceannotation-private.h"
#include "gtksourcehoverdisplay.h"
#include "gtksourceannotationprovider-private.h"

/**
 * GtkSourceAnnotationProvider:
 *
 * It is used to provide annotations and display them on [class@View] and also populate
 * [class@HoverDisplay] when the user hovers over an annotation.
 *
 * You can subclass this object and implement [method@AnnotationProvider.populate_hover_async] and
 * [method@AnnotationProvider.populate_hover_finish] or connect to [signal@AnnotationProvider::populate]
 * and call [method@AnnotationProvider.populate] or do it asynchronously.
 *
 * Since: 5.18
 */

typedef struct
{
	GPtrArray *annotations;
} GtkSourceAnnotationProviderPrivate;

G_DEFINE_TYPE_WITH_PRIVATE (GtkSourceAnnotationProvider, gtk_source_annotation_provider, G_TYPE_OBJECT)

enum {
	CHANGED,
	N_SIGNALS
};

static guint signals [N_SIGNALS];

static void
gtk_source_annotation_provider_real_populate_hover_async (GtkSourceAnnotationProvider *self,
                                                          GtkSourceAnnotation         *annotation,
                                                          GtkSourceHoverDisplay       *display,
                                                          GCancellable                *cancellable,
                                                          GAsyncReadyCallback          callback,
                                                          gpointer                     user_data)
{
	GTask *task = NULL;
	gboolean ret = FALSE;

	g_assert (GTK_SOURCE_IS_ANNOTATION_PROVIDER (self));
	g_assert (GTK_SOURCE_IS_HOVER_DISPLAY (display));
	g_assert (!cancellable || G_IS_CANCELLABLE (cancellable));

	task = g_task_new (self, cancellable, callback, user_data);
	g_task_set_source_tag (task, gtk_source_annotation_provider_populate_hover_async);

	if (!ret)
	{
		g_task_return_new_error (task,
		                         G_IO_ERROR,
		                         G_IO_ERROR_FAILED,
		                         "Provider has not implemented populate");

		g_object_unref (task);
		return;
	}

	g_task_return_boolean (task, ret);

	g_object_unref (task);
}

static gboolean
gtk_source_annotation_provider_real_populate_hover_finish (GtkSourceAnnotationProvider  *self,
                                                           GAsyncResult                 *result,
                                                           GError                      **error)
{
	g_return_val_if_fail (GTK_SOURCE_IS_ANNOTATION_PROVIDER (self), FALSE);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

	return g_task_propagate_boolean (G_TASK (result), error);
}

static void
gtk_source_annotation_provider_finalize (GObject *object)
{
	GtkSourceAnnotationProvider *self = GTK_SOURCE_ANNOTATION_PROVIDER (object);
	GtkSourceAnnotationProviderPrivate *priv = gtk_source_annotation_provider_get_instance_private (self);

	g_clear_pointer (&priv->annotations, g_ptr_array_unref);

	G_OBJECT_CLASS (gtk_source_annotation_provider_parent_class)->finalize (object);
}

static void
gtk_source_annotation_provider_init (GtkSourceAnnotationProvider *self)
{
	GtkSourceAnnotationProviderPrivate *priv = gtk_source_annotation_provider_get_instance_private (self);

	priv->annotations = g_ptr_array_new_with_free_func (g_object_unref);
}

static void
gtk_source_annotation_provider_class_init (GtkSourceAnnotationProviderClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);

	object_class->finalize = gtk_source_annotation_provider_finalize;

	klass->populate_hover_async = gtk_source_annotation_provider_real_populate_hover_async;
	klass->populate_hover_finish = gtk_source_annotation_provider_real_populate_hover_finish;

	/**
	 * GtkSourceAnnotationProvider::changed:
	 *
	 * Since: 5.18
	 */
	signals[CHANGED] =
		g_signal_new ("changed",
		              G_TYPE_FROM_CLASS (klass),
		              G_SIGNAL_RUN_LAST,
		              0,
		              NULL, NULL,
		              NULL,
		              G_TYPE_NONE, 0);
}

/**
 * gtk_source_annotation_provider_new:
 *
 * Used to create a new annotation provider.
 *
 * Returns: a new #GtkSourceAnnotationProvider
 *
 * Since: 5.18
 */
GtkSourceAnnotationProvider *
gtk_source_annotation_provider_new (void)
{
	return g_object_new (GTK_SOURCE_TYPE_ANNOTATION_PROVIDER, NULL);
}

/**
 * gtk_source_annotation_provider_populate_hover_async:
 * @self: a #GtkSourceAnnotationProvider
 * @annotation: a #GtkSourceAnnotation
 * @display: a #GtkSourceHoverDisplay to populate
 *
 * Used to populate the [class@HoverDisplay] asynchronously, use
 * [method@AnnotationProvider.populate_hover] to do it synchronously.
 *
 * Since: 5.18
 */
void
gtk_source_annotation_provider_populate_hover_async (GtkSourceAnnotationProvider *self,
                                                     GtkSourceAnnotation         *annotation,
                                                     GtkSourceHoverDisplay       *display,
                                                     GCancellable                *cancellable,
                                                     GAsyncReadyCallback          callback,
                                                     gpointer                     user_data)
{
	g_return_if_fail (GTK_SOURCE_IS_ANNOTATION_PROVIDER (self));
	g_return_if_fail (GTK_SOURCE_IS_ANNOTATION (annotation));
	g_return_if_fail (GTK_SOURCE_IS_HOVER_DISPLAY (display));
	g_return_if_fail (!cancellable || G_IS_CANCELLABLE (cancellable));

	GTK_SOURCE_ANNOTATION_PROVIDER_GET_CLASS (self)->populate_hover_async (self,
	                                                                       annotation,
	                                                                       display,
	                                                                       cancellable,
	                                                                       callback,
	                                                                       user_data);
}

/**
 * gtk_source_annotation_provider_populate_hover_finish:
 *
 * Finishes populating the [class@HoverDisplay] asynchronously.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and @error is set.
 *
 * Since: 5.18
 */
gboolean
gtk_source_annotation_provider_populate_hover_finish (GtkSourceAnnotationProvider  *self,
                                                      GAsyncResult                 *result,
                                                      GError                      **error)
{
	g_return_val_if_fail (GTK_SOURCE_IS_ANNOTATION_PROVIDER (self), FALSE);
	g_return_val_if_fail (G_IS_ASYNC_RESULT (result), FALSE);

	return GTK_SOURCE_ANNOTATION_PROVIDER_GET_CLASS (self)->populate_hover_finish (self, result, error);
}

/**
 * gtk_source_annotation_provider_add_annotation:
 *
 * Add an annotation to the provider.
 *
 * Since: 5.18
 */
void
gtk_source_annotation_provider_add_annotation (GtkSourceAnnotationProvider *self,
                                               GtkSourceAnnotation         *annotation)
{
	GtkSourceAnnotationProviderPrivate *priv = gtk_source_annotation_provider_get_instance_private (self);

	g_return_if_fail (GTK_SOURCE_IS_ANNOTATION_PROVIDER (self));
	g_return_if_fail (GTK_SOURCE_IS_ANNOTATION (annotation));

	g_ptr_array_add (priv->annotations, g_object_ref (annotation));

	g_signal_emit (self, signals[CHANGED], 0);
}

/**
 * gtk_source_annotation_provider_remove_annotation:
 *
 * Remove an annotation from the provider.
 *
 * Returns: %TRUE if the annotation was found and removed
 *
 * Since: 5.18
 */
gboolean
gtk_source_annotation_provider_remove_annotation (GtkSourceAnnotationProvider *self,
                                                  GtkSourceAnnotation         *annotation)
{
	GtkSourceAnnotationProviderPrivate *priv = gtk_source_annotation_provider_get_instance_private (self);
	gboolean result;

	g_return_val_if_fail (GTK_SOURCE_IS_ANNOTATION_PROVIDER (self), FALSE);
	g_return_val_if_fail (GTK_SOURCE_IS_ANNOTATION (annotation), FALSE);

	result = g_ptr_array_remove (priv->annotations, annotation);

	return result;
}

/**
 * gtk_source_annotation_provider_remove_all:
 *
 * Removes all annotations from the provider.
 *
 * Since: 5.18
 */
void
gtk_source_annotation_provider_remove_all (GtkSourceAnnotationProvider  *self)
{
	GtkSourceAnnotationProviderPrivate *priv = gtk_source_annotation_provider_get_instance_private (self);

	g_return_if_fail (GTK_SOURCE_IS_ANNOTATION_PROVIDER (self));

	if (priv->annotations != NULL)
	{
		g_ptr_array_remove_range (priv->annotations, 0, priv->annotations->len);
	}

	g_signal_emit (self, signals[CHANGED], 0);
}

GPtrArray *
_gtk_source_annotation_provider_get_annotations (GtkSourceAnnotationProvider  *self)
{
	GtkSourceAnnotationProviderPrivate *priv = gtk_source_annotation_provider_get_instance_private (self);

	g_return_val_if_fail (GTK_SOURCE_IS_ANNOTATION_PROVIDER (self), NULL);

	return priv->annotations;
}
