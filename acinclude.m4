##############################################################################
# GTK_SOURCE_VIEW_AC_PCRE
# This is essentially pcre's configure.in, contains checks and defines
# needed for pcre
#
AC_DEFUN([GTK_SOURCE_VIEW_AC_PCRE],[

# FIXME some day, BUILD_EGG_REGEX may be false

AC_ARG_WITH([system-pcre],
AC_HELP_STRING([--with-system-pcre], [whether to use system copy of pcre library (default = YES)]),[
    if test x$with_system_pcre = "xyes"; then
        USE_SYSTEM_PCRE="yes"
    else
        USE_SYSTEM_PCRE="no"
    fi
],[
    USE_SYSTEM_PCRE="auto"
])

if test "x$USE_SYSTEM_PCRE" != xno; then
    have_pcre="no"

    PKG_CHECK_MODULES(PCRE, [libpcre >= 6.7], [
        have_pcre="yes"
    ], [
        have_pcre="no"
    ])

    if test $have_pcre = yes; then
        AC_MSG_CHECKING(pcre UTF8 support)

        save_CPPFLAGS="$CPPFLAGS"
        CPPFLAGS="$CPPFLAGS $PCRE_CFLAGS"
        save_CFLAGS="$CFLAGS"
        CFLAGS="$CFLAGS $PCRE_CFLAGS"
        save_LDFLAGS="$LDFLAGS"
        LDFLAGS="$LDFLAGS $PCRE_LIBS"

        AC_RUN_IFELSE([
            AC_LANG_SOURCE([[
            #include <pcre.h>
            #include <stdlib.h>
            int main (int argc, char *argv[])
            {
                int result = 0;

                pcre_config (PCRE_CONFIG_UTF8, &result);

                if (result)
                    pcre_config (PCRE_CONFIG_UNICODE_PROPERTIES, &result);

                if (result)
                    exit (0);
                else
                    exit (1);
            }]
        ])],[
            AC_MSG_RESULT(yes)
            have_pcre=yes
        ],[
            AC_MSG_RESULT(no)
            have_pcre=no
        ],[
            AC_MSG_RESULT(can't check when crosscompiling, assuming it's fine)
            have_pcre=yes
        ])

        CFLAGS="$save_CFLAGS"
        CPPFLAGS="$save_CPPFLAGS"
        LDFLAGS="$save_LDFLAGS"
    fi
fi

if test x$USE_SYSTEM_PCRE != xno; then
    if test x$have_pcre = xyes; then
        USE_SYSTEM_PCRE="yes"
        AC_MSG_NOTICE([using installed libpcre])
    else
        USE_SYSTEM_PCRE="no"
        PCRE_CFLAGS=
        PCRE_LIBS=
        AC_MSG_NOTICE([building pcre library])
    fi
else
    USE_SYSTEM_PCRE="no"
    AC_MSG_NOTICE([building pcre library])
fi

AM_CONDITIONAL(USE_SYSTEM_PCRE, test x$USE_SYSTEM_PCRE = xyes)
if test x$USE_SYSTEM_PCRE = xyes; then
    AC_DEFINE(USE_SYSTEM_PCRE,, [USE_SYSTEM_PCRE - use installed pcre library])
fi


## ===================================================================
## build pcre
##
if test x$USE_SYSTEM_PCRE = xno; then

    AC_C_CONST
    AC_TYPE_SIZE_T
    AC_CHECK_FUNCS(memmove strerror)

    AC_DEFINE(NEWLINE, '\n', [The value of NEWLINE determines the newline character used in pcre])

    AC_DEFINE(LINK_SIZE, 2, [The value of LINK_SIZE determines the number of bytes used to store dnl
    links as offsets within the compiled regex. The default is 2, which allows for dnl
    compiled patterns up to 64K long. This covers the vast majority of cases. dnl
    However, PCRE can also be compiled to use 3 or 4 bytes instead. This allows for dnl
    longer patterns in extreme cases.])

    AC_DEFINE(POSIX_MALLOC_THRESHOLD, 10, [POSIX_MALLOC_THRESHOLD])

    AC_DEFINE(MATCH_LIMIT, 10000000, [The value of MATCH_LIMIT determines the default number of times the match() dnl
    function can be called during a single execution of pcre_exec(). (There is a dnl
    runtime method of setting a different limit.) The limit exists in order to dnl
    catch runaway regular expressions that take for ever to determine that they do dnl
    not match. The default is set very large so that it does not accidentally catch dnl
    legitimate cases.])
    AC_DEFINE(MATCH_LIMIT_RECURSION, MATCH_LIMIT, [The above limit applies to all calls of match(), whether or not they
    increase the recursion depth. In some environments it is desirable to limit the
    depth of recursive calls of match() more strictly, in order to restrict the
    maximum amount of stack (or heap, if NO_RECURSE is defined) that is used. The
    value of MATCH_LIMIT_RECURSION applies only to recursive calls of match(). To
    have any useful effect, it must be less than the value of MATCH_LIMIT. There is
    a runtime method for setting a different limit. On systems that support it,
    "configure" can be used to override this default default.])

    AC_DEFINE(MAX_NAME_SIZE, 32, [These three limits are parameterized just in case anybody ever wants to
    change them. Care must be taken if they are increased, because they guard
    against integer overflow caused by enormously large patterns.])
    AC_DEFINE(MAX_NAME_COUNT, 10000, [MAX_NAME_COUNT])
    AC_DEFINE(MAX_DUPLENGTH, 30000, [MAX_DUPLENGTH])

    AC_DEFINE(SUPPORT_UTF8, , [SUPPORT_UTF8])
    AC_DEFINE(SUPPORT_UCP, , [SUPPORT_UCP])

    dnl pcre does "#if !EBCDIC", not "#ifndef EBCDIC"
    AC_DEFINE(EBCDIC, 0, [If you are compiling for a system that uses EBCDIC instead of ASCII dnl
    character codes, define this macro as 1.])

    # AC_ARG_ENABLE(stack-for-recursion,
    # [  --disable-stack-for-recursion  disable use of stack recursion when matching],
    # if test "$enableval" = "no"; then
    #   NO_RECURSE=-DNO_RECURSE
    # fi
    # )
fi

]) # end of MOO_AC_PCRE
