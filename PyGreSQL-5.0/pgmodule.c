/*
 * $Id: pgmodule.c 844 2016-02-08 23:15:53Z cito $
 * PyGres, version 2.2 A Python interface for PostgreSQL database. Written by
 * D'Arcy J.M. Cain, (darcy@druid.net).  Based heavily on code written by
 * Pascal Andre, andre@chimay.via.ecp.fr. Copyright (c) 1995, Pascal Andre
 * (andre@via.ecp.fr).
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without a written
 * agreement is hereby granted, provided that the above copyright notice and
 * this paragraph and the following two paragraphs appear in all copies or in
 * any new file that contains a substantial portion of this file.
 *
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
 * SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST PROFITS,
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN IF THE
 * AUTHOR HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE AUTHOR SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND THE
 * AUTHOR HAS NO OBLIGATIONS TO PROVIDE MAINTENANCE, SUPPORT, UPDATES,
 * ENHANCEMENTS, OR MODIFICATIONS.
 *
 * Further modifications copyright 1997 to 2016 by D'Arcy J.M. Cain
 * (darcy@PyGreSQL.org) subject to the same terms and conditions as above.
 *
 */

/* Note: This should be linked against the same C runtime lib as Python */

#include <Python.h>

#include <libpq-fe.h>
#include <libpq/libpq-fs.h>

/* the type definitions from <server/catalog/pg_type.h> */
#include "pgtypes.h"

/* macros for single-source Python 2/3 compatibility */
#include "py3c.h"

static PyObject *Error, *Warning, *InterfaceError,
	*DatabaseError, *InternalError, *OperationalError, *ProgrammingError,
	*IntegrityError, *DataError, *NotSupportedError;

#define _TOSTRING(x) #x
#define TOSTRING(x) _TOSTRING(x)
static const char *PyPgVersion = TOSTRING(PYGRESQL_VERSION);

#if SIZEOF_SIZE_T != SIZEOF_INT
#define Py_InitModule4 Py_InitModule4_64
#endif

/* default values */
#define PG_ARRAYSIZE		1

/* flags for object validity checks */
#define CHECK_OPEN			1
#define CHECK_CLOSE			2
#define CHECK_CNX			4
#define CHECK_RESULT		8
#define CHECK_DQL			16

/* query result types */
#define RESULT_EMPTY		1
#define RESULT_DML			2
#define RESULT_DDL			3
#define RESULT_DQL			4

/* flags for move methods */
#define QUERY_MOVEFIRST		1
#define QUERY_MOVELAST		2
#define QUERY_MOVENEXT		3
#define QUERY_MOVEPREV		4

#define MAX_BUFFER_SIZE 8192	/* maximum transaction size */
#define MAX_ARRAY_DEPTH 16		/* maximum allowed depth of an array */

/* MODULE GLOBAL VARIABLES */

#ifdef DEFAULT_VARS
static PyObject *pg_default_host;	/* default database host */
static PyObject *pg_default_base;	/* default database name */
static PyObject *pg_default_opt;	/* default connection options */
static PyObject *pg_default_port;	/* default connection port */
static PyObject *pg_default_user;	/* default username */
static PyObject *pg_default_passwd;	/* default password */
#endif	/* DEFAULT_VARS */

static PyObject *decimal = NULL, /* decimal type */
				*namedresult = NULL, /* function for getting named results */
				*jsondecode = NULL; /* function for decoding json strings */
static const char *date_format = NULL; /* date format that is always assumed */
static char decimal_point = '.'; /* decimal point used in money values */
static int bool_as_text = 0; /* whether bool shall be returned as text */
static int array_as_text = 0; /* whether arrays shall be returned as text */
static int bytea_escaped = 0; /* whether bytea shall be returned escaped */

static int pg_encoding_utf8 = 0;
static int pg_encoding_latin1 = 0;
static int pg_encoding_ascii = 0;

/*
OBJECTS
=======

  Each object has a number of elements.  The naming scheme will be based on
  the object type.  Here are the elements using example object type "foo".
   - fooObject: A structure to hold local object information.
   - fooXxx: Object methods such as Delete and Getattr.
   - fooMethods: Methods declaration.
   - fooType: Type definition for object.

  This is followed by the object methods.

  The objects that we need to create:
   - pg: The module itself.
   - conn: Connection object returned from pg.connect().
   - notice: Notice object returned from pg.notice().
   - large: Large object returned by pg.conn.locreate() and Pg.Conn.loimport().
   - query: Query object returned by pg.conn.Conn.query().
   - source: Source object returned by pg.conn.source().
*/

/* forward declarations for types */
static PyTypeObject noticeType;
static PyTypeObject queryType;
static PyTypeObject sourceType;
static PyTypeObject largeType;
static PyTypeObject connType;

/* forward static declarations */
static void notice_receiver(void *, const PGresult *);

/* --------------------------------------------------------------------- */
/* Object declarations													 */
/* --------------------------------------------------------------------- */
typedef struct
{
	PyObject_HEAD
	int			valid;				/* validity flag */
	PGconn	   *cnx;				/* Postgres connection handle */
	const char *date_format;		/* date format derived from datestyle */
	PyObject   *cast_hook;			/* external typecast method */
	PyObject   *notice_receiver;	/* current notice receiver */
}	connObject;
#define is_connObject(v) (PyType(v) == &connType)

typedef struct
{
	PyObject_HEAD
	int			valid;			/* validity flag */
	connObject *pgcnx;			/* parent connection object */
	PGresult	*result;		/* result content */
	int			encoding; 		/* client encoding */
	int			result_type;	/* result type (DDL/DML/DQL) */
	long		arraysize;		/* array size for fetch method */
	int			current_row;	/* current selected row */
	int			max_row;		/* number of rows in the result */
	int			num_fields;		/* number of fields in each row */
}	sourceObject;
#define is_sourceObject(v) (PyType(v) == &sourceType)

typedef struct
{
	PyObject_HEAD
	connObject *pgcnx;			/* parent connection object */
	PGresult	const *res;		/* an error or warning */
}	noticeObject;
#define is_noticeObject(v) (PyType(v) == &noticeType)

typedef struct
{
	PyObject_HEAD
	connObject *pgcnx;			/* parent connection object */
	PGresult   *result;			/* result content */
	int			encoding; 		/* client encoding */
}	queryObject;
#define is_queryObject(v) (PyType(v) == &queryType)

#ifdef LARGE_OBJECTS
typedef struct
{
	PyObject_HEAD
	connObject *pgcnx;			/* parent connection object */
	Oid			lo_oid;			/* large object oid */
	int			lo_fd;			/* large object fd */
}	largeObject;
#define is_largeObject(v) (PyType(v) == &largeType)
#endif /* LARGE_OBJECTS */

/* PyGreSQL internal types */

/* simple types */
#define PYGRES_INT 1
#define PYGRES_LONG 2
#define PYGRES_FLOAT 3
#define PYGRES_DECIMAL 4
#define PYGRES_MONEY 5
#define PYGRES_BOOL 6
/* text based types */
#define PYGRES_TEXT 8
#define PYGRES_BYTEA 9
#define PYGRES_JSON 10
#define PYGRES_OTHER 11
/* array types */
#define PYGRES_ARRAY 16

/* --------------------------------------------------------------------- */
/* Internal Functions													 */
/* --------------------------------------------------------------------- */

/* shared function for encoding and decoding strings */

static PyObject *
get_decoded_string(const char *str, Py_ssize_t size, int encoding)
{
	if (encoding == pg_encoding_utf8)
		return PyUnicode_DecodeUTF8(str, size, "strict");
	if (encoding == pg_encoding_latin1)
		return PyUnicode_DecodeLatin1(str, size, "strict");
	if (encoding == pg_encoding_ascii)
		return PyUnicode_DecodeASCII(str, size, "strict");
	/* encoding name should be properly translated to Python here */
	return PyUnicode_Decode(str, size,
		pg_encoding_to_char(encoding), "strict");
}

static PyObject *
get_encoded_string(PyObject *unicode_obj, int encoding)
{
	if (encoding == pg_encoding_utf8)
		return PyUnicode_AsUTF8String(unicode_obj);
	if (encoding == pg_encoding_latin1)
		return PyUnicode_AsLatin1String(unicode_obj);
	if (encoding == pg_encoding_ascii)
		return PyUnicode_AsASCIIString(unicode_obj);
	/* encoding name should be properly translated to Python here */
	return PyUnicode_AsEncodedString(unicode_obj,
		pg_encoding_to_char(encoding), "strict");
}

/* helper functions */

/* get PyGreSQL internal types for a PostgreSQL type */
static int
get_type(Oid pgtype)
{
	int t;

	switch (pgtype)
	{
		/* simple types */

		case INT2OID:
		case INT4OID:
		case CIDOID:
		case OIDOID:
		case XIDOID:
			t = PYGRES_INT;
			break;

		case INT8OID:
			t = PYGRES_LONG;
			break;

		case FLOAT4OID:
		case FLOAT8OID:
			t = PYGRES_FLOAT;
			break;

		case NUMERICOID:
			t = PYGRES_DECIMAL;
			break;

		case CASHOID:
			t = decimal_point ? PYGRES_MONEY : PYGRES_TEXT;
			break;

		case BOOLOID:
			t = PYGRES_BOOL;
			break;

		case BYTEAOID:
			t = bytea_escaped ? PYGRES_TEXT : PYGRES_BYTEA;
			break;

		case JSONOID:
		case JSONBOID:
			t = jsondecode ? PYGRES_JSON : PYGRES_TEXT;
			break;

		case BPCHAROID:
		case CHAROID:
		case TEXTOID:
		case VARCHAROID:
		case NAMEOID:
		case REGTYPEOID:
			t = PYGRES_TEXT;
			break;

		/* array types */

		case INT2ARRAYOID:
		case INT4ARRAYOID:
		case CIDARRAYOID:
		case OIDARRAYOID:
		case XIDARRAYOID:
			t = array_as_text ? PYGRES_TEXT : (PYGRES_INT | PYGRES_ARRAY);
			break;

		case INT8ARRAYOID:
			t = array_as_text ? PYGRES_TEXT : (PYGRES_LONG | PYGRES_ARRAY);
			break;

		case FLOAT4ARRAYOID:
		case FLOAT8ARRAYOID:
			t = array_as_text ? PYGRES_TEXT : (PYGRES_FLOAT | PYGRES_ARRAY);
			break;

		case NUMERICARRAYOID:
			t = array_as_text ? PYGRES_TEXT : (PYGRES_DECIMAL | PYGRES_ARRAY);
			break;

		case CASHARRAYOID:
			t = array_as_text ? PYGRES_TEXT : ((decimal_point ?
				PYGRES_MONEY : PYGRES_TEXT) | PYGRES_ARRAY);
			break;

		case BOOLARRAYOID:
			t = array_as_text ? PYGRES_TEXT : (PYGRES_BOOL | PYGRES_ARRAY);
			break;

		case BYTEAARRAYOID:
			t = array_as_text ? PYGRES_TEXT : ((bytea_escaped ?
			    PYGRES_TEXT : PYGRES_BYTEA) | PYGRES_ARRAY);
			break;

		case JSONARRAYOID:
		case JSONBARRAYOID:
			t = array_as_text ? PYGRES_TEXT : ((jsondecode ?
			    PYGRES_JSON : PYGRES_TEXT) | PYGRES_ARRAY);
			break;

		case BPCHARARRAYOID:
		case CHARARRAYOID:
		case TEXTARRAYOID:
		case VARCHARARRAYOID:
		case NAMEARRAYOID:
		case REGTYPEARRAYOID:
			t = array_as_text ? PYGRES_TEXT : (PYGRES_TEXT | PYGRES_ARRAY);
			break;

		default:
			t = PYGRES_OTHER;
	}

	return t;
}

/* get PyGreSQL column types for all result columns */
static int *
get_col_types(PGresult *result, int nfields)
{
	int *types, *t, j;

	if (!(types = PyMem_Malloc(sizeof(int) * nfields)))
		return (int *)PyErr_NoMemory();

	for (j = 0, t=types; j < nfields; ++j)
		*t++ = get_type(PQftype(result, j));

	return types;
}

/* Cast a bytea encoded text based type to a Python object.
   This assumes the text is null-terminated character string. */
static PyObject *
cast_bytea_text(char *s)
{
	PyObject   *obj;
	char	   *tmp_str;
	size_t		str_len;

    /* this function should not be called when bytea_escaped is set */
	tmp_str = (char *)PQunescapeBytea((unsigned char*)s, &str_len);
	obj = PyBytes_FromStringAndSize(tmp_str, str_len);
	if (tmp_str)
		PQfreemem(tmp_str);
	return obj;
}

/* Cast a text based type to a Python object.
   This needs the character string, size and encoding. */
static PyObject *
cast_sized_text(char *s, Py_ssize_t size, int encoding, int type)
{
	PyObject   *obj, *tmp_obj;
	char	   *tmp_str;
	size_t		str_len;

	switch (type) /* this must be the PyGreSQL internal type */
	{
		case PYGRES_BYTEA:
		    /* this type should not be passed when bytea_escaped is set */
			/* we need to add a null byte */
			tmp_str = (char *) PyMem_Malloc(size + 1);
			if (!tmp_str) return PyErr_NoMemory();
			memcpy(tmp_str, s, size);
			s = tmp_str; *(s + size) = '\0';
			tmp_str = (char *)PQunescapeBytea((unsigned char*)s, &str_len);
			PyMem_Free(s);
			if (!tmp_str) return PyErr_NoMemory();
			obj = PyBytes_FromStringAndSize(tmp_str, str_len);
			if (tmp_str)
				PQfreemem(tmp_str);
			break;

		case PYGRES_JSON:
		 	/* this type should only be passed when jsondecode is set */
			obj = get_decoded_string(s, size, encoding);
			if (obj && jsondecode) /* was able to decode */
			{
				tmp_obj = Py_BuildValue("(O)", obj);
				obj = PyObject_CallObject(jsondecode, tmp_obj);
				Py_DECREF(tmp_obj);
			}
			break;

		default:  /* PYGRES_TEXT */
#if IS_PY3
			obj = get_decoded_string(s, size, encoding);
			if (!obj) /* cannot decode */
#endif
			obj = PyBytes_FromStringAndSize(s, size);
	}

	return obj;
}

/* Cast an arbitrary type to a Python object using a callback function.
   This needs the character string, size, encoding, the Postgres type
   and the external typecast function to be called. */
static PyObject *
cast_other(char *s, Py_ssize_t size, int encoding, int pgtype,
	PyObject *cast_hook)
{
	PyObject *obj;

	obj = cast_sized_text(s, size, encoding, PYGRES_TEXT);

	if (cast_hook)
	{
		PyObject *tmp_obj = obj;
		obj = PyObject_CallFunction(cast_hook, "(Oi)", obj, pgtype);
		Py_DECREF(tmp_obj);
	}
	return obj;
}

/* Cast a simple type to a Python object.
   This needs a character string representation with a given size. */
static PyObject *
cast_sized_simple(char *s, Py_ssize_t size, int type)
{
	PyObject   *obj, *tmp_obj;
	char		buf[64], *t;
	int			i, j, n;

	switch (type) /* this must be the PyGreSQL internal type */
	{
		case PYGRES_INT:
			n = sizeof(buf)/sizeof(buf[0]) - 1;
			if (size < n) n = size;
			for (i = 0, t = buf; i < n; ++i) *t++ = *s++;
			*t = '\0';
			obj = PyInt_FromString(buf, NULL, 10);
			break;

		case PYGRES_LONG:
			n = sizeof(buf)/sizeof(buf[0]) - 1;
			if (size < n) n = size;
			for (i = 0, t = buf; i < n; ++i) *t++ = *s++;
			*t = '\0';
			obj = PyLong_FromString(buf, NULL, 10);
			break;

		case PYGRES_FLOAT:
			tmp_obj = PyStr_FromStringAndSize(s, size);
			obj = PyFloat_FromString(tmp_obj);
			Py_DECREF(tmp_obj);
			break;

		case PYGRES_MONEY:
			/* this type should only be passed when decimal_point is set */
			n = sizeof(buf)/sizeof(buf[0]) - 1;
			for (i = 0, j = 0; i < size && j < n; ++i, ++s)
			{
				if (*s >= '0' && *s <= '9')
					buf[j++] = *s;
				else if (*s == decimal_point)
					buf[j++] = '.';
				else if (*s == '(' || *s == '-')
					buf[j++] = '-';
			}
			if (decimal)
			{
				buf[j] = '\0';
				obj = PyObject_CallFunction(decimal, "(s)", buf);
			}
			else
			{
				tmp_obj = PyStr_FromString(buf);
				obj = PyFloat_FromString(tmp_obj);
				Py_DECREF(tmp_obj);

			}
			break;

		case PYGRES_DECIMAL:
			tmp_obj = PyStr_FromStringAndSize(s, size);
			obj = decimal ? PyObject_CallFunctionObjArgs(
				decimal, tmp_obj, NULL) : PyFloat_FromString(tmp_obj);
			Py_DECREF(tmp_obj);
			break;

		case PYGRES_BOOL:
			/* convert to bool only if bool_as_text is not set */
			if (bool_as_text)
			{
				obj = PyStr_FromString(*s == 't' ? "t" : "f");
			}
			else
			{
				obj = *s == 't' ? Py_True : Py_False;
				Py_INCREF(obj);
			}
			break;

		default:
			/* other types should never be passed, use cast_sized_text */
			obj = PyStr_FromStringAndSize(s, size);
	}

	return obj;
}

/* Cast a simple type to a Python object.
   This needs a null-terminated character string representation. */
static PyObject *
cast_unsized_simple(char *s, int type)
{
	PyObject   *obj, *tmp_obj;
	char		buf[64];
	int			j, n;

	switch (type) /* this must be the PyGreSQL internal type */
	{
		case PYGRES_INT:
			obj = PyInt_FromString(s, NULL, 10);
			break;

		case PYGRES_LONG:
			obj = PyLong_FromString(s, NULL, 10);
			break;

		case PYGRES_FLOAT:
			tmp_obj = PyStr_FromString(s);
			obj = PyFloat_FromString(tmp_obj);
			Py_DECREF(tmp_obj);
			break;

		case PYGRES_MONEY:
			/* this type should only be passed when decimal_point is set */
			n = sizeof(buf)/sizeof(buf[0]) - 1;
			for (j = 0; *s && j < n; ++s)
			{
				if (*s >= '0' && *s <= '9')
					buf[j++] = *s;
				else if (*s == decimal_point)
					buf[j++] = '.';
				else if (*s == '(' || *s == '-')
					buf[j++] = '-';
			}
			buf[j] = '\0'; s = buf;
			/* FALLTHROUGH */ /* no break here */

		case PYGRES_DECIMAL:
			if (decimal)
			{
				obj = PyObject_CallFunction(decimal, "(s)", s);
			}
			else
			{
				tmp_obj = PyStr_FromString(s);
				obj = PyFloat_FromString(tmp_obj);
				Py_DECREF(tmp_obj);
			}
			break;

		case PYGRES_BOOL:
			/* convert to bool only if bool_as_text is not set */
			if (bool_as_text)
			{
				obj = PyStr_FromString(*s == 't' ? "t" : "f");
			}
			else
			{
				obj = *s == 't' ? Py_True : Py_False;
				Py_INCREF(obj);
			}
			break;

		default:
			/* other types should never be passed, use cast_sized_text */
			obj = PyStr_FromString(s);
	}

	return obj;
}

/* quick case insensitive check if given sized string is null */
#define STR_IS_NULL(s, n) (n == 4 \
	&& (s[0] == 'n' || s[0] == 'N') \
	&& (s[1] == 'u' || s[1] == 'U') \
	&& (s[2] == 'l' || s[2] == 'L') \
	&& (s[3] == 'l' || s[3] == 'L'))

/* Cast string s with size and encoding to a Python list,
   using the input and output syntax for arrays.
   Use internal type or cast function to cast elements.
   The parameter delim specifies the delimiter for the elements,
   since some types do not use the default delimiter of a comma. */
static PyObject *
cast_array(char *s, Py_ssize_t size, int encoding,
	 int type, PyObject *cast, char delim)
{
	PyObject   *result, *stack[MAX_ARRAY_DEPTH];
	char	   *end = s + size, *t;
	int			depth, ranges = 0, level = 0;

	if (type)
	{
		type &= ~PYGRES_ARRAY; /* get the base type */
		if (!type) type = PYGRES_TEXT;
	}
	if (!delim)
		delim = ',';
	else if (delim == '{' || delim =='}' || delim=='\\')
	{
		PyErr_SetString(PyExc_ValueError, "Invalid array delimiter");
		return NULL;
	}

	/* strip blanks at the beginning */
	while (s != end && *s == ' ') ++s;
	if (*s == '[') /* dimension ranges */
	{
		int valid;

		for (valid = 0; !valid;)
		{
			if (s == end || *s++ != '[') break;
			while (s != end && *s == ' ') ++s;
			if (s != end && (*s == '+' || *s == '-')) ++s;
			if (s == end || *s <= '0' || *s >= '9') break;
			while (s != end && *s >= '0' && *s <= '9') ++s;
			if (s == end || *s++ != ':') break;
			if (s != end && (*s == '+' || *s == '-')) ++s;
			if (s == end || *s <= '0' || *s >= '9') break;
			while (s != end && *s >= '0' && *s <= '9') ++s;
			if (s == end || *s++ != ']') break;
			while (s != end && *s == ' ') ++s;
			++ranges;
			if (s != end && *s == '=')
			{
				do ++s; while (s != end && *s == ' ');
				valid = 1;
			}
		}
		if (!valid)
		{
			PyErr_SetString(PyExc_ValueError, "Invalid array dimensions");
			return NULL;
		}
	}
	for (t = s, depth = 0; t != end && (*t == '{' || *t == ' '); ++t)
		if (*t == '{') ++depth;
	if (!depth)
	{
		PyErr_SetString(PyExc_ValueError,
			"Array must start with a left brace");
		return NULL;
	}
	if (ranges && depth != ranges)
	{
		PyErr_SetString(PyExc_ValueError,
			"Array dimensions do not match content");
		return NULL;
	}
	if (depth > MAX_ARRAY_DEPTH)
	{
		PyErr_SetString(PyExc_ValueError, "Array is too deeply nested");
		return NULL;
	}
	depth--; /* next level of parsing */
	result = PyList_New(0);
	if (!result) return NULL;
	do ++s; while (s != end && *s == ' ');
	/* everything is set up, start parsing the array */
	while (s != end)
	{
		if (*s == '}')
		{
			PyObject *subresult;

			if (!level) break; /* top level array ended */
			do ++s; while (s != end && *s == ' ');
			if (s == end) break; /* error */
			if (*s == delim)
			{
				do ++s; while (s != end && *s == ' ');
				if (s == end) break; /* error */
				if (*s != '{')
				{
					PyErr_SetString(PyExc_ValueError,
						"Subarray expected but not found");
					Py_DECREF(result); return NULL;
				}
			}
			else if (*s != '}') break; /* error */
			subresult = result;
			result = stack[--level];
			if (PyList_Append(result, subresult))
			{
				Py_DECREF(result); return NULL;
			}
		}
		else if (level == depth) /* we expect elements at this level */
		{
			PyObject   *element;
			char	   *estr;
			Py_ssize_t	esize;
			int escaped = 0;

			if (*s == '{')
			{
				PyErr_SetString(PyExc_ValueError,
					"Subarray found where not expected");
				Py_DECREF(result); return NULL;
			}
			if (*s == '"') /* quoted element */
			{
				estr = ++s;
				while (s != end && *s != '"')
				{
					if (*s == '\\')
					{
						++s; if (s == end) break;
						escaped = 1;
					}
					++s;
				}
				esize = s - estr;
				do ++s; while (s != end && *s == ' ');
			}
			else /* unquoted element */
			{
				estr = s;
				/* can contain blanks inside */
				while (s != end && *s != '"' &&
					*s != '{' && *s != '}' && *s != delim)
				{
					if (*s == '\\')
					{
						++s; if (s == end) break;
						escaped = 1;
					}
					++s;
				}
				t = s; while (t > estr && *(t - 1) == ' ') --t;
				if (!(esize = t - estr))
				{
					s = end; break; /* error */
				}
				if (STR_IS_NULL(estr, esize)) /* NULL gives None */
					estr = NULL;
			}
			if (s == end) break; /* error */
			if (estr)
			{
				if (escaped)
				{
					char	   *r;
					Py_ssize_t	i;

					/* create unescaped string */
					t = estr;
					estr = (char *) PyMem_Malloc(esize);
					if (!estr)
					{
						Py_DECREF(result); return PyErr_NoMemory();
					}
					for (i = 0, r = estr; i < esize; ++i)
					{
						if (*t == '\\') ++t, ++i;
						*r++ = *t++;
					}
					esize = r - estr;
				}
				if (type) /* internal casting of base type */
				{
					if (type & PYGRES_TEXT)
						element = cast_sized_text(estr, esize, encoding, type);
					else
						element = cast_sized_simple(estr, esize, type);
				}
				else /* external casting of base type */
				{
#if IS_PY3
					element = encoding == pg_encoding_ascii ? NULL :
						get_decoded_string(estr, esize, encoding);
					if (!element) /* no decoding necessary or possible */
#endif
					element = PyBytes_FromStringAndSize(estr, esize);
					if (element && cast)
					{
						PyObject *tmp = element;
						element = PyObject_CallFunctionObjArgs(
							cast, element, NULL);
						Py_DECREF(tmp);
					}
				}
				if (escaped) PyMem_Free(estr);
				if (!element)
				{
					Py_DECREF(result); return NULL;
				}
			}
			else
			{
				Py_INCREF(Py_None); element = Py_None;
			}
			if (PyList_Append(result, element))
			{
				Py_DECREF(element); Py_DECREF(result); return NULL;
			}
			Py_DECREF(element);
			if (*s == delim)
			{
				do ++s; while (s != end && *s == ' ');
				if (s == end) break; /* error */
			}
			else if (*s != '}') break; /* error */
		}
		else /* we expect arrays at this level */
		{
			if (*s != '{')
			{
				PyErr_SetString(PyExc_ValueError,
					"Subarray must start with a left brace");
				Py_DECREF(result); return NULL;
			}
			do ++s; while (s != end && *s == ' ');
			if (s == end) break; /* error */
			stack[level++] = result;
			if (!(result = PyList_New(0))) return NULL;
		}
	}
	if (s == end || *s != '}')
	{
		PyErr_SetString(PyExc_ValueError,
			"Unexpected end of array");
		Py_DECREF(result); return NULL;
	}
	do ++s; while (s != end && *s == ' ');
	if (s != end)
	{
		PyErr_SetString(PyExc_ValueError,
			"Unexpected characters after end of array");
		Py_DECREF(result); return NULL;
	}
	return result;
}

/* Cast string s with size and encoding to a Python tuple.
   using the input and output syntax for composite types.
   Use array of internal types or cast function or sequence of cast
   functions to cast elements. The parameter len is the record size.
   The parameter delim can specify a delimiter for the elements,
   although composite types always use a comma as delimiter. */

static PyObject *
cast_record(char *s, Py_ssize_t size, int encoding,
	 int *type, PyObject *cast, Py_ssize_t len, char delim)
{
	PyObject   *result, *ret;
	char	   *end = s + size, *t;
	Py_ssize_t	i;

	if (!delim)
		delim = ',';
	else if (delim == '(' || delim ==')' || delim=='\\')
	{
		PyErr_SetString(PyExc_ValueError, "Invalid record delimiter");
		return NULL;
	}

	/* strip blanks at the beginning */
	while (s != end && *s == ' ') ++s;
	if (s == end || *s != '(')
	{
		PyErr_SetString(PyExc_ValueError,
			"Record must start with a left parenthesis");
		return NULL;
	}
	result = PyList_New(0);
	if (!result) return NULL;
	i = 0;
	/* everything is set up, start parsing the record */
	while (++s != end)
	{
		PyObject   *element;

		if (*s == ')' || *s == delim)
		{
			Py_INCREF(Py_None); element = Py_None;
		}
		else
		{
			char	   *estr;
			Py_ssize_t	esize;
			int quoted = 0, escaped =0;

			estr = s;
			quoted = *s == '"';
			if (quoted) ++s;
			esize = 0;
			while (s != end)
			{
				if (!quoted && (*s == ')' || *s == delim))
					break;
				if (*s == '"')
				{
					++s; if (s == end) break;
					if (!(quoted && *s == '"'))
					{
						quoted = !quoted; continue;
					}
				}
				if (*s == '\\')
				{
					++s; if (s == end) break;
				}
				++s, ++esize;
			}
			if (s == end) break; /* error */
			if (estr + esize != s)
			{
				char	   *r;

				escaped = 1;
				/* create unescaped string */
				t = estr;
				estr = (char *) PyMem_Malloc(esize);
				if (!estr)
				{
					Py_DECREF(result); return PyErr_NoMemory();
				}
				quoted = 0;
				r = estr;
				while (t != s)
				{
					if (*t == '"')
					{
						++t;
						if (!(quoted && *t == '"'))
						{
							quoted = !quoted; continue;
						}
					}
					if (*t == '\\') ++t;
					*r++ = *t++;
				}
			}
			if (type) /* internal casting of element type */
			{
				int etype = type[i];

				if (etype & PYGRES_ARRAY)
					element = cast_array(
						estr, esize, encoding, etype, NULL, 0);
				else if (etype & PYGRES_TEXT)
					element = cast_sized_text(estr, esize, encoding, etype);
				else
					element = cast_sized_simple(estr, esize, etype);
			}
			else /* external casting of base type */
			{
#if IS_PY3
				element = encoding == pg_encoding_ascii ? NULL :
					get_decoded_string(estr, esize, encoding);
				if (!element) /* no decoding necessary or possible */
#endif
				element = PyBytes_FromStringAndSize(estr, esize);
				if (element && cast)
				{
					if (len)
					{
						PyObject *ecast = PySequence_GetItem(cast, i);

						if (ecast)
						{
							if (ecast != Py_None)
							{
								PyObject *tmp = element;
								element = PyObject_CallFunctionObjArgs(
									ecast, element, NULL);
								Py_DECREF(tmp);
							}
						}
						else
						{
							Py_DECREF(element); element = NULL;
						}
					}
					else
					{
						PyObject *tmp = element;
						element = PyObject_CallFunctionObjArgs(
							cast, element, NULL);
						Py_DECREF(tmp);
					}
				}
			}
			if (escaped) PyMem_Free(estr);
			if (!element)
			{
				Py_DECREF(result); return NULL;
			}
		}
		if (PyList_Append(result, element))
		{
			Py_DECREF(element); Py_DECREF(result); return NULL;
		}
		Py_DECREF(element);
		if (len) ++i;
		if (*s != delim) break; /* no next record */
		if (len && i >= len)
		{
			PyErr_SetString(PyExc_ValueError, "Too many columns");
			Py_DECREF(result); return NULL;
		}
	}
	if (s == end || *s != ')')
	{
		PyErr_SetString(PyExc_ValueError, "Unexpected end of record");
		Py_DECREF(result); return NULL;
	}
	do ++s; while (s != end && *s == ' ');
	if (s != end)
	{
		PyErr_SetString(PyExc_ValueError,
			"Unexpected characters after end of record");
		Py_DECREF(result); return NULL;
	}
	if (len && i < len)
	{
		PyErr_SetString(PyExc_ValueError, "Too few columns");
		Py_DECREF(result); return NULL;
	}

	ret = PyList_AsTuple(result);
	Py_DECREF(result);
	return ret;
}

/* Cast string s with size and encoding to a Python dictionary.
   using the input and output syntax for hstore values. */

static PyObject *
cast_hstore(char *s, Py_ssize_t size, int encoding)
{
	PyObject   *result;
	char	   *end = s + size;

    result = PyDict_New();

	/* everything is set up, start parsing the record */
	while (s != end)
	{
		char	   *key, *val;
		PyObject   *key_obj, *val_obj;
		Py_ssize_t	key_esc = 0, val_esc = 0, size;
		int			quoted;

		while (s != end && *s == ' ') ++s;
		if (s == end) break;
		quoted = *s == '"';
		if (quoted)
		{
			key = ++s;
			while (s != end)
			{
				if (*s == '"') break;
				if (*s == '\\')
				{
					if (++s == end) break;
					++key_esc;
				}
				++s;
			}
			if (s == end)
			{
				PyErr_SetString(PyExc_ValueError, "Unterminated quote");
				Py_DECREF(result); return NULL;
			}
		}
		else
		{
			key = s;
			while (s != end)
			{
				if (*s == '=' || *s == ' ') break;
				if (*s == '\\')
				{
					if (++s == end) break;
					++key_esc;
				}
				++s;
			}
			if (s == key)
			{
				PyErr_SetString(PyExc_ValueError, "Missing key");
				Py_DECREF(result); return NULL;
			}
		}
		size = s - key - key_esc;
		if (key_esc)
		{
			char *r = key, *t;
			key = (char *) PyMem_Malloc(size);
			if (!key)
			{
				Py_DECREF(result); return PyErr_NoMemory();
			}
			t = key;
			while (r != s)
			{
				if (*r == '\\')
				{
					++r; if (r == s) break;
				}
				*t++ = *r++;
			}
		}
		key_obj = cast_sized_text(key, size, encoding, PYGRES_TEXT);
		if (key_esc) PyMem_Free(key);
		if (!key_obj)
		{
			Py_DECREF(result); return NULL;
		}
		if (quoted) ++s;
		while (s != end && *s == ' ') ++s;
		if (s == end || *s++ != '=' || s == end || *s++ != '>')
		{
			PyErr_SetString(PyExc_ValueError, "Invalid characters after key");
			Py_DECREF(key_obj); Py_DECREF(result); return NULL;
		}
		while (s != end && *s == ' ') ++s;
		quoted = *s == '"';
		if (quoted)
		{
			val = ++s;
			while (s != end)
			{
				if (*s == '"') break;
				if (*s == '\\')
				{
					if (++s == end) break;
					++val_esc;
				}
				++s;
			}
			if (s == end)
			{
				PyErr_SetString(PyExc_ValueError, "Unterminated quote");
				Py_DECREF(result); return NULL;
			}
		}
		else
		{
			val = s;
			while (s != end)
			{
				if (*s == ',' || *s == ' ') break;
				if (*s == '\\')
				{
					if (++s == end) break;
					++val_esc;
				}
				++s;
			}
			if (s == val)
			{
				PyErr_SetString(PyExc_ValueError, "Missing value");
				Py_DECREF(key_obj); Py_DECREF(result); return NULL;
			}
			if (STR_IS_NULL(val, s - val))
				val = NULL;
		}
		if (val)
		{
			size = s - val - val_esc;
			if (val_esc)
			{
				char *r = val, *t;
				val = (char *) PyMem_Malloc(size);
				if (!val)
				{
					Py_DECREF(key_obj); Py_DECREF(result);
					return PyErr_NoMemory();
				}
				t = val;
				while (r != s)
				{
					if (*r == '\\')
					{
						++r; if (r == s) break;
					}
					*t++ = *r++;
				}
			}
			val_obj = cast_sized_text(val, size, encoding, PYGRES_TEXT);
			if (val_esc) PyMem_Free(val);
			if (!val_obj)
			{
				Py_DECREF(key_obj); Py_DECREF(result); return NULL;
			}
		}
		else
		{
			Py_INCREF(Py_None); val_obj = Py_None;
		}
		if (quoted) ++s;
		while (s != end && *s == ' ') ++s;
		if (s != end)
		{
			if (*s++ != ',')
			{
				PyErr_SetString(PyExc_ValueError,
					"Invalid characters after val");
				Py_DECREF(key_obj); Py_DECREF(val_obj);
				Py_DECREF(result); return NULL;
			}
			while (s != end && *s == ' ') ++s;
			if (s == end)
			{
				PyErr_SetString(PyExc_ValueError, "Missing entry");
				Py_DECREF(key_obj); Py_DECREF(val_obj);
				Py_DECREF(result); return NULL;
			}
		}
		PyDict_SetItem(result, key_obj, val_obj);
		Py_DECREF(key_obj); Py_DECREF(val_obj);
	}
	return result;
}

/* internal wrapper for the notice receiver callback */
static void
notice_receiver(void *arg, const PGresult *res)
{
	PyGILState_STATE gstate = PyGILState_Ensure();
	connObject *self = (connObject*) arg;
	PyObject *func = self->notice_receiver;

	if (func)
	{
		noticeObject *notice = PyObject_NEW(noticeObject, &noticeType);
		PyObject *ret;
		if (notice)
		{
			notice->pgcnx = arg;
			notice->res = res;
		}
		else
		{
			Py_INCREF(Py_None);
			notice = (noticeObject *)(void *)Py_None;
		}
		ret = PyObject_CallFunction(func, "(O)", notice);
		Py_XDECREF(ret);
	}
	PyGILState_Release(gstate);
}

/* gets appropriate error type from sqlstate */
static PyObject *
get_error_type(const char *sqlstate)
{
	switch (sqlstate[0]) {
		case '0':
			switch (sqlstate[1])
			{
				case 'A':
					return NotSupportedError;
			}
			break;
		case '2':
			switch (sqlstate[1])
			{
				case '0':
				case '1':
					return ProgrammingError;
				case '2':
					return DataError;
				case '3':
					return IntegrityError;
				case '4':
				case '5':
					return InternalError;
				case '6':
				case '7':
				case '8':
					return OperationalError;
				case 'B':
				case 'D':
				case 'F':
					return InternalError;
			}
			break;
		case '3':
			switch (sqlstate[1])
			{
				case '4':
					return OperationalError;
				case '8':
				case '9':
				case 'B':
					return InternalError;
				case 'D':
				case 'F':
					return ProgrammingError;
			}
			break;
		case '4':
			switch (sqlstate[1])
			{
				case '0':
					return OperationalError;
				case '2':
				case '4':
					return ProgrammingError;
			}
			break;
		case '5':
		case 'H':
			return OperationalError;
		case 'F':
		case 'P':
		case 'X':
			return InternalError;
	}
	return DatabaseError;
}

/* sets database error message and sqlstate attribute */
static void
set_error_msg_and_state(PyObject *type,
	const char *msg, int encoding, const char *sqlstate)
{
	PyObject   *err_obj, *msg_obj, *sql_obj = NULL;

#if IS_PY3
	if (encoding == -1) /* unknown */
	{
		msg_obj = PyUnicode_DecodeLocale(msg, NULL);
	}
	else
		msg_obj = get_decoded_string(msg, strlen(msg), encoding);
	if (!msg_obj) /* cannot decode */
#endif
	msg_obj = PyBytes_FromString(msg);

	if (sqlstate)
		sql_obj = PyStr_FromStringAndSize(sqlstate, 5);
	else
	{
		Py_INCREF(Py_None); sql_obj = Py_None;
	}

	err_obj = PyObject_CallFunctionObjArgs(type, msg_obj, NULL);
	if (err_obj)
	{
		Py_DECREF(msg_obj);
		PyObject_SetAttrString(err_obj, "sqlstate", sql_obj);
		Py_DECREF(sql_obj);
		PyErr_SetObject(type, err_obj);
		Py_DECREF(err_obj);
	}
	else
	{
		PyErr_SetString(type, msg);
	}
}

/* sets given database error message */
static void
set_error_msg(PyObject *type, const char *msg)
{
	set_error_msg_and_state(type, msg, pg_encoding_ascii, NULL);
}

/* sets database error from connection and/or result */
static void
set_error(PyObject *type, const char * msg, PGconn *cnx, PGresult *result)
{
	char *sqlstate = NULL; int encoding = pg_encoding_ascii;

	if (cnx)
	{
		char *err_msg = PQerrorMessage(cnx);
		if (err_msg)
		{
			msg = err_msg;
			encoding = PQclientEncoding(cnx);
		}
	}
	if (result)
	{
		sqlstate = PQresultErrorField(result, PG_DIAG_SQLSTATE);
		if (sqlstate) type = get_error_type(sqlstate);
	}

	set_error_msg_and_state(type, msg, encoding, sqlstate);
}

/* checks connection validity */
static int
check_cnx_obj(connObject *self)
{
	if (!self || !self->valid || !self->cnx)
	{
		set_error_msg(OperationalError, "Connection has been closed");
		return 0;
	}
	return 1;
}

/* format result (mostly useful for debugging) */
/* Note: This is similar to the Postgres function PQprint().
 * PQprint() is not used because handing over a stream from Python to
 * Postgres can be problematic if they use different libs for streams
 * and because using PQprint() and tp_print is not recommended any more.
 */
static PyObject *
format_result(const PGresult *res)
{
	const int n = PQnfields(res);

	if (n <= 0)
		return PyStr_FromString("(nothing selected)");

	char * const aligns = (char *) PyMem_Malloc(n * sizeof(char));
	int * const sizes = (int *) PyMem_Malloc(n * sizeof(int));

	if (!aligns || !sizes)
	{
		PyMem_Free(aligns); PyMem_Free(sizes); return PyErr_NoMemory();
	}

	const int m = PQntuples(res);
	int i, j;
	size_t size;
	char *buffer;

	/* calculate sizes and alignments */
	for (j = 0; j < n; ++j)
	{
		const char * const s = PQfname(res, j);
		const int format = PQfformat(res, j);

		sizes[j] = s ? (int)strlen(s) : 0;
		if (format)
		{
			aligns[j] = '\0';
			if (m && sizes[j] < 8)
				/* "<binary>" must fit */
				sizes[j] = 8;
		}
		else
		{
			const Oid ftype = PQftype(res, j);

			switch (ftype)
			{
				case INT2OID:
				case INT4OID:
				case INT8OID:
				case FLOAT4OID:
				case FLOAT8OID:
				case NUMERICOID:
				case OIDOID:
				case XIDOID:
				case CIDOID:
				case CASHOID:
					aligns[j] = 'r';
					break;
				default:
					aligns[j] = 'l';
			}
		}
	}
	for (i = 0; i < m; ++i)
	{
		for (j = 0; j < n; ++j)
		{
			if (aligns[j])
			{
				const int k = PQgetlength(res, i, j);

				if (sizes[j] < k)
					/* value must fit */
					sizes[j] = k;
			}
		}
	}
	size = 0;
	/* size of one row */
	for (j = 0; j < n; ++j) size += sizes[j] + 1;
	/* times number of rows incl. heading */
	size *= (m + 2);
	/* plus size of footer */
	size += 40;
	/* is the buffer size that needs to be allocated */
	buffer = (char *) PyMem_Malloc(size);
	if (!buffer)
	{
		PyMem_Free(aligns); PyMem_Free(sizes); return PyErr_NoMemory();
	}
	char *p = buffer;
	PyObject *result;

	/* create the header */
	for (j = 0; j < n; ++j)
	{
		const char * const s = PQfname(res, j);
		const int k = sizes[j];
		const int h = (k - (int)strlen(s)) / 2;

		sprintf(p, "%*s", h, "");
		sprintf(p + h, "%-*s", k - h, s);
		p += k;
		if (j + 1 < n)
			*p++ = '|';
	}
	*p++ = '\n';
	for (j = 0; j < n; ++j)
	{
		int k = sizes[j];

		while (k--)
			*p++ = '-';
		if (j + 1 < n)
			*p++ = '+';
	}
	*p++ = '\n';
	/* create the body */
	for (i = 0; i < m; ++i)
	{
		for (j = 0; j < n; ++j)
		{
			const char align = aligns[j];
			const int k = sizes[j];

			if (align)
			{
				sprintf(p, align == 'r' ?
					"%*s" : "%-*s", k,
					PQgetvalue(res, i, j));
			}
			else
			{
				sprintf(p, "%-*s", k,
					PQgetisnull(res, i, j) ?
					"" : "<binary>");
			}
			p += k;
			if (j + 1 < n)
				*p++ = '|';
		}
		*p++ = '\n';
	}
	/* free memory */
	PyMem_Free(aligns); PyMem_Free(sizes);
	/* create the footer */
	sprintf(p, "(%d row%s)", m, m == 1 ? "" : "s");
	/* return the result */
	result = PyStr_FromString(buffer);
	PyMem_Free(buffer);
	return result;
}

/* --------------------------------------------------------------------- */
/* large objects														 */
/* --------------------------------------------------------------------- */
#ifdef LARGE_OBJECTS

/* checks large object validity */
static int
check_lo_obj(largeObject *self, int level)
{
	if (!check_cnx_obj(self->pgcnx))
		return 0;

	if (!self->lo_oid)
	{
		set_error_msg(IntegrityError, "Object is not valid (null oid)");
		return 0;
	}

	if (level & CHECK_OPEN)
	{
		if (self->lo_fd < 0)
		{
			PyErr_SetString(PyExc_IOError, "Object is not opened");
			return 0;
		}
	}

	if (level & CHECK_CLOSE)
	{
		if (self->lo_fd >= 0)
		{
			PyErr_SetString(PyExc_IOError, "Object is already opened");
			return 0;
		}
	}

	return 1;
}

/* constructor (internal use only) */
static largeObject *
largeNew(connObject *pgcnx, Oid oid)
{
	largeObject *npglo;

	if (!(npglo = PyObject_NEW(largeObject, &largeType)))
		return NULL;

	Py_XINCREF(pgcnx);
	npglo->pgcnx = pgcnx;
	npglo->lo_fd = -1;
	npglo->lo_oid = oid;

	return npglo;
}

/* destructor */
static void
largeDealloc(largeObject *self)
{
	if (self->lo_fd >= 0 && self->pgcnx->valid)
		lo_close(self->pgcnx->cnx, self->lo_fd);

	Py_XDECREF(self->pgcnx);
	PyObject_Del(self);
}

/* opens large object */
static char largeOpen__doc__[] =
"open(mode) -- open access to large object with specified mode\n\n"
"The mode must be one of INV_READ, INV_WRITE (module level constants).\n";

static PyObject *
largeOpen(largeObject *self, PyObject *args)
{
	int			mode,
				fd;

	/* gets arguments */
	if (!PyArg_ParseTuple(args, "i", &mode))
	{
		PyErr_SetString(PyExc_TypeError,
			"The open() method takes an integer argument");
		return NULL;
	}

	/* check validity */
	if (!check_lo_obj(self, CHECK_CLOSE))
		return NULL;

	/* opens large object */
	if ((fd = lo_open(self->pgcnx->cnx, self->lo_oid, mode)) < 0)
	{
		PyErr_SetString(PyExc_IOError, "Can't open large object");
		return NULL;
	}
	self->lo_fd = fd;

	/* no error : returns Py_None */
	Py_INCREF(Py_None);
	return Py_None;
}

/* close large object */
static char largeClose__doc__[] =
"close() -- close access to large object data";

static PyObject *
largeClose(largeObject *self, PyObject *noargs)
{
	/* checks validity */
	if (!check_lo_obj(self, CHECK_OPEN))
		return NULL;

	/* closes large object */
	if (lo_close(self->pgcnx->cnx, self->lo_fd))
	{
		PyErr_SetString(PyExc_IOError, "Error while closing large object fd");
		return NULL;
	}
	self->lo_fd = -1;

	/* no error : returns Py_None */
	Py_INCREF(Py_None);
	return Py_None;
}

/* reads from large object */
static char largeRead__doc__[] =
"read(size) -- read from large object to sized string\n\n"
"Object must be opened in read mode before calling this method.\n";

static PyObject *
largeRead(largeObject *self, PyObject *args)
{
	int			size;
	PyObject   *buffer;

	/* gets arguments */
	if (!PyArg_ParseTuple(args, "i", &size))
	{
		PyErr_SetString(PyExc_TypeError,
			"Method read() takes an integer argument");
		return NULL;
	}

	if (size <= 0)
	{
		PyErr_SetString(PyExc_ValueError,
			"Method read() takes a positive integer as argument");
		return NULL;
	}

	/* checks validity */
	if (!check_lo_obj(self, CHECK_OPEN))
		return NULL;

	/* allocate buffer and runs read */
	buffer = PyBytes_FromStringAndSize((char *) NULL, size);

	if ((size = lo_read(self->pgcnx->cnx, self->lo_fd,
		PyBytes_AS_STRING((PyBytesObject *)(buffer)), size)) < 0)
	{
		PyErr_SetString(PyExc_IOError, "Error while reading");
		Py_XDECREF(buffer);
		return NULL;
	}

	/* resize buffer and returns it */
	_PyBytes_Resize(&buffer, size);
	return buffer;
}

/* write to large object */
static char largeWrite__doc__[] =
"write(string) -- write sized string to large object\n\n"
"Object must be opened in read mode before calling this method.\n";

static PyObject *
largeWrite(largeObject *self, PyObject *args)
{
	char	   *buffer;
	int			size,
				bufsize;

	/* gets arguments */
	if (!PyArg_ParseTuple(args, "s#", &buffer, &bufsize))
	{
		PyErr_SetString(PyExc_TypeError,
			"Method write() expects a sized string as argument");
		return NULL;
	}

	/* checks validity */
	if (!check_lo_obj(self, CHECK_OPEN))
		return NULL;

	/* sends query */
	if ((size = lo_write(self->pgcnx->cnx, self->lo_fd, buffer,
						 bufsize)) < bufsize)
	{
		PyErr_SetString(PyExc_IOError, "Buffer truncated during write");
		return NULL;
	}

	/* no error : returns Py_None */
	Py_INCREF(Py_None);
	return Py_None;
}

/* go to position in large object */
static char largeSeek__doc__[] =
"seek(offset, whence) -- move to specified position\n\n"
"Object must be opened before calling this method. The whence option\n"
"can be SEEK_SET, SEEK_CUR or SEEK_END (module level constants).\n";

static PyObject *
largeSeek(largeObject *self, PyObject *args)
{
	/* offset and whence are initialized to keep compiler happy */
	int			ret,
				offset = 0,
				whence = 0;

	/* gets arguments */
	if (!PyArg_ParseTuple(args, "ii", &offset, &whence))
	{
		PyErr_SetString(PyExc_TypeError,
			"Method lseek() expects two integer arguments");
		return NULL;
	}

	/* checks validity */
	if (!check_lo_obj(self, CHECK_OPEN))
		return NULL;

	/* sends query */
	if ((ret = lo_lseek(self->pgcnx->cnx, self->lo_fd, offset, whence)) == -1)
	{
		PyErr_SetString(PyExc_IOError, "Error while moving cursor");
		return NULL;
	}

	/* returns position */
	return PyInt_FromLong(ret);
}

/* gets large object size */
static char largeSize__doc__[] =
"size() -- return large object size\n\n"
"The object must be opened before calling this method.\n";

static PyObject *
largeSize(largeObject *self, PyObject *noargs)
{
	int			start,
				end;

	/* checks validity */
	if (!check_lo_obj(self, CHECK_OPEN))
		return NULL;

	/* gets current position */
	if ((start = lo_tell(self->pgcnx->cnx, self->lo_fd)) == -1)
	{
		PyErr_SetString(PyExc_IOError, "Error while getting current position");
		return NULL;
	}

	/* gets end position */
	if ((end = lo_lseek(self->pgcnx->cnx, self->lo_fd, 0, SEEK_END)) == -1)
	{
		PyErr_SetString(PyExc_IOError, "Error while getting end position");
		return NULL;
	}

	/* move back to start position */
	if ((start = lo_lseek(
		self->pgcnx->cnx, self->lo_fd, start, SEEK_SET)) == -1)
	{
		PyErr_SetString(PyExc_IOError,
			"Error while moving back to first position");
		return NULL;
	}

	/* returns size */
	return PyInt_FromLong(end);
}

/* gets large object cursor position */
static char largeTell__doc__[] =
"tell() -- give current position in large object\n\n"
"The object must be opened before calling this method.\n";

static PyObject *
largeTell(largeObject *self, PyObject *noargs)
{
	int			start;

	/* checks validity */
	if (!check_lo_obj(self, CHECK_OPEN))
		return NULL;

	/* gets current position */
	if ((start = lo_tell(self->pgcnx->cnx, self->lo_fd)) == -1)
	{
		PyErr_SetString(PyExc_IOError, "Error while getting position");
		return NULL;
	}

	/* returns size */
	return PyInt_FromLong(start);
}

/* exports large object as unix file */
static char largeExport__doc__[] =
"export(filename) -- export large object data to specified file\n\n"
"The object must be closed when calling this method.\n";

static PyObject *
largeExport(largeObject *self, PyObject *args)
{
	char *name;

	/* checks validity */
	if (!check_lo_obj(self, CHECK_CLOSE))
		return NULL;

	/* gets arguments */
	if (!PyArg_ParseTuple(args, "s", &name))
	{
		PyErr_SetString(PyExc_TypeError,
			"The method export() takes a filename as argument");
		return NULL;
	}

	/* runs command */
	if (!lo_export(self->pgcnx->cnx, self->lo_oid, name))
	{
		PyErr_SetString(PyExc_IOError, "Error while exporting large object");
		return NULL;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/* deletes a large object */
static char largeUnlink__doc__[] =
"unlink() -- destroy large object\n\n"
"The object must be closed when calling this method.\n";

static PyObject *
largeUnlink(largeObject *self, PyObject *noargs)
{
	/* checks validity */
	if (!check_lo_obj(self, CHECK_CLOSE))
		return NULL;

	/* deletes the object, invalidate it on success */
	if (!lo_unlink(self->pgcnx->cnx, self->lo_oid))
	{
		PyErr_SetString(PyExc_IOError, "Error while unlinking large object");
		return NULL;
	}
	self->lo_oid = 0;

	Py_INCREF(Py_None);
	return Py_None;
}

/* get the list of large object attributes */
static PyObject *
largeDir(largeObject *self, PyObject *noargs)
{
	PyObject *attrs;

	attrs = PyObject_Dir(PyObject_Type((PyObject *)self));
	PyObject_CallMethod(attrs, "extend", "[sss]",
		"oid", "pgcnx", "error");

	return attrs;
}

/* large object methods */
static struct PyMethodDef largeMethods[] = {
	{"__dir__", (PyCFunction) largeDir,  METH_NOARGS, NULL},
	{"open", (PyCFunction) largeOpen, METH_VARARGS, largeOpen__doc__},
	{"close", (PyCFunction) largeClose, METH_NOARGS, largeClose__doc__},
	{"read", (PyCFunction) largeRead, METH_VARARGS, largeRead__doc__},
	{"write", (PyCFunction) largeWrite, METH_VARARGS, largeWrite__doc__},
	{"seek", (PyCFunction) largeSeek, METH_VARARGS, largeSeek__doc__},
	{"size", (PyCFunction) largeSize, METH_NOARGS, largeSize__doc__},
	{"tell", (PyCFunction) largeTell, METH_NOARGS, largeTell__doc__},
	{"export",(PyCFunction) largeExport, METH_VARARGS, largeExport__doc__},
	{"unlink",(PyCFunction) largeUnlink, METH_NOARGS, largeUnlink__doc__},
	{NULL, NULL}
};

/* gets large object attributes */
static PyObject *
largeGetAttr(largeObject *self, PyObject *nameobj)
{
	const char *name = PyStr_AsString(nameobj);

	/* list postgreSQL large object fields */

	/* associated pg connection object */
	if (!strcmp(name, "pgcnx"))
	{
		if (check_lo_obj(self, 0))
		{
			Py_INCREF(self->pgcnx);
			return (PyObject *) (self->pgcnx);
		}
		PyErr_Clear();
		Py_INCREF(Py_None);
		return Py_None;
	}

	/* large object oid */
	if (!strcmp(name, "oid"))
	{
		if (check_lo_obj(self, 0))
			return PyInt_FromLong(self->lo_oid);
		PyErr_Clear();
		Py_INCREF(Py_None);
		return Py_None;
	}

	/* error (status) message */
	if (!strcmp(name, "error"))
		return PyStr_FromString(PQerrorMessage(self->pgcnx->cnx));

	/* seeks name in methods (fallback) */
	return PyObject_GenericGetAttr((PyObject *) self, nameobj);
}

/* return large object as string in human readable form */
static PyObject *
largeStr(largeObject *self)
{
	char		str[80];
	sprintf(str, self->lo_fd >= 0 ?
			"Opened large object, oid %ld" :
			"Closed large object, oid %ld", (long) self->lo_oid);
	return PyStr_FromString(str);
}

static char large__doc__[] = "PostgreSQL large object";

/* large object type definition */
static PyTypeObject largeType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"pg.LargeObject",				/* tp_name */
	sizeof(largeObject),			/* tp_basicsize */
	0,								/* tp_itemsize */

	/* methods */
	(destructor) largeDealloc,		/* tp_dealloc */
	0,								/* tp_print */
	0,								/* tp_getattr */
	0,								/* tp_setattr */
	0,								/* tp_compare */
	0,								/* tp_repr */
	0,								/* tp_as_number */
	0,								/* tp_as_sequence */
	0,								/* tp_as_mapping */
	0,								/* tp_hash */
	0,								/* tp_call */
	(reprfunc) largeStr,			/* tp_str */
	(getattrofunc) largeGetAttr,	/* tp_getattro */
	0,								/* tp_setattro */
	0,								/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,				/* tp_flags */
	large__doc__,					/* tp_doc */
	0,								/* tp_traverse */
	0,								/* tp_clear */
	0,								/* tp_richcompare */
	0,								/* tp_weaklistoffset */
	0,								/* tp_iter */
	0,								/* tp_iternext */
	largeMethods,					/* tp_methods */
};
#endif /* LARGE_OBJECTS */

/* --------------------------------------------------------------------- */
/* connection object													 */
/* --------------------------------------------------------------------- */
static void
connDelete(connObject *self)
{
	if (self->cnx)
	{
		Py_BEGIN_ALLOW_THREADS
		PQfinish(self->cnx);
		Py_END_ALLOW_THREADS
	}
	Py_XDECREF(self->cast_hook);
	Py_XDECREF(self->notice_receiver);
	PyObject_Del(self);
}

/* source creation */
static char connSource__doc__[] =
"source() -- create a new source object for this connection";

static PyObject *
connSource(connObject *self, PyObject *noargs)
{
	sourceObject *npgobj;

	/* checks validity */
	if (!check_cnx_obj(self))
		return NULL;

	/* allocates new query object */
	if (!(npgobj = PyObject_NEW(sourceObject, &sourceType)))
		return NULL;

	/* initializes internal parameters */
	Py_XINCREF(self);
	npgobj->pgcnx = self;
	npgobj->result = NULL;
	npgobj->valid = 1;
	npgobj->arraysize = PG_ARRAYSIZE;

	return (PyObject *) npgobj;
}

/* database query */
static char connQuery__doc__[] =
"query(sql, [arg]) -- create a new query object for this connection\n\n"
"You must pass the SQL (string) request and you can optionally pass\n"
"a tuple with positional parameters.\n";

static PyObject *
connQuery(connObject *self, PyObject *args)
{
	PyObject	*query_obj;
	PyObject	*param_obj = NULL;
	char		*query;
	PGresult	*result;
	queryObject *npgobj;
	int			encoding,
				status,
				nparms = 0;

	if (!self->cnx)
	{
		PyErr_SetString(PyExc_TypeError, "Connection is not valid");
		return NULL;
	}

	/* get query args */
	if (!PyArg_ParseTuple(args, "O|O", &query_obj, &param_obj))
	{
		return NULL;
	}

	encoding = PQclientEncoding(self->cnx);

	if (PyBytes_Check(query_obj))
	{
		query = PyBytes_AsString(query_obj);
		query_obj = NULL;
	}
	else if (PyUnicode_Check(query_obj))
	{
		query_obj = get_encoded_string(query_obj, encoding);
		if (!query_obj) return NULL; /* pass the UnicodeEncodeError */
		query = PyBytes_AsString(query_obj);
	}
	else
	{
		PyErr_SetString(PyExc_TypeError,
			"Method query() expects a string as first argument");
		return NULL;
	}

	/* If param_obj is passed, ensure it's a non-empty tuple. We want to treat
	 * an empty tuple the same as no argument since we'll get that when the
	 * caller passes no arguments to db.query(), and historic behaviour was
	 * to call PQexec() in that case, which can execute multiple commands. */
	if (param_obj)
	{
		param_obj = PySequence_Fast(param_obj,
			"Method query() expects a sequence as second argument");
		if (!param_obj)
		{
			Py_XDECREF(query_obj);
			return NULL;
		}
		nparms = (int)PySequence_Fast_GET_SIZE(param_obj);

		/* if there's a single argument and it's a list or tuple, it
		 * contains the positional arguments. */
		if (nparms == 1)
		{
			PyObject *first_obj = PySequence_Fast_GET_ITEM(param_obj, 0);
			if (PyList_Check(first_obj) || PyTuple_Check(first_obj))
			{
				Py_DECREF(param_obj);
				param_obj = PySequence_Fast(first_obj, NULL);
				nparms = (int)PySequence_Fast_GET_SIZE(param_obj);
			}
		}
	}

	/* gets result */
	if (nparms)
	{
		/* prepare arguments */
		PyObject	**str, **s;
		char		**parms, **p;
		register int i;

		str = (PyObject **)PyMem_Malloc(nparms * sizeof(*str));
		parms = (char **)PyMem_Malloc(nparms * sizeof(*parms));
		if (!str || !parms)
		{
			PyMem_Free(parms); PyMem_Free(str);
			Py_XDECREF(query_obj); Py_XDECREF(param_obj);
			return PyErr_NoMemory();
		}

		/* convert optional args to a list of strings -- this allows
		 * the caller to pass whatever they like, and prevents us
		 * from having to map types to OIDs */
		for (i = 0, s=str, p=parms; i < nparms; ++i, ++p)
		{
			PyObject *obj = PySequence_Fast_GET_ITEM(param_obj, i);

			if (obj == Py_None)
			{
				*p = NULL;
			}
			else if (PyBytes_Check(obj))
			{
				*p = PyBytes_AsString(obj);
			}
			else if (PyUnicode_Check(obj))
			{
				PyObject *str_obj = get_encoded_string(obj, encoding);
				if (!str_obj)
				{
					PyMem_Free(parms);
					while (s != str) { s--; Py_DECREF(*s); }
					PyMem_Free(str);
					Py_XDECREF(query_obj);
					Py_XDECREF(param_obj);
					/* pass the UnicodeEncodeError */
					return NULL;
				}
				*s++ = str_obj;
				*p = PyBytes_AsString(str_obj);
			}
			else
			{
				PyObject *str_obj = PyObject_Str(obj);
				if (!str_obj)
				{
					PyMem_Free(parms);
					while (s != str) { s--; Py_DECREF(*s); }
					PyMem_Free(str);
					Py_XDECREF(query_obj);
					Py_XDECREF(param_obj);
					PyErr_SetString(PyExc_TypeError,
						"Query parameter has no string representation");
					return NULL;
				}
				*s++ = str_obj;
				*p = PyStr_AsString(str_obj);
			}
		}

		Py_BEGIN_ALLOW_THREADS
		result = PQexecParams(self->cnx, query, nparms,
			NULL, (const char * const *)parms, NULL, NULL, 0);
		Py_END_ALLOW_THREADS

		PyMem_Free(parms);
		while (s != str) { s--; Py_DECREF(*s); }
		PyMem_Free(str);
	}
	else
	{
		Py_BEGIN_ALLOW_THREADS
		result = PQexec(self->cnx, query);
		Py_END_ALLOW_THREADS
	}

	/* we don't need the query and its params any more */
	Py_XDECREF(query_obj);
	Py_XDECREF(param_obj);

	/* checks result validity */
	if (!result)
	{
		PyErr_SetString(PyExc_ValueError, PQerrorMessage(self->cnx));
		return NULL;
	}

	/* this may have changed the datestyle, so we reset the date format
	   in order to force fetching it newly when next time requested */
	self->date_format = date_format; /* this is normally NULL */

	/* checks result status */
	if ((status = PQresultStatus(result)) != PGRES_TUPLES_OK)
	{
		switch (status)
		{
			case PGRES_EMPTY_QUERY:
				PyErr_SetString(PyExc_ValueError, "Empty query");
				break;
			case PGRES_BAD_RESPONSE:
			case PGRES_FATAL_ERROR:
			case PGRES_NONFATAL_ERROR:
				set_error(ProgrammingError, "Cannot execute query",
					self->cnx, result);
				break;
			case PGRES_COMMAND_OK:
				{						/* INSERT, UPDATE, DELETE */
					Oid		oid = PQoidValue(result);
					if (oid == InvalidOid)	/* not a single insert */
					{
						char	*ret = PQcmdTuples(result);

						PQclear(result);
						if (ret[0])		/* return number of rows affected */
						{
							return PyStr_FromString(ret);
						}
						Py_INCREF(Py_None);
						return Py_None;
					}
					/* for a single insert, return the oid */
					PQclear(result);
					return PyInt_FromLong(oid);
				}
			case PGRES_COPY_OUT:		/* no data will be received */
			case PGRES_COPY_IN:
				PQclear(result);
				Py_INCREF(Py_None);
				return Py_None;
			default:
				set_error_msg(InternalError, "Unknown result status");
		}

		PQclear(result);
		return NULL;			/* error detected on query */
	}

	if (!(npgobj = PyObject_NEW(queryObject, &queryType)))
		return PyErr_NoMemory();

	/* stores result and returns object */
	Py_XINCREF(self);
	npgobj->pgcnx = self;
	npgobj->result = result;
	npgobj->encoding = encoding;
	return (PyObject *) npgobj;
}

#ifdef DIRECT_ACCESS
static char connPutLine__doc__[] =
"putline(line) -- send a line directly to the backend";

/* direct access function: putline */
static PyObject *
connPutLine(connObject *self, PyObject *args)
{
	char *line;
	int line_length;

	if (!self->cnx)
	{
		PyErr_SetString(PyExc_TypeError, "Connection is not valid");
		return NULL;
	}

	/* reads args */
	if (!PyArg_ParseTuple(args, "s#", &line, &line_length))
	{
		PyErr_SetString(PyExc_TypeError,
			"Method putline() takes a string argument");
		return NULL;
	}

	/* sends line to backend */
	if (PQputline(self->cnx, line))
	{
		PyErr_SetString(PyExc_IOError, PQerrorMessage(self->cnx));
		return NULL;
	}
	Py_INCREF(Py_None);
	return Py_None;
}

/* direct access function: getline */
static char connGetLine__doc__[] =
"getline() -- get a line directly from the backend";

static PyObject *
connGetLine(connObject *self, PyObject *noargs)
{
	char		line[MAX_BUFFER_SIZE];
	PyObject   *str = NULL;		/* GCC */

	if (!self->cnx)
	{
		PyErr_SetString(PyExc_TypeError, "Connection is not valid");
		return NULL;
	}

	/* gets line */
	switch (PQgetline(self->cnx, line, MAX_BUFFER_SIZE))
	{
		case 0:
			str = PyStr_FromString(line);
			break;
		case 1:
			PyErr_SetString(PyExc_MemoryError, "Buffer overflow");
			str = NULL;
			break;
		case EOF:
			Py_INCREF(Py_None);
			str = Py_None;
			break;
	}

	return str;
}

/* direct access function: end copy */
static char connEndCopy__doc__[] =
"endcopy() -- synchronize client and server";

static PyObject *
connEndCopy(connObject *self, PyObject *noargs)
{
	if (!self->cnx)
	{
		PyErr_SetString(PyExc_TypeError, "Connection is not valid");
		return NULL;
	}

	/* ends direct copy */
	if (PQendcopy(self->cnx))
	{
		PyErr_SetString(PyExc_IOError, PQerrorMessage(self->cnx));
		return NULL;
	}
	Py_INCREF(Py_None);
	return Py_None;
}
#endif /* DIRECT_ACCESS */

/* return query as string in human readable form */
static PyObject *
queryStr(queryObject *self)
{
	return format_result(self->result);
}

/* insert table */
static char connInsertTable__doc__[] =
"inserttable(table, data) -- insert list into table\n\n"
"The fields in the list must be in the same order as in the table.\n";

static PyObject *
connInsertTable(connObject *self, PyObject *args)
{
	PGresult	*result;
	char		*table,
				*buffer,
				*bufpt;
	int			encoding;
	size_t		bufsiz;
	PyObject	*list,
				*sublist,
				*item;
	PyObject	*(*getitem) (PyObject *, Py_ssize_t);
	PyObject	*(*getsubitem) (PyObject *, Py_ssize_t);
	Py_ssize_t	i,
				j,
				m,
				n;

	if (!self->cnx)
	{
		PyErr_SetString(PyExc_TypeError, "Connection is not valid");
		return NULL;
	}

	/* gets arguments */
	if (!PyArg_ParseTuple(args, "sO:filter", &table, &list))
	{
		PyErr_SetString(PyExc_TypeError,
			"Method inserttable() expects a string and a list as arguments");
		return NULL;
	}

	/* checks list type */
	if (PyTuple_Check(list))
	{
		m = PyTuple_Size(list);
		getitem = PyTuple_GetItem;
	}
	else if (PyList_Check(list))
	{
		m = PyList_Size(list);
		getitem = PyList_GetItem;
	}
	else
	{
		PyErr_SetString(PyExc_TypeError,
			"Method inserttable() expects some kind of array"
			" as second argument");
		return NULL;
	}

	/* allocate buffer */
	if (!(buffer = PyMem_Malloc(MAX_BUFFER_SIZE)))
		return PyErr_NoMemory();

	/* starts query */
	sprintf(buffer, "copy %s from stdin", table);

	Py_BEGIN_ALLOW_THREADS
	result = PQexec(self->cnx, buffer);
	Py_END_ALLOW_THREADS

	if (!result)
	{
		PyMem_Free(buffer);
		PyErr_SetString(PyExc_ValueError, PQerrorMessage(self->cnx));
		return NULL;
	}

	encoding = PQclientEncoding(self->cnx);

	PQclear(result);

	n = 0; /* not strictly necessary but avoids warning */

	/* feed table */
	for (i = 0; i < m; ++i)
	{
		sublist = getitem(list, i);
		if (PyTuple_Check(sublist))
		{
			j = PyTuple_Size(sublist);
			getsubitem = PyTuple_GetItem;
		}
		else if (PyList_Check(sublist))
		{
			j = PyList_Size(sublist);
			getsubitem = PyList_GetItem;
		}
		else
		{
			PyErr_SetString(PyExc_TypeError,
				"Second arg must contain some kind of arrays");
			return NULL;
		}
		if (i)
		{
			if (j != n)
			{
				PyMem_Free(buffer);
				PyErr_SetString(PyExc_TypeError,
					"Arrays contained in second arg must have same size");
				return NULL;
			}
		}
		else
		{
			n = j; /* never used before this assignment */
		}

		/* builds insert line */
		bufpt = buffer;
		bufsiz = MAX_BUFFER_SIZE - 1;

		for (j = 0; j < n; ++j)
		{
			if (j)
			{
				*bufpt++ = '\t'; --bufsiz;
			}

			item = getsubitem(sublist, j);

			/* convert item to string and append to buffer */
			if (item == Py_None)
			{
				if (bufsiz > 2)
				{
					*bufpt++ = '\\'; *bufpt++ = 'N';
					bufsiz -= 2;
				}
				else
					bufsiz = 0;
			}
			else if (PyBytes_Check(item))
			{
				const char* t = PyBytes_AsString(item);
				while (*t && bufsiz)
				{
					if (*t == '\\' || *t == '\t' || *t == '\n')
					{
						*bufpt++ = '\\'; --bufsiz;
						if (!bufsiz) break;
					}
					*bufpt++ = *t++; --bufsiz;
				}
			}
			else if (PyUnicode_Check(item))
			{
				PyObject *s = get_encoded_string(item, encoding);
				if (!s)
				{
					PyMem_Free(buffer);
					return NULL; /* pass the UnicodeEncodeError */
				}
				const char* t = PyBytes_AsString(s);
				while (*t && bufsiz)
				{
					if (*t == '\\' || *t == '\t' || *t == '\n')
					{
						*bufpt++ = '\\'; --bufsiz;
						if (!bufsiz) break;
					}
					*bufpt++ = *t++; --bufsiz;
				}
				Py_DECREF(s);
			}
			else if (PyInt_Check(item) || PyLong_Check(item))
			{
				PyObject* s = PyObject_Str(item);
				const char* t = PyStr_AsString(s);
				while (*t && bufsiz)
				{
					*bufpt++ = *t++; --bufsiz;
				}
				Py_DECREF(s);
			}
			else
			{
				PyObject* s = PyObject_Repr(item);
				const char* t = PyStr_AsString(s);
				while (*t && bufsiz)
				{
					if (*t == '\\' || *t == '\t' || *t == '\n')
					{
						*bufpt++ = '\\'; --bufsiz;
						if (!bufsiz) break;
					}
					*bufpt++ = *t++; --bufsiz;
				}
				Py_DECREF(s);
			}

			if (bufsiz <= 0)
			{
				PyMem_Free(buffer); return PyErr_NoMemory();
			}

		}

		*bufpt++ = '\n'; *bufpt = '\0';

		/* sends data */
		if (PQputline(self->cnx, buffer))
		{
			PyErr_SetString(PyExc_IOError, PQerrorMessage(self->cnx));
			PQendcopy(self->cnx);
			PyMem_Free(buffer);
			return NULL;
		}
	}

	/* ends query */
	if (PQputline(self->cnx, "\\.\n"))
	{
		PyErr_SetString(PyExc_IOError, PQerrorMessage(self->cnx));
		PQendcopy(self->cnx);
		PyMem_Free(buffer);
		return NULL;
	}

	if (PQendcopy(self->cnx))
	{
		PyErr_SetString(PyExc_IOError, PQerrorMessage(self->cnx));
		PyMem_Free(buffer);
		return NULL;
	}

	PyMem_Free(buffer);

	/* no error : returns nothing */
	Py_INCREF(Py_None);
	return Py_None;
}

/* get transaction state */
static char connTransaction__doc__[] =
"transaction() -- return the current transaction status";

static PyObject *
connTransaction(connObject *self, PyObject *noargs)
{
	if (!self->cnx)
	{
		PyErr_SetString(PyExc_TypeError, "Connection is not valid");
		return NULL;
	}

	return PyInt_FromLong(PQtransactionStatus(self->cnx));
}

/* get parameter setting */
static char connParameter__doc__[] =
"parameter(name) -- look up a current parameter setting";

static PyObject *
connParameter(connObject *self, PyObject *args)
{
	const char *name;

	if (!self->cnx)
	{
		PyErr_SetString(PyExc_TypeError, "Connection is not valid");
		return NULL;
	}

	/* get query args */
	if (!PyArg_ParseTuple(args, "s", &name))
	{
		PyErr_SetString(PyExc_TypeError,
			"Method parameter() takes a string as argument");
		return NULL;
	}

	name = PQparameterStatus(self->cnx, name);

	if (name)
		return PyStr_FromString(name);

	/* unknown parameter, return None */
	Py_INCREF(Py_None);
	return Py_None;
}

/* internal function converting a Postgres datestyles to date formats */
static const char *
date_style_to_format(const char *s)
{
	static const char *formats[] = {
		"%Y-%m-%d",		/* 0 = ISO */
		"%m-%d-%Y",		/* 1 = Postgres, MDY */
		"%d-%m-%Y",		/* 2 = Postgres, DMY */
		"%m/%d/%Y",		/* 3 = SQL, MDY */
		"%d/%m/%Y",		/* 4 = SQL, DMY */
		"%d.%m.%Y"}; 	/* 5 = German */

	switch (s ? *s : 'I')
	{
		case 'P': /* Postgres */
			s = strchr(s + 1, ',');
			if (s) do ++s; while (*s && *s == ' ');
			return formats[s && *s == 'D' ? 2 : 1];
		case 'S': /* SQL */
			s = strchr(s + 1, ',');
			if (s) do ++s; while (*s && *s == ' ');
			return formats[s && *s == 'D' ? 4 : 3];
		case 'G': /* German */
			return formats[5];
		default: /* ISO */
			return formats[0]; /* ISO is the default */
	}
}

/* internal function converting a date format to a Postgres datestyle */
static const char *
date_format_to_style(const char *s)
{
	static const char *datestyle[] = {
		"ISO, YMD",			/* 0 = %Y-%m-%d */
		"Postgres, MDY", 	/* 1 = %m-%d-%Y */
		"Postgres, DMY", 	/* 2 = %d-%m-%Y */
		"SQL, MDY", 		/* 3 = %m/%d/%Y */
		"SQL, DMY", 		/* 4 = %d/%m/%Y */
		"German, DMY"};		/* 5 = %d.%m.%Y */

	switch (s ? s[1] : 'Y')
	{
		case 'm':
			switch (s[2])
			{
				case '/':
					return datestyle[3]; /* SQL, MDY */
				default:
					return datestyle[1]; /* Postgres, MDY */
			}
		case 'd':
			switch (s[2])
			{
				case '/':
					return datestyle[4]; /* SQL, DMY */
				case '.':
					return datestyle[5]; /* German */
				default:
					return datestyle[2]; /* Postgres, DMY */
			}
		default:
			return datestyle[0]; /* ISO */
	}
}

/* get current date format */
static char connDateFormat__doc__[] =
"date_format() -- return the current date format";

static PyObject *
connDateFormat(connObject *self, PyObject *noargs)
{
	const char *fmt;

	if (!self->cnx)
	{
		PyErr_SetString(PyExc_TypeError, "Connection is not valid");
		return NULL;
	}

	/* check if the date format is cached in the connection */
	fmt = self->date_format;
	if (!fmt)
	{
		fmt = date_style_to_format(PQparameterStatus(self->cnx, "DateStyle"));
		self->date_format = fmt; /* cache the result */
	}

	return PyStr_FromString(fmt);
}

#ifdef ESCAPING_FUNCS

/* escape literal */
static char connEscapeLiteral__doc__[] =
"escape_literal(str) -- escape a literal constant for use within SQL";

static PyObject *
connEscapeLiteral(connObject *self, PyObject *string)
{
	PyObject   *tmp_obj = NULL, /* auxiliary string object */
			   *to_obj; /* string object to return */
	char 	   *from, /* our string argument as encoded string */
			   *to; /* the result as encoded string */
	Py_ssize_t 	from_length; /* length of string */
	size_t		to_length; /* length of result */
	int			encoding = -1; /* client encoding */

	if (PyBytes_Check(string))
	{
		PyBytes_AsStringAndSize(string, &from, &from_length);
	}
	else if (PyUnicode_Check(string))
	{
		encoding = PQclientEncoding(self->cnx);
		tmp_obj = get_encoded_string(string, encoding);
		if (!tmp_obj) return NULL; /* pass the UnicodeEncodeError */
		PyBytes_AsStringAndSize(tmp_obj, &from, &from_length);
	}
	else
	{
		PyErr_SetString(PyExc_TypeError,
			"Method escape_literal() expects a string as argument");
		return NULL;
	}

	to = PQescapeLiteral(self->cnx, from, (size_t)from_length);
	to_length = strlen(to);

	Py_XDECREF(tmp_obj);

	if (encoding == -1)
		to_obj = PyBytes_FromStringAndSize(to, to_length);
	else
		to_obj = get_decoded_string(to, to_length, encoding);
	if (to)
		PQfreemem(to);
	return to_obj;
}

/* escape identifier */
static char connEscapeIdentifier__doc__[] =
"escape_identifier(str) -- escape an identifier for use within SQL";

static PyObject *
connEscapeIdentifier(connObject *self, PyObject *string)
{
	PyObject   *tmp_obj = NULL, /* auxiliary string object */
			   *to_obj; /* string object to return */
	char 	   *from, /* our string argument as encoded string */
			   *to; /* the result as encoded string */
	Py_ssize_t 	from_length; /* length of string */
	size_t		to_length; /* length of result */
	int			encoding = -1; /* client encoding */

	if (PyBytes_Check(string))
	{
		PyBytes_AsStringAndSize(string, &from, &from_length);
	}
	else if (PyUnicode_Check(string))
	{
		encoding = PQclientEncoding(self->cnx);
		tmp_obj = get_encoded_string(string, encoding);
		if (!tmp_obj) return NULL; /* pass the UnicodeEncodeError */
		PyBytes_AsStringAndSize(tmp_obj, &from, &from_length);
	}
	else
	{
		PyErr_SetString(PyExc_TypeError,
			"Method escape_identifier() expects a string as argument");
		return NULL;
	}

	to = PQescapeIdentifier(self->cnx, from, (size_t)from_length);
	to_length = strlen(to);

	Py_XDECREF(tmp_obj);

	if (encoding == -1)
		to_obj = PyBytes_FromStringAndSize(to, to_length);
	else
		to_obj = get_decoded_string(to, to_length, encoding);
	if (to)
		PQfreemem(to);
	return to_obj;
}

#endif	/* ESCAPING_FUNCS */

/* escape string */
static char connEscapeString__doc__[] =
"escape_string(str) -- escape a string for use within SQL";

static PyObject *
connEscapeString(connObject *self, PyObject *string)
{
	PyObject   *tmp_obj = NULL, /* auxiliary string object */
			   *to_obj; /* string object to return */
	char 	   *from, /* our string argument as encoded string */
			   *to; /* the result as encoded string */
	Py_ssize_t 	from_length; /* length of string */
	size_t		to_length; /* length of result */
	int			encoding = -1; /* client encoding */

	if (PyBytes_Check(string))
	{
		PyBytes_AsStringAndSize(string, &from, &from_length);
	}
	else if (PyUnicode_Check(string))
	{
		encoding = PQclientEncoding(self->cnx);
		tmp_obj = get_encoded_string(string, encoding);
		if (!tmp_obj) return NULL; /* pass the UnicodeEncodeError */
		PyBytes_AsStringAndSize(tmp_obj, &from, &from_length);
	}
	else
	{
		PyErr_SetString(PyExc_TypeError,
			"Method escape_string() expects a string as argument");
		return NULL;
	}

	to_length = 2*from_length + 1;
	if ((Py_ssize_t)to_length < from_length) /* overflow */
	{
		to_length = from_length;
		from_length = (from_length - 1)/2;
	}
	to = (char *)PyMem_Malloc(to_length);
	to_length = PQescapeStringConn(self->cnx,
		to, from, (size_t)from_length, NULL);

	Py_XDECREF(tmp_obj);

	if (encoding == -1)
		to_obj = PyBytes_FromStringAndSize(to, to_length);
	else
		to_obj = get_decoded_string(to, to_length, encoding);
	PyMem_Free(to);
	return to_obj;
}

/* escape bytea */
static char connEscapeBytea__doc__[] =
"escape_bytea(data) -- escape binary data for use within SQL as type bytea";

static PyObject *
connEscapeBytea(connObject *self, PyObject *data)
{
	PyObject   *tmp_obj = NULL, /* auxiliary string object */
			   *to_obj; /* string object to return */
	char 	   *from, /* our string argument as encoded string */
			   *to; /* the result as encoded string */
	Py_ssize_t 	from_length; /* length of string */
	size_t		to_length; /* length of result */
	int			encoding = -1; /* client encoding */

	if (PyBytes_Check(data))
	{
		PyBytes_AsStringAndSize(data, &from, &from_length);
	}
	else if (PyUnicode_Check(data))
	{
		encoding = PQclientEncoding(self->cnx);
		tmp_obj = get_encoded_string(data, encoding);
		if (!tmp_obj) return NULL; /* pass the UnicodeEncodeError */
		PyBytes_AsStringAndSize(tmp_obj, &from, &from_length);
	}
	else
	{
		PyErr_SetString(PyExc_TypeError,
			"Method escape_bytea() expects a string as argument");
		return NULL;
	}

	to = (char *)PQescapeByteaConn(self->cnx,
		(unsigned char *)from, (size_t)from_length, &to_length);

	Py_XDECREF(tmp_obj);

	if (encoding == -1)
		to_obj = PyBytes_FromStringAndSize(to, to_length - 1);
	else
		to_obj = get_decoded_string(to, to_length - 1, encoding);
	if (to)
		PQfreemem(to);
	return to_obj;
}

#ifdef LARGE_OBJECTS
/* creates large object */
static char connCreateLO__doc__[] =
"locreate(mode) -- create a new large object in the database";

static PyObject *
connCreateLO(connObject *self, PyObject *args)
{
	int			mode;
	Oid			lo_oid;

	/* checks validity */
	if (!check_cnx_obj(self))
		return NULL;

	/* gets arguments */
	if (!PyArg_ParseTuple(args, "i", &mode))
	{
		PyErr_SetString(PyExc_TypeError,
			"Method locreate() takes an integer argument");
		return NULL;
	}

	/* creates large object */
	lo_oid = lo_creat(self->cnx, mode);
	if (lo_oid == 0)
	{
		set_error_msg(OperationalError, "Can't create large object");
		return NULL;
	}

	return (PyObject *) largeNew(self, lo_oid);
}

/* init from already known oid */
static char connGetLO__doc__[] =
"getlo(oid) -- create a large object instance for the specified oid";

static PyObject *
connGetLO(connObject *self, PyObject *args)
{
	int			lo_oid;

	/* checks validity */
	if (!check_cnx_obj(self))
		return NULL;

	/* gets arguments */
	if (!PyArg_ParseTuple(args, "i", &lo_oid))
	{
		PyErr_SetString(PyExc_TypeError,
			"Method getlo() takes an integer argument");
		return NULL;
	}

	if (!lo_oid)
	{
		PyErr_SetString(PyExc_ValueError, "The object oid can't be null");
		return NULL;
	}

	/* creates object */
	return (PyObject *) largeNew(self, lo_oid);
}

/* import unix file */
static char connImportLO__doc__[] =
"loimport(name) -- create a new large object from specified file";

static PyObject *
connImportLO(connObject *self, PyObject *args)
{
	char   *name;
	Oid		lo_oid;

	/* checks validity */
	if (!check_cnx_obj(self))
		return NULL;

	/* gets arguments */
	if (!PyArg_ParseTuple(args, "s", &name))
	{
		PyErr_SetString(PyExc_TypeError,
			"Method loimport() takes a string argument");
		return NULL;
	}

	/* imports file and checks result */
	lo_oid = lo_import(self->cnx, name);
	if (lo_oid == 0)
	{
		set_error_msg(OperationalError, "Can't create large object");
		return NULL;
	}

	return (PyObject *) largeNew(self, lo_oid);
}
#endif /* LARGE_OBJECTS */

/* resets connection */
static char connReset__doc__[] =
"reset() -- reset connection with current parameters\n\n"
"All derived queries and large objects derived from this connection\n"
"will not be usable after this call.\n";

static PyObject *
connReset(connObject *self, PyObject *noargs)
{
	if (!self->cnx)
	{
		PyErr_SetString(PyExc_TypeError, "Connection is not valid");
		return NULL;
	}

	/* resets the connection */
	PQreset(self->cnx);
	Py_INCREF(Py_None);
	return Py_None;
}

/* cancels current command */
static char connCancel__doc__[] =
"cancel() -- abandon processing of the current command";

static PyObject *
connCancel(connObject *self, PyObject *noargs)
{
	if (!self->cnx)
	{
		PyErr_SetString(PyExc_TypeError, "Connection is not valid");
		return NULL;
	}

	/* request that the server abandon processing of the current command */
	return PyInt_FromLong((long) PQrequestCancel(self->cnx));
}

/* get connection socket */
static char connFileno__doc__[] =
"fileno() -- return database connection socket file handle";

static PyObject *
connFileno(connObject *self, PyObject *noargs)
{
	if (!self->cnx)
	{
		PyErr_SetString(PyExc_TypeError, "Connection is not valid");
		return NULL;
	}

#ifdef NO_PQSOCKET
	return PyInt_FromLong((long) self->cnx->sock);
#else
	return PyInt_FromLong((long) PQsocket(self->cnx));
#endif
}

/* set external typecast callback function */
static char connSetCastHook__doc__[] =
"set_cast_hook(func) -- set a fallback typecast function";

static PyObject *
connSetCastHook(connObject *self, PyObject *func)
{
	PyObject *ret = NULL;

	if (func == Py_None)
	{
		Py_XDECREF(self->cast_hook);
		self->cast_hook = NULL;
		Py_INCREF(Py_None); ret = Py_None;
	}
	else if (PyCallable_Check(func))
	{
		Py_XINCREF(func); Py_XDECREF(self->cast_hook);
		self->cast_hook = func;
		Py_INCREF(Py_None); ret = Py_None;
	}
	else
		PyErr_SetString(PyExc_TypeError,
			"Method set_cast_hook() expects"
			 " a callable or None as argument");

	return ret;
}

/* get notice receiver callback function */
static char connGetCastHook__doc__[] =
"get_cast_hook() -- get the fallback typecast function";

static PyObject *
connGetCastHook(connObject *self, PyObject *noargs)
{
	PyObject *ret = self->cast_hook;;

	if (!ret)
		ret = Py_None;
	Py_INCREF(ret);

	return ret;
}

/* set notice receiver callback function */
static char connSetNoticeReceiver__doc__[] =
"set_notice_receiver(func) -- set the current notice receiver";

static PyObject *
connSetNoticeReceiver(connObject *self, PyObject *func)
{
	PyObject *ret = NULL;

	if (func == Py_None)
	{
		Py_XDECREF(self->notice_receiver);
		self->notice_receiver = NULL;
		Py_INCREF(Py_None); ret = Py_None;
	}
	else if (PyCallable_Check(func))
	{
		Py_XINCREF(func); Py_XDECREF(self->notice_receiver);
		self->notice_receiver = func;
		PQsetNoticeReceiver(self->cnx, notice_receiver, self);
		Py_INCREF(Py_None); ret = Py_None;
	}
	else
		PyErr_SetString(PyExc_TypeError,
			"Method set_notice_receiver() expects"
			 " a callable or None as argument");

	return ret;
}

/* get notice receiver callback function */
static char connGetNoticeReceiver__doc__[] =
"get_notice_receiver() -- get the current notice receiver";

static PyObject *
connGetNoticeReceiver(connObject *self, PyObject *noargs)
{
	PyObject *ret = self->notice_receiver;

	if (!ret)
		ret = Py_None;
	Py_INCREF(ret);

	return ret;
}

/* close without deleting */
static char connClose__doc__[] =
"close() -- close connection\n\n"
"All instances of the connection object and derived objects\n"
"(queries and large objects) can no longer be used after this call.\n";

static PyObject *
connClose(connObject *self, PyObject *noargs)
{
	/* connection object cannot already be closed */
	if (!self->cnx)
	{
		set_error_msg(InternalError, "Connection already closed");
		return NULL;
	}

	Py_BEGIN_ALLOW_THREADS
	PQfinish(self->cnx);
	Py_END_ALLOW_THREADS

	self->cnx = NULL;
	Py_INCREF(Py_None);
	return Py_None;
}

/* gets asynchronous notify */
static char connGetNotify__doc__[] =
"getnotify() -- get database notify for this connection";

static PyObject *
connGetNotify(connObject *self, PyObject *noargs)
{
	PGnotify   *notify;

	if (!self->cnx)
	{
		PyErr_SetString(PyExc_TypeError, "Connection is not valid");
		return NULL;
	}

	/* checks for NOTIFY messages */
	PQconsumeInput(self->cnx);

	if (!(notify = PQnotifies(self->cnx)))
	{
		Py_INCREF(Py_None);
		return Py_None;
	}
	else
	{
		PyObject   *notify_result,
				   *temp;

		if (!(temp = PyStr_FromString(notify->relname)))
			return NULL;

		if (!(notify_result = PyTuple_New(3)))
			return NULL;

		PyTuple_SET_ITEM(notify_result, 0, temp);

		if (!(temp = PyInt_FromLong(notify->be_pid)))
		{
			Py_DECREF(notify_result);
			return NULL;
		}

		PyTuple_SET_ITEM(notify_result, 1, temp);

		/* extra exists even in old versions that did not support it */
		if (!(temp = PyStr_FromString(notify->extra)))
		{
			Py_DECREF(notify_result);
			return NULL;
		}

		PyTuple_SET_ITEM(notify_result, 2, temp);

		PQfreemem(notify);

		return notify_result;
	}
}

/* get the list of connection attributes */
static PyObject *
connDir(connObject *self, PyObject *noargs)
{
	PyObject *attrs;

	attrs = PyObject_Dir(PyObject_Type((PyObject *)self));
	PyObject_CallMethod(attrs, "extend", "[sssssssss]",
		"host", "port", "db", "options", "error", "status", "user",
		"protocol_version", "server_version");

	return attrs;
}

/* connection object methods */
static struct PyMethodDef connMethods[] = {
	{"__dir__", (PyCFunction) connDir,  METH_NOARGS, NULL},

	{"source", (PyCFunction) connSource, METH_NOARGS, connSource__doc__},
	{"query", (PyCFunction) connQuery, METH_VARARGS, connQuery__doc__},
	{"reset", (PyCFunction) connReset, METH_NOARGS, connReset__doc__},
	{"cancel", (PyCFunction) connCancel, METH_NOARGS, connCancel__doc__},
	{"close", (PyCFunction) connClose, METH_NOARGS, connClose__doc__},
	{"fileno", (PyCFunction) connFileno, METH_NOARGS, connFileno__doc__},
	{"get_cast_hook", (PyCFunction) connGetCastHook, METH_NOARGS,
			connGetCastHook__doc__},
	{"set_cast_hook", (PyCFunction) connSetCastHook, METH_O,
			connSetCastHook__doc__},
	{"get_notice_receiver", (PyCFunction) connGetNoticeReceiver, METH_NOARGS,
			connGetNoticeReceiver__doc__},
	{"set_notice_receiver", (PyCFunction) connSetNoticeReceiver, METH_O,
			connSetNoticeReceiver__doc__},
	{"getnotify", (PyCFunction) connGetNotify, METH_NOARGS,
			connGetNotify__doc__},
	{"inserttable", (PyCFunction) connInsertTable, METH_VARARGS,
			connInsertTable__doc__},
	{"transaction", (PyCFunction) connTransaction, METH_NOARGS,
			connTransaction__doc__},
	{"parameter", (PyCFunction) connParameter, METH_VARARGS,
			connParameter__doc__},
	{"date_format", (PyCFunction) connDateFormat, METH_NOARGS,
			connDateFormat__doc__},

#ifdef ESCAPING_FUNCS
	{"escape_literal", (PyCFunction) connEscapeLiteral, METH_O,
			connEscapeLiteral__doc__},
	{"escape_identifier", (PyCFunction) connEscapeIdentifier, METH_O,
			connEscapeIdentifier__doc__},
#endif	/* ESCAPING_FUNCS */
	{"escape_string", (PyCFunction) connEscapeString, METH_O,
			connEscapeString__doc__},
	{"escape_bytea", (PyCFunction) connEscapeBytea, METH_O,
			connEscapeBytea__doc__},

#ifdef DIRECT_ACCESS
	{"putline", (PyCFunction) connPutLine, METH_VARARGS, connPutLine__doc__},
	{"getline", (PyCFunction) connGetLine, METH_NOARGS, connGetLine__doc__},
	{"endcopy", (PyCFunction) connEndCopy, METH_NOARGS, connEndCopy__doc__},
#endif /* DIRECT_ACCESS */

#ifdef LARGE_OBJECTS
	{"locreate", (PyCFunction) connCreateLO, METH_VARARGS, connCreateLO__doc__},
	{"getlo", (PyCFunction) connGetLO, METH_VARARGS, connGetLO__doc__},
	{"loimport", (PyCFunction) connImportLO, METH_VARARGS, connImportLO__doc__},
#endif /* LARGE_OBJECTS */

	{NULL, NULL} /* sentinel */
};

/* gets connection attributes */
static PyObject *
connGetAttr(connObject *self, PyObject *nameobj)
{
	const char *name = PyStr_AsString(nameobj);

	/*
	 * Although we could check individually, there are only a few
	 * attributes that don't require a live connection and unless someone
	 * has an urgent need, this will have to do
	 */

	/* first exception - close which returns a different error */
	if (strcmp(name, "close") && !self->cnx)
	{
		PyErr_SetString(PyExc_TypeError, "Connection is not valid");
		return NULL;
	}

	/* list PostgreSQL connection fields */

	/* postmaster host */
	if (!strcmp(name, "host"))
	{
		char *r = PQhost(self->cnx);
		if (!r)
			r = "localhost";
		return PyStr_FromString(r);
	}

	/* postmaster port */
	if (!strcmp(name, "port"))
		return PyInt_FromLong(atol(PQport(self->cnx)));

	/* selected database */
	if (!strcmp(name, "db"))
		return PyStr_FromString(PQdb(self->cnx));

	/* selected options */
	if (!strcmp(name, "options"))
		return PyStr_FromString(PQoptions(self->cnx));

	/* error (status) message */
	if (!strcmp(name, "error"))
		return PyStr_FromString(PQerrorMessage(self->cnx));

	/* connection status : 1 - OK, 0 - BAD */
	if (!strcmp(name, "status"))
		return PyInt_FromLong(PQstatus(self->cnx) == CONNECTION_OK ? 1 : 0);

	/* provided user name */
	if (!strcmp(name, "user"))
		return PyStr_FromString(PQuser(self->cnx));

	/* protocol version */
	if (!strcmp(name, "protocol_version"))
		return PyInt_FromLong(PQprotocolVersion(self->cnx));

	/* backend version */
	if (!strcmp(name, "server_version"))
		return PyInt_FromLong(PQserverVersion(self->cnx));

	return PyObject_GenericGetAttr((PyObject *) self, nameobj);
}

/* connection type definition */
static PyTypeObject connType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"pg.Connection",			/* tp_name */
	sizeof(connObject),			/* tp_basicsize */
	0,							/* tp_itemsize */
	(destructor) connDelete,	/* tp_dealloc */
	0,							/* tp_print */
	0,							/* tp_getattr */
	0,							/* tp_setattr */
	0,							/* tp_reserved */
	0,							/* tp_repr */
	0,							/* tp_as_number */
	0,							/* tp_as_sequence */
	0,							/* tp_as_mapping */
	0,							/* tp_hash */
	0,							/* tp_call */
	0,							/* tp_str */
	(getattrofunc) connGetAttr,	/* tp_getattro */
	0,							/* tp_setattro */
	0,							/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,			/* tp_flags */
	0,							/* tp_doc */
	0,							/* tp_traverse */
	0,							/* tp_clear */
	0,							/* tp_richcompare */
	0,							/* tp_weaklistoffset */
	0,							/* tp_iter */
	0,							/* tp_iternext */
	connMethods,				/* tp_methods */
};

/* --------------------------------------------------------------------- */
/* source object														 */
/* --------------------------------------------------------------------- */
/* checks source object validity */
static int
check_source_obj(sourceObject *self, int level)
{
	if (!self->valid)
	{
		set_error_msg(OperationalError, "Object has been closed");
		return 0;
	}

	if ((level & CHECK_RESULT) && !self->result)
	{
		set_error_msg(DatabaseError, "No result");
		return 0;
	}

	if ((level & CHECK_DQL) && self->result_type != RESULT_DQL)
	{
		set_error_msg(DatabaseError, "Last query did not return tuples");
		return 0;
	}

	if ((level & CHECK_CNX) && !check_cnx_obj(self->pgcnx))
		return 0;

	return 1;
}

/* destructor */
static void
sourceDealloc(sourceObject *self)
{
	if (self->result)
		PQclear(self->result);

	Py_XDECREF(self->pgcnx);
	PyObject_Del(self);
}

/* closes object */
static char sourceClose__doc__[] =
"close() -- close query object without deleting it\n\n"
"All instances of the query object can no longer be used after this call.\n";

static PyObject *
sourceClose(sourceObject *self, PyObject *noargs)
{
	/* frees result if necessary and invalidates object */
	if (self->result)
	{
		PQclear(self->result);
		self->result_type = RESULT_EMPTY;
		self->result = NULL;
	}

	self->valid = 0;

	/* return None */
	Py_INCREF(Py_None);
	return Py_None;
}

/* database query */
static char sourceExecute__doc__[] =
"execute(sql) -- execute a SQL statement (string)\n\n"
"On success, this call returns the number of affected rows, or None\n"
"for DQL (SELECT, ...) statements.  The fetch (fetch(), fetchone()\n"
"and fetchall()) methods can be used to get result rows.\n";

static PyObject *
sourceExecute(sourceObject *self, PyObject *sql)
{
	PyObject   *tmp_obj = NULL; /* auxiliary string object */
	char	   *query;
	int			encoding;

	/* checks validity */
	if (!check_source_obj(self, CHECK_CNX))
		return NULL;

	encoding = PQclientEncoding(self->pgcnx->cnx);

	if (PyBytes_Check(sql))
	{
		query = PyBytes_AsString(sql);
	}
	else if (PyUnicode_Check(sql))
	{
		tmp_obj = get_encoded_string(sql, encoding);
		if (!tmp_obj) return NULL; /* pass the UnicodeEncodeError */
		query = PyBytes_AsString(tmp_obj);
	}
	else
	{
		PyErr_SetString(PyExc_TypeError,
			"Method execute() expects a string as argument");
		return NULL;
	}

	/* frees previous result */
	if (self->result)
	{
		PQclear(self->result);
		self->result = NULL;
	}
	self->max_row = 0;
	self->current_row = 0;
	self->num_fields = 0;
	self->encoding = encoding;

	/* gets result */
	Py_BEGIN_ALLOW_THREADS
	self->result = PQexec(self->pgcnx->cnx, query);
	Py_END_ALLOW_THREADS

	/* we don't need the auxiliary string any more */
	Py_XDECREF(tmp_obj);

	/* checks result validity */
	if (!self->result)
	{
		PyErr_SetString(PyExc_ValueError, PQerrorMessage(self->pgcnx->cnx));
		return NULL;
	}

	/* this may have changed the datestyle, so we reset the date format
	   in order to force fetching it newly when next time requested */
	self->pgcnx->date_format = date_format; /* this is normally NULL */

	/* checks result status */
	switch (PQresultStatus(self->result))
	{
		long	num_rows;
		char   *temp;

		/* query succeeded */
		case PGRES_TUPLES_OK:	/* DQL: returns None (DB-SIG compliant) */
			self->result_type = RESULT_DQL;
			self->max_row = PQntuples(self->result);
			self->num_fields = PQnfields(self->result);
			Py_INCREF(Py_None);
			return Py_None;
		case PGRES_COMMAND_OK:	/* other requests */
		case PGRES_COPY_OUT:
		case PGRES_COPY_IN:
			self->result_type = RESULT_DDL;
			temp = PQcmdTuples(self->result);
			num_rows = -1;
			if (temp[0])
			{
				self->result_type = RESULT_DML;
				num_rows = atol(temp);
			}
			return PyInt_FromLong(num_rows);

		/* query failed */
		case PGRES_EMPTY_QUERY:
			PyErr_SetString(PyExc_ValueError, "Empty query");
			break;
		case PGRES_BAD_RESPONSE:
		case PGRES_FATAL_ERROR:
		case PGRES_NONFATAL_ERROR:
			set_error(ProgrammingError, "Cannot execute command",
				self->pgcnx->cnx, self->result);
			break;
		default:
			set_error_msg(InternalError, "Internal error: "
				"unknown result status");
	}

	/* frees result and returns error */
	PQclear(self->result);
	self->result = NULL;
	self->result_type = RESULT_EMPTY;
	return NULL;
}

/* gets oid status for last query (valid for INSERTs, 0 for other) */
static char sourceStatusOID__doc__[] =
"oidstatus() -- return oid of last inserted row (if available)";

static PyObject *
sourceStatusOID(sourceObject *self, PyObject *noargs)
{
	Oid			oid;

	/* checks validity */
	if (!check_source_obj(self, CHECK_RESULT))
		return NULL;

	/* retrieves oid status */
	if ((oid = PQoidValue(self->result)) == InvalidOid)
	{
		Py_INCREF(Py_None);
		return Py_None;
	}

	return PyInt_FromLong(oid);
}

/* fetches rows from last result */
static char sourceFetch__doc__[] =
"fetch(num) -- return the next num rows from the last result in a list\n\n"
"If num parameter is omitted arraysize attribute value is used.\n"
"If size equals -1, all rows are fetched.\n";

static PyObject *
sourceFetch(sourceObject *self, PyObject *args)
{
	PyObject   *reslist;
	int			i,
				k;
	long		size;
#if IS_PY3
	int			encoding;
#endif

	/* checks validity */
	if (!check_source_obj(self, CHECK_RESULT | CHECK_DQL | CHECK_CNX))
		return NULL;

	/* checks args */
	size = self->arraysize;
	if (!PyArg_ParseTuple(args, "|l", &size))
	{
		PyErr_SetString(PyExc_TypeError,
			"fetch(num), with num (integer, optional)");
		return NULL;
	}

	/* seeks last line */
	/* limit size to be within the amount of data we actually have */
	if (size == -1 || (self->max_row - self->current_row) < size)
		size = self->max_row - self->current_row;

	/* allocate list for result */
	if (!(reslist = PyList_New(0))) return NULL;

#if IS_PY3
	encoding = self->encoding;
#endif

	/* builds result */
	for (i = 0, k = self->current_row; i < size; ++i, ++k)
	{
		PyObject   *rowtuple;
		int			j;

		if (!(rowtuple = PyTuple_New(self->num_fields)))
		{
			Py_DECREF(reslist); return NULL;
		}

		for (j = 0; j < self->num_fields; ++j)
		{
			PyObject   *str;

			if (PQgetisnull(self->result, k, j))
			{
				Py_INCREF(Py_None);
				str = Py_None;
			}
			else
			{
				char *s = PQgetvalue(self->result, k, j);
				Py_ssize_t size = PQgetlength(self->result, k, j);
#if IS_PY3
				if (PQfformat(self->result, j) == 0) /* textual format */
				{
					str = get_decoded_string(s, size, encoding);
					if (!str) /* cannot decode */
						str = PyBytes_FromStringAndSize(s, size);
				}
				else
#endif
				str = PyBytes_FromStringAndSize(s, size);
			}
			PyTuple_SET_ITEM(rowtuple, j, str);
		}

		if (PyList_Append(reslist, rowtuple))
		{
			Py_DECREF(rowtuple); Py_DECREF(reslist); return NULL;
		}
		Py_DECREF(rowtuple);
	}

	self->current_row = k;
	return reslist;
}

/* changes current row (internal wrapper for all "move" methods) */
static PyObject *
pgsource_move(sourceObject *self, int move)
{
	/* checks validity */
	if (!check_source_obj(self, CHECK_RESULT | CHECK_DQL))
		return NULL;

	/* changes the current row */
	switch (move)
	{
		case QUERY_MOVEFIRST:
			self->current_row = 0;
			break;
		case QUERY_MOVELAST:
			self->current_row = self->max_row - 1;
			break;
		case QUERY_MOVENEXT:
			if (self->current_row != self->max_row)
				++self->current_row;
			break;
		case QUERY_MOVEPREV:
			if (self->current_row > 0)
				self->current_row--;
			break;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/* move to first result row */
static char sourceMoveFirst__doc__[] =
"movefirst() -- move to first result row";

static PyObject *
sourceMoveFirst(sourceObject *self, PyObject *noargs)
{
	return pgsource_move(self, QUERY_MOVEFIRST);
}

/* move to last result row */
static char sourceMoveLast__doc__[] =
"movelast() -- move to last valid result row";

static PyObject *
sourceMoveLast(sourceObject *self, PyObject *noargs)
{
	return pgsource_move(self, QUERY_MOVELAST);
}

/* move to next result row */
static char sourceMoveNext__doc__[] =
"movenext() -- move to next result row";

static PyObject *
sourceMoveNext(sourceObject *self, PyObject *noargs)
{
	return pgsource_move(self, QUERY_MOVENEXT);
}

/* move to previous result row */
static char sourceMovePrev__doc__[] =
"moveprev() -- move to previous result row";

static PyObject *
sourceMovePrev(sourceObject *self, PyObject *noargs)
{
	return pgsource_move(self, QUERY_MOVEPREV);
}

/* put copy data */
static char sourcePutData__doc__[] =
"putdata(buffer) -- send data to server during copy from stdin";

static PyObject *
sourcePutData(sourceObject *self, PyObject *buffer)
{
	PyObject   *tmp_obj = NULL; /* an auxiliary object */
	char 	   *buf; /* the buffer as encoded string */
	Py_ssize_t 	nbytes; /* length of string */
	char	   *errormsg = NULL; /* error message */
	int			res; /* direct result of the operation */
	PyObject   *ret; /* return value */

	/* checks validity */
	if (!check_source_obj(self, CHECK_CNX))
		return NULL;

	/* make sure that the connection object is valid */
	if (!self->pgcnx->cnx)
		return NULL;

	if (buffer == Py_None)
	{
		/* pass None for terminating the operation */
		buf = errormsg = NULL;
	}
	else if (PyBytes_Check(buffer))
	{
		/* or pass a byte string */
		PyBytes_AsStringAndSize(buffer, &buf, &nbytes);
	}
	else if (PyUnicode_Check(buffer))
	{
		/* or pass a unicode string */
		tmp_obj = get_encoded_string(
			buffer, PQclientEncoding(self->pgcnx->cnx));
		if (!tmp_obj) return NULL; /* pass the UnicodeEncodeError */
		PyBytes_AsStringAndSize(tmp_obj, &buf, &nbytes);
	}
	else if (PyErr_GivenExceptionMatches(buffer, PyExc_BaseException))
	{
		/* or pass a Python exception for sending an error message */
		tmp_obj = PyObject_Str(buffer);
		if (PyUnicode_Check(tmp_obj))
		{
			PyObject *obj = tmp_obj;
			tmp_obj = get_encoded_string(
				obj, PQclientEncoding(self->pgcnx->cnx));
			Py_DECREF(obj);
			if (!tmp_obj) return NULL; /* pass the UnicodeEncodeError */
		}
		errormsg = PyBytes_AsString(tmp_obj);
		buf = NULL;
	}
	else
	{
		PyErr_SetString(PyExc_TypeError,
			"Method putdata() expects a buffer, None"
			 " or an exception as argument");
		return NULL;
	}

	/* checks validity */
	if (!check_source_obj(self, CHECK_CNX | CHECK_RESULT) ||
			PQresultStatus(self->result) != PGRES_COPY_IN)
	{
		PyErr_SetString(PyExc_IOError,
			"Connection is invalid or not in copy_in state");
		Py_XDECREF(tmp_obj);
		return NULL;
	}

	if (buf)
	{
		res = nbytes ? PQputCopyData(self->pgcnx->cnx, buf, (int)nbytes) : 1;
	}
	else
	{
		res = PQputCopyEnd(self->pgcnx->cnx, errormsg);
	}

	Py_XDECREF(tmp_obj);

	if (res != 1)
	{
		PyErr_SetString(PyExc_IOError, PQerrorMessage(self->pgcnx->cnx));
		return NULL;
	}

	if (buf) /* buffer has been sent */
	{
		ret = Py_None;
		Py_INCREF(ret);
	}
	else /* copy is done */
	{
		PGresult   *result; /* final result of the operation */

		Py_BEGIN_ALLOW_THREADS;
		result = PQgetResult(self->pgcnx->cnx);
		Py_END_ALLOW_THREADS;

		if (PQresultStatus(result) == PGRES_COMMAND_OK)
		{
			char   *temp;
			long	num_rows;

			temp = PQcmdTuples(result);
			num_rows = temp[0] ? atol(temp) : -1;
			ret = PyInt_FromLong(num_rows);
		}
		else
		{
			if (!errormsg) errormsg = PQerrorMessage(self->pgcnx->cnx);
			PyErr_SetString(PyExc_IOError, errormsg);
			ret = NULL;
		}

		PQclear(self->result);
		self->result = NULL;
		self->result_type = RESULT_EMPTY;
	}

	return ret; /* None or number of rows */
}

/* get copy data */
static char sourceGetData__doc__[] =
"getdata(decode) -- receive data to server during copy to stdout";

static PyObject *
sourceGetData(sourceObject *self, PyObject *args)
{
	int		   *decode = 0; /* decode flag */
	char 	   *buffer; /* the copied buffer as encoded byte string */
	Py_ssize_t 	nbytes; /* length of the byte string */
	PyObject   *ret; /* return value */

	/* checks validity */
	if (!check_source_obj(self, CHECK_CNX))
		return NULL;

	/* make sure that the connection object is valid */
	if (!self->pgcnx->cnx)
		return NULL;

	if (!PyArg_ParseTuple(args, "|i", &decode))
		return NULL;

	/* checks validity */
	if (!check_source_obj(self, CHECK_CNX | CHECK_RESULT) ||
			PQresultStatus(self->result) != PGRES_COPY_OUT)
	{
		PyErr_SetString(PyExc_IOError,
			"Connection is invalid or not in copy_out state");
		return NULL;
	}

	nbytes = PQgetCopyData(self->pgcnx->cnx, &buffer, 0);

	if (!nbytes || nbytes < -1) /* an error occurred */
	{
		PyErr_SetString(PyExc_IOError, PQerrorMessage(self->pgcnx->cnx));
		return NULL;
	}

	if (nbytes == -1) /* copy is done */
	{
		PGresult   *result; /* final result of the operation */

		Py_BEGIN_ALLOW_THREADS;
		result = PQgetResult(self->pgcnx->cnx);
		Py_END_ALLOW_THREADS;

		if (PQresultStatus(result) == PGRES_COMMAND_OK)
		{
			char   *temp;
			long	num_rows;

			temp = PQcmdTuples(result);
			num_rows = temp[0] ? atol(temp) : -1;
			ret = PyInt_FromLong(num_rows);
		}
		else
		{
			PyErr_SetString(PyExc_IOError, PQerrorMessage(self->pgcnx->cnx));
			ret = NULL;
		}

		PQclear(self->result);
		self->result = NULL;
		self->result_type = RESULT_EMPTY;
	}
	else /* a row has been returned */
	{
		ret = decode ? get_decoded_string(
				buffer, nbytes, PQclientEncoding(self->pgcnx->cnx)) :
			PyBytes_FromStringAndSize(buffer, nbytes);
		PQfreemem(buffer);
	}

	return ret; /* buffer or number of rows */
}

/* finds field number from string/integer (internal use only) */
static int
sourceFieldindex(sourceObject *self, PyObject *param, const char *usage)
{
	int			num;

	/* checks validity */
	if (!check_source_obj(self, CHECK_RESULT | CHECK_DQL))
		return -1;

	/* gets field number */
	if (PyStr_Check(param))
		num = PQfnumber(self->result, PyBytes_AsString(param));
	else if (PyInt_Check(param))
		num = PyInt_AsLong(param);
	else
	{
		PyErr_SetString(PyExc_TypeError, usage);
		return -1;
	}

	/* checks field validity */
	if (num < 0 || num >= self->num_fields)
	{
		PyErr_SetString(PyExc_ValueError, "Unknown field");
		return -1;
	}

	return num;
}

/* builds field information from position (internal use only) */
static PyObject *
pgsource_buildinfo(sourceObject *self, int num)
{
	PyObject *result;

	/* allocates tuple */
	result = PyTuple_New(5);
	if (!result)
		return NULL;

	/* affects field information */
	PyTuple_SET_ITEM(result, 0, PyInt_FromLong(num));
	PyTuple_SET_ITEM(result, 1,
		PyStr_FromString(PQfname(self->result, num)));
	PyTuple_SET_ITEM(result, 2,
		PyInt_FromLong(PQftype(self->result, num)));
	PyTuple_SET_ITEM(result, 3,
		PyInt_FromLong(PQfsize(self->result, num)));
	PyTuple_SET_ITEM(result, 4,
		PyInt_FromLong(PQfmod(self->result, num)));

	return result;
}

/* lists fields info */
static char sourceListInfo__doc__[] =
"listinfo() -- get information for all fields (position, name, type oid)";

static PyObject *
sourceListInfo(sourceObject *self, PyObject *noargs)
{
	int			i;
	PyObject   *result,
			   *info;

	/* checks validity */
	if (!check_source_obj(self, CHECK_RESULT | CHECK_DQL))
		return NULL;

	/* builds result */
	if (!(result = PyTuple_New(self->num_fields)))
		return NULL;

	for (i = 0; i < self->num_fields; ++i)
	{
		info = pgsource_buildinfo(self, i);
		if (!info)
		{
			Py_DECREF(result);
			return NULL;
		}
		PyTuple_SET_ITEM(result, i, info);
	}

	/* returns result */
	return result;
};

/* list fields information for last result */
static char sourceFieldInfo__doc__[] =
"fieldinfo(desc) -- get specified field info (position, name, type oid)";

static PyObject *
sourceFieldInfo(sourceObject *self, PyObject *desc)
{
	int			num;

	/* checks args and validity */
	if ((num = sourceFieldindex(self, desc,
			"Method fieldinfo() needs a string or integer as argument")) == -1)
		return NULL;

	/* returns result */
	return pgsource_buildinfo(self, num);
};

/* retrieve field value */
static char sourceField__doc__[] =
"field(desc) -- return specified field value";

static PyObject *
sourceField(sourceObject *self, PyObject *desc)
{
	int			num;

	/* checks args and validity */
	if ((num = sourceFieldindex(self, desc,
			"Method field() needs a string or integer as argument")) == -1)
		return NULL;

	return PyStr_FromString(
		PQgetvalue(self->result, self->current_row, num));
}

/* get the list of source object attributes */
static PyObject *
sourceDir(connObject *self, PyObject *noargs)
{
	PyObject *attrs;

	attrs = PyObject_Dir(PyObject_Type((PyObject *)self));
	PyObject_CallMethod(attrs, "extend", "[sssss]",
		"pgcnx", "arraysize", "resulttype", "ntuples", "nfields");

	return attrs;
}

/* source object methods */
static PyMethodDef sourceMethods[] = {
	{"__dir__", (PyCFunction) sourceDir,  METH_NOARGS, NULL},
	{"close", (PyCFunction) sourceClose, METH_NOARGS, sourceClose__doc__},
	{"execute", (PyCFunction) sourceExecute, METH_O, sourceExecute__doc__},
	{"oidstatus", (PyCFunction) sourceStatusOID, METH_NOARGS,
			sourceStatusOID__doc__},
	{"fetch", (PyCFunction) sourceFetch, METH_VARARGS,
			sourceFetch__doc__},
	{"movefirst", (PyCFunction) sourceMoveFirst, METH_NOARGS,
			sourceMoveFirst__doc__},
	{"movelast", (PyCFunction) sourceMoveLast, METH_NOARGS,
			sourceMoveLast__doc__},
	{"movenext", (PyCFunction) sourceMoveNext, METH_NOARGS,
			sourceMoveNext__doc__},
	{"moveprev", (PyCFunction) sourceMovePrev, METH_NOARGS,
			sourceMovePrev__doc__},
	{"putdata", (PyCFunction) sourcePutData, METH_O, sourcePutData__doc__},
	{"getdata", (PyCFunction) sourceGetData, METH_VARARGS,
			sourceGetData__doc__},
	{"field", (PyCFunction) sourceField, METH_O,
			sourceField__doc__},
	{"fieldinfo", (PyCFunction) sourceFieldInfo, METH_O,
			sourceFieldInfo__doc__},
	{"listinfo", (PyCFunction) sourceListInfo, METH_NOARGS,
			sourceListInfo__doc__},
	{NULL, NULL}
};

/* gets source object attributes */
static PyObject *
sourceGetAttr(sourceObject *self, PyObject *nameobj)
{
	const char *name = PyStr_AsString(nameobj);

	/* pg connection object */
	if (!strcmp(name, "pgcnx"))
	{
		if (check_source_obj(self, 0))
		{
			Py_INCREF(self->pgcnx);
			return (PyObject *) (self->pgcnx);
		}
		Py_INCREF(Py_None);
		return Py_None;
	}

	/* arraysize */
	if (!strcmp(name, "arraysize"))
		return PyInt_FromLong(self->arraysize);

	/* resulttype */
	if (!strcmp(name, "resulttype"))
		return PyInt_FromLong(self->result_type);

	/* ntuples */
	if (!strcmp(name, "ntuples"))
		return PyInt_FromLong(self->max_row);

	/* nfields */
	if (!strcmp(name, "nfields"))
		return PyInt_FromLong(self->num_fields);

	/* seeks name in methods (fallback) */
	return PyObject_GenericGetAttr((PyObject *) self, nameobj);
}

/* sets query object attributes */
static int
sourceSetAttr(sourceObject *self, char *name, PyObject *v)
{
	/* arraysize */
	if (!strcmp(name, "arraysize"))
	{
		if (!PyInt_Check(v))
		{
			PyErr_SetString(PyExc_TypeError, "arraysize must be integer");
			return -1;
		}

		self->arraysize = PyInt_AsLong(v);
		return 0;
	}

	/* unknown attribute */
	PyErr_SetString(PyExc_TypeError, "Not a writable attribute");
	return -1;
}

/* return source object as string in human readable form */
static PyObject *
sourceStr(sourceObject *self)
{
	switch (self->result_type)
	{
		case RESULT_DQL:
			return format_result(self->result);
		case RESULT_DDL:
		case RESULT_DML:
			return PyStr_FromString(PQcmdStatus(self->result));
		case RESULT_EMPTY:
		default:
			return PyStr_FromString("(empty PostgreSQL source object)");
	}
}

static char source__doc__[] = "PyGreSQL source object";

/* source type definition */
static PyTypeObject sourceType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"pgdb.Source",					/* tp_name */
	sizeof(sourceObject),			/* tp_basicsize */
	0,								/* tp_itemsize */
	/* methods */
	(destructor) sourceDealloc,		/* tp_dealloc */
	0,								/* tp_print */
	0,								/* tp_getattr */
	(setattrfunc) sourceSetAttr,	/* tp_setattr */
	0,								/* tp_compare */
	0,								/* tp_repr */
	0,								/* tp_as_number */
	0,								/* tp_as_sequence */
	0,								/* tp_as_mapping */
	0,								/* tp_hash */
	0,								/* tp_call */
	(reprfunc) sourceStr,			/* tp_str */
	(getattrofunc) sourceGetAttr,	/* tp_getattro */
	0,								/* tp_setattro */
	0,								/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,				/* tp_flags */
	source__doc__,					/* tp_doc */
	0,								/* tp_traverse */
	0,								/* tp_clear */
	0,								/* tp_richcompare */
	0,								/* tp_weaklistoffset */
	0,								/* tp_iter */
	0,								/* tp_iternext */
	sourceMethods,					/* tp_methods */
};

/* connects to a database */
static char pgConnect__doc__[] =
"connect(dbname, host, port, opt) -- connect to a PostgreSQL database\n\n"
"The connection uses the specified parameters (optional, keywords aware).\n";

static PyObject *
pgConnect(PyObject *self, PyObject *args, PyObject *dict)
{
	static const char *kwlist[] = {"dbname", "host", "port", "opt",
	"user", "passwd", NULL};

	char	   *pghost,
			   *pgopt,
			   *pgdbname,
			   *pguser,
			   *pgpasswd;
	int			pgport;
	char		port_buffer[20];
	connObject *npgobj;

	pghost = pgopt = pgdbname = pguser = pgpasswd = NULL;
	pgport = -1;

	/*
	 * parses standard arguments With the right compiler warnings, this
	 * will issue a diagnostic. There is really no way around it.  If I
	 * don't declare kwlist as const char *kwlist[] then it complains when
	 * I try to assign all those constant strings to it.
	 */
	if (!PyArg_ParseTupleAndKeywords(args, dict, "|zzizzz", (char **) kwlist,
		&pgdbname, &pghost, &pgport, &pgopt, &pguser, &pgpasswd))
		return NULL;

#ifdef DEFAULT_VARS
	/* handles defaults variables (for uninitialised vars) */
	if ((!pghost) && (pg_default_host != Py_None))
		pghost = PyBytes_AsString(pg_default_host);

	if ((pgport == -1) && (pg_default_port != Py_None))
		pgport = PyInt_AsLong(pg_default_port);

	if ((!pgopt) && (pg_default_opt != Py_None))
		pgopt = PyBytes_AsString(pg_default_opt);

	if ((!pgdbname) && (pg_default_base != Py_None))
		pgdbname = PyBytes_AsString(pg_default_base);

	if ((!pguser) && (pg_default_user != Py_None))
		pguser = PyBytes_AsString(pg_default_user);

	if ((!pgpasswd) && (pg_default_passwd != Py_None))
		pgpasswd = PyBytes_AsString(pg_default_passwd);
#endif /* DEFAULT_VARS */

	if (!(npgobj = PyObject_NEW(connObject, &connType)))
	{
		set_error_msg(InternalError, "Can't create new connection object");
		return NULL;
	}

	npgobj->valid = 1;
	npgobj->cnx = NULL;
	npgobj->date_format = date_format;
	npgobj->cast_hook = NULL;
	npgobj->notice_receiver = NULL;

	if (pgport != -1)
	{
		memset(port_buffer, 0, sizeof(port_buffer));
		sprintf(port_buffer, "%d", pgport);
	}

	Py_BEGIN_ALLOW_THREADS
	npgobj->cnx = PQsetdbLogin(pghost, pgport == -1 ? NULL : port_buffer,
		pgopt, NULL, pgdbname, pguser, pgpasswd);
	Py_END_ALLOW_THREADS

	if (PQstatus(npgobj->cnx) == CONNECTION_BAD)
	{
		set_error(InternalError, "Cannot connect", npgobj->cnx, NULL);
		Py_XDECREF(npgobj);
		return NULL;
	}

	return (PyObject *) npgobj;
}

static void
queryDealloc(queryObject *self)
{
	Py_XDECREF(self->pgcnx);
	if (self->result)
		PQclear(self->result);

	PyObject_Del(self);
}

/* get number of rows */
static char queryNTuples__doc__[] =
"ntuples() -- return number of tuples returned by query";

static PyObject *
queryNTuples(queryObject *self, PyObject *noargs)
{
	return PyInt_FromLong((long) PQntuples(self->result));
}

/* list fields names from query result */
static char queryListFields__doc__[] =
"listfields() -- List field names from result";

static PyObject *
queryListFields(queryObject *self, PyObject *noargs)
{
	int			i,
				n;
	char	   *name;
	PyObject   *fieldstuple,
			   *str;

	/* builds tuple */
	n = PQnfields(self->result);
	fieldstuple = PyTuple_New(n);

	for (i = 0; i < n; ++i)
	{
		name = PQfname(self->result, i);
		str = PyStr_FromString(name);
		PyTuple_SET_ITEM(fieldstuple, i, str);
	}

	return fieldstuple;
}

/* get field name from last result */
static char queryFieldName__doc__[] =
"fieldname(num) -- return name of field from result from its position";

static PyObject *
queryFieldName(queryObject *self, PyObject *args)
{
	int		i;
	char   *name;

	/* gets args */
	if (!PyArg_ParseTuple(args, "i", &i))
	{
		PyErr_SetString(PyExc_TypeError,
			"Method fieldname() takes an integer as argument");
		return NULL;
	}

	/* checks number validity */
	if (i >= PQnfields(self->result))
	{
		PyErr_SetString(PyExc_ValueError, "Invalid field number");
		return NULL;
	}

	/* gets fields name and builds object */
	name = PQfname(self->result, i);
	return PyStr_FromString(name);
}

/* gets fields number from name in last result */
static char queryFieldNumber__doc__[] =
"fieldnum(name) -- return position in query for field from its name";

static PyObject *
queryFieldNumber(queryObject *self, PyObject *args)
{
	int		num;
	char   *name;

	/* gets args */
	if (!PyArg_ParseTuple(args, "s", &name))
	{
		PyErr_SetString(PyExc_TypeError,
			"Method fieldnum() takes a string as argument");
		return NULL;
	}

	/* gets field number */
	if ((num = PQfnumber(self->result, name)) == -1)
	{
		PyErr_SetString(PyExc_ValueError, "Unknown field");
		return NULL;
	}

	return PyInt_FromLong(num);
}

/* retrieves last result */
static char queryGetResult__doc__[] =
"getresult() -- Get the result of a query\n\n"
"The result is returned as a list of rows, each one a tuple of fields\n"
"in the order returned by the server.\n";

static PyObject *
queryGetResult(queryObject *self, PyObject *noargs)
{
	PyObject   *reslist;
	int			i, m, n, *col_types;
	int			encoding = self->encoding;

	/* stores result in tuple */
	m = PQntuples(self->result);
	n = PQnfields(self->result);
	if (!(reslist = PyList_New(m))) return NULL;

	if (!(col_types = get_col_types(self->result, n))) return NULL;

	for (i = 0; i < m; ++i)
	{
		PyObject   *rowtuple;
		int			j;

		if (!(rowtuple = PyTuple_New(n)))
		{
			Py_DECREF(reslist);
			reslist = NULL;
			goto exit;
		}

		for (j = 0; j < n; ++j)
		{
			PyObject * val;

			if (PQgetisnull(self->result, i, j))
			{
				Py_INCREF(Py_None);
				val = Py_None;
			}
			else /* not null */
			{
				/* get the string representation of the value */
				/* note: this is always null-terminated text format */
				char   *s = PQgetvalue(self->result, i, j);
				/* get the PyGreSQL type of the column */
				int		type = col_types[j];

				if (type & PYGRES_ARRAY)
					val = cast_array(s, PQgetlength(self->result, i, j),
						encoding, type, NULL, 0);
				else if (type == PYGRES_BYTEA)
					val = cast_bytea_text(s);
				else if (type == PYGRES_OTHER)
					val = cast_other(s,
						PQgetlength(self->result, i, j), encoding,
						PQftype(self->result, j), self->pgcnx->cast_hook);
				else if (type & PYGRES_TEXT)
					val = cast_sized_text(s, PQgetlength(self->result, i, j),
						encoding, type);
				else
					val = cast_unsized_simple(s, type);
			}

			if (!val)
			{
				Py_DECREF(reslist);
				Py_DECREF(rowtuple);
				reslist = NULL;
				goto exit;
			}

			PyTuple_SET_ITEM(rowtuple, j, val);
		}

		PyList_SET_ITEM(reslist, i, rowtuple);
	}

exit:
	PyMem_Free(col_types);

	/* returns list */
	return reslist;
}

/* retrieves last result as a list of dictionaries*/
static char queryDictResult__doc__[] =
"dictresult() -- Get the result of a query\n\n"
"The result is returned as a list of rows, each one a dictionary with\n"
"the field names used as the labels.\n";

static PyObject *
queryDictResult(queryObject *self, PyObject *noargs)
{
	PyObject   *reslist;
	int			i,
				m,
				n,
			   *col_types;
	int			encoding = self->encoding;

	/* stores result in list */
	m = PQntuples(self->result);
	n = PQnfields(self->result);
	if (!(reslist = PyList_New(m))) return NULL;

	if (!(col_types = get_col_types(self->result, n))) return NULL;

	for (i = 0; i < m; ++i)
	{
		PyObject   *dict;
		int			j;

		if (!(dict = PyDict_New()))
		{
			Py_DECREF(reslist);
			reslist = NULL;
			goto exit;
		}

		for (j = 0; j < n; ++j)
		{
			PyObject * val;

			if (PQgetisnull(self->result, i, j))
			{
				Py_INCREF(Py_None);
				val = Py_None;
			}
			else /* not null */
			{
				/* get the string representation of the value */
				/* note: this is always null-terminated text format */
				char   *s = PQgetvalue(self->result, i, j);
				/* get the PyGreSQL type of the column */
				int		type = col_types[j];

				if (type & PYGRES_ARRAY)
					val = cast_array(s, PQgetlength(self->result, i, j),
						encoding, type, NULL, 0);
				else if (type == PYGRES_BYTEA)
					val = cast_bytea_text(s);
				else if (type == PYGRES_OTHER)
					val = cast_other(s,
						PQgetlength(self->result, i, j), encoding,
						PQftype(self->result, j), self->pgcnx->cast_hook);
				else if (type & PYGRES_TEXT)
					val = cast_sized_text(s, PQgetlength(self->result, i, j),
						encoding, type);
				else
					val = cast_unsized_simple(s, type);
			}

			if (!val)
			{
				Py_DECREF(dict);
				Py_DECREF(reslist);
				reslist = NULL;
				goto exit;
			}

			PyDict_SetItemString(dict, PQfname(self->result, j), val);
			Py_DECREF(val);
		}

		PyList_SET_ITEM(reslist, i, dict);
	}

exit:
	PyMem_Free(col_types);

	/* returns list */
	return reslist;
}

/* retrieves last result as named tuples */
static char queryNamedResult__doc__[] =
"namedresult() -- Get the result of a query\n\n"
"The result is returned as a list of rows, each one a tuple of fields\n"
"in the order returned by the server.\n";

static PyObject *
queryNamedResult(queryObject *self, PyObject *noargs)
{
	PyObject   *ret;

	if (namedresult)
	{
		ret = PyObject_CallFunction(namedresult, "(O)", self);

		if (ret == NULL)
			return NULL;
		}
	else
	{
		ret = queryGetResult(self, NULL);
	}

	return ret;
}

/* gets notice object attributes */
static PyObject *
noticeGetAttr(noticeObject *self, PyObject *nameobj)
{
	PGresult const *res = self->res;
	const char *name = PyStr_AsString(nameobj);
	int fieldcode;

	if (!res)
	{
		PyErr_SetString(PyExc_TypeError, "Cannot get current notice");
		return NULL;
	}

	/* pg connection object */
	if (!strcmp(name, "pgcnx"))
	{
		if (self->pgcnx && check_cnx_obj(self->pgcnx))
		{
			Py_INCREF(self->pgcnx);
			return (PyObject *) self->pgcnx;
		}
		else
		{
			Py_INCREF(Py_None);
			return Py_None;
		}
	}

	/* full message */
	if (!strcmp(name, "message"))
		return PyStr_FromString(PQresultErrorMessage(res));

	/* other possible fields */
	fieldcode = 0;
	if (!strcmp(name, "severity"))
		fieldcode = PG_DIAG_SEVERITY;
	else if (!strcmp(name, "primary"))
		fieldcode = PG_DIAG_MESSAGE_PRIMARY;
	else if (!strcmp(name, "detail"))
		fieldcode = PG_DIAG_MESSAGE_DETAIL;
	else if (!strcmp(name, "hint"))
		fieldcode = PG_DIAG_MESSAGE_HINT;
	if (fieldcode)
	{
		char *s = PQresultErrorField(res, fieldcode);
		if (s)
			return PyStr_FromString(s);
		else
		{
			Py_INCREF(Py_None); return Py_None;
		}
	}

	return PyObject_GenericGetAttr((PyObject *) self, nameobj);
}

/* return notice as string in human readable form */
static PyObject *
noticeStr(noticeObject *self)
{
	return noticeGetAttr(self, PyBytes_FromString("message"));
}

/* get the list of notice attributes */
static PyObject *
noticeDir(noticeObject *self, PyObject *noargs)
{
	PyObject *attrs;

	attrs = PyObject_Dir(PyObject_Type((PyObject *)self));
	PyObject_CallMethod(attrs, "extend", "[ssssss]",
		"pgcnx", "severity", "message", "primary", "detail", "hint");

	return attrs;
}

/* notice object methods */
static struct PyMethodDef noticeMethods[] = {
	{"__dir__", (PyCFunction) noticeDir,  METH_NOARGS, NULL},
	{NULL, NULL}
};

/* notice type definition */
static PyTypeObject noticeType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"pg.Notice",					/* tp_name */
	sizeof(noticeObject),			/* tp_basicsize */
	0,								/* tp_itemsize */
	/* methods */
	0,								/* tp_dealloc */
	0,								/* tp_print */
	0,								/* tp_getattr */
	0,								/* tp_setattr */
	0,								/* tp_compare */
	0,								/* tp_repr */
	0,								/* tp_as_number */
	0,								/* tp_as_sequence */
	0,								/* tp_as_mapping */
	0,								/* tp_hash */
	0,								/* tp_call */
	(reprfunc) noticeStr,			/* tp_str */
	(getattrofunc) noticeGetAttr,	/* tp_getattro */
	PyObject_GenericSetAttr,		/* tp_setattro */
	0,								/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,				/* tp_flags */
	0,								/* tp_doc */
	0,								/* tp_traverse */
	0,								/* tp_clear */
	0,								/* tp_richcompare */
	0,								/* tp_weaklistoffset */
	0,								/* tp_iter */
	0,								/* tp_iternext */
	noticeMethods,					/* tp_methods */
};

/* query object methods */
static struct PyMethodDef queryMethods[] = {
	{"getresult", (PyCFunction) queryGetResult, METH_NOARGS,
			queryGetResult__doc__},
	{"dictresult", (PyCFunction) queryDictResult, METH_NOARGS,
			queryDictResult__doc__},
	{"namedresult", (PyCFunction) queryNamedResult, METH_NOARGS,
			queryNamedResult__doc__},
	{"fieldname", (PyCFunction) queryFieldName, METH_VARARGS,
			 queryFieldName__doc__},
	{"fieldnum", (PyCFunction) queryFieldNumber, METH_VARARGS,
			queryFieldNumber__doc__},
	{"listfields", (PyCFunction) queryListFields, METH_NOARGS,
			queryListFields__doc__},
	{"ntuples", (PyCFunction) queryNTuples, METH_NOARGS,
			queryNTuples__doc__},
	{NULL, NULL}
};

/* query type definition */
static PyTypeObject queryType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	"pg.Query",						/* tp_name */
	sizeof(queryObject),			/* tp_basicsize */
	0,								/* tp_itemsize */
	/* methods */
	(destructor) queryDealloc,		/* tp_dealloc */
	0,								/* tp_print */
	0,								/* tp_getattr */
	0,								/* tp_setattr */
	0,								/* tp_compare */
	0,								/* tp_repr */
	0,								/* tp_as_number */
	0,								/* tp_as_sequence */
	0,								/* tp_as_mapping */
	0,								/* tp_hash */
	0,								/* tp_call */
	(reprfunc) queryStr,			/* tp_str */
	PyObject_GenericGetAttr,		/* tp_getattro */
	0,								/* tp_setattro */
	0,								/* tp_as_buffer */
	Py_TPFLAGS_DEFAULT,				/* tp_flags */
	0,								/* tp_doc */
	0,								/* tp_traverse */
	0,								/* tp_clear */
	0,								/* tp_richcompare */
	0,								/* tp_weaklistoffset */
	0,								/* tp_iter */
	0,								/* tp_iternext */
	queryMethods,					/* tp_methods */
};

/* --------------------------------------------------------------------- */

/* MODULE FUNCTIONS */

/* escape string */
static char pgEscapeString__doc__[] =
"escape_string(string) -- escape a string for use within SQL";

static PyObject *
pgEscapeString(PyObject *self, PyObject *string)
{
	PyObject   *tmp_obj = NULL, /* auxiliary string object */
			   *to_obj; /* string object to return */
	char 	   *from, /* our string argument as encoded string */
			   *to; /* the result as encoded string */
	Py_ssize_t 	from_length; /* length of string */
	size_t		to_length; /* length of result */
	int			encoding = -1; /* client encoding */

	if (PyBytes_Check(string))
	{
		PyBytes_AsStringAndSize(string, &from, &from_length);
	}
	else if (PyUnicode_Check(string))
	{
		encoding = pg_encoding_ascii;
		tmp_obj = get_encoded_string(string, encoding);
		if (!tmp_obj) return NULL; /* pass the UnicodeEncodeError */
		PyBytes_AsStringAndSize(tmp_obj, &from, &from_length);
	}
	else
	{
		PyErr_SetString(PyExc_TypeError,
			"Method escape_string() expects a string as argument");
		return NULL;
	}

	to_length = 2*from_length + 1;
	if ((Py_ssize_t)to_length < from_length) /* overflow */
	{
		to_length = from_length;
		from_length = (from_length - 1)/2;
	}
	to = (char *)PyMem_Malloc(to_length);
	to_length = (int)PQescapeString(to, from, (size_t)from_length);

	Py_XDECREF(tmp_obj);

	if (encoding == -1)
		to_obj = PyBytes_FromStringAndSize(to, to_length);
	else
		to_obj = get_decoded_string(to, to_length, encoding);
	PyMem_Free(to);
	return to_obj;
}

/* escape bytea */
static char pgEscapeBytea__doc__[] =
"escape_bytea(data) -- escape binary data for use within SQL as type bytea";

static PyObject *
pgEscapeBytea(PyObject *self, PyObject *data)
{
	PyObject   *tmp_obj = NULL, /* auxiliary string object */
			   *to_obj; /* string object to return */
	char 	   *from, /* our string argument as encoded string */
			   *to; /* the result as encoded string */
	Py_ssize_t 	from_length; /* length of string */
	size_t		to_length; /* length of result */
	int			encoding = -1; /* client encoding */

	if (PyBytes_Check(data))
	{
		PyBytes_AsStringAndSize(data, &from, &from_length);
	}
	else if (PyUnicode_Check(data))
	{
		encoding = pg_encoding_ascii;
		tmp_obj = get_encoded_string(data, encoding);
		if (!tmp_obj) return NULL; /* pass the UnicodeEncodeError */
		PyBytes_AsStringAndSize(tmp_obj, &from, &from_length);
	}
	else
	{
		PyErr_SetString(PyExc_TypeError,
			"Method escape_bytea() expects a string as argument");
		return NULL;
	}

	to = (char *)PQescapeBytea(
		(unsigned char*)from, (size_t)from_length, &to_length);

	Py_XDECREF(tmp_obj);

	if (encoding == -1)
		to_obj = PyBytes_FromStringAndSize(to, to_length - 1);
	else
		to_obj = get_decoded_string(to, to_length - 1, encoding);
	if (to)
		PQfreemem(to);
	return to_obj;
}

/* unescape bytea */
static char pgUnescapeBytea__doc__[] =
"unescape_bytea(string) -- unescape bytea data retrieved as text";

static PyObject *
pgUnescapeBytea(PyObject *self, PyObject *data)
{
	PyObject   *tmp_obj = NULL, /* auxiliary string object */
			   *to_obj; /* string object to return */
	char 	   *from, /* our string argument as encoded string */
			   *to; /* the result as encoded string */
	Py_ssize_t 	from_length; /* length of string */
	size_t		to_length; /* length of result */

	if (PyBytes_Check(data))
	{
		PyBytes_AsStringAndSize(data, &from, &from_length);
	}
	else if (PyUnicode_Check(data))
	{
		tmp_obj = get_encoded_string(data, pg_encoding_ascii);
		if (!tmp_obj) return NULL; /* pass the UnicodeEncodeError */
		PyBytes_AsStringAndSize(tmp_obj, &from, &from_length);
	}
	else
	{
		PyErr_SetString(PyExc_TypeError,
			"Method unescape_bytea() expects a string as argument");
		return NULL;
	}

	to = (char *)PQunescapeBytea((unsigned char*)from, &to_length);

	Py_XDECREF(tmp_obj);

	if (!to) return PyErr_NoMemory();

	to_obj = PyBytes_FromStringAndSize(to, to_length);
	PQfreemem(to);

	return to_obj;
}

/* set fixed datestyle */
static char pgSetDatestyle__doc__[] =
"set_datestyle(style) -- set which style is assumed";

static PyObject *
pgSetDatestyle(PyObject *self, PyObject *args)
{
	const char	   *datestyle = NULL;

	/* gets arguments */
	if (!PyArg_ParseTuple(args, "z", &datestyle))
	{
		PyErr_SetString(PyExc_TypeError,
			"Function set_datestyle() expects a string or None as argument");
		return NULL;
	}

	date_format = datestyle ? date_style_to_format(datestyle) : NULL;

	Py_INCREF(Py_None); return Py_None;
}

/* get fixed datestyle */
static char pgGetDatestyle__doc__[] =
"get_datestyle() -- get which date style is assumed";

static PyObject *
pgGetDatestyle(PyObject *self, PyObject *noargs)
{
	if (date_format)
	{
		return PyStr_FromString(date_format_to_style(date_format));
	}
	else
	{
		Py_INCREF(Py_None); return Py_None;
	}
}

/* get decimal point */
static char pgGetDecimalPoint__doc__[] =
"get_decimal_point() -- get decimal point to be used for money values";

static PyObject *
pgGetDecimalPoint(PyObject *self, PyObject *noargs)
{
	PyObject *ret;
	char s[2];

	if (decimal_point)
	{
		s[0] = decimal_point; s[1] = '\0';
		ret = PyStr_FromString(s);
	}
	else
	{
		Py_INCREF(Py_None); ret = Py_None;
	}

	return ret;
}

/* set decimal point */
static char pgSetDecimalPoint__doc__[] =
"set_decimal_point(char) -- set decimal point to be used for money values";

static PyObject *
pgSetDecimalPoint(PyObject *self, PyObject *args)
{
	PyObject *ret = NULL;
	char *s = NULL;

	/* gets arguments */
	if (PyArg_ParseTuple(args, "z", &s))
	{
		if (!s)
			s = "\0";
		else if (*s && (*(s+1) || !strchr(".,;: '*/_`|", *s)))
			s = NULL;
	}

	if (s)
	{
		decimal_point = *s;
		Py_INCREF(Py_None); ret = Py_None;
	}
	else
		PyErr_SetString(PyExc_TypeError,
			"Function set_decimal_mark() expects"
			" a decimal mark character as argument");

	return ret;
}

/* get decimal type */
static char pgGetDecimal__doc__[] =
"get_decimal() -- get the decimal type to be used for numeric values";

static PyObject *
pgGetDecimal(PyObject *self, PyObject *noargs)
{
	PyObject *ret;

	ret = decimal ? decimal : Py_None;
	Py_INCREF(ret);

	return ret;
}

/* set decimal type */
static char pgSetDecimal__doc__[] =
"set_decimal(cls) -- set a decimal type to be used for numeric values";

static PyObject *
pgSetDecimal(PyObject *self, PyObject *cls)
{
	PyObject *ret = NULL;

	if (cls == Py_None)
	{
		Py_XDECREF(decimal); decimal = NULL;
		Py_INCREF(Py_None); ret = Py_None;
	}
	else if (PyCallable_Check(cls))
	{
		Py_XINCREF(cls); Py_XDECREF(decimal); decimal = cls;
		Py_INCREF(Py_None); ret = Py_None;
	}
	else
		PyErr_SetString(PyExc_TypeError,
			"Function set_decimal() expects"
			 " a callable or None as argument");

	return ret;
}

/* get usage of bool values */
static char pgGetBool__doc__[] =
"get_bool() -- check whether boolean values are converted to bool";

static PyObject *
pgGetBool(PyObject *self, PyObject *noargs)
{
	PyObject *ret;

	ret = bool_as_text ? Py_False : Py_True;
	Py_INCREF(ret);

	return ret;
}

/* set usage of bool values */
static char pgSetBool__doc__[] =
"set_bool(on) -- set whether boolean values should be converted to bool";

static PyObject *
pgSetBool(PyObject *self, PyObject *args)
{
	PyObject *ret = NULL;
	int			i;

	/* gets arguments */
	if (PyArg_ParseTuple(args, "i", &i))
	{
		bool_as_text = i ? 0 : 1;
		Py_INCREF(Py_None); ret = Py_None;
	}
	else
		PyErr_SetString(PyExc_TypeError,
			"Function set_bool() expects a boolean value as argument");

	return ret;
}

/* get conversion of arrays to lists */
static char pgGetArray__doc__[] =
"get_array() -- check whether arrays are converted as lists";

static PyObject *
pgGetArray(PyObject *self, PyObject *noargs)
{
	PyObject *ret;

	ret = array_as_text ? Py_False : Py_True;
	Py_INCREF(ret);

	return ret;
}

/* set conversion of arrays to lists */
static char pgSetArray__doc__[] =
"set_array(on) -- set whether arrays should be converted to lists";

static PyObject *
pgSetArray(PyObject *self, PyObject *args)
{
	PyObject *ret = NULL;
	int			i;

	/* gets arguments */
	if (PyArg_ParseTuple(args, "i", &i))
	{
		array_as_text = i ? 0 : 1;
		Py_INCREF(Py_None); ret = Py_None;
	}
	else
		PyErr_SetString(PyExc_TypeError,
			"Function set_array() expects a boolean value as argument");

	return ret;
}

/* check whether bytea values are unescaped */
static char pgGetByteaEscaped__doc__[] =
"get_bytea_escaped() -- check whether bytea will be returned escaped";

static PyObject *
pgGetByteaEscaped(PyObject *self, PyObject *noargs)
{
	PyObject *ret;

	ret = bytea_escaped ? Py_True : Py_False;
	Py_INCREF(ret);

	return ret;
}

/* set usage of bool values */
static char pgSetByteaEscaped__doc__[] =
"set_bytea_escaped(on) -- set whether bytea will be returned escaped";

static PyObject *
pgSetByteaEscaped(PyObject *self, PyObject *args)
{
	PyObject *ret = NULL;
	int			i;

	/* gets arguments */
	if (PyArg_ParseTuple(args, "i", &i))
	{
		bytea_escaped = i ? 1 : 0;
		Py_INCREF(Py_None); ret = Py_None;
	}
	else
		PyErr_SetString(PyExc_TypeError,
			"Function set_bytea_escaped() expects a boolean value as argument");

	return ret;
}

/* get named result factory */
static char pgGetNamedresult__doc__[] =
"get_namedresult() -- get the function used for getting named results";

static PyObject *
pgGetNamedresult(PyObject *self, PyObject *noargs)
{
	PyObject *ret;

	ret = namedresult ? namedresult : Py_None;
	Py_INCREF(ret);

	return ret;
}

/* set named result factory */
static char pgSetNamedresult__doc__[] =
"set_namedresult(func) -- set a function to be used for getting named results";

static PyObject *
pgSetNamedresult(PyObject *self, PyObject *func)
{
	PyObject *ret = NULL;

	if (func == Py_None)
	{
		Py_XDECREF(namedresult); namedresult = NULL;
		Py_INCREF(Py_None); ret = Py_None;
	}
	else if (PyCallable_Check(func))
	{
		Py_XINCREF(func); Py_XDECREF(namedresult); namedresult = func;
		Py_INCREF(Py_None); ret = Py_None;
	}
	else
		PyErr_SetString(PyExc_TypeError,
			"Function set_namedresult() expects"
			 " a callable or None as argument");

	return ret;
}

/* get json decode function */
static char pgGetJsondecode__doc__[] =
"get_jsondecode() -- get the function used for decoding json results";

static PyObject *
pgGetJsondecode(PyObject *self, PyObject *noargs)
{
	PyObject *ret;

	ret = jsondecode;
	if (!ret)
		ret = Py_None;
	Py_INCREF(ret);

	return ret;
}

/* set json decode function */
static char pgSetJsondecode__doc__[] =
"set_jsondecode(func) -- set a function to be used for decoding json results";

static PyObject *
pgSetJsondecode(PyObject *self, PyObject *func)
{
	PyObject *ret = NULL;

	if (func == Py_None)
	{
		Py_XDECREF(jsondecode); jsondecode = NULL;
		Py_INCREF(Py_None); ret = Py_None;
	}
	else if (PyCallable_Check(func))
	{
		Py_XINCREF(func); Py_XDECREF(jsondecode); jsondecode = func;
		Py_INCREF(Py_None); ret = Py_None;
	}
	else
		PyErr_SetString(PyExc_TypeError,
			"Function jsondecode() expects"
			 " a callable or None as argument");

	return ret;
}

#ifdef DEFAULT_VARS

/* gets default host */
static char pgGetDefHost__doc__[] =
"get_defhost() -- return default database host";

static PyObject *
pgGetDefHost(PyObject *self, PyObject *noargs)
{
	Py_XINCREF(pg_default_host);
	return pg_default_host;
}

/* sets default host */
static char pgSetDefHost__doc__[] =
"set_defhost(string) -- set default database host and return previous value";

static PyObject *
pgSetDefHost(PyObject *self, PyObject *args)
{
	char	   *temp = NULL;
	PyObject   *old;

	/* gets arguments */
	if (!PyArg_ParseTuple(args, "z", &temp))
	{
		PyErr_SetString(PyExc_TypeError,
			"Function set_defhost() expects a string or None as argument");
		return NULL;
	}

	/* adjusts value */
	old = pg_default_host;

	if (temp)
		pg_default_host = PyStr_FromString(temp);
	else
	{
		Py_INCREF(Py_None);
		pg_default_host = Py_None;
	}

	return old;
}

/* gets default base */
static char pgGetDefBase__doc__[] =
"get_defbase() -- return default database name";

static PyObject *
pgGetDefBase(PyObject *self, PyObject *noargs)
{
	Py_XINCREF(pg_default_base);
	return pg_default_base;
}

/* sets default base */
static char pgSetDefBase__doc__[] =
"set_defbase(string) -- set default database name and return previous value";

static PyObject *
pgSetDefBase(PyObject *self, PyObject *args)
{
	char	   *temp = NULL;
	PyObject   *old;

	/* gets arguments */
	if (!PyArg_ParseTuple(args, "z", &temp))
	{
		PyErr_SetString(PyExc_TypeError,
			"Function set_defbase() Argument a string or None as argument");
		return NULL;
	}

	/* adjusts value */
	old = pg_default_base;

	if (temp)
		pg_default_base = PyStr_FromString(temp);
	else
	{
		Py_INCREF(Py_None);
		pg_default_base = Py_None;
	}

	return old;
}

/* gets default options */
static char pgGetDefOpt__doc__[] =
"get_defopt() -- return default database options";

static PyObject *
pgGetDefOpt(PyObject *self, PyObject *noargs)
{
	Py_XINCREF(pg_default_opt);
	return pg_default_opt;
}

/* sets default opt */
static char pgSetDefOpt__doc__[] =
"set_defopt(string) -- set default options and return previous value";

static PyObject *
pgSetDefOpt(PyObject *self, PyObject *args)
{
	char	   *temp = NULL;
	PyObject   *old;

	/* gets arguments */
	if (!PyArg_ParseTuple(args, "z", &temp))
	{
		PyErr_SetString(PyExc_TypeError,
			"Function set_defopt() expects a string or None as argument");
		return NULL;
	}

	/* adjusts value */
	old = pg_default_opt;

	if (temp)
		pg_default_opt = PyStr_FromString(temp);
	else
	{
		Py_INCREF(Py_None);
		pg_default_opt = Py_None;
	}

	return old;
}

/* gets default username */
static char pgGetDefUser__doc__[] =
"get_defuser() -- return default database username";

static PyObject *
pgGetDefUser(PyObject *self, PyObject *noargs)
{
	Py_XINCREF(pg_default_user);
	return pg_default_user;
}

/* sets default username */

static char pgSetDefUser__doc__[] =
"set_defuser(name) -- set default username and return previous value";

static PyObject *
pgSetDefUser(PyObject *self, PyObject *args)
{
	char	   *temp = NULL;
	PyObject   *old;

	/* gets arguments */
	if (!PyArg_ParseTuple(args, "z", &temp))
	{
		PyErr_SetString(PyExc_TypeError,
			"Function set_defuser() expects a string or None as argument");
		return NULL;
	}

	/* adjusts value */
	old = pg_default_user;

	if (temp)
		pg_default_user = PyStr_FromString(temp);
	else
	{
		Py_INCREF(Py_None);
		pg_default_user = Py_None;
	}

	return old;
}

/* sets default password */
static char pgSetDefPassword__doc__[] =
"set_defpasswd(password) -- set default database password";

static PyObject *
pgSetDefPassword(PyObject *self, PyObject *args)
{
	char	   *temp = NULL;

	/* gets arguments */
	if (!PyArg_ParseTuple(args, "z", &temp))
	{
		PyErr_SetString(PyExc_TypeError,
			"Function set_defpasswd() expects a string or None as argument");
		return NULL;
	}

	if (temp)
		pg_default_passwd = PyStr_FromString(temp);
	else
	{
		Py_INCREF(Py_None);
		pg_default_passwd = Py_None;
	}

	Py_INCREF(Py_None);
	return Py_None;
}

/* gets default port */
static char pgGetDefPort__doc__[] =
"get_defport() -- return default database port";

static PyObject *
pgGetDefPort(PyObject *self, PyObject *noargs)
{
	Py_XINCREF(pg_default_port);
	return pg_default_port;
}

/* sets default port */
static char pgSetDefPort__doc__[] =
"set_defport(port) -- set default port and return previous value";

static PyObject *
pgSetDefPort(PyObject *self, PyObject *args)
{
	long int	port = -2;
	PyObject   *old;

	/* gets arguments */
	if ((!PyArg_ParseTuple(args, "l", &port)) || (port < -1))
	{
		PyErr_SetString(PyExc_TypeError,
			"Function set_deport expects"
			 " a positive integer or -1 as argument");
		return NULL;
	}

	/* adjusts value */
	old = pg_default_port;

	if (port != -1)
		pg_default_port = PyInt_FromLong(port);
	else
	{
		Py_INCREF(Py_None);
		pg_default_port = Py_None;
	}

	return old;
}
#endif /* DEFAULT_VARS */

/* cast a string with a text representation of an array to a list */
static char pgCastArray__doc__[] =
"cast_array(string, cast=None, delim=',') -- cast a string as an array";

PyObject *
pgCastArray(PyObject *self, PyObject *args, PyObject *dict)
{
	static const char *kwlist[] = {"string", "cast", "delim", NULL};
	PyObject   *string_obj, *cast_obj = NULL, *ret;
	char  	   *string, delim = ',';
	Py_ssize_t	size;
	int			encoding;

	if (!PyArg_ParseTupleAndKeywords(args, dict, "O|Oc",
			(char **) kwlist, &string_obj, &cast_obj, &delim))
		return NULL;

	if (PyBytes_Check(string_obj))
	{
		PyBytes_AsStringAndSize(string_obj, &string, &size);
		string_obj = NULL;
		encoding = pg_encoding_ascii;
	}
	else if (PyUnicode_Check(string_obj))
	{
		string_obj = PyUnicode_AsUTF8String(string_obj);
		if (!string_obj) return NULL; /* pass the UnicodeEncodeError */
		PyBytes_AsStringAndSize(string_obj, &string, &size);
		encoding = pg_encoding_utf8;
	}
	else
	{
		PyErr_SetString(PyExc_TypeError,
			"Function cast_array() expects a string as first argument");
		return NULL;
	}

	if (!cast_obj || cast_obj == Py_None)
	{
		if (cast_obj)
		{
			Py_DECREF(cast_obj); cast_obj = NULL;
		}
	}
	else if (!PyCallable_Check(cast_obj))
	{
		PyErr_SetString(PyExc_TypeError,
			"Function cast_array() expects a callable as second argument");
		return NULL;
	}

	ret = cast_array(string, size, encoding, 0, cast_obj, delim);

	Py_XDECREF(string_obj);

	return ret;
}

/* cast a string with a text representation of a record to a tuple */
static char pgCastRecord__doc__[] =
"cast_record(string, cast=None, delim=',') -- cast a string as a record";

PyObject *
pgCastRecord(PyObject *self, PyObject *args, PyObject *dict)
{
	static const char *kwlist[] = {"string", "cast", "delim", NULL};
	PyObject   *string_obj, *cast_obj = NULL, *ret;
	char  	   *string, delim = ',';
	Py_ssize_t	size, len;
	int			encoding;

	if (!PyArg_ParseTupleAndKeywords(args, dict, "O|Oc",
			(char **) kwlist, &string_obj, &cast_obj, &delim))
		return NULL;

	if (PyBytes_Check(string_obj))
	{
		PyBytes_AsStringAndSize(string_obj, &string, &size);
		string_obj = NULL;
		encoding = pg_encoding_ascii;
	}
	else if (PyUnicode_Check(string_obj))
	{
		string_obj = PyUnicode_AsUTF8String(string_obj);
		if (!string_obj) return NULL; /* pass the UnicodeEncodeError */
		PyBytes_AsStringAndSize(string_obj, &string, &size);
		encoding = pg_encoding_utf8;
	}
	else
	{
		PyErr_SetString(PyExc_TypeError,
			"Function cast_record() expects a string as first argument");
		return NULL;
	}

	if (!cast_obj || PyCallable_Check(cast_obj))
	{
		len = 0;
	}
	else if (cast_obj == Py_None)
	{
		Py_DECREF(cast_obj); cast_obj = NULL; len = 0;
	}
	else if (PyTuple_Check(cast_obj) || PyList_Check(cast_obj))
	{
		len = PySequence_Size(cast_obj);
		if (!len)
		{
			Py_DECREF(cast_obj); cast_obj = NULL;
		}
	}
	else
	{
		PyErr_SetString(PyExc_TypeError,
			"Function cast_record() expects a callable"
			 " or tuple or list of callables as second argument");
		return NULL;
	}

	ret = cast_record(string, size, encoding, 0, cast_obj, len, delim);

	Py_XDECREF(string_obj);

	return ret;
}

/* cast a string with a text representation of an hstore to a dict */
static char pgCastHStore__doc__[] =
"cast_hstore(string) -- cast a string as an hstore";

PyObject *
pgCastHStore(PyObject *self, PyObject *string)
{
	PyObject   *tmp_obj = NULL, *ret;
	char  	   *s;
	Py_ssize_t	size;
	int			encoding;

	if (PyBytes_Check(string))
	{
		PyBytes_AsStringAndSize(string, &s, &size);
		encoding = pg_encoding_ascii;
	}
	else if (PyUnicode_Check(string))
	{
		tmp_obj = PyUnicode_AsUTF8String(string);
		if (!tmp_obj) return NULL; /* pass the UnicodeEncodeError */
		PyBytes_AsStringAndSize(tmp_obj, &s, &size);
		encoding = pg_encoding_utf8;
	}
	else
	{
		PyErr_SetString(PyExc_TypeError,
			"Function cast_hstore() expects a string as first argument");
		return NULL;
	}

	ret = cast_hstore(s, size, encoding);

	Py_XDECREF(tmp_obj);

	return ret;
}

/* List of functions defined in the module */

static struct PyMethodDef pgMethods[] = {
	{"connect", (PyCFunction) pgConnect, METH_VARARGS|METH_KEYWORDS,
			pgConnect__doc__},
	{"escape_string", (PyCFunction) pgEscapeString, METH_O,
			pgEscapeString__doc__},
	{"escape_bytea", (PyCFunction) pgEscapeBytea, METH_O,
			pgEscapeBytea__doc__},
	{"unescape_bytea", (PyCFunction) pgUnescapeBytea, METH_O,
			pgUnescapeBytea__doc__},
	{"get_datestyle", (PyCFunction) pgGetDatestyle, METH_NOARGS,
			pgGetDatestyle__doc__},
	{"set_datestyle", (PyCFunction) pgSetDatestyle, METH_VARARGS,
			pgSetDatestyle__doc__},
	{"get_decimal_point", (PyCFunction) pgGetDecimalPoint, METH_NOARGS,
			pgGetDecimalPoint__doc__},
	{"set_decimal_point", (PyCFunction) pgSetDecimalPoint, METH_VARARGS,
			pgSetDecimalPoint__doc__},
	{"get_decimal", (PyCFunction) pgGetDecimal, METH_NOARGS,
			pgGetDecimal__doc__},
	{"set_decimal", (PyCFunction) pgSetDecimal, METH_O,
			pgSetDecimal__doc__},
	{"get_bool", (PyCFunction) pgGetBool, METH_NOARGS, pgGetBool__doc__},
	{"set_bool", (PyCFunction) pgSetBool, METH_VARARGS, pgSetBool__doc__},
	{"get_array", (PyCFunction) pgGetArray, METH_NOARGS, pgGetArray__doc__},
	{"set_array", (PyCFunction) pgSetArray, METH_VARARGS, pgSetArray__doc__},
	{"get_bytea_escaped", (PyCFunction) pgGetByteaEscaped, METH_NOARGS,
		pgGetByteaEscaped__doc__},
	{"set_bytea_escaped", (PyCFunction) pgSetByteaEscaped, METH_VARARGS,
		pgSetByteaEscaped__doc__},
	{"get_namedresult", (PyCFunction) pgGetNamedresult, METH_NOARGS,
			pgGetNamedresult__doc__},
	{"set_namedresult", (PyCFunction) pgSetNamedresult, METH_O,
			pgSetNamedresult__doc__},
	{"get_jsondecode", (PyCFunction) pgGetJsondecode, METH_NOARGS,
			pgGetJsondecode__doc__},
	{"set_jsondecode", (PyCFunction) pgSetJsondecode, METH_O,
			pgSetJsondecode__doc__},
	{"cast_array", (PyCFunction) pgCastArray, METH_VARARGS|METH_KEYWORDS,
			pgCastArray__doc__},
	{"cast_record", (PyCFunction) pgCastRecord, METH_VARARGS|METH_KEYWORDS,
			pgCastRecord__doc__},
	{"cast_hstore", (PyCFunction) pgCastHStore, METH_O, pgCastHStore__doc__},

#ifdef DEFAULT_VARS
	{"get_defhost", pgGetDefHost, METH_NOARGS, pgGetDefHost__doc__},
	{"set_defhost", pgSetDefHost, METH_VARARGS, pgSetDefHost__doc__},
	{"get_defbase", pgGetDefBase, METH_NOARGS, pgGetDefBase__doc__},
	{"set_defbase", pgSetDefBase, METH_VARARGS, pgSetDefBase__doc__},
	{"get_defopt", pgGetDefOpt, METH_NOARGS, pgGetDefOpt__doc__},
	{"set_defopt", pgSetDefOpt, METH_VARARGS, pgSetDefOpt__doc__},
	{"get_defport", pgGetDefPort, METH_NOARGS, pgGetDefPort__doc__},
	{"set_defport", pgSetDefPort, METH_VARARGS, pgSetDefPort__doc__},
	{"get_defuser", pgGetDefUser, METH_NOARGS, pgGetDefUser__doc__},
	{"set_defuser", pgSetDefUser, METH_VARARGS, pgSetDefUser__doc__},
	{"set_defpasswd", pgSetDefPassword, METH_VARARGS, pgSetDefPassword__doc__},
#endif /* DEFAULT_VARS */
	{NULL, NULL} /* sentinel */
};

static char pg__doc__[] = "Python interface to PostgreSQL DB";

static struct PyModuleDef moduleDef = {
	PyModuleDef_HEAD_INIT,
	"_pg",		/* m_name */
	pg__doc__,	/* m_doc */
	-1,			/* m_size */
	pgMethods	/* m_methods */
};

/* Initialization function for the module */
MODULE_INIT_FUNC(_pg)
{
	PyObject   *mod, *dict, *s;

	/* Create the module and add the functions */

	mod = PyModule_Create(&moduleDef);

	/* Initialize here because some Windows platforms get confused otherwise */
#if IS_PY3
	connType.tp_base = noticeType.tp_base =
		queryType.tp_base = sourceType.tp_base = &PyBaseObject_Type;
#ifdef LARGE_OBJECTS
	largeType.tp_base = &PyBaseObject_Type;
#endif
#else
	connType.ob_type = noticeType.ob_type =
		queryType.ob_type = sourceType.ob_type = &PyType_Type;
#ifdef LARGE_OBJECTS
	largeType.ob_type = &PyType_Type;
#endif
#endif

	if (PyType_Ready(&connType)
		|| PyType_Ready(&noticeType)
		|| PyType_Ready(&queryType)
		|| PyType_Ready(&sourceType)
#ifdef LARGE_OBJECTS
		|| PyType_Ready(&largeType)
#endif
		) return NULL;

	dict = PyModule_GetDict(mod);

	/* Exceptions as defined by DB-API 2.0 */
	Error = PyErr_NewException("pg.Error", PyExc_Exception, NULL);
	PyDict_SetItemString(dict, "Error", Error);

	Warning = PyErr_NewException("pg.Warning", PyExc_Exception, NULL);
	PyDict_SetItemString(dict, "Warning", Warning);

	InterfaceError = PyErr_NewException("pg.InterfaceError", Error, NULL);
	PyDict_SetItemString(dict, "InterfaceError", InterfaceError);

	DatabaseError = PyErr_NewException("pg.DatabaseError", Error, NULL);
	PyDict_SetItemString(dict, "DatabaseError", DatabaseError);

	InternalError = PyErr_NewException("pg.InternalError", DatabaseError, NULL);
	PyDict_SetItemString(dict, "InternalError", InternalError);

	OperationalError =
		PyErr_NewException("pg.OperationalError", DatabaseError, NULL);
	PyDict_SetItemString(dict, "OperationalError", OperationalError);

	ProgrammingError =
		PyErr_NewException("pg.ProgrammingError", DatabaseError, NULL);
	PyDict_SetItemString(dict, "ProgrammingError", ProgrammingError);

	IntegrityError =
		PyErr_NewException("pg.IntegrityError", DatabaseError, NULL);
	PyDict_SetItemString(dict, "IntegrityError", IntegrityError);

	DataError = PyErr_NewException("pg.DataError", DatabaseError, NULL);
	PyDict_SetItemString(dict, "DataError", DataError);

	NotSupportedError =
		PyErr_NewException("pg.NotSupportedError", DatabaseError, NULL);
	PyDict_SetItemString(dict, "NotSupportedError", NotSupportedError);

	/* Make the version available */
	s = PyStr_FromString(PyPgVersion);
	PyDict_SetItemString(dict, "version", s);
	PyDict_SetItemString(dict, "__version__", s);
	Py_DECREF(s);

	/* results type for queries */
	PyDict_SetItemString(dict, "RESULT_EMPTY", PyInt_FromLong(RESULT_EMPTY));
	PyDict_SetItemString(dict, "RESULT_DML", PyInt_FromLong(RESULT_DML));
	PyDict_SetItemString(dict, "RESULT_DDL", PyInt_FromLong(RESULT_DDL));
	PyDict_SetItemString(dict, "RESULT_DQL", PyInt_FromLong(RESULT_DQL));

	/* transaction states */
	PyDict_SetItemString(dict,"TRANS_IDLE",PyInt_FromLong(PQTRANS_IDLE));
	PyDict_SetItemString(dict,"TRANS_ACTIVE",PyInt_FromLong(PQTRANS_ACTIVE));
	PyDict_SetItemString(dict,"TRANS_INTRANS",PyInt_FromLong(PQTRANS_INTRANS));
	PyDict_SetItemString(dict,"TRANS_INERROR",PyInt_FromLong(PQTRANS_INERROR));
	PyDict_SetItemString(dict,"TRANS_UNKNOWN",PyInt_FromLong(PQTRANS_UNKNOWN));

#ifdef LARGE_OBJECTS
	/* create mode for large objects */
	PyDict_SetItemString(dict, "INV_READ", PyInt_FromLong(INV_READ));
	PyDict_SetItemString(dict, "INV_WRITE", PyInt_FromLong(INV_WRITE));

	/* position flags for lo_lseek */
	PyDict_SetItemString(dict, "SEEK_SET", PyInt_FromLong(SEEK_SET));
	PyDict_SetItemString(dict, "SEEK_CUR", PyInt_FromLong(SEEK_CUR));
	PyDict_SetItemString(dict, "SEEK_END", PyInt_FromLong(SEEK_END));
#endif /* LARGE_OBJECTS */

#ifdef DEFAULT_VARS
	/* prepares default values */
	Py_INCREF(Py_None);
	pg_default_host = Py_None;
	Py_INCREF(Py_None);
	pg_default_base = Py_None;
	Py_INCREF(Py_None);
	pg_default_opt = Py_None;
	Py_INCREF(Py_None);
	pg_default_port = Py_None;
	Py_INCREF(Py_None);
	pg_default_user = Py_None;
	Py_INCREF(Py_None);
	pg_default_passwd = Py_None;
#endif /* DEFAULT_VARS */

	/* store common pg encoding ids */

	pg_encoding_utf8 = pg_char_to_encoding("UTF8");
	pg_encoding_latin1 = pg_char_to_encoding("LATIN1");
	pg_encoding_ascii = pg_char_to_encoding("SQL_ASCII");

	/* Check for errors */
	if (PyErr_Occurred())
		return NULL;

	return mod;
}
