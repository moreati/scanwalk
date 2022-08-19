from __future__ import annotations

import os
import stat
from typing import AnyStr
from typing import Generator
from typing import Union

import _scanwalk


class DirEntry(os.PathLike):
    def __init__(self, parent, dirent):
        self._parent = parent
        self._dirent = dirent
        self._stat = None
        self._lstat = None
        self.skip = False

    @property
    def name(self) -> AnyStr:
        name_b = _scanwalk.ffi.unpack(self._dirent.d_name, self._dirent.d_namlen)
        if isinstance(self._parent, bytes):
            return name_b
        return os.fsdecode(name_b)

    @property
    def path(self) -> os.AnyStr:
        return os.path.join(self._parent, self.name)

    def inode(self) -> int:
        return self._dirent.d_ino

    def is_dir(self, *, follow_symlinks:bool=True) -> bool:
        if self._need_stat(follow_symlinks):
            st = self.stat(follow_symlinks=follow_symlinks)
            return stat.S_ISDIR(st.st_mode)
        return self._dirent.d_type == _scanwalk.lib.DT_DIR

    def is_file(self, *, follow_symlinks:bool=True) -> bool:
        if self._need_stat(follow_symlinks):
            st = self.stat(follow_symlinks=follow_symlinks)
            return stat.S_ISREG(st.st_mode)
        return self._dirent.d_type == _scanwalk.lib.DT_REG

    def is_symlink(self) -> bool:
        if self._need_stat(False):
            st = self.stat(follow_symlinks=False)
            return stat.S_ISLNK(st.st_mode)
        return self._dirent.d_type == _scanwalk.lib.DT_LNK

    def stat(self, *, follow_symlinks:bool=True) -> os.stat_result:
        if follow_symlinks:
            if self._stat is None:
                self._stat = os.stat(self.path, follow_symlinks)
            return self._stat
        else:
            if self._lstat is None:
                self._lstat = os.stat(self.path, follow_symlinks)
            return self._lstat

    def _need_stat(self, follow_symlinks:bool) -> bool:
        if self._dirent.d_type == _scanwalk.lib.DT_UNKNOWN:
            return True
        if follow_symlinks and self._dirent.d_type == _scanwalk.lib.DT_LNK:
            return True
        return False

    def __fspath__(self) -> AnyStr:
        return self.path

    def __repr__(self) -> str:
        return f'<{self.__class__.__name__} {self.path!r}>'


class FakeDirEntry(os.PathLike):
    '''
    A stand-in for os.DirEntry, that can be instantiated directly.
    '''
    def __init__(self, path:os.PathLike):
        self.path = path
        self.skip = False

    @property
    def name(self) -> AnyStr:
        return os.path.basename(self.path)

    def inode(self) -> int:
        return self.stat(follow_symlinks=False).st_inode

    def is_dir(self, *, follow_symlinks:bool=True) -> bool:
        return stat.S_ISDIR(self.stat(follow_symlinks=follow_symlinks).st_mode)

    def is_file(self, *, follow_symlinks:bool=True) -> bool:
        return stat.S_ISREG(self.stat(follow_symlinks=follow_symlinks).st_mode)

    def is_symlink(self) -> bool:
        return stat.S_ISLNK(self.stat(follow_symlinks=False).st_mode)

    def stat(self, *, follow_symlinks:bool=True) -> os.stat_result:
        return os.stat(self.path, follow_symlinks=follow_symlinks)

    def __fspath__(self) -> AnyStr:
        return os.fspath(self.path)

    def __repr__(self) -> str:
        return f'<{self.__class__.__name__} {self.path!r}>'


WalkGenerator = Generator[Union[os.DirEntry, FakeDirEntry], None, None]


def scandir(path='.'):
    if isinstance(path, str):
        path_b = os.fsencode(path)
    else:
        path_b = path

    with _scanwalk.ffi.new('struct dirent ***namelist') as namelist:
        select = _scanwalk.ffi.NULL
        compar = _scanwalk.ffi.NULL
        count = _scanwalk.lib.scandir(path_b, namelist, select, compar)
        if count < 0:
            raise OSError(_scanwalk.ffi.errno, os.strerror(_scanwalk.ffi.errno), filename=path)
        unwanted = frozenset({b'.', b'..'})
        for i in range(count):
            dirent = namelist[0][i][0]
            name_b = _scanwalk.ffi.unpack(dirent.d_name, dirent.d_namlen)
            if name_b in unwanted:
                continue
            yield DirEntry(path, dirent)


def walk(top:os.PathLike, *, follow_symlinks:bool=False) -> WalkGenerator:
    """
    Generate DirEntry objects for top, and directories/files under top
    (excluding '.' and '..' entries).

    It aims to be a faster alternative to `os.walk()`. It uses `scandir()`
    output directly, avoiding intermediate lists and sort operations.
    """
    if isinstance(top, (DirEntry, FakeDirEntry)):
        yield top
    elif isinstance(top, os.DirEntry):
        yield DirEntry(top)
    else:
        yield FakeDirEntry(top)
    yield from _walk(top, follow_symlinks=follow_symlinks)


def _walk(path:os.PathLike, *, follow_symlinks:bool=False) -> WalkGenerator:
    for entry in scandir(path):
        yield entry
        if entry.skip:
            continue
        if entry.is_dir(follow_symlinks=follow_symlinks):
            yield from _walk(entry.path)


__all__ = (
    DirEntry.__name__,
    FakeDirEntry.__name__,
    scandir.__name__,
    walk.__name__,
)
