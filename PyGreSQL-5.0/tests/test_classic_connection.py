#! /usr/bin/python
# -*- coding: utf-8 -*-

"""Test the classic PyGreSQL interface.

Sub-tests for the low-level connection object.

Contributed by Christoph Zwerschke.

These tests need a database to test against.
"""

try:
    import unittest2 as unittest  # for Python < 2.7
except ImportError:
    import unittest
import threading
import time
import os

import pg  # the module under test

from decimal import Decimal

# We need a database to test against.  If LOCAL_PyGreSQL.py exists we will
# get our information from that.  Otherwise we use the defaults.
# These tests should be run with various PostgreSQL versions and databases
# created with different encodings and locales.  Particularly, make sure the
# tests are running against databases created with both SQL_ASCII and UTF8.
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

try:
    long
except NameError:  # Python >= 3.0
    long = int

try:
    unicode
except NameError:  # Python >= 3.0
    unicode = str

unicode_strings = str is not bytes

windows = os.name == 'nt'

# There is a known a bug in libpq under Windows which can cause
# the interface to crash when calling PQhost():
do_not_ask_for_host = windows
do_not_ask_for_host_reason = 'libpq issue on Windows'


def connect():
    """Create a basic pg connection to the test database."""
    connection = pg.connect(dbname, dbhost, dbport)
    connection.query("set client_min_messages=warning")
    return connection


class TestCanConnect(unittest.TestCase):
    """Test whether a basic connection to PostgreSQL is possible."""

    def testCanConnect(self):
        try:
            connection = connect()
        except pg.Error as error:
            self.fail('Cannot connect to database %s:\n%s' % (dbname, error))
        try:
            connection.close()
        except pg.Error:
            self.fail('Cannot close the database connection')


class TestConnectObject(unittest.TestCase):
    """Test existence of basic pg connection methods."""

    def setUp(self):
        self.connection = connect()

    def tearDown(self):
        try:
            self.connection.close()
        except pg.InternalError:
            pass

    def is_method(self, attribute):
        """Check if given attribute on the connection is a method."""
        if do_not_ask_for_host and attribute == 'host':
            return False
        return callable(getattr(self.connection, attribute))

    def testClassName(self):
        self.assertEqual(self.connection.__class__.__name__, 'Connection')

    def testModuleName(self):
        self.assertEqual(self.connection.__class__.__module__, 'pg')

    def testStr(self):
        r = str(self.connection)
        self.assertTrue(r.startswith('<pg.Connection object'), r)

    def testRepr(self):
        r = repr(self.connection)
        self.assertTrue(r.startswith('<pg.Connection object'), r)

    def testAllConnectAttributes(self):
        attributes = '''db error host options port
            protocol_version server_version status user'''.split()
        connection_attributes = [a for a in dir(self.connection)
            if not a.startswith('__') and not self.is_method(a)]
        self.assertEqual(attributes, connection_attributes)

    def testAllConnectMethods(self):
        methods = '''cancel close date_format endcopy
            escape_bytea escape_identifier escape_literal escape_string
            fileno get_cast_hook get_notice_receiver getline getlo getnotify
            inserttable locreate loimport parameter putline query reset
            set_cast_hook set_notice_receiver source transaction'''.split()
        connection_methods = [a for a in dir(self.connection)
            if not a.startswith('__') and self.is_method(a)]
        self.assertEqual(methods, connection_methods)

    def testAttributeDb(self):
        self.assertEqual(self.connection.db, dbname)

    def testAttributeError(self):
        error = self.connection.error
        self.assertTrue(not error or 'krb5_' in error)

    @unittest.skipIf(do_not_ask_for_host, do_not_ask_for_host_reason)
    def testAttributeHost(self):
        def_host = 'localhost'
        self.assertIsInstance(self.connection.host, str)
        self.assertEqual(self.connection.host, dbhost or def_host)

    def testAttributeOptions(self):
        no_options = ''
        self.assertEqual(self.connection.options, no_options)

    def testAttributePort(self):
        def_port = 5432
        self.assertIsInstance(self.connection.port, int)
        self.assertEqual(self.connection.port, dbport or def_port)

    def testAttributeProtocolVersion(self):
        protocol_version = self.connection.protocol_version
        self.assertIsInstance(protocol_version, int)
        self.assertTrue(2 <= protocol_version < 4)

    def testAttributeServerVersion(self):
        server_version = self.connection.server_version
        self.assertIsInstance(server_version, int)
        self.assertTrue(70400 <= server_version < 100000)

    def testAttributeStatus(self):
        status_ok = 1
        self.assertIsInstance(self.connection.status, int)
        self.assertEqual(self.connection.status, status_ok)

    def testAttributeUser(self):
        no_user = 'Deprecated facility'
        user = self.connection.user
        self.assertTrue(user)
        self.assertIsInstance(user, str)
        self.assertNotEqual(user, no_user)

    def testMethodQuery(self):
        query = self.connection.query
        query("select 1+1")
        query("select 1+$1", (1,))
        query("select 1+$1+$2", (2, 3))
        query("select 1+$1+$2", [2, 3])

    def testMethodQueryEmpty(self):
        self.assertRaises(ValueError, self.connection.query, '')

    def testMethodEndcopy(self):
        try:
            self.connection.endcopy()
        except IOError:
            pass

    def testMethodClose(self):
        self.connection.close()
        try:
            self.connection.reset()
        except (pg.Error, TypeError):
            pass
        else:
            self.fail('Reset should give an error for a closed connection')
        self.assertRaises(pg.InternalError, self.connection.close)
        try:
            self.connection.query('select 1')
        except (pg.Error, TypeError):
            pass
        else:
            self.fail('Query should give an error for a closed connection')
        self.connection = connect()

    def testMethodReset(self):
        query = self.connection.query
        # check that client encoding gets reset
        encoding = query('show client_encoding').getresult()[0][0].upper()
        changed_encoding = 'LATIN1' if encoding == 'UTF8' else 'UTF8'
        self.assertNotEqual(encoding, changed_encoding)
        self.connection.query("set client_encoding=%s" % changed_encoding)
        new_encoding = query('show client_encoding').getresult()[0][0].upper()
        self.assertEqual(new_encoding, changed_encoding)
        self.connection.reset()
        new_encoding = query('show client_encoding').getresult()[0][0].upper()
        self.assertNotEqual(new_encoding, changed_encoding)
        self.assertEqual(new_encoding, encoding)

    def testMethodCancel(self):
        r = self.connection.cancel()
        self.assertIsInstance(r, int)
        self.assertEqual(r, 1)

    def testCancelLongRunningThread(self):
        errors = []

        def sleep():
            try:
                self.connection.query('select pg_sleep(5)').getresult()
            except pg.DatabaseError as error:
                errors.append(str(error))

        thread = threading.Thread(target=sleep)
        t1 = time.time()
        thread.start()  # run the query
        while 1:  # make sure the query is really running
            time.sleep(0.1)
            if thread.is_alive() or time.time() - t1 > 5:
                break
        r = self.connection.cancel()  # cancel the running query
        thread.join()  # wait for the thread to end
        t2 = time.time()

        self.assertIsInstance(r, int)
        self.assertEqual(r, 1)  # return code should be 1
        self.assertLessEqual(t2 - t1, 3)  # time should be under 3 seconds
        self.assertTrue(errors)

    def testMethodFileNo(self):
        r = self.connection.fileno()
        self.assertIsInstance(r, int)
        self.assertGreaterEqual(r, 0)

    def testMethodTransaction(self):
        transaction = self.connection.transaction
        self.assertRaises(TypeError, transaction, None)
        self.assertEqual(transaction(), pg.TRANS_IDLE)
        self.connection.query('begin')
        self.assertEqual(transaction(), pg.TRANS_INTRANS)
        self.connection.query('rollback')
        self.assertEqual(transaction(), pg.TRANS_IDLE)

    def testMethodParameter(self):
        parameter = self.connection.parameter
        query = self.connection.query
        self.assertRaises(TypeError, parameter)
        r = parameter('this server setting does not exist')
        self.assertIsNone(r)
        s = query('show server_version').getresult()[0][0].upper()
        self.assertIsNotNone(s)
        r = parameter('server_version')
        self.assertEqual(r, s)
        s = query('show server_encoding').getresult()[0][0].upper()
        self.assertIsNotNone(s)
        r = parameter('server_encoding')
        self.assertEqual(r, s)
        s = query('show client_encoding').getresult()[0][0].upper()
        self.assertIsNotNone(s)
        r = parameter('client_encoding')
        self.assertEqual(r, s)
        s = query('show server_encoding').getresult()[0][0].upper()
        self.assertIsNotNone(s)
        r = parameter('server_encoding')
        self.assertEqual(r, s)


class TestSimpleQueries(unittest.TestCase):
    """Test simple queries via a basic pg connection."""

    def setUp(self):
        self.c = connect()

    def tearDown(self):
        self.doCleanups()
        self.c.close()

    def testClassName(self):
        r = self.c.query("select 1")
        self.assertEqual(r.__class__.__name__, 'Query')

    def testModuleName(self):
        r = self.c.query("select 1")
        self.assertEqual(r.__class__.__module__, 'pg')

    def testStr(self):
        q = ("select 1 as a, 'hello' as h, 'w' as world"
            " union select 2, 'xyz', 'uvw'")
        r = self.c.query(q)
        self.assertEqual(str(r),
            'a|  h  |world\n'
            '-+-----+-----\n'
            '1|hello|w    \n'
            '2|xyz  |uvw  \n'
            '(2 rows)')

    def testRepr(self):
        r = repr(self.c.query("select 1"))
        self.assertTrue(r.startswith('<pg.Query object'), r)

    def testSelect0(self):
        q = "select 0"
        self.c.query(q)

    def testSelect0Semicolon(self):
        q = "select 0;"
        self.c.query(q)

    def testSelectDotSemicolon(self):
        q = "select .;"
        self.assertRaises(pg.DatabaseError, self.c.query, q)

    def testGetresult(self):
        q = "select 0"
        result = [(0,)]
        r = self.c.query(q).getresult()
        self.assertIsInstance(r, list)
        v = r[0]
        self.assertIsInstance(v, tuple)
        self.assertIsInstance(v[0], int)
        self.assertEqual(r, result)

    def testGetresultLong(self):
        q = "select 9876543210"
        result = long(9876543210)
        self.assertIsInstance(result, long)
        v = self.c.query(q).getresult()[0][0]
        self.assertIsInstance(v, long)
        self.assertEqual(v, result)

    def testGetresultDecimal(self):
        q = "select 98765432109876543210"
        result = Decimal(98765432109876543210)
        v = self.c.query(q).getresult()[0][0]
        self.assertIsInstance(v, Decimal)
        self.assertEqual(v, result)

    def testGetresultString(self):
        result = 'Hello, world!'
        q = "select '%s'" % result
        v = self.c.query(q).getresult()[0][0]
        self.assertIsInstance(v, str)
        self.assertEqual(v, result)

    def testDictresult(self):
        q = "select 0 as alias0"
        result = [{'alias0': 0}]
        r = self.c.query(q).dictresult()
        self.assertIsInstance(r, list)
        v = r[0]
        self.assertIsInstance(v, dict)
        self.assertIsInstance(v['alias0'], int)
        self.assertEqual(r, result)

    def testDictresultLong(self):
        q = "select 9876543210 as longjohnsilver"
        result = long(9876543210)
        self.assertIsInstance(result, long)
        v = self.c.query(q).dictresult()[0]['longjohnsilver']
        self.assertIsInstance(v, long)
        self.assertEqual(v, result)

    def testDictresultDecimal(self):
        q = "select 98765432109876543210 as longjohnsilver"
        result = Decimal(98765432109876543210)
        v = self.c.query(q).dictresult()[0]['longjohnsilver']
        self.assertIsInstance(v, Decimal)
        self.assertEqual(v, result)

    def testDictresultString(self):
        result = 'Hello, world!'
        q = "select '%s' as greeting" % result
        v = self.c.query(q).dictresult()[0]['greeting']
        self.assertIsInstance(v, str)
        self.assertEqual(v, result)

    def testNamedresult(self):
        q = "select 0 as alias0"
        result = [(0,)]
        r = self.c.query(q).namedresult()
        self.assertEqual(r, result)
        v = r[0]
        self.assertEqual(v._fields, ('alias0',))
        self.assertEqual(v.alias0, 0)

    def testGet3Cols(self):
        q = "select 1,2,3"
        result = [(1, 2, 3)]
        r = self.c.query(q).getresult()
        self.assertEqual(r, result)

    def testGet3DictCols(self):
        q = "select 1 as a,2 as b,3 as c"
        result = [dict(a=1, b=2, c=3)]
        r = self.c.query(q).dictresult()
        self.assertEqual(r, result)

    def testGet3NamedCols(self):
        q = "select 1 as a,2 as b,3 as c"
        result = [(1, 2, 3)]
        r = self.c.query(q).namedresult()
        self.assertEqual(r, result)
        v = r[0]
        self.assertEqual(v._fields, ('a', 'b', 'c'))
        self.assertEqual(v.b, 2)

    def testGet3Rows(self):
        q = "select 3 union select 1 union select 2 order by 1"
        result = [(1,), (2,), (3,)]
        r = self.c.query(q).getresult()
        self.assertEqual(r, result)

    def testGet3DictRows(self):
        q = ("select 3 as alias3"
            " union select 1 union select 2 order by 1")
        result = [{'alias3': 1}, {'alias3': 2}, {'alias3': 3}]
        r = self.c.query(q).dictresult()
        self.assertEqual(r, result)

    def testGet3NamedRows(self):
        q = ("select 3 as alias3"
            " union select 1 union select 2 order by 1")
        result = [(1,), (2,), (3,)]
        r = self.c.query(q).namedresult()
        self.assertEqual(r, result)
        for v in r:
            self.assertEqual(v._fields, ('alias3',))

    def testDictresultNames(self):
        q = "select 'MixedCase' as MixedCaseAlias"
        result = [{'mixedcasealias': 'MixedCase'}]
        r = self.c.query(q).dictresult()
        self.assertEqual(r, result)
        q = "select 'MixedCase' as \"MixedCaseAlias\""
        result = [{'MixedCaseAlias': 'MixedCase'}]
        r = self.c.query(q).dictresult()
        self.assertEqual(r, result)

    def testNamedresultNames(self):
        q = "select 'MixedCase' as MixedCaseAlias"
        result = [('MixedCase',)]
        r = self.c.query(q).namedresult()
        self.assertEqual(r, result)
        v = r[0]
        self.assertEqual(v._fields, ('mixedcasealias',))
        self.assertEqual(v.mixedcasealias, 'MixedCase')
        q = "select 'MixedCase' as \"MixedCaseAlias\""
        r = self.c.query(q).namedresult()
        self.assertEqual(r, result)
        v = r[0]
        self.assertEqual(v._fields, ('MixedCaseAlias',))
        self.assertEqual(v.MixedCaseAlias, 'MixedCase')

    def testBigGetresult(self):
        num_cols = 100
        num_rows = 100
        q = "select " + ','.join(map(str, range(num_cols)))
        q = ' union all '.join((q,) * num_rows)
        r = self.c.query(q).getresult()
        result = [tuple(range(num_cols))] * num_rows
        self.assertEqual(r, result)

    def testListfields(self):
        q = ('select 0 as a, 0 as b, 0 as c,'
            ' 0 as c, 0 as b, 0 as a,'
            ' 0 as lowercase, 0 as UPPERCASE,'
            ' 0 as MixedCase, 0 as "MixedCase",'
            ' 0 as a_long_name_with_underscores,'
            ' 0 as "A long name with Blanks"')
        r = self.c.query(q).listfields()
        result = ('a', 'b', 'c', 'c', 'b', 'a',
            'lowercase', 'uppercase', 'mixedcase', 'MixedCase',
            'a_long_name_with_underscores',
            'A long name with Blanks')
        self.assertEqual(r, result)

    def testFieldname(self):
        q = "select 0 as z, 0 as a, 0 as x, 0 as y"
        r = self.c.query(q).fieldname(2)
        self.assertEqual(r, 'x')
        r = self.c.query(q).fieldname(3)
        self.assertEqual(r, 'y')

    def testFieldnum(self):
        q = "select 1 as x"
        self.assertRaises(ValueError, self.c.query(q).fieldnum, 'y')
        q = "select 1 as x"
        r = self.c.query(q).fieldnum('x')
        self.assertIsInstance(r, int)
        self.assertEqual(r, 0)
        q = "select 0 as z, 0 as a, 0 as x, 0 as y"
        r = self.c.query(q).fieldnum('x')
        self.assertIsInstance(r, int)
        self.assertEqual(r, 2)
        r = self.c.query(q).fieldnum('y')
        self.assertIsInstance(r, int)
        self.assertEqual(r, 3)

    def testNtuples(self):
        q = "select 1 where false"
        r = self.c.query(q).ntuples()
        self.assertIsInstance(r, int)
        self.assertEqual(r, 0)
        q = ("select 1 as a, 2 as b, 3 as c, 4 as d"
            " union select 5 as a, 6 as b, 7 as c, 8 as d")
        r = self.c.query(q).ntuples()
        self.assertIsInstance(r, int)
        self.assertEqual(r, 2)
        q = ("select 1 union select 2 union select 3"
            " union select 4 union select 5 union select 6")
        r = self.c.query(q).ntuples()
        self.assertIsInstance(r, int)
        self.assertEqual(r, 6)

    def testQuery(self):
        query = self.c.query
        query("drop table if exists test_table")
        self.addCleanup(query, "drop table test_table")
        q = "create table test_table (n integer) with oids"
        r = query(q)
        self.assertIsNone(r)
        q = "insert into test_table values (1)"
        r = query(q)
        self.assertIsInstance(r, int)
        q = "insert into test_table select 2"
        r = query(q)
        self.assertIsInstance(r, int)
        oid = r
        q = "select oid from test_table where n=2"
        r = query(q).getresult()
        self.assertEqual(len(r), 1)
        r = r[0]
        self.assertEqual(len(r), 1)
        r = r[0]
        self.assertIsInstance(r, int)
        self.assertEqual(r, oid)
        q = "insert into test_table select 3 union select 4 union select 5"
        r = query(q)
        self.assertIsInstance(r, str)
        self.assertEqual(r, '3')
        q = "update test_table set n=4 where n<5"
        r = query(q)
        self.assertIsInstance(r, str)
        self.assertEqual(r, '4')
        q = "delete from test_table"
        r = query(q)
        self.assertIsInstance(r, str)
        self.assertEqual(r, '5')


class TestUnicodeQueries(unittest.TestCase):
    """Test unicode strings as queries via a basic pg connection."""

    def setUp(self):
        self.c = connect()
        self.c.query('set client_encoding=utf8')

    def tearDown(self):
        self.c.close()

    def testGetresulAscii(self):
        result = u'Hello, world!'
        q = u"select '%s'" % result
        v = self.c.query(q).getresult()[0][0]
        self.assertIsInstance(v, str)
        self.assertEqual(v, result)

    def testDictresulAscii(self):
        result = u'Hello, world!'
        q = u"select '%s' as greeting" % result
        v = self.c.query(q).dictresult()[0]['greeting']
        self.assertIsInstance(v, str)
        self.assertEqual(v, result)

    def testGetresultUtf8(self):
        result = u'Hello, wörld & мир!'
        q = u"select '%s'" % result
        if not unicode_strings:
            result = result.encode('utf8')
        # pass the query as unicode
        try:
            v = self.c.query(q).getresult()[0][0]
        except(pg.DataError, pg.NotSupportedError):
            self.skipTest("database does not support utf8")
        self.assertIsInstance(v, str)
        self.assertEqual(v, result)
        q = q.encode('utf8')
        # pass the query as bytes
        v = self.c.query(q).getresult()[0][0]
        self.assertIsInstance(v, str)
        self.assertEqual(v, result)

    def testDictresultUtf8(self):
        result = u'Hello, wörld & мир!'
        q = u"select '%s' as greeting" % result
        if not unicode_strings:
            result = result.encode('utf8')
        try:
            v = self.c.query(q).dictresult()[0]['greeting']
        except (pg.DataError, pg.NotSupportedError):
            self.skipTest("database does not support utf8")
        self.assertIsInstance(v, str)
        self.assertEqual(v, result)
        q = q.encode('utf8')
        v = self.c.query(q).dictresult()[0]['greeting']
        self.assertIsInstance(v, str)
        self.assertEqual(v, result)

    def testDictresultLatin1(self):
        try:
            self.c.query('set client_encoding=latin1')
        except (pg.DataError, pg.NotSupportedError):
            self.skipTest("database does not support latin1")
        result = u'Hello, wörld!'
        q = u"select '%s'" % result
        if not unicode_strings:
            result = result.encode('latin1')
        v = self.c.query(q).getresult()[0][0]
        self.assertIsInstance(v, str)
        self.assertEqual(v, result)
        q = q.encode('latin1')
        v = self.c.query(q).getresult()[0][0]
        self.assertIsInstance(v, str)
        self.assertEqual(v, result)

    def testDictresultLatin1(self):
        try:
            self.c.query('set client_encoding=latin1')
        except (pg.DataError, pg.NotSupportedError):
            self.skipTest("database does not support latin1")
        result = u'Hello, wörld!'
        q = u"select '%s' as greeting" % result
        if not unicode_strings:
            result = result.encode('latin1')
        v = self.c.query(q).dictresult()[0]['greeting']
        self.assertIsInstance(v, str)
        self.assertEqual(v, result)
        q = q.encode('latin1')
        v = self.c.query(q).dictresult()[0]['greeting']
        self.assertIsInstance(v, str)
        self.assertEqual(v, result)

    def testGetresultCyrillic(self):
        try:
            self.c.query('set client_encoding=iso_8859_5')
        except (pg.DataError, pg.NotSupportedError):
            self.skipTest("database does not support cyrillic")
        result = u'Hello, мир!'
        q = u"select '%s'" % result
        if not unicode_strings:
            result = result.encode('cyrillic')
        v = self.c.query(q).getresult()[0][0]
        self.assertIsInstance(v, str)
        self.assertEqual(v, result)
        q = q.encode('cyrillic')
        v = self.c.query(q).getresult()[0][0]
        self.assertIsInstance(v, str)
        self.assertEqual(v, result)

    def testDictresultCyrillic(self):
        try:
            self.c.query('set client_encoding=iso_8859_5')
        except (pg.DataError, pg.NotSupportedError):
            self.skipTest("database does not support cyrillic")
        result = u'Hello, мир!'
        q = u"select '%s' as greeting" % result
        if not unicode_strings:
            result = result.encode('cyrillic')
        v = self.c.query(q).dictresult()[0]['greeting']
        self.assertIsInstance(v, str)
        self.assertEqual(v, result)
        q = q.encode('cyrillic')
        v = self.c.query(q).dictresult()[0]['greeting']
        self.assertIsInstance(v, str)
        self.assertEqual(v, result)

    def testGetresultLatin9(self):
        try:
            self.c.query('set client_encoding=latin9')
        except (pg.DataError, pg.NotSupportedError):
            self.skipTest("database does not support latin9")
        result = u'smœrebrœd with pražská šunka (pay in ¢, £, €, or ¥)'
        q = u"select '%s'" % result
        if not unicode_strings:
            result = result.encode('latin9')
        v = self.c.query(q).getresult()[0][0]
        self.assertIsInstance(v, str)
        self.assertEqual(v, result)
        q = q.encode('latin9')
        v = self.c.query(q).getresult()[0][0]
        self.assertIsInstance(v, str)
        self.assertEqual(v, result)

    def testDictresultLatin9(self):
        try:
            self.c.query('set client_encoding=latin9')
        except (pg.DataError, pg.NotSupportedError):
            self.skipTest("database does not support latin9")
        result = u'smœrebrœd with pražská šunka (pay in ¢, £, €, or ¥)'
        q = u"select '%s' as menu" % result
        if not unicode_strings:
            result = result.encode('latin9')
        v = self.c.query(q).dictresult()[0]['menu']
        self.assertIsInstance(v, str)
        self.assertEqual(v, result)
        q = q.encode('latin9')
        v = self.c.query(q).dictresult()[0]['menu']
        self.assertIsInstance(v, str)
        self.assertEqual(v, result)


class TestParamQueries(unittest.TestCase):
    """Test queries with parameters via a basic pg connection."""

    def setUp(self):
        self.c = connect()
        self.c.query('set client_encoding=utf8')

    def tearDown(self):
        self.c.close()

    def testQueryWithNoneParam(self):
        self.assertRaises(TypeError, self.c.query, "select $1", None)
        self.assertRaises(TypeError, self.c.query, "select $1+$2", None, None)
        self.assertEqual(self.c.query("select $1::integer", (None,)
            ).getresult(), [(None,)])
        self.assertEqual(self.c.query("select $1::text", [None]
            ).getresult(), [(None,)])
        self.assertEqual(self.c.query("select $1::text", [[None]]
            ).getresult(), [(None,)])

    def testQueryWithBoolParams(self, bool_enabled=None):
        query = self.c.query
        if bool_enabled is not None:
            bool_enabled_default = pg.get_bool()
            pg.set_bool(bool_enabled)
        try:
            bool_on = bool_enabled or bool_enabled is None
            v_false, v_true = (False, True) if bool_on else 'ft'
            r_false, r_true = [(v_false,)], [(v_true,)]
            self.assertEqual(query("select false").getresult(), r_false)
            self.assertEqual(query("select true").getresult(), r_true)
            q = "select $1::bool"
            self.assertEqual(query(q, (None,)).getresult(), [(None,)])
            self.assertEqual(query(q, ('f',)).getresult(), r_false)
            self.assertEqual(query(q, ('t',)).getresult(), r_true)
            self.assertEqual(query(q, ('false',)).getresult(), r_false)
            self.assertEqual(query(q, ('true',)).getresult(), r_true)
            self.assertEqual(query(q, ('n',)).getresult(), r_false)
            self.assertEqual(query(q, ('y',)).getresult(), r_true)
            self.assertEqual(query(q, (0,)).getresult(), r_false)
            self.assertEqual(query(q, (1,)).getresult(), r_true)
            self.assertEqual(query(q, (False,)).getresult(), r_false)
            self.assertEqual(query(q, (True,)).getresult(), r_true)
        finally:
            if bool_enabled is not None:
                pg.set_bool(bool_enabled_default)

    def testQueryWithBoolParamsNotDefault(self):
        self.testQueryWithBoolParams(bool_enabled=not pg.get_bool())

    def testQueryWithIntParams(self):
        query = self.c.query
        self.assertEqual(query("select 1+1").getresult(), [(2,)])
        self.assertEqual(query("select 1+$1", (1,)).getresult(), [(2,)])
        self.assertEqual(query("select 1+$1", [1]).getresult(), [(2,)])
        self.assertEqual(query("select $1::integer", (2,)).getresult(), [(2,)])
        self.assertEqual(query("select $1::text", (2,)).getresult(), [('2',)])
        self.assertEqual(query("select 1+$1::numeric", [1]).getresult(),
            [(Decimal('2'),)])
        self.assertEqual(query("select 1, $1::integer", (2,)
            ).getresult(), [(1, 2)])
        self.assertEqual(query("select 1 union select $1::integer", (2,)
            ).getresult(), [(1,), (2,)])
        self.assertEqual(query("select $1::integer+$2", (1, 2)
            ).getresult(), [(3,)])
        self.assertEqual(query("select $1::integer+$2", [1, 2]
            ).getresult(), [(3,)])
        self.assertEqual(query("select 0+$1+$2+$3+$4+$5+$6", list(range(6))
            ).getresult(), [(15,)])

    def testQueryWithStrParams(self):
        query = self.c.query
        self.assertEqual(query("select $1||', world!'", ('Hello',)
            ).getresult(), [('Hello, world!',)])
        self.assertEqual(query("select $1||', world!'", ['Hello']
            ).getresult(), [('Hello, world!',)])
        self.assertEqual(query("select $1||', '||$2||'!'", ('Hello', 'world'),
            ).getresult(), [('Hello, world!',)])
        self.assertEqual(query("select $1::text", ('Hello, world!',)
            ).getresult(), [('Hello, world!',)])
        self.assertEqual(query("select $1::text,$2::text", ('Hello', 'world')
            ).getresult(), [('Hello', 'world')])
        self.assertEqual(query("select $1::text,$2::text", ['Hello', 'world']
            ).getresult(), [('Hello', 'world')])
        self.assertEqual(query("select $1::text union select $2::text",
            ('Hello', 'world')).getresult(), [('Hello',), ('world',)])
        try:
            query("select 'wörld'")
        except (pg.DataError, pg.NotSupportedError):
            self.skipTest('database does not support utf8')
        self.assertEqual(query("select $1||', '||$2||'!'", ('Hello',
            'w\xc3\xb6rld')).getresult(), [('Hello, w\xc3\xb6rld!',)])

    def testQueryWithUnicodeParams(self):
        query = self.c.query
        try:
            query('set client_encoding=utf8')
            query("select 'wörld'").getresult()[0][0] == 'wörld'
        except (pg.DataError, pg.NotSupportedError):
            self.skipTest("database does not support utf8")
        self.assertEqual(query("select $1||', '||$2||'!'",
            ('Hello', u'wörld')).getresult(), [('Hello, wörld!',)])

    def testQueryWithUnicodeParamsLatin1(self):
        query = self.c.query
        try:
            query('set client_encoding=latin1')
            query("select 'wörld'").getresult()[0][0] == 'wörld'
        except (pg.DataError, pg.NotSupportedError):
            self.skipTest("database does not support latin1")
        r = query("select $1||', '||$2||'!'", ('Hello', u'wörld')).getresult()
        if unicode_strings:
            self.assertEqual(r, [('Hello, wörld!',)])
        else:
            self.assertEqual(r, [(u'Hello, wörld!'.encode('latin1'),)])
        self.assertRaises(UnicodeError, query, "select $1||', '||$2||'!'",
            ('Hello', u'мир'))
        query('set client_encoding=iso_8859_1')
        r = query("select $1||', '||$2||'!'",
            ('Hello', u'wörld')).getresult()
        if unicode_strings:
            self.assertEqual(r, [('Hello, wörld!',)])
        else:
            self.assertEqual(r, [(u'Hello, wörld!'.encode('latin1'),)])
        self.assertRaises(UnicodeError, query, "select $1||', '||$2||'!'",
            ('Hello', u'мир'))
        query('set client_encoding=sql_ascii')
        self.assertRaises(UnicodeError, query, "select $1||', '||$2||'!'",
            ('Hello', u'wörld'))

    def testQueryWithUnicodeParamsCyrillic(self):
        query = self.c.query
        try:
            query('set client_encoding=iso_8859_5')
            query("select 'мир'").getresult()[0][0] == 'мир'
        except (pg.DataError, pg.NotSupportedError):
            self.skipTest("database does not support cyrillic")
        self.assertRaises(UnicodeError, query, "select $1||', '||$2||'!'",
            ('Hello', u'wörld'))
        r = query("select $1||', '||$2||'!'",
            ('Hello', u'мир')).getresult()
        if unicode_strings:
            self.assertEqual(r, [('Hello, мир!',)])
        else:
            self.assertEqual(r, [(u'Hello, мир!'.encode('cyrillic'),)])
        query('set client_encoding=sql_ascii')
        self.assertRaises(UnicodeError, query, "select $1||', '||$2||'!'",
            ('Hello', u'мир!'))

    def testQueryWithMixedParams(self):
        self.assertEqual(self.c.query("select $1+2,$2||', world!'",
            (1, 'Hello'),).getresult(), [(3, 'Hello, world!')])
        self.assertEqual(self.c.query("select $1::integer,$2::date,$3::text",
            (4711, None, 'Hello!'),).getresult(), [(4711, None, 'Hello!')])

    def testQueryWithDuplicateParams(self):
        self.assertRaises(pg.ProgrammingError,
            self.c.query, "select $1+$1", (1,))
        self.assertRaises(pg.ProgrammingError,
            self.c.query, "select $1+$1", (1, 2))

    def testQueryWithZeroParams(self):
        self.assertEqual(self.c.query("select 1+1", []
            ).getresult(), [(2,)])

    def testQueryWithGarbage(self):
        garbage = r"'\{}+()-#[]oo324"
        self.assertEqual(self.c.query("select $1::text AS garbage", (garbage,)
            ).dictresult(), [{'garbage': garbage}])


class TestQueryResultTypes(unittest.TestCase):
    """Test proper result types via a basic pg connection."""

    def setUp(self):
        self.c = connect()
        self.c.query('set client_encoding=utf8')
        self.c.query("set datestyle='ISO,YMD'")

    def tearDown(self):
        self.c.close()

    def assert_proper_cast(self, value, pgtype, pytype):
        q = 'select $1::%s' % (pgtype,)
        try:
            r = self.c.query(q, (value,)).getresult()[0][0]
        except pg.ProgrammingError:
            if pgtype in ('json', 'jsonb'):
                self.skipTest('database does not support json')
        self.assertIsInstance(r, pytype)
        if isinstance(value, str):
            if not value or ' ' in value or '{' in value:
                value = '"%s"' % value
        value = '{%s}' % value
        r = self.c.query(q + '[]', (value,)).getresult()[0][0]
        if pgtype.startswith(('date', 'time', 'interval')):
            # arrays of these are casted by the DB wrapper only
            self.assertEqual(r, value)
        else:
            self.assertIsInstance(r, list)
            self.assertEqual(len(r), 1)
            self.assertIsInstance(r[0], pytype)

    def testInt(self):
        self.assert_proper_cast(0, 'int', int)
        self.assert_proper_cast(0, 'smallint', int)
        self.assert_proper_cast(0, 'oid', int)
        self.assert_proper_cast(0, 'cid', int)
        self.assert_proper_cast(0, 'xid', int)

    def testLong(self):
        self.assert_proper_cast(0, 'bigint', long)

    def testFloat(self):
        self.assert_proper_cast(0, 'float', float)
        self.assert_proper_cast(0, 'real', float)
        self.assert_proper_cast(0, 'double', float)
        self.assert_proper_cast(0, 'double precision', float)
        self.assert_proper_cast('infinity', 'float', float)

    def testFloat(self):
        decimal = pg.get_decimal()
        self.assert_proper_cast(decimal(0), 'numeric', decimal)
        self.assert_proper_cast(decimal(0), 'decimal', decimal)

    def testMoney(self):
        decimal = pg.get_decimal()
        self.assert_proper_cast(decimal('0'), 'money', decimal)

    def testBool(self):
        bool_type = bool if pg.get_bool() else str
        self.assert_proper_cast('f', 'bool', bool_type)

    def testDate(self):
        self.assert_proper_cast('1956-01-31', 'date', str)
        self.assert_proper_cast('10:20:30', 'interval', str)
        self.assert_proper_cast('08:42:15', 'time', str)
        self.assert_proper_cast('08:42:15+00', 'timetz', str)
        self.assert_proper_cast('1956-01-31 08:42:15', 'timestamp', str)
        self.assert_proper_cast('1956-01-31 08:42:15+00', 'timestamptz', str)

    def testText(self):
        self.assert_proper_cast('', 'text', str)
        self.assert_proper_cast('', 'char', str)
        self.assert_proper_cast('', 'bpchar', str)
        self.assert_proper_cast('', 'varchar', str)

    def testBytea(self):
        self.assert_proper_cast('', 'bytea', bytes)

    def testJson(self):
        self.assert_proper_cast('{}', 'json', dict)


class TestInserttable(unittest.TestCase):
    """Test inserttable method."""

    cls_set_up = False

    @classmethod
    def setUpClass(cls):
        c = connect()
        c.query("drop table if exists test cascade")
        c.query("create table test ("
            "i2 smallint, i4 integer, i8 bigint, b boolean, dt date, ti time,"
            "d numeric, f4 real, f8 double precision, m money,"
            "c char(1), v4 varchar(4), c4 char(4), t text)")
        # Check whether the test database uses SQL_ASCII - this means
        # that it does not consider encoding when calculating lengths.
        c.query("set client_encoding=utf8")
        try:
            c.query("select 'ä'")
        except (pg.DataError, pg.NotSupportedError):
            cls.has_encoding = False
        else:
            cls.has_encoding = c.query(
                "select length('ä') - length('a')").getresult()[0][0] == 0
        c.close()
        cls.cls_set_up = True

    @classmethod
    def tearDownClass(cls):
        c = connect()
        c.query("drop table test cascade")
        c.close()

    def setUp(self):
        self.assertTrue(self.cls_set_up)
        self.c = connect()
        self.c.query("set client_encoding=utf8")
        self.c.query("set datestyle='ISO,YMD'")
        self.c.query("set lc_monetary='C'")

    def tearDown(self):
        self.c.query("truncate table test")
        self.c.close()

    data = [
        (-1, -1, long(-1), True, '1492-10-12', '08:30:00',
            -1.2345, -1.75, -1.875, '-1.25', '-', 'r?', '!u', 'xyz'),
        (0, 0, long(0), False, '1607-04-14', '09:00:00',
            0.0, 0.0, 0.0, '0.0', ' ', '0123', '4567', '890'),
        (1, 1, long(1), True, '1801-03-04', '03:45:00',
            1.23456, 1.75, 1.875, '1.25', 'x', 'bc', 'cdef', 'g'),
        (2, 2, long(2), False, '1903-12-17', '11:22:00',
            2.345678, 2.25, 2.125, '2.75', 'y', 'q', 'ijk', 'mnop\nstux!')]

    @classmethod
    def db_len(cls, s, encoding):
        if cls.has_encoding:
            s = s if isinstance(s, unicode) else s.decode(encoding)
        else:
            s = s.encode(encoding) if isinstance(s, unicode) else s
        return len(s)

    def get_back(self, encoding='utf-8'):
        """Convert boolean and decimal values back."""
        data = []
        for row in self.c.query("select * from test order by 1").getresult():
            self.assertIsInstance(row, tuple)
            row = list(row)
            if row[0] is not None:  # smallint
                self.assertIsInstance(row[0], int)
            if row[1] is not None:  # integer
                self.assertIsInstance(row[1], int)
            if row[2] is not None:  # bigint
                self.assertIsInstance(row[2], long)
            if row[3] is not None:  # boolean
                self.assertIsInstance(row[3], bool)
            if row[4] is not None:  # date
                self.assertIsInstance(row[4], str)
                self.assertTrue(row[4].replace('-', '').isdigit())
            if row[5] is not None:  # time
                self.assertIsInstance(row[5], str)
                self.assertTrue(row[5].replace(':', '').isdigit())
            if row[6] is not None:  # numeric
                self.assertIsInstance(row[6], Decimal)
                row[6] = float(row[6])
            if row[7] is not None:  # real
                self.assertIsInstance(row[7], float)
            if row[8] is not None:  # double precision
                self.assertIsInstance(row[8], float)
                row[8] = float(row[8])
            if row[9] is not None:  # money
                self.assertIsInstance(row[9], Decimal)
                row[9] = str(float(row[9]))
            if row[10] is not None:  # char(1)
                self.assertIsInstance(row[10], str)
                self.assertEqual(self.db_len(row[10], encoding), 1)
            if row[11] is not None:  # varchar(4)
                self.assertIsInstance(row[11], str)
                self.assertLessEqual(self.db_len(row[11], encoding), 4)
            if row[12] is not None:  # char(4)
                self.assertIsInstance(row[12], str)
                self.assertEqual(self.db_len(row[12], encoding), 4)
                row[12] = row[12].rstrip()
            if row[13] is not None:  # text
                self.assertIsInstance(row[13], str)
            row = tuple(row)
            data.append(row)
        return data

    def testInserttable1Row(self):
        data = self.data[2:3]
        self.c.inserttable('test', data)
        self.assertEqual(self.get_back(), data)

    def testInserttable4Rows(self):
        data = self.data
        self.c.inserttable('test', data)
        self.assertEqual(self.get_back(), data)

    def testInserttableMultipleRows(self):
        num_rows = 100
        data = self.data[2:3] * num_rows
        self.c.inserttable('test', data)
        r = self.c.query("select count(*) from test").getresult()[0][0]
        self.assertEqual(r, num_rows)

    def testInserttableMultipleCalls(self):
        num_rows = 10
        data = self.data[2:3]
        for _i in range(num_rows):
            self.c.inserttable('test', data)
        r = self.c.query("select count(*) from test").getresult()[0][0]
        self.assertEqual(r, num_rows)

    def testInserttableNullValues(self):
        data = [(None,) * 14] * 100
        self.c.inserttable('test', data)
        self.assertEqual(self.get_back(), data)

    def testInserttableMaxValues(self):
        data = [(2 ** 15 - 1, int(2 ** 31 - 1), long(2 ** 31 - 1),
            True, '2999-12-31', '11:59:59', 1e99,
            1.0 + 1.0 / 32, 1.0 + 1.0 / 32, None,
            "1", "1234", "1234", "1234" * 100)]
        self.c.inserttable('test', data)
        self.assertEqual(self.get_back(), data)

    def testInserttableByteValues(self):
        try:
            self.c.query("select '€', 'käse', 'сыр', 'pont-l''évêque'")
        except pg.DataError:
            self.skipTest("database does not support utf8")
        # non-ascii chars do not fit in char(1) when there is no encoding
        c = u'€' if self.has_encoding else u'$'
        row_unicode = (0, 0, long(0), False, u'1970-01-01', u'00:00:00',
            0.0, 0.0, 0.0, u'0.0',
            c, u'bäd', u'bäd', u"käse сыр pont-l'évêque")
        row_bytes = tuple(s.encode('utf-8')
            if isinstance(s, unicode) else s for s in row_unicode)
        data = [row_bytes] * 2
        self.c.inserttable('test', data)
        if unicode_strings:
            data = [row_unicode] * 2
        self.assertEqual(self.get_back(), data)

    def testInserttableUnicodeUtf8(self):
        try:
            self.c.query("select '€', 'käse', 'сыр', 'pont-l''évêque'")
        except pg.DataError:
            self.skipTest("database does not support utf8")
        # non-ascii chars do not fit in char(1) when there is no encoding
        c = u'€' if self.has_encoding else u'$'
        row_unicode = (0, 0, long(0), False, u'1970-01-01', u'00:00:00',
            0.0, 0.0, 0.0, u'0.0',
            c, u'bäd', u'bäd', u"käse сыр pont-l'évêque")
        data = [row_unicode] * 2
        self.c.inserttable('test', data)
        if not unicode_strings:
            row_bytes = tuple(s.encode('utf-8')
                if isinstance(s, unicode) else s for s in row_unicode)
            data = [row_bytes] * 2
        self.assertEqual(self.get_back(), data)

    def testInserttableUnicodeLatin1(self):

        try:
            self.c.query("set client_encoding=latin1")
            self.c.query("select '¥'")
        except (pg.DataError, pg.NotSupportedError):
            self.skipTest("database does not support latin1")
        # non-ascii chars do not fit in char(1) when there is no encoding
        c = u'€' if self.has_encoding else u'$'
        row_unicode = (0, 0, long(0), False, u'1970-01-01', u'00:00:00',
            0.0, 0.0, 0.0, u'0.0',
            c, u'bäd', u'bäd', u"for käse and pont-l'évêque pay in €")
        data = [row_unicode]
        # cannot encode € sign with latin1 encoding
        self.assertRaises(UnicodeEncodeError, self.c.inserttable, 'test', data)
        row_unicode = tuple(s.replace(u'€', u'¥')
            if isinstance(s, unicode) else s for s in row_unicode)
        data = [row_unicode] * 2
        self.c.inserttable('test', data)
        if not unicode_strings:
            row_bytes = tuple(s.encode('latin1')
                if isinstance(s, unicode) else s for s in row_unicode)
            data = [row_bytes] * 2
        self.assertEqual(self.get_back('latin1'), data)

    def testInserttableUnicodeLatin9(self):
        try:
            self.c.query("set client_encoding=latin9")
            self.c.query("select '€'")
        except (pg.DataError, pg.NotSupportedError):
            self.skipTest("database does not support latin9")
            return
        # non-ascii chars do not fit in char(1) when there is no encoding
        c = u'€' if self.has_encoding else u'$'
        row_unicode = (0, 0, long(0), False, u'1970-01-01', u'00:00:00',
            0.0, 0.0, 0.0, u'0.0',
            c, u'bäd', u'bäd', u"for käse and pont-l'évêque pay in €")
        data = [row_unicode] * 2
        self.c.inserttable('test', data)
        if not unicode_strings:
            row_bytes = tuple(s.encode('latin9')
                if isinstance(s, unicode) else s for s in row_unicode)
            data = [row_bytes] * 2
        self.assertEqual(self.get_back('latin9'), data)

    def testInserttableNoEncoding(self):
        self.c.query("set client_encoding=sql_ascii")
        # non-ascii chars do not fit in char(1) when there is no encoding
        c = u'€' if self.has_encoding else u'$'
        row_unicode = (0, 0, long(0), False, u'1970-01-01', u'00:00:00',
            0.0, 0.0, 0.0, u'0.0',
            c, u'bäd', u'bäd', u"for käse and pont-l'évêque pay in €")
        data = [row_unicode]
        # cannot encode non-ascii unicode without a specific encoding
        self.assertRaises(UnicodeEncodeError, self.c.inserttable, 'test', data)


class TestDirectSocketAccess(unittest.TestCase):
    """Test copy command with direct socket access."""

    cls_set_up = False

    @classmethod
    def setUpClass(cls):
        c = connect()
        c.query("drop table if exists test cascade")
        c.query("create table test (i int, v varchar(16))")
        c.close()
        cls.cls_set_up = True

    @classmethod
    def tearDownClass(cls):
        c = connect()
        c.query("drop table test cascade")
        c.close()

    def setUp(self):
        self.assertTrue(self.cls_set_up)
        self.c = connect()
        self.c.query("set client_encoding=utf8")

    def tearDown(self):
        self.c.query("truncate table test")
        self.c.close()

    def testPutline(self):
        putline = self.c.putline
        query = self.c.query
        data = list(enumerate("apple pear plum cherry banana".split()))
        query("copy test from stdin")
        try:
            for i, v in data:
                putline("%d\t%s\n" % (i, v))
            putline("\\.\n")
        finally:
            self.c.endcopy()
        r = query("select * from test").getresult()
        self.assertEqual(r, data)

    def testPutlineBytesAndUnicode(self):
        putline = self.c.putline
        query = self.c.query
        try:
            query("select 'käse+würstel'")
        except (pg.DataError, pg.NotSupportedError):
            self.skipTest('database does not support utf8')
        query("copy test from stdin")
        try:
            putline(u"47\tkäse\n".encode('utf8'))
            putline("35\twürstel\n")
            putline(b"\\.\n")
        finally:
            self.c.endcopy()
        r = query("select * from test").getresult()
        self.assertEqual(r, [(47, 'käse'), (35, 'würstel')])

    def testGetline(self):
        getline = self.c.getline
        query = self.c.query
        data = list(enumerate("apple banana pear plum strawberry".split()))
        n = len(data)
        self.c.inserttable('test', data)
        query("copy test to stdout")
        try:
            for i in range(n + 2):
                v = getline()
                if i < n:
                    self.assertEqual(v, '%d\t%s' % data[i])
                elif i == n:
                    self.assertEqual(v, '\\.')
                else:
                    self.assertIsNone(v)
        finally:
            try:
                self.c.endcopy()
            except IOError:
                pass

    def testGetlineBytesAndUnicode(self):
        getline = self.c.getline
        query = self.c.query
        try:
            query("select 'käse+würstel'")
        except (pg.DataError, pg.NotSupportedError):
            self.skipTest('database does not support utf8')
        data = [(54, u'käse'.encode('utf8')), (73, u'würstel')]
        self.c.inserttable('test', data)
        query("copy test to stdout")
        try:
            v = getline()
            self.assertIsInstance(v, str)
            self.assertEqual(v, '54\tkäse')
            v = getline()
            self.assertIsInstance(v, str)
            self.assertEqual(v, '73\twürstel')
            self.assertEqual(getline(), '\\.')
            self.assertIsNone(getline())
        finally:
            try:
                self.c.endcopy()
            except IOError:
                pass

    def testParameterChecks(self):
        self.assertRaises(TypeError, self.c.putline)
        self.assertRaises(TypeError, self.c.getline, 'invalid')
        self.assertRaises(TypeError, self.c.endcopy, 'invalid')


class TestNotificatons(unittest.TestCase):
    """Test notification support."""

    def setUp(self):
        self.c = connect()

    def tearDown(self):
        self.doCleanups()
        self.c.close()

    def testGetNotify(self):
        getnotify = self.c.getnotify
        query = self.c.query
        self.assertIsNone(getnotify())
        query('listen test_notify')
        try:
            self.assertIsNone(self.c.getnotify())
            query("notify test_notify")
            r = getnotify()
            self.assertIsInstance(r, tuple)
            self.assertEqual(len(r), 3)
            self.assertIsInstance(r[0], str)
            self.assertIsInstance(r[1], int)
            self.assertIsInstance(r[2], str)
            self.assertEqual(r[0], 'test_notify')
            self.assertEqual(r[2], '')
            self.assertIsNone(self.c.getnotify())
            query("notify test_notify, 'test_payload'")
            r = getnotify()
            self.assertTrue(isinstance(r, tuple))
            self.assertEqual(len(r), 3)
            self.assertIsInstance(r[0], str)
            self.assertIsInstance(r[1], int)
            self.assertIsInstance(r[2], str)
            self.assertEqual(r[0], 'test_notify')
            self.assertEqual(r[2], 'test_payload')
            self.assertIsNone(getnotify())
        finally:
            query('unlisten test_notify')

    def testGetNoticeReceiver(self):
        self.assertIsNone(self.c.get_notice_receiver())

    def testSetNoticeReceiver(self):
        self.assertRaises(TypeError, self.c.set_notice_receiver, 42)
        self.assertRaises(TypeError, self.c.set_notice_receiver, 'invalid')
        self.assertIsNone(self.c.set_notice_receiver(lambda notice: None))
        self.assertIsNone(self.c.set_notice_receiver(None))

    def testSetAndGetNoticeReceiver(self):
        r = lambda notice: None
        self.assertIsNone(self.c.set_notice_receiver(r))
        self.assertIs(self.c.get_notice_receiver(), r)
        self.assertIsNone(self.c.set_notice_receiver(None))
        self.assertIsNone(self.c.get_notice_receiver())

    def testNoticeReceiver(self):
        self.addCleanup(self.c.query, 'drop function bilbo_notice();')
        self.c.query('''create function bilbo_notice() returns void AS $$
            begin
                raise warning 'Bilbo was here!';
            end;
            $$ language plpgsql''')
        received = {}

        def notice_receiver(notice):
            for attr in dir(notice):
                if attr.startswith('__'):
                    continue
                value = getattr(notice, attr)
                if isinstance(value, str):
                    value = value.replace('WARNUNG', 'WARNING')
                received[attr] = value

        self.c.set_notice_receiver(notice_receiver)
        self.c.query('select bilbo_notice()')
        self.assertEqual(received, dict(
            pgcnx=self.c, message='WARNING:  Bilbo was here!\n',
            severity='WARNING', primary='Bilbo was here!',
            detail=None, hint=None))


class TestConfigFunctions(unittest.TestCase):
    """Test the functions for changing default settings.

    To test the effect of most of these functions, we need a database
    connection.  That's why they are covered in this test module.
    """

    def setUp(self):
        self.c = connect()
        self.c.query("set client_encoding=utf8")
        self.c.query('set bytea_output=hex')
        self.c.query("set lc_monetary='C'")

    def tearDown(self):
        self.c.close()

    def testGetDecimalPoint(self):
        point = pg.get_decimal_point()
        # error if a parameter is passed
        self.assertRaises(TypeError, pg.get_decimal_point, point)
        self.assertIsInstance(point, str)
        self.assertEqual(point, '.')  # the default setting
        pg.set_decimal_point(',')
        try:
            r = pg.get_decimal_point()
        finally:
            pg.set_decimal_point(point)
        self.assertIsInstance(r, str)
        self.assertEqual(r, ',')
        pg.set_decimal_point("'")
        try:
            r = pg.get_decimal_point()
        finally:
            pg.set_decimal_point(point)
        self.assertIsInstance(r, str)
        self.assertEqual(r, "'")
        pg.set_decimal_point('')
        try:
            r = pg.get_decimal_point()
        finally:
            pg.set_decimal_point(point)
        self.assertIsNone(r)
        pg.set_decimal_point(None)
        try:
            r = pg.get_decimal_point()
        finally:
            pg.set_decimal_point(point)
        self.assertIsNone(r)

    def testSetDecimalPoint(self):
        d = pg.Decimal
        point = pg.get_decimal_point()
        self.assertRaises(TypeError, pg.set_decimal_point)
        # error if decimal point is not a string
        self.assertRaises(TypeError, pg.set_decimal_point, 0)
        # error if more than one decimal point passed
        self.assertRaises(TypeError, pg.set_decimal_point, '.', ',')
        self.assertRaises(TypeError, pg.set_decimal_point, '.,')
        # error if decimal point is not a punctuation character
        self.assertRaises(TypeError, pg.set_decimal_point, '0')
        query = self.c.query
        # check that money values are interpreted as decimal values
        # only if decimal_point is set, and that the result is correct
        # only if it is set suitable for the current lc_monetary setting
        select_money = "select '34.25'::money"
        proper_money = d('34.25')
        bad_money = d('3425')
        en_locales = 'en', 'en_US', 'en_US.utf8', 'en_US.UTF-8'
        en_money = '$34.25', '$ 34.25', '34.25$', '34.25 $', '34.25 Dollar'
        de_locales = 'de', 'de_DE', 'de_DE.utf8', 'de_DE.UTF-8'
        de_money = ('34,25€', '34,25 €', '€34,25', '€ 34,25',
            'EUR34,25', 'EUR 34,25', '34,25 EUR', '34,25 Euro', '34,25 DM')
        # first try with English localization (using the point)
        for lc in en_locales:
            try:
                query("set lc_monetary='%s'" % lc)
            except pg.DataError:
                pass
            else:
                break
        else:
            self.skipTest("cannot set English money locale")
        try:
            r = query(select_money)
        except pg.DataError:
            # this can happen if the currency signs cannot be
            # converted using the encoding of the test database
            self.skipTest("database does not support English money")
        pg.set_decimal_point(None)
        try:
            r = r.getresult()[0][0]
        finally:
            pg.set_decimal_point(point)
        self.assertIsInstance(r, str)
        self.assertIn(r, en_money)
        r = query(select_money)
        pg.set_decimal_point('')
        try:
            r = r.getresult()[0][0]
        finally:
            pg.set_decimal_point(point)
        self.assertIsInstance(r, str)
        self.assertIn(r, en_money)
        r = query(select_money)
        pg.set_decimal_point('.')
        try:
            r = r.getresult()[0][0]
        finally:
            pg.set_decimal_point(point)
        self.assertIsInstance(r, d)
        self.assertEqual(r, proper_money)
        r = query(select_money)
        pg.set_decimal_point(',')
        try:
            r = r.getresult()[0][0]
        finally:
            pg.set_decimal_point(point)
        self.assertIsInstance(r, d)
        self.assertEqual(r, bad_money)
        r = query(select_money)
        pg.set_decimal_point("'")
        try:
            r = r.getresult()[0][0]
        finally:
            pg.set_decimal_point(point)
        self.assertIsInstance(r, d)
        self.assertEqual(r, bad_money)
        # then try with German localization (using the comma)
        for lc in de_locales:
            try:
                query("set lc_monetary='%s'" % lc)
            except pg.DataError:
                pass
            else:
                break
        else:
            self.skipTest("cannot set German money locale")
        select_money = select_money.replace('.', ',')
        try:
            r = query(select_money)
        except pg.DataError:
            self.skipTest("database does not support English money")
        pg.set_decimal_point(None)
        try:
            r = r.getresult()[0][0]
        finally:
            pg.set_decimal_point(point)
        self.assertIsInstance(r, str)
        self.assertIn(r, de_money)
        r = query(select_money)
        pg.set_decimal_point('')
        try:
            r = r.getresult()[0][0]
        finally:
            pg.set_decimal_point(point)
        self.assertIsInstance(r, str)
        self.assertIn(r, de_money)
        r = query(select_money)
        pg.set_decimal_point(',')
        try:
            r = r.getresult()[0][0]
        finally:
            pg.set_decimal_point(point)
        self.assertIsInstance(r, d)
        self.assertEqual(r, proper_money)
        r = query(select_money)
        pg.set_decimal_point('.')
        try:
            r = r.getresult()[0][0]
        finally:
            pg.set_decimal_point(point)
        self.assertEqual(r, bad_money)
        r = query(select_money)
        pg.set_decimal_point("'")
        try:
            r = r.getresult()[0][0]
        finally:
            pg.set_decimal_point(point)
        self.assertEqual(r, bad_money)

    def testGetDecimal(self):
        decimal_class = pg.get_decimal()
        # error if a parameter is passed
        self.assertRaises(TypeError, pg.get_decimal, decimal_class)
        self.assertIs(decimal_class, pg.Decimal)  # the default setting
        pg.set_decimal(int)
        try:
            r = pg.get_decimal()
        finally:
            pg.set_decimal(decimal_class)
        self.assertIs(r, int)
        r = pg.get_decimal()
        self.assertIs(r, decimal_class)

    def testSetDecimal(self):
        decimal_class = pg.get_decimal()
        # error if no parameter is passed
        self.assertRaises(TypeError, pg.set_decimal)
        query = self.c.query
        try:
            r = query("select 3425::numeric")
        except pg.DatabaseError:
            self.skipTest('database does not support numeric')
        r = r.getresult()[0][0]
        self.assertIsInstance(r, decimal_class)
        self.assertEqual(r, decimal_class('3425'))
        r = query("select 3425::numeric")
        pg.set_decimal(int)
        try:
            r = r.getresult()[0][0]
        finally:
            pg.set_decimal(decimal_class)
        self.assertNotIsInstance(r, decimal_class)
        self.assertIsInstance(r, int)
        self.assertEqual(r, int(3425))

    def testGetBool(self):
        use_bool = pg.get_bool()
        # error if a parameter is passed
        self.assertRaises(TypeError, pg.get_bool, use_bool)
        self.assertIsInstance(use_bool, bool)
        self.assertIs(use_bool, True)  # the default setting
        pg.set_bool(False)
        try:
            r = pg.get_bool()
        finally:
            pg.set_bool(use_bool)
        self.assertIsInstance(r, bool)
        self.assertIs(r, False)
        pg.set_bool(True)
        try:
            r = pg.get_bool()
        finally:
            pg.set_bool(use_bool)
        self.assertIsInstance(r, bool)
        self.assertIs(r, True)
        pg.set_bool(0)
        try:
            r = pg.get_bool()
        finally:
            pg.set_bool(use_bool)
        self.assertIsInstance(r, bool)
        self.assertIs(r, False)
        pg.set_bool(1)
        try:
            r = pg.get_bool()
        finally:
            pg.set_bool(use_bool)
        self.assertIsInstance(r, bool)
        self.assertIs(r, True)

    def testSetBool(self):
        use_bool = pg.get_bool()
        # error if no parameter is passed
        self.assertRaises(TypeError, pg.set_bool)
        query = self.c.query
        try:
            r = query("select true::bool")
        except pg.ProgrammingError:
            self.skipTest('database does not support bool')
        r = r.getresult()[0][0]
        self.assertIsInstance(r, bool)
        self.assertEqual(r, True)
        r = query("select true::bool")
        pg.set_bool(False)
        try:
            r = r.getresult()[0][0]
        finally:
            pg.set_bool(use_bool)
        self.assertIsInstance(r, str)
        self.assertIs(r, 't')
        r = query("select true::bool")
        pg.set_bool(True)
        try:
            r = r.getresult()[0][0]
        finally:
            pg.set_bool(use_bool)
        self.assertIsInstance(r, bool)
        self.assertIs(r, True)

    def testGetByteEscaped(self):
        bytea_escaped = pg.get_bytea_escaped()
        # error if a parameter is passed
        self.assertRaises(TypeError, pg.get_bytea_escaped, bytea_escaped)
        self.assertIsInstance(bytea_escaped, bool)
        self.assertIs(bytea_escaped, False)  # the default setting
        pg.set_bytea_escaped(True)
        try:
            r = pg.get_bytea_escaped()
        finally:
            pg.set_bytea_escaped(bytea_escaped)
        self.assertIsInstance(r, bool)
        self.assertIs(r, True)
        pg.set_bytea_escaped(False)
        try:
            r = pg.get_bytea_escaped()
        finally:
            pg.set_bytea_escaped(bytea_escaped)
        self.assertIsInstance(r, bool)
        self.assertIs(r, False)
        pg.set_bytea_escaped(1)
        try:
            r = pg.get_bytea_escaped()
        finally:
            pg.set_bytea_escaped(bytea_escaped)
        self.assertIsInstance(r, bool)
        self.assertIs(r, True)
        pg.set_bytea_escaped(0)
        try:
            r = pg.get_bytea_escaped()
        finally:
            pg.set_bytea_escaped(bytea_escaped)
        self.assertIsInstance(r, bool)
        self.assertIs(r, False)

    def testSetByteaEscaped(self):
        bytea_escaped = pg.get_bytea_escaped()
        # error if no parameter is passed
        self.assertRaises(TypeError, pg.set_bytea_escaped)
        query = self.c.query
        try:
            r = query("select 'data'::bytea")
        except pg.ProgrammingError:
            self.skipTest('database does not support bytea')
        r = r.getresult()[0][0]
        self.assertIsInstance(r, bytes)
        self.assertEqual(r, b'data')
        r = query("select 'data'::bytea")
        pg.set_bytea_escaped(True)
        try:
            r = r.getresult()[0][0]
        finally:
            pg.set_bytea_escaped(bytea_escaped)
        self.assertIsInstance(r, str)
        self.assertEqual(r, '\\x64617461')
        r = query("select 'data'::bytea")
        pg.set_bytea_escaped(False)
        try:
            r = r.getresult()[0][0]
        finally:
            pg.set_bytea_escaped(bytea_escaped)
        self.assertIsInstance(r, bytes)
        self.assertEqual(r, b'data')

    def testGetNamedresult(self):
        namedresult = pg.get_namedresult()
        # error if a parameter is passed
        self.assertRaises(TypeError, pg.get_namedresult, namedresult)
        self.assertIs(namedresult, pg._namedresult)  # the default setting

    def testSetNamedresult(self):
        namedresult = pg.get_namedresult()
        self.assertTrue(callable(namedresult))

        query = self.c.query

        r = query("select 1 as x, 2 as y").namedresult()[0]
        self.assertIsInstance(r, tuple)
        self.assertEqual(r, (1, 2))
        self.assertIsNot(type(r), tuple)
        self.assertEqual(r._fields, ('x', 'y'))
        self.assertEqual(r._asdict(), {'x': 1, 'y': 2})
        self.assertEqual(r.__class__.__name__, 'Row')

        def listresult(q):
            return [list(row) for row in q.getresult()]

        pg.set_namedresult(listresult)
        try:
            r = pg.get_namedresult()
            self.assertIs(r, listresult)
            r = query("select 1 as x, 2 as y").namedresult()[0]
            self.assertIsInstance(r, list)
            self.assertEqual(r, [1, 2])
            self.assertIsNot(type(r), tuple)
            self.assertFalse(hasattr(r, '_fields'))
            self.assertNotEqual(r.__class__.__name__, 'Row')
        finally:
            pg.set_namedresult(namedresult)

        r = pg.get_namedresult()
        self.assertIs(r, namedresult)


class TestStandaloneEscapeFunctions(unittest.TestCase):
    """Test pg escape functions.

    The libpq interface memorizes some parameters of the last opened
    connection that influence the result of these functions.  Therefore
    we need to open a connection with fixed parameters prior to testing
    in order to ensure that the tests always run under the same conditions.
    That's why these tests are included in this test module.
    """

    cls_set_up = False

    @classmethod
    def setUpClass(cls):
        query = connect().query
        query('set client_encoding=sql_ascii')
        query('set standard_conforming_strings=off')
        query('set bytea_output=escape')
        cls.cls_set_up = True

    def testEscapeString(self):
        self.assertTrue(self.cls_set_up)
        f = pg.escape_string
        r = f(b'plain')
        self.assertIsInstance(r, bytes)
        self.assertEqual(r, b'plain')
        r = f(u'plain')
        self.assertIsInstance(r, unicode)
        self.assertEqual(r, u'plain')
        r = f(u"das is' käse".encode('utf-8'))
        self.assertIsInstance(r, bytes)
        self.assertEqual(r, u"das is'' käse".encode('utf-8'))
        r = f(u"that's cheesy")
        self.assertIsInstance(r, unicode)
        self.assertEqual(r, u"that''s cheesy")
        r = f(r"It's bad to have a \ inside.")
        self.assertEqual(r, r"It''s bad to have a \\ inside.")

    def testEscapeBytea(self):
        self.assertTrue(self.cls_set_up)
        f = pg.escape_bytea
        r = f(b'plain')
        self.assertIsInstance(r, bytes)
        self.assertEqual(r, b'plain')
        r = f(u'plain')
        self.assertIsInstance(r, unicode)
        self.assertEqual(r, u'plain')
        r = f(u"das is' käse".encode('utf-8'))
        self.assertIsInstance(r, bytes)
        self.assertEqual(r, b"das is'' k\\\\303\\\\244se")
        r = f(u"that's cheesy")
        self.assertIsInstance(r, unicode)
        self.assertEqual(r, u"that''s cheesy")
        r = f(b'O\x00ps\xff!')
        self.assertEqual(r, b'O\\\\000ps\\\\377!')


if __name__ == '__main__':
    unittest.main()
