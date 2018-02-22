project('gtksourceview', 'c',
  version: '4.0.4',
  license: 'LGPL-2.1-or-later',
  meson_version: '>= 0.40.1',
  default_options: [
    'buildtype=debugoptimized',
  ]
)

i18n = import('i18n')
gnome = import('gnome')

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

# Dependencies
cc = meson.get_compiler('c')

libm_dep = cc.find_library('m', required: true)

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

gladeui_dep = dependency('gladeui-2.0', version: gladeui_req, required: false)
introspection_dep = dependency('gobject-introspection-1.0', version: introspection_req, required: false)
vapigen_dep = dependency('vapigen', required: false)
gtk_doc_dep = dependency('gtk-doc', version: gtk_doc_req, required: false)

gtk_quartz_dep = dependency('gtk+-quartz-3.0', version: gtk_doc_req, required: false)

xmllint_prg = find_program('xmllint', required: false)

# Configurations
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
  config_h.set_quoted('OS_OSX', 'yes')
endif

configure_file(
  output: 'config.h',
  configuration: config_h
)

public_req = ' '.join([
  'glib-2.0', glib_req,
  'gobject-2.0', glib_req,
  'gio-2.0', glib_req,
  'gtk+-3.0', gtk_req
])

private_req = ' '.join([
  'libxml-2.0', libxml_req,
])

pkg_pc = configuration_data()
pkg_pc.set('prefix', prefix)
pkg_pc.set('exec_prefix', '${prefix}')
pkg_pc.set('libdir', '${exec_prefix}/@0@'.format(get_option('libdir')))
pkg_pc.set('includedir', '${prefix}/@0@'.format(get_option('includedir')))

pkg_pc.set('PACKAGE_NAME', package_name)
pkg_pc.set('PACKAGE_VERSION', version)
pkg_pc.set('AX_PACKAGE_REQUIRES', public_req)
pkg_pc.set('AX_PACKAGE_REQUIRES_PRIVATE', private_req)
pkg_pc.set('GSV_API_VERSION', api_version)

configure_file(
  input: 'gtksourceview.pc.in',
  output: '@0@.pc'.format(package_string),
  configuration: pkg_pc,
  install: true,
  install_dir: pkgconfigdir
)

# Options
install_glade_catalog = get_option('glade_catalog')
if install_glade_catalog and not gladeui_dep.found()
  install_glade_catalog = false
endif

generate_gir = get_option('gir')
if generate_gir and not introspection_dep.found()
  generate_gir = false
endif

generate_vapi = generate_gir and get_option('vapi')
if generate_vapi and not vapigen_dep.found()
  generate_vapi = false
endif

build_gtk_doc = get_option('gtk_doc')
if build_gtk_doc and not gtk_doc_dep.found()
  build_gtk_doc = false
endif

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
