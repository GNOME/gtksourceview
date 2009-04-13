/* 
 *  gsc-provider-test.h - Type here a short description of your plugin
 *
 *  Copyright (C) 2008 - perriman
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
 */
#ifndef __TEST_PROVIDER_H__
#define __TEST_PROVIDER_H__

#include <glib.h>
#include <glib-object.h>
#include <gtksourceview/gtksourcecompletionprovider.h>

G_BEGIN_DECLS

#define GSC_TYPE_PROVIDER_TEST (gsc_provider_test_get_type ())
#define GSC_PROVIDER_TEST(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), GSC_TYPE_PROVIDER_TEST, GscProviderTest))
#define GSC_PROVIDER_TEST_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), GSC_TYPE_PROVIDER_TEST, GscProviderTestClass))
#define GSC_IS_PROVIDER_TEST(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GSC_TYPE_PROVIDER_TEST))
#define GSC_IS_PROVIDER_TEST_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GSC_TYPE_PROVIDER_TEST))
#define GSC_PROVIDER_TEST_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), GSC_TYPE_PROVIDER_TEST, GscProviderTestClass))

#define GSC_PROVIDER_TEST_NAME "GscProviderTest"

typedef struct _GscProviderTest GscProviderTest;
typedef struct _GscProviderTestPrivate GscProviderTestPrivate;
typedef struct _GscProviderTestClass GscProviderTestClass;

struct _GscProviderTest
{
	GObject parent;
	
	GscProviderTestPrivate *priv;
};

struct _GscProviderTestClass
{
	GObjectClass parent;
};

GType		 gsc_provider_test_get_type	(void) G_GNUC_CONST;

GscProviderTest *gsc_provider_test_new (const gchar *name);

G_END_DECLS

#endif
