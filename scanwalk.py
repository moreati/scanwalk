import os
import stat


class FakeDirEntry(os.PathLike):
    '''
    A stand-in for os.DirEntry, that can be instantiated directly.
    '''
    def __init__(self, path):
        self.path = path

    def __fspath__(self):
        return self.path

    @property
    def name(self):
        return os.path.basename(self.path)

    def is_dir(self, *, follow_symlinks=True):
        return stat.S_ISDIR(self.stat(follow_symlinks=follow_symlinks).st_mode)

    def is_file(self, *, follow_symlinks=True):
        return stat.S_ISREG(self.stat(follow_symlinks=follow_symlinks).st_mode)

    def is_symlink(self):
        return stat.S_ISLNK(self.stat(follow_symlinks=False).st_mode)

    def stat(self, *, follow_symlinks=True):
        return os.stat(self.path, follow_symlinks=follow_symlinks)

    def __repr__(self):
        return f'<{self.__class__.__name__} {self.path!r}>'


def walk(top, *, follow_symlinks=False):
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


def _walk(path, *, follow_symlinks=False):
    with os.scandir(path) as it:
        for entry in it:
            skip = yield entry
            if skip:
                continue
            if entry.is_dir(follow_symlinks=follow_symlinks):
                yield from _walk(entry)


