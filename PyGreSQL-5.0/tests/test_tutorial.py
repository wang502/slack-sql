#! /usr/bin/python
# -*- coding: utf-8 -*-

from __future__ import print_function

try:
    import unittest2 as unittest  # for Python < 2.7
except ImportError:
    import unittest

from pg import DB
from pgdb import connect

# We need a database to test against.  If LOCAL_PyGreSQL.py exists we will
# get our information from that.  Otherwise we use the defaults.
dbname = 'unittest'
dbhost = None
dbport = 5432

try:
    from .LOCAL_PyGreSQL import *
except (ImportError, ValueError):
    try:
        from LOCAL_PyGreSQL import *
    except ImportError:
        pass


class TestClassicTutorial(unittest.TestCase):
    """Test the First Steps Tutorial for the classic interface."""

    def setUp(self):
        """Setup test tables or empty them if they already exist."""
        db = DB(dbname=dbname, host=dbhost, port=dbport)
        db.query("set datestyle to 'iso'")
        db.query("set default_with_oids=false")
        db.query("set standard_conforming_strings=false")
        db.query("set client_min_messages=warning")
        db.query("drop table if exists fruits cascade")
        db.query("create table fruits(id serial primary key, name varchar)")
        self.db = db

    def tearDown(self):
        db = self.db
        db.query("drop table fruits")
        db.close()

    def test_all_steps(self):
        db = self.db
        r = db.get_tables()
        self.assertIsInstance(r, list)
        self.assertIn('public.fruits', r)
        r = db.get_attnames('fruits')
        self.assertIsInstance(r, dict)
        self.assertEqual(r, {'id': 'int', 'name': 'text'})
        r = db.has_table_privilege('fruits', 'insert')
        self.assertTrue(r)
        r = db.insert('fruits', name='apple')
        self.assertIsInstance(r, dict)
        self.assertEqual(r, {'name': 'apple', 'id': 1})
        banana = r = db.insert('fruits', name='banana')
        self.assertIsInstance(r, dict)
        self.assertEqual(r, {'name': 'banana', 'id': 2})
        more_fruits = 'cherimaya durian eggfruit fig grapefruit'.split()
        data = list(enumerate(more_fruits, start=3))
        db.inserttable('fruits', data)
        q = db.query('select * from fruits')
        r = str(q).splitlines()
        self.assertEqual(r[0], 'id|   name   ')
        self.assertEqual(r[1], '--+----------')
        self.assertEqual(r[2], ' 1|apple     ')
        self.assertEqual(r[8], ' 7|grapefruit')
        self.assertEqual(r[9], '(7 rows)')
        q = db.query('select * from fruits')
        r = q.getresult()
        self.assertIsInstance(r, list)
        self.assertIsInstance(r[0], tuple)
        self.assertEqual(r[0], (1, 'apple'))
        self.assertEqual(r[6], (7, 'grapefruit'))
        r = q.dictresult()
        self.assertIsInstance(r, list)
        self.assertIsInstance(r[0], dict)
        self.assertEqual(r[0], {'id': 1, 'name': 'apple'})
        self.assertEqual(r[6], {'id': 7, 'name': 'grapefruit'})
        rows = r = q.namedresult()
        self.assertIsInstance(r, list)
        self.assertIsInstance(r[0], tuple)
        self.assertEqual(rows[3].name, 'durian')
        r = db.update('fruits', banana, name=banana['name'].capitalize())
        self.assertIsInstance(r, dict)
        self.assertEqual(r, {'id': 2, 'name': 'Banana'})
        q = db.query('select * from fruits where id between 1 and 3')
        r = str(q).splitlines()
        self.assertEqual(r[0], 'id|  name   ')
        self.assertEqual(r[1], '--+---------')
        self.assertEqual(r[2], ' 1|apple    ')
        self.assertEqual(r[3], ' 2|Banana   ')
        self.assertEqual(r[4], ' 3|cherimaya')
        self.assertEqual(r[5], '(3 rows)')
        r = db.query('update fruits set name=initcap(name)')
        self.assertIsInstance(r, str)
        self.assertEqual(r, '7')
        r = db.delete('fruits', banana)
        self.assertIsInstance(r, int)
        self.assertEqual(r, 1)
        r = db.delete('fruits', banana)
        self.assertIsInstance(r, int)
        self.assertEqual(r, 0)
        r = db.insert('fruits', banana)
        self.assertIsInstance(r, dict)
        self.assertEqual(r, {'id': 2, 'name': 'Banana'})
        apple = r = db.get('fruits', 1)
        self.assertIsInstance(r, dict)
        self.assertEqual(r, {'name': 'Apple', 'id': 1})
        r = db.insert('fruits', apple, id=8)
        self.assertIsInstance(r, dict)
        self.assertEqual(r, {'id': 8, 'name': 'Apple'})
        r = db.delete('fruits', id=8)
        self.assertIsInstance(r, int)
        self.assertEqual(r, 1)


class TestDbApi20Tutorial(unittest.TestCase):
    """Test the First Steps Tutorial for the DB-API 2.0 interface."""

    def setUp(self):
        """Setup test tables or empty them if they already exist."""
        database = dbname
        host = '%s:%d' % (dbhost or '', dbport or -1)
        con = connect(database=database, host=host)
        cur = con.cursor()
        cur.execute("set datestyle to 'iso'")
        cur.execute("set default_with_oids=false")
        cur.execute("set standard_conforming_strings=false")
        cur.execute("set client_min_messages=warning")
        cur.execute("drop table if exists fruits cascade")
        cur.execute("create table fruits(id serial primary key, name varchar)")
        cur.close()
        self.con = con

    def tearDown(self):
        con = self.con
        cur = con.cursor()
        cur.execute("drop table fruits")
        cur.close()
        con.close()

    def test_all_steps(self):
        con = self.con
        cursor = con.cursor()
        cursor.execute("insert into fruits (name) values ('apple')")
        cursor.execute("insert into fruits (name) values (%s)", ('banana',))
        more_fruits = 'cherimaya durian eggfruit fig grapefruit'.split()
        parameters = [(name,) for name in more_fruits]
        cursor.executemany("insert into fruits (name) values (%s)", parameters)
        con.commit()
        cursor.execute('select * from fruits where id=1')
        r = cursor.fetchone()
        self.assertIsInstance(r, tuple)
        self.assertEqual(len(r), 2)
        r = str(r)
        self.assertEqual(r, "Row(id=1, name='apple')")
        cursor.execute('select * from fruits')
        r = cursor.fetchall()
        self.assertIsInstance(r, list)
        self.assertEqual(len(r), 7)
        self.assertEqual(str(r[0]), "Row(id=1, name='apple')")
        self.assertEqual(str(r[6]), "Row(id=7, name='grapefruit')")
        cursor.execute('select * from fruits')
        r = cursor.fetchmany(2)
        self.assertIsInstance(r, list)
        self.assertEqual(len(r), 2)
        self.assertEqual(str(r[0]), "Row(id=1, name='apple')")
        self.assertEqual(str(r[1]), "Row(id=2, name='banana')")


if __name__ == '__main__':
    unittest.main()
