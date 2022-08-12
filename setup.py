#!/usr/bin/env python3

import setuptools

setuptools.setup(
    ext_modules=[
        setuptools.Extension(
            name='_scanwalk',
            sources=[
                #'_scanwalk.h',
                #'_scanwalk.c.h',
                '_scanwalk.c',
            ],
        ),
    ],
)
