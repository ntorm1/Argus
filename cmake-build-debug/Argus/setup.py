from distutils.core import setup

import sys
if sys.version_info < (3, 0):
    sys.exit('Sorry, Python < 3.0 is not supported')

setup(
    name='cmake_cpp_pybind11',
    version='0.1',  # TODO: might want to use commit ID here
    packages=['Argus'],
    package_dir={
        '': '/Users/nathantormaschy/CLionProjects/Argus/cmake-build-debug'
    },
    package_data={
        '': ['Argus.so']
    }
)
