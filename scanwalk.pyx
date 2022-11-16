# cython: language_level=3
from __future__ import annotations

import os
import stat
from typing import AnyStr
from typing import Generator
from typing import Union

from cpython.exc cimport PyErr_SetFromErrnoWithFilenameObject
from libc.stdlib cimport free, malloc
from posix.stat cimport S_ISDIR, S_ISLNK, S_ISREG
from posix.stat cimport lstat, stat, struct_stat
from posix.types cimport ino_t


cdef extern from "<dirent.h>":
    const unsigned char DT_BLK
    const unsigned char DT_CHR
    const unsigned char DT_DIR
    const unsigned char DT_FIFO
    const unsigned char DT_LNK
    const unsigned char DT_REG
    const unsigned char DT_SOCK
    const unsigned char DT_UNKNOWN

    cdef struct dirent:
        ino_t           d_ino           # POSIX
        unsigned char   d_type          # BSD 4.4, glibc, macOS 10.6+, ...
        # char d_name[] in the POSIX standard, but Cython didn't like that
        char*           d_name          # POSIX

    int scandir(
        const char* dirname,
        dirent*** namelist,
        int (* select)(const dirent* d),
        int (* compar)(const dirent** d1, const dirent** d2)
    )


cdef class DirEntry:
    cdef readonly str name
    cdef readonly str path
    cdef public bint skip
    cdef bytes pathb
    cdef dirent* dirent
    cdef struct_stat* st
    cdef struct_stat* lst

    def __cinit__(self, char* path, bint follow_symlinks):
        cdef struct_stat* st
        self.path = os.fsdecode(path)
        self.name = os.path.basename(self.path)
        self.skip = False
        self.pathb = os.fsencode(self)      

    @staticmethod
    cdef DirEntry from_dirent(char* parent, dirent* dirent):
        entry = DirEntry()
        entry.name = os.fsdecode(dirent.d_name)
        entry.path = os.path.join(os.fsdecode(parent), entry.name)
        entry.dirent = dirent
        entry.pathb = os.fsencode(entry.path)
        entry.skip = False
        return entry

    def __dealloc__(self):
        if self.dirent is not NULL: free(self.dirent)
        if self.st is not NULL: free(self.st)
        if self.lst is not NULL: free(self.lst)

    cdef struct_stat* get_lstat(self):
        cdef char* dirp = self.pathb
        cdef int res
        if self.lst is NULL:
            self.lst = <struct_stat *> malloc(sizeof(struct_stat))
            if not self.lst:
                raise MemoryError()
            res = lstat(dirp, self.lst)
            if res != 0:
                PyErr_SetFromErrnoWithFilenameObject(OSError, self.pathb)
        return self.lst

    cdef struct_stat* get_stat(self, bint follow_symlinks):
        cdef char* dirp = self.pathb
        cdef int res
        if follow_symlinks:
            if self.st is NULL:
                self.st = <struct_stat *> malloc(sizeof(struct_stat))
                if not self.st:
                    raise MemoryError()
                res = stat(dirp, self.st)
                if res != 0:
                    PyErr_SetFromErrnoWithFilenameObject(OSError, self.pathb)
            return self.st
        else:
            return self.get_lstat()

    cpdef ino_t inode(self):
        cdef dirent d = self.dirent[0]
        return d.d_ino

    cpdef bint is_dir(self, bint follow_symlinks=True):
        if self.dirent is NULL or follow_symlinks:
            return S_ISDIR(self.get_stat(follow_symlinks)[0].st_mode)
        return self.dirent.d_type == DT_DIR

    cpdef bint is_file(self, bint follow_symlinks=True):
        if self.dirent is NULL or follow_symlinks:
            return S_ISREG(self.get_stat(follow_symlinks)[0].st_mode)
        return self.dirent.d_type == DT_REG

    cpdef bint is_symlink(self):
        if self.dirent is NULL:
            return S_ISLNK(self.get_lstat()[0].st_mode)
        return self.dirent.d_type == DT_LNK

    #def os.stat_result stat(self, bint follow_symlinks=True):
    #    return os.stat(self.path, follow_symlinks=follow_symlinks)

    def __fspath__(self) -> AnyStr:
        return os.fspath(self.path)

    def __repr__(self) -> str:
        return f'<{self.__class__.__name__} {self.path!r}>'

#WalkGenerator = Generator[Union[os.DirEntry, FakeDirEntry], None, None]

def _scandir(str path):
    cdef int res
    cdef bytes path_b
    cdef char* dirp
    cdef dirent** namelist
    cdef dirent* dirent
    cdef DirEntry direntry

    path_b = os.fsencode(path)
    dirp = path_b
    res = scandir(dirp, &namelist, NULL, NULL)
    if res < 1:
        raise OSError

    try:
        for i in range(res):
            dirent = namelist[i]
            if dirent.d_name == b'.' or dirent.d_name == b'..':
                continue
            direntry = DirEntry.from_dirent(dirp, dirent)
            yield direntry
    finally:
        free(namelist)


def walk(top:str, follow_symlinks:bool=False):
    """
    Generate DirEntry objects for top, and directories/files under top
    (excluding '.' and '..' entries).

    It aims to be a faster alternative to `os.walk()`. It uses `os.scandir()`
    output directly, avoiding intermediate lists and sort operations.
    """
    yield DirEntry(top, follow_symlinks)
    yield from _walk(top, follow_symlinks=follow_symlinks)


def _walk(path:str, follow_symlinks:bool=False):
    for entry in _scandir(path):
        yield entry
        if entry.skip:
            continue
        if entry.is_dir(follow_symlinks=follow_symlinks):
            yield from _walk(entry.path)
