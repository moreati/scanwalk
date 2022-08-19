#!/usr/bin/env python3

import platform
import sys

import setuptools
import wheel.bdist_wheel


class bdist_wheel(wheel.bdist_wheel.bdist_wheel):
    def finalize_options(self) -> None:
        self.py_limited_api = f'cp{sys.version_info.major}{sys.version_info.minor}'
        super().finalize_options()


if platform.python_implementation() == 'CPython':
    cmdclass = {'bdist_wheel': bdist_wheel}
else:
    cmdclass = {}

setuptools.setup(
    cffi_modules=['scanwalk_build.py:ffibuilder'],
    cmdclass=cmdclass,
)
