#define _PyCFunction_CAST(func) ((PyCFunction)(void(*)(void))(func))

PyDoc_STRVAR(os_DirEntry_is_symlink__doc__,
"is_symlink($self, /)\n"
"--\n"
"\n"
"Return True if the entry is a symbolic link; cached per entry.");

#define OS_DIRENTRY_IS_SYMLINK_METHODDEF    \
    {"is_symlink", _PyCFunction_CAST(os_DirEntry_is_symlink), METH_METHOD|METH_FASTCALL|METH_KEYWORDS, os_DirEntry_is_symlink__doc__},

static int
os_DirEntry_is_symlink_impl(DirEntry *self, PyTypeObject *defining_class);

static PyObject *
os_DirEntry_is_symlink(DirEntry *self, PyTypeObject *defining_class, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    int _return_value;

    if (nargs) {
        PyErr_SetString(PyExc_TypeError, "is_symlink() takes no arguments");
        goto exit;
    }
    _return_value = os_DirEntry_is_symlink_impl(self, defining_class);
    if ((_return_value == -1) && PyErr_Occurred()) {
        goto exit;
    }
    return_value = PyBool_FromLong((long)_return_value);

exit:
    return return_value;
}

PyDoc_STRVAR(os_DirEntry_stat__doc__,
"stat($self, /, *, follow_symlinks=True)\n"
"--\n"
"\n"
"Return stat_result object for the entry; cached per entry.");

#define OS_DIRENTRY_STAT_METHODDEF    \
    {"stat", _PyCFunction_CAST(os_DirEntry_stat), METH_METHOD|METH_FASTCALL|METH_KEYWORDS, os_DirEntry_stat__doc__},

static PyObject *
os_DirEntry_stat_impl(DirEntry *self, PyTypeObject *defining_class,
                      int follow_symlinks);

static PyObject *
os_DirEntry_stat(DirEntry *self, PyTypeObject *defining_class, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"follow_symlinks", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "stat", 0};
    PyObject *argsbuf[1];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 0;
    int follow_symlinks = 1;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 0, 0, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_kwonly;
    }
    follow_symlinks = PyObject_IsTrue(args[0]);
    if (follow_symlinks < 0) {
        goto exit;
    }
skip_optional_kwonly:
    return_value = os_DirEntry_stat_impl(self, defining_class, follow_symlinks);

exit:
    return return_value;
}

PyDoc_STRVAR(os_DirEntry_is_dir__doc__,
"is_dir($self, /, *, follow_symlinks=True)\n"
"--\n"
"\n"
"Return True if the entry is a directory; cached per entry.");

#define OS_DIRENTRY_IS_DIR_METHODDEF    \
    {"is_dir", _PyCFunction_CAST(os_DirEntry_is_dir), METH_METHOD|METH_FASTCALL|METH_KEYWORDS, os_DirEntry_is_dir__doc__},

static int
os_DirEntry_is_dir_impl(DirEntry *self, PyTypeObject *defining_class,
                        int follow_symlinks);

static PyObject *
os_DirEntry_is_dir(DirEntry *self, PyTypeObject *defining_class, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"follow_symlinks", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "is_dir", 0};
    PyObject *argsbuf[1];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 0;
    int follow_symlinks = 1;
    int _return_value;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 0, 0, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_kwonly;
    }
    follow_symlinks = PyObject_IsTrue(args[0]);
    if (follow_symlinks < 0) {
        goto exit;
    }
skip_optional_kwonly:
    _return_value = os_DirEntry_is_dir_impl(self, defining_class, follow_symlinks);
    if ((_return_value == -1) && PyErr_Occurred()) {
        goto exit;
    }
    return_value = PyBool_FromLong((long)_return_value);

exit:
    return return_value;
}

PyDoc_STRVAR(os_DirEntry_is_file__doc__,
"is_file($self, /, *, follow_symlinks=True)\n"
"--\n"
"\n"
"Return True if the entry is a file; cached per entry.");

#define OS_DIRENTRY_IS_FILE_METHODDEF    \
    {"is_file", _PyCFunction_CAST(os_DirEntry_is_file), METH_METHOD|METH_FASTCALL|METH_KEYWORDS, os_DirEntry_is_file__doc__},

static int
os_DirEntry_is_file_impl(DirEntry *self, PyTypeObject *defining_class,
                         int follow_symlinks);

static PyObject *
os_DirEntry_is_file(DirEntry *self, PyTypeObject *defining_class, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"follow_symlinks", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "is_file", 0};
    PyObject *argsbuf[1];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 0;
    int follow_symlinks = 1;
    int _return_value;

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 0, 0, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_kwonly;
    }
    follow_symlinks = PyObject_IsTrue(args[0]);
    if (follow_symlinks < 0) {
        goto exit;
    }
skip_optional_kwonly:
    _return_value = os_DirEntry_is_file_impl(self, defining_class, follow_symlinks);
    if ((_return_value == -1) && PyErr_Occurred()) {
        goto exit;
    }
    return_value = PyBool_FromLong((long)_return_value);

exit:
    return return_value;
}

PyDoc_STRVAR(os_DirEntry_inode__doc__,
"inode($self, /)\n"
"--\n"
"\n"
"Return inode of the entry; cached per entry.");

#define OS_DIRENTRY_INODE_METHODDEF    \
    {"inode", (PyCFunction)os_DirEntry_inode, METH_NOARGS, os_DirEntry_inode__doc__},

static PyObject *
os_DirEntry_inode_impl(DirEntry *self);

static PyObject *
os_DirEntry_inode(DirEntry *self, PyObject *Py_UNUSED(ignored))
{
    return os_DirEntry_inode_impl(self);
}

PyDoc_STRVAR(os_DirEntry___fspath____doc__,
"__fspath__($self, /)\n"
"--\n"
"\n"
"Returns the path for the entry.");

#define OS_DIRENTRY___FSPATH___METHODDEF    \
    {"__fspath__", (PyCFunction)os_DirEntry___fspath__, METH_NOARGS, os_DirEntry___fspath____doc__},

static PyObject *
os_DirEntry___fspath___impl(DirEntry *self);

static PyObject *
os_DirEntry___fspath__(DirEntry *self, PyObject *Py_UNUSED(ignored))
{
    return os_DirEntry___fspath___impl(self);
}

PyDoc_STRVAR(os_scandir__doc__,
"scandir($module, /, path=None)\n"
"--\n"
"\n"
"Return an iterator of DirEntry objects for given path.\n"
"\n"
"path can be specified as either str, bytes, or a path-like object.  If path\n"
"is bytes, the names of yielded DirEntry objects will also be bytes; in\n"
"all other circumstances they will be str.\n"
"\n"
"If path is None, uses the path=\'.\'.");

#define OS_SCANDIR_METHODDEF    \
    {"scandir", _PyCFunction_CAST(os_scandir), METH_FASTCALL|METH_KEYWORDS, os_scandir__doc__},

static PyObject *
os_scandir_impl(PyObject *module, path_t *path);

static PyObject *
os_scandir(PyObject *module, PyObject *const *args, Py_ssize_t nargs, PyObject *kwnames)
{
    PyObject *return_value = NULL;
    static const char * const _keywords[] = {"path", NULL};
    static _PyArg_Parser _parser = {NULL, _keywords, "scandir", 0};
    PyObject *argsbuf[1];
    Py_ssize_t noptargs = nargs + (kwnames ? PyTuple_GET_SIZE(kwnames) : 0) - 0;
    path_t path = PATH_T_INITIALIZE("scandir", "path", 1, PATH_HAVE_FDOPENDIR);

    args = _PyArg_UnpackKeywords(args, nargs, NULL, kwnames, &_parser, 0, 1, 0, argsbuf);
    if (!args) {
        goto exit;
    }
    if (!noptargs) {
        goto skip_optional_pos;
    }
    if (!path_converter(args[0], &path)) {
        goto exit;
    }
skip_optional_pos:
    return_value = os_scandir_impl(module, &path);

exit:
    /* Cleanup for path */
    path_cleanup(&path);

    return return_value;
}
