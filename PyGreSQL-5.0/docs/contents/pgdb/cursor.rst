Cursor -- The cursor object
===========================

.. py:currentmodule:: pgdb

.. class:: Cursor

These objects represent a database cursor, which is used to manage the context
of a fetch operation. Cursors created from the same connection are not
isolated, i.e., any changes done to the database by a cursor are immediately
visible by the other cursors. Cursors created from different connections can
or can not be isolated, depending on the level of transaction isolation.
The default PostgreSQL transaction isolation level is "read committed".

Cursor objects respond to the following methods and attributes.

Note that ``Cursor`` objects also implement both the iterator and the
context manager protocol, i.e. you can iterate over them and you can use them
in a ``with`` statement.

description -- details regarding the result columns
---------------------------------------------------

.. attribute:: Cursor.description

    This read-only attribute is a sequence of 7-item named tuples.

    Each of these named tuples contains information describing
    one result column:

        - *name*
        - *type_code*
        - *display_size*
        - *internal_size*
        - *precision*
        - *scale*
        - *null_ok*

    The values for *precision* and *scale* are only set for numeric types.
    The values for *display_size* and *null_ok* are always ``None``.

    This attribute will be ``None`` for operations that do not return rows
    or if the cursor has not had an operation invoked via the
    :meth:`Cursor.execute` or :meth:`Cursor.executemany` method yet.

.. versionchanged:: 5.0
    Before version 5.0, this attribute was an ordinary tuple.

rowcount -- number of rows of the result
----------------------------------------

.. attribute:: Cursor.rowcount

    This read-only attribute specifies the number of rows that the last
    :meth:`Cursor.execute` or :meth:`Cursor.executemany` call produced
    (for DQL statements like SELECT) or affected (for DML statements like
    UPDATE or INSERT). It is also set by the :meth:`Cursor.copy_from` and
    :meth':`Cursor.copy_to` methods. The attribute is -1 in case no such
    method call has been performed on the cursor or the rowcount of the
    last operation cannot be determined by the interface.

close -- close the cursor
-------------------------

.. method:: Cursor.close()

    Close the cursor now (rather than whenever it is deleted)

    :rtype: None

The cursor will be unusable from this point forward; an :exc:`Error`
(or subclass) exception will be raised if any operation is attempted
with the cursor.

execute -- execute a database operation
---------------------------------------

.. method:: Cursor.execute(operation, [parameters])

    Prepare and execute a database operation (query or command)

    :param str operation: the database operation
    :param parameters: a sequence or mapping of parameters
    :returns: the cursor, so you can chain commands

Parameters may be provided as sequence or mapping and will be bound to
variables in the operation. Variables are specified using Python extended
format codes, e.g. ``" ... WHERE name=%(name)s"``.

A reference to the operation will be retained by the cursor. If the same
operation object is passed in again, then the cursor can optimize its behavior.
This is most effective for algorithms where the same operation is used,
but different parameters are bound to it (many times).

The parameters may also be specified as list of tuples to e.g. insert multiple
rows in a single operation, but this kind of usage is deprecated:
:meth:`Cursor.executemany` should be used instead.

Note that in case this method raises a :exc:`DatabaseError`, you can get
information about the error condition that has occurred by introspecting
its :attr:`DatabaseError.sqlstate` attribute, which will be the ``SQLSTATE``
error code associated with the error.  Applications that need to know which
error condition has occurred should usually test the error code, rather than
looking at the textual error message.

executemany -- execute many similar database operations
-------------------------------------------------------

.. method:: Cursor.executemany(operation, [seq_of_parameters])

    Prepare and execute many similar database operations (queries or commands)

    :param str operation: the database operation
    :param seq_of_parameters: a sequence or mapping of parameter tuples or mappings
    :returns: the cursor, so you can chain commands

Prepare a database operation (query or command) and then execute it against
all parameter tuples or mappings found in the sequence *seq_of_parameters*.

Parameters are bounded to the query using Python extended format codes,
e.g. ``" ... WHERE name=%(name)s"``.

callproc -- Call a stored procedure
-----------------------------------

.. method:: Cursor.callproc(self, procname, [parameters]):

    Call a stored database procedure with the given name

    :param str procname: the name of the database function
    :param parameters: a sequence of parameters (can be empty or omitted)

This method calls a stored procedure (function) in the PostgreSQL database.

The sequence of parameters must contain one entry for each input argument
that the function expects. The result of the call is the same as this input
sequence; replacement of output and input/output parameters in the return
value is currently not supported.

The function may also provide a result set as output. These can be requested
through the standard fetch methods of the cursor.

.. versionadded:: 5.0

fetchone -- fetch next row of the query result
----------------------------------------------

.. method:: Cursor.fetchone()

    Fetch the next row of a query result set

    :returns: the next row of the query result set
    :rtype: named tuple or None

Fetch the next row of a query result set, returning a single named tuple,
or ``None`` when no more data is available. The field names of the named
tuple are the same as the column names of the database query as long as
they are valid Python identifiers.

An :exc:`Error` (or subclass) exception is raised if the previous call to
:meth:`Cursor.execute` or :meth:`Cursor.executemany` did not produce
any result set or no call was issued yet.

.. versionchanged:: 5.0
    Before version 5.0, this method returned ordinary tuples.

fetchmany -- fetch next set of rows of the query result
-------------------------------------------------------

.. method:: Cursor.fetchmany([size=None], [keep=False])

    Fetch the next set of rows of a query result

    :param size: the number of rows to be fetched
    :type size: int or None
    :param keep: if set to true, will keep the passed arraysize
    :tpye keep: bool
    :returns: the next set of rows of the query result
    :rtype: list of named tuples

Fetch the next set of rows of a query result, returning a list of named
tuples. An empty sequence is returned when no more rows are available.
The field names of the named tuple are the same as the column names of
the database query as long as they are valid Python identifiers.

The number of rows to fetch per call is specified by the *size* parameter.
If it is not given, the cursor's :attr:`arraysize` determines the number of
rows to be fetched. If you set the *keep* parameter to True, this is kept as
new :attr:`arraysize`.

The method tries to fetch as many rows as indicated by the *size* parameter.
If this is not possible due to the specified number of rows not being
available, fewer rows may be returned.

An :exc:`Error` (or subclass) exception is raised if the previous call to
:meth:`Cursor.execute` or :meth:`Cursor.executemany` did not produce
any result set or no call was issued yet.

Note there are performance considerations involved with the *size* parameter.
For optimal performance, it is usually best to use the :attr:`arraysize`
attribute. If the *size* parameter is used, then it is best for it to retain
the same value from one :meth:`Cursor.fetchmany` call to the next.

.. versionchanged:: 5.0
    Before version 5.0, this method returned ordinary tuples.

fetchall -- fetch all rows of the query result
----------------------------------------------

.. method:: Cursor.fetchall()

    Fetch all (remaining) rows of a query result

    :returns: the set of all rows of the query result
    :rtype: list of named tuples

Fetch all (remaining) rows of a query result, returning them as list of
named tuples. The field names of the named tuple are the same as the column
names of the database query as long as they are valid Python identifiers.

Note that the cursor's :attr:`arraysize` attribute can affect the performance
of this operation.

.. versionchanged:: 5.0
    Before version 5.0, this method returned ordinary tuples.

arraysize - the number of rows to fetch at a time
-------------------------------------------------

.. attribute:: Cursor.arraysize

    The number of rows to fetch at a time

This read/write attribute specifies the number of rows to fetch at a time with
:meth:`Cursor.fetchmany`. It defaults to 1, meaning to fetch a single row
at a time.

Methods and attributes that are not part of the standard
--------------------------------------------------------

.. note::

    The following methods and attributes are not part of the DB-API 2 standard.

.. method:: Cursor.copy_from(stream, table, [format], [sep], [null], [size], [columns])

    Copy data from an input stream to the specified table

    :param stream: the input stream
        (must be a file-like object, a string or an iterable returning strings)
    :param str table: the name of a database table
    :param str format: the format of the data in the input stream,
        can be ``'text'`` (the default), ``'csv'``, or ``'binary'``
    :param str sep: a single character separator
        (the default is ``'\t'`` for text and ``','`` for csv)
    :param str null: the textual representation of the ``NULL`` value,
        can also be an empty string (the default is ``'\\N'``)
    :param int size: the size of the buffer when reading file-like objects
    :param list column: an optional list of column names
    :returns: the cursor, so you can chain commands

    :raises TypeError: parameters with wrong types
    :raises ValueError: invalid parameters
    :raises IOError: error when executing the copy operation

This method can be used to copy data from an input stream on the client side
to a database table on the server side using the ``COPY FROM`` command.
The input stream can be provided in form of a file-like object (which must
have a ``read()`` method), a string, or an iterable returning one row or
multiple rows of input data on each iteration.

The format must be text, csv or binary. The sep option sets the column
separator (delimiter) used in the non binary formats. The null option sets
the textual representation of ``NULL`` in the input.

The size option sets the size of the buffer used when reading data from
file-like objects.

The copy operation can be restricted to a subset of columns. If no columns are
specified, all of them will be copied.

.. versionadded:: 5.0

.. method:: Cursor.copy_to(stream, table, [format], [sep], [null], [decode], [columns])

    Copy data from the specified table to an output stream

    :param stream: the output stream (must be a file-like object or ``None``)
    :param str table: the name of a database table or a ``SELECT`` query
    :param str format: the format of the data in the input stream,
        can be ``'text'`` (the default), ``'csv'``, or ``'binary'``
    :param str sep: a single character separator
        (the default is ``'\t'`` for text and ``','`` for csv)
    :param str null: the textual representation of the ``NULL`` value,
        can also be an empty string (the default is ``'\\N'``)
    :param bool decode: whether decoded strings shall be returned
        for non-binary formats (the default is True in Python 3)
    :param list column: an optional list of column names
    :returns: a generator if stream is set to ``None``, otherwise the cursor

    :raises TypeError: parameters with wrong types
    :raises ValueError: invalid parameters
    :raises IOError: error when executing the copy operation

This method can be used to copy data from a database table on the server side
to an output stream on the client side using the ``COPY TO`` command.

The output stream can be provided in form of a file-like object (which must
have a ``write()`` method). Alternatively, if ``None`` is passed as the
output stream, the method will return a generator yielding one row of output
data on each iteration.

Output will be returned as byte strings unless you set decode to true.

Note that you can also use a ``SELECT`` query instead of the table name.

The format must be text, csv or binary. The sep option sets the column
separator (delimiter) used in the non binary formats. The null option sets
the textual representation of ``NULL`` in the output.

The copy operation can be restricted to a subset of columns. If no columns are
specified, all of them will be copied.

.. versionadded:: 5.0

.. method:: Cursor.row_factory(row)

    Process rows before they are returned

    :param list row: the currently processed row of the result set
    :returns: the transformed row that the fetch methods shall return

This method is used for processing result rows before returning them through
one of the fetch methods. By default, rows are returned as named tuples.
You can overwrite this method with a custom row factory if you want to
return the rows as different kids of objects. This same row factory will then
be used for all result sets. If you overwrite this method, the method
:meth:`Cursor.build_row_factory` for creating row factories dynamically
will be ignored.

Note that named tuples are very efficient and can be easily converted to
dicts (even OrderedDicts) by calling ``row._asdict()``. If you still want
to return rows as dicts, you can create a custom cursor class like this::

    class DictCursor(pgdb.Cursor):

        def row_factory(self, row):
            return {key: value for key, value in zip(self.colnames, row)}

    cur = DictCursor(con)  # get one DictCursor instance or
    con.cursor_type = DictCursor  # always use DictCursor instances

.. versionadded:: 4.0

.. method:: Cursor.build_row_factory()

    Build a row factory based on the current description

    :returns: callable with the signature of :meth:`Cursor.row_factory`

This method returns row factories for creating named tuples. It is called
whenever a new result set is created, and :attr:`Cursor.row_factory` is
then assigned the return value of this method. You can overwrite this method
with a custom row factory builder if you want to use different row factories
for different result sets. Otherwise, you can also simply overwrite the
:meth:`Cursor.row_factory` method. This method will then be ignored.

The default implementation that delivers rows as named tuples essentially
looks like this::

    def build_row_factory(self):
        return namedtuple('Row', self.colnames, rename=True)._make

.. versionadded:: 5.0

.. attribute:: Cursor.colnames

    The list of columns names of the current result set

The values in this list are the same values as the *name* elements
in the :attr:`Cursor.description` attribute. Always use the latter
if you want to remain standard compliant.

.. versionadded:: 5.0

.. attribute:: Cursor.coltypes

    The list of columns types of the current result set

The values in this list are the same values as the *type_code* elements
in the :attr:`Cursor.description` attribute. Always use the latter
if you want to remain standard compliant.

.. versionadded:: 5.0
