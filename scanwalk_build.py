import cffi

ffibuilder = cffi.FFI()

ffibuilder.cdef('''
#define DT_BLK      ...
#define DT_CHR      ...
#define DT_DIR      ...
#define DT_FIFO     ...
#define DT_LNK      ...
#define DT_REG      ...
#define DT_SOCK     ...
#define DT_UNKNOWN  ...

typedef int... ino_t;

struct dirent {
    ino_t           d_ino;          /* POSIX */
    unsigned char   d_type;         /* BSD 4.4, glibc, macOS 10.6+, ... */
    char            d_name[];       /* POSIX */
    uint16_t        d_namlen;
    ...;
};

int scandir(
    const char *dirname,
    struct dirent ***namelist,
    int (*select)(const struct dirent *),
    int (*compar)(const struct dirent **, const struct dirent **)
);
''')

ffibuilder.set_source(
    '_scanwalk',
    '''
#include <dirent.h>
''',
)
