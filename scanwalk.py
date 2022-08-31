import ctypes
import ctypes.util
import errno
import os
import stat
import sys

class GenericDirEntry(object):
    __slots__ = ('name', '_stat', '_lstat', '_scandir_path', '_path')

    def __init__(self, scandir_path, name):
        self._scandir_path = scandir_path
        self.name = name
        self._stat = None
        self._lstat = None
        self._path = None

    @property
    def path(self):
        if self._path is None:
            self._path = os.path.join(self._scandir_path, self.name)
        return self._path

    def stat(self, follow_symlinks=True):
        if follow_symlinks:
            if self._stat is None:
                self._stat = os.stat(self.path)
            return self._stat
        else:
            if self._lstat is None:
                self._lstat = os.lstat(self.path)
            return self._lstat

    # The code duplication below is intentional: this is for slightly
    # better performance on systems that fall back to GenericDirEntry.
    # It avoids an additional attribute lookup and method call, which
    # are relatively slow on CPython.
    def is_dir(self, follow_symlinks=True):
        try:
            st = self.stat(follow_symlinks=follow_symlinks)
        except OSError as e:
            if e.errno != errno.ENOENT:
                raise
            return False  # Path doesn't exist or is a broken symlink
        return st.st_mode & 0o170000 == stat.S_IFDIR

    def is_file(self, follow_symlinks=True):
        try:
            st = self.os.stat(follow_symlinks=follow_symlinks)
        except OSError as e:
            if e.errno != errno.ENOENT:
                raise
            return False  # Path doesn't exist or is a broken symlink
        return st.st_mode & 0o170000 == stat.S_IFREG

    def is_symlink(self):
        try:
            st = self.os.stat(follow_symlinks=False)
        except OSError as e:
            if e.errno != stat.ENOENT:
                raise
            return False  # Path doesn't exist or is a broken symlink
        return st.st_mode & 0o170000 == stat.S_IFLNK

    def inode(self):
        st = self.os.stat(follow_symlinks=False)
        return st.st_ino

    def __str__(self):
        return f'<{self.__class__.__name__}: {self.name!r}>'

    __repr__ = __str__


# Rather annoying how the dirent struct is slightly different on each
# platform. The only fields we care about are d_name and d_type.
class Dirent(ctypes.Structure):
    if sys.platform.startswith('linux'):
        _fields_ = (
            ('d_ino', ctypes.c_ulong),
            ('d_off', ctypes.c_long),
            ('d_reclen', ctypes.c_ushort),
            ('d_type', ctypes.c_byte),
            ('d_name', ctypes.c_char * 256),
        )
    elif 'openbsd' in sys.platform:
        _fields_ = (
            ('d_ino', ctypes.c_uint64),
            ('d_off', ctypes.c_uint64),
            ('d_reclen', ctypes.c_uint16),
            ('d_type', ctypes.c_uint8),
            ('d_namlen', ctypes.c_uint8),
            ('__d_padding', ctypes.c_uint8 * 4),
            ('d_name', ctypes.c_char * 256),
        )
    elif sys.platform.startswith('darwin'):
        # macOS 10.6+ defaults to _DARWIN_FEATURE_64_BIT_INODE
        # https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/stat.2.html
        _fields_ = (
            ('d_ino',       ctypes.c_uint64),
            ('d_seekoff',   ctypes.c_uint64),
            ('d_reclen',    ctypes.c_uint16),
            ('d_namlen',    ctypes.c_uint16),
            ('d_type',      ctypes.c_uint8),
            ('d_name',      ctypes.c_char * 1024),
        )
    else:
        _fields_ = (
            ('d_ino', ctypes.c_uint32),  # must be uint32, not ulong
            ('d_reclen', ctypes.c_ushort),
            ('d_type', ctypes.c_byte),
            ('d_namlen', ctypes.c_byte),
            ('d_name', ctypes.c_char * 256),
        )

DT_UNKNOWN = 0
DT_DIR = 4
DT_REG = 8
DT_LNK = 10

Dirent_p = ctypes.POINTER(Dirent)
Dirent_pp = ctypes.POINTER(Dirent_p)
Dirent_ppp = ctypes.POINTER(Dirent_pp)

# https://sourceforge.net/p/ctypes/mailman/ctypes-users/thread/ff8cem%244ss%241%40ger.gmane.org/
def _from_param(cls, obj):
    if obj is None:
        return None
    return ctypes._CFuncPtr.from_param(obj)

SELECT_fn = ctypes.CFUNCTYPE(ctypes.c_int, Dirent_p)
SELECT_fn.from_param = classmethod(_from_param)
COMPAR_fn = ctypes.CFUNCTYPE(ctypes.c_int, Dirent_pp, Dirent_pp)
COMPAR_fn.from_param = classmethod(_from_param)

#SELECT_NULL = ctypes.cast(None, SELECT_fn)
#COMPAR_NULL = ctypes.cast(None, COMPAR_fn)

libc = ctypes.CDLL(ctypes.util.find_library('c'), use_errno=True)
free = libc.free
free.argtypes = [ctypes.c_void_p]
free.restype = None

_scandir = libc.scandir
_scandir.argtypes = [
    ctypes.c_char_p,  # dirp
    Dirent_ppp,  # namelist
    SELECT_fn,  # select
    COMPAR_fn,  # compar
]

file_system_encoding = sys.getfilesystemencoding()

class PosixDirEntry(object):
    __slots__ = ('name', '_d_type', '_stat', '_lstat', '_scandir_path', '_path', '_inode')

    def __init__(self, scandir_path, name, d_type, inode):
        self._scandir_path = scandir_path
        self.name = name
        self._d_type = d_type
        self._inode = inode
        self._stat = None
        self._lstat = None
        self._path = None

    @property
    def path(self):
        if self._path is None:
            self._path = os.path.join(self._scandir_path, self.name)
        return self._path

    def stat(self, follow_symlinks=True):
        if follow_symlinks:
            if self._stat is None:
                if self.is_symlink():
                    self._stat = os.stat(self.path)
                else:
                    if self._lstat is None:
                        self._lstat = os.lstat(self.path)
                    self._stat = self._lstat
            return self._stat
        else:
            if self._lstat is None:
                self._lstat = os.lstat(self.path)
            return self._lstat

    def is_dir(self, follow_symlinks=True):
        if (self._d_type == DT_UNKNOWN or
                (follow_symlinks and self.is_symlink())):
            try:
                st = self.os.stat(follow_symlinks=follow_symlinks)
            except OSError as e:
                if e.errno != errno.ENOENT:
                    raise
                return False
            return st.st_mode & 0o170000 == stat.S_IFDIR
        else:
            return self._d_type == DT_DIR

    def is_file(self, follow_symlinks=True):
        if (self._d_type == DT_UNKNOWN or
                (follow_symlinks and self.is_symlink())):
            try:
                st = self.os.stat(follow_symlinks=follow_symlinks)
            except OSError as e:
                if e.errno != errno.ENOENT:
                    raise
                return False
            return st.st_mode & 0o170000 == stat.S_IFREG
        else:
            return self._d_type == DT_REG

    def is_symlink(self):
        if self._d_type == DT_UNKNOWN:
            try:
                st = self.os.stat(follow_symlinks=False)
            except OSError as e:
                if e.errno != errno.ENOENT:
                    raise
                return False
            return st.st_mode & 0o170000 == errno.S_IFLNK
        else:
            return self._d_type == DT_LNK

    def inode(self):
        return self._inode

    def __str__(self):
        return '<{0}: {1!r}>'.format(self.__class__.__name__, self.name)

    __repr__ = __str__


def posix_error(filename):
    errno = ctypes.get_errno()
    exc = OSError(errno, os.strerror(errno))
    exc.filename = filename
    return exc


def scandir(path='.'):
    if isinstance(path, bytes):
        scandir_path = path
        is_bytes = True
    else:
        scandir_path = path.encode(file_system_encoding)
        is_bytes = False
    namelist = Dirent_pp()
    try:
        count = _scandir(scandir_path, namelist, None, None)
        if count < 0:
            errno = ctypes.get_errno()
            raise OSError(errno, os.strerror(errno), path)
        for i in range(count):
            entry = namelist[i][0]
            name = entry.d_name[:entry.d_namlen]
            if name not in (b'.', b'..'):
                if not is_bytes:
                    name = name.decode(file_system_encoding)
                yield PosixDirEntry(path, name, entry.d_type, entry.d_ino)
            free(ctypes.cast(namelist[i], ctypes.c_void_p))
    finally:
        free(ctypes.cast(namelist, ctypes.c_void_p))


def walk(path:os.PathLike, *, follow_symlinks:bool=False):
    yield PosixDirEntry(path, path, DT_DIR, 0)
    yield from _walk(path)


def _walk(path:os.PathLike, *, follow_symlinks:bool=False):
    for entry in scandir(path):
        yield entry
        if entry.is_dir(follow_symlinks=follow_symlinks):
            yield from _walk(entry.path)
