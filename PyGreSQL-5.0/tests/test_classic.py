#! /usr/bin/python
# -*- coding: utf-8 -*-

from __future__ import print_function

try:
    import unittest2 as unittest  # for Python < 2.7
except ImportError:
    import unittest

import sys
from functools import partial
from time import sleep
from threading import Thread

from pg import *

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


def opendb():
    db = DB(dbname, dbhost, dbport)
    db.query("SET DATESTYLE TO 'ISO'")
    db.query("SET TIME ZONE 'EST5EDT'")
    db.query("SET DEFAULT_WITH_OIDS=FALSE")
    db.query("SET STANDARD_CONFORMING_STRINGS=FALSE")
    db.query("SET CLIENT_MIN_MESSAGES=WARNING")
    return db

db = opendb()
for q in (
    "DROP TABLE _test1._test_schema",
    "DROP TABLE _test2._test_schema",
    "DROP SCHEMA _test1",
    "DROP SCHEMA _test2",
):
    try:
        db.query(q)
    except Exception:
        pass
db.close()


class UtilityTest(unittest.TestCase):

    def setUp(self):
        """Setup test tables or empty them if they already exist."""
        db = opendb()

        for t in ('_test1', '_test2'):
            try:
                db.query("CREATE SCHEMA " + t)
            except Error:
                pass
            try:
                db.query("CREATE TABLE %s._test_schema "
                    "(%s int PRIMARY KEY)" % (t, t))
            except Error:
                db.query("DELETE FROM %s._test_schema" % t)
        try:
            db.query("CREATE TABLE _test_schema "
                "(_test int PRIMARY KEY, _i interval, dvar int DEFAULT 999)")
        except Error:
            db.query("DELETE FROM _test_schema")
        try:
            db.query("CREATE VIEW _test_vschema AS "
                "SELECT _test, 'abc'::text AS _test2 FROM _test_schema")
        except Error:
            pass

    def test_invalidname(self):
        """Make sure that invalid table names are caught"""
        db = opendb()
        self.assertRaises(NotSupportedError, db.get_attnames, 'x.y.z')

    def test_schema(self):
        """Does it differentiate the same table name in different schemas"""
        db = opendb()
        # see if they differentiate the table names properly
        self.assertEqual(
            db.get_attnames('_test_schema'),
            {'_test': 'int', '_i': 'date', 'dvar': 'int'}
        )
        self.assertEqual(
            db.get_attnames('public._test_schema'),
            {'_test': 'int', '_i': 'date', 'dvar': 'int'}
        )
        self.assertEqual(
            db.get_attnames('_test1._test_schema'),
            {'_test1': 'int'}
        )
        self.assertEqual(
            db.get_attnames('_test2._test_schema'),
            {'_test2': 'int'}
        )

    def test_pkey(self):
        db = opendb()
        self.assertEqual(db.pkey('_test_schema'), '_test')
        self.assertEqual(db.pkey('public._test_schema'), '_test')
        self.assertEqual(db.pkey('_test1._test_schema'), '_test1')
        self.assertEqual(db.pkey('_test2._test_schema'), '_test2')
        self.assertRaises(KeyError, db.pkey, '_test_vschema')

    def test_get(self):
        db = opendb()
        db.query("INSERT INTO _test_schema VALUES (1234)")
        db.get('_test_schema', 1234)
        db.get('_test_schema', 1234, keyname='_test')
        self.assertRaises(ProgrammingError, db.get, '_test_vschema', 1234)
        db.get('_test_vschema', 1234, keyname='_test')

    def test_params(self):
        db = opendb()
        db.query("INSERT INTO _test_schema VALUES ($1, $2, $3)", 12, None, 34)
        d = db.get('_test_schema', 12)
        self.assertEqual(d['dvar'], 34)

    def test_insert(self):
        db = opendb()
        d = dict(_test=1234)
        db.insert('_test_schema', d)
        self.assertEqual(d['dvar'], 999)
        db.insert('_test_schema', _test=1235)
        self.assertEqual(d['dvar'], 999)

    def test_context_manager(self):
        db = opendb()
        t = '_test_schema'
        d = dict(_test=1235)
        with db:
            db.insert(t, d)
            d['_test'] += 1
            db.insert(t, d)
        try:
            with db:
                d['_test'] += 1
                db.insert(t, d)
                db.insert(t, d)
        except IntegrityError:
            pass
        with db:
            d['_test'] += 1
            db.insert(t, d)
            d['_test'] += 1
            db.insert(t, d)
        self.assertTrue(db.get(t, 1235))
        self.assertTrue(db.get(t, 1236))
        self.assertRaises(DatabaseError, db.get, t, 1237)
        self.assertTrue(db.get(t, 1238))
        self.assertTrue(db.get(t, 1239))

    def test_sqlstate(self):
        db = opendb()
        db.query("INSERT INTO _test_schema VALUES (1234)")
        try:
            db.query("INSERT INTO _test_schema VALUES (1234)")
        except DatabaseError as error:
            self.assertTrue(isinstance(error, IntegrityError))
            # the SQLSTATE error code for unique violation is 23505
            self.assertEqual(error.sqlstate, '23505')

    def test_mixed_case(self):
        db = opendb()
        try:
            db.query('CREATE TABLE _test_mc ("_Test" int PRIMARY KEY)')
        except Error:
            db.query("DELETE FROM _test_mc")
        d = dict(_Test=1234)
        db.insert('_test_mc', d)

    def test_update(self):
        db = opendb()
        db.query("INSERT INTO _test_schema VALUES (1234)")

        r = db.get('_test_schema', 1234)
        r['dvar'] = 123
        db.update('_test_schema', r)
        r = db.get('_test_schema', 1234)
        self.assertEqual(r['dvar'], 123)

        r = db.get('_test_schema', 1234)
        self.assertIn('dvar', r)
        db.update('_test_schema', _test=1234, dvar=456)
        r = db.get('_test_schema', 1234)
        self.assertEqual(r['dvar'], 456)

        r = db.get('_test_schema', 1234)
        db.update('_test_schema', r, dvar=456)
        r = db.get('_test_schema', 1234)
        self.assertEqual(r['dvar'], 456)

    def notify_callback(self, arg_dict):
        if arg_dict:
            arg_dict['called'] = True
        else:
            self.notify_timeout = True

    def test_notify(self, options=None):
        if not options:
            options = {}
        run_as_method = options.get('run_as_method')
        call_notify = options.get('call_notify')
        two_payloads = options.get('two_payloads')
        db = opendb()
        # Get function under test, can be standalone or DB method.
        fut = db.notification_handler if run_as_method else partial(
            NotificationHandler, db)
        arg_dict = dict(event=None, called=False)
        self.notify_timeout = False
        # Listen for 'event_1'.
        target = fut('event_1', self.notify_callback, arg_dict, 5)
        thread = Thread(None, target)
        thread.start()
        try:
            # Wait until the thread has started.
            for n in range(500):
                if target.listening:
                    break
                sleep(0.01)
            self.assertTrue(target.listening)
            self.assertTrue(thread.is_alive())
            # Open another connection for sending notifications.
            db2 = opendb()
            # Generate notification from the other connection.
            if two_payloads:
                db2.begin()
            if call_notify:
                if two_payloads:
                    target.notify(db2, payload='payload 0')
                target.notify(db2, payload='payload 1')
            else:
                if two_payloads:
                    db2.query("notify event_1, 'payload 0'")
                db2.query("notify event_1, 'payload 1'")
            if two_payloads:
                db2.commit()
            # Wait until the notification has been caught.
            for n in range(500):
                if arg_dict['called'] or self.notify_timeout:
                    break
                sleep(0.01)
            # Check that callback has been invoked.
            self.assertTrue(arg_dict['called'])
            self.assertEqual(arg_dict['event'], 'event_1')
            self.assertEqual(arg_dict['extra'], 'payload 1')
            self.assertTrue(isinstance(arg_dict['pid'], int))
            self.assertFalse(self.notify_timeout)
            arg_dict['called'] = False
            self.assertTrue(thread.is_alive())
            # Generate stop notification.
            if call_notify:
                target.notify(db2, stop=True, payload='payload 2')
            else:
                db2.query("notify stop_event_1, 'payload 2'")
            db2.close()
            # Wait until the notification has been caught.
            for n in range(500):
                if arg_dict['called'] or self.notify_timeout:
                    break
                sleep(0.01)
            # Check that callback has been invoked.
            self.assertTrue(arg_dict['called'])
            self.assertEqual(arg_dict['event'], 'stop_event_1')
            self.assertEqual(arg_dict['extra'], 'payload 2')
            self.assertTrue(isinstance(arg_dict['pid'], int))
            self.assertFalse(self.notify_timeout)
            thread.join(5)
            self.assertFalse(thread.is_alive())
            self.assertFalse(target.listening)
            target.close()
        except Exception:
            target.close()
            if thread.is_alive():
                thread.join(5)

    def test_notify_other_options(self):
        for run_as_method in False, True:
            for call_notify in False, True:
                for two_payloads in False, True:
                    options = dict(
                        run_as_method=run_as_method,
                        call_notify=call_notify,
                        two_payloads=two_payloads)
                    if any(options.values()):
                        self.test_notify(options)

    def test_notify_timeout(self):
        for run_as_method in False, True:
            db = opendb()
            # Get function under test, can be standalone or DB method.
            fut = db.notification_handler if run_as_method else partial(
                NotificationHandler, db)
            arg_dict = dict(event=None, called=False)
            self.notify_timeout = False
            # Listen for 'event_1' with timeout of 10ms.
            target = fut('event_1', self.notify_callback, arg_dict, 0.01)
            thread = Thread(None, target)
            thread.start()
            # Sleep 20ms, long enough to time out.
            sleep(0.02)
            # Verify that we've indeed timed out.
            self.assertFalse(arg_dict.get('called'))
            self.assertTrue(self.notify_timeout)
            self.assertFalse(thread.is_alive())
            self.assertFalse(target.listening)
            target.close()


if __name__ == '__main__':
    if len(sys.argv) == 2 and sys.argv[1] == '-l':
        print('\n'.join(unittest.getTestCaseNames(UtilityTest, 'test_')))
        sys.exit(0)

    test_list = [name for name in sys.argv[1:] if not name.startswith('-')]
    if not test_list:
        test_list = unittest.getTestCaseNames(UtilityTest, 'test_')

    suite = unittest.TestSuite()
    for test_name in test_list:
        try:
            suite.addTest(UtilityTest(test_name))
        except Exception:
            print("\n ERROR: %s.\n" % sys.exc_value)
            sys.exit(1)

    verbosity = '-v' in sys.argv[1:] and 2 or 1
    failfast = '-l' in sys.argv[1:]
    runner = unittest.TextTestRunner(verbosity=verbosity, failfast=failfast)
    rc = runner.run(suite)
    sys.exit(1 if rc.errors or rc.failures else 0)
