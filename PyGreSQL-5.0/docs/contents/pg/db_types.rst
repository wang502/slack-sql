DbTypes -- The internal cache for database types
================================================

.. py:currentmodule:: pg

.. class:: DbTypes

.. versionadded:: 5.0

The :class:`DbTypes` object is essentially a dictionary mapping PostgreSQL
internal type names and type OIDs to PyGreSQL "type names" (which are also
returned by :meth:`DB.get_attnames` as dictionary values).

These type names are strings which are equal to the simple PyGreSQL name or
to regular type names if these have been enabled with :meth:`DB.use_regtypes`.
Besides being strings, they are also carrying additional information about the
associated PostgreSQL type in the following attributes:

        - *oid* -- the PostgreSQL type OID
        - *pgtype* -- the PostgreSQL type name
        - *regtype* -- the regular type name
        - *simple* -- the simple PyGreSQL type name
        - *typtype* -- `b` = base type, `c` = composite type etc.
        - *category* -- `A` = Array, `b` =Boolean, `C` = Composite etc.
        - *delim* -- delimiter for array types
        - *relid* -- corresponding table for composite types
        - *attnames* -- attributes for composite types

For details, see the PostgreSQL documentation on `pg_type
<http://www.postgresql.org/docs/current/static/catalog-pg-type.html>`_.

In addition to the dictionary methods, the :class:`DbTypes` class also
provides the following methods:

.. method:: DbTypes.get_attnames(typ)

    Get the names and types of the fields of composite types

    :param typ: PostgreSQL type name or OID of a composite type
    :type typ: str or int
    :returns: an ordered dictionary mapping field names to type names

.. method:: DbTypes.get_typecast(typ)

    Get the cast function for the given database type

    :param str typ: PostgreSQL type name
    :returns: the typecast function for the specified type
    :rtype: function or None

.. method:: DbTypes.set_typecast(typ, cast)

    Set a typecast function for the given database type(s)

    :param typ: PostgreSQL type name or list of type names
    :type typ: str or list
    :param cast: the typecast function to be set for the specified type(s)
    :type typ: str or int

The typecast function must take one string object as argument and return a
Python object into which the PostgreSQL type shall be casted.  If the function
takes another parameter named *connection*, then the current database
connection will also be passed to the typecast function.  This may sometimes
be necessary to look up certain database settings.

.. method:: DbTypes.reset_typecast([typ])

    Reset the typecasts for the specified (or all) type(s) to their defaults

    :param str typ: PostgreSQL type name or list of type names,
        or None to reset all typecast functions
    :type typ: str, list or None

.. method:: DbTypes.typecast(value, typ)

    Cast the given value according to the given database type

    :param str typ: PostgreSQL type name or type code
    :returns: the casted value

.. note::

    Note that :class:`DbTypes` object is always bound to a database connection.
    You can also get and set and reset typecast functions on a global level
    using the functions :func:`pg.get_typecast` and :func:`pg.set_typecast`.
    If you do this, the current database connections will continue to use their
    already cached typecast functions unless you reset the typecast functions
    by calling the :meth:`DbTypes.reset_typecast` method on :attr:`DB.dbtypes`
    objects of the running connections.

    Also note that the typecasting for all of the basic types happens already
    in the C extension module.  The typecast functions that can be set with
    the above methods are only called for the types that are not already
    supported by the C extension module.
