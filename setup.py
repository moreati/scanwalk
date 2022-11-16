#!/usr/bin/env python3

import Cython.Build
import setuptools

setuptools.setup(
    ext_modules=Cython.Build.cythonize('scanwalk.pyx'),
    zip_safe=False,
)
