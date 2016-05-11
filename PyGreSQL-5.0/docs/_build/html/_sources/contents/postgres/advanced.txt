Examples for advanced features
==============================

.. py:currentmodule:: pg

In this section, we show how to use some advanced features of PostgreSQL
using the classic PyGreSQL interface.

We assume that you have already created a connection to the PostgreSQL
database, as explained in the :doc:`basic`::

    >>> from pg import DB
    >>> db = DB()
    >>> query = query

Inheritance
-----------

A table can inherit from zero or more tables. A query can reference either
all rows of a table or all rows of a table plus all of its descendants.

For example, the capitals table inherits from cities table (it inherits
all data fields from cities)::

    >>> data = [('cities', [
    ...         "'San Francisco', 7.24E+5, 63",
    ...         "'Las Vegas', 2.583E+5, 2174",
    ...         "'Mariposa', 1200, 1953"]),
    ...     ('capitals', [
    ...         "'Sacramento',3.694E+5,30,'CA'",
    ...         "'Madison', 1.913E+5, 845, 'WI'"])]

Now, let's populate the tables::

    >>> data = ['cities', [
    ...         "'San Francisco', 7.24E+5, 63"
    ...         "'Las Vegas', 2.583E+5, 2174"
    ...         "'Mariposa', 1200, 1953"],
    ...     'capitals', [
    ...         "'Sacramento',3.694E+5,30,'CA'",
    ...         "'Madison', 1.913E+5, 845, 'WI'"]]
    >>> for table, rows in data:
    ...     for row in rows:
    ...         query("INSERT INTO %s VALUES (%s)" % (table, row))
    >>> print(query("SELECT * FROM cities"))
        name     |population|altitude
    -------------+----------+--------
    San Francisco|    724000|      63
    Las Vegas    |    258300|    2174
    Mariposa     |      1200|    1953
    Sacramento   |    369400|      30
    Madison      |    191300|     845
    (5 rows)
    >>> print(query("SELECT * FROM capitals"))
       name   |population|altitude|state
    ----------+----------+--------+-----
    Sacramento|    369400|      30|CA
    Madison   |    191300|     845|WI
    (2 rows)

You can find all cities, including capitals, that are located at an altitude
of 500 feet or higher by::

    >>> print(query("""SELECT c.name, c.altitude
    ...     FROM cities
    ...     WHERE altitude > 500"""))
      name   |altitude
    ---------+--------
    Las Vegas|    2174
    Mariposa |    1953
    Madison  |     845
    (3 rows)

On the other hand, the following query references rows of the base table only,
i.e. it finds all cities that are not state capitals and are situated at an
altitude of 500 feet or higher::

    >>> print(query("""SELECT name, altitude
    ...     FROM ONLY cities
    ...     WHERE altitude > 500"""))
      name   |altitude
    ---------+--------
    Las Vegas|    2174
    Mariposa |    1953
    (2 rows)

Arrays
------

Attributes can be arrays of base types or user-defined types::

    >>> query("""CREATE TABLE sal_emp (
    ...        name                  text,
    ...        pay_by_quarter        int4[],
    ...        pay_by_extra_quarter  int8[],
    ...        schedule              text[][])""")


Insert instances with array attributes. Note the use of braces::

    >>> query("""INSERT INTO sal_emp VALUES (
    ...     'Bill', '{10000,10000,10000,10000}',
    ...     '{9223372036854775800,9223372036854775800,9223372036854775800}',
    ...     '{{"meeting", "lunch"}, {"training", "presentation"}}')""")
    >>> query("""INSERT INTO sal_emp VALUES (
    ...     'Carol', '{20000,25000,25000,25000}',
    ...      '{9223372036854775807,9223372036854775807,9223372036854775807}',
    ...      '{{"breakfast", "consulting"}, {"meeting", "lunch"}}')""")


Queries on array attributes::

    >>> query("""SELECT name FROM sal_emp WHERE
    ...     sal_emp.pay_by_quarter[1] != sal_emp.pay_by_quarter[2]""")
    name
    -----
    Carol
    (1 row)

Retrieve third quarter pay of all employees::

    >>> query("SELECT sal_emp.pay_by_quarter[3] FROM sal_emp")
    pay_by_quarter
    --------------
             10000
             25000
    (2 rows)

Retrieve third quarter extra pay of all employees::

    >>> query("SELECT sal_emp.pay_by_extra_quarter[3] FROM sal_emp")
    pay_by_extra_quarter
    --------------------
     9223372036854775800
     9223372036854775807
    (2 rows)

Retrieve first two quarters of extra quarter pay of all employees::

    >>> query("SELECT sal_emp.pay_by_extra_quarter[1:2] FROM sal_emp")
              pay_by_extra_quarter
    -----------------------------------------
    {9223372036854775800,9223372036854775800}
    {9223372036854775807,9223372036854775807}
    (2 rows)

Select subarrays::

    >>> query("""SELECT sal_emp.schedule[1:2][1:1] FROM sal_emp
    ...     WHERE sal_emp.name = 'Bill'""")
           schedule
    ----------------------
    {{meeting},{training}}
    (1 row)
