from setuptools import setup, Extension
import numpy as np

solve_module = Extension(
    'gaussian_solver',
    sources=['gaussian_solver_py.c', 'gaussian_solver.c'],
    include_dirs=[np.get_include()],
    extra_compile_args=['-O3', '-Wall', '-Wextra', '-std=c11', '-fopenmp'],
    extra_link_args=['-fopenmp'],
    language='c'
)

setup(
    name='gaussian_solver',
    version='1.0',
    description='C extension for solving f from h and g in Falcon signatures',
    ext_modules=[solve_module]
)
