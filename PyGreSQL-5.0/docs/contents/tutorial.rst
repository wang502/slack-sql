First Steps with PyGreSQL
=========================

In this small tutorial we show you the basic operations you can perform
with both flavors of the PyGreSQL interface. Please choose your flavor:

.. contents::
    :local:


First Steps with the classic PyGreSQL Interface
-----------------------------------------------

.. py:currentmodule:: pg

The first thing you need to do anything with your PostgreSQL database is
to create a database connection.

To do this, simply import the :class:`DB` wrapper class and create an
instance of it, passing the necessary connection parameters, like this::

    >>> from pg import DB
    >>> db = DB(dbname='testdb', host='pgserver', port=5432,
    ...     user='scott', passwd='tiger')

You can omit one or even all parameters if you want to use their default
values. PostgreSQL will use the name of the current operating system user
as the login and the database name, and will try to connect to the local
host on port 5432 if nothing else is specified.

The `db` object has all methods of the lower-level :class:`Connection` class
plus some more convenience methods provided by the :class:`DB` wrapper.

You can now execute database queries using the :meth:`DB.query` method::

    >>> db.query("create table fruits(id serial primary key, name varchar)")

You can list all database tables with the :meth:`DB.get_tables` method::

    >>> db.get_tables()
    ['public.fruits']

To get the attributes of the *fruits* table, use :meth:`DB.get_attnames`::

    >>> db.get_attnames('fruits')
    {'id': 'int', 'name': 'text'}

Verify that you can insert into the newly created *fruits* table:

    >>> db.has_table_privilege('fruits', 'insert')
    True

You can insert a new row into the table using the :meth:`DB.insert` method,
for example::

    >>> db.insert('fruits', name='apple')
    {'name': 'apple', 'id': 1}

Note how this method returns the full row as a dictionary including its *id*
column that has been generated automatically by a database sequence. You can
also pass a dictionary to the :meth:`DB.insert` method instead of or in
addition to using keyword arguments.

Let's add another row to the table:

   >>> banana = db.insert('fruits', name='banana')

Or, you can add a whole bunch of fruits at the same time using the
:meth:`Connection.inserttable` method.  Note that this method uses the COPY
command of PostgreSQL to insert all data in one batch operation, which is much
faster than sending many individual INSERT commands::

    >>> more_fruits = 'cherimaya durian eggfruit fig grapefruit'.split()
    >>> data = list(enumerate(more_fruits, start=3))
    >>> db.inserttable('fruits', data)

We can now query the database for all rows that have been inserted into
the *fruits* table::

    >>> print(db.query('select * from fruits'))
    id|   name
    --+----------
     1|apple
     2|banana
     3|cherimaya
     4|durian
     5|eggfruit
     6|fig
     7|grapefruit
    (7 rows)

Instead of simply printing the :class:`Query` instance that has been returned
by this query, we can also request the data as list of tuples::

    >>> q = db.query('select * from fruits')
    >>> q.getresult()
    ... [(1, 'apple'), ..., (7, 'grapefruit')]

Instead of a list of tuples, we can also request a list of dicts::

    >>> q.dictresult()
    [{'id': 1, 'name': 'apple'}, ..., {'id': 7, 'name': 'grapefruit'}]

You can also return the rows as named tuples::

    >>> rows = q.namedresult()
    >>> rows[3].name
    'durian'

Using the method :meth:`DB.get_as_dict`, you can easily import the whole table
into a Python dictionary mapping the primary key *id* to the *name*::

    >>> db.get_as_dict('fruits', scalar=True)
    OrderedDict([(1, 'apple'),
                 (2, 'banana'),
                 (3, 'cherimaya'),
                 (4, 'durian'),
                 (5, 'eggfruit'),
                 (6, 'fig'),
                 (7, 'grapefruit')])

To change a single row in the database, you can use the :meth:`DB.update`
method. For instance, if you want to capitalize the name 'banana'::

    >>> db.update('fruits', banana, name=banana['name'].capitalize())
    {'id': 2, 'name': 'Banana'}
    >>> print(db.query('select * from fruits where id between 1 and 3'))
    id|  name
    --+---------
     1|apple
     2|Banana
     3|cherimaya
    (3 rows)

Let's also capitalize the other names in the database::

    >>> db.query('update fruits set name=initcap(name)')
    '7'

The returned string `'7'` tells us the number of updated rows. It is returned
as a string to discern it from an OID which will be returned as an integer,
if a new row has been inserted into a table with an OID column.

To delete a single row from the database, use the :meth:`DB.delete` method::

    >>> db.delete('fruits', banana)
    1

The returned integer value `1` tells us that one row has been deleted. If we
try it again, the method returns the integer value `0`. Naturally, this method
can only return 0 or 1::

    >>> db.delete('fruits', banana)
    0

Of course, we can insert the row back again::

    >>> db.insert('fruits', banana)
    {'id': 2, 'name': 'Banana'}

If we want to change a different row, we can get its current state with::

    >>> apple = db.get('fruits', 1)
    >>> apple
    {'name': 'Apple', 'id': 1}

We can duplicate the row like this::

    >>> db.insert('fruits', apple, id=8)
    {'id': 8, 'name': 'Apple'}

 To remove the duplicated row, we can do::

    >>> db.delete('fruits', id=8)
    1

Finally, to remove the table from the database and close the connection::

    >>> db.query("drop table fruits")
    >>> db.close()

For more advanced features and details, see the reference: :doc:`pg/index`

First Steps with the DB-API 2.0 Interface
-----------------------------------------

.. py:currentmodule:: pgdb

As with the classic interface, the first thing you need to do is to create
a database connection. To do this, use the function :func:`pgdb.connect`
in the :mod:`pgdb` module, passing the connection parameters::

    >>> from pgdb import connect
    >>> con = connect(database='testdb', host='pgserver:5432',
    ...     user='scott', password='tiger')

Note that like in the classic interface, you can omit parameters if they
are the default values used by PostgreSQL.

To do anything with the connection, you need to request a cursor object
from it, which is thought of as the Python representation of a database
cursor. The connection has a method that lets you get a cursor::

   >>> cursor = con.cursor()

The cursor now has a method that lets you execute database queries::

   >>> cursor.execute("create table fruits("
   ...     "id serial primary key, name varchar)")


To insert data into the table, also can also use this method::

   >>> cursor.execute("insert into fruits (name) values ('apple')")

You can pass parameters in a safe way::

   >>> cursor.execute("insert into fruits (name) values (%s)", ('banana',))

For inserting multiple rows at once, you can use the following method::

   >>> more_fruits = 'cherimaya durian eggfruit fig grapefruit'.split()
   >>> parameters = [(name,) for name in more_fruits]
   >>> cursor.executemany("insert into fruits (name) values (%s)", parameters)

The cursor also has a :meth:`Cursor.copy_from` method to quickly insert
large amounts of data into the database, and a :meth:`Cursor.copy_to`
method to quickly dump large amounts of data from the database, using the
PostgreSQL COPY command. Note however, that these methods are an extension
provided by PyGreSQL, they are not part of the DB-API 2 standard.

Also note that the DB API 2.0 interface does not have an autocommit as you
may be used from PostgreSQL. So in order to make these inserts permanent,
you need to commit them to the database first::

   >>> con.commit()

If you end the program without calling the commit method of the connection,
or if you call the rollback method of the connection, then all the changes
will be discarded.

In a similar way, you can also update or delete rows in the database,
executing UPDATE or DELETE statements instead of INSERT statements.

To fetch rows from the database, execute a SELECT statement first. Then
you can use one of several fetch methods to retrieve the results. For
instance, to request a single row::

   >>> cursor.execute('select * from fruits where id=1')
   >>> cursor.fetchone()
   Row(id=1, name='apple')

The result is a named tuple. This means you can access its elements either
using an index number like in an ordinary tuple, or using the column name
like you access object attributes.

To fetch all rows of the query, use this method instead::

   >>> cursor.execute('select * from fruits')
   >>> cursor.fetchall()
   [Row(id=1, name='apple'), ..., Row(id=7, name='grapefruit')]

The output is a list of named tuples.

If you want to fetch only a limited number of rows from the query::

   >>> cursor.execute('select * from fruits')
   >>> cursor.fetchmany(2)
   [Row(id=1, name='apple'), Row(id=2, name='banana')]

Finally, to remove the table from the database and close the connection::

    >>> db.execute("drop table fruits")
    >>> cur.close()
    >>> db.close()

For more advanced features and details, see the reference: :doc:`pgdb/index`