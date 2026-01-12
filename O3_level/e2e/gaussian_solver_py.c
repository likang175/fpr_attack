#include <Python.h>
#include <numpy/arrayobject.h>
#include "gaussian_solver.h"

#define UNUSED(x) (void)(x)

static char module_docstring[] =
    "A C extension for solving f from h and g in Falcon lattice-based signatures";

static char solve_unknown_f_docstring[] =
    "Solve for unknown f coefficients given partial f, g, and complete h.\n\n"
    "Parameters:\n"
    "    g: numpy array of int32, known g coefficients (use a mask array)\n"
    "    g_mask: numpy array of bool, True where g is known\n"
    "    f: numpy array of int32, known f coefficients\n"
    "    f_mask: numpy array of bool, True where f is known\n"
    "    h: numpy array of int32, complete h vector\n"
    "    mod: int, modulus (default 12289)\n\n"
    "Returns:\n"
    "    Tuple (success, result, message):\n"
    "    - success: bool, True if solution found\n"
    "    - result: numpy array of int32, recovered f coefficients (only unknowns)\n"
    "    - message: str, status message\n";

static char gaussian_solve_docstring[] =
    "Solve A*x ≡ b (mod q) using Gaussian elimination.\n\n"
    "Parameters:\n"
    "    A: numpy 2D array of int32, coefficient matrix (n x n)\n"
    "    b: numpy array of int32, right-hand side (length n)\n"
    "    q: int, modulus\n\n"
    "Returns:\n"
    "    Tuple (success, result, message):\n"
    "    - success: bool, True if solution found\n"
    "    - result: numpy array of int32, solution vector x\n"
    "    - message: str, status message\n";

/**
 * Python wrapper for solve_unknown_f
 */
static PyObject *py_solve_unknown_f(PyObject *self, PyObject *args) {
    UNUSED(self);

    PyObject *g_obj, *g_mask_obj, *f_obj, *f_mask_obj, *h_obj;
    int mod = 12289;  // Default Falcon modulus

    if (!PyArg_ParseTuple(args, "OOOOO|i", &g_obj, &g_mask_obj, &f_obj, &f_mask_obj, &h_obj, &mod)) {
        return NULL;
    }

    // Convert to numpy arrays
    PyArrayObject *g_arr = (PyArrayObject*)PyArray_FROM_OTF(g_obj, NPY_INT32, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *g_mask_arr = (PyArrayObject*)PyArray_FROM_OTF(g_mask_obj, NPY_BOOL, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *f_arr = (PyArrayObject*)PyArray_FROM_OTF(f_obj, NPY_INT32, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *f_mask_arr = (PyArrayObject*)PyArray_FROM_OTF(f_mask_obj, NPY_BOOL, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *h_arr = (PyArrayObject*)PyArray_FROM_OTF(h_obj, NPY_INT32, NPY_ARRAY_IN_ARRAY);

    if (!g_arr || !g_mask_arr || !f_arr || !f_mask_arr || !h_arr) {
        Py_XDECREF(g_arr);
        Py_XDECREF(g_mask_arr);
        Py_XDECREF(f_arr);
        Py_XDECREF(f_mask_arr);
        Py_XDECREF(h_arr);
        PyErr_SetString(PyExc_TypeError, "Arguments must be numpy arrays");
        return NULL;
    }

    // Check dimensions
    if (PyArray_NDIM(g_arr) != 1 || PyArray_NDIM(g_mask_arr) != 1 ||
        PyArray_NDIM(f_arr) != 1 || PyArray_NDIM(f_mask_arr) != 1 ||
        PyArray_NDIM(h_arr) != 1) {
        Py_DECREF(g_arr); Py_DECREF(g_mask_arr);
        Py_DECREF(f_arr); Py_DECREF(f_mask_arr);
        Py_DECREF(h_arr);
        PyErr_SetString(PyExc_ValueError, "All arrays must be 1-dimensional");
        return NULL;
    }

    int n = PyArray_DIM(h_arr, 0);
    if (PyArray_DIM(g_arr, 0) != n || PyArray_DIM(g_mask_arr, 0) != n ||
        PyArray_DIM(f_arr, 0) != n || PyArray_DIM(f_mask_arr, 0) != n) {
        Py_DECREF(g_arr); Py_DECREF(g_mask_arr);
        Py_DECREF(f_arr); Py_DECREF(f_mask_arr);
        Py_DECREF(h_arr);
        PyErr_SetString(PyExc_ValueError, "All arrays must have the same length");
        return NULL;
    }

    // Get data pointers
    int32_t *g = (int32_t*)PyArray_DATA(g_arr);
    bool *g_mask = (bool*)PyArray_DATA(g_mask_arr);
    int32_t *f = (int32_t*)PyArray_DATA(f_arr);
    bool *f_mask = (bool*)PyArray_DATA(f_mask_arr);
    int32_t *h = (int32_t*)PyArray_DATA(h_arr);

    // Call the C function
    solve_result_t result = solve_unknown_f(g, g_mask, f, f_mask, h, n, mod);

    // Prepare return values
    PyObject *success = (result.status == SOLVE_OK) ? Py_True : Py_False;
    Py_INCREF(success);

    PyObject *message = PyUnicode_FromString(result.message);

    PyObject *result_arr = NULL;
    if (result.status == SOLVE_OK && result.x != NULL) {
        // Create numpy array from result
        npy_intp dims[1] = {result.n};
        result_arr = PyArray_SimpleNew(1, dims, NPY_INT32);
        int32_t *data = (int32_t*)PyArray_DATA((PyArrayObject*)result_arr);
        for (int i = 0; i < result.n; i++) {
            data[i] = result.x[i];
        }
    } else {
        result_arr = Py_None;
        Py_INCREF(Py_None);
    }

    // Cleanup
    free_solve_result(&result);
    Py_DECREF(g_arr);
    Py_DECREF(g_mask_arr);
    Py_DECREF(f_arr);
    Py_DECREF(f_mask_arr);
    Py_DECREF(h_arr);

    return Py_BuildValue("(OOO)", success, result_arr, message);
}

/**
 * Python wrapper for gaussian_mod_q_solve
 */
static PyObject *py_gaussian_solve(PyObject *self, PyObject *args) {
    UNUSED(self);

    PyObject *A_obj, *b_obj;
    int q = 12289;

    if (!PyArg_ParseTuple(args, "OO|i", &A_obj, &b_obj, &q)) {
        return NULL;
    }

    // Convert to numpy arrays
    PyArrayObject *A_arr = (PyArrayObject*)PyArray_FROM_OTF(A_obj, NPY_INT32, NPY_ARRAY_IN_ARRAY);
    PyArrayObject *b_arr = (PyArrayObject*)PyArray_FROM_OTF(b_obj, NPY_INT32, NPY_ARRAY_IN_ARRAY);

    if (!A_arr || !b_arr) {
        Py_XDECREF(A_arr);
        Py_XDECREF(b_arr);
        PyErr_SetString(PyExc_TypeError, "Arguments must be numpy arrays");
        return NULL;
    }

    // Check dimensions
    if (PyArray_NDIM(A_arr) != 2 || PyArray_NDIM(b_arr) != 1) {
        Py_DECREF(A_arr);
        Py_DECREF(b_arr);
        PyErr_SetString(PyExc_ValueError, "A must be 2D, b must be 1D");
        return NULL;
    }

    int n = PyArray_DIM(A_arr, 0);
    int m = PyArray_DIM(A_arr, 1);
    if (n != m || PyArray_DIM(b_arr, 0) != n) {
        Py_DECREF(A_arr);
        Py_DECREF(b_arr);
        PyErr_SetString(PyExc_ValueError, "A must be square and b must match A's size");
        return NULL;
    }

    // Extract matrix data
    int32_t **A = (int32_t**)malloc(n * sizeof(int32_t*));
    for (int i = 0; i < n; i++) {
        A[i] = (int32_t*)malloc(n * sizeof(int32_t));
        for (int j = 0; j < n; j++) {
            A[i][j] = *(int32_t*)PyArray_GETPTR2(A_arr, i, j);
        }
    }

    int32_t *b = (int32_t*)PyArray_DATA(b_arr);

    // Call the C function
    solve_result_t result = gaussian_mod_q_solve(A, b, n, q);

    // Prepare return values
    PyObject *success = (result.status == SOLVE_OK) ? Py_True : Py_False;
    Py_INCREF(success);

    PyObject *message = PyUnicode_FromString(result.message);

    PyObject *result_arr = NULL;
    if (result.status == SOLVE_OK && result.x != NULL) {
        npy_intp dims[1] = {result.n};
        result_arr = PyArray_SimpleNew(1, dims, NPY_INT32);
        int32_t *data = (int32_t*)PyArray_DATA((PyArrayObject*)result_arr);
        for (int i = 0; i < result.n; i++) {
            data[i] = result.x[i];
        }
    } else {
        result_arr = Py_None;
        Py_INCREF(Py_None);
    }

    // Cleanup
    for (int i = 0; i < n; i++) free(A[i]);
    free(A);
    free_solve_result(&result);
    Py_DECREF(A_arr);
    Py_DECREF(b_arr);

    return Py_BuildValue("(OOO)", success, result_arr, message);
}

/**
 * Module method definitions
 */
static PyMethodDef module_methods[] = {
    {"solve_unknown_f", py_solve_unknown_f, METH_VARARGS, solve_unknown_f_docstring},
    {"gaussian_solve", py_gaussian_solve, METH_VARARGS, gaussian_solve_docstring},
    {NULL, NULL, 0, NULL}
};

/**
 * Module definition
 */
static struct PyModuleDef solve_module = {
    PyModuleDef_HEAD_INIT,
    "gaussian_solver",
    module_docstring,
    -1,
    module_methods
};

/**
 * Module initialization
 */
PyMODINIT_FUNC PyInit_gaussian_solver(void) {
    PyObject *module = PyModule_Create(&solve_module);

    if (module == NULL) {
        return NULL;
    }

    import_array();
    return module;
}
