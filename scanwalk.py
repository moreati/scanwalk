from __future__ import annotations

import os
import stat
from typing import AnyStr
from typing import Generator


class FakeDirEntry(os.PathLike):
    '''
    A stand-in for os.DirEntry, that can be instantiated directly.
    '''
    def __init__(self, path:os.PathLike):
        self.path = path

    @property
    def name(self) -> AnyStr:
        return os.path.basename(self.path)

    def inode(self) -> int:
        return self.stat(follow_symlinks=False).st_inode

    def is_dir(self, *, follow_symlinks:bool=True) -> bool:
        return stat.S_ISDIR(self.stat(follow_symlinks=follow_symlinks).st_mode)

    def is_file(self, *, follow_symlinks:bool=True) -> bool:
        return stat.S_ISREG(self.stat(follow_symlinks=follow_symlinks).st_mode)

    def is_symlink(self):
        return stat.S_ISLNK(self.stat(follow_symlinks=False).st_mode)

    def stat(self, *, follow_symlinks:bool=True) -> os.stat_result:
        return os.stat(self.path, follow_symlinks=follow_symlinks)

    def __fspath__(self) -> AnyStr:
        return os.fspath(self.path)

    def __repr__(self) -> str:
        return f'<{self.__class__.__name__} {self.path!r}>'


WalkGenerator = Generator[os.DirEntry|FakeDirEntry, None, None]


def walk(top:os.PathLike, *, follow_symlinks:bool=False) -> WalkGenerator:
    """
    Generate DirEntry objects for top, and directories/files under top
    (excluding '.' and '..' entries).

    It aims to be a faster alternative to `os.walk()`. It uses `os.scandir()`
    output directly, avoiding intermediate lists and sort operations.
    """
    if not isinstance(top, (os.DirEntry, FakeDirEntry)):
        yield FakeDirEntry(top)
    else:
        yield top
    yield from _walk(top, follow_symlinks=follow_symlinks)


def _walk(path:os.PathLike, *, follow_symlinks:bool=False) -> WalkGenerator:
    with os.scandir(path) as it:
        for entry in it:
            skip = yield entry
            if skip:
                continue
            if entry.is_dir(follow_symlinks=follow_symlinks):
                yield from _walk(entry)


__all__ = (
    FakeDirEntry.__name__,
    WalkGenerator.__name__,
    walk.__name__,
)
