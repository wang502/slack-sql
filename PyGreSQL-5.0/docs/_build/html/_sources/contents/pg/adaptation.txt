Remarks on Adaptation and Typecasting
=====================================

.. py:currentmodule:: pg

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
When you use the higher level methods of the classic :mod:`pg` module like
:meth:`DB.insert()` or :meth:`DB.update()`, you don't need to care about
adaptation of parameters, since all of this is happening automatically behind
the scenes.  You only need to consider this issue when creating SQL commands
manually and sending them to the database using the :meth:`DB.query` method.

Imagine you have created a user  login form that stores the login name as
*login* and the password as *passwd* and you now want to get the user
data for that user.  You may be tempted to execute a query like this::

    >>> db = pg.DB(...)
    >>> sql = "SELECT * FROM user_table WHERE login = '%s' AND passwd = '%s'"
    >>> db.query(sql % (login, passwd)).getresult()[0]

This seems to work at a first glance, but you will notice an error as soon as
you try to use a login name containing a single quote.  Even worse, this error
can be exploited through a so called "SQL injection", where an attacker inserts
malicious SQL statements into the query that you never intended to be executed.
For instance, with a login name something like ``' OR ''='`` the user could
easily log in and see the user data of another user in the database.

One solution for this problem would be to clean your input from "dangerous"
characters like the single quote, but this is tedious and it is likely that
you overlook something or break the application e.g. for users with names
like "D'Arcy".  A better solution is to use the escaping functions provided
by PostgreSQL which are available as methods on the :class:`DB` object::

    >>> login = "D'Arcy"
    >>> db.escape_string(login)
    "D''Arcy"

As you see, :meth:`DB.escape_string` has doubled the single quote which is
the right thing to do in SQL.  However, there are better ways of passing
parameters to the query, without having to manually escape them.  If you
pass the parameters as positional arguments to :meth:`DB.query`, then
PyGreSQL will send them to the database separately, without the need for
quoting them inside the SQL command, and without the problems inherent with
that process.  In this case you must put placeholders of the form ``$1``,
``$2`` etc. in the SQL command in place of the parameters that should go there.
For instance::

    >>> sql = "SELECT * FROM user_table WHERE login = $1 AND passwd = $2"
    >>> db.query(sql, login, passwd).getresult()[0]

That's much better.  So please always keep the following warning in mind:

.. warning::

  Remember to **never** insert parameters directly into your queries using
  the ``%`` operator.  Always pass the parameters separately.

If you like the ``%`` format specifications of Python better than the
placeholders used by PostgreSQL, there is still a way to use them, via the
:meth:`DB.query_formatted` method::

    >>> sql = "SELECT * FROM user_table WHERE login = %s AND passwd = %s"
    >>> db.query_formatted(sql, (login, passwd)).getresult()[0]

Note that we need to pass the parameters not as positional arguments here,
but as a single tuple.  Also note again that we did not use the ``%``
operator of Python to format the SQL string, we just used the ``%s`` format
specifications of Python and let PyGreSQL care about the formatting.
Even better, you can also pass the parameters as a dictionary if you use
the :meth:`DB.query_formatted` method::

    >>> sql = """SELECT * FROM user_table
    ...     WHERE login = %(login)s AND passwd = %(passwd)s"""
    >>> parameters = dict(login=login, passwd=passwd)
    >>> db.query_formatted(sql, parameters).getresult()[0]

Here is another example::

    >>> sql = "SELECT 'Hello, ' || %s || '!'"
    >>> db.query_formatted(sql, (login,)).getresult()[0]

You would think that the following even simpler example should work, too:

    >>> sql = "SELECT %s"
    >>> db.query_formatted(sql, (login,)).getresult()[0]
    ProgrammingError: Could not determine data type of parameter $1

The issue here is that :meth:`DB.query_formatted` by default still uses
PostgreSQL parameters, transforming the Python style ``%s`` placeholder
into a ``$1`` placeholder, and sending the login name separately from
the query.  In the query we looked at before, the concatenation with other
strings made it clear that it should be interpreted as a string. This simple
query however does not give PostgreSQL a clue what data type the ``$1``
placeholder stands for.

This is different when you are embedding the login name directly into the
query instead of passing it as parameter to PostgreSQL.  You can achieve this
by setting the *inline* parameter of :meth:`DB.query_formatted`, like so::

    >>> sql = "SELECT %s"
    >>> db.query_formatted(sql, (login,), inline=True).getresult()[0]

Another way of making this query work while still sending the parameters
separately is to simply cast the parameter values::

    >>> sql = "SELECT %s::text"
    >>> db.query_formatted(sql, (login,), inline=False).getresult()[0]

In real world examples you will rarely have to cast your parameters like that,
since in an INSERT statement or a WHERE clause comparing the parameter to a
table column the data type will be clear from the context.

When binding the parameters to a query, PyGreSQL does not only adapt the basic
types like ``int``, ``float``, ``bool`` and ``str``, but also tries to make
sense of Python lists and tuples.

Lists are adapted as PostgreSQL arrays::

    >>> params = dict(array=[[1, 2],[3, 4]])
    >>> db.query_formatted("SELECT %(array)s::int[]", params).getresult()[0][0]
    [[1, 2], [3, 4]]

Note that again we only need to cast the array parameter or use inline
parameters because this simple query does not provide enough context.
Also note that the query gives the value back as Python lists again.  This
is achieved by the typecasting mechanism explained in the next section.

Tuples are adapted as PostgreSQL composite types.  If you use inline paramters,
they can also be used with the ``IN`` syntax.

Let's think of a more real world example again where we create a table with a
composite type in PostgreSQL:

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

    >>> db.query_formatted("INSERT INTO on_hand VALUES (%(item)s, %(count)s)",
    ...     dict(item=inventory_item('fuzzy dice', 42, 1.99), count=1000))
    >>> db.query("SELECT * FROM on_hand").getresult()[0][0]
    Row(item=inventory_item(name='fuzzy dice', supplier_id=42,
            price=Decimal('1.99')), count=1000)

The :meth:`DB.insert` method provides a simpler way to achieve the same::

    >>> row = dict(item=inventory_item('fuzzy dice', 42, 1.99), count=1000)
    >>> db.insert('on_hand', row)
    {'count': 1000,  'item': inventory_item(name='fuzzy dice',
            supplier_id=42, price=Decimal('1.99'))}

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
will get an error.  This is because PyGreSQL tries to pass the string
representation of the object as a parameter to PostgreSQL, but this is just a
human readable string and not useful for PostgreSQL to build a composite type.
However, it is possible to make such custom classes adapt themselves to
PostgreSQL by adding a "magic" method with the name ``__pg_str__``, like so::

    >>> class InventoryItem:
    ...
    ...     ...
    ...
    ...     def __str__(self):
    ...         return '%s (from %s, at $%s)' % (
    ...             self.name, self.supplier_id, self.price)
    ...
    ...     def __pg_str__(self, typ):
    ...         return (self.name, self.supplier_id, self.price)

Now you can insert class instances the same way as you insert named tuples.
You can even make these objects adapt to different types in different ways::

    >>> class InventoryItem:
    ...
    ...     ...
    ...
    ...     def __pg_str__(self, typ):
    ...         if typ == 'text':
    ...             return str(self)
    ...        return (self.name, self.supplier_id, self.price)
    ...
    >>> db.query("ALTER TABLE on_hand ADD COLUMN remark varchar")
    >>> item=InventoryItem('fuzzy dice', 42, 1.99)
    >>> row = dict(item=item, remark=item, count=1000)
    >>> db.insert('on_hand', row)
    {'count': 1000, 'item': inventory_item(name='fuzzy dice',
        supplier_id=42, price=Decimal('1.99')),
        'remark': 'fuzzy dice (from 42, at $1.99)'}

There is also another "magic" method ``__pg_repr__`` which does not take the
*typ* parameter.  That method is used instead of ``__pg_str__`` when passing
parameters inline.  You must be more careful when using ``__pg_repr__``,
because it must return a properly escaped string that can be put literally
inside the SQL.  The only exception is when you return a tuple or list,
because these will be adapted and properly escaped by PyGreSQL again.

Typecasting to Python
---------------------

As you noticed, PyGreSQL automatically converted the PostgreSQL data to
suitable Python objects when returning values via the :meth:`DB.get()`,
:meth:`Query.getresult()` and similar methods.  This is done by the use
of built-in typecast functions.

If you want to use different typecast functions or add your own if no
built-in typecast function is available, then this is possible using
the :func:`set_typecast` function.  With the :func:`get_typecast` function
you can check which function is currently set.  If no typecast function
is set, then PyGreSQL will return the raw strings from the database.

For instance, you will find that PyGreSQL uses the normal ``int`` function
to cast PostgreSQL ``int4`` type values to Python::

    >>> pg.get_typecast('int4')
    int

In the classic PyGreSQL module, the typecasting for these basic types is
always done internally by the C extension module for performance reasons.
We can set a different typecast function for ``int4``, but it will not
become effective, the C module continues to use its internal typecasting.

However, we can add new typecast functions for the database types that are
not supported by the C modul. Fore example, we can create a typecast function
that casts items of the composite PostgreSQL type used as example in the
previous section to instances of the corresponding Python class.

To do this, at first we get the default typecast function that PyGreSQL has
created for the current :class:`DB` connection.  This default function casts
composite types to named tuples, as we have seen in the section before.
We can grab it from the :attr:`DB.dbtypes` object as follows::

    >>> cast_tuple = db.dbtypes.get_typecast('inventory_item')

Now we can create a new typecast function that converts the tuple to
an instance of our custom class::

    >>> cast_item = lambda value: InventoryItem(*cast_tuple(value))

Finally, we set this typecast function, either globally with
:func:`set_typecast`, or locally for the current connection like this::

    >>> db.dbtypes.set_typecast('inventory_item', cast_item)

Now we can get instances of our custom class directly from the database::

    >>> item = db.query("SELECT * FROM on_hand").getresult()[0][0]
    >>> str(item)
    'fuzzy dice (from 42, at $1.99)'

Note that some of the typecast functions used by the C module are configurable
with separate module level functions, such as :meth:`set_decimal`,
:meth:`set_bool` or :meth:`set_jsondecode`.  You need to use these instead of
:meth:`set_typecast` if you want to change the behavior of the C module.

Also note that after changing global typecast functions with
:meth:`set_typecast`, you may need to run ``db.dbtypes.reset_typecast()``
to make these changes effective on connections that were already open.

As one last example, let us try to typecast the geometric data type ``circle``
of PostgreSQL into a `SymPy <http://www.sympy.org>`_ ``Circle`` object.  Let's
assume we have created and populated a table with two circles, like so:

.. code-block:: sql

    CREATE TABLE circle (
        name varchar(8) primary key, circle circle);
    INSERT INTO circle VALUES ('C1', '<(2, 3), 3>');
    INSERT INTO circle VALUES ('C2', '<(1, -1), 4>');

With PostgreSQL we can easily calculate that these two circles overlap::

    >>> q = db.query("""SELECT c1.circle && c2.circle
    ...     FROM circle c1, circle c2
    ...     WHERE c1.name = 'C1' AND c2.name = 'C2'""")
    >>> q.getresult()[0][0]
    True

However, calculating the intersection points between the two circles using the
``#`` operator does not work (at least not as of PostgreSQL version 9.5).
So let' resort to SymPy to find out.  To ease importing circles from
PostgreSQL to SymPy, we create and register the following typecast function::

    >>> from sympy import Point, Circle
    >>>
    >>> def cast_circle(s):
    ...     p, r = s[1:-1].split(',')
    ...     p = p[1:-1].split(',')
    ...     return Circle(Point(float(p[0]), float(p[1])), float(r))
    ...
    >>> pg.set_typecast('circle', cast_circle)

Now we can import the circles in the table into Python simply using::

    >>> circle = db.get_as_dict('circle', scalar=True)

The result is a dictionary mapping circle names to SymPy ``Circle`` objects.
We can verify that the circles have been imported correctly:

    >>> circle['C1']
    Circle(Point(2, 3), 3.0)
    >>> circle['C2']
    Circle(Point(1, -1), 4.0)

Finally we can find the exact intersection points with SymPy:

    >>> circle['C1'].intersection(circle['C2'])
    [Point(29/17 + 64564173230121*sqrt(17)/100000000000000,
        -80705216537651*sqrt(17)/500000000000000 + 31/17),
     Point(-64564173230121*sqrt(17)/100000000000000 + 29/17,
        80705216537651*sqrt(17)/500000000000000 + 31/17)]
