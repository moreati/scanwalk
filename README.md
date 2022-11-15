scanwalk
========

`scanwalk.walk()` walks a directory tree, generating `DirEntry` objects.
It's an alternative to `os.walk()` modelled on `os.scandir()`.

```pycon
>>> import scanwalk
>>> for entry in scanwalk.walk('demo'):
...     print('📁' if entry.is_dir() else '📄', entry.path)
...
📁 demo
📁 demo/dir2
📁 demo/dir1
📁 demo/dir1/dir1.1
📄 demo/dir1/dir1.1/file_a
📄 demo/dir1/file_c
📁 demo/dir1/dir1.2
📄 demo/dir1/dir1.2/file_b
```

a rough equivalent using `os.walk()` would be

```pycon
>>> import os
>>> for parent, dirnames, filenames in os.walk('demo'):
...     print('📁', parent)
...     for name in filenames:
...         print('📄', os.path.join(parent, name))
...
📁 demo
📁 demo/dir2
📁 demo/dir1
📄 demo/dir1/file_c
📁 demo/dir1/dir1.1
📄 demo/dir1/dir1.1/file_a
📁 demo/dir1/dir1.2
📄 demo/dir1/dir1.2/file_b
```

## Comparison

|             | `os.walk()`                          | `scanwalk.walk()`                                  |
|-------------|--------------------------------------|----------------------------------------------------|
| Yields      | `(dirpath, dirnames, filenames)`     | `DirEntry` objects                                 |
| Consumers   | Nested `for` loops                   | Flat `for` loop, list comprehension, or generator expression |
| Grouping    | Directories & files seperated        | Directories & files intermingled                   |
| Traversal   | Depth first or breadth first         | Semi depth first, directories traversed on arrival |
| Exceptions  | `onerror()` callback                 | `try`/`except` block                               |
| Allocations | Builds intermediate lists            | Direct from `os.scandir()`                         |
| Maturity    | Mature                               | Alpha                                              |
| Tests       | Thorough automated unit tests        | None                                               |
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

`os.walk()` is plenty good enough, it's just an awkward return type to use
inside a list comprehension, a generator expression, or similar.

### Why use `scanwalk`?
`scanwalk.walk()` eeks out a little more speed (10-20% in an adhoc benchmark).
It doesn't require nested for loops, so code is a bit easier to read and write.
In particular list comprehensions  and generator expressions become simpler.

### Why not use `scanwalk`?
`scanwalk` is still alpha, mostly untested, and almost entirely undocumented.
It only supports newer Pythons, on platforms with a working `os.scandir()`.

`scanwalk.walk()` behaviour differs from `os.walk()`
- directories and files are intermingled, rather than seperated
- Traversal is always semi depth-first
- there's no way to skip directories (WIP)

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
