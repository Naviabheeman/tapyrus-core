dnl require autoconf 2.60 (AS_ECHO/AS_ECHO_N)
AC_PREREQ([2.60])
define(_CLIENT_VERSION_MAJOR, 0)
define(_CLIENT_VERSION_MINOR, 6)
define(_CLIENT_VERSION_REVISION, 0)
define(_CLIENT_VERSION_BUILD, 0)
define(_CLIENT_VERSION_IS_RELEASE, true)
define(_COPYRIGHT_YEAR, 2018)
define(_COPYRIGHT_HOLDERS,[The %s developers])
define(_COPYRIGHT_HOLDERS_SUBSTITUTION,[[Bitcoin Core]])
AC_INIT([Tapyrus Core],[_CLIENT_VERSION_MAJOR._CLIENT_VERSION_MINOR._CLIENT_VERSION_REVISION],[https://github.com/chaintope/tapyrus-core/issues],[tapyrus-core],[https://www.chaintope.com])
AC_CONFIG_SRCDIR([src/validation.cpp])
AC_CONFIG_HEADERS([src/config/bitcoin-config.h])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([build-aux/m4])

BITCOIN_DAEMON_NAME=tapyrusd
BITCOIN_GUI_NAME=tapyrus-qt
BITCOIN_CLI_NAME=tapyrus-cli
BITCOIN_TX_NAME=tapyrus-tx
TAPYRUS_GENESIS_NAME=tapyrus-genesis

dnl Unless the user specified ARFLAGS, force it to be cr
AC_ARG_VAR(ARFLAGS, [Flags for the archiver, defaults to <cr> if not set])
if test "x${ARFLAGS+set}" != "xset"; then
  ARFLAGS="cr"
fi

AC_CANONICAL_HOST

AH_TOP([#ifndef BITCOIN_CONFIG_H])
AH_TOP([#define BITCOIN_CONFIG_H])
AH_BOTTOM([#endif //BITCOIN_CONFIG_H])

dnl faketime breaks configure and is only needed for make. Disable it here.
unset FAKETIME

dnl Automake init set-up and checks
AM_INIT_AUTOMAKE([1.13 no-define subdir-objects foreign])

dnl faketime messes with timestamps and causes configure to be re-run.
dnl --disable-maintainer-mode can be used to bypass this.
AM_MAINTAINER_MODE([enable])

dnl make the compilation flags quiet unless V=1 is used
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])

dnl Compiler checks (here before libtool).
if test "x${CXXFLAGS+set}" = "xset"; then
  CXXFLAGS_overridden=yes
else
  CXXFLAGS_overridden=no
fi
AC_PROG_CXX

dnl By default, libtool for mingw refuses to link static libs into a dll for
dnl fear of mixing pic/non-pic objects, and import/export complications. Since
dnl we have those under control, re-enable that functionality.
case $host in
  *mingw*)
     lt_cv_deplibs_check_method="pass_all"

      dnl Remove unwanted -DDLL_EXPORT from these variables.
      dnl We do not use this macro, but system headers may export unwanted symbols
      dnl if it's set.
      lt_cv_prog_compiler_pic="-DPIC"
      lt_cv_prog_compiler_pic_CXX="-DPIC"
  ;;
  *darwin*)
        dnl Because it prints a verbose warning, lld fails the following check
        dnl for "-Wl,-single_module" from libtool.m4:
        dnl   # If there is a non-empty error log, and "single_module"
        dnl   # appears in it, assume the flag caused a linker warning
        dnl "-single_module" works fine on ld64 and lld, so just bypass the test.
        dnl Failure to set this to "yes" causes libtool to use a very broken
        dnl link-line for shared libs.
    lt_cv_apple_cc_single_mod="yes"
    ;;
esac
dnl Require C++17 compiler (no GNU extensions)
AX_CXX_COMPILE_STDCXX([17], [noext], [mandatory], [nodefault])

dnl Unless the user specified OBJCXX, force it to be the same as CXX. This ensures
dnl that we get the same -std flags for both.
m4_ifdef([AC_PROG_OBJCXX],[
if test "x${OBJCXX+set}" = "x"; then
  OBJCXX="${CXX}"
fi
AC_PROG_OBJCXX
])

dnl Libtool init checks.
LT_INIT([pic-only win32-dll])

dnl Check/return PATH for base programs.
AC_PATH_TOOL(AR, ar)
AC_PATH_TOOL([GCOV], [gcov])
AC_PATH_TOOL([LLVM_COV], [llvm-cov])
AC_PATH_PROG([LCOV], [lcov])
dnl The minimum supported version is specified in .python-version and should be used if available, see doc/dependencies.md
AC_PATH_PROGS([PYTHON], [python3.9 python3.10 python3.11 python3.12 python3 python])
AC_PATH_PROG([GENHTML], [genhtml])
AC_PATH_PROG([GIT], [git])
AC_PATH_PROG([CCACHE], [ccache])
AC_PATH_PROG([XGETTEXT], [xgettext])
AC_PATH_PROG([HEXDUMP], [hexdump])
AC_PATH_TOOL([OBJDUMP], [objdump])
AC_PATH_TOOL([OBJCOPY], [objcopy])
AC_PATH_PROG([DOXYGEN], [doxygen])
AM_CONDITIONAL([HAVE_DOXYGEN], [test -n "$DOXYGEN"])

AC_ARG_VAR(PYTHONPATH, Augments the default search path for python module files)

# Enable wallet
AC_ARG_ENABLE([wallet],
  [AS_HELP_STRING([--disable-wallet],
  [disable wallet (enabled by default)])],
  [enable_wallet=$enableval],
  [enable_wallet=yes])

AC_ARG_ENABLE([usdt],
  [AS_HELP_STRING([--enable-usdt],
  [enable tracepoints for Userspace, Statically Defined Tracing (default is yes if sys/sdt.h is found)])],
  [use_usdt=$enableval],
  [use_usdt=yes])

AC_ARG_WITH([miniupnpc],
  [AS_HELP_STRING([--with-miniupnpc],
  [enable UPNP (default is yes if libminiupnpc is found)])],
  [use_upnp=$withval],
  [use_upnp=auto])

AC_ARG_ENABLE([upnp-default],
  [AS_HELP_STRING([--enable-upnp-default],
  [if UPNP is enabled, turn it on at startup (default is no)])],
  [use_upnp_default=$enableval],
  [use_upnp_default=no])

AC_ARG_ENABLE(tests,
    AS_HELP_STRING([--disable-tests],[do not compile tests (default is to compile)]),
    [use_tests=$enableval],
    [use_tests=yes])

AC_ARG_ENABLE(gui-tests,
    AS_HELP_STRING([--disable-gui-tests],[do not compile GUI tests (default is to compile if GUI and tests enabled)]),
    [use_gui_tests=no],
    [use_gui_tests=no])

AC_ARG_ENABLE(bench,
    AS_HELP_STRING([--disable-bench],[do not compile benchmarks (default is to compile)]),
    [use_bench=$enableval],
    [use_bench=yes])

AC_ARG_ENABLE([extended-functional-tests],
    AS_HELP_STRING([--enable-extended-functional-tests],[enable expensive functional tests when using lcov (default no)]),
    [use_extended_functional_tests=$enableval],
    [use_extended_functional_tests=no])

AC_ARG_WITH([qrencode],
  [AS_HELP_STRING([--with-qrencode],
  [enable QR code support (default is yes if qt is enabled and libqrencode is found)])],
  [use_qr=$withval],
  [use_qr=auto])

AC_ARG_ENABLE([hardening],
  [AS_HELP_STRING([--disable-hardening],
  [do not attempt to harden the resulting executables (default is to harden when possible)])],
  [use_hardening=$enableval],
  [use_hardening=auto])

AC_ARG_ENABLE([reduce-exports],
  [AS_HELP_STRING([--enable-reduce-exports],
  [attempt to reduce exported symbols in the resulting executables (default is no)])],
  [use_reduce_exports=$enableval],
  [use_reduce_exports=no])

AC_ARG_ENABLE([ccache],
  [AS_HELP_STRING([--disable-ccache],
  [do not use ccache for building (default is to use if found)])],
  [use_ccache=$enableval],
  [use_ccache=auto])

dnl Suppress warnings from external headers (e.g. Boost, Qt).
dnl May be useful if warnings from external headers clutter the build output
dnl too much, so that it becomes difficult to spot Bitcoin Core warnings
dnl or if they cause a build failure with --enable-werror.
AC_ARG_ENABLE([suppress-external-warnings],
  [AS_HELP_STRING([--disable-suppress-external-warnings],
                  [Do not suppress warnings from external headers (default is to suppress)])],
  [suppress_external_warnings=$enableval],
  [suppress_external_warnings=yes])

AC_ARG_ENABLE([lcov],
  [AS_HELP_STRING([--enable-lcov],
  [enable lcov testing (default is no)])],
  [use_lcov=$enableval],
  [use_lcov=no])

AC_ARG_ENABLE([lcov-branch-coverage],
  [AS_HELP_STRING([--enable-lcov-branch-coverage],
  [enable lcov testing branch coverage (default is no)])],
  [use_lcov_branch=yes],
  [use_lcov_branch=no])

AC_ARG_ENABLE([zmq],
  [AS_HELP_STRING([--disable-zmq],
  [disable ZMQ notifications])],
  [use_zmq=$enableval],
  [use_zmq=yes])

AC_ARG_ENABLE(man,
    [AS_HELP_STRING([--disable-man],
                    [do not install man pages (default is to install)])],,
    enable_man=yes)
AM_CONDITIONAL(ENABLE_MAN, test "$enable_man" != no)

# Enable debug
AC_ARG_ENABLE([debug],
    [AS_HELP_STRING([--enable-debug],
                    [use debug compiler flags and macros (default is no)])],
    [enable_debug=$enableval],
    [enable_debug=no])

# Enable different -fsanitize options
AC_ARG_WITH([sanitizers],
    [AS_HELP_STRING([--with-sanitizers],
                    [comma separated list of extra sanitizers to build with (default is none enabled)])],
    [use_sanitizers=$withval])

AC_LANG_PUSH([C++])
AX_CHECK_COMPILE_FLAG([-Werror],[CXXFLAG_WERROR="-Werror"],[CXXFLAG_WERROR=""])

dnl Check for a flag to turn linker warnings into errors. When flags are passed to linkers via the
dnl compiler driver using a -Wl,-foo flag, linker warnings may be swallowed rather than bubbling up.
dnl See note above, the same applies here as well.
dnl
dnl LDFLAG_WERROR Should only be used when testing -Wl,*
case $host in
 *darwin*)
    AX_CHECK_LINK_FLAG([-Wl,-fatal_warnings], [LDFLAG_WERROR="-Wl,-fatal_warnings"], [LDFLAG_WERROR=""])
    ;;
  *)
    AX_CHECK_LINK_FLAG([-Wl,--fatal-warnings], [LDFLAG_WERROR="-Wl,--fatal-warnings"], [LDFLAG_WERROR=""])
    ;;
esac

if test "x$enable_debug" = xyes; then
  dnl Disable all optimizations
  AX_CHECK_COMPILE_FLAG([-O0], [DEBUG_CXXFLAGS="$DEBUG_CXXFLAGS -O0"], [], [$CXXFLAG_WERROR])

  dnl Prefer -g3, fall back to -g if that is unavailable.
  AX_CHECK_COMPILE_FLAG(
    [-g3],
    [DEBUG_CXXFLAGS="$DEBUG_CXXFLAGS -g3"],
    [AX_CHECK_COMPILE_FLAG([-g], [DEBUG_CXXFLAGS="$DEBUG_CXXFLAGS -g"], [], [$CXXFLAG_WERROR])],
    [$CXXFLAG_WERROR])

  AX_CHECK_PREPROC_FLAG([-DDEBUG], [DEBUG_CPPFLAGS="$DEBUG_CPPFLAGS -DDEBUG"], [], [$CXXFLAG_WERROR])
  AX_CHECK_PREPROC_FLAG([-DDEBUG_LOCKORDER], [DEBUG_CPPFLAGS="$DEBUG_CPPFLAGS -DDEBUG_LOCKORDER"], [], [$CXXFLAG_WERROR])
  AX_CHECK_PREPROC_FLAG([-DDEBUG_LOCKCONTENTION], [DEBUG_CPPFLAGS="$DEBUG_CPPFLAGS -DDEBUG_LOCKCONTENTION"], [], [$CXXFLAG_WERROR])
  AX_CHECK_PREPROC_FLAG([-DRPC_DOC_CHECK], [DEBUG_CPPFLAGS="$DEBUG_CPPFLAGS -DRPC_DOC_CHECK"], [], [$CXXFLAG_WERROR])
  AX_CHECK_PREPROC_FLAG([-DABORT_ON_FAILED_ASSUME], [DEBUG_CPPFLAGS="$DEBUG_CPPFLAGS -DABORT_ON_FAILED_ASSUME"], [], [$CXXFLAG_WERROR])
  AX_CHECK_COMPILE_FLAG([-ftrapv], [DEBUG_CXXFLAGS="$DEBUG_CXXFLAGS -ftrapv"], [], [$CXXFLAG_WERROR])
fi

if test x$use_sanitizers != x; then
  dnl First check if the compiler accepts flags. If an incompatible pair like
  dnl -fsanitize=address,thread is used here, this check will fail. This will also
  dnl fail if a bad argument is passed, e.g. -fsanitize=undfeined
  AX_CHECK_COMPILE_FLAG(
    [-fsanitize=$use_sanitizers],
    [SANITIZER_CXXFLAGS="-fsanitize=$use_sanitizers"
     SANITIZER_CFLAGS="-fsanitize=$use_sanitizers"],
    [AC_MSG_ERROR([compiler did not accept requested flags])])

  dnl Some compilers (e.g. GCC) require additional libraries like libasan,
  dnl libtsan, libubsan, etc. Make sure linking still works with the sanitize
  dnl flag. This is a separate check so we can give a better error message when
  dnl the sanitize flags are supported by the compiler but the actual sanitizer
  dnl libs are missing.
  AX_CHECK_LINK_FLAG(
    [-fsanitize=$use_sanitizers],
    [SANITIZER_LDFLAGS="-fsanitize=$use_sanitizers"],
    [AC_MSG_ERROR([linker did not accept requested flags, you are missing required libraries])],
    [],
    [AC_LANG_PROGRAM([[
    #include <cstdint>
    #include <cstddef>
    extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) { return 0; }
    __attribute__((weak)) // allow for libFuzzer linking
    ]],[[]])])
fi

ERROR_CXXFLAGS=
if test "$enable_werror" = "yes"; then
  if test "$CXXFLAG_WERROR" = ""; then
    AC_MSG_ERROR([enable-werror set but -Werror is not usable])
  fi
  ERROR_CXXFLAGS=$CXXFLAG_WERROR
fi

AX_CHECK_COMPILE_FLAG([-Wall], [WARN_CXXFLAGS="$WARN_CXXFLAGS -Wall"], [], [$CXXFLAG_WERROR])
AX_CHECK_COMPILE_FLAG([-Wextra], [WARN_CXXFLAGS="$WARN_CXXFLAGS -Wextra"], [], [$CXXFLAG_WERROR])
AX_CHECK_COMPILE_FLAG([-Wgnu], [WARN_CXXFLAGS="$WARN_CXXFLAGS -Wgnu"], [], [$CXXFLAG_WERROR])
dnl some compilers will ignore -Wformat-security without -Wformat, so just combine the two here.
AX_CHECK_COMPILE_FLAG([-Wformat -Wformat-security], [WARN_CXXFLAGS="$WARN_CXXFLAGS -Wformat -Wformat-security"], [], [$CXXFLAG_WERROR])
AX_CHECK_COMPILE_FLAG([-Wvla], [WARN_CXXFLAGS="$WARN_CXXFLAGS -Wvla"], [], [$CXXFLAG_WERROR])
AX_CHECK_COMPILE_FLAG([-Wshadow-field], [WARN_CXXFLAGS="$WARN_CXXFLAGS -Wshadow-field"], [], [$CXXFLAG_WERROR])
AX_CHECK_COMPILE_FLAG([-Wthread-safety], [WARN_CXXFLAGS="$WARN_CXXFLAGS -Wthread-safety"], [], [$CXXFLAG_WERROR])
AX_CHECK_COMPILE_FLAG([-Wloop-analysis], [WARN_CXXFLAGS="$WARN_CXXFLAGS -Wloop-analysis"], [], [$CXXFLAG_WERROR])
dnl AX_CHECK_COMPILE_FLAG([-Wredundant-decls], [WARN_CXXFLAGS="$WARN_CXXFLAGS -Wredundant-decls"], [], [$CXXFLAG_WERROR])
dnl AX_CHECK_COMPILE_FLAG([-Wunused-member-function], [WARN_CXXFLAGS="$WARN_CXXFLAGS -Wunused-member-function"], [], [$CXXFLAG_WERROR])
dnl AX_CHECK_COMPILE_FLAG([-Wdate-time], [WARN_CXXFLAGS="$WARN_CXXFLAGS -Wdate-time"], [], [$CXXFLAG_WERROR])
dnl AX_CHECK_COMPILE_FLAG([-Wconditional-uninitialized], [WARN_CXXFLAGS="$WARN_CXXFLAGS -Wconditional-uninitialized"], [], [$CXXFLAG_WERROR])
dnl AX_CHECK_COMPILE_FLAG([-Wduplicated-branches], [WARN_CXXFLAGS="$WARN_CXXFLAGS -Wduplicated-branches"], [], [$CXXFLAG_WERROR])
dnl AX_CHECK_COMPILE_FLAG([-Wduplicated-cond], [WARN_CXXFLAGS="$WARN_CXXFLAGS -Wduplicated-cond"], [], [$CXXFLAG_WERROR])
dnl AX_CHECK_COMPILE_FLAG([-Wlogical-op], [WARN_CXXFLAGS="$WARN_CXXFLAGS -Wlogical-op"], [], [$CXXFLAG_WERROR])
dnl AX_CHECK_COMPILE_FLAG([-Woverloaded-virtual], [WARN_CXXFLAGS="$WARN_CXXFLAGS -Woverloaded-virtual"], [], [$CXXFLAG_WERROR])
dnl AX_CHECK_COMPILE_FLAG([-Wsuggest-override], [WARN_CXXFLAGS="$WARN_CXXFLAGS -Wsuggest-override"], [], [$CXXFLAG_WERROR])
dnl AX_CHECK_COMPILE_FLAG([-Wimplicit-fallthrough], [WARN_CXXFLAGS="$WARN_CXXFLAGS -Wimplicit-fallthrough"], [], [$CXXFLAG_WERROR])
dnl AX_CHECK_COMPILE_FLAG([-Wunreachable-code], [WARN_CXXFLAGS="$WARN_CXXFLAGS -Wunreachable-code"], [], [$CXXFLAG_WERROR])
dnl AX_CHECK_COMPILE_FLAG([-Wdocumentation], [WARN_CXXFLAGS="$WARN_CXXFLAGS -Wdocumentation"], [], [$CXXFLAG_WERROR])
dnl AX_CHECK_COMPILE_FLAG([-Wself-assign], [WARN_CXXFLAGS="$WARN_CXXFLAGS -Wself-assign"], [], [$CXXFLAG_WERROR])
dnl AX_CHECK_COMPILE_FLAG([-Wundef], [WARN_CXXFLAGS="$WARN_CXXFLAGS -Wundef"], [], [$CXXFLAG_WERROR])

dnl Some compilers (gcc) ignore unknown -Wno-* options, but warn about all
dnl unknown options if any other warning is produced. Test the -Wfoo case, and
dnl set the -Wno-foo case if it works.
AX_CHECK_COMPILE_FLAG([-Wunused-parameter], [NOWARN_CXXFLAGS="$NOWARN_CXXFLAGS -Wno-unused-parameter"], [], [$CXXFLAG_WERROR])

dnl Don't allow extended (non-ASCII) symbols in identifiers. This is easier for code review.
AX_CHECK_COMPILE_FLAG([-fno-extended-identifiers], [CORE_CXXFLAGS="$CORE_CXXFLAGS -fno-extended-identifiers"], [], [$CXXFLAG_WERROR])

dnl Currently all versions of gcc are subject to a class of bugs, see the
dnl gccbug_90348 test case (only reproduces on GCC 11 and earlier) and
dnl https://gcc.gnu.org/bugzilla/show_bug.cgi?id=111843. To work around that, set
dnl -fstack-reuse=none for all gcc builds. (Only gcc understands this flag)
AX_CHECK_COMPILE_FLAG([-fstack-reuse=none], [CORE_CXXFLAGS="$CORE_CXXFLAGS -fstack-reuse=none"])

enable_arm_crc=no
enable_sse42=no
enable_sse41=no
enable_avx2=no

# Check for optional instruction set support. Enabling these does _not_ imply that all code will
# be compiled with them, rather that specific objects/libs may use them after checking for runtime
# compatibility.
AX_CHECK_COMPILE_FLAG([-msse4.2],[[SSE42_CXXFLAGS="-msse4.2"]],,[[$CXXFLAG_WERROR]])
AX_CHECK_COMPILE_FLAG([-msse4.1],[[SSE41_CXXFLAGS="-msse4.1"]],,[[$CXXFLAG_WERROR]])
AX_CHECK_COMPILE_FLAG([-mavx -mavx2],[[AVX2_CXXFLAGS="-mavx -mavx2"]],,[[$CXXFLAG_WERROR]])
AX_CHECK_COMPILE_FLAG([-msse4 -msha],[[SHANI_CXXFLAGS="-msse4 -msha"]],,[[$CXXFLAG_WERROR]])

TEMP_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="$CXXFLAGS $SSE42_CXXFLAGS"
AC_MSG_CHECKING(for assembler crc32 support)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
    #include <stdint.h>
    #if defined(_MSC_VER)
    #include <intrin.h>
    #elif defined(__GNUC__) && defined(__SSE4_2__)
    #include <nmmintrin.h>
    #endif
  ]],[[
    uint64_t l = 0;
    l = _mm_crc32_u8(l, 0);
    l = _mm_crc32_u32(l, 0);
    l = _mm_crc32_u64(l, 0);
    return l;
  ]])],
 [ AC_MSG_RESULT(yes); enable_hwcrc32=yes],
 [ AC_MSG_RESULT(no)]
)
CXXFLAGS="$TEMP_CXXFLAGS"

TEMP_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="$CXXFLAGS $SSE41_CXXFLAGS"
AC_MSG_CHECKING(for SSE4.1 intrinsics)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
    #include <stdint.h>
    #include <immintrin.h>
  ]],[[
    __m128i l = _mm_set1_epi32(0);
    return _mm_extract_epi32(l, 3);
  ]])],
 [ AC_MSG_RESULT(yes); enable_sse41=yes; AC_DEFINE(ENABLE_SSE41, 1, [Define this symbol to build code that uses SSE4.1 intrinsics]) ],
 [ AC_MSG_RESULT(no)]
)
CXXFLAGS="$TEMP_CXXFLAGS"

TEMP_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="$CXXFLAGS $AVX2_CXXFLAGS"
AC_MSG_CHECKING(for AVX2 intrinsics)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
    #include <stdint.h>
    #include <immintrin.h>
  ]],[[
    __m256i l = _mm256_set1_epi32(0);
    return _mm256_extract_epi32(l, 7);
  ]])],
 [ AC_MSG_RESULT(yes); enable_avx2=yes; AC_DEFINE(ENABLE_AVX2, 1, [Define this symbol to build code that uses AVX2 intrinsics]) ],
 [ AC_MSG_RESULT(no)]
)
CXXFLAGS="$TEMP_CXXFLAGS"


TEMP_CXXFLAGS="$CXXFLAGS"
CXXFLAGS="$CXXFLAGS $SHANI_CXXFLAGS"
AC_MSG_CHECKING(for SHA-NI intrinsics)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
    #include <stdint.h>
    #include <immintrin.h>
  ]],[[
    __m128i i = _mm_set1_epi32(0);
    __m128i j = _mm_set1_epi32(1);
    __m128i k = _mm_set1_epi32(2);
    return _mm_extract_epi32(_mm_sha256rnds2_epu32(i, i, k), 0);
  ]])],
 [ AC_MSG_RESULT(yes); enable_shani=yes; AC_DEFINE(ENABLE_SHANI, 1, [Define this symbol to build code that uses SHA-NI intrinsics]) ],
 [ AC_MSG_RESULT(no)]
)
CXXFLAGS="$TEMP_CXXFLAGS"

CORE_CPPFLAGS="$CORE_CPPFLAGS -DHAVE_BUILD_INFO"

AC_ARG_WITH([utils],
  [AS_HELP_STRING([--with-utils],
  [build tapyrus-cli tapyrus-tx tapyrus-genesis (default=yes)])],
  [build_tapyrus_utils=$withval],
  [build_tapyrus_utils=yes])

AC_ARG_WITH([libs],
  [AS_HELP_STRING([--with-libs],
  [build libraries (default=yes)])],
  [build_tapyrus_libs=$withval],
  [build_tapyrus_libs=yes])

AC_ARG_WITH([daemon],
  [AS_HELP_STRING([--with-daemon],
  [build tapyrusd daemon (default=yes)])],
  [build_tapyrusd=$withval],
  [build_tapyrusd=yes])

use_pkgconfig=yes
case $host in
  *mingw*)

     #pkgconfig does more harm than good with MinGW
     use_pkgconfig=no

     TARGET_OS=windows
     AC_CHECK_LIB([mingwthrd],      [main],, AC_MSG_ERROR(libmingwthrd missing))
     AC_CHECK_LIB([kernel32],      [main],, AC_MSG_ERROR(libkernel32 missing))
     AC_CHECK_LIB([user32],      [main],, AC_MSG_ERROR(libuser32 missing))
     AC_CHECK_LIB([gdi32],      [main],, AC_MSG_ERROR(libgdi32 missing))
     AC_CHECK_LIB([comdlg32],      [main],, AC_MSG_ERROR(libcomdlg32 missing))
     AC_CHECK_LIB([winspool],      [main],, AC_MSG_ERROR(libwinspool missing))
     AC_CHECK_LIB([winmm],      [main],, AC_MSG_ERROR(libwinmm missing))
     AC_CHECK_LIB([shell32],      [main],, AC_MSG_ERROR(libshell32 missing))
     AC_CHECK_LIB([comctl32],      [main],, AC_MSG_ERROR(libcomctl32 missing))
     AC_CHECK_LIB([ole32],      [main],, AC_MSG_ERROR(libole32 missing))
     AC_CHECK_LIB([oleaut32],      [main],, AC_MSG_ERROR(liboleaut32 missing))
     AC_CHECK_LIB([uuid],      [main],, AC_MSG_ERROR(libuuid missing))
     AC_CHECK_LIB([rpcrt4],      [main],, AC_MSG_ERROR(librpcrt4 missing))
     AC_CHECK_LIB([advapi32],      [main],, AC_MSG_ERROR(libadvapi32 missing))
     AC_CHECK_LIB([ws2_32],      [main],, AC_MSG_ERROR(libws2_32 missing))
     AC_CHECK_LIB([mswsock],      [main],, AC_MSG_ERROR(libmswsock missing))
     AC_CHECK_LIB([shlwapi],      [main],, AC_MSG_ERROR(libshlwapi missing))
     AC_CHECK_LIB([iphlpapi],      [main],, AC_MSG_ERROR(libiphlpapi missing))
     AC_CHECK_LIB([crypt32],      [main],, AC_MSG_ERROR(libcrypt32 missing))

     # -static is interpreted by libtool, where it has a different meaning.
     # In libtool-speak, it's -all-static.
     AX_CHECK_LINK_FLAG([[-static]],[LIBTOOL_APP_LDFLAGS="$LIBTOOL_APP_LDFLAGS -all-static"])

     AC_PATH_PROG([MAKENSIS], [makensis], none)
     if test x$MAKENSIS = xnone; then
       AC_MSG_WARN("makensis not found. Cannot create installer.")
     fi

     AC_PATH_TOOL(WINDRES, windres, none)
     if test x$WINDRES = xnone; then
       AC_MSG_ERROR("windres not found")
     fi

     CORE_CPPFLAGS="$CORE_CPPFLAGS -DSECP256K1_STATIC"

     CORE_CPPFLAGS="$CORE_CPPFLAGS -D_MT -DWIN32 -D_WINDOWS -D_WIN32_WINNT=0x0601 -D_WIN32_IE=0x0501 -DWIN32_LEAN_AND_MEAN"
     dnl Prevent the definition of min/max macros.
     dnl We always want to use the standard library.
     CORE_CPPFLAGS="$CORE_CPPFLAGS -DNOMINMAX"

     dnl libtool insists upon adding -nostdlib and a list of objects/libs to link against.
     dnl That breaks our ability to build dll's with static libgcc/libstdc++/libssp. Override
     dnl its command here, with the predeps/postdeps removed, and -static inserted. Postdeps are
     dnl also overridden to prevent their insertion later.
     dnl This should only affect dll's.
     archive_cmds_CXX="\$CC -shared \$libobjs \$deplibs \$compiler_flags -static -o \$output_objdir/\$soname \${wl}--enable-auto-image-base -Xlinker --out-implib -Xlinker \$lib"
     postdeps_CXX=

     dnl We require Windows 7 (NT 6.1) or later
     AX_CHECK_LINK_FLAG([-Wl,--major-subsystem-version -Wl,6 -Wl,--minor-subsystem-version -Wl,1], [CORE_LDFLAGS="$CORE_LDFLAGS -Wl,--major-subsystem-version -Wl,6 -Wl,--minor-subsystem-version -Wl,1"], [], [$LDFLAG_WERROR])

     dnl Avoid the use of aligned vector instructions when building for Windows.
     dnl See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=54412.
     AX_CHECK_COMPILE_FLAG([-Wa,-muse-unaligned-vector-move], [CORE_CXXFLAGS="$CORE_CXXFLAGS -Wa,-muse-unaligned-vector-move"], [], [$CXXFLAG_WERROR])
     ;;
  *darwin*)
     TARGET_OS=darwin
     if  test x$cross_compiling != xyes; then
       BUILD_OS=darwin

       AX_CHECK_LINK_FLAG([-Wl,-headerpad_max_install_names], [CORE_LDFLAGS="$CORE_LDFLAGS -Wl,-headerpad_max_install_names"], [], [$LDFLAG_WERROR])

       AC_CHECK_PROG([BREW], [brew], [brew])
       if test "$BREW" = "brew"; then
         dnl These Homebrew packages may be keg-only, meaning that they won't be found
         dnl in expected paths because they may conflict with system files. Ask
         dnl Homebrew where each one is located, then adjust paths accordingly.
         dnl It's safe to add these paths even if the functionality is disabled by
         dnl the user (--without-wallet or --without-gui for example).

         dnl Homebrew may create symlinks in /usr/local/include for some packages.
         dnl Because MacOS's clang internally adds "-I /usr/local/include" to its search
         dnl paths, this will negate efforts to use -isystem for those packages, as they
         dnl will be found first in /usr/local. Use the internal "-internal-isystem"
         dnl option to system-ify all /usr/local/include paths without adding it to the list
         dnl of search paths in case it's not already there.
         if test "$suppress_external_warnings" != "no"; then
           AX_CHECK_PREPROC_FLAG([-Xclang -internal-isystem -Xclang /usr/local/include/], [CORE_CPPFLAGS="$CORE_CPPFLAGS -Xclang -internal-isystem -Xclang /usr/local/include/"], [], [$CXXFLAG_WERROR])
         fi

         if test "$use_bdb" != "no" && $BREW list --versions berkeley-db@4 >/dev/null && test "$BDB_CFLAGS" = "" && test "$BDB_LIBS" = ""; then
           bdb_prefix=$($BREW --prefix berkeley-db@4 2>/dev/null)
           dnl This must precede the call to BITCOIN_FIND_BDB48 below.
           BDB_CFLAGS="-I$bdb_prefix/include"
           BDB_LIBS="-L$bdb_prefix/lib -ldb_cxx-4.8"
         fi

         if $BREW list --versions qt@5 >/dev/null; then
           export PKG_CONFIG_PATH="$($BREW --prefix qt@5 2>/dev/null)/lib/pkgconfig:$PKG_CONFIG_PATH"
         fi

         case $host in
           *aarch64*)
             dnl The preferred Homebrew prefix for Apple Silicon is /opt/homebrew.
             dnl Therefore, as we do not use pkg-config to detect miniupnpc and libnatpmp
             dnl packages, we should set the CPPFLAGS and LDFLAGS variables for them
             dnl explicitly.
             if test "$use_upnp" != "no" && $BREW list --versions miniupnpc >/dev/null; then
               miniupnpc_prefix=$($BREW --prefix miniupnpc 2>/dev/null)
               if test "$suppress_external_warnings" != "no"; then
                 MINIUPNPC_CPPFLAGS="-isystem $miniupnpc_prefix/include"
               else
                 MINIUPNPC_CPPFLAGS="-I$miniupnpc_prefix/include"
               fi
               MINIUPNPC_LIBS="-L$miniupnpc_prefix/lib"
             fi
             if test "$use_natpmp" != "no" && $BREW list --versions libnatpmp >/dev/null; then
               libnatpmp_prefix=$($BREW --prefix libnatpmp 2>/dev/null)
               if test "$suppress_external_warnings" != "no"; then
                 NATPMP_CPPFLAGS="-isystem $libnatpmp_prefix/include"
               else
                 NATPMP_CPPFLAGS="-I$libnatpmp_prefix/include"
               fi
               NATPMP_LIBS="-L$libnatpmp_prefix/lib"
             fi
             ;;
         esac
       fi

     else
       case $build_os in
         *darwin*)
           BUILD_OS=darwin
           ;;
         *)
           AC_PATH_TOOL([DSYMUTIL], [dsymutil], [dsymutil])
           AC_PATH_PROG([ZIP], [zip], [zip])

           dnl libtool will try to strip the static lib, which is a problem for
           dnl cross-builds because strip attempts to call a hard-coded ld,
           dnl which may not exist in the path. Stripping the .a is not
           dnl necessary, so just disable it.
           old_striplib=
           ;;
       esac
     fi

     CORE_CPPFLAGS="$CORE_CPPFLAGS -DMAC_OSX -DOBJC_OLD_DISPATCH_PROTOTYPES=0"

     dnl ignore deprecated-declarations warnings coming from objcxx code
     dnl "'NSUserNotificationCenter' is deprecated: first deprecated in macOS 11.0".
     OBJCXXFLAGS="$CXXFLAGS -Wno-deprecated-declarations"
     ;;
   *linux*)
     TARGET_OS=linux
     ;;
esac

if test x$use_extended_functional_tests != xno; then
  AC_SUBST(EXTENDED_FUNCTIONAL_TESTS, --extended)
fi

if test x$use_lcov = xyes; then
  if test x$LCOV = x; then
    AC_MSG_ERROR("lcov testing requested but lcov not found")
  fi
  if test x$PYTHON = x; then
    AC_MSG_ERROR("lcov testing requested but python not found")
  fi
  if test x$GENHTML = x; then
    AC_MSG_ERROR("lcov testing requested but genhtml not found")
  fi

  AC_MSG_CHECKING([whether compiler is Clang])
  AC_PREPROC_IFELSE([AC_LANG_SOURCE([[
      #if defined(__clang__) && defined(__llvm__)
      // Compiler is Clang
      #else
      #  error Compiler is not Clang
      #endif
    ]])],[
      AC_MSG_RESULT([yes])
      if test "$LLVM_COV" = ""; then
        AC_MSG_ERROR([lcov testing requested but llvm-cov not found])
      fi
      COV_TOOL="$LLVM_COV gcov"
    ],[
      AC_MSG_RESULT([no])
      if test "$GCOV" = "x"; then
        AC_MSG_ERROR([lcov testing requested but gcov not found])
      fi
      COV_TOOL="$GCOV"
  ])
  AC_SUBST(COV_TOOL)
  AC_SUBST(COV_TOOL_WRAPPER, "cov_tool_wrapper.sh")
  LCOV="$LCOV --gcov-tool $(pwd)/$COV_TOOL_WRAPPER"

  AX_CHECK_LINK_FLAG([--coverage], [CORE_LDFLAGS="$CORE_LDFLAGS --coverage"],
    [AC_MSG_ERROR([lcov testing requested but --coverage linker flag does not work])])
  AX_CHECK_COMPILE_FLAG([--coverage],[CORE_CXXFLAGS="$CORE_CXXFLAGS --coverage"],
    [AC_MSG_ERROR([lcov testing requested but --coverage flag does not work])])
  CORE_CXXFLAGS="$CORE_CXXFLAGS -Og"

  AC_SUBST(LCOV_OPTS)
fi

dnl Check for endianness
AC_C_BIGENDIAN

dnl Check for pthread compile/link requirements
AX_PTHREAD

dnl Check if -latomic is required for <std::atomic>
CHECK_ATOMIC

# The following macro will add the necessary defines to bitcoin-config.h, but
# they also need to be passed down to any subprojects. Pull the results out of
# the cache and add them to CPPFLAGS.
AC_SYS_LARGEFILE
# detect POSIX or GNU variant of strerror_r
AC_FUNC_STRERROR_R

if test x$ac_cv_sys_file_offset_bits != x &&
   test x$ac_cv_sys_file_offset_bits != xno &&
   test x$ac_cv_sys_file_offset_bits != xunknown; then
  CPPFLAGS="$CPPFLAGS -D_FILE_OFFSET_BITS=$ac_cv_sys_file_offset_bits"
fi

if test x$ac_cv_sys_large_files != x &&
   test x$ac_cv_sys_large_files != xno &&
   test x$ac_cv_sys_large_files != xunknown; then
  CPPFLAGS="$CPPFLAGS -D_LARGE_FILES=$ac_cv_sys_large_files"
fi

if test "$TARGET_OS" != "windows"; then
  dnl All windows code is PIC, forcing it on just adds useless compile warnings
  AX_CHECK_COMPILE_FLAG([-fPIC], [PIC_FLAGS="-fPIC"])
fi

if test x$use_hardening != xno; then
 AX_CHECK_COMPILE_FLAG([-Wstack-protector], [HARDENED_CXXFLAGS="$HARDENED_CXXFLAGS -Wstack-protector"])
  AX_CHECK_COMPILE_FLAG([-fstack-protector-all], [HARDENED_CXXFLAGS="$HARDENED_CXXFLAGS -fstack-protector-all"])

  AX_CHECK_COMPILE_FLAG([-fcf-protection=full], [HARDENED_CXXFLAGS="$HARDENED_CXXFLAGS -fcf-protection=full"])

  case $host in
    *mingw*)
      dnl stack-clash-protection doesn't compile with GCC 10 and earlier.
      dnl In any case, it is a no-op for Windows.
      dnl See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=90458 for more details.
      ;;
    *)
      AX_CHECK_COMPILE_FLAG([-fstack-clash-protection], [HARDENED_CXXFLAGS="$HARDENED_CXXFLAGS -fstack-clash-protection"], [], [$CXXFLAG_WERROR])
      ;;
  esac

  case $host in
    *aarch64*)
      AX_CHECK_COMPILE_FLAG([-mbranch-protection=bti], [HARDENED_CXXFLAGS="$HARDENED_CXXFLAGS -mbranch-protection=bti"])
    ;;
  esac

  dnl When enable_debug is yes, all optimizations are disabled.
  dnl However, FORTIFY_SOURCE requires that there is some level of optimization, otherwise it does nothing and just creates a compiler warning.
  dnl Since FORTIFY_SOURCE is a no-op without optimizations, do not enable it when enable_debug is yes.
  if test "$enable_debug" != "yes"; then
    AX_CHECK_PREPROC_FLAG([-D_FORTIFY_SOURCE=3],[
      AX_CHECK_PREPROC_FLAG([-U_FORTIFY_SOURCE],[
        HARDENED_CPPFLAGS="$HARDENED_CPPFLAGS -U_FORTIFY_SOURCE"
      ])
      HARDENED_CPPFLAGS="$HARDENED_CPPFLAGS -D_FORTIFY_SOURCE=3"
    ])
  fi

  AX_CHECK_LINK_FLAG([-Wl,--enable-reloc-section], [HARDENED_LDFLAGS="$HARDENED_LDFLAGS -Wl,--enable-reloc-section"], [], [$LDFLAG_WERROR])
  AX_CHECK_LINK_FLAG([-Wl,--dynamicbase], [HARDENED_LDFLAGS="$HARDENED_LDFLAGS -Wl,--dynamicbase"], [], [$LDFLAG_WERROR])
  AX_CHECK_LINK_FLAG([-Wl,--nxcompat], [HARDENED_LDFLAGS="$HARDENED_LDFLAGS -Wl,--nxcompat"], [], [$LDFLAG_WERROR])
  AX_CHECK_LINK_FLAG([-Wl,--high-entropy-va], [HARDENED_LDFLAGS="$HARDENED_LDFLAGS -Wl,--high-entropy-va"], [], [$LDFLAG_WERROR])
  AX_CHECK_LINK_FLAG([-Wl,-z,relro], [HARDENED_LDFLAGS="$HARDENED_LDFLAGS -Wl,-z,relro"], [], [$LDFLAG_WERROR])
  AX_CHECK_LINK_FLAG([-Wl,-z,now], [HARDENED_LDFLAGS="$HARDENED_LDFLAGS -Wl,-z,now"], [], [$LDFLAG_WERROR])
  AX_CHECK_LINK_FLAG([-Wl,-z,separate-code], [HARDENED_LDFLAGS="$HARDENED_LDFLAGS -Wl,-z,separate-code"], [], [$LDFLAG_WERROR])
  AX_CHECK_LINK_FLAG([-fPIE -pie], [PIE_FLAGS="-fPIE"; HARDENED_LDFLAGS="$HARDENED_LDFLAGS -pie"], [], [$CXXFLAG_WERROR])
fi

dnl These flags are specific to ld64, and may cause issues with other linkers.
dnl For example: GNU ld will interpret -dead_strip as -de and then try and use
dnl "ad_strip" as the symbol for the entry point.
if test "$TARGET_OS" = "darwin"; then
  AX_CHECK_LINK_FLAG([-Wl,-dead_strip], [CORE_LDFLAGS="$CORE_LDFLAGS -Wl,-dead_strip"], [], [$LDFLAG_WERROR])
  AX_CHECK_LINK_FLAG([-Wl,-dead_strip_dylibs], [CORE_LDFLAGS="$CORE_LDFLAGS -Wl,-dead_strip_dylibs"], [], [$LDFLAG_WERROR])
  AX_CHECK_LINK_FLAG([-Wl,-fixup_chains], [HARDENED_LDFLAGS="$HARDENED_LDFLAGS -Wl,-fixup_chains"], [], [$LDFLAG_WERROR])
fi


AC_CHECK_HEADERS([endian.h sys/endian.h byteswap.h stdio.h stdlib.h unistd.h strings.h sys/types.h sys/stat.h sys/select.h sys/prctl.h])

# Check for daemon(3), unrelated to --with-daemon (although used by it)
AC_CHECK_DECLS([daemon])
AC_CHECK_DECLS([fork])
AC_CHECK_DECLS([setsid])

AC_CHECK_DECLS([pipe2])

dnl Check for malloc_info (for memory statistics information in getmemoryinfo)
AC_MSG_CHECKING(for getmemoryinfo)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <malloc.h>]],
 [[ int f = malloc_info(0, NULL); ]])],
 [ AC_MSG_RESULT(yes); AC_DEFINE(HAVE_MALLOC_INFO, 1,[Define this symbol if you have malloc_info]) ],
 [ AC_MSG_RESULT(no)]
)

dnl Check for mallopt(M_ARENA_MAX) (to set glibc arenas)
AC_MSG_CHECKING(for mallopt M_ARENA_MAX)
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <malloc.h>]],
 [[ mallopt(M_ARENA_MAX, 1); ]])],
 [ AC_MSG_RESULT(yes); AC_DEFINE(HAVE_MALLOPT_ARENA_MAX, 1,[Define this symbol if you have mallopt with M_ARENA_MAX]) ],
 [ AC_MSG_RESULT(no)]
)

TEMP_LDFLAGS="$LDFLAGS"
LDFLAGS="$TEMP_LDFLAGS $PTHREAD_CFLAGS"
AC_MSG_CHECKING([for thread_local support])
AC_LINK_IFELSE([AC_LANG_SOURCE([
  #include <thread>
  static thread_local int foo = 0;
  static void run_thread() { foo++;}
  int main(){
  for(int i = 0; i < 10; i++) { std::thread(run_thread).detach();}
  return foo;
  }
  ])],
  [
    AC_DEFINE(HAVE_THREAD_LOCAL,1,[Define if thread_local is supported.])
    AC_MSG_RESULT(yes)
  ],
  [
    AC_MSG_RESULT(no)
  ]
)
LDFLAGS="$TEMP_LDFLAGS"

dnl Check for different ways of gathering OS randomness
AC_MSG_CHECKING([for Linux getrandom function])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
  #include <sys/random.h>]],
 [[ getrandom(nullptr, 32, 0); ]])],
 [ AC_MSG_RESULT([yes]); AC_DEFINE([HAVE_GETRANDOM], [1], [Define this symbol if the Linux getrandom function call is available]) ],
 [ AC_MSG_RESULT([no])]
)

AC_MSG_CHECKING([for getentropy via sys/random.h])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
 #include <sys/random.h>]],
 [[ getentropy(nullptr, 32) ]])],
 [ AC_MSG_RESULT([yes]); AC_DEFINE([HAVE_GETENTROPY_RAND], [1], [Define this symbol if the BSD getentropy system call is available with sys/random.h]) ],
 [ AC_MSG_RESULT([no])]
)

AC_MSG_CHECKING([for sysctl])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
  #include <sys/sysctl.h>]],
 [[ #ifdef __linux__
    #error "Don't use sysctl on Linux, it's deprecated even when it works"
    #endif
    sysctl(nullptr, 2, nullptr, nullptr, nullptr, 0); ]])],
 [ AC_MSG_RESULT([yes]); AC_DEFINE([HAVE_SYSCTL], [1], [Define this symbol if the BSD sysctl() is available]) ],
 [ AC_MSG_RESULT([no])]
)

AC_MSG_CHECKING([for sysctl KERN_ARND])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <sys/types.h>
  #include <sys/sysctl.h>]],
 [[ #ifdef __linux__
    #error "Don't use sysctl on Linux, it's deprecated even when it works"
    #endif
    static int name[2] = {CTL_KERN, KERN_ARND};
    sysctl(name, 2, nullptr, nullptr, nullptr, 0); ]])],
 [ AC_MSG_RESULT([yes]); AC_DEFINE([HAVE_SYSCTL_ARND], [1], [Define this symbol if the BSD sysctl(KERN_ARND) is available]) ],
 [ AC_MSG_RESULT([no])]
)

dnl SUPPRESSED_CPPFLAGS=SUPPRESS_WARNINGS([$SOME_CPPFLAGS])
dnl Replace -I with -isystem in $SOME_CPPFLAGS to suppress warnings from
dnl headers from its include directories and return the result.
dnl See -isystem documentation:
dnl https://gcc.gnu.org/onlinedocs/gcc/Directory-Options.html
dnl https://clang.llvm.org/docs/ClangCommandLineReference.html#cmdoption-clang-isystem-directory
dnl Do not change "-I/usr/include" to "-isystem /usr/include" because that
dnl is not necessary (/usr/include is already a system directory) and because
dnl it would break GCC's #include_next.
AC_DEFUN([SUPPRESS_WARNINGS],
         [[$(echo $1 |${SED} -E -e 's/(^| )-I/\1-isystem /g' -e 's;-isystem /usr/include/*( |$);-I/usr/include\1;g')]])


if test x$enable_wallet != xno; then
    dnl Check for libdb_cxx only if wallet enabled
    BITCOIN_FIND_BDB48
    if test "$suppress_external_warnings" != "no" ; then
        BDB_CPPFLAGS=SUPPRESS_WARNINGS($BDB_CPPFLAGS)
    fi
fi

if test "$use_usdt" != "no"; then
  AC_MSG_CHECKING([whether Userspace, Statically Defined Tracing tracepoints are supported])
  AC_COMPILE_IFELSE([
    AC_LANG_PROGRAM(
      [#include <sys/sdt.h>],
      [DTRACE_PROBE("context", "event");]
    )],
    [AC_MSG_RESULT([yes]); AC_DEFINE([ENABLE_TRACING], [1], [Define to 1 to enable tracepoints for Userspace, Statically Defined Tracing])],
    [AC_MSG_RESULT([no]); use_usdt=no;]
  )
fi
AM_CONDITIONAL([ENABLE_USDT_TRACEPOINTS], [test "$use_usdt" = "yes"])

dnl Check for libminiupnpc (optional)
if test "$use_upnp" != "no"; then
  TEMP_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="$CPPFLAGS $MINIUPNPC_CPPFLAGS"
  AC_CHECK_HEADERS([miniupnpc/miniupnpc.h miniupnpc/upnpcommands.h miniupnpc/upnperrors.h], [], [have_miniupnpc=no])

  if test "$have_miniupnpc" != "no"; then
    AC_CHECK_LIB([miniupnpc], [upnpDiscover], [MINIUPNPC_LIBS="$MINIUPNPC_LIBS -lminiupnpc"], [have_miniupnpc=no], [$MINIUPNPC_LIBS])

    dnl The minimum supported miniUPnPc API version is set to 17. This excludes
    dnl versions with known vulnerabilities.
    AC_MSG_CHECKING([whether miniUPnPc API version is supported])
    AC_PREPROC_IFELSE([AC_LANG_PROGRAM([[
        @%:@include <miniupnpc/miniupnpc.h>
      ]], [[
        #if MINIUPNPC_API_VERSION >= 17
        // Everything is okay
        #else
        #  error miniUPnPc API version is too old
        #endif
      ]])],[
        AC_MSG_RESULT([yes])
      ],[
      AC_MSG_RESULT([no])
      AC_MSG_WARN([miniUPnPc API version < 17 is unsupported, disabling UPnP support.])
      have_miniupnpc=no
    ])
  fi
  CPPFLAGS="$TEMP_CPPFLAGS"
fi

dnl Check for libnatpmp (optional).
if test "$use_natpmp" != "no"; then
  TEMP_CPPFLAGS="$CPPFLAGS"
  CPPFLAGS="$CPPFLAGS $NATPMP_CPPFLAGS"
  AC_CHECK_HEADERS([natpmp.h], [], [have_natpmp=no])

  if test "$have_natpmp" != "no"; then
    AC_CHECK_LIB([natpmp], [initnatpmp], [NATPMP_LIBS="$NATPMP_LIBS -lnatpmp"], [have_natpmp=no], [$NATPMP_LIBS])
  fi

  CPPFLAGS="$TEMP_CPPFLAGS"
fi

BITCOIN_QT_INIT

dnl sets $tapyrus_enable_qt, $tapyrus_enable_qt_test, $tapyrus_enable_qt_dbus
BITCOIN_QT_CONFIGURE

dnl Keep a copy of the original $QT_INCLUDES and use it when invoking qt's moc
QT_INCLUDES_UNSUPPRESSED=$QT_INCLUDES
if test "$suppress_external_warnings" != "no" ; then
    QT_INCLUDES=SUPPRESS_WARNINGS($QT_INCLUDES)
    QT_DBUS_INCLUDES=SUPPRESS_WARNINGS($QT_DBUS_INCLUDES)
    QT_TEST_INCLUDES=SUPPRESS_WARNINGS($QT_TEST_INCLUDES)
fi

if test x$build_tapyrus_utils$build_tapyrusd$tapyrus_enable_qt$use_tests$use_bench = xnonononono; then
    use_boost=no
else
    use_boost=yes
fi


if test "$use_reduce_exports" = "yes"; then
  AX_CHECK_COMPILE_FLAG([-fvisibility=hidden], [CORE_CXXFLAGS="$CORE_CXXFLAGS -fvisibility=hidden"],
  [AC_MSG_ERROR([Cannot set hidden symbol visibility. Use --disable-reduce-exports.])], [$CXXFLAG_WERROR])
  AX_CHECK_LINK_FLAG([-Wl,--exclude-libs,ALL], [RELDFLAGS="-Wl,--exclude-libs,ALL"], [], [$LDFLAG_WERROR])
  AX_CHECK_LINK_FLAG([-Wl,-no_exported_symbols], [LIBTOOL_APP_LDFLAGS="$LIBTOOL_APP_LDFLAGS -Wl,-no_exported_symbols"], [], [$LDFLAG_WERROR])
fi

if test x$use_tests = xyes; then

  if test x$HEXDUMP = x; then
    AC_MSG_ERROR(hexdump is required for tests)
  fi

fi

if test x$use_boost = xyes; then
  dnl Check for Boost headers
  AX_BOOST_BASE([1.73.0],[],[AC_MSG_ERROR([Boost is not available!])])
  if test "x$want_boost" = "xno"; then
    AC_MSG_ERROR([Boost is required])
  fi

  dnl we don't use multi_index serialization
  BOOST_CPPFLAGS="$BOOST_CPPFLAGS -DBOOST_MULTI_INDEX_DISABLE_SERIALIZATION"

  dnl Prevent use of std::unary_function, which was removed in C++17,
  dnl and will generate warnings with newer compilers for Boost
  dnl older than 1.80.
  dnl See: https://github.com/boostorg/config/pull/430.
  AX_CHECK_PREPROC_FLAG([-DBOOST_NO_CXX98_FUNCTION_BASE], [BOOST_CPPFLAGS="$BOOST_CPPFLAGS -DBOOST_NO_CXX98_FUNCTION_BASE"], [], [$CXXFLAG_WERROR],
                        [AC_LANG_PROGRAM([[#include <boost/config.hpp>]])])

  if test "x$suppress_external_warnings" != "xno"; then
    BOOST_CPPFLAGS=SUPPRESS_WARNINGS($BOOST_CPPFLAGS)
  fi
fi

dnl libevent check
use_libevent=no
if test "$build_bitcoin_cli$build_bitcoind$bitcoin_enable_qt$enable_fuzz_binary$use_tests$use_bench" != "nononononono"; then
  PKG_CHECK_MODULES([EVENT], [libevent >= 2.1.8], [use_libevent=yes], [AC_MSG_ERROR([libevent version 2.1.8 or greater not found.])])
  if test "$TARGET_OS" != "windows"; then
    PKG_CHECK_MODULES([EVENT_PTHREADS], [libevent_pthreads >= 2.1.8], [], [AC_MSG_ERROR([libevent_pthreads version 2.1.8 or greater not found.])])
  fi

  if test "$suppress_external_warnings" != "no"; then
    EVENT_CFLAGS=SUPPRESS_WARNINGS($EVENT_CFLAGS)
  fi
fi

if test x$use_libevent = xyes; then
  TEMP_CXXFLAGS="$CXXFLAGS"
  CXXFLAGS="$CXXFLAGS $EVENT_CFLAGS"
  AC_MSG_CHECKING([if evhttp_connection_get_peer expects const char**])
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
      #include <cstdint>
      #include <event2/http.h>
    ]], [[
      evhttp_connection *conn = (evhttp_connection *)1;
      const char *host;
      uint16_t port;

      evhttp_connection_get_peer(conn, &host, &port);
    ]])],
    [ AC_MSG_RESULT([yes]); AC_DEFINE([HAVE_EVHTTP_CONNECTION_GET_PEER_CONST_CHAR], [1], [Define this symbol if evhttp_connection_get_peer expects const char**]) ],
    [ AC_MSG_RESULT([no]) ]
  )
  CXXFLAGS="$TEMP_CXXFLAGS"
fi

dnl QR Code encoding library check

if test "$use_qr" != "no"; then
  BITCOIN_QT_CHECK([PKG_CHECK_MODULES([QR], [libqrencode], [have_qrencode=yes], [have_qrencode=no])])
fi

dnl ZMQ check

if test "$use_zmq" = "yes"; then
  PKG_CHECK_MODULES([ZMQ], [libzmq >= 4],
    AC_DEFINE([ENABLE_ZMQ], [1], [Define this symbol to enable ZMQ functions]),
    [AC_MSG_WARN([libzmq version 4.x or greater not found, disabling])
    use_zmq=no])
fi

if test "$use_zmq" = "yes"; then
  dnl Assume libzmq was built for static linking
  case $host in
    *mingw*)
      ZMQ_CFLAGS="$ZMQ_CFLAGS -DZMQ_STATIC"
    ;;
  esac
fi

AM_CONDITIONAL([ENABLE_ZMQ], [test "$use_zmq" = "yes"])


AC_MSG_CHECKING([whether to build tapyrusd])
AM_CONDITIONAL([BUILD_BITCOIND], [test x$build_tapyrusd = xyes])
AC_MSG_RESULT($build_tapyrusd)

AC_MSG_CHECKING([whether to build utils (tapyrus-cli tapyrus-tx tapyrus-genesis)])
AM_CONDITIONAL([BUILD_BITCOIN_UTILS], [test x$build_tapyrus_utils = xyes])
AC_MSG_RESULT($build_tapyrus_utils)

AC_MSG_CHECKING([whether to build libraries])
AM_CONDITIONAL([BUILD_BITCOIN_LIBS], [test x$build_tapyrus_libs = xyes])
if test x$build_tapyrus_libs = xyes; then
  AC_DEFINE(HAVE_CONSENSUS_LIB, 1, [Define this symbol if the consensus lib has been built])
  AC_CONFIG_FILES([libtapyrusconsensus.pc:libtapyrusconsensus.pc.in])
fi
AC_MSG_RESULT($build_tapyrus_libs)

AC_LANG_POP

if test "x$use_ccache" != "xno"; then
  AC_MSG_CHECKING(if ccache should be used)
  if test x$CCACHE = x; then
    if test "x$use_ccache" = "xyes"; then
      AC_MSG_ERROR([ccache not found.]);
    else
      use_ccache=no
    fi
  else
    use_ccache=yes
    CC="$ac_cv_path_CCACHE $CC"
    CXX="$ac_cv_path_CCACHE $CXX"
  fi
  AC_MSG_RESULT($use_ccache)
fi
if test "x$use_ccache" = "xyes"; then
    AX_CHECK_PREPROC_FLAG([-Qunused-arguments],[CPPFLAGS="-Qunused-arguments $CPPFLAGS"])
fi

dnl enable wallet
AC_MSG_CHECKING([if wallet should be enabled])
if test x$enable_wallet != xno; then
  AC_MSG_RESULT(yes)
  AC_DEFINE_UNQUOTED([ENABLE_WALLET],[1],[Define to 1 to enable wallet functions])

else
  AC_MSG_RESULT(no)
fi

dnl enable upnp support
AC_MSG_CHECKING([whether to build with support for UPnP])
if test x$have_miniupnpc = xno; then
  if test x$use_upnp = xyes; then
     AC_MSG_ERROR("UPnP requested but cannot be built. use --without-miniupnpc")
  fi
  use_upnp=no
  AC_MSG_RESULT(no)
else
  if test x$use_upnp != xno; then
    AC_MSG_RESULT(yes)
    AC_MSG_CHECKING([whether to build with UPnP enabled by default])
    use_upnp=yes
    upnp_setting=0
    if test x$use_upnp_default != xno; then
      use_upnp_default=yes
      upnp_setting=1
    fi
    AC_MSG_RESULT($use_upnp_default)
    AC_DEFINE_UNQUOTED([USE_UPNP],[$upnp_setting],[UPnP support not compiled if undefined, otherwise value (0 or 1) determines default state])
    if test x$TARGET_OS = xwindows; then
      MINIUPNPC_CPPFLAGS="-DSTATICLIB -DMINIUPNP_STATICLIB"
    fi
  else
    use_upnp=no
    AC_MSG_RESULT(no)
  fi
fi

dnl Enable NAT-PMP support.
AC_MSG_CHECKING([whether to build with support for NAT-PMP])
if test "$have_natpmp" = "no"; then
  if test "$use_natpmp" = "yes"; then
     AC_MSG_ERROR([NAT-PMP requested but cannot be built. Use --without-natpmp])
  fi
  AC_MSG_RESULT([no])
  use_natpmp=no
else
  if test "$use_natpmp" != "no"; then
    AC_MSG_RESULT([yes])
    use_natpmp=yes
    AC_DEFINE([USE_NATPMP], [1], [Define to 1 if UPnP support should be compiled in.])
    if test "$TARGET_OS" = "windows"; then
      NATPMP_CPPFLAGS="$NATPMP_CPPFLAGS -DSTATICLIB -DNATPMP_STATICLIB"
    fi
  else
    AC_MSG_RESULT([no])
  fi
fi

dnl these are only used when qt is enabled
BUILD_TEST_QT=""
if test x$tapyrus_enable_qt != xno; then
  dnl enable dbus support
  AC_MSG_CHECKING([whether to build GUI with support for D-Bus])
  if test x$tapyrus_enable_qt_dbus != xno; then
    AC_DEFINE([USE_DBUS],[1],[Define if dbus support should be compiled in])
  fi
  AC_MSG_RESULT($tapyrus_enable_qt_dbus)

  dnl enable qr support
  AC_MSG_CHECKING([whether to build GUI with support for QR codes])
  if test x$have_qrencode = xno; then
    if test x$use_qr = xyes; then
     AC_MSG_ERROR("QR support requested but cannot be built. use --without-qrencode")
    fi
    AC_MSG_RESULT(no)
  else
    if test x$use_qr != xno; then
      AC_MSG_RESULT(yes)
      AC_DEFINE([USE_QRCODE],[1],[Define if QR support should be compiled in])
      use_qr=yes
    else
      AC_MSG_RESULT(no)
    fi
  fi

  if test x$XGETTEXT = x; then
    AC_MSG_WARN("xgettext is required to update qt translations")
  fi

  AC_MSG_CHECKING([whether to build test_tapyrus_qt])
  if test x$use_gui_tests$tapyrus_enable_qt_test = xyesyes; then
    AC_MSG_RESULT([yes])
    BUILD_TEST_QT="yes"
  else
    AC_MSG_RESULT([no])
  fi
fi

AC_MSG_CHECKING([whether to build test_tapyrus])
if test x$use_tests = xyes; then
  AC_MSG_RESULT([yes])
  BUILD_TEST="yes"
else
  AC_MSG_RESULT([no])
  BUILD_TEST=""
fi

AC_MSG_CHECKING([whether to reduce exports])
if test x$use_reduce_exports = xyes; then
  AC_MSG_RESULT([yes])
else
  AC_MSG_RESULT([no])
fi

if test x$build_tapyrus_utils$build_tapyrus_libs$build_tapyrusd$tapyrus_enable_qt$use_bench$use_tests = xnononononono; then
  AC_MSG_ERROR([No targets! Please specify at least one of: --with-utils --with-libs --with-daemon --with-gui --enable-bench or --enable-tests])
fi

AM_CONDITIONAL([TARGET_DARWIN], [test "$TARGET_OS" = "darwin"])
AM_CONDITIONAL([BUILD_DARWIN], [test "$BUILD_OS" = "darwin"])
AM_CONDITIONAL([TARGET_LINUX], [test "$TARGET_OS" = "linux"])
AM_CONDITIONAL([TARGET_WINDOWS], [test "$TARGET_OS" = "windows"])
AM_CONDITIONAL([ENABLE_WALLET], [test "$enable_wallet" = "yes"])
AM_CONDITIONAL([USE_SQLITE], [test "$use_sqlite" = "yes"])
AM_CONDITIONAL([USE_BDB], [test "$use_bdb" = "yes"])
AM_CONDITIONAL([ENABLE_TESTS], [test "$BUILD_TEST" = "yes"])
AM_CONDITIONAL([ENABLE_FUZZ], [test "$enable_fuzz" = "yes"])
AM_CONDITIONAL([ENABLE_FUZZ_BINARY], [test "$enable_fuzz_binary" = "yes"])
AM_CONDITIONAL([ENABLE_QT], [test "$bitcoin_enable_qt" = "yes"])
AM_CONDITIONAL([ENABLE_QT_TESTS], [test "$BUILD_TEST_QT" = "yes"])
AM_CONDITIONAL([ENABLE_BENCH], [test "$use_bench" = "yes"])
AM_CONDITIONAL([USE_QRCODE], [test "$use_qr" = "yes"])
AM_CONDITIONAL([USE_LCOV], [test "$use_lcov" = "yes"])
AM_CONDITIONAL([HARDEN], [test "$use_hardening" = "yes"])
AM_CONDITIONAL([ENABLE_SSE42], [test "$enable_sse42" = "yes"])
AM_CONDITIONAL([ENABLE_SSE41], [test "$enable_sse41" = "yes"])
AM_CONDITIONAL([ENABLE_AVX2], [test "$enable_avx2" = "yes"])
AM_CONDITIONAL([ENABLE_SHANI], [test "$enable_x86_shani" = "yes"])
AM_CONDITIONAL([USE_ASM],[test x$use_asm = xyes])
AM_CONDITIONAL([ENABLE_ARM_CRC], [test "$enable_arm_crc" = "yes"])
AM_CONDITIONAL([WORDS_BIGENDIAN], [test "$ac_cv_c_bigendian" = "yes"])
AM_CONDITIONAL([USE_NATPMP], [test "$use_natpmp" = "yes"])
AM_CONDITIONAL([USE_UPNP], [test "$use_upnp" = "yes"])

AC_DEFINE(CLIENT_VERSION_MAJOR, _CLIENT_VERSION_MAJOR, [Major version])
AC_DEFINE(CLIENT_VERSION_MINOR, _CLIENT_VERSION_MINOR, [Minor version])
AC_DEFINE(CLIENT_VERSION_REVISION, _CLIENT_VERSION_REVISION, [Build revision])
AC_DEFINE(CLIENT_VERSION_BUILD, _CLIENT_VERSION_BUILD, [Version Build])
AC_DEFINE(CLIENT_VERSION_IS_RELEASE, _CLIENT_VERSION_IS_RELEASE, [Version is release])
AC_DEFINE(COPYRIGHT_YEAR, _COPYRIGHT_YEAR, [Copyright year])
AC_DEFINE(COPYRIGHT_HOLDERS, "_COPYRIGHT_HOLDERS", [Copyright holder(s) before %s replacement])
AC_DEFINE(COPYRIGHT_HOLDERS_SUBSTITUTION, "_COPYRIGHT_HOLDERS_SUBSTITUTION", [Replacement for %s in copyright holders string])
define(_COPYRIGHT_HOLDERS_FINAL, [patsubst(_COPYRIGHT_HOLDERS, [%s], [_COPYRIGHT_HOLDERS_SUBSTITUTION])])
AC_DEFINE(COPYRIGHT_HOLDERS_FINAL, "_COPYRIGHT_HOLDERS_FINAL", [Copyright holder(s)])
AC_SUBST(CLIENT_VERSION_MAJOR, _CLIENT_VERSION_MAJOR)
AC_SUBST(CLIENT_VERSION_MINOR, _CLIENT_VERSION_MINOR)
AC_SUBST(CLIENT_VERSION_REVISION, _CLIENT_VERSION_REVISION)
AC_SUBST(CLIENT_VERSION_BUILD, _CLIENT_VERSION_BUILD)
AC_SUBST(CLIENT_VERSION_IS_RELEASE, _CLIENT_VERSION_IS_RELEASE)
AC_SUBST(COPYRIGHT_YEAR, _COPYRIGHT_YEAR)
AC_SUBST(COPYRIGHT_HOLDERS, "_COPYRIGHT_HOLDERS")
AC_SUBST(COPYRIGHT_HOLDERS_SUBSTITUTION, "_COPYRIGHT_HOLDERS_SUBSTITUTION")
AC_SUBST(COPYRIGHT_HOLDERS_FINAL, "_COPYRIGHT_HOLDERS_FINAL")
AC_SUBST(BITCOIN_DAEMON_NAME)
AC_SUBST(BITCOIN_GUI_NAME)
AC_SUBST(BITCOIN_CLI_NAME)
AC_SUBST(BITCOIN_TX_NAME)
AC_SUBST(TAPYRUS_GENESIS_NAME)

AC_SUBST(RELDFLAGS)
AC_SUBST(CORE_LDFLAGS)
AC_SUBST(CORE_CPPFLAGS)
AC_SUBST(CORE_CXXFLAGS)
AC_SUBST(DEBUG_CPPFLAGS)
AC_SUBST(WARN_CXXFLAGS)
AC_SUBST(NOWARN_CXXFLAGS)
AC_SUBST(DEBUG_CXXFLAGS)
AC_SUBST(ERROR_CXXFLAGS)
AC_SUBST(HARDENED_CXXFLAGS)
AC_SUBST(HARDENED_CPPFLAGS)
AC_SUBST(HARDENED_LDFLAGS)
AC_SUBST(PIC_FLAGS)
AC_SUBST(PIE_FLAGS)
AC_SUBST(SANITIZER_CXXFLAGS)
AC_SUBST(SANITIZER_LDFLAGS)
AC_SUBST(SSE42_CXXFLAGS)
AC_SUBST(SSE41_CXXFLAGS)
AC_SUBST(AVX2_CXXFLAGS)
AC_SUBST(X86_SHANI_CXXFLAGS)
AC_SUBST(ARM_CRC_CXXFLAGS)
AC_SUBST(ARM_SHANI_CXXFLAGS)
AC_SUBST(LIBTOOL_APP_LDFLAGS)
AC_SUBST(USE_UPNP)
AC_SUBST(USE_QRCODE)
AC_SUBST(TESTDEFS)
AC_SUBST(MINIUPNPC_CPPFLAGS)
AC_SUBST(MINIUPNPC_LIBS)
AC_CONFIG_FILES([Makefile src/Makefile doc/man/Makefile share/setup.nsi share/qt/Info.plist test/config.ini])
AC_CONFIG_FILES([contrib/devtools/split-debug.sh],[chmod +x contrib/devtools/split-debug.sh])
AM_COND_IF([HAVE_DOXYGEN], [AC_CONFIG_FILES([doc/Doxyfile])])
AC_CONFIG_LINKS([contrib/filter-lcov.py:contrib/filter-lcov.py])
AC_CONFIG_LINKS([test/functional/test_runner.py:test/functional/test_runner.py])
AC_CONFIG_LINKS([test/util/tapyrus-util-test.py:test/util/tapyrus-util-test.py])
AC_CONFIG_LINKS([test/util/rpcauth-test.py:test/util/rpcauth-test.py])

dnl boost's m4 checks do something really nasty: they export these vars. As a
dnl result, they leak into secp256k1's configure and crazy things happen.
dnl Until this is fixed upstream and we've synced, we'll just un-export them.
CPPFLAGS_TEMP="$CPPFLAGS"
unset CPPFLAGS
CPPFLAGS="$CPPFLAGS_TEMP"

LDFLAGS_TEMP="$LDFLAGS"
unset LDFLAGS
LDFLAGS="$LDFLAGS_TEMP"

LIBS_TEMP="$LIBS"
unset LIBS
LIBS="$LIBS_TEMP"

PKGCONFIG_PATH_TEMP="$PKG_CONFIG_PATH"
unset PKG_CONFIG_PATH
PKG_CONFIG_PATH="$PKGCONFIG_PATH_TEMP"

PKGCONFIG_LIBDIR_TEMP="$PKG_CONFIG_LIBDIR"
unset PKG_CONFIG_LIBDIR
PKG_CONFIG_LIBDIR="$PKGCONFIG_LIBDIR_TEMP"

if test x$need_bundled_univalue = xyes; then
  AC_CONFIG_SUBDIRS([src/univalue])
fi

ac_configure_args="${ac_configure_args} --disable-shared --with-pic --with-bignum=no --enable-module-recovery --disable-jni"
AC_CONFIG_SUBDIRS([src/secp256k1])

AC_OUTPUT

dnl An old hack similar to a98356fee to remove hard-coded
dnl bind_at_load flag from libtool
case $host in
  *darwin*)
     AC_MSG_RESULT([Removing -Wl,bind_at_load from libtool.])
     sed < libtool > libtool-2 '/bind_at_load/d'
     mv libtool-2 libtool
     chmod 755 libtool
   ;;
esac

dnl Replace the BUILDDIR path with the correct Windows path if compiling on Native Windows
case ${OS} in
   *Windows*)
     sed  's/BUILDDIR="\/\([[a-z]]\)/BUILDDIR="\1:/'  test/config.ini > test/config-2.ini
     mv test/config-2.ini test/config.ini
   ;;
esac

echo
echo "Options used to compile and link:"
echo "  with wallet   = $enable_wallet"
echo "  with gui / qt = $tapyrus_enable_qt"
if test x$tapyrus_enable_qt != xno; then
    echo "    with qr     = $use_qr"
fi
echo "  with zmq      = $use_zmq"
echo "  with test     = $use_tests"
echo "  with bench    = $use_bench"
echo "  with upnp     = $use_upnp"
echo "  use asm       = $use_asm"
echo "  USDT tracing  = $use_usdt"
echo "  sanitizers    = $use_sanitizers"
echo "  debug enabled = $enable_debug"
echo "  gprof enabled = $enable_gprof"
echo "  werror        = $enable_werror"
echo
echo "  target os     = $TARGET_OS"
echo "  build os      = $BUILD_OS"
echo
echo "  CC            = $CC"
echo "  CFLAGS        = $PTHREAD_CFLAGS $SANITIZER_CFLAGS $CFLAGS"
echo "  CPPFLAGS      = $DEBUG_CPPFLAGS $HARDENED_CPPFLAGS $CORE_CPPFLAGS $CPPFLAGS"
echo "  CXX           = $CXX"
echo "  CXXFLAGS      = $CORE_CXXFLAGS $DEBUG_CXXFLAGS $HARDENED_CXXFLAGS $WARN_CXXFLAGS $NOWARN_CXXFLAGS $ERROR_CXXFLAGS $SANITIZER_CXXFLAGS $CXXFLAGS"
echo "  LDFLAGS       = $PTHREAD_LIBS $HARDENED_LDFLAGS $SANITIZER_LDFLAGS $CORE_LDFLAGS $LDFLAGS"
echo "  AR            = $AR"
echo "  ARFLAGS       = $ARFLAGS"
echo
