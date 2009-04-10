#ifndef __GSC_PROVIDER_DEVHELP_H__
#define __GSC_PROVIDER_DEVHELP_H__

#include <gtksourceview/gtksourcecompletionprovider.h>
#include <gtksourceview/gtksourceview.h>

G_BEGIN_DECLS

#define GSC_TYPE_PROVIDER_DEVHELP		(gsc_provider_devhelp_get_type ())
#define GSC_PROVIDER_DEVHELP(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GSC_TYPE_PROVIDER_DEVHELP, GscProviderDevhelp))
#define GSC_PROVIDER_DEVHELP_CONST(obj)		(G_TYPE_CHECK_INSTANCE_CAST ((obj), GSC_TYPE_PROVIDER_DEVHELP, GscProviderDevhelp const))
#define GSC_PROVIDER_DEVHELP_CLASS(klass)	(G_TYPE_CHECK_CLASS_CAST ((klass), GSC_TYPE_PROVIDER_DEVHELP, GscProviderDevhelpClass))
#define GSC_IS_PROVIDER_DEVHELP(obj)		(G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSC_TYPE_PROVIDER_DEVHELP))
#define GSC_IS_PROVIDER_DEVHELP_CLASS(klass)	(G_TYPE_CHECK_CLASS_TYPE ((klass), GSC_TYPE_PROVIDER_DEVHELP))
#define GSC_PROVIDER_DEVHELP_GET_CLASS(obj)	(G_TYPE_INSTANCE_GET_CLASS ((obj), GSC_TYPE_PROVIDER_DEVHELP, GscProviderDevhelpClass))

typedef struct _GscProviderDevhelp		GscProviderDevhelp;
typedef struct _GscProviderDevhelpClass		GscProviderDevhelpClass;
typedef struct _GscProviderDevhelpPrivate	GscProviderDevhelpPrivate;

struct _GscProviderDevhelp {
	GObject parent;
	
	GscProviderDevhelpPrivate *priv;
};

struct _GscProviderDevhelpClass {
	GObjectClass parent_class;
};

GType gsc_provider_devhelp_get_type (void) G_GNUC_CONST;
GscProviderDevhelp *gsc_provider_devhelp_new (GtkSourceView *view);


G_END_DECLS

#endif /* __GSC_PROVIDER_DEVHELP_H__ */
