project('gtksourceview', 'c',
          version: '5.1.1',
          license: 'LGPL-2.1-or-later',
    meson_version: '>= 0.53.0',
  default_options: [ 'c_std=gnu99',
                     'buildtype=debugoptimized',
                     'warning_level=2' ],
)

buildtype = get_option('buildtype')

i18n = import('i18n')
gnome = import('gnome')
pkg = import('pkgconfig')

# Versioning
version = meson.project_version()
version_arr = version.split('.')
version_major = version_arr[0].to_int()
version_minor = version_arr[1].to_int()
version_micro = version_arr[2].to_int()

api_version = '5'

lib_version = '0.0.0'
lib_version_arr = lib_version.split('.')
lib_version_major = lib_version_arr[0].to_int()
lib_version_minor = lib_version_arr[1].to_int()
lib_version_micro = lib_version_arr[2].to_int()

osx_current = lib_version_major + 1
lib_osx_version = [osx_current, '@0@.@1@'.format(osx_current, lib_version_minor)]


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
gir = find_program('g-ir-scanner', required : get_option('introspection'))
generate_gir = gir.found() and (not meson.is_cross_build() or get_option('introspection').enabled())
generate_vapi = generate_gir and get_option('vapi')
build_gtk_doc = get_option('gtk_doc')

# Dependencies
cc = meson.get_compiler('c')

glib_req_version = '2.66'
gtk_req_version = '4.2'

libm_dep = cc.find_library('m', required: false)

glib_req = '>= @0@'.format(glib_req_version)
gtk_req = '>= @0@'.format(gtk_req_version)
libxml_req = '>= 2.6'
introspection_req  = '>= 1.42.0'
gtk_doc_req = '>= 1.25'
fribidi_req = '>= 0.19.7'
pcre2_req = '>= 10.21'

glib_dep = dependency('glib-2.0', version: glib_req)
gobject_dep = dependency('gobject-2.0', version: glib_req)
gio_dep = dependency('gio-2.0', version: glib_req)
gtk_dep = dependency('gtk4', version: gtk_req)
libxml_dep = dependency('libxml-2.0', version: libxml_req,
                        fallback : ['libxml2', 'xml2lib_dep'])
fribidi_dep = dependency('fribidi', version: fribidi_req)
fontconfig_dep = dependency('fontconfig', required: false)
pangoft2_dep = dependency('pangoft2', required: false)
pcre2_dep = dependency('libpcre2-8', version: pcre2_req,
                       fallback : ['pcre2', 'libpcre2_8'])

gtk_quartz_dep = dependency('gtk4-quartz', version: gtk_doc_req, required: false)

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

glib_version_arr = glib_req_version.split('.')
glib_major_version = glib_version_arr[0]
glib_minor_version = glib_version_arr[1]

gtk_version_arr = gtk_req_version.split('.')
gtk_major_version = gtk_version_arr[0]
gtk_minor_version = gtk_version_arr[1]

# Configurations
add_project_arguments([
  '-DG_DISABLE_DEPRECATED',
  '-DGDK_DISABLE_DEPRECATED',
  '-DGTK_DISABLE_DEPRECATED',
  '-DGDK_PIXBUF_DISABLE_DEPRECATED',
  '-DGDK_VERSION_MIN_REQUIRED=GDK_VERSION_@0@_@1@'.format(gtk_major_version, gtk_minor_version),
  '-DGDK_VERSION_MAX_ALLOWED=GDK_VERSION_@0@_@1@'.format(gtk_major_version, gtk_minor_version),
  '-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_@0@_@1@'.format(glib_major_version, glib_minor_version),
  '-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_@0@_@1@'.format(glib_major_version, glib_minor_version),
], language: 'c')

config_h = configuration_data()
config_h.set_quoted('GETTEXT_PACKAGE', package_string)
config_h.set_quoted('DATADIR', datadir)
config_h.set_quoted('HICOLORDIR', join_paths(datadir, 'icons/hicolor'))
config_h.set_quoted('PACKAGE_DATADIR', pkgdatadir)
config_h.set_quoted('GSV_API_VERSION_S', api_version)
config_h.set('GSV_API_VERSION', api_version)
config_h.set('PACKAGE_VERSION', version)
config_h.set10('ENABLE_FONT_CONFIG', fontconfig_dep.found() and pangoft2_dep.found())

if (version_minor % 2 == 1) or (version_minor >= 90)
  config_h.set10('DEVELOPMENT_BUILD', 1)
endif

check_headers = [
  ['unistd.h', 'HAVE_UNISTD_H'],
]

foreach header: check_headers
  if cc.has_header(header.get(0))
    config_h.set(header.get(1), 1)
  endif
endforeach

# libsysprof-capture support
if get_option('sysprof')
  libsysprof_capture_dep = dependency('sysprof-capture-4',
    required: get_option('sysprof'),
    default_options: [
      'enable_examples=false',
      'enable_gtk=false',
      'enable_tests=false',
      'enable_tools=false',
      'libsysprof=true',
      'with_sysprofd=none',
      'help=false',
    ],
    fallback: ['sysprof', 'libsysprof_capture_dep'],
  )
  config_h.set('HAVE_SYSPROF', libsysprof_capture_dep.found())
  profiler_enabled = true
else
  libsysprof_capture_dep = disabler()
  profiler_enabled = false
endif

if build_machine.system() == 'darwin' and gtk_quartz_dep.found()
  cocoa_dep = dependency('appleframeworks', modules: 'Cocoa', required: false)
  assert(cocoa_dep.found(), 'Dependency appleframeworks not found')
  config_h.set_quoted('OS_OSX', 'yes')
endif

# Compiler flags and warnings
global_c_args = []
if cc.get_id() == 'msvc'
  test_c_args = [
    '/FImsvc_recommended_pragmas.h',
  ]
else
  test_c_args = [
    '-Wcast-align',
    '-Wdeclaration-after-statement',
    '-Werror=address',
    '-Werror=array-bounds',
    '-Werror=empty-body',
    '-Werror=implicit',
    '-Werror=implicit-function-declaration',
    # disabled due to glib volatile issues
    # '-Werror=incompatible-pointer-types',
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
    '-Wno-typedef-redefinition',
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

  # Ignore some Werror for clang until GLib fixes volatile usage in
  # GType registration.
  if meson.get_compiler('c').get_id() == 'clang'
    test_c_args += ['-Wno-incompatible-pointer-types-discards-qualifiers']
  endif

  if buildtype != 'plain'
    test_c_args += '-fstack-protector-strong'
  endif
endif

foreach arg: test_c_args
  if cc.has_multi_arguments(arg)
    global_c_args += arg
  endif
endforeach

if cc.get_id() != 'msvc' and cc.has_multi_arguments('-Wmissing-declarations')
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

# Various optimizations for non-debug builds including disabling of
# cast checks, asserts, and additional link options.
release_link_args = []
test_link_args = [
  '-Wl,-z,defs',   # Detect and reject underlinking
  '-Wl,-z,now',    # Disable lazy binding
  '-Wl,-z,relro',  # Read-only segments after relocation
]
if not buildtype.startswith('debug')
  add_global_arguments([
    '-DG_DISABLE_ASSERT',
    '-DG_DISABLE_CAST_CHECKS',
  ], language: 'c')
  test_link_args += ['-Wl,-Bsymbolic', '-fno-plt']
endif
foreach link_arg: test_link_args
  if cc.has_link_argument(link_arg)
    release_link_args += link_arg
  endif
endforeach

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

summary({
  'Documentation': build_gtk_doc,
  'Install tests': get_option('install_tests'),
  'Introspection': generate_gir,
  'Vala vapi': generate_vapi,
  }, bool_yn: true,
)

if meson.version().version_compare('>=0.57.0')
  gnome.post_install(
    gtk_update_icon_cache: true,
  )
else
  meson.add_install_script('meson_postinstall.py')
endif
