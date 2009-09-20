#ifndef __GSC_PROVIDER_WORDS_H__
#define __GSC_PROVIDER_WORDS_H__

#include <gtksourceview/gtksourcecompletionprovider.h>
#include <gtksourceview/gtksourceview.h>

G_BEGIN_DECLS

#define GSC_TYPE_PROVIDER_WORDS			(gsc_provider_words_get_type ())
#define GSC_PROVIDER_WORDS(obj)			(G_TYPE_CHECK_INSTANCE_CAST ((obj), GSC_TYPE_PROVIDER_WORDS, GscProviderWords))
#define GSC_PROVIDER_WORDS_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GSC_TYPE_PROVIDER_WORDS, GscProviderWords const))
#define GSC_PROVIDER_WORDS_CLASS(klass)		(G_TYPE_CHECK_CLASS_CAST ((klass), GSC_TYPE_PROVIDER_WORDS, GscProviderWordsClass))
#define GSC_IS_PROVIDER_WORDS(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSC_TYPE_PROVIDER_WORDS))
#define GSC_IS_PROVIDER_WORDS_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GSC_TYPE_PROVIDER_WORDS))
#define GSC_PROVIDER_WORDS_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GSC_TYPE_PROVIDER_WORDS, GscProviderWordsClass))

typedef struct _GscProviderWords		GscProviderWords;
typedef struct _GscProviderWordsClass		GscProviderWordsClass;
typedef struct _GscProviderWordsPrivate	GscProviderWordsPrivate;

struct _GscProviderWords {
	GObject parent;
	
	GscProviderWordsPrivate *priv;
};

struct _GscProviderWordsClass {
	GObjectClass parent_class;
};

GType gsc_provider_words_get_type (void) G_GNUC_CONST;
GscProviderWords *gsc_provider_words_new (GtkSourceView *view);

G_END_DECLS

#endif /* __GSC_PROVIDER_WORDS_H__ */
