#if PY_VERSION_HEX > 0x02070000
    #include <pycapsule.h>
#endif

#include "store.hpp"
#include "plugin.hpp"
#include "digest.hpp"
#include "track.hpp"

using namespace yappi::plugin;

typedef yappi::helpers::track<PyObject*, Py_DecRef> object_t;

void store_object_t::deallocate(store_object_t* self) {
    delete self->store_id;
    self->ob_type->tp_free(reinterpret_cast<PyObject*>(self));
}
            
int store_object_t::initialize(store_object_t* self, PyObject* args, PyObject* kwargs) {
    object_t source(NULL);

    if(!PyArg_ParseTuple(args, "O", &source)) {
        return -1;
    }

#if PY_VERSION_HEX > 0x02070000
    if(!PyCapsule_CheckExact(source)) {
#else
    if(!PyCObject_Check(source)) {
#endif
        PyErr_SetString(PyExc_RuntimeError,
            "This class cannot be instantiated directly");
        return -1;
    }

#if PY_VERSION_HEX > 0x02070000
    std::string uri = static_cast<source_t*>(
        PyCapsule_GetPointer(source, NULL))->uri();
#else
    std::string uri = static_cast<source_t*>(
        PyCObject_AsVoidPtr(source))->uri();
#endif

    self->store_id = new std::string(security::digest_t().get(uri));

    return 0;
}

PyObject* store_object_t::get(store_object_t* self, PyObject* args, PyObject* kwargs) {
    std::string error;
    Json::Value store;
    PyObject* key;

    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "S", const_cast<char**>(get_kwlist), &key)) {
        return NULL;
    }

    Py_BEGIN_ALLOW_THREADS
        try {
            store = storage::storage_t::instance()->get("storage", *self->store_id);
        } catch(const std::runtime_error& e) {
            error = e.what();
        }
    Py_END_ALLOW_THREADS
    
    if(!error.empty()) {
        PyErr_SetString(PyExc_RuntimeError, error.c_str());
        return NULL;
    }

    Json::Value value = store[PyString_AsString(key)];

    if(!value.empty()) {
        if(value.isBool()) {
            return PyBool_FromLong(value.asBool());
        } else if(value.isIntegral()) {
            return PyLong_FromLong(value.asInt());
        } else if(value.isDouble()) {
            return PyFloat_FromDouble(value.asDouble());
        } else if(value.isString()) {
            return PyString_FromString(value.asCString());
        } else {
            PyErr_SetString(PyExc_TypeError,
                "Invalid storage data format");
            return NULL;
        }
    } else {
        Py_RETURN_NONE;
    }
}

PyObject* store_object_t::set(store_object_t* self, PyObject* args, PyObject* kwargs) {
    std::string error;
    Json::Value store, object;
    PyObject *key, *value;
    
    if(!PyArg_ParseTupleAndKeywords(args, kwargs, "SO", const_cast<char**>(set_kwlist), &key, &value)) {
        return NULL;
    }
   
    if(PyBool_Check(value)) {
        object = (value == Py_True ? true : false);
    } else if(PyInt_Check(value)) {
        object = static_cast<Json::Int>(PyInt_AsLong(value));
    } else if(PyLong_Check(value)) {
        object = static_cast<Json::Int>(PyLong_AsLong(value));
    } else if(PyFloat_Check(value)) {
        object = PyFloat_AsDouble(value);
    } else if(PyString_Check(value)) {
        object = PyString_AsString(value);
    } else {
        PyErr_SetString(PyExc_TypeError,
            "Only primitive types are allowed");
        return NULL;
    }

    Py_BEGIN_ALLOW_THREADS
        try {
            store = storage::storage_t::instance()->get("storage", *self->store_id);
        } catch(const std::runtime_error& e) {
            error = e.what();
        }
    Py_END_ALLOW_THREADS
  
    if(!error.empty()) {
        PyErr_SetString(PyExc_RuntimeError, error.c_str());
        return NULL;
    }

    store[PyString_AsString(key)] = object;

    Py_BEGIN_ALLOW_THREADS
        try {
            storage::storage_t::instance()->put("storage", *self->store_id, store);
        } catch(const std::runtime_error& e) {
            error = e.what();
        }
    Py_END_ALLOW_THREADS

    if(!error.empty()) {
        PyErr_SetString(PyExc_RuntimeError, error.c_str());
        return NULL;
    }

    Py_RETURN_TRUE;
}

const char* store_object_t::get_kwlist[] = { "key", NULL };
const char* store_object_t::set_kwlist[] = { "key", "value", NULL };
