Building GtkSourceView for Windows using Visual Studio
=========

Meson is now the supported method of building GtkSourceView with Visual Studio, where the process
of doing so is described in the following section:

Prior to building, you will need the following tools and programs:

- Python 3.6.x or later
- Meson 0.59.0 (or later) at this time of writing
- A version of Visual Studio 2015 to 2022.
- Ninja build tool (unless `--backend=vs` is
 specified in the Meson configure command line for
 Visual Studio 2015, 2017, 2019, 2022)
- The pkg-config utility (or a compatible one), either in
 your PATH or found by setting PKG_CONFIG to point to
 such a tool.
- CMake, either installed manually or as part of Visual Studio 2017 or later
 (optional, but recommended, to help locating dependencies)

You will also need the following prerequesites installed:
- GLib 2.70.0 (or later) at this time of writing
- GTK 4.5.0 (or later) at this time of writing
- LibXML2 2.6 (or later) at this time of writing
- GObject-Introspection 1.70.0 (or later) at this time of writing [optional]

For the depedent packages, you will need to ensure that their pkg-config (.pc) files
could be found by the `pkg-config` utility, either directly or by passing in
`--pkg-config-path=...` to point to the path(s) where the .pc files could be found in the
Meson command line.  For libxml2, if .pc files are not available, ensure that its headers and
import library can be found by ensuring your `%INCLUDE%` and `%LIB%` environment variables
include these repective paths, and ensure that `cmake` is installed and in your `%PATH%`.
For the introspection to complete successfully, you will also need to ensure that the DLLs
can be found in the bindir entries of your .pc files or in your `%PATH%`.

#### Building GtkSourceView

Please do the following:

- Open a Visual Studio command prompt.
- In the command prompt, create an empty build directory that is on the same drive
  as the GtkSourceView sources.
- Run the command in the command prompt in your build directory:
   ```
  meson <path_to_GtkSourceView_sources> --buildtype=... --prefix=... -Dvapi=false
  ```
 
 Where:
 - `--buildtype...` can be `debug`, `release`, `plain` or `debugoptmized` (default),
   please see the Meson documentation for more details
 - `--prefix=...` is where the built files are copied to upon 'install'
 - `Dvapi=false` is for the build to not check for Vala, which is normally not present
   on Visual Studio builds.

 You may want to specify the following as well:
 -  `--backend=vs`: Generate Visual Studio projects for building on Visual Studio 2015
                       2017, 2019 and 2022.  This will remove the need for Ninja.  Please
                       note that the generated project files are only valid on the machine
                       they are generated, and such builds may not be that well-supported
                       by Meson.  Any issues that are found using this method but are not
                       found using Ninja should be reported to the Meson project.

If the previous command completed successfully, carry out the build by running `ninja` or
by opening and building the projects with the generated Visual Studio projects, if using
`--backend=vs`.  If desired, run the test using `ninja test` or building the `test`
project in the Visual Studio projects.

Install the build results using `ninja install` or building the `install` project in the
Visual Studio projects.
