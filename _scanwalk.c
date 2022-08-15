/* _scanwalk module implementation */

#define PY_SSIZE_T_CLEAN

#include <Python.h>
// Include <windows.h> before pycore internal headers. FSCTL_GET_REPARSE_POINT
// is not exported by <windows.h> if the WIN32_LEAN_AND_MEAN macro is defined,
// whereas pycore_condvar.h defines the WIN32_LEAN_AND_MEAN macro.
#ifdef MS_WINDOWS
#  include <windows.h>
#  include <pathcch.h>
#endif

#include <structseq.h>
#include <structmember.h>         // PyMemberDef
#ifndef MS_WINDOWS
#  include "_scanwalk.h"
#else
#  include "winreparse.h"
#endif

/*
 * A number of APIs are available on macOS from a certain macOS version.
 * To support building with a new SDK while deploying to older versions
 * the availability test is split into two:
 *   - HAVE_<FUNCTION>:  The configure check for compile time availability
 *   - HAVE_<FUNCTION>_RUNTIME: Runtime check for availability
 *
 * The latter is always true when not on macOS, or when using a compiler
 * that does not support __has_builtin (older versions of Xcode).
 *
 * Due to compiler restrictions there is one valid use of HAVE_<FUNCTION>_RUNTIME:
 *    if (HAVE_<FUNCTION>_RUNTIME) { ... }
 *
 * In mixing the test with other tests or using negations will result in compile
 * errors.
 */
#if defined(__APPLE__)

#if defined(__has_builtin)
#if __has_builtin(__builtin_available)
#define HAVE_BUILTIN_AVAILABLE 1
#endif
#endif

#ifdef HAVE_BUILTIN_AVAILABLE
#  define HAVE_FSTATAT_RUNTIME __builtin_available(macOS 10.10, iOS 8.0, *)
#  define HAVE_FACCESSAT_RUNTIME __builtin_available(macOS 10.10, iOS 8.0, *)
#  define HAVE_FCHMODAT_RUNTIME __builtin_available(macOS 10.10, iOS 8.0, *)
#  define HAVE_FCHOWNAT_RUNTIME __builtin_available(macOS 10.10, iOS 8.0, *)
#  define HAVE_LINKAT_RUNTIME __builtin_available(macOS 10.10, iOS 8.0, *)
#  define HAVE_FDOPENDIR_RUNTIME __builtin_available(macOS 10.10, iOS 8.0, *)
#  define HAVE_MKDIRAT_RUNTIME __builtin_available(macOS 10.10, iOS 8.0, *)
#  define HAVE_RENAMEAT_RUNTIME __builtin_available(macOS 10.10, iOS 8.0, *)
#  define HAVE_UNLINKAT_RUNTIME __builtin_available(macOS 10.10, iOS 8.0, *)
#  define HAVE_OPENAT_RUNTIME __builtin_available(macOS 10.10, iOS 8.0, *)
#  define HAVE_READLINKAT_RUNTIME __builtin_available(macOS 10.10, iOS 8.0, *)
#  define HAVE_SYMLINKAT_RUNTIME __builtin_available(macOS 10.10, iOS 8.0, *)
#  define HAVE_FUTIMENS_RUNTIME __builtin_available(macOS 10.13, iOS 11.0, tvOS 11.0, watchOS 4.0, *)
#  define HAVE_UTIMENSAT_RUNTIME __builtin_available(macOS 10.13, iOS 11.0, tvOS 11.0, watchOS 4.0, *)
#  define HAVE_PWRITEV_RUNTIME __builtin_available(macOS 11.0, iOS 14.0, tvOS 14.0, watchOS 7.0, *)

#  define HAVE_POSIX_SPAWN_SETSID_RUNTIME __builtin_available(macOS 10.15, *)

#else /* Xcode 8 or earlier */

   /* __builtin_available is not present in these compilers, but
    * some of the symbols might be weak linked (10.10 SDK or later
    * deploying on 10.9.
    *
    * Fall back to the older style of availability checking for
    * symbols introduced in macOS 10.10.
    */

#  ifdef HAVE_FSTATAT
#    define HAVE_FSTATAT_RUNTIME (fstatat != NULL)
#  endif

#  ifdef HAVE_FACCESSAT
#    define HAVE_FACCESSAT_RUNTIME (faccessat != NULL)
#  endif

#  ifdef HAVE_FCHMODAT
#    define HAVE_FCHMODAT_RUNTIME (fchmodat != NULL)
#  endif

#  ifdef HAVE_FCHOWNAT
#    define HAVE_FCHOWNAT_RUNTIME (fchownat != NULL)
#  endif

#  ifdef HAVE_LINKAT
#    define HAVE_LINKAT_RUNTIME (linkat != NULL)
#  endif

#  ifdef HAVE_FDOPENDIR
#    define HAVE_FDOPENDIR_RUNTIME (fdopendir != NULL)
#  endif

#  ifdef HAVE_MKDIRAT
#    define HAVE_MKDIRAT_RUNTIME (mkdirat != NULL)
#  endif

#  ifdef HAVE_RENAMEAT
#    define HAVE_RENAMEAT_RUNTIME (renameat != NULL)
#  endif

#  ifdef HAVE_UNLINKAT
#    define HAVE_UNLINKAT_RUNTIME (unlinkat != NULL)
#  endif

#  ifdef HAVE_OPENAT
#    define HAVE_OPENAT_RUNTIME (openat != NULL)
#  endif

#  ifdef HAVE_READLINKAT
#    define HAVE_READLINKAT_RUNTIME (readlinkat != NULL)
#  endif

#  ifdef HAVE_SYMLINKAT
#    define HAVE_SYMLINKAT_RUNTIME (symlinkat != NULL)
#  endif

#endif

#ifdef HAVE_FUTIMESAT
/* Some of the logic for weak linking depends on this assertion */
# error "HAVE_FUTIMESAT unexpectedly defined"
#endif

#else
#  define HAVE_FSTATAT_RUNTIME 1
#  define HAVE_FACCESSAT_RUNTIME 1
#  define HAVE_FCHMODAT_RUNTIME 1
#  define HAVE_FCHOWNAT_RUNTIME 1
#  define HAVE_LINKAT_RUNTIME 1
#  define HAVE_FDOPENDIR_RUNTIME 1
#  define HAVE_MKDIRAT_RUNTIME 1
#  define HAVE_RENAMEAT_RUNTIME 1
#  define HAVE_UNLINKAT_RUNTIME 1
#  define HAVE_OPENAT_RUNTIME 1
#  define HAVE_READLINKAT_RUNTIME 1
#  define HAVE_SYMLINKAT_RUNTIME 1
#  define HAVE_FUTIMENS_RUNTIME 1
#  define HAVE_UTIMENSAT_RUNTIME 1
#  define HAVE_PWRITEV_RUNTIME 1
#endif


#ifdef __cplusplus
extern "C" {
#endif

PyDoc_STRVAR(_scanwalk__doc__,
"This module provides scandir() and supportting data structures.");

#ifdef HAVE_FCNTL_H
#  include <fcntl.h>
#endif

#ifdef HAVE_DIRENT_H
#  include <dirent.h>
#  define NAMLEN(dirent) strlen((dirent)->d_name)
#else
#  if defined(__WATCOMC__) && !defined(__QNX__)
#    include <direct.h>
#    define NAMLEN(dirent) strlen((dirent)->d_name)
#  else
#    define dirent direct
#    define NAMLEN(dirent) (dirent)->d_namlen
#  endif
#  ifdef HAVE_SYS_NDIR_H
#    include <sys/ndir.h>
#  endif
#  ifdef HAVE_SYS_DIR_H
#    include <sys/dir.h>
#  endif
#  ifdef HAVE_NDIR_H
#    include <ndir.h>
#  endif
#endif

#ifndef MAXPATHLEN
#  if defined(PATH_MAX) && PATH_MAX > 1024
#    define MAXPATHLEN PATH_MAX
#  else
#    define MAXPATHLEN 1024
#  endif
#endif /* MAXPATHLEN */

/* choose the appropriate stat and fstat functions and return structs */
#undef STAT
#undef FSTAT
#undef STRUCT_STAT
#ifdef MS_WINDOWS
#  define STAT win32_stat
#  define LSTAT win32_lstat
#  define FSTAT _Py_fstat_noraise
#  define STRUCT_STAT struct _Py_stat_struct
#else
#  define STAT stat
#  define LSTAT lstat
#  define FSTAT fstat
#  define STRUCT_STAT struct stat
#endif

#  define INITFUNC PyInit__scanwalk
#  define MODNAME "_scanwalk"

#if defined(__sun)
/* Something to implement in autoconf, not present in autoconf 2.69 */
#  define HAVE_STRUCT_STAT_ST_FSTYPE 1
#endif

#ifdef _Py_MEMORY_SANITIZER
#  include <sanitizer/msan_interface.h>
#endif

#ifndef MS_WINDOWS
PyObject *
_PyLong_FromUid(uid_t uid)
{
    if (uid == (uid_t)-1)
        return PyLong_FromLong(-1);
    return PyLong_FromUnsignedLong(uid);
}

PyObject *
_PyLong_FromGid(gid_t gid)
{
    if (gid == (gid_t)-1)
        return PyLong_FromLong(-1);
    return PyLong_FromUnsignedLong(gid);
}

int
_Py_Uid_Converter(PyObject *obj, uid_t *p)
{
    uid_t uid;
    PyObject *index;
    int overflow;
    long result;
    unsigned long uresult;

    index = PyNumber_Index(obj);
    if (index == NULL) {
        PyErr_Format(PyExc_TypeError,
                     "uid should be integer, not %.200s",
                     _PyType_Name(Py_TYPE(obj)));
        return 0;
    }

    /*
     * Handling uid_t is complicated for two reasons:
     *  * Although uid_t is (always?) unsigned, it still
     *    accepts -1.
     *  * We don't know its size in advance--it may be
     *    bigger than an int, or it may be smaller than
     *    a long.
     *
     * So a bit of defensive programming is in order.
     * Start with interpreting the value passed
     * in as a signed long and see if it works.
     */

    result = PyLong_AsLongAndOverflow(index, &overflow);

    if (!overflow) {
        uid = (uid_t)result;

        if (result == -1) {
            if (PyErr_Occurred())
                goto fail;
            /* It's a legitimate -1, we're done. */
            goto success;
        }

        /* Any other negative number is disallowed. */
        if (result < 0)
            goto underflow;

        /* Ensure the value wasn't truncated. */
        if (sizeof(uid_t) < sizeof(long) &&
            (long)uid != result)
            goto underflow;
        goto success;
    }

    if (overflow < 0)
        goto underflow;

    /*
     * Okay, the value overflowed a signed long.  If it
     * fits in an *unsigned* long, it may still be okay,
     * as uid_t may be unsigned long on this platform.
     */
    uresult = PyLong_AsUnsignedLong(index);
    if (PyErr_Occurred()) {
        if (PyErr_ExceptionMatches(PyExc_OverflowError))
            goto overflow;
        goto fail;
    }

    uid = (uid_t)uresult;

    /*
     * If uid == (uid_t)-1, the user actually passed in ULONG_MAX,
     * but this value would get interpreted as (uid_t)-1  by chown
     * and its siblings.   That's not what the user meant!  So we
     * throw an overflow exception instead.   (We already
     * handled a real -1 with PyLong_AsLongAndOverflow() above.)
     */
    if (uid == (uid_t)-1)
        goto overflow;

    /* Ensure the value wasn't truncated. */
    if (sizeof(uid_t) < sizeof(long) &&
        (unsigned long)uid != uresult)
        goto overflow;
    /* fallthrough */

success:
    Py_DECREF(index);
    *p = uid;
    return 1;

underflow:
    PyErr_SetString(PyExc_OverflowError,
                    "uid is less than minimum");
    goto fail;

overflow:
    PyErr_SetString(PyExc_OverflowError,
                    "uid is greater than maximum");
    /* fallthrough */

fail:
    Py_DECREF(index);
    return 0;
}

int
_Py_Gid_Converter(PyObject *obj, gid_t *p)
{
    gid_t gid;
    PyObject *index;
    int overflow;
    long result;
    unsigned long uresult;

    index = PyNumber_Index(obj);
    if (index == NULL) {
        PyErr_Format(PyExc_TypeError,
                     "gid should be integer, not %.200s",
                     _PyType_Name(Py_TYPE(obj)));
        return 0;
    }

    /*
     * Handling gid_t is complicated for two reasons:
     *  * Although gid_t is (always?) unsigned, it still
     *    accepts -1.
     *  * We don't know its size in advance--it may be
     *    bigger than an int, or it may be smaller than
     *    a long.
     *
     * So a bit of defensive programming is in order.
     * Start with interpreting the value passed
     * in as a signed long and see if it works.
     */

    result = PyLong_AsLongAndOverflow(index, &overflow);

    if (!overflow) {
        gid = (gid_t)result;

        if (result == -1) {
            if (PyErr_Occurred())
                goto fail;
            /* It's a legitimate -1, we're done. */
            goto success;
        }

        /* Any other negative number is disallowed. */
        if (result < 0) {
            goto underflow;
        }

        /* Ensure the value wasn't truncated. */
        if (sizeof(gid_t) < sizeof(long) &&
            (long)gid != result)
            goto underflow;
        goto success;
    }

    if (overflow < 0)
        goto underflow;

    /*
     * Okay, the value overflowed a signed long.  If it
     * fits in an *unsigned* long, it may still be okay,
     * as gid_t may be unsigned long on this platform.
     */
    uresult = PyLong_AsUnsignedLong(index);
    if (PyErr_Occurred()) {
        if (PyErr_ExceptionMatches(PyExc_OverflowError))
            goto overflow;
        goto fail;
    }

    gid = (gid_t)uresult;

    /*
     * If gid == (gid_t)-1, the user actually passed in ULONG_MAX,
     * but this value would get interpreted as (gid_t)-1  by chown
     * and its siblings.   That's not what the user meant!  So we
     * throw an overflow exception instead.   (We already
     * handled a real -1 with PyLong_AsLongAndOverflow() above.)
     */
    if (gid == (gid_t)-1)
        goto overflow;

    /* Ensure the value wasn't truncated. */
    if (sizeof(gid_t) < sizeof(long) &&
        (unsigned long)gid != uresult)
        goto overflow;
    /* fallthrough */

success:
    Py_DECREF(index);
    *p = gid;
    return 1;

underflow:
    PyErr_SetString(PyExc_OverflowError,
                    "gid is less than minimum");
    goto fail;

overflow:
    PyErr_SetString(PyExc_OverflowError,
                    "gid is greater than maximum");
    /* fallthrough */

fail:
    Py_DECREF(index);
    return 0;
}
#endif /* MS_WINDOWS */


#define _PyLong_FromDev PyLong_FromLongLong

#ifdef AT_FDCWD
/*
 * Why the (int) cast?  Solaris 10 defines AT_FDCWD as 0xffd19553 (-3041965);
 * without the int cast, the value gets interpreted as uint (4291925331),
 * which doesn't play nicely with all the initializer lines in this file that
 * look like this:
 *      int dir_fd = DEFAULT_DIR_FD;
 */
#define DEFAULT_DIR_FD (int)AT_FDCWD
#else
#define DEFAULT_DIR_FD (-100)
#endif

static int
_fd_converter(PyObject *o, int *p)
{
    int overflow;
    long long_value;

    PyObject *index = PyNumber_Index(o);
    if (index == NULL) {
        return 0;
    }

    assert(PyLong_Check(index));
    long_value = PyLong_AsLongAndOverflow(index, &overflow);
    Py_DECREF(index);
    assert(!PyErr_Occurred());
    if (overflow > 0 || long_value > INT_MAX) {
        PyErr_SetString(PyExc_OverflowError,
                        "fd is greater than maximum");
        return 0;
    }
    if (overflow < 0 || long_value < INT_MIN) {
        PyErr_SetString(PyExc_OverflowError,
                        "fd is less than minimum");
        return 0;
    }

    *p = (int)long_value;
    return 1;
}

static int
dir_fd_converter(PyObject *o, void *p)
{
    if (o == Py_None) {
        *(int *)p = DEFAULT_DIR_FD;
        return 1;
    }
    else if (PyIndex_Check(o)) {
        return _fd_converter(o, (int *)p);
    }
    else {
        PyErr_Format(PyExc_TypeError,
                     "argument should be integer or None, not %.200s",
                     _PyType_Name(Py_TYPE(o)));
        return 0;
    }
}

typedef struct {
    PyObject *billion;
    PyObject *DirEntryType;
    PyObject *ScandirIteratorType;
    PyObject *StatResultType;
    PyObject *st_mode;
} _scanwalkstate;


static inline _scanwalkstate*
get_scanwalk_state(PyObject *module)
{
    void *state = PyModule_GetState(module);
    assert(state != NULL);
    return (_scanwalkstate *)state;
}

/*
 * A PyArg_ParseTuple "converter" function
 * that handles filesystem paths in the manner
 * preferred by the os module.
 *
 * path_converter accepts (Unicode) strings and their
 * subclasses, and bytes and their subclasses.  What
 * it does with the argument depends on the platform:
 *
 *   * On Windows, if we get a (Unicode) string we
 *     extract the wchar_t * and return it; if we get
 *     bytes we decode to wchar_t * and return that.
 *
 *   * On all other platforms, strings are encoded
 *     to bytes using PyUnicode_FSConverter, then we
 *     extract the char * from the bytes object and
 *     return that.
 *
 * path_converter also optionally accepts signed
 * integers (representing open file descriptors) instead
 * of path strings.
 *
 * Input fields:
 *   path.nullable
 *     If nonzero, the path is permitted to be None.
 *   path.allow_fd
 *     If nonzero, the path is permitted to be a file handle
 *     (a signed int) instead of a string.
 *   path.function_name
 *     If non-NULL, path_converter will use that as the name
 *     of the function in error messages.
 *     (If path.function_name is NULL it omits the function name.)
 *   path.argument_name
 *     If non-NULL, path_converter will use that as the name
 *     of the parameter in error messages.
 *     (If path.argument_name is NULL it uses "path".)
 *
 * Output fields:
 *   path.wide
 *     Points to the path if it was expressed as Unicode
 *     and was not encoded.  (Only used on Windows.)
 *   path.narrow
 *     Points to the path if it was expressed as bytes,
 *     or it was Unicode and was encoded to bytes. (On Windows,
 *     is a non-zero integer if the path was expressed as bytes.
 *     The type is deliberately incompatible to prevent misuse.)
 *   path.fd
 *     Contains a file descriptor if path.accept_fd was true
 *     and the caller provided a signed integer instead of any
 *     sort of string.
 *
 *     WARNING: if your "path" parameter is optional, and is
 *     unspecified, path_converter will never get called.
 *     So if you set allow_fd, you *MUST* initialize path.fd = -1
 *     yourself!
 *   path.length
 *     The length of the path in characters, if specified as
 *     a string.
 *   path.object
 *     The original object passed in (if get a PathLike object,
 *     the result of PyOS_FSPath() is treated as the original object).
 *     Own a reference to the object.
 *   path.cleanup
 *     For internal use only.  May point to a temporary object.
 *     (Pay no attention to the man behind the curtain.)
 *
 *   At most one of path.wide or path.narrow will be non-NULL.
 *   If path was None and path.nullable was set,
 *     or if path was an integer and path.allow_fd was set,
 *     both path.wide and path.narrow will be NULL
 *     and path.length will be 0.
 *
 *   path_converter takes care to not write to the path_t
 *   unless it's successful.  However it must reset the
 *   "cleanup" field each time it's called.
 *
 * Use as follows:
 *      path_t path;
 *      memset(&path, 0, sizeof(path));
 *      PyArg_ParseTuple(args, "O&", path_converter, &path);
 *      // ... use values from path ...
 *      path_cleanup(&path);
 *
 * (Note that if PyArg_Parse fails you don't need to call
 * path_cleanup().  However it is safe to do so.)
 */
typedef struct {
    const char *function_name;
    const char *argument_name;
    int nullable;
    int allow_fd;
    const wchar_t *wide;
#ifdef MS_WINDOWS
    BOOL narrow;
#else
    const char *narrow;
#endif
    int fd;
    Py_ssize_t length;
    PyObject *object;
    PyObject *cleanup;
} path_t;

#ifdef MS_WINDOWS
#define PATH_T_INITIALIZE(function_name, argument_name, nullable, allow_fd) \
    {function_name, argument_name, nullable, allow_fd, NULL, FALSE, -1, 0, NULL, NULL}
#else
#define PATH_T_INITIALIZE(function_name, argument_name, nullable, allow_fd) \
    {function_name, argument_name, nullable, allow_fd, NULL, NULL, -1, 0, NULL, NULL}
#endif

static void
path_cleanup(path_t *path)
{
    wchar_t *wide = (wchar_t *)path->wide;
    path->wide = NULL;
    PyMem_Free(wide);
    Py_CLEAR(path->object);
    Py_CLEAR(path->cleanup);
}

static int
path_converter(PyObject *o, void *p)
{
    path_t *path = (path_t *)p;
    PyObject *bytes = NULL;
    Py_ssize_t length = 0;
    int is_index, is_buffer, is_bytes, is_unicode;
    const char *narrow;
#ifdef MS_WINDOWS
    PyObject *wo = NULL;
    wchar_t *wide = NULL;
#endif

#define FORMAT_EXCEPTION(exc, fmt) \
    PyErr_Format(exc, "%s%s" fmt, \
        path->function_name ? path->function_name : "", \
        path->function_name ? ": "                : "", \
        path->argument_name ? path->argument_name : "path")

    /* Py_CLEANUP_SUPPORTED support */
    if (o == NULL) {
        path_cleanup(path);
        return 1;
    }

    /* Ensure it's always safe to call path_cleanup(). */
    path->object = path->cleanup = NULL;
    /* path->object owns a reference to the original object */
    Py_INCREF(o);

    if ((o == Py_None) && path->nullable) {
        path->wide = NULL;
#ifdef MS_WINDOWS
        path->narrow = FALSE;
#else
        path->narrow = NULL;
#endif
        path->fd = -1;
        goto success_exit;
    }

    /* Only call this here so that we don't treat the return value of
       os.fspath() as an fd or buffer. */
    is_index = path->allow_fd && PyIndex_Check(o);
    is_buffer = PyObject_CheckBuffer(o);
    is_bytes = PyBytes_Check(o);
    is_unicode = PyUnicode_Check(o);

    if (!is_index && !is_buffer && !is_unicode && !is_bytes) {
        /* Inline PyOS_FSPath() for better error messages. */
        PyObject *func, *res;

        func = PyObject_GetAttrString(o, "__fspath__");
        if (NULL == func) {
            goto error_format;
        }
        res = PyObject_CallNoArgs(func);
        Py_DECREF(func);
        if (NULL == res) {
            goto error_exit;
        }
        else if (PyUnicode_Check(res)) {
            is_unicode = 1;
        }
        else if (PyBytes_Check(res)) {
            is_bytes = 1;
        }
        else {
            PyErr_Format(PyExc_TypeError,
                 "expected %.200s.__fspath__() to return str or bytes, "
                 "not %.200s", _PyType_Name(Py_TYPE(o)),
                 _PyType_Name(Py_TYPE(res)));
            Py_DECREF(res);
            goto error_exit;
        }

        /* still owns a reference to the original object */
        Py_DECREF(o);
        o = res;
    }

    if (is_unicode) {
#ifdef MS_WINDOWS
        wide = PyUnicode_AsWideCharString(o, &length);
        if (!wide) {
            goto error_exit;
        }
        if (length > 32767) {
            FORMAT_EXCEPTION(PyExc_ValueError, "%s too long for Windows");
            goto error_exit;
        }
        if (wcslen(wide) != length) {
            FORMAT_EXCEPTION(PyExc_ValueError, "embedded null character in %s");
            goto error_exit;
        }

        path->wide = wide;
        path->narrow = FALSE;
        path->fd = -1;
        wide = NULL;
        goto success_exit;
#else
        if (!PyUnicode_FSConverter(o, &bytes)) {
            goto error_exit;
        }
#endif
    }
    else if (is_bytes) {
        bytes = o;
        Py_INCREF(bytes);
    }
    else if (is_buffer) {
        /* XXX Replace PyObject_CheckBuffer with PyBytes_Check in other code
           after removing support of non-bytes buffer objects. */
        if (PyErr_WarnFormat(PyExc_DeprecationWarning, 1,
            "%s%s%s should be %s, not %.200s",
            path->function_name ? path->function_name : "",
            path->function_name ? ": "                : "",
            path->argument_name ? path->argument_name : "path",
            path->allow_fd && path->nullable ? "string, bytes, os.PathLike, "
                                               "integer or None" :
            path->allow_fd ? "string, bytes, os.PathLike or integer" :
            path->nullable ? "string, bytes, os.PathLike or None" :
                             "string, bytes or os.PathLike",
            _PyType_Name(Py_TYPE(o)))) {
            goto error_exit;
        }
        bytes = PyBytes_FromObject(o);
        if (!bytes) {
            goto error_exit;
        }
    }
    else if (is_index) {
        if (!_fd_converter(o, &path->fd)) {
            goto error_exit;
        }
        path->wide = NULL;
#ifdef MS_WINDOWS
        path->narrow = FALSE;
#else
        path->narrow = NULL;
#endif
        goto success_exit;
    }
    else {
 error_format:
        PyErr_Format(PyExc_TypeError, "%s%s%s should be %s, not %.200s",
            path->function_name ? path->function_name : "",
            path->function_name ? ": "                : "",
            path->argument_name ? path->argument_name : "path",
            path->allow_fd && path->nullable ? "string, bytes, os.PathLike, "
                                               "integer or None" :
            path->allow_fd ? "string, bytes, os.PathLike or integer" :
            path->nullable ? "string, bytes, os.PathLike or None" :
                             "string, bytes or os.PathLike",
            _PyType_Name(Py_TYPE(o)));
        goto error_exit;
    }

    length = PyBytes_GET_SIZE(bytes);
    narrow = PyBytes_AS_STRING(bytes);
    if ((size_t)length != strlen(narrow)) {
        FORMAT_EXCEPTION(PyExc_ValueError, "embedded null character in %s");
        goto error_exit;
    }

#ifdef MS_WINDOWS
    wo = PyUnicode_DecodeFSDefaultAndSize(
        narrow,
        length
    );
    if (!wo) {
        goto error_exit;
    }

    wide = PyUnicode_AsWideCharString(wo, &length);
    Py_DECREF(wo);
    if (!wide) {
        goto error_exit;
    }
    if (length > 32767) {
        FORMAT_EXCEPTION(PyExc_ValueError, "%s too long for Windows");
        goto error_exit;
    }
    if (wcslen(wide) != length) {
        FORMAT_EXCEPTION(PyExc_ValueError, "embedded null character in %s");
        goto error_exit;
    }
    path->wide = wide;
    path->narrow = TRUE;
    Py_DECREF(bytes);
    wide = NULL;
#else
    path->wide = NULL;
    path->narrow = narrow;
    if (bytes == o) {
        /* Still a reference owned by path->object, don't have to
           worry about path->narrow is used after free. */
        Py_DECREF(bytes);
    }
    else {
        path->cleanup = bytes;
    }
#endif
    path->fd = -1;

 success_exit:
    path->length = length;
    path->object = o;
    return Py_CLEANUP_SUPPORTED;

 error_exit:
    Py_XDECREF(o);
    Py_XDECREF(bytes);
#ifdef MS_WINDOWS
    PyMem_Free(wide);
#endif
    return 0;
}

static void
argument_unavailable_error(const char *function_name, const char *argument_name)
{
    PyErr_Format(PyExc_NotImplementedError,
        "%s%s%s unavailable on this platform",
        (function_name != NULL) ? function_name : "",
        (function_name != NULL) ? ": ": "",
        argument_name);
}

static int
dir_fd_unavailable(PyObject *o, void *p)
{
    int dir_fd;
    if (!dir_fd_converter(o, &dir_fd))
        return 0;
    if (dir_fd != DEFAULT_DIR_FD) {
        argument_unavailable_error(NULL, "dir_fd");
        return 0;
    }
    *(int *)p = dir_fd;
    return 1;
}

static PyObject *
posix_path_object_error(PyObject *path)
{
    return PyErr_SetFromErrnoWithFilenameObject(PyExc_OSError, path);
}

static PyObject *
path_object_error(PyObject *path)
{
#ifdef MS_WINDOWS
    return PyErr_SetExcFromWindowsErrWithFilenameObject(
                PyExc_OSError, 0, path);
#else
    return posix_path_object_error(path);
#endif
}

static PyObject *
path_error(path_t *path)
{
    return path_object_error(path->object);
}

/* POSIX generic methods */

#ifdef MS_WINDOWS
/* The CRT of Windows has a number of flaws wrt. its stat() implementation:
   - time stamps are restricted to second resolution
   - file modification times suffer from forth-and-back conversions between
     UTC and local time
   Therefore, we implement our own stat, based on the Win32 API directly.
*/
#define HAVE_STAT_NSEC 1
#define HAVE_STRUCT_STAT_ST_FILE_ATTRIBUTES 1
#define HAVE_STRUCT_STAT_ST_REPARSE_TAG 1

static void
find_data_to_file_info(WIN32_FIND_DATAW *pFileData,
                       BY_HANDLE_FILE_INFORMATION *info,
                       ULONG *reparse_tag)
{
    memset(info, 0, sizeof(*info));
    info->dwFileAttributes = pFileData->dwFileAttributes;
    info->ftCreationTime   = pFileData->ftCreationTime;
    info->ftLastAccessTime = pFileData->ftLastAccessTime;
    info->ftLastWriteTime  = pFileData->ftLastWriteTime;
    info->nFileSizeHigh    = pFileData->nFileSizeHigh;
    info->nFileSizeLow     = pFileData->nFileSizeLow;
/*  info->nNumberOfLinks   = 1; */
    if (pFileData->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
        *reparse_tag = pFileData->dwReserved0;
    else
        *reparse_tag = 0;
}

static BOOL
attributes_from_dir(LPCWSTR pszFile, BY_HANDLE_FILE_INFORMATION *info, ULONG *reparse_tag)
{
    HANDLE hFindFile;
    WIN32_FIND_DATAW FileData;
    LPCWSTR filename = pszFile;
    size_t n = wcslen(pszFile);
    if (n && (pszFile[n - 1] == L'\\' || pszFile[n - 1] == L'/')) {
        // cannot use PyMem_Malloc here because we do not hold the GIL
        filename = (LPCWSTR)malloc((n + 1) * sizeof(filename[0]));
        wcsncpy_s((LPWSTR)filename, n + 1, pszFile, n);
        while (--n > 0 && (filename[n] == L'\\' || filename[n] == L'/')) {
            ((LPWSTR)filename)[n] = L'\0';
        }
        if (!n || (n == 1 && filename[1] == L':')) {
            // Nothing left to query
            free((void *)filename);
            return FALSE;
        }
    }
    hFindFile = FindFirstFileW(filename, &FileData);
    if (pszFile != filename) {
        free((void *)filename);
    }
    if (hFindFile == INVALID_HANDLE_VALUE) {
        return FALSE;
    }
    FindClose(hFindFile);
    find_data_to_file_info(&FileData, info, reparse_tag);
    return TRUE;
}

static int
win32_xstat_impl(const wchar_t *path, struct _Py_stat_struct *result,
                 BOOL traverse)
{
    HANDLE hFile;
    BY_HANDLE_FILE_INFORMATION fileInfo;
    FILE_ATTRIBUTE_TAG_INFO tagInfo = { 0 };
    DWORD fileType, error;
    BOOL isUnhandledTag = FALSE;
    int retval = 0;

    DWORD access = FILE_READ_ATTRIBUTES;
    DWORD flags = FILE_FLAG_BACKUP_SEMANTICS; /* Allow opening directories. */
    if (!traverse) {
        flags |= FILE_FLAG_OPEN_REPARSE_POINT;
    }

    hFile = CreateFileW(path, access, 0, NULL, OPEN_EXISTING, flags, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        /* Either the path doesn't exist, or the caller lacks access. */
        error = GetLastError();
        switch (error) {
        case ERROR_ACCESS_DENIED:     /* Cannot sync or read attributes. */
        case ERROR_SHARING_VIOLATION: /* It's a paging file. */
            /* Try reading the parent directory. */
            if (!attributes_from_dir(path, &fileInfo, &tagInfo.ReparseTag)) {
                /* Cannot read the parent directory. */
                switch (GetLastError()) {
                case ERROR_FILE_NOT_FOUND: /* File cannot be found */
                case ERROR_PATH_NOT_FOUND: /* File parent directory cannot be found */
                case ERROR_NOT_READY: /* Drive exists but unavailable */
                case ERROR_BAD_NET_NAME: /* Remote drive unavailable */
                    break;
                /* Restore the error from CreateFileW(). */
                default:
                    SetLastError(error);
                }

                return -1;
            }
            if (fileInfo.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
                if (traverse ||
                    !IsReparseTagNameSurrogate(tagInfo.ReparseTag)) {
                    /* The stat call has to traverse but cannot, so fail. */
                    SetLastError(error);
                    return -1;
                }
            }
            break;

        case ERROR_INVALID_PARAMETER:
            /* \\.\con requires read or write access. */
            hFile = CreateFileW(path, access | GENERIC_READ,
                        FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                        OPEN_EXISTING, flags, NULL);
            if (hFile == INVALID_HANDLE_VALUE) {
                SetLastError(error);
                return -1;
            }
            break;

        case ERROR_CANT_ACCESS_FILE:
            /* bpo37834: open unhandled reparse points if traverse fails. */
            if (traverse) {
                traverse = FALSE;
                isUnhandledTag = TRUE;
                hFile = CreateFileW(path, access, 0, NULL, OPEN_EXISTING,
                            flags | FILE_FLAG_OPEN_REPARSE_POINT, NULL);
            }
            if (hFile == INVALID_HANDLE_VALUE) {
                SetLastError(error);
                return -1;
            }
            break;

        default:
            return -1;
        }
    }

    if (hFile != INVALID_HANDLE_VALUE) {
        /* Handle types other than files on disk. */
        fileType = GetFileType(hFile);
        if (fileType != FILE_TYPE_DISK) {
            if (fileType == FILE_TYPE_UNKNOWN && GetLastError() != 0) {
                retval = -1;
                goto cleanup;
            }
            DWORD fileAttributes = GetFileAttributesW(path);
            memset(result, 0, sizeof(*result));
            if (fileAttributes != INVALID_FILE_ATTRIBUTES &&
                fileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                /* \\.\pipe\ or \\.\mailslot\ */
                result->st_mode = _S_IFDIR;
            } else if (fileType == FILE_TYPE_CHAR) {
                /* \\.\nul */
                result->st_mode = _S_IFCHR;
            } else if (fileType == FILE_TYPE_PIPE) {
                /* \\.\pipe\spam */
                result->st_mode = _S_IFIFO;
            }
            /* FILE_TYPE_UNKNOWN, e.g. \\.\mailslot\waitfor.exe\spam */
            goto cleanup;
        }

        /* Query the reparse tag, and traverse a non-link. */
        if (!traverse) {
            if (!GetFileInformationByHandleEx(hFile, FileAttributeTagInfo,
                    &tagInfo, sizeof(tagInfo))) {
                /* Allow devices that do not support FileAttributeTagInfo. */
                switch (GetLastError()) {
                case ERROR_INVALID_PARAMETER:
                case ERROR_INVALID_FUNCTION:
                case ERROR_NOT_SUPPORTED:
                    tagInfo.FileAttributes = FILE_ATTRIBUTE_NORMAL;
                    tagInfo.ReparseTag = 0;
                    break;
                default:
                    retval = -1;
                    goto cleanup;
                }
            } else if (tagInfo.FileAttributes &
                         FILE_ATTRIBUTE_REPARSE_POINT) {
                if (IsReparseTagNameSurrogate(tagInfo.ReparseTag)) {
                    if (isUnhandledTag) {
                        /* Traversing previously failed for either this link
                           or its target. */
                        SetLastError(ERROR_CANT_ACCESS_FILE);
                        retval = -1;
                        goto cleanup;
                    }
                /* Traverse a non-link, but not if traversing already failed
                   for an unhandled tag. */
                } else if (!isUnhandledTag) {
                    CloseHandle(hFile);
                    return win32_xstat_impl(path, result, TRUE);
                }
            }
        }

        if (!GetFileInformationByHandle(hFile, &fileInfo)) {
            switch (GetLastError()) {
            case ERROR_INVALID_PARAMETER:
            case ERROR_INVALID_FUNCTION:
            case ERROR_NOT_SUPPORTED:
                /* Volumes and physical disks are block devices, e.g.
                   \\.\C: and \\.\PhysicalDrive0. */
                memset(result, 0, sizeof(*result));
                result->st_mode = 0x6000; /* S_IFBLK */
                goto cleanup;
            }
            retval = -1;
            goto cleanup;
        }
    }

    _Py_attribute_data_to_stat(&fileInfo, tagInfo.ReparseTag, result);

    if (!(fileInfo.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
        /* Fix the file execute permissions. This hack sets S_IEXEC if
           the filename has an extension that is commonly used by files
           that CreateProcessW can execute. A real implementation calls
           GetSecurityInfo, OpenThreadToken/OpenProcessToken, and
           AccessCheck to check for generic read, write, and execute
           access. */
        const wchar_t *fileExtension = wcsrchr(path, '.');
        if (fileExtension) {
            if (_wcsicmp(fileExtension, L".exe") == 0 ||
                _wcsicmp(fileExtension, L".bat") == 0 ||
                _wcsicmp(fileExtension, L".cmd") == 0 ||
                _wcsicmp(fileExtension, L".com") == 0) {
                result->st_mode |= 0111;
            }
        }
    }

cleanup:
    if (hFile != INVALID_HANDLE_VALUE) {
        /* Preserve last error if we are failing */
        error = retval ? GetLastError() : 0;
        if (!CloseHandle(hFile)) {
            retval = -1;
        } else if (retval) {
            /* Restore last error */
            SetLastError(error);
        }
    }

    return retval;
}

static int
win32_xstat(const wchar_t *path, struct _Py_stat_struct *result, BOOL traverse)
{
    /* Protocol violation: we explicitly clear errno, instead of
       setting it to a POSIX error. Callers should use GetLastError. */
    int code = win32_xstat_impl(path, result, traverse);
    errno = 0;
    return code;
}
/* About the following functions: win32_lstat_w, win32_stat, win32_stat_w

   In Posix, stat automatically traverses symlinks and returns the stat
   structure for the target.  In Windows, the equivalent GetFileAttributes by
   default does not traverse symlinks and instead returns attributes for
   the symlink.

   Instead, we will open the file (which *does* traverse symlinks by default)
   and GetFileInformationByHandle(). */

static int
win32_lstat(const wchar_t* path, struct _Py_stat_struct *result)
{
    return win32_xstat(path, result, FALSE);
}

static int
win32_stat(const wchar_t* path, struct _Py_stat_struct *result)
{
    return win32_xstat(path, result, TRUE);
}

#endif /* MS_WINDOWS */

#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
#define ST_BLKSIZE_IDX 16
#else
#define ST_BLKSIZE_IDX 15
#endif

#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
#define ST_BLOCKS_IDX (ST_BLKSIZE_IDX+1)
#else
#define ST_BLOCKS_IDX ST_BLKSIZE_IDX
#endif

#ifdef HAVE_STRUCT_STAT_ST_RDEV
#define ST_RDEV_IDX (ST_BLOCKS_IDX+1)
#else
#define ST_RDEV_IDX ST_BLOCKS_IDX
#endif

#ifdef HAVE_STRUCT_STAT_ST_FLAGS
#define ST_FLAGS_IDX (ST_RDEV_IDX+1)
#else
#define ST_FLAGS_IDX ST_RDEV_IDX
#endif

#ifdef HAVE_STRUCT_STAT_ST_GEN
#define ST_GEN_IDX (ST_FLAGS_IDX+1)
#else
#define ST_GEN_IDX ST_FLAGS_IDX
#endif

#ifdef HAVE_STRUCT_STAT_ST_BIRTHTIME
#define ST_BIRTHTIME_IDX (ST_GEN_IDX+1)
#else
#define ST_BIRTHTIME_IDX ST_GEN_IDX
#endif

#ifdef HAVE_STRUCT_STAT_ST_FILE_ATTRIBUTES
#define ST_FILE_ATTRIBUTES_IDX (ST_BIRTHTIME_IDX+1)
#else
#define ST_FILE_ATTRIBUTES_IDX ST_BIRTHTIME_IDX
#endif

#ifdef HAVE_STRUCT_STAT_ST_FSTYPE
#define ST_FSTYPE_IDX (ST_FILE_ATTRIBUTES_IDX+1)
#else
#define ST_FSTYPE_IDX ST_FILE_ATTRIBUTES_IDX
#endif

#ifdef HAVE_STRUCT_STAT_ST_REPARSE_TAG
#define ST_REPARSE_TAG_IDX (ST_FSTYPE_IDX+1)
#else
#define ST_REPARSE_TAG_IDX ST_FSTYPE_IDX
#endif

static int
_scanwalk_clear(PyObject *module)
{
    _scanwalkstate *state = get_scanwalk_state(module);
    Py_CLEAR(state->billion);
    Py_CLEAR(state->DirEntryType);
    Py_CLEAR(state->ScandirIteratorType);
    Py_CLEAR(state->StatResultType);
    Py_CLEAR(state->st_mode);
    return 0;
}

static int
_scanwalk_traverse(PyObject *module, visitproc visit, void *arg)
{
    _scanwalkstate *state = get_scanwalk_state(module);
    Py_VISIT(state->billion);
    Py_VISIT(state->DirEntryType);
    Py_VISIT(state->ScandirIteratorType);
    Py_VISIT(state->StatResultType);
    Py_VISIT(state->st_mode);
    return 0;
}

static void
_scanwalk_free(void *module)
{
   _scanwalk_clear((PyObject *)module);
}

static void
fill_time(PyObject *module, PyObject *v, int index, time_t sec, unsigned long nsec)
{
    PyObject *s = _PyLong_FromTime_t(sec);
    PyObject *ns_fractional = PyLong_FromUnsignedLong(nsec);
    PyObject *s_in_ns = NULL;
    PyObject *ns_total = NULL;
    PyObject *float_s = NULL;

    if (!(s && ns_fractional))
        goto exit;

    s_in_ns = PyNumber_Multiply(s, get_scanwalk_state(module)->billion);
    if (!s_in_ns)
        goto exit;

    ns_total = PyNumber_Add(s_in_ns, ns_fractional);
    if (!ns_total)
        goto exit;

    float_s = PyFloat_FromDouble(sec + 1e-9*nsec);
    if (!float_s) {
        goto exit;
    }

    PyStructSequence_SET_ITEM(v, index, s);
    PyStructSequence_SET_ITEM(v, index+3, float_s);
    PyStructSequence_SET_ITEM(v, index+6, ns_total);
    s = NULL;
    float_s = NULL;
    ns_total = NULL;
exit:
    Py_XDECREF(s);
    Py_XDECREF(ns_fractional);
    Py_XDECREF(s_in_ns);
    Py_XDECREF(ns_total);
    Py_XDECREF(float_s);
}

/* pack a system stat C structure into the Python stat tuple
   (used by posix_stat() and posix_fstat()) */
static PyObject*
_pystat_fromstructstat(PyObject *module, STRUCT_STAT *st)
{
    unsigned long ansec, mnsec, cnsec;
    PyObject *StatResultType = get_scanwalk_state(module)->StatResultType;
    PyObject *v = PyStructSequence_New((PyTypeObject *)StatResultType);
    if (v == NULL)
        return NULL;

    PyStructSequence_SET_ITEM(v, 0, PyLong_FromLong((long)st->st_mode));
    static_assert(sizeof(unsigned long long) >= sizeof(st->st_ino),
                  "stat.st_ino is larger than unsigned long long");
    PyStructSequence_SET_ITEM(v, 1, PyLong_FromUnsignedLongLong(st->st_ino));
#ifdef MS_WINDOWS
    PyStructSequence_SET_ITEM(v, 2, PyLong_FromUnsignedLong(st->st_dev));
#else
    PyStructSequence_SET_ITEM(v, 2, _PyLong_FromDev(st->st_dev));
#endif
    PyStructSequence_SET_ITEM(v, 3, PyLong_FromLong((long)st->st_nlink));
#if defined(MS_WINDOWS)
    PyStructSequence_SET_ITEM(v, 4, PyLong_FromLong(0));
    PyStructSequence_SET_ITEM(v, 5, PyLong_FromLong(0));
#else
    PyStructSequence_SET_ITEM(v, 4, _PyLong_FromUid(st->st_uid));
    PyStructSequence_SET_ITEM(v, 5, _PyLong_FromGid(st->st_gid));
#endif
    static_assert(sizeof(long long) >= sizeof(st->st_size),
                  "stat.st_size is larger than long long");
    PyStructSequence_SET_ITEM(v, 6, PyLong_FromLongLong(st->st_size));

#if defined(HAVE_STAT_TV_NSEC)
    ansec = st->st_atim.tv_nsec;
    mnsec = st->st_mtim.tv_nsec;
    cnsec = st->st_ctim.tv_nsec;
#elif defined(HAVE_STAT_TV_NSEC2)
    ansec = st->st_atimespec.tv_nsec;
    mnsec = st->st_mtimespec.tv_nsec;
    cnsec = st->st_ctimespec.tv_nsec;
#elif defined(HAVE_STAT_NSEC)
    ansec = st->st_atime_nsec;
    mnsec = st->st_mtime_nsec;
    cnsec = st->st_ctime_nsec;
#else
    ansec = mnsec = cnsec = 0;
#endif
    fill_time(module, v, 7, st->st_atime, ansec);
    fill_time(module, v, 8, st->st_mtime, mnsec);
    fill_time(module, v, 9, st->st_ctime, cnsec);

#ifdef HAVE_STRUCT_STAT_ST_BLKSIZE
    PyStructSequence_SET_ITEM(v, ST_BLKSIZE_IDX,
                              PyLong_FromLong((long)st->st_blksize));
#endif
#ifdef HAVE_STRUCT_STAT_ST_BLOCKS
    PyStructSequence_SET_ITEM(v, ST_BLOCKS_IDX,
                              PyLong_FromLong((long)st->st_blocks));
#endif
#ifdef HAVE_STRUCT_STAT_ST_RDEV
    PyStructSequence_SET_ITEM(v, ST_RDEV_IDX,
                              PyLong_FromLong((long)st->st_rdev));
#endif
#ifdef HAVE_STRUCT_STAT_ST_GEN
    PyStructSequence_SET_ITEM(v, ST_GEN_IDX,
                              PyLong_FromLong((long)st->st_gen));
#endif
#ifdef HAVE_STRUCT_STAT_ST_BIRTHTIME
    {
      PyObject *val;
      unsigned long bsec,bnsec;
      bsec = (long)st->st_birthtime;
#ifdef HAVE_STAT_TV_NSEC2
      bnsec = st->st_birthtimespec.tv_nsec;
#else
      bnsec = 0;
#endif
      val = PyFloat_FromDouble(bsec + 1e-9*bnsec);
      PyStructSequence_SET_ITEM(v, ST_BIRTHTIME_IDX,
                                val);
    }
#endif
#ifdef HAVE_STRUCT_STAT_ST_FLAGS
    PyStructSequence_SET_ITEM(v, ST_FLAGS_IDX,
                              PyLong_FromLong((long)st->st_flags));
#endif
#ifdef HAVE_STRUCT_STAT_ST_FILE_ATTRIBUTES
    PyStructSequence_SET_ITEM(v, ST_FILE_ATTRIBUTES_IDX,
                              PyLong_FromUnsignedLong(st->st_file_attributes));
#endif
#ifdef HAVE_STRUCT_STAT_ST_FSTYPE
   PyStructSequence_SET_ITEM(v, ST_FSTYPE_IDX,
                              PyUnicode_FromString(st->st_fstype));
#endif
#ifdef HAVE_STRUCT_STAT_ST_REPARSE_TAG
    PyStructSequence_SET_ITEM(v, ST_REPARSE_TAG_IDX,
                              PyLong_FromUnsignedLong(st->st_reparse_tag));
#endif

    if (PyErr_Occurred()) {
        Py_DECREF(v);
        return NULL;
    }

    return v;
}

/* POSIX methods */

/*[python input]

for s in """

FACCESSAT
FCHMODAT
FCHOWNAT
FSTATAT
LINKAT
MKDIRAT
MKFIFOAT
MKNODAT
OPENAT
READLINKAT
SYMLINKAT
UNLINKAT

""".strip().split():
    s = s.strip()
    print("""
#ifdef HAVE_{s}
    #define {s}_DIR_FD_CONVERTER dir_fd_converter
#else
    #define {s}_DIR_FD_CONVERTER dir_fd_unavailable
#endif
""".rstrip().format(s=s))

for s in """

FCHDIR
FCHMOD
FCHOWN
FDOPENDIR
FEXECVE
FPATHCONF
FSTATVFS
FTRUNCATE

""".strip().split():
    s = s.strip()
    print("""
#ifdef HAVE_{s}
    #define PATH_HAVE_{s} 1
#else
    #define PATH_HAVE_{s} 0
#endif

""".rstrip().format(s=s))
[python start generated code]*/

#ifdef HAVE_FACCESSAT
    #define FACCESSAT_DIR_FD_CONVERTER dir_fd_converter
#else
    #define FACCESSAT_DIR_FD_CONVERTER dir_fd_unavailable
#endif

#ifdef HAVE_FSTATAT
    #define FSTATAT_DIR_FD_CONVERTER dir_fd_converter
#else
    #define FSTATAT_DIR_FD_CONVERTER dir_fd_unavailable
#endif

#ifdef HAVE_FDOPENDIR
    #define PATH_HAVE_FDOPENDIR 1
#else
    #define PATH_HAVE_FDOPENDIR 0
#endif

#if defined(MS_WINDOWS)

/* Remove the last portion of the path - return 0 on success */
static int
_dirnameW(WCHAR *path)
{
    WCHAR *ptr;
    size_t length = wcsnlen_s(path, MAX_PATH);
    if (length == MAX_PATH) {
        return -1;
    }

    /* walk the path from the end until a backslash is encountered */
    for(ptr = path + length; ptr != path; ptr--) {
        if (*ptr == L'\\' || *ptr == L'/') {
            break;
        }
    }
    *ptr = 0;
    return 0;
}

#endif

typedef struct {
    PyObject_HEAD
    PyObject *name;
    PyObject *path;
    PyObject *skip;
    PyObject *stat;
    PyObject *lstat;
#ifdef MS_WINDOWS
    struct _Py_stat_struct win32_lstat;
    uint64_t win32_file_index;
    int got_file_index;
#else /* POSIX */
#ifdef HAVE_DIRENT_D_TYPE
    unsigned char d_type;
#endif
    ino_t d_ino;
    int dir_fd;
#endif
} DirEntry;

static PyObject *
_disabled_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
    PyErr_Format(PyExc_TypeError,
        "cannot create '%.100s' instances", _PyType_Name(type));
    return NULL;
}

static void
DirEntry_dealloc(DirEntry *entry)
{
    PyTypeObject *tp = Py_TYPE(entry);
    Py_XDECREF(entry->name);
    Py_XDECREF(entry->path);
    Py_XDECREF(entry->skip);
    Py_XDECREF(entry->stat);
    Py_XDECREF(entry->lstat);
    freefunc free_func = PyType_GetSlot(tp, Py_tp_free);
    free_func(entry);
    Py_DECREF(tp);
}

/* Forward reference */
static int
DirEntry_test_mode(PyTypeObject *defining_class, DirEntry *self,
                   int follow_symlinks, unsigned short mode_bits);

/*[clinic input]
os.DirEntry.is_symlink -> bool
    defining_class: defining_class
    /

Return True if the entry is a symbolic link; cached per entry.
[clinic start generated code]*/

static int
os_DirEntry_is_symlink_impl(DirEntry *self, PyTypeObject *defining_class)
/*[clinic end generated code: output=293096d589b6d47c input=e9acc5ee4d511113]*/
{
#ifdef MS_WINDOWS
    return (self->win32_lstat.st_mode & S_IFMT) == S_IFLNK;
#elif defined(HAVE_DIRENT_D_TYPE)
    /* POSIX */
    if (self->d_type != DT_UNKNOWN)
        return self->d_type == DT_LNK;
    else
        return DirEntry_test_mode(defining_class, self, 0, S_IFLNK);
#else
    /* POSIX without d_type */
    return DirEntry_test_mode(defining_class, self, 0, S_IFLNK);
#endif
}

static PyObject *
DirEntry_fetch_stat(PyObject *module, DirEntry *self, int follow_symlinks)
{
    int result;
    STRUCT_STAT st;
    PyObject *ub;

#ifdef MS_WINDOWS
    if (!PyUnicode_FSDecoder(self->path, &ub))
        return NULL;
    wchar_t *path = PyUnicode_AsWideCharString(ub, NULL);
    Py_DECREF(ub);
#else /* POSIX */
    if (!PyUnicode_FSConverter(self->path, &ub))
        return NULL;
    const char *path = PyBytes_AS_STRING(ub);
    if (self->dir_fd != DEFAULT_DIR_FD) {
#ifdef HAVE_FSTATAT
      if (HAVE_FSTATAT_RUNTIME) {
        Py_BEGIN_ALLOW_THREADS
        result = fstatat(self->dir_fd, path, &st,
                         follow_symlinks ? 0 : AT_SYMLINK_NOFOLLOW);
        Py_END_ALLOW_THREADS
      } else

#endif /* HAVE_FSTATAT */
      {
        Py_DECREF(ub);
        PyErr_SetString(PyExc_NotImplementedError, "can't fetch stat");
        return NULL;
      }
    }
    else
#endif
    {
        Py_BEGIN_ALLOW_THREADS
        if (follow_symlinks) {
            result = STAT(path, &st);
        }
        else {
            result = LSTAT(path, &st);
        }
        Py_END_ALLOW_THREADS
    }
#if defined(MS_WINDOWS)
    PyMem_Free(path);
#else
    Py_DECREF(ub);
#endif

    if (result != 0)
        return path_object_error(self->path);

    return _pystat_fromstructstat(module, &st);
}

static PyObject *
DirEntry_get_lstat(PyTypeObject *defining_class, DirEntry *self)
{
    if (!self->lstat) {
        PyObject *module = PyType_GetModule(defining_class);
#ifdef MS_WINDOWS
        self->lstat = _pystat_fromstructstat(module, &self->win32_lstat);
#else /* POSIX */
        self->lstat = DirEntry_fetch_stat(module, self, 0);
#endif
    }
    Py_XINCREF(self->lstat);
    return self->lstat;
}

/*[clinic input]
os.DirEntry.stat
    defining_class: defining_class
    /
    *
    follow_symlinks: bool = True

Return stat_result object for the entry; cached per entry.
[clinic start generated code]*/

static PyObject *
os_DirEntry_stat_impl(DirEntry *self, PyTypeObject *defining_class,
                      int follow_symlinks)
/*[clinic end generated code: output=23f803e19c3e780e input=e816273c4e67ee98]*/
{
    if (!follow_symlinks) {
        return DirEntry_get_lstat(defining_class, self);
    }

    if (!self->stat) {
        int result = os_DirEntry_is_symlink_impl(self, defining_class);
        if (result == -1) {
            return NULL;
        }
        if (result) {
            PyObject *module = PyType_GetModule(defining_class);
            self->stat = DirEntry_fetch_stat(module, self, 1);
        }
        else {
            self->stat = DirEntry_get_lstat(defining_class, self);
        }
    }

    Py_XINCREF(self->stat);
    return self->stat;
}

/* Set exception and return -1 on error, 0 for False, 1 for True */
static int
DirEntry_test_mode(PyTypeObject *defining_class, DirEntry *self,
                   int follow_symlinks, unsigned short mode_bits)
{
    PyObject *stat = NULL;
    PyObject *st_mode = NULL;
    long mode;
    int result;
#if defined(MS_WINDOWS) || defined(HAVE_DIRENT_D_TYPE)
    int is_symlink;
    int need_stat;
#endif
#ifdef MS_WINDOWS
    unsigned long dir_bits;
#endif

#ifdef MS_WINDOWS
    is_symlink = (self->win32_lstat.st_mode & S_IFMT) == S_IFLNK;
    need_stat = follow_symlinks && is_symlink;
#elif defined(HAVE_DIRENT_D_TYPE)
    is_symlink = self->d_type == DT_LNK;
    need_stat = self->d_type == DT_UNKNOWN || (follow_symlinks && is_symlink);
#endif

#if defined(MS_WINDOWS) || defined(HAVE_DIRENT_D_TYPE)
    if (need_stat) {
#endif
        stat = os_DirEntry_stat_impl(self, defining_class, follow_symlinks);
        if (!stat) {
            if (PyErr_ExceptionMatches(PyExc_FileNotFoundError)) {
                /* If file doesn't exist (anymore), then return False
                   (i.e., say it's not a file/directory) */
                PyErr_Clear();
                return 0;
            }
            goto error;
        }
        _scanwalkstate* state = get_scanwalk_state(PyType_GetModule(defining_class));
        st_mode = PyObject_GetAttr(stat, state->st_mode);
        if (!st_mode)
            goto error;

        mode = PyLong_AsLong(st_mode);
        if (mode == -1 && PyErr_Occurred())
            goto error;
        Py_CLEAR(st_mode);
        Py_CLEAR(stat);
        result = (mode & S_IFMT) == mode_bits;
#if defined(MS_WINDOWS) || defined(HAVE_DIRENT_D_TYPE)
    }
    else if (is_symlink) {
        assert(mode_bits != S_IFLNK);
        result = 0;
    }
    else {
        assert(mode_bits == S_IFDIR || mode_bits == S_IFREG);
#ifdef MS_WINDOWS
        dir_bits = self->win32_lstat.st_file_attributes & FILE_ATTRIBUTE_DIRECTORY;
        if (mode_bits == S_IFDIR)
            result = dir_bits != 0;
        else
            result = dir_bits == 0;
#else /* POSIX */
        if (mode_bits == S_IFDIR)
            result = self->d_type == DT_DIR;
        else
            result = self->d_type == DT_REG;
#endif
    }
#endif

    return result;

error:
    Py_XDECREF(st_mode);
    Py_XDECREF(stat);
    return -1;
}

/*[clinic input]
os.DirEntry.is_dir -> bool
    defining_class: defining_class
    /
    *
    follow_symlinks: bool = True

Return True if the entry is a directory; cached per entry.
[clinic start generated code]*/

static int
os_DirEntry_is_dir_impl(DirEntry *self, PyTypeObject *defining_class,
                        int follow_symlinks)
/*[clinic end generated code: output=0cd453b9c0987fdf input=1a4ffd6dec9920cb]*/
{
    return DirEntry_test_mode(defining_class, self, follow_symlinks, S_IFDIR);
}

/*[clinic input]
os.DirEntry.is_file -> bool
    defining_class: defining_class
    /
    *
    follow_symlinks: bool = True

Return True if the entry is a file; cached per entry.
[clinic start generated code]*/

static int
os_DirEntry_is_file_impl(DirEntry *self, PyTypeObject *defining_class,
                         int follow_symlinks)
/*[clinic end generated code: output=f7c277ab5ba80908 input=0a64c5a12e802e3b]*/
{
    return DirEntry_test_mode(defining_class, self, follow_symlinks, S_IFREG);
}

/*[clinic input]
os.DirEntry.inode

Return inode of the entry; cached per entry.
[clinic start generated code]*/

static PyObject *
os_DirEntry_inode_impl(DirEntry *self)
/*[clinic end generated code: output=156bb3a72162440e input=3ee7b872ae8649f0]*/
{
#ifdef MS_WINDOWS
    if (!self->got_file_index) {
        PyObject *unicode;
        STRUCT_STAT stat;
        int result;

        if (!PyUnicode_FSDecoder(self->path, &unicode))
            return NULL;
        wchar_t *path = PyUnicode_AsWideCharString(unicode, NULL);
        Py_DECREF(unicode);
        result = LSTAT(path, &stat);
        PyMem_Free(path);

        if (result != 0)
            return path_object_error(self->path);

        self->win32_file_index = stat.st_ino;
        self->got_file_index = 1;
    }
    static_assert(sizeof(unsigned long long) >= sizeof(self->win32_file_index),
                  "DirEntry.win32_file_index is larger than unsigned long long");
    return PyLong_FromUnsignedLongLong(self->win32_file_index);
#else /* POSIX */
    static_assert(sizeof(unsigned long long) >= sizeof(self->d_ino),
                  "DirEntry.d_ino is larger than unsigned long long");
    return PyLong_FromUnsignedLongLong(self->d_ino);
#endif
}

static PyObject *
DirEntry_repr(DirEntry *self)
{
    return PyUnicode_FromFormat("<DirEntry %R skip=%R>", self->name, self->skip);
}

/*[clinic input]
os.DirEntry.__fspath__

Returns the path for the entry.
[clinic start generated code]*/

static PyObject *
os_DirEntry___fspath___impl(DirEntry *self)
/*[clinic end generated code: output=6dd7f7ef752e6f4f input=3c49d0cf38df4fac]*/
{
    Py_INCREF(self->path);
    return self->path;
}

static PyMemberDef DirEntry_members[] = {
    {"name", T_OBJECT_EX, offsetof(DirEntry, name), READONLY,
     "the entry's base filename, relative to scandir() \"path\" argument"},
    {"path", T_OBJECT_EX, offsetof(DirEntry, path), READONLY,
     "the entry's full path name; equivalent to os.path.join(scandir_path, entry.name)"},
    {"skip", T_OBJECT_EX, offsetof(DirEntry, skip), 0,
     "whether to skip over the entry when walking a directory tree"},
    {NULL}
};

#include "_scanwalk.c.h"

static PyMethodDef DirEntry_methods[] = {
    OS_DIRENTRY_IS_DIR_METHODDEF
    OS_DIRENTRY_IS_FILE_METHODDEF
    OS_DIRENTRY_IS_SYMLINK_METHODDEF
    OS_DIRENTRY_STAT_METHODDEF
    OS_DIRENTRY_INODE_METHODDEF
    OS_DIRENTRY___FSPATH___METHODDEF
    {"__class_getitem__",       Py_GenericAlias,
    METH_O|METH_CLASS,          PyDoc_STR("See PEP 585")},
    {NULL}
};

static PyType_Slot DirEntryType_slots[] = {
    {Py_tp_new, _disabled_new},
    {Py_tp_dealloc, DirEntry_dealloc},
    {Py_tp_repr, DirEntry_repr},
    {Py_tp_methods, DirEntry_methods},
    {Py_tp_members, DirEntry_members},
    {0, 0},
};

static PyType_Spec DirEntryType_spec = {
    MODNAME ".DirEntry",
    sizeof(DirEntry),
    0,
    Py_TPFLAGS_DEFAULT,
    DirEntryType_slots
};


#ifdef MS_WINDOWS

static wchar_t *
join_path_filenameW(const wchar_t *path_wide, const wchar_t *filename)
{
    Py_ssize_t path_len;
    Py_ssize_t size;
    wchar_t *result;
    wchar_t ch;

    if (!path_wide) { /* Default arg: "." */
        path_wide = L".";
        path_len = 1;
    }
    else {
        path_len = wcslen(path_wide);
    }

    /* The +1's are for the path separator and the NUL */
    size = path_len + 1 + wcslen(filename) + 1;
    result = PyMem_New(wchar_t, size);
    if (!result) {
        PyErr_NoMemory();
        return NULL;
    }
    wcscpy(result, path_wide);
    if (path_len > 0) {
        ch = result[path_len - 1];
        if (ch != SEP && ch != ALTSEP && ch != L':')
            result[path_len++] = SEP;
        wcscpy(result + path_len, filename);
    }
    return result;
}

static PyObject *
DirEntry_from_find_data(PyObject *module, path_t *path, WIN32_FIND_DATAW *dataW)
{
    DirEntry *entry;
    BY_HANDLE_FILE_INFORMATION file_info;
    ULONG reparse_tag;
    wchar_t *joined_path;

    PyObject *DirEntryType = get_scanwalk_state(module)->DirEntryType;
    entry = PyObject_New(DirEntry, (PyTypeObject *)DirEntryType);
    if (!entry)
        return NULL;
    entry->name = NULL;
    entry->path = NULL;
    entry->skip = Py_False;
    Py_XINCREF(Py_False;
    entry->stat = NULL;
    entry->lstat = NULL;
    entry->got_file_index = 0;

    entry->name = PyUnicode_FromWideChar(dataW->cFileName, -1);
    if (!entry->name)
        goto error;
    if (path->narrow) {
        Py_SETREF(entry->name, PyUnicode_EncodeFSDefault(entry->name));
        if (!entry->name)
            goto error;
    }

    joined_path = join_path_filenameW(path->wide, dataW->cFileName);
    if (!joined_path)
        goto error;

    entry->path = PyUnicode_FromWideChar(joined_path, -1);
    PyMem_Free(joined_path);
    if (!entry->path)
        goto error;
    if (path->narrow) {
        Py_SETREF(entry->path, PyUnicode_EncodeFSDefault(entry->path));
        if (!entry->path)
            goto error;
    }

    find_data_to_file_info(dataW, &file_info, &reparse_tag);
    _Py_attribute_data_to_stat(&file_info, reparse_tag, &entry->win32_lstat);

    return (PyObject *)entry;

error:
    Py_DECREF(entry);
    return NULL;
}

#else /* POSIX */

static char *
join_path_filename(const char *path_narrow, const char* filename, Py_ssize_t filename_len)
{
    Py_ssize_t path_len;
    Py_ssize_t size;
    char *result;

    if (!path_narrow) { /* Default arg: "." */
        path_narrow = ".";
        path_len = 1;
    }
    else {
        path_len = strlen(path_narrow);
    }

    if (filename_len == -1)
        filename_len = strlen(filename);

    /* The +1's are for the path separator and the NUL */
    size = path_len + 1 + filename_len + 1;
    result = PyMem_New(char, size);
    if (!result) {
        PyErr_NoMemory();
        return NULL;
    }
    strcpy(result, path_narrow);
    if (path_len > 0 && result[path_len - 1] != '/')
        result[path_len++] = '/';
    strcpy(result + path_len, filename);
    return result;
}

static PyObject *
DirEntry_from_posix_info(PyObject *module, path_t *path, const char *name,
                         Py_ssize_t name_len, ino_t d_ino
#ifdef HAVE_DIRENT_D_TYPE
                         , unsigned char d_type
#endif
                         )
{
    DirEntry *entry;
    char *joined_path;

    PyObject *DirEntryType = get_scanwalk_state(module)->DirEntryType;
    entry = PyObject_New(DirEntry, (PyTypeObject *)DirEntryType);
    if (!entry)
        return NULL;
    entry->name = NULL;
    entry->path = NULL;
    entry->skip = Py_False;
    Py_INCREF(Py_False);
    entry->stat = NULL;
    entry->lstat = NULL;

    if (path->fd != -1) {
        entry->dir_fd = path->fd;
        joined_path = NULL;
    }
    else {
        entry->dir_fd = DEFAULT_DIR_FD;
        joined_path = join_path_filename(path->narrow, name, name_len);
        if (!joined_path)
            goto error;
    }

    if (!path->narrow || !PyObject_CheckBuffer(path->object)) {
        entry->name = PyUnicode_DecodeFSDefaultAndSize(name, name_len);
        if (joined_path)
            entry->path = PyUnicode_DecodeFSDefault(joined_path);
    }
    else {
        entry->name = PyBytes_FromStringAndSize(name, name_len);
        if (joined_path)
            entry->path = PyBytes_FromString(joined_path);
    }
    PyMem_Free(joined_path);
    if (!entry->name)
        goto error;

    if (path->fd != -1) {
        entry->path = entry->name;
        Py_INCREF(entry->path);
    }
    else if (!entry->path)
        goto error;

#ifdef HAVE_DIRENT_D_TYPE
    entry->d_type = d_type;
#endif
    entry->d_ino = d_ino;

    return (PyObject *)entry;

error:
    Py_XDECREF(entry);
    return NULL;
}

#endif


typedef struct {
    PyObject_HEAD
    path_t path;
#ifdef MS_WINDOWS
    HANDLE handle;
    WIN32_FIND_DATAW file_data;
    int first_time;
#else /* POSIX */
    DIR *dirp;
#endif
#ifdef HAVE_FDOPENDIR
    int fd;
#endif
} ScandirIterator;

#ifdef MS_WINDOWS

static int
ScandirIterator_is_closed(ScandirIterator *iterator)
{
    return iterator->handle == INVALID_HANDLE_VALUE;
}

static void
ScandirIterator_closedir(ScandirIterator *iterator)
{
    HANDLE handle = iterator->handle;

    if (handle == INVALID_HANDLE_VALUE)
        return;

    iterator->handle = INVALID_HANDLE_VALUE;
    Py_BEGIN_ALLOW_THREADS
    FindClose(handle);
    Py_END_ALLOW_THREADS
}

static PyObject *
ScandirIterator_iternext(ScandirIterator *iterator)
{
    WIN32_FIND_DATAW *file_data = &iterator->file_data;
    BOOL success;
    PyObject *entry;

    /* Happens if the iterator is iterated twice, or closed explicitly */
    if (iterator->handle == INVALID_HANDLE_VALUE)
        return NULL;

    while (1) {
        if (!iterator->first_time) {
            Py_BEGIN_ALLOW_THREADS
            success = FindNextFileW(iterator->handle, file_data);
            Py_END_ALLOW_THREADS
            if (!success) {
                /* Error or no more files */
                if (GetLastError() != ERROR_NO_MORE_FILES)
                    path_error(&iterator->path);
                break;
            }
        }
        iterator->first_time = 0;

        /* Skip over . and .. */
        if (wcscmp(file_data->cFileName, L".") != 0 &&
            wcscmp(file_data->cFileName, L"..") != 0)
        {
            PyObject *module = PyType_GetModule(Py_TYPE(iterator));
            entry = DirEntry_from_find_data(module, &iterator->path, file_data);
            if (!entry)
                break;
            return entry;
        }

        /* Loop till we get a non-dot directory or finish iterating */
    }

    /* Error or no more files */
    ScandirIterator_closedir(iterator);
    return NULL;
}

#else /* POSIX */

static int
ScandirIterator_is_closed(ScandirIterator *iterator)
{
    return !iterator->dirp;
}

static void
ScandirIterator_closedir(ScandirIterator *iterator)
{
    DIR *dirp = iterator->dirp;

    if (!dirp)
        return;

    iterator->dirp = NULL;
    Py_BEGIN_ALLOW_THREADS
#ifdef HAVE_FDOPENDIR
    if (iterator->path.fd != -1)
        rewinddir(dirp);
#endif
    closedir(dirp);
    Py_END_ALLOW_THREADS
    return;
}

static PyObject *
ScandirIterator_iternext(ScandirIterator *iterator)
{
    struct dirent *direntp;
    Py_ssize_t name_len;
    int is_dot;
    PyObject *entry;

    /* Happens if the iterator is iterated twice, or closed explicitly */
    if (!iterator->dirp)
        return NULL;

    while (1) {
        errno = 0;
        Py_BEGIN_ALLOW_THREADS
        direntp = readdir(iterator->dirp);
        Py_END_ALLOW_THREADS

        if (!direntp) {
            /* Error or no more files */
            if (errno != 0)
                path_error(&iterator->path);
            break;
        }

        /* Skip over . and .. */
        name_len = NAMLEN(direntp);
        is_dot = direntp->d_name[0] == '.' &&
                 (name_len == 1 || (direntp->d_name[1] == '.' && name_len == 2));
        if (!is_dot) {
            PyObject *module = PyType_GetModule(Py_TYPE(iterator));
            entry = DirEntry_from_posix_info(module,
                                             &iterator->path, direntp->d_name,
                                             name_len, direntp->d_ino
#ifdef HAVE_DIRENT_D_TYPE
                                             , direntp->d_type
#endif
                                            );
            if (!entry)
                break;
            return entry;
        }

        /* Loop till we get a non-dot directory or finish iterating */
    }

    /* Error or no more files */
    ScandirIterator_closedir(iterator);
    return NULL;
}

#endif

static PyObject *
ScandirIterator_close(ScandirIterator *self, PyObject *args)
{
    ScandirIterator_closedir(self);
    Py_RETURN_NONE;
}

static PyObject *
ScandirIterator_enter(PyObject *self, PyObject *args)
{
    Py_INCREF(self);
    return self;
}

static PyObject *
ScandirIterator_exit(ScandirIterator *self, PyObject *args)
{
    ScandirIterator_closedir(self);
    Py_RETURN_NONE;
}

static void
ScandirIterator_finalize(ScandirIterator *iterator)
{
    PyObject *error_type, *error_value, *error_traceback;

    /* Save the current exception, if any. */
    PyErr_Fetch(&error_type, &error_value, &error_traceback);

    if (!ScandirIterator_is_closed(iterator)) {
        ScandirIterator_closedir(iterator);

        if (PyErr_ResourceWarning((PyObject *)iterator, 1,
                                  "unclosed scandir iterator %R", iterator)) {
            /* Spurious errors can appear at shutdown */
            if (PyErr_ExceptionMatches(PyExc_Warning)) {
                PyErr_WriteUnraisable((PyObject *) iterator);
            }
        }
    }

    path_cleanup(&iterator->path);

    /* Restore the saved exception. */
    PyErr_Restore(error_type, error_value, error_traceback);
}

static void
ScandirIterator_dealloc(ScandirIterator *iterator)
{
    PyTypeObject *tp = Py_TYPE(iterator);
    if (PyObject_CallFinalizerFromDealloc((PyObject *)iterator) < 0)
        return;

    freefunc free_func = PyType_GetSlot(tp, Py_tp_free);
    free_func(iterator);
    Py_DECREF(tp);
}

static PyMethodDef ScandirIterator_methods[] = {
    {"__enter__", (PyCFunction)ScandirIterator_enter, METH_NOARGS},
    {"__exit__", (PyCFunction)ScandirIterator_exit, METH_VARARGS},
    {"close", (PyCFunction)ScandirIterator_close, METH_NOARGS},
    {NULL}
};

static PyType_Slot ScandirIteratorType_slots[] = {
    {Py_tp_new, _disabled_new},
    {Py_tp_dealloc, ScandirIterator_dealloc},
    {Py_tp_finalize, ScandirIterator_finalize},
    {Py_tp_iter, PyObject_SelfIter},
    {Py_tp_iternext, ScandirIterator_iternext},
    {Py_tp_methods, ScandirIterator_methods},
    {0, 0},
};

static PyType_Spec ScandirIteratorType_spec = {
    MODNAME ".ScandirIterator",
    sizeof(ScandirIterator),
    0,
    // bpo-40549: Py_TPFLAGS_BASETYPE should not be used, since
    // PyType_GetModule(Py_TYPE(self)) doesn't work on a subclass instance.
    (Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_FINALIZE),
    ScandirIteratorType_slots
};

/*[clinic input]
os.scandir

    path : path_t(nullable=True, allow_fd='PATH_HAVE_FDOPENDIR') = None

Return an iterator of DirEntry objects for given path.

path can be specified as either str, bytes, or a path-like object.  If path
is bytes, the names of yielded DirEntry objects will also be bytes; in
all other circumstances they will be str.

If path is None, uses the path='.'.
[clinic start generated code]*/

static PyObject *
os_scandir_impl(PyObject *module, path_t *path)
/*[clinic end generated code: output=6eb2668b675ca89e input=6bdd312708fc3bb0]*/
{
    ScandirIterator *iterator;
#ifdef MS_WINDOWS
    wchar_t *path_strW;
#else
    const char *path_str;
#ifdef HAVE_FDOPENDIR
    int fd = -1;
#endif
#endif

    if (PySys_Audit("os.scandir", "O",
                    path->object ? path->object : Py_None) < 0) {
        return NULL;
    }

    PyObject *ScandirIteratorType = get_scanwalk_state(module)->ScandirIteratorType;
    iterator = PyObject_New(ScandirIterator, (PyTypeObject *)ScandirIteratorType);
    if (!iterator)
        return NULL;

#ifdef MS_WINDOWS
    iterator->handle = INVALID_HANDLE_VALUE;
#else
    iterator->dirp = NULL;
#endif

    /* Move the ownership to iterator->path */
    memcpy(&iterator->path, path, sizeof(path_t));
    memset(path, 0, sizeof(path_t));

#ifdef MS_WINDOWS
    iterator->first_time = 1;

    path_strW = join_path_filenameW(iterator->path.wide, L"*.*");
    if (!path_strW)
        goto error;

    Py_BEGIN_ALLOW_THREADS
    iterator->handle = FindFirstFileW(path_strW, &iterator->file_data);
    Py_END_ALLOW_THREADS

    PyMem_Free(path_strW);

    if (iterator->handle == INVALID_HANDLE_VALUE) {
        path_error(&iterator->path);
        goto error;
    }
#else /* POSIX */
    errno = 0;
#ifdef HAVE_FDOPENDIR
    if (iterator->path.fd != -1) {
      if (HAVE_FDOPENDIR_RUNTIME) {
        /* closedir() closes the FD, so we duplicate it */
        fd = _Py_dup(iterator->path.fd);
        if (fd == -1)
            goto error;

        Py_BEGIN_ALLOW_THREADS
        iterator->dirp = fdopendir(fd);
        Py_END_ALLOW_THREADS
      } else {
        PyErr_SetString(PyExc_TypeError,
            "scandir: path should be string, bytes, os.PathLike or None, not int");
        return NULL;
      }
    }
    else
#endif
    {
        if (iterator->path.narrow)
            path_str = iterator->path.narrow;
        else
            path_str = ".";

        Py_BEGIN_ALLOW_THREADS
        iterator->dirp = opendir(path_str);
        Py_END_ALLOW_THREADS
    }

    if (!iterator->dirp) {
        path_error(&iterator->path);
#ifdef HAVE_FDOPENDIR
        if (fd != -1) {
            Py_BEGIN_ALLOW_THREADS
            close(fd);
            Py_END_ALLOW_THREADS
        }
#endif
        goto error;
    }
#endif

    return (PyObject *)iterator;

error:
    Py_DECREF(iterator);
    return NULL;
}

static PyMethodDef scanwalk_methods[] = {
    OS_SCANDIR_METHODDEF
    {NULL,              NULL}            /* Sentinel */
};

static int
scanwalkmodule_exec(PyObject *m)
{
    _scanwalkstate *state = get_scanwalk_state(m);

    PyObject *os_module = PyImport_ImportModule("os");
    if (os_module == NULL) {
        return -1;
    }
    PyObject *StatResultType = PyObject_GetAttrString(os_module, "stat_result");
    if (StatResultType == NULL) {
        return -1;
    }
    state->StatResultType = StatResultType;

#ifdef NEED_TICKS_PER_SECOND
#  if defined(HAVE_SYSCONF) && defined(_SC_CLK_TCK)
    ticks_per_second = sysconf(_SC_CLK_TCK);
#  elif defined(HZ)
    ticks_per_second = HZ;
#  else
    ticks_per_second = 60; /* magic fallback value; may be bogus */
#  endif
#endif

    /* initialize scandir types */
    PyObject *ScandirIteratorType = PyType_FromModuleAndSpec(m, &ScandirIteratorType_spec, NULL);
    if (ScandirIteratorType == NULL) {
        return -1;
    }
    state->ScandirIteratorType = ScandirIteratorType;

    PyObject *DirEntryType = PyType_FromModuleAndSpec(m, &DirEntryType_spec, NULL);
    if (DirEntryType == NULL) {
        return -1;
    }
    Py_INCREF(DirEntryType);
    PyModule_AddObject(m, "DirEntry", DirEntryType);
    state->DirEntryType = DirEntryType;

    if ((state->billion = PyLong_FromLong(1000000000)) == NULL)
        return -1;
    state->st_mode = PyUnicode_InternFromString("st_mode");
    if (state->st_mode == NULL)
        return -1;

    /* suppress "function not used" warnings */
    {
    int ignored;
    dir_fd_converter(Py_None, &ignored);
    dir_fd_unavailable(Py_None, &ignored);
    }

    return 0;
}


static PyModuleDef_Slot scanwalkmodile_slots[] = {
    {Py_mod_exec, scanwalkmodule_exec},
    {0, NULL}
};

static struct PyModuleDef scanwalkmodule = {
    PyModuleDef_HEAD_INIT,
    .m_name = MODNAME,
    .m_doc = _scanwalk__doc__,
    .m_size = sizeof(_scanwalkstate),
    .m_methods = scanwalk_methods,
    .m_slots = scanwalkmodile_slots,
    .m_traverse = _scanwalk_traverse,
    .m_clear = _scanwalk_clear,
    .m_free = _scanwalk_free,
};

PyMODINIT_FUNC
INITFUNC(void)
{
    return PyModuleDef_Init(&scanwalkmodule);
}

#ifdef __cplusplus
}
#endif
