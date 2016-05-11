Module functions and constants
==============================

.. py:currentmodule:: pgdb

The :mod:`pgdb` module defines a :func:`connect` function that allows to
connect to a database, some global constants describing the capabilities
of the module as well as several exception classes.

connect -- Open a PostgreSQL connection
---------------------------------------

.. function:: connect([dsn], [user], [password], [host], [database])

    Return a new connection to the database

    :param str dsn: data source name as string
    :param str user: the database user name
    :param str password: the database password
    :param str host: the hostname of the database
    :param database: the name of the database
    :returns: a connection object
    :rtype: :class:`Connection`
    :raises pgdb.OperationalError: error connecting to the database

This function takes parameters specifying how to connect to a PostgreSQL
database and returns a :class:`Connection` object using these parameters.
If specified, the *dsn* parameter must be a string with the format
``'host:base:user:passwd:opt'``. All of the parts specified in the *dsn*
are optional. You can also specify the parameters individually using keyword
arguments, which always take precedence. The *host* can also contain a port
if specified in the format ``'host:port'``. In the *opt* part of the *dsn*
you can pass command-line options to the server.

Example::

    con = connect(dsn='myhost:mydb', user='guido', password='234$')


get/set/reset_typecast -- Control the global typecast functions
---------------------------------------------------------------

PyGreSQL uses typecast functions to cast the raw data coming from the
database to Python objects suitable for the particular database type.
These functions take a single string argument that represents the data
to be casted and must return the casted value.

PyGreSQL provides built-in typecast functions for the common database types,
but if you want to change these or add more typecast functions, you can set
these up using the following functions.

.. note::

    The following functions are not part of the DB-API 2 standard.

.. method:: get_typecast(typ)

    Get the global cast function for the given database type

    :param str typ: PostgreSQL type name or type code
    :returns: the typecast function for the specified type
    :rtype: function or None

.. versionadded:: 5.0

.. method:: set_typecast(typ, cast)

    Set a global typecast function for the given database type(s)

    :param typ: PostgreSQL type name or type code, or list of such
    :type typ: str or list
    :param cast: the typecast function to be set for the specified type(s)
    :type typ: str or int

The typecast function must take one string object as argument and return a
Python object into which the PostgreSQL type shall be casted.  If the function
takes another parameter named *connection*, then the current database
connection will also be passed to the typecast function.  This may sometimes
be necessary to look up certain database settings.

.. versionadded:: 5.0

.. method:: reset_typecast([typ])

    Reset the typecasts for the specified (or all) type(s) to their defaults

    :param str typ: PostgreSQL type name or type code, or list of such,
        or None to reset all typecast functions
    :type typ: str, list or None

.. versionadded:: 5.0

Note that database connections cache types and their cast functions using
connection specific :class:`TypeCache` objects.  You can also get, set and
reset typecast functions on the connection level using the methods
:meth:`TypeCache.get_typecast`, :meth:`TypeCache.set_typecast` and
:meth:`TypeCache.reset_typecast` of the :attr:`Connection.type_cache`.  This
will not affect other connections or future connections.  In order to be sure
a global change is picked up by a running connection, you must reopen it or
call :meth:`TypeCache.reset_typecast` on the :attr:`Connection.type_cache`.

Module constants
----------------

.. data:: apilevel

    The string constant ``'2.0'``, stating that the module is DB-API 2.0 level
    compliant.

.. data:: threadsafety

    The integer constant 1, stating that the module itself is thread-safe,
    but the connections are not thread-safe, and therefore must be protected
    with a lock if you want to use them from different threads.

.. data:: paramstyle

    The string constant ``pyformat``, stating that parameters should be passed
    using Python extended format codes, e.g. ``" ... WHERE name=%(name)s"``.

Errors raised by this module
----------------------------

The errors that can be raised by the :mod:`pgdb` module are the following:

.. exception:: Warning

    Exception raised for important warnings like data truncations while
    inserting.

.. exception:: Error

    Exception that is the base class of all other error exceptions. You can
    use this to catch all errors with one single except statement.
    Warnings are not considered errors and thus do not use this class as base.

.. exception:: InterfaceError

    Exception raised for errors that are related to the database interface
    rather than the database itself.

.. exception:: DatabaseError

    Exception raised for errors that are related to the database.

    In PyGreSQL, this also has a :attr:`DatabaseError.sqlstate` attribute
    that contains the ``SQLSTATE`` error code of this error.

.. exception:: DataError

    Exception raised for errors that are due to problems with the processed
    data like division by zero or numeric value out of range.

.. exception:: OperationalError

    Exception raised for errors that are related to the database's operation
    and not necessarily under the control of the programmer, e.g. an unexpected
    disconnect occurs, the data source name is not found, a transaction could
    not be processed, or a memory allocation error occurred during processing.

.. exception:: IntegrityError

    Exception raised when the relational integrity of the database is affected,
    e.g. a foreign key check fails.

.. exception:: ProgrammingError

    Exception raised for programming errors, e.g. table not found or already
    exists, syntax error in the SQL statement or wrong number of parameters
    specified.

.. exception:: NotSupportedError

    Exception raised in case a method or database API was used which is not
    supported by the database.
