TypeCache -- The internal cache for database types
==================================================

.. py:currentmodule:: pgdb

.. class:: TypeCache

.. versionadded:: 5.0

The internal :class:`TypeCache` of PyGreSQL is not part of the DB-API 2
standard, but is documented here in case you need full control and
understanding of the internal handling of database types.

The TypeCache is essentially a dictionary mapping PostgreSQL internal
type names and type OIDs to DB-API 2 "type codes" (which are also returned
as the *type_code* field of the :attr:`Cursor.description` attribute).

These type codes are strings which are equal to the PostgreSQL internal
type name, but they are also carrying additional information about the
associated PostgreSQL type in the following attributes:

        - *oid* -- the OID of the type
        - *len*  -- the internal size
        - *type*  -- ``'b'`` = base, ``'c'`` = composite, ...
        - *category*  -- ``'A'`` = Array, ``'B'`` = Boolean, ...
        - *delim*  -- delimiter to be used when parsing arrays
        - *relid*  -- the table OID for composite types

For details, see the PostgreSQL documentation on `pg_type
<http://www.postgresql.org/docs/current/static/catalog-pg-type.html>`_.

In addition to the dictionary methods, the :class:`TypeCache` provides
the following methods:

.. method:: TypeCache.get_fields(typ)

    Get the names and types of the fields of composite types

    :param typ: PostgreSQL type name or OID of a composite type
    :type typ: str or int
    :returns: a list of pairs of field names and types
    :rtype: list

.. method:: TypeCache.get_typecast(typ)

    Get the cast function for the given database type

    :param str typ: PostgreSQL type name or type code
    :returns: the typecast function for the specified type
    :rtype: function or None

.. method:: TypeCache.set_typecast(typ, cast)

    Set a typecast function for the given database type(s)

    :param typ: PostgreSQL type name or type code, or list of such
    :type typ: str or list
    :param cast: the typecast function to be set for the specified type(s)
    :type typ: str or int

The typecast function must take one string object as argument and return a
Python object into which the PostgreSQL type shall be casted.  If the function
takes another parameter named *connection*, then the current database
connection will also be passed to the typecast function.  This may sometimes
be necessary to look up certain database settings.

.. method:: TypeCache.reset_typecast([typ])

    Reset the typecasts for the specified (or all) type(s) to their defaults

    :param str typ: PostgreSQL type name or type code, or list of such,
        or None to reset all typecast functions
    :type typ: str, list or None

.. method:: TypeCache.typecast(value, typ)

    Cast the given value according to the given database type

    :param str typ: PostgreSQL type name or type code
    :returns: the casted value

.. note::

    Note that the :class:`TypeCache` is always bound to a database connection.
    You can also get, set and reset typecast functions on a global level using
    the functions :func:`pgdb.get_typecast`, :func:`pgdb.set_typecast` and
    :func:`pgdb.reset_typecast`.  If you do this, the current database
    connections will continue to use their already cached typecast functions
    unless call the :meth:`TypeCache.reset_typecast` method on the
    :attr:`Connection.type_cache` objects of the running connections.
