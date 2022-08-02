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

## Installation

```sh
python -m pip install scanwalk
```

## Requirements

- Python 3.7+

## License

MIT

## Questions and Answers

### What's wrong with `os.walk()`?

`scanwalk.walk()` isn't better or worse then `os.walk()`, each has tradeoffs.
`os.walk()` is fine for most use cases, if you're happy with it then carry on.

### Why use `scanwalk`?
`scanwalk.walk()` eeks out a little more speed (10-20% in an adhoc benchmark).
It doesn't require nested for loops, so code is easier to read and write.
In particular list comprehensions  and generator expressions become simpler.

### Why not use `scanwalk`?
`scanwalk` is still alpha, mostly untested, and almost entirely undocumented.
It only supports newer Pythons, on platforms with a working `os.scandir()`.

`scanwalk.walk()` lacks features compared to `os.walk()`
- entries aren't sorted, they arrive in an undefined order
- there's no control over traversal order (e.g. depth first, breadth first)
- there's no way to skip directories

## Related work

- [`scandir`](https://pypi.org/project/scandir/) - backport of `os.scandir()`
  for Python 2.7 and 3.4

## TODO

- Expose directory skip mechanism, probably `generator.send()`
- Implement context manager protocol, similar to `os.scandir()`
- Documentation
- Tests
- Continuous Integration
- Coverage
- Code quality checks (MyPy, flake8, etc.)
- `scanwalk.copytree()`?
- `scanwalk.DirEntry.depth`?
- Linux io_uring support?
