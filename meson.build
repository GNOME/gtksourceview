project('gtksourceview', 'c',
          version: '4.3.0',
          license: 'LGPL-2.1-or-later',
    meson_version: '>= 0.50.0',
  default_options: [ 'c_std=gnu99',
                     'buildtype=debugoptimized',
                     'warning_level=2' ],
)

i18n = import('i18n')
gnome = import('gnome')
pkg = import('pkgconfig')

# Versioning
version = meson.project_version()
version_arr = version.split('.')
version_major = version_arr[0].to_int()
version_minor = version_arr[1].to_int()
version_micro = version_arr[2].to_int()

api_version = '4'

lib_version = '0.0.0'
lib_version_arr = version.split('.')
lib_version_major = version_arr[0].to_int()
lib_version_minor = version_arr[1].to_int()
lib_version_micro = version_arr[2].to_int()

package_name = meson.project_name()
package_string = '@0@-@1@'.format(package_name, api_version)

# Paths
rootdir = include_directories('.')
srcdir = meson.source_root()
builddir = meson.build_root()

prefix = get_option('prefix')

includedir = join_paths(prefix, get_option('includedir'))
libdir = join_paths(prefix, get_option('libdir'))
libexecdir = join_paths(prefix, get_option('libexecdir'))
typelibdir = join_paths(libdir, 'girepository-1.0')
datadir = join_paths(prefix, get_option('datadir'))
localedir = join_paths(prefix, get_option('localedir'))
pkgincludedir = join_paths(includedir, package_string)
pkgconfigdir = join_paths(libdir, 'pkgconfig')
pkgdatadir = join_paths(datadir, package_string)
girdir = join_paths(datadir, 'gir-1.0')
vapidir = join_paths(datadir, 'vala', 'vapi')
mandir = join_paths(prefix, get_option('mandir'))
testexecdir = join_paths(libexecdir, 'installed-tests', package_string)
testdatadir = join_paths(datadir, 'installed-tests', package_string)
docpath = join_paths(datadir, 'gtk-doc', 'html', package_string)

# Options
install_glade_catalog = get_option('glade_catalog')
generate_gir = get_option('gir')
generate_vapi = generate_gir and get_option('vapi')
build_gtk_doc = get_option('gtk_doc')

# Dependencies
cc = meson.get_compiler('c')

libm_dep = cc.find_library('m', required: false)

glib_req = '>= 2.48'
gtk_req = '>= 3.20'
libxml_req = '>= 2.6'
gladeui_req = '>= 3.9'
introspection_req  = '>= 1.42.0'
gtk_doc_req = '>= 1.25'

glib_dep = dependency('glib-2.0', version: glib_req)
gobject_dep = dependency('gobject-2.0', version: glib_req)
gio_dep = dependency('gio-2.0', version: glib_req)
gtk_dep = dependency('gtk+-3.0', version: gtk_req)
libxml_dep = dependency('libxml-2.0', version: libxml_req)

gtk_quartz_dep = dependency('gtk+-quartz-3.0', version: gtk_doc_req, required: false)

if install_glade_catalog
  gladeui_dep = dependency('gladeui-2.0', version: gladeui_req)
else
  gladeui_dep = []
endif

if generate_gir
  introspection_dep = dependency('gobject-introspection-1.0', version: introspection_req)
else
  introspection_dep = []
endif

if generate_vapi
  vapigen_dep = dependency('vapigen')
else
  vapigen_dep = []
endif

if build_gtk_doc
  gtk_doc_dep = dependency('gtk-doc', version: gtk_doc_req)
else
  gtk_doc_dep = []
endif

xmllint_prg = find_program('xmllint', required: false)

# Configurations
add_project_arguments([
  '-DG_DISABLE_DEPRECATED',
  '-DGDK_DISABLE_DEPRECATED',
  '-DGTK_DISABLE_DEPRECATED',
  '-DGDK_PIXBUF_DISABLE_DEPRECATED',
], language: 'c')

config_h = configuration_data()
config_h.set_quoted('GSV_API_VERSION', api_version)
config_h.set_quoted('GETTEXT_PACKAGE', package_string)
config_h.set_quoted('DATADIR', datadir)

check_headers = [
  ['unistd.h', 'HAVE_UNISTD_H'],
]

foreach header: check_headers
  if cc.has_header(header.get(0))
    config_h.set(header.get(1), 1)
  endif
endforeach

if build_machine.system() == 'darwin' and gtk_quartz_dep.found()
  cocoa_dep = dependency('appleframeworks', modules: 'Cocoa', required: false)
  assert(cocoa_dep.found(), 'Dependency appleframeworks not found')
  config_h.set_quoted('OS_OSX', 'yes')
endif

# Compiler flags and warnings
global_c_args = []
test_c_args = [
  '-Wcast-align',
  '-Wdeclaration-after-statement',
  '-Werror=address',
  '-Werror=array-bounds',
  '-Werror=empty-body',
  '-Werror=implicit',
  '-Werror=implicit-function-declaration',
  '-Werror=incompatible-pointer-types',
  '-Werror=init-self',
  '-Werror=int-conversion',
  '-Werror=int-to-pointer-cast',
  '-Werror=main',
  '-Werror=misleading-indentation',
  '-Werror=missing-braces',
  '-Werror=missing-include-dirs',
  '-Werror=nonnull',
  '-Werror=overflow',
  '-Werror=parenthesis',
  '-Werror=pointer-arith',
  '-Werror=pointer-to-int-cast',
  '-Werror=return-type',
  '-Werror=sequence-point',
  '-Werror=shadow',
  '-Werror=strict-prototypes',
  '-Werror=trigraphs',
  '-Werror=undef',
  '-Werror=write-strings',
  '-Wformat-nonliteral',
  ['-Werror=format-security', '-Werror=format=2' ],
  '-Wignored-qualifiers',
  '-Wimplicit-function-declaration',
  '-Wlogical-op',
  '-Wmissing-format-attribute',
  '-Wmissing-include-dirs',
  '-Wmissing-noreturn',
  '-Wnested-externs',
  '-Wno-cast-function-type',
  '-Wno-missing-field-initializers',
  '-Wno-sign-compare',
  '-Wno-unused-parameter',
  '-Wold-style-definition',
  '-Wpointer-arith',
  '-Wstrict-prototypes',
  '-Wswitch-default',
  '-Wswitch-enum',
  '-Wundef',
  '-Wuninitialized',
  '-Wunused',
  '-fno-strict-aliasing',
]
if get_option('buildtype') != 'plain'
  test_c_args += '-fstack-protector-strong'
endif

foreach arg: test_c_args
  if cc.has_multi_arguments(arg)
    global_c_args += arg
  endif
endforeach

if cc.has_multi_arguments('-Wmissing-declarations')
  global_c_args += '-Wmissing-declarations'
endif

# Detect and set symbol visibility
if get_option('default_library') != 'static'
  if host_machine.system() == 'windows'
    config_h.set('DLL_EXPORT', true)
    if cc.get_id() == 'msvc'
      config_h.set('_GTK_SOURCE_EXTERN', '__declspec(dllexport) extern')
    elif cc.has_argument('-fvisibility=hidden')
      config_h.set('_GTK_SOURCE_EXTERN', '__attribute__((visibility("default"))) __declspec(dllexport) extern')
    endif
  elif cc.has_argument('-fvisibility=hidden')
    config_h.set('_GTK_SOURCE_EXTERN', '__attribute__((visibility("default"))) extern')
  endif
endif

add_project_arguments(global_c_args, language: 'c')

configure_file(
         output: 'config.h',
  configuration: config_h
)

subdir('gtksourceview')
subdir('data')
subdir('docs')
subdir('po')
subdir('tests')
subdir('testsuite')

summary = [
  '',
  '------',
  'GtkSourceView @0@ (@1@)'.format(version, api_version),
  '',
  '  Documentation: @0@'.format(build_gtk_doc),
  '  Glade catalog: @0@'.format(install_glade_catalog),
  '  Install tests: @0@'.format(get_option('install_tests')),
  '  Introspection: @0@'.format(generate_gir),
  '      Vala vapi: @0@'.format(generate_vapi),
  '',
  'Directories:',
  '         prefix: @0@'.format(prefix),
  '     includedir: @0@'.format(includedir),
  '         libdir: @0@'.format(libdir),
  '        datadir: @0@'.format(datadir),
  '------',
]

message('\n'.join(summary))