Module functions and constants
==============================

.. py:currentmodule:: pg

The :mod:`pg` module defines a few functions that allow to connect
to a database and to define "default variables" that override
the environment variables used by PostgreSQL.

These "default variables" were designed to allow you to handle general
connection parameters without heavy code in your programs. You can prompt the
user for a value, put it in the default variable, and forget it, without
having to modify your environment. The support for default variables can be
disabled by setting the ``-DNO_DEF_VAR`` option in the Python setup file.
Methods relative to this are specified by the tag [DV].

All variables are set to ``None`` at module initialization, specifying that
standard environment variables should be used.

connect -- Open a PostgreSQL connection
---------------------------------------

.. function:: connect([dbname], [host], [port], [opt], [user], [passwd])

    Open a :mod:`pg` connection

    :param dbname: name of connected database (*None* = :data:`defbase`)
    :type str: str or None
    :param host: name of the server host (*None* = :data:`defhost`)
    :type host:  str or None
    :param port: port used by the database server (-1 = :data:`defport`)
    :type port: int
    :param opt: connection options (*None* = :data:`defopt`)
    :type opt: str or None
    :param user: PostgreSQL user (*None* = :data:`defuser`)
    :type user: str or None
    :param passwd: password for user (*None* = :data:`defpasswd`)
    :type passwd: str or None
    :returns: If successful, the :class:`Connection` handling the connection
    :rtype: :class:`Connection`
    :raises TypeError: bad argument type, or too many arguments
    :raises SyntaxError: duplicate argument definition
    :raises pg.InternalError: some error occurred during pg connection definition
    :raises Exception: (all exceptions relative to object allocation)

This function opens a connection to a specified database on a given
PostgreSQL server. You can use keywords here, as described in the
Python tutorial. The names of the keywords are the name of the
parameters given in the syntax line. For a precise description
of the parameters, please refer to the PostgreSQL user manual.

Example::

    import pg

    con1 = pg.connect('testdb', 'myhost', 5432, None, None, 'bob', None)
    con2 = pg.connect(dbname='testdb', host='localhost', user='bob')

get/set_defhost -- default server host [DV]
-------------------------------------------

.. function:: get_defhost(host)

    Get the default host

    :returns: the current default host specification
    :rtype: str or None
    :raises TypeError: too many arguments

This method returns the current default host specification,
or ``None`` if the environment variables should be used.
Environment variables won't be looked up.

.. function:: set_defhost(host)

    Set the default host

    :param host: the new default host specification
    :type host: str or None
    :returns: the previous default host specification
    :rtype: str or None
    :raises TypeError: bad argument type, or too many arguments

This methods sets the default host value for new connections.
If ``None`` is supplied as parameter, environment variables will
be used in future connections. It returns the previous setting
for default host.

get/set_defport -- default server port [DV]
-------------------------------------------

.. function:: get_defport()

    Get the default port

    :returns: the current default port specification
    :rtype: int
    :raises TypeError: too many arguments

This method returns the current default port specification,
or ``None`` if the environment variables should be used.
Environment variables won't be looked up.

.. function::  set_defport(port)

    Set the default port

    :param port: the new default port
    :type port: int
    :returns: previous default port specification
    :rtype: int or None

This methods sets the default port value for new connections. If -1 is
supplied as parameter, environment variables will be used in future
connections. It returns the previous setting for default port.

get/set_defopt --  default connection options [DV]
--------------------------------------------------

.. function:: get_defopt()

    Get the default connection options

    :returns: the current default options specification
    :rtype: str or None
    :raises TypeError: too many arguments

This method returns the current default connection options specification,
or ``None`` if the environment variables should be used. Environment variables
won't be looked up.

.. function:: set_defopt(options)

    Set the default connection options

    :param options: the new default connection options
    :type options: str or None
    :returns: previous default options specification
    :rtype: str or None
    :raises TypeError: bad argument type, or too many arguments

This methods sets the default connection options value for new connections.
If ``None`` is supplied as parameter, environment variables will be used in
future connections. It returns the previous setting for default options.

get/set_defbase -- default database name [DV]
---------------------------------------------

.. function:: get_defbase()

    Get the default database name

    :returns: the current default database name specification
    :rtype: str or None
    :raises TypeError: too many arguments

This method returns the current default database name specification, or
``None`` if the environment variables should be used. Environment variables
won't be looked up.

.. function:: set_defbase(base)

    Set the default database name

    :param base: the new default base name
    :type base: str or None
    :returns: the previous default database name specification
    :rtype: str or None
    :raises TypeError: bad argument type, or too many arguments

This method sets the default database name value for new connections. If
``None`` is supplied as parameter, environment variables will be used in
future connections. It returns the previous setting for default host.

get/set_defuser -- default database user [DV]
---------------------------------------------

.. function:: get_defuser()

    Get the default database user

    :returns: the current default database user specification
    :rtype: str or None
    :raises TypeError: too many arguments

This method returns the current default database user specification, or
``None`` if the environment variables should be used. Environment variables
won't be looked up.

.. function:: set_defuser(user)

    Set the default database user

    :param user: the new default database user
    :type base: str or None
    :returns: the previous default database user specification
    :rtype: str or None
    :raises TypeError: bad argument type, or too many arguments

This method sets the default database user name for new connections. If
``None`` is supplied as parameter, environment variables will be used in
future connections. It returns the previous setting for default host.

get/set_defpasswd -- default database password [DV]
---------------------------------------------------

.. function:: get_defpasswd()

    Get the default database password

    :returns: the current default database password specification
    :rtype: str or None
    :raises TypeError: too many arguments

This method returns the current default database password specification, or
``None`` if the environment variables should be used. Environment variables
won't be looked up.

.. function:: set_defpasswd(passwd)

    Set the default database password

    :param passwd: the new default database password
    :type base: str or None
    :returns: the previous default database password specification
    :rtype: str or None
    :raises TypeError: bad argument type, or too many arguments

This method sets the default database password for new connections. If
``None`` is supplied as parameter, environment variables will be used in
future connections. It returns the previous setting for default host.

escape_string -- escape a string for use within SQL
---------------------------------------------------

.. function:: escape_string(string)

    Escape a string for use within SQL

    :param str string: the string that is to be escaped
    :returns: the escaped string
    :rtype: str
    :raises TypeError: bad argument type, or too many arguments

This function escapes a string for use within an SQL command.
This is useful when inserting data values as literal constants
in SQL commands. Certain characters (such as quotes and backslashes)
must be escaped to prevent them from being interpreted specially
by the SQL parser. :func:`escape_string` performs this operation.
Note that there is also a :class:`Connection` method with the same name
which takes connection properties into account.

.. note::

    It is especially important to do proper escaping when
    handling strings that were received from an untrustworthy source.
    Otherwise there is a security risk: you are vulnerable to "SQL injection"
    attacks wherein unwanted SQL commands are fed to your database.

Example::

    name = input("Name? ")
    phone = con.query("select phone from employees where name='%s'"
        % escape_string(name)).getresult()

escape_bytea -- escape binary data for use within SQL
-----------------------------------------------------

.. function:: escape_bytea(datastring)

    escape binary data for use within SQL as type ``bytea``

    :param str datastring: string containing the binary data that is to be escaped
    :returns: the escaped string
    :rtype: str
    :raises TypeError: bad argument type, or too many arguments

Escapes binary data for use within an SQL command with the type ``bytea``.
As with :func:`escape_string`, this is only used when inserting data directly
into an SQL command string.

Note that there is also a :class:`Connection` method with the same name
which takes connection properties into account.

Example::

    picture = open('garfield.gif', 'rb').read()
    con.query("update pictures set img='%s' where name='Garfield'"
        % escape_bytea(picture))

unescape_bytea -- unescape data that has been retrieved as text
---------------------------------------------------------------

.. function:: unescape_bytea(string)

    Unescape ``bytea`` data that has been retrieved as text

    :param str datastring: the ``bytea`` data string that has been retrieved as text
    :returns: byte string containing the binary data
    :rtype: bytes
    :raises TypeError: bad argument type, or too many arguments

Converts an escaped string representation of binary data stored as ``bytea``
into the raw byte string representing the binary data  -- this is the reverse
of :func:`escape_bytea`.  Since the :class:`Query` results will already
return unescaped byte strings, you normally don't have to use this method.

Note that there is also a :class:`DB` method with the same name
which does exactly the same.

get/set_namedresult -- conversion to named tuples
-------------------------------------------------

.. function:: get_namedresult()

    Get the function that converts to named tuples

This returns the function used by PyGreSQL to construct the result of the
:meth:`Query.namedresult` method.

.. versionadded:: 4.1

.. function:: set_namedresult(func)

    Set a function that will convert to named tuples

    :param func: the function to be used to convert results to named tuples

You can use this if you want to create different kinds of named tuples
returned by the :meth:`Query.namedresult` method.  If you set this function
to *None*, then it will become equal to :meth:`Query.getresult`.

.. versionadded:: 4.1

get/set_decimal -- decimal type to be used for numeric values
-------------------------------------------------------------

.. function:: get_decimal()

    Get the decimal type to be used for numeric values

    :returns: the Python class used for PostgreSQL numeric values
    :rtype: class

This function returns the Python class that is used by PyGreSQL to hold
PostgreSQL numeric values. The default class is :class:`decimal.Decimal`
if available, otherwise the :class:`float` type is used.

.. function:: set_decimal(cls)

    Set a decimal type to be used for numeric values

    :param class cls: the Python class to be used for PostgreSQL numeric values

This function can be used to specify the Python class that shall
be used by PyGreSQL to hold PostgreSQL numeric values.
The default class is :class:`decimal.Decimal` if available,
otherwise the :class:`float` type is used.

get/set_decimal_point -- decimal mark used for monetary values
--------------------------------------------------------------

.. function:: get_decimal_point()

    Get the decimal mark used for monetary values

    :returns: string with one character representing the decimal mark
    :rtype: str

This function returns the decimal mark used by PyGreSQL to interpret
PostgreSQL monetary values when converting them to decimal numbers.
The default setting is ``'.'`` as a decimal point. This setting is not
adapted automatically to the locale used by PostGreSQL, but you can use
:func:`set_decimal()` to set a different decimal mark manually.  A return
value of ``None`` means monetary values are not interpreted as decimal
numbers, but returned as strings including the formatting and currency.

.. versionadded:: 4.1.1

.. function:: set_decimal_point(string)

    Specify which decimal mark is used for interpreting monetary values

    :param str string: string with one character representing the decimal mark

This function can be used to specify the decimal mark used by PyGreSQL
to interpret PostgreSQL monetary values. The default value is '.' as
a decimal point. This value is not adapted automatically to the locale
used by PostGreSQL, so if you are dealing with a database set to a
locale that uses a ``','`` instead of ``'.'`` as the decimal point,
then you need to call ``set_decimal(',')`` to have PyGreSQL interpret
monetary values correctly. If you don't want money values to be converted
to decimal numbers, then you can call ``set_decimal(None)``, which will
cause PyGreSQL to return monetary values as strings including their
formatting and currency.

.. versionadded:: 4.1.1

get/set_bool -- whether boolean values are returned as bool objects
-------------------------------------------------------------------

.. function:: get_bool()

    Check whether boolean values are returned as bool objects

    :returns: whether or not bool objects will be returned
    :rtype: bool

This function checks whether PyGreSQL returns PostgreSQL boolean
values converted to Python bool objects, or as ``'f'`` and ``'t'``
strings which are the values used internally by PostgreSQL.  By default,
conversion to bool objects is activated, but you can disable this with
the :func:`set_bool` function.

.. versionadded:: 4.2

.. function:: set_bool(on)

    Set whether boolean values are returned as bool objects

    :param on: whether or not bool objects shall be returned

This function can be used to specify whether PyGreSQL shall return
PostgreSQL boolean values converted to Python bool objects, or as
``'f'`` and ``'t'`` strings which are the values used internally by
PostgreSQL.  By default, conversion to bool objects is activated,
but you can disable this by calling ``set_bool(True)``.

.. versionadded:: 4.2

.. versionchanged:: 5.0
    Boolean values had been returned as string by default in earlier versions.

get/set_array -- whether arrays are returned as list objects
------------------------------------------------------------

.. function:: get_array()

    Check whether arrays are returned as list objects

    :returns: whether or not list objects will be returned
    :rtype: bool

This function checks whether PyGreSQL returns PostgreSQL arrays converted
to Python list objects, or simply as text in the internal special output
syntax of PostgreSQL.  By default, conversion to list objects is activated,
but you can disable this with the :func:`set_array` function.

.. versionadded:: 5.0

.. function:: set_array(on)

    Set whether arrays are returned as list objects

    :param on: whether or not list objects shall be returned

This function can be used to specify whether PyGreSQL shall return PostgreSQL
arrays converted to Python list objects, or simply as text in the internal
special output syntax of PostgreSQL.  By default, conversion to list objects
is activated, but you can disable this by calling ``set_array(False)``.

.. versionadded:: 5.0

.. versionchanged:: 5.0
    Arrays had been always returned as text strings only in earlier versions.

get/set_bytea_escaped -- whether bytea data is returned escaped
---------------------------------------------------------------

.. function:: get_bytea_escaped()

    Check whether bytea values are returned as escaped strings

    :returns: whether or not bytea objects will be returned escaped
    :rtype: bool

This function checks whether PyGreSQL returns PostgreSQL ``bytea`` values in
escaped form or in unescaped from as byte strings.  By default, bytea values
will be returned unescaped as byte strings, but you can change this with the
:func:`set_bytea_escaped` function.

.. versionadded:: 5.0

.. function:: set_bytea_escaped(on)

    Set whether bytea values are returned as escaped strings

    :param on: whether or not bytea objects shall be returned escaped

This function can be used to specify whether PyGreSQL shall return
PostgreSQL ``bytea`` values in escaped form or in unescaped from as byte
strings.  By default, bytea values will be returned unescaped as byte
strings, but you can change this by calling ``set_bytea_escaped(True)``.

.. versionadded:: 5.0

.. versionchanged:: 5.0
    Bytea data had been returned in escaped form by default in earlier versions.

get/set_jsondecode -- decoding JSON format
------------------------------------------

.. function:: get_jsondecode()

    Get the function that deserializes JSON formatted strings

This returns the function used by PyGreSQL to construct Python objects
from JSON formatted strings.

.. function:: set_jsondecode(func)

    Set a function that will deserialize JSON formatted strings

    :param func: the function to be used for deserializing JSON strings

You can use this if you do not want to deserialize JSON strings coming
in from the database, or if want to use a different function than the
standard function :func:`json.loads` or if you want to use it with parameters
different from the default ones.  If you set this function to *None*, then
the automatic deserialization of JSON strings will be deactivated.

.. versionadded:: 5.0

.. versionchanged:: 5.0
    JSON data had been always returned as text strings in earlier versions.

get/set_cast_hook -- fallback typecast function
-----------------------------------------------

.. function:: get_cast_hook()

    Get the function that handles all external typecasting

This returns the callback function used by PyGreSQL to provide plug-in
Python typecast functions.

.. function:: set_cast_hook(func)

    Set a function that will handle all external typecasting

    :param func: the function to be used as a callback

If you set this function to *None*, then only the typecast functions
implemented in the C extension module are enabled.  You normally would
not want to change this.  Instead, you can use :func:`get_typecast` and
:func:`set_typecast` to add or change the plug-in Python typecast functions.

.. versionadded:: 5.0

get/set_datestyle -- assume a fixed date style
----------------------------------------------

.. function:: get_datestyle()

    Get the assumed date style for typecasting

This returns the PostgreSQL date style that is silently assumed when
typecasting dates or *None* if no fixed date style is assumed, in which case
the date style is requested from the database when necessary (this is the
default).  Note that this method will *not* get the date style that is
currently set in the session or in the database.  You can get the current
setting with the methods :meth:`DB.get_parameter` and
:meth:`Connection.parameter`.  You can also get the date format corresponding
to the current date style by calling :meth:`Connection.date_format`.

.. versionadded:: 5.0

.. function:: set_datestyle(datestyle)

    Set a fixed date style that shall be assumed when typecasting

    :param str datestyle: the date style that shall be assumed,
      or *None* if no fixed dat style shall be assumed

PyGreSQL is able to automatically pick up the right date style for typecasting
date values from the database, even if you change it for the current session
with a ``SET DateStyle`` command.  This is happens very effectively without
an additional database request being involved.  If you still want to have
PyGreSQL always assume a fixed date style instead, then you can set one with
this function.  Note that calling this function will *not* alter the date
style of the database or the current session.  You can do that by calling
the method :meth:`DB.set_parameter` instead.

.. versionadded:: 5.0

get/set_typecast -- custom typecasting
--------------------------------------

PyGreSQL uses typecast functions to cast the raw data coming from the
database to Python objects suitable for the particular database type.
These functions take a single string argument that represents the data
to be casted and must return the casted value.

PyGreSQL provides through its C extension module basic typecast functions
for the common database types, but if you want to add more typecast functions,
you can set these using the following functions.

.. method:: get_typecast(typ)

    Get the global cast function for the given database type

    :param str typ: PostgreSQL type name
    :returns: the typecast function for the specified type
    :rtype: function or None

.. versionadded:: 5.0

.. method:: set_typecast(typ, cast)

    Set a global typecast function for the given database type(s)

    :param typ: PostgreSQL type name or list of type names
    :type typ: str or list
    :param cast: the typecast function to be set for the specified type(s)
    :type typ: str or int

The typecast function must take one string object as argument and return a
Python object into which the PostgreSQL type shall be casted.  If the function
takes another parameter named *connection*, then the current database
connection will also be passed to the typecast function.  This may sometimes
be necessary to look up certain database settings.

.. versionadded:: 5.0

Note that database connections cache types and their cast functions using
connection specific :class:`DbTypes` objects.  You can also get, set and
reset typecast functions on the connection level using the methods
:meth:`DbTypes.get_typecast`, :meth:`DbTypes.set_typecast` and
:meth:`DbTypes.reset_typecast` of the :attr:`DB.dbtypes` object.  This will
not affect other connections or future connections.  In order to be sure
a global change is picked up by a running connection, you must reopen it or
call :meth:`DbTypes.reset_typecast` on the :attr:`DB.dbtypes` object.

Also note that the typecasting for all of the basic types happens already
in the C extension module.  The typecast functions that can be set with
the above methods are only called for the types that are not already
supported by the C extension module.

cast_array/record -- fast parsers for arrays and records
--------------------------------------------------------

PosgreSQL returns arrays and records (composite types) using a special output
syntax with several quirks that cannot easily and quickly be parsed in Python.
Therefore the C extension module provides two fast parsers that allow quickly
turning these text representations into Python objects: Arrays will be
converted to Python lists, and records to Python tuples.  These fast parsers
are used automatically by PyGreSQL in order to return arrays and records from
database queries as lists and tuples, so you normally don't need to call them
directly.  You may only need them for typecasting arrays of data types that
are not supported by default in PostgreSQL.

.. function::  cast_array(string, [cast], [delim])

    Cast a string representing a PostgreSQL array to a Python list

    :param str string: the string with the text representation of the array
    :param cast: a typecast function for the elements of the array
    :type cast: callable or None
    :param delim: delimiter character between adjacent elements
    :type str: byte string with a single character
    :returns: a list representing the PostgreSQL array in Python
    :rtype: list
    :raises TypeError: invalid argument types
    :raises ValueError: error in the syntax of the given array

This function takes a *string* containing the text representation of a
PostgreSQL array (which may look like ``'{{1,2}{3,4}}'`` for a two-dimensional
array), a typecast function *cast* that is called for every element, and
an optional delimiter character *delim* (usually a comma), and returns a
Python list representing the array (which may be nested like
``[[1, 2], [3, 4]]`` in this example).  The cast function must take a single
argument which will be the text representation of the element and must output
the corresponding Python object that shall be put into the list.  If you don't
pass a cast function or set it to *None*, then unprocessed text strings will
be returned as elements of the array.  If you don't pass a delimiter character,
then a comma will be used by default.

.. versionadded:: 5.0

.. function::  cast_record(string, [cast], [delim])

    Cast a string representing a PostgreSQL record to a Python list

    :param str string: the string with the text representation of the record
    :param cast: typecast function(s) for the elements of the record
    :type cast: callable, list or tuple of callables, or None
    :param delim: delimiter character between adjacent elements
    :type str: byte string with a single character
    :returns: a tuple representing the PostgreSQL record in Python
    :rtype: tuple
    :raises TypeError: invalid argument types
    :raises ValueError: error in the syntax of the given array

This function takes a *string* containing the text representation of a
PostgreSQL record (which may look like ``'(1,a,2,b)'`` for a record composed
of four fields), a typecast function *cast* that is called for every element,
or a list or tuple of such functions corresponding to the individual fields
of the record, and an optional delimiter character *delim* (usually a comma),
and returns a Python tuple representing the record (which may be inhomogeneous
like ``(1, 'a', 2, 'b')`` in this example).  The cast function(s) must take a
single argument which will be the text representation of the element and must
output the corresponding Python object that shall be put into the tuple.  If
you don't pass cast function(s) or pass *None* instead, then unprocessed text
strings will be returned as elements of the tuple.  If you don't pass a
delimiter character, then a comma will be used by default.

.. versionadded:: 5.0

Note that besides using parentheses instead of braces, there are other subtle
differences in escaping special characters and NULL values between the syntax
used for arrays and the one used for composite types, which these functions
take into account.

Type helpers
------------

The module provides the following type helper functions.  You can wrap
parameters with these functions when passing them to :meth:`DB.query`
or :meth:`DB.query_formatted` in order to give PyGreSQL a hint about the
type of the parameters, if it cannot be derived from the context.

.. function:: Bytea(bytes)

    A wrapper for holding a bytea value

.. versionadded:: 5.0

.. function:: HStore(dict)

    A wrapper for holding an hstore dictionary

.. versionadded:: 5.0

.. function:: Json(obj)

    A wrapper for holding an object serializable to JSON

.. versionadded:: 5.0

The following additional type helper is only meaningful when used with
:meth:`DB.query_formatted`.  It marks a parameter as text that shall be
literally included into the SQL.  This is useful for passing table names
for instance.

.. function:: Literal(sql)

    A wrapper for holding a literal SQL string

.. versionadded:: 5.0


Module constants
----------------

Some constants are defined in the module dictionary.
They are intended to be used as parameters for methods calls.
You should refer to the libpq description in the PostgreSQL user manual
for more information about them. These constants are:

.. data:: version
.. data:: __version__

    constants that give the current version

.. data:: INV_READ
.. data:: INV_WRITE

    large objects access modes,
    used by :meth:`Connection.locreate` and :meth:`LargeObject.open`

.. data:: SEEK_SET
.. data:: SEEK_CUR
.. data:: SEEK_END

    positional flags, used by :meth:`LargeObject.seek`

.. data:: TRANS_IDLE
.. data:: TRANS_ACTIVE
.. data:: TRANS_INTRANS
.. data:: TRANS_INERROR
.. data:: TRANS_UNKNOWN

    transaction states, used by :meth:`Connection.transaction`
