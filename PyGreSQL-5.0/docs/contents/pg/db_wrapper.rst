The DB wrapper class
====================

.. py:currentmodule:: pg

.. class:: DB

The :class:`Connection` methods are wrapped in the class :class:`DB`
which also adds convenient higher level methods for working with the
database.  It also serves as a context manager for the connection.
The preferred way to use this module is as follows::

    import pg

    with pg.DB(...) as db:  # for parameters, see below
        for r in db.query(  # just for example
                "SELECT foo, bar FROM foo_bar_table WHERE foo !~ bar"
                ).dictresult():
            print('%(foo)s %(bar)s' % r)

This class can be subclassed as in this example::

    import pg

    class DB_ride(pg.DB):
        """Ride database wrapper

        This class encapsulates the database functions and the specific
        methods for the ride database."""

    def __init__(self):
        """Open a database connection to the rides database"""
        pg.DB.__init__(self, dbname='ride')
        self.query("SET DATESTYLE TO 'ISO'")

    [Add or override methods here]

The following describes the methods and variables of this class.

Initialization
--------------
The :class:`DB` class is initialized with the same arguments as the
:func:`connect` function described above. It also initializes a few
internal variables. The statement ``db = DB()`` will open the local
database with the name of the user just like ``connect()`` does.

You can also initialize the DB class with an existing :mod:`pg` or :mod:`pgdb`
connection. Pass this connection as a single unnamed parameter, or as a
single parameter named ``db``. This allows you to use all of the methods
of the DB class with a DB-API 2 compliant connection. Note that the
:meth:`Connection.close` and :meth:`Connection.reopen` methods are inoperative
in this case.

pkey -- return the primary key of a table
-----------------------------------------

.. method:: DB.pkey(table)

    Return the primary key of a table

    :param str table: name of table
    :returns: Name of the field which is the primary key of the table
    :rtype: str
    :raises KeyError: the table does not have a primary key

This method returns the primary key of a table.  Single primary keys are
returned as strings unless you set the composite flag.  Composite primary
keys are always represented as tuples.  Note that this raises a KeyError
if the table does not have a primary key.

get_databases -- get list of databases in the system
----------------------------------------------------

.. method:: DB.get_databases()

    Get the list of databases in the system

    :returns: all databases in the system
    :rtype: list

Although you can do this with a simple select, it is added here for
convenience.

get_relations -- get list of relations in connected database
------------------------------------------------------------

.. method:: DB.get_relations([kinds], [system])

    Get the list of relations in connected database

    :param str kinds: a string or sequence of type letters
    :param bool system: whether system relations should be returned
    :returns: all relations of the given kinds in the database
    :rtype: list

This method returns the list of relations in the connected database.  Although
you can do this with a simple select, it is added here for convenience.  You
can select which kinds of relations you are interested in by passing type
letters in the `kinds` parameter.  The type letters are ``r`` = ordinary table,
``i`` = index, ``S`` = sequence, ``v`` = view, ``c`` = composite type,
``s`` = special, ``t`` = TOAST table.  If `kinds` is None or an empty string,
all relations are returned (this is also the default).  If `system` is set to
`True`, then system tables and views (temporary tables, toast tables, catalog
vies and tables) will be returned as well, otherwise they will be ignored.

get_tables -- get list of tables in connected database
------------------------------------------------------

.. method:: DB.get_tables([system])

    Get the list of tables in connected database

    :param bool system: whether system tables should be returned
    :returns: all tables in connected database
    :rtype: list

This is a shortcut for ``get_relations('r', system)`` that has been added for
convenience.

get_attnames -- get the attribute names of a table
--------------------------------------------------

.. method:: DB.get_attnames(table)

    Get the attribute names of a table

    :param str table: name of table
    :returns: an ordered dictionary mapping attribute names to type names

Given the name of a table, digs out the set of attribute names.

Returns a read-only dictionary of attribute names (the names are the keys,
the values are the names of the attributes' types) with the column names
in the proper order if you iterate over it.

By default, only a limited number of simple types will be returned.
You can get the regular types after enabling this by calling the
:meth:`DB.use_regtypes` method.

has_table_privilege -- check table privilege
--------------------------------------------

.. method:: DB.has_table_privilege(table, privilege)

    Check whether current user has specified table privilege

    :param str table: the name of the table
    :param str privilege: privilege to be checked -- default is 'select'
    :returns: whether current user has specified table privilege
    :rtype: bool

Returns True if the current user has the specified privilege for the table.

.. versionadded:: 4.0

get/set_parameter -- get or set  run-time parameters
----------------------------------------------------

.. method:: DB.get_parameter(parameter)

    Get the value of run-time parameters

    :param parameter: the run-time parameter(s) to get
    :type param: str, tuple, list or dict
    :returns: the current value(s) of the run-time parameter(s)
    :rtype: str, list or dict
    :raises TypeError: Invalid parameter type(s)
    :raises pg.ProgrammingError: Invalid parameter name(s)

If the parameter is a string, the return value will also be a string
that is the current setting of the run-time parameter with that name.

You can get several parameters at once by passing a list, set or dict.
When passing a list of parameter names, the return value will be a
corresponding list of parameter settings.  When passing a set of
parameter names, a new dict will be returned, mapping these parameter
names to their settings.  Finally, if you pass a dict as parameter,
its values will be set to the current parameter settings corresponding
to its keys.

By passing the special name ``'all'`` as the parameter, you can get a dict
of all existing configuration parameters.

Note that you can request most of the important parameters also using
:meth:`Connection.parameter()` which does not involve a database query
like it is the case for :meth:`DB.get_parameter` and :meth:`DB.set_parameter`.

.. versionadded:: 4.2

.. method:: DB.set_parameter(parameter, [value], [local])

    Set the value of run-time parameters

    :param parameter: the run-time parameter(s) to set
    :type param: string, tuple, list or dict
    :param value: the value to set
    :type param: str or None
    :raises TypeError: Invalid parameter type(s)
    :raises ValueError: Invalid value argument(s)
    :raises pg.ProgrammingError: Invalid parameter name(s) or values

If the parameter and the value are strings, the run-time parameter
will be set to that value.  If no value or *None* is passed as a value,
then the run-time parameter will be restored to its default value.

You can set several parameters at once by passing a list of parameter
names, together with a single value that all parameters should be
set to or with a corresponding list of values.  You can also pass
the parameters as a set if you only provide a single value.
Finally, you can pass a dict with parameter names as keys.  In this
case, you should not pass a value, since the values for the parameters
will be taken from the dict.

By passing the special name ``'all'`` as the parameter, you can reset
all existing settable run-time parameters to their default values.

If you set *local* to `True`, then the command takes effect for only the
current transaction.  After :meth:`DB.commit` or :meth:`DB.rollback`,
the session-level setting takes effect again.  Setting *local* to `True`
will appear to have no effect if it is executed outside a transaction,
since the transaction will end immediately.

.. versionadded:: 4.2

begin/commit/rollback/savepoint/release -- transaction handling
---------------------------------------------------------------

.. method:: DB.begin([mode])

    Begin a transaction

    :param str mode: an optional transaction mode such as 'READ ONLY'

    This initiates a transaction block, that is, all following queries
    will be executed in a single transaction until :meth:`DB.commit`
    or :meth:`DB.rollback` is called.

.. versionadded:: 4.1

.. method:: DB.start()

    This is the same as the :meth:`DB.begin` method.

.. method:: DB.commit()

    Commit a transaction

    This commits the current transaction. All changes made by the
    transaction become visible to others and are guaranteed to be
    durable if a crash occurs.

.. method:: DB.end()

    This is the same as the :meth:`DB.commit` method.

.. versionadded:: 4.1

.. method:: DB.rollback([name])

    Roll back a transaction

    :param str name: optionally, roll back to the specified savepoint

    This rolls back the current transaction and causes all the updates
    made by the transaction to be discarded.

.. method:: DB.abort()

    This is the same as the :meth:`DB.rollback` method.

.. versionadded:: 4.2

.. method:: DB.savepoint(name)

    Define a new savepoint

    :param str name: the name to give to the new savepoint

    This establishes a new savepoint within the current transaction.

.. versionadded:: 4.1

.. method:: DB.release(name)

    Destroy a savepoint

    :param str name: the name of the savepoint to destroy

    This destroys a savepoint previously defined in the current transaction.

.. versionadded:: 4.1

get -- get a row from a database table or view
----------------------------------------------

.. method:: DB.get(table, row, [keyname])

    Get a row from a database table or view

    :param str table: name of table or view
    :param row: either a dictionary or the value to be looked up
    :param str keyname: name of field to use as key (optional)
    :returns: A dictionary - the keys are the attribute names,
      the values are the row values.
    :raises pg.ProgrammingError: table has no primary key or missing privilege
    :raises KeyError: missing key value for the row

This method is the basic mechanism to get a single row.  It assumes
that the *keyname* specifies a unique row.  It must be the name of a
single column or a tuple of column names.  If *keyname* is not specified,
then the primary key for the table is used.

If *row* is a dictionary, then the value for the key is taken from it.
Otherwise, the row must be a single value or a tuple of values
corresponding to the passed *keyname* or primary key.  The fetched row
from the table will be returned as a new dictionary or used to replace
the existing values when row was passed as aa dictionary.

The OID is also put into the dictionary if the table has one, but
in order to allow the caller to work with multiple tables, it is
munged as ``oid(table)`` using the actual name of the table.

Note that since PyGreSQL 5.0 this will return the value of an array
type column as a Python list.

insert -- insert a row into a database table
--------------------------------------------

.. method:: DB.insert(table, [row], [col=val, ...])

    Insert a row into a database table

    :param str table: name of table
    :param dict row: optional dictionary of values
    :param col: optional keyword arguments for updating the dictionary
    :returns: the inserted values in the database
    :rtype: dict
    :raises pg.ProgrammingError: missing privilege or conflict

This method inserts a row into a table.  If the optional dictionary is
not supplied then the required values must be included as keyword/value
pairs.  If a dictionary is supplied then any keywords provided will be
added to or replace the entry in the dictionary.

The dictionary is then reloaded with the values actually inserted in order
to pick up values modified by rules, triggers, etc.

Note that since PyGreSQL 5.0 it is possible to insert a value for an
array type column by passing it as Python list.

update -- update a row in a database table
------------------------------------------

.. method:: DB.update(table, [row], [col=val, ...])

    Update a row in a database table

    :param str table: name of table
    :param dict row: optional dictionary of values
    :param col: optional keyword arguments for updating the dictionary
    :returns: the new row in the database
    :rtype: dict
    :raises pg.ProgrammingError: table has no primary key or missing privilege
    :raises KeyError: missing key value for the row

Similar to insert but updates an existing row.  The update is based on
the primary key of the table or the OID value as munged by :meth:`DB.get`
or passed as keyword.

The dictionary is then modified to reflect any changes caused by the
update due to triggers, rules, default values, etc.

Like insert, the dictionary is optional and updates will be performed
on the fields in the keywords.  There must be an OID or primary key
either in the dictionary where the OID must be munged, or in the keywords
where it can be simply the string ``'oid'``.

upsert -- insert a row with conflict resolution
-----------------------------------------------

.. method:: DB.upsert(table, [row], [col=val, ...])

    Insert a row into a database table with conflict resolution

    :param str table: name of table
    :param dict row: optional dictionary of values
    :param col: optional keyword arguments for specifying the update
    :returns: the new row in the database
    :rtype: dict
    :raises pg.ProgrammingError: table has no primary key or missing privilege

This method inserts a row into a table, but instead of raising a
ProgrammingError exception in case a row with the same primary key already
exists, an update will be executed instead.  This will be performed as a
single atomic operation on the database, so race conditions can be avoided.

Like the insert method, the first parameter is the name of the table and the
second parameter can be used to pass the values to be inserted as a dictionary.

Unlike the insert und update statement, keyword parameters are not used to
modify the dictionary, but to specify which columns shall be updated in case
of a conflict, and in which way:

A value of `False` or `None` means the column shall not be updated,
a value of `True` means the column shall be updated with the value that
has been proposed for insertion, i.e. has been passed as value in the
dictionary.  Columns that are not specified by keywords but appear as keys
in the dictionary are also updated like in the case keywords had been passed
with the value `True`.

So if in the case of a conflict you want to update every column that has been
passed in the dictionary `d` , you would call ``upsert(table, d)``.  If you
don't want to do anything in case of a conflict, i.e. leave the existing row
as it is, call ``upsert(table, d, **dict.fromkeys(d))``.

If you need more fine-grained control of what gets updated, you can also pass
strings in the keyword parameters.  These strings will be used as SQL
expressions for the update columns.  In these expressions you can refer
to the value that already exists in the table by writing the table prefix
``included.`` before the column name, and you can refer to the value that
has been proposed for insertion by writing ``excluded.`` as table prefix.

The dictionary is modified in any case to reflect the values in the database
after the operation has completed.

.. note::

    The method uses the PostgreSQL "upsert" feature which is only available
    since PostgreSQL 9.5. With older PostgreSQL versions, you will get a
    ProgrammingError if you use this method.

.. versionadded:: 5.0

query -- execute a SQL command string
-------------------------------------

.. method:: DB.query(command, [arg1, [arg2, ...]])

    Execute a SQL command string

    :param str command: SQL command
    :param arg*: optional positional arguments
    :returns: result values
    :rtype: :class:`Query`, None
    :raises TypeError: bad argument type, or too many arguments
    :raises TypeError: invalid connection
    :raises ValueError: empty SQL query or lost connection
    :raises pg.ProgrammingError: error in query
    :raises pg.InternalError: error during query processing

Similar to the :class:`Connection` function with the same name, except that
positional arguments can be passed either as a single list or tuple, or as
individual positional arguments.

Example::

    name = input("Name? ")
    phone = input("Phone? ")
    rows = db.query("update employees set phone=$2 where name=$1",
        (name, phone)).getresult()[0][0]
    # or
    rows = db.query("update employees set phone=$2 where name=$1",
         name, phone).getresult()[0][0]

query_formatted -- execute a formatted SQL command string
---------------------------------------------------------

.. method:: DB.query_formatted(command, parameters, [types], [inline])

    Execute a formatted SQL command string

    :param str command: SQL command
    :param parameters: the values of the parameters for the SQL command
    :type parameters: tuple, list or dict
    :param types: optionally, the types of the parameters
    :type types: tuple, list or dict
    :param bool inline: whether the parameters should be passed in the SQL
    :rtype: :class:`Query`, None
    :raises TypeError: bad argument type, or too many arguments
    :raises TypeError: invalid connection
    :raises ValueError: empty SQL query or lost connection
    :raises pg.ProgrammingError: error in query
    :raises pg.InternalError: error during query processing

Similar to :meth:`DB.query`, but using Python format placeholders of the form
``%s`` or ``%(names)s`` instead of PostgreSQL placeholders of the form ``$1``.
The parameters must be passed as a tuple, list or dict.  You can also pass a
corresponding tuple, list or dict of database types in order to format the
parameters properly in case there is ambiguity.

If you set *inline* to True, the parameters will be sent to the database
embedded in the SQL command, otherwise they will be sent separately.

Example::

    name = input("Name? ")
    phone = input("Phone? ")
    rows = db.query_formatted(
        "update employees set phone=%s where name=%s",
        (phone, name)).getresult()[0][0]
    # or
    rows = db.query_formatted(
        "update employees set phone=%(phone)s where name=%(name)s",
        dict(name=name, phone=phone)).getresult()[0][0]

clear -- clear row values in memory
-----------------------------------

.. method:: DB.clear(table, [row])

    Clear row values in memory

    :param str table: name of table
    :param dict row: optional dictionary of values
    :returns: an empty row
    :rtype: dict

This method clears all the attributes to values determined by the types.
Numeric types are set to 0, Booleans are set to *False*, and everything
else is set to the empty string.  If the row argument is present, it is
used as the row dictionary and any entries matching attribute names are
cleared with everything else left unchanged.

If the dictionary is not supplied a new one is created.

delete -- delete a row from a database table
--------------------------------------------

.. method:: DB.delete(table, [row], [col=val, ...])

    Delete a row from a database table

    :param str table: name of table
    :param dict d: optional dictionary of values
    :param col: optional keyword arguments for updating the dictionary
    :rtype: None
    :raises pg.ProgrammingError: table has no primary key,
        row is still referenced or missing privilege
    :raises KeyError: missing key value for the row

This method deletes the row from a table.  It deletes based on the
primary key of the table or the OID value as munged by :meth:`DB.get`
or passed as keyword.

The return value is the number of deleted rows (i.e. 0 if the row did not
exist and 1 if the row was deleted).

Note that if the row cannot be deleted because e.g. it is still referenced
by another table, this method will raise a ProgrammingError.

truncate -- quickly empty database tables
-----------------------------------------

.. method:: DB.truncate(table, [restart], [cascade], [only])

    Empty a table or set of tables

    :param table: the name of the table(s)
    :type table: str, list or set
    :param bool restart: whether table sequences should be restarted
    :param bool cascade: whether referenced tables should also be truncated
    :param only: whether only parent tables should be truncated
    :type only: bool or list

This method quickly removes all rows from the given table or set
of tables.  It has the same effect as an unqualified DELETE on each
table, but since it does not actually scan the tables it is faster.
Furthermore, it reclaims disk space immediately, rather than requiring
a subsequent VACUUM operation. This is most useful on large tables.

If *restart* is set to `True`, sequences owned by columns of the truncated
table(s) are automatically restarted.  If *cascade* is set to `True`, it
also truncates all tables that have foreign-key references to any of
the named tables.  If the parameter *only* is not set to `True`, all the
descendant tables (if any) will also be truncated. Optionally, a ``*``
can be specified after the table name to explicitly indicate that
descendant tables are included.  If the parameter *table* is a list,
the parameter *only* can also be a list of corresponding boolean values.

.. versionadded:: 4.2

get_as_list/dict -- read a table as a list or dictionary
--------------------------------------------------------

.. method:: DB.get_as_list(table, [what], [where], [order], [limit], [offset], [scalar])

    Get a table as a list

    :param str table: the name of the table (the FROM clause)
    :param what: column(s) to be returned (the SELECT clause)
    :type what: str, list, tuple or None
    :param where: conditions(s) to be fulfilled (the WHERE clause)
    :type where: str, list, tuple or None
    :param order: column(s) to sort by (the ORDER BY clause)
    :type order: str, list, tuple, False or None
    :param int limit: maximum number of rows returned (the LIMIT clause)
    :param int offset: number of rows to be skipped (the OFFSET clause)
    :param bool scalar: whether only the first column shall be returned
    :returns: the content of the table as a list
    :rtype: list
    :raises TypeError: the table name has not been specified

This gets a convenient representation of the table as a list of named tuples
in Python.  You only need to pass the name of the table (or any other SQL
expression returning rows).  Note that by default this will return the full
content of the table which can be huge and overflow your memory.  However, you
can control the amount of data returned using the other optional parameters.

The parameter *what* can restrict the query to only return a subset of the
table columns.  The parameter *where* can restrict the query to only return a
subset of the table rows.  The specified SQL expressions all need to be
fulfilled for a row to get into the result.  The parameter *order* specifies
the ordering of the rows.  If no ordering is specified, the result will be
ordered by the primary key(s) or all columns if no primary key exists.
You can set *order* to *False* if you don't care about the ordering.
The parameters *limit* and *offset* specify the maximum number of rows
returned and a number of rows skipped over.

If you set the *scalar* option to *True*, then instead of the named tuples
you will get the first items of these tuples.  This is useful if the result
has only one column anyway.

.. method:: DB.get_as_dict(table, [keyname], [what], [where], [order], [limit], [offset], [scalar])

    Get a table as a dictionary

    :param str table: the name of the table (the FROM clause)
    :param keyname: column(s) to be used as key(s) of the dictionary
    :type keyname: str, list, tuple or None
    :param what: column(s) to be returned (the SELECT clause)
    :type what: str, list, tuple or None
    :param where: conditions(s) to be fulfilled (the WHERE clause)
    :type where: str, list, tuple or None
    :param order: column(s) to sort by (the ORDER BY clause)
    :type order: str, list, tuple, False or None
    :param int limit: maximum number of rows returned (the LIMIT clause)
    :param int offset: number of rows to be skipped (the OFFSET clause)
    :param bool scalar: whether only the first column shall be returned
    :returns: the content of the table as a list
    :rtype: dict or OrderedDict
    :raises TypeError: the table name has not been specified
    :raises KeyError: keyname(s) are invalid or not part of the result
    :raises pg.ProgrammingError: no keyname(s) and table has no primary key

This method is similar to :meth:`DB.get_as_list`, but returns the table as
a Python dict instead of a Python list, which can be even more convenient.
The primary key column(s) of the table will be used as the keys of the
dictionary, while the other column(s) will be the corresponding values.
The keys will be named tuples if the table has a composite primary key.
The rows will be also named tuples unless the *scalar* option has been set
to *True*.  With the optional parameter *keyname* you can specify a different
set of columns to be used as the keys of the dictionary.

If the Python version supports it, the dictionary will be an *OrderedDict*
using the order specified with the *order* parameter or the key column(s)
if not specified.  You can set *order* to *False* if you don't care about the
ordering.  In this case the returned dictionary will be an ordinary one.

escape_literal/identifier/string/bytea -- escape for SQL
--------------------------------------------------------

The following methods escape text or binary strings so that they can be
inserted directly into an SQL command.  Except for :meth:`DB.escape_byte`,
you don't need to call these methods for the strings passed as parameters
to :meth:`DB.query`.  You also don't need to call any of these methods
when storing data using :meth:`DB.insert` and similar.

.. method:: DB.escape_literal(string)

    Escape a string for use within SQL as a literal constant

    :param str string: the string that is to be escaped
    :returns: the escaped string
    :rtype: str

This method escapes a string for use within an SQL command. This is useful
when inserting data values as literal constants in SQL commands. Certain
characters (such as quotes and backslashes) must be escaped to prevent them
from being interpreted specially by the SQL parser.

.. versionadded:: 4.1

.. method:: DB.escape_identifier(string)

    Escape a string for use within SQL as an identifier

    :param str string: the string that is to be escaped
    :returns: the escaped string
    :rtype: str

This method escapes a string for use as an SQL identifier, such as a table,
column, or function name. This is useful when a user-supplied identifier
might contain special characters that would otherwise not be interpreted
as part of the identifier by the SQL parser, or when the identifier might
contain upper case characters whose case should be preserved.

.. versionadded:: 4.1

.. method:: DB.escape_string(string)

    Escape a string for use within SQL

    :param str string: the string that is to be escaped
    :returns: the escaped string
    :rtype: str

Similar to the module function :func:`pg.escape_string` with the same name,
but the behavior of this method is adjusted depending on the connection
properties (such as character encoding).

.. method:: DB.escape_bytea(datastring)

    Escape binary data for use within SQL as type ``bytea``

    :param str datastring: string containing the binary data that is to be escaped
    :returns: the escaped string
    :rtype: str

Similar to the module function :func:`pg.escape_bytea` with the same name,
but the behavior of this method is adjusted depending on the connection
properties (in particular, whether standard-conforming strings are enabled).

unescape_bytea -- unescape data retrieved from the database
-----------------------------------------------------------

.. method:: DB.unescape_bytea(string)

    Unescape ``bytea`` data that has been retrieved as text

    :param datastring: the ``bytea`` data string that has been retrieved as text
    :returns: byte string containing the binary data
    :rtype: bytes

Converts an escaped string representation of binary data stored as ``bytea``
into the raw byte string representing the binary data  -- this is the reverse
of :meth:`DB.escape_bytea`.  Since the :class:`Query` results will already
return unescaped byte strings, you normally don't have to use this method.

encode/decode_json -- encode and decode JSON data
-------------------------------------------------

The following methods can be used to encode end decode data in
`JSON <http://www.json.org/>`_ format.

.. method:: DB.encode_json(obj)

    Encode a Python object for use within SQL as type ``json`` or ``jsonb``

    :param obj: Python object that shall be encoded to JSON format
    :type obj: dict, list or None
    :returns: string representation of the Python object in JSON format
    :rtype: str

This method serializes a Python object into a JSON formatted string that can
be used within SQL.  You don't need to use this method on the data stored
with :meth:`DB.insert` and similar, only if you store the data directly as
part of an SQL command or parameter with :meth:`DB.query`.  This is the same
as the :func:`json.dumps` function from the standard library.

.. versionadded:: 5.0

.. method:: DB.decode_json(string)

    Decode ``json`` or ``jsonb`` data that has been retrieved as text

    :param string: JSON formatted string shall be decoded into a Python object
    :type string: str
    :returns: Python object representing the JSON formatted string
    :rtype: dict, list or None

This method deserializes a JSON formatted string retrieved as text from the
database to a Python object.  You normally don't need to use this method as
JSON data is automatically decoded by PyGreSQL.  If you don't want the data
to be decoded, then you can cast ``json`` or ``jsonb`` columns to ``text``
in PostgreSQL or you can set the decoding function to *None* or a different
function using :func:`pg.set_jsondecode`.  By default this is the same as
the :func:`json.dumps` function from the standard library.

.. versionadded:: 5.0

use_regtypes -- determine use of regular type names
---------------------------------------------------

.. method:: DB.use_regtypes([regtypes])

    Determine whether regular type names shall be used

    :param bool regtypes: if passed, set whether regular type names shall be used
    :returns: whether regular type names are used

The :meth:`DB.get_attnames` method can return either simplified "classic"
type names (the default) or more specific "regular" type names. Which kind
of type names is used can be changed by calling :meth:`DB.get_regtypes`.
If you pass a boolean, it sets whether regular type names shall be used.
The method can also be used to check through its return value whether
currently regular type names are used.

.. versionadded:: 4.1

notification_handler -- create a notification handler
-----------------------------------------------------

.. class:: DB.notification_handler(event, callback, [arg_dict], [timeout], [stop_event])

    Create a notification handler instance

    :param str event: the name of an event to listen for
    :param callback: a callback function
    :param dict arg_dict: an optional dictionary for passing arguments
    :param timeout: the time-out when waiting for notifications
    :type timeout: int, float or None
    :param str stop_event: an optional different name to be used as stop event

This method creates a :class:`pg.NotificationHandler` object using the
:class:`DB` connection as explained under :doc:`notification`.

.. versionadded:: 4.1.1

Attributes of the DB wrapper class
----------------------------------

.. attribute:: DB.db

    The wrapped :class:`Connection` object

You normally don't need this, since all of the members can be accessed
from the :class:`DB` wrapper class as well.

.. attribute:: DB.dbname

    The name of the database that the connection is using

.. attribute:: DB.dbtypes

    A dictionary with the various type names for the PostgreSQL types

This can be used for getting more information on the PostgreSQL database
types or changing the typecast functions used for the connection.  See the
description of the :class:`DbTypes` class for details.

.. versionadded:: 5.0

.. attribute:: DB.adapter

    A class with some helper functions for adapting parameters

This can be used for building queries with parameters.  You normally will
not need this, as you can use the :class:`DB.query_formatted` method.

.. versionadded:: 5.0
