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

|             | `os.walk()`                          | `scanwalk.walk()`                                  |
|-------------|--------------------------------------|----------------------------------------------------|
| Yields      | `(dirpath, dirnames, filenames)`     | `DirEntry` objects                                 |
| Consumers   | Nested `for` loops                   | `for` loop, generator expression, or comprehension |
| Order       | Sorted, directories & files seperate | Unsorted, directories & files intermingled         |
| Traversal   | Depth first or breadth first         | Semi depth first, directories traversed on arrival |
| Exceptions  | `onerror()` callback                 | `try`/`except` block                               |
| Allocations | Builds intermediate lists            | Direct from `os.scandir()`                         |
| Performance | 1.0x                                 | 1.1 - 1.2x faster                                  |

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
