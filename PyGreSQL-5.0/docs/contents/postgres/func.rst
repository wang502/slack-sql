Examples for using SQL functions
================================

.. py:currentmodule:: pg

We assume that you have already created a connection to the PostgreSQL
database, as explained in the :doc:`basic`::

    >>> from pg import DB
    >>> db = DB()
    >>> query = db.query

Creating SQL Functions on Base Types
------------------------------------

A **CREATE FUNCTION** statement lets you create a new function that can be
used in expressions (in SELECT, INSERT, etc.). We will start with functions
that return values of base types.

Let's create a simple SQL function that takes no arguments and returns 1::

    >>> query("""CREATE FUNCTION one() RETURNS int4
    ...     AS 'SELECT 1 as ONE' LANGUAGE SQL""")

Functions can be used in any expressions (eg. in the target"list or
qualifications)::

    >>> print(db.query("SELECT one() AS answer"))
    answer
    ------
         1
    (1 row)


Here's how you create a function that takes arguments. The following function
returns the sum of its two arguments::

    >>> query("""CREATE FUNCTION add_em(int4, int4) RETURNS int4
    ...     AS $$ SELECT $1 + $2 $$ LANGUAGE SQL""")
    >>> print(query("SELECT add_em(1, 2) AS answer"))
    answer
    ------
         3
    (1 row)


Creating SQL Functions on Composite Types
-----------------------------------------

It is also possible to create functions that return values of composite types.

Before we create more sophisticated functions, let's populate an EMP table::

    >>> query("""CREATE TABLE EMP (
    ...     name   text,
    ...     salary int4,
    ...     age f   int4,
    ...     dept   varchar(16))""")
    >>> emps = ["'Sam', 1200, 16, 'toy'",
    ...     "'Claire', 5000, 32, 'shoe'",
    ...     "'Andy', -1000, 2, 'candy'",
    ...     "'Bill', 4200, 36, 'shoe'",
    ...     "'Ginger', 4800, 30, 'candy'"]
    >>> for emp in emps:
    ...     query("INSERT INTO EMP VALUES (%s)" % emp)

Every INSERT statement will return a '1' indicating that it has inserted
one row into the EMP table.

The argument of a function can also be a tuple. For instance, *double_salary*
takes a tuple of the EMP table::

    >>> query("""CREATE FUNCTION double_salary(EMP) RETURNS int4
    ...     AS $$ SELECT $1.salary * 2 AS salary $$ LANGUAGE SQL""")
    >>> print(query("""SELECT name, double_salary(EMP) AS dream
    ...     FROM EMP WHERE EMP.dept = 'toy'"""))
    name|dream
    ----+-----
    Sam | 2400
    (1 row)

The return value of a function can also be a tuple. However, make sure that the
expressions in the target list are in the same order as the columns of EMP::

    >>> query("""CREATE FUNCTION new_emp() RETURNS EMP AS $$
    ...     SELECT 'None'::text AS name,
    ...         1000 AS salary,
    ...         25 AS age,
    ...         'None'::varchar(16) AS dept
    ...     $$ LANGUAGE SQL""")

You can then project a column out of resulting the tuple by using the
"function notation" for projection columns (i.e. ``bar(foo)`` is equivalent
to ``foo.bar``). Note that ``new_emp().name`` isn't supported::

    >>> print(query("SELECT name(new_emp()) AS nobody"))
    nobody
    ------
    None
    (1 row)

Let's try one more function that returns tuples::

    >>> query("""CREATE FUNCTION high_pay() RETURNS setof EMP
    ...         AS 'SELECT * FROM EMP where salary > 1500'
    ...     LANGUAGE SQL""")
    >>> query("SELECT name(high_pay()) AS overpaid")
    overpaid
    --------
    Claire
    Bill
    Ginger
    (3 rows)


Creating SQL Functions with multiple SQL statements
---------------------------------------------------

You can also create functions that do more than just a SELECT.

You may have noticed that Andy has a negative salary. We'll create a function
that removes employees with negative salaries::

    >>> query("SELECT * FROM EMP")
     name |salary|age|dept
    ------+------+---+-----
    Sam   |  1200| 16|toy
    Claire|  5000| 32|shoe
    Andy  | -1000|  2|candy
    Bill  |  4200| 36|shoe
    Ginger|  4800| 30|candy
    (5 rows)
    >>> query("""CREATE FUNCTION clean_EMP () RETURNS int4 AS
    ...         'DELETE FROM EMP WHERE EMP.salary <= 0;
    ...          SELECT 1 AS ignore_this'
    ...     LANGUAGE SQL""")
    >>> query("SELECT clean_EMP()")
    clean_emp
    ---------
            1
    (1 row)
    >>> query("SELECT * FROM EMP")
     name |salary|age|dept
    ------+------+---+-----
    Sam   |  1200| 16|toy
    Claire|  5000| 32|shoe
    Bill  |  4200| 36|shoe
    Ginger|  4800| 30|candy
    (4 rows)

Remove functions that were created in this example
--------------------------------------------------

We can remove the functions that we have created in this example and the
table EMP, by using the DROP command::

    query("DROP FUNCTION clean_EMP()")
    query("DROP FUNCTION high_pay()")
    query("DROP FUNCTION new_emp()")
    query("DROP FUNCTION add_em(int4, int4)")
    query("DROP FUNCTION one()")
    query("DROP TABLE EMP CASCADE")
