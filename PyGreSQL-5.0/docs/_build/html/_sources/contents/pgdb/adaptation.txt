Remarks on Adaptation and Typecasting
=====================================

.. py:currentmodule:: pgdb

Both PostgreSQL and Python have the concept of data types, but there
are of course differences between the two type systems.  Therefore PyGreSQL
needs to adapt Python objects to the representation required by PostgreSQL
when passing values as query parameters, and it needs to typecast the
representation of PostgreSQL data types returned by database queries to
Python objects.  Here are some explanations about how this works in
detail in case you want to better understand or change the default
behavior of PyGreSQL.

Supported data types
--------------------

The following automatic data type conversions are supported by PyGreSQL
out of the box.  If you need other automatic type conversions or want to
change the default conversions, you can achieve this by using the methods
explained in the next two sections.

================================== ==================
PostgreSQL                          Python
================================== ==================
char, bpchar, name, text, varchar  str
bool                               bool
bytea                              bytes
int2, int4, int8, oid, serial      int [#int8]_
int2vector                         list of int
float4, float8                     float
numeric, money                     Decimal
date                               datetime.date
time, timetz                       datetime.time
timestamp, timestamptz             datetime.datetime
interval                           datetime.timedelta
hstore                             dict
json, jsonb                        list or dict
uuid                               uuid.UUID
array                              list
record                             tuple
================================== ==================

.. note::

    Elements of arrays and records will also be converted accordingly.

    .. [#int8] int8 is converted to long in Python 2

Adaptation of parameters
------------------------

PyGreSQL knows how to adapt the common Python types to get a suitable
representation of their values for PostgreSQL when you pass parameters
to a query. For example::

    >>> con = pgdb.connect(...)
    >>> cur = con.cursor()
    >>> parameters = (144, 3.75, 'hello', None)
    >>> tuple(cur.execute('SELECT %s, %s, %s, %s', parameters).fetchone()
    (144, Decimal('3.75'), 'hello', None)

This is the result we can expect, so obviously PyGreSQL has adapted the
parameters and sent the following query to PostgreSQL:

.. code-block:: sql

    SELECT 144, 3.75, 'hello', NULL

Note the subtle, but important detail that even though the SQL string passed
to :meth:`cur.execute` contains conversion specifications normally used in
Python with the ``%`` operator for formatting strings, we didn't use the ``%``
operator to format the parameters, but passed them as the second argument to
:meth:`cur.execute`.  I.e. we **didn't** write the following::

>>> tuple(cur.execute('SELECT %s, %s, %s, %s' % parameters).fetchone()

If we had done this, PostgreSQL would have complained because the parameters
were not adapted.  Particularly, there would be no quotes around the value
``'hello'``, so PostgreSQL would have interpreted this as a database column,
which would have caused a :exc:`ProgrammingError`.  Also, the Python value
``None`` would have been included in the SQL command literally, instead of
being converted to the SQL keyword ``NULL``, which would have been another
reason for PostgreSQL to complain about our bad query:

.. code-block:: sql

    SELECT 144, 3.75, hello, None

Even worse, building queries with the use of the ``%`` operator makes us
vulnerable to so called "SQL injection" exploits, where an attacker inserts
malicious SQL statements into our queries that we never intended to be
executed.  We could avoid this by carefully quoting and escaping the
parameters, but this would be tedious and if we overlook something, our
code will still be vulnerable.  So please don't do this.  This cannot be
emphasized enough, because it is such a subtle difference and using the ``%``
operator looks so natural:

.. warning::

  Remember to **never** insert parameters directly into your queries using
  the ``%`` operator.  Always pass the parameters separately.

The good thing is that by letting PyGreSQL do the work for you, you can treat
all your parameters equally and don't need to ponder where you need to put
quotes or need to escape strings.  You can and should also always use the
general ``%s`` specification instead of e.g. using ``%d`` for integers.
Actually, to avoid mistakes and make it easier to insert parameters at more
than one location, you can and should use named specifications, like this::

    >>> params = dict(greeting='Hello', name='HAL')
    >>> sql = """SELECT %(greeting)s || ', ' || %(name)s
    ...    || '. Do you read me, ' || %(name)s || '?'"""
    >>> cur.execute(sql, params).fetchone()[0]
    'Hello, HAL. Do you read me, HAL?'

PyGreSQL does not only adapt the basic types like ``int``, ``float``,
``bool`` and ``str``, but also tries to make sense of Python lists and tuples.

Lists are adapted as PostgreSQL arrays::

    >>> params = dict(array=[[1, 2],[3, 4]])
    >>> cur.execute("SELECT %(array)s", params).fetchone()[0]
    [[1, 2], [3, 4]]

Note that the query gives the value back as Python lists again.  This
is achieved by the typecasting mechanism explained in the next section.
The query that was actually executed was this:

.. code-block:: sql

    SELECT ARRAY[[1,2],[3,4]]

Again, if we had inserted the list using the ``%`` operator without adaptation,
the ``ARRAY`` keyword would have been missing in the query.

Tuples are adapted as PostgreSQL composite types::

    >>> params = dict(record=('Bond', 'James'))
    >>> cur.execute("SELECT %(record)s", params).fetchone()[0]
    ('Bond', 'James')

You can also use this feature with the ``IN`` syntax of SQL::

    >>> params = dict(what='needle', where=('needle', 'haystack'))
    >>> cur.execute("SELECT %(what)s IN %(where)s", params).fetchone()[0]
    True

Sometimes a Python type can be ambiguous. For instance, you might want
to insert a Python list not into an array column, but into a JSON column.
Or you want to interpret a string as a date and insert it into a DATE column.
In this case you can give PyGreSQL a hint by using :ref:`type_constructors`::

    >>> cur.execute("CREATE TABLE json_data (data json, created date)")
    >>> params = dict(
    ...     data=pgdb.Json([1, 2, 3]), created=pgdb.Date(2016, 1, 29))
    >>> sql = ("INSERT INTO json_data VALUES (%(data)s, %(created)s)")
    >>> cur.execute(sql, params)
    >>> cur.execute("SELECT * FROM json_data").fetchone()
    Row(data=[1, 2, 3], created='2016-01-29')

Let's think of another example where we create a table with a composite
type in PostgreSQL:

.. code-block:: sql

    CREATE TABLE on_hand (
        item      inventory_item,
        count     integer)

We assume the composite type ``inventory_item`` has been created like this:

.. code-block:: sql

    CREATE TYPE inventory_item AS (
        name            text,
        supplier_id     integer,
        price           numeric)

In Python we can use a named tuple as an equivalent to this PostgreSQL type::

    >>> from collections import namedtuple
    >>> inventory_item = namedtuple(
    ...     'inventory_item', ['name', 'supplier_id', 'price'])

Using the automatic adaptation of Python tuples, an item can now be
inserted into the database and then read back as follows::

    >>> cur.execute("INSERT INTO on_hand VALUES (%(item)s, %(count)s)",
    ...     dict(item=inventory_item('fuzzy dice', 42, 1.99), count=1000))
    >>> cur.execute("SELECT * FROM on_hand").fetchone()
    Row(item=inventory_item(name='fuzzy dice', supplier_id=42,
            price=Decimal('1.99')), count=1000)

However, we may not want to use named tuples, but custom Python classes
to hold our values, like this one::

    >>> class InventoryItem:
    ...
    ...     def __init__(self, name, supplier_id, price):
    ...         self.name = name
    ...         self.supplier_id = supplier_id
    ...         self.price = price
    ...
    ...     def __str__(self):
    ...         return '%s (from %s, at $%s)' % (
    ...             self.name, self.supplier_id, self.price)

But when we try to insert an instance of this class in the same way, we
will get an error::

    >>> cur.execute("INSERT INTO on_hand VALUES (%(item)s, %(count)s)",
    ...     dict(item=InventoryItem('fuzzy dice', 42, 1.99), count=1000))
    InterfaceError: Do not know how to adapt type <class 'InventoryItem'>

While PyGreSQL knows how to adapt tuples, it does not know what to make out
of our custom class.  To simply convert the object to a string using the
``str`` function is not a solution, since this yields a human readable string
that is not useful for PostgreSQL.  However, it is possible to make such
custom classes adapt themselves to PostgreSQL by adding a "magic" method
with the name ``__pg_repr__``, like this::

  >>> class InventoryItem:
    ...
    ...     ...
    ...
    ...     def __str__(self):
    ...         return '%s (from %s, at $%s)' % (
    ...             self.name, self.supplier_id, self.price)
    ...
    ...     def __pg_repr__(self):
    ...         return (self.name, self.supplier_id, self.price)

Now you can insert class instances the same way as you insert named tuples.

Note that PyGreSQL adapts the result of ``__pg_repr__`` again if it is a
tuple or a list.  Otherwise, it must be a properly escaped string.

Typecasting to Python
---------------------

As you noticed, PyGreSQL automatically converted the PostgreSQL data to
suitable Python objects when returning values via one of the "fetch" methods
of a cursor.  This is done by the use of built-in typecast functions.

If you want to use different typecast functions or add your own if no
built-in typecast function is available, then this is possible using
the :func:`set_typecast` function.  With the :func:`get_typecast` function
you can check which function is currently set, and :func:`reset_typecast`
allows you to reset the typecast function to its default.  If no typecast
function is set, then PyGreSQL will return the raw strings from the database.

For instance, you will find that PyGreSQL uses the normal ``int`` function
to cast PostgreSQL ``int4`` type values to Python::

    >>> pgdb.get_typecast('int4')
    int

You can change this to return float values instead::

    >>> pgdb.set_typecast('int4', float)
    >>> con = pgdb.connect(...)
    >>> cur = con.cursor()
    >>> cur.execute('select 42::int4').fetchone()[0]
    42.0

Note that the connections cache the typecast functions, so you may need to
reopen the database connection, or reset the cache of the connection to
make this effective, using the following command::

    >>> con.type_cache.reset_typecast()

The :class:`TypeCache` of the connection can also be used to change typecast
functions locally for one database connection only.

As a more useful example, we can create a typecast function that casts
items of the composite type used as example in the previous section
to instances of the corresponding Python class::

    >>> con.type_cache.reset_typecast()
    >>> cast_tuple = con.type_cache.get_typecast('inventory_item')
    >>> cast_item = lambda value: InventoryItem(*cast_tuple(value))
    >>> con.type_cache.set_typecast('inventory_item', cast_item)
    >>> str(cur.execute("SELECT * FROM on_hand").fetchone()[0])
    'fuzzy dice (from 42, at $1.99)'

As you saw in the last section you, PyGreSQL also has a typecast function
for JSON, which is the default JSON decoder from the standard library.
Let's assume we want to use a slight variation of that decoder in which
every integer in JSON is converted to a float in Python. This can be
accomplished as follows::

    >>> from json import loads
    >>> cast_json = lambda v: loads(v, parse_int=float)
    >>> pgdb.set_typecast('json', cast_json)
    >>> cur.execute("SELECT data FROM json_data").fetchone()[0]
    [1.0, 2.0, 3.0]

Note again that you may need to run ``con.type_cache.reset_typecast()`` to
make this effective.  Also note that the two types ``json`` and ``jsonb`` have
their own typecast functions, so if you use ``jsonb`` instead of ``json``, you
need to use this type name when setting the typecast function::

    >>> pgdb.set_typecast('jsonb', cast_json)

As one last example, let us try to typecast the geometric data type ``circle``
of PostgreSQL into a `SymPy <http://www.sympy.org>`_ ``Circle`` object.  Let's
assume we have created and populated a table with two circles, like so:

.. code-block:: sql

    CREATE TABLE circle (
        name varchar(8) primary key, circle circle);
    INSERT INTO circle VALUES ('C1', '<(2, 3), 3>');
    INSERT INTO circle VALUES ('C2', '<(1, -1), 4>');

With PostgreSQL we can easily calculate that these two circles overlap::

    >>> con.cursor().execute("""SELECT c1.circle && c2.circle
    ...     FROM circle c1, circle c2
    ...     WHERE c1.name = 'C1' AND c2.name = 'C2'""").fetchone()[0]
    True

However, calculating the intersection points between the two circles using the
``#`` operator does not work (at least not as of PostgreSQL version 9.5).
So let' resort to SymPy to find out.  To ease importing circles from
PostgreSQL to SymPy, we create and register the following typecast function::

    >>> from sympy import Point, Circle
    >>>
    >>> def cast_circle(s):
    ...     p, r = s[1:-1].rsplit(',', 1)
    ...     p = p[1:-1].split(',')
    ...     return Circle(Point(float(p[0]), float(p[1])), float(r))
    ...
    >>> pgdb.set_typecast('circle', cast_circle)

Now we can import the circles in the table into Python quite easily::

    >>> circle = {c.name: c.circle for c in con.cursor().execute(
    ...     "SELECT * FROM circle").fetchall()}

The result is a dictionary mapping circle names to SymPy ``Circle`` objects.
We can verify that the circles have been imported correctly:

    >>> circle
    {'C1': Circle(Point(2, 3), 3.0),
     'C2': Circle(Point(1, -1), 4.0)}

Finally we can find the exact intersection points with SymPy:

    >>> circle['C1'].intersection(circle['C2'])
    [Point(29/17 + 64564173230121*sqrt(17)/100000000000000,
        -80705216537651*sqrt(17)/500000000000000 + 31/17),
     Point(-64564173230121*sqrt(17)/100000000000000 + 29/17,
        80705216537651*sqrt(17)/500000000000000 + 31/17)]
