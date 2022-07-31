scanwalk
========

`scanwalk.walk()` walks a directory tree, generating `DirEntry` objects.
It's an alternative to `os.walk()` modelled on `os.scandir()`.

```pycon
>>> import scanwalk
>>> for entry in scanwalk.walk('data/demo'):
...     print(entry.path, entry.name, entry.is_dir(), entry.is_file())
...
data/demo demo True False
data/demo/adir adir True False
data/demo/adir/anotherfile anotherfile False True
data/demo/adir/anotherdir anotherdir True False
data/demo/afile afile False True
```

a rough equivalent with `os.walk()` would be

```pycon
>>> import os
>>> for parent, dirs, files in os.walk('data/demo'):
...     print(parent, name, True, False)
...     for name in dirs:
...         print(os.path.join(parent, name), name, True, False)
...     for name in files:
...         print(os.path.join(parent, name), name, False, True)
...
data/demo demo True False
data/demo/adir adir True False
data/demo/afile afile False True
data/demo/adir/anotherdir anotherdir True False
data/demo/adir/anotherfile anotherfile False True
```

Notable features and differences between `scanwalk.walk()` and `os.walk()`

- `scanwalk` generates a flat stream of `DirEntry` objects.
  Nested loops aren't needed.
- `scanwalk` doesn't sort entries.
  Directories and files are intermingled (within a given parent directory).
- `scanwalk` descends directories as it encounters them.
  It's neither depth first or breadth first. `os.walk()` supports both.
- `scanwalk` doesn't build intermediate lists
- `scanwalk` doesn't need an `onerror()` callback.
- `scanwalk` can be 10-20% faster.

Installation
------------

```sh
python -m pip install scanwalk
```

Requirements
------------

- Python 3.6+

License
-------

MIT
