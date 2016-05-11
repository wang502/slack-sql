Examples for using the system catalogs
======================================

.. py:currentmodule:: pg

The system catalogs are regular tables where PostgreSQL stores schema metadata,
such as information about tables and columns, and internal bookkeeping
information. You can drop and recreate the tables, add columns, insert and
update values, and severely mess up your system that way. Normally, one
should not change the system catalogs by hand, there are always SQL commands
to do that. For example, CREATE DATABASE inserts a row into the *pg_database*
catalog — and actually creates the database on disk.

It this section we want to show examples for how to parse some of the system
catalogs, making queries with the classic PyGreSQL interface.

We assume that you have already created a connection to the PostgreSQL
database, as explained in the :doc:`basic`::

    >>> from pg import DB
    >>> db = DB()
    >>> query = query

Lists indices
-------------

This query lists all simple indices in the database::

    print(query("""SELECT bc.relname AS class_name,
            ic.relname AS index_name, a.attname
        FROM pg_class bc, pg_class ic, pg_index i, pg_attribute a
        WHERE i.indrelid = bc.oid AND i.indexrelid = ic.oid
            AND i.indkey[0] = a.attnum AND a.attrelid = bc.oid
            AND NOT a.attisdropped
        ORDER BY class_name, index_name, attname"""))


List user defined attributes
----------------------------

This query lists all user defined attributes and their type
in user-defined classes::

    print(query("""SELECT c.relname, a.attname, t.typname
        FROM pg_class c, pg_attribute a, pg_type t
        WHERE c.relkind = 'r' and c.relname !~ '^pg_'
            AND c.relname !~ '^Inv' and a.attnum > 0
            AND a.attrelid = c.oid and a.atttypid = t.oid
            AND NOT a.attisdropped
        ORDER BY relname, attname"""))

List user defined base types
----------------------------

This query lists all user defined base types::

    print(query("""SELECT r.rolname, t.typname
        FROM pg_type t, pg_authid r
        WHERE r.oid = t.typowner
            AND t.typrelid = '0'::oid and t.typelem = '0'::oid
            AND r.rolname != 'postgres'
        ORDER BY rolname, typname"""))


List  operators
---------------

This query lists all right-unary operators::

    print(query("""SELECT o.oprname AS right_unary,
            lt.typname AS operand, result.typname AS return_type
        FROM pg_operator o, pg_type lt, pg_type result
        WHERE o.oprkind='r' and o.oprleft = lt.oid
            AND o.oprresult = result.oid
        ORDER BY operand"""))


This query lists all left-unary operators::

    print(query("""SELECT o.oprname AS left_unary,
            rt.typname AS operand, result.typname AS return_type
        FROM pg_operator o, pg_type rt, pg_type result
        WHERE o.oprkind='l' AND o.oprright = rt.oid
            AND o.oprresult = result.oid
        ORDER BY operand"""))


And this one lists all of the binary operators::

    print(query("""SELECT o.oprname AS binary_op,
            rt.typname AS right_opr, lt.typname AS left_opr,
            result.typname AS return_type
        FROM pg_operator o, pg_type rt, pg_type lt, pg_type result
        WHERE o.oprkind = 'b' AND o.oprright = rt.oid
            AND o.oprleft = lt.oid AND o.oprresult = result.oid"""))


List functions of a language
----------------------------

Given a programming language, this query returns the name, args and return
type from all functions of a language::

    language = 'sql'
    print(query("""SELECT p.proname, p.pronargs, t.typname
        FROM pg_proc p, pg_language l, pg_type t
        WHERE p.prolang = l.oid AND p.prorettype = t.oid
            AND l.lanname = $1
        ORDER BY proname""", (language,)))


List aggregate functions
------------------------

This query lists all of the aggregate functions and the type to which
they can be applied::

    print(query("""SELECT p.proname, t.typname
        FROM pg_aggregate a, pg_proc p, pg_type t
        WHERE a.aggfnoid = p.oid
            and p.proargtypes[0] = t.oid
        ORDER BY proname, typname"""))

List operator families
----------------------

The following query lists all defined operator families and all the operators
included in each family::

    print(query("""SELECT am.amname, opf.opfname, amop.amopopr::regoperator
        FROM pg_am am, pg_opfamily opf, pg_amop amop
        WHERE opf.opfmethod = am.oid
            AND amop.amopfamily = opf.oid
        ORDER BY amname, opfname, amopopr"""))
