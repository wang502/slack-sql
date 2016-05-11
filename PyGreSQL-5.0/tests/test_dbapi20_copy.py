#! /usr/bin/python
# -*- coding: utf-8 -*-

"""Test the modern PyGreSQL interface.

Sub-tests for the copy methods.

Contributed by Christoph Zwerschke.

These tests need a database to test against.
"""

try:
    import unittest2 as unittest  # for Python < 2.7
except ImportError:
    import unittest

from collections import Iterable

import pgdb  # the module under test

# We need a database to test against.  If LOCAL_PyGreSQL.py exists we will
# get our information from that.  Otherwise we use the defaults.
# The current user must have create schema privilege on the database.
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
    unicode
except NameError:  # Python >= 3.0
    unicode = str


class InputStream:

    def __init__(self, data):
        if isinstance(data, unicode):
            data = data.encode('utf-8')
        self.data = data or b''
        self.sizes = []

    def __str__(self):
        data = self.data
        if str is unicode:  # Python >= 3.0
            data = data.decode('utf-8')
        return data

    def __len__(self):
        return len(self.data)

    def read(self, size=None):
        if size is None:
            output, data = self.data, b''
        else:
            output, data = self.data[:size], self.data[size:]
        self.data = data
        self.sizes.append(size)
        return output


class OutputStream:

    def __init__(self):
        self.data = b''
        self.sizes = []

    def __str__(self):
        data = self.data
        if str is unicode:  # Python >= 3.0
            data = data.decode('utf-8')
        return data

    def __len__(self):
        return len(self.data)

    def write(self, data):
        if isinstance(data, unicode):
            data = data.encode('utf-8')
        self.data += data
        self.sizes.append(len(data))


class TestStreams(unittest.TestCase):

    def test_input(self):
        stream = InputStream('Hello, Wörld!')
        self.assertIsInstance(stream.data, bytes)
        self.assertEqual(stream.data, b'Hello, W\xc3\xb6rld!')
        self.assertIsInstance(str(stream), str)
        self.assertEqual(str(stream), 'Hello, Wörld!')
        self.assertEqual(len(stream), 14)
        self.assertEqual(stream.read(3), b'Hel')
        self.assertEqual(stream.read(2), b'lo')
        self.assertEqual(stream.read(1), b',')
        self.assertEqual(stream.read(1), b' ')
        self.assertEqual(stream.read(), b'W\xc3\xb6rld!')
        self.assertEqual(stream.read(), b'')
        self.assertEqual(len(stream), 0)
        self.assertEqual(stream.sizes, [3, 2, 1, 1, None, None])

    def test_output(self):
        stream = OutputStream()
        self.assertEqual(len(stream), 0)
        for chunk in 'Hel', 'lo', ',', ' ', 'Wörld!':
            stream.write(chunk)
        self.assertIsInstance(stream.data, bytes)
        self.assertEqual(stream.data, b'Hello, W\xc3\xb6rld!')
        self.assertIsInstance(str(stream), str)
        self.assertEqual(str(stream), 'Hello, Wörld!')
        self.assertEqual(len(stream), 14)
        self.assertEqual(stream.sizes, [3, 2, 1, 1, 7])


class TestCopy(unittest.TestCase):

    cls_set_up = False

    @staticmethod
    def connect():
        return pgdb.connect(database=dbname,
            host='%s:%d' % (dbhost or '', dbport or -1))

    @classmethod
    def setUpClass(cls):
        con = cls.connect()
        cur = con.cursor()
        cur.execute("set client_min_messages=warning")
        cur.execute("drop table if exists copytest cascade")
        cur.execute("create table copytest ("
            "id smallint primary key, name varchar(64))")
        cur.close()
        con.commit()
        cur = con.cursor()
        try:
            cur.execute("set client_encoding=utf8")
            cur.execute("select 'Plácido and José'").fetchone()
        except (pgdb.DataError, pgdb.NotSupportedError):
            cls.data[1] = (1941, 'Plaacido Domingo')
            cls.data[2] = (1946, 'Josee Carreras')
            cls.can_encode = False
        cur.close()
        con.close()
        cls.cls_set_up = True

    @classmethod
    def tearDownClass(cls):
        con = cls.connect()
        cur = con.cursor()
        cur.execute("set client_min_messages=warning")
        cur.execute("drop table if exists copytest cascade")
        con.commit()
        con.close()

    def setUp(self):
        self.assertTrue(self.cls_set_up)
        self.con = self.connect()
        self.cursor = self.con.cursor()
        self.cursor.execute("set client_encoding=utf8")

    def tearDown(self):
        try:
            self.cursor.close()
        except Exception:
            pass
        try:
            self.con.rollback()
        except Exception:
            pass
        try:
            self.con.close()
        except Exception:
            pass

    data = [(1935, 'Luciano Pavarotti'),
            (1941, 'Plácido Domingo'),
            (1946, 'José Carreras')]

    can_encode = True

    @property
    def data_text(self):
        return ''.join('%d\t%s\n' % row for row in self.data)

    @property
    def data_csv(self):
        return ''.join('%d,%s\n' % row for row in self.data)

    def truncate_table(self):
        self.cursor.execute("truncate table copytest")

    @property
    def table_data(self):
        self.cursor.execute("select * from copytest")
        return self.cursor.fetchall()

    def check_table(self):
        self.assertEqual(self.table_data, self.data)

    def check_rowcount(self, number=len(data)):
        self.assertEqual(self.cursor.rowcount, number)


class TestCopyFrom(TestCopy):
    """Test the copy_from method."""

    def tearDown(self):
        super(TestCopyFrom, self).tearDown()
        self.setUp()
        self.truncate_table()
        super(TestCopyFrom, self).tearDown()

    def copy_from(self, stream, **options):
        return self.cursor.copy_from(stream, 'copytest', **options)

    @property
    def data_file(self):
        return InputStream(self.data_text)

    def test_bad_params(self):
        call = self.cursor.copy_from
        call('0\t', 'copytest'), self.cursor
        call('1\t', 'copytest',
             format='text', sep='\t', null='', columns=['id', 'name'])
        self.assertRaises(TypeError, call)
        self.assertRaises(TypeError, call, None)
        self.assertRaises(TypeError, call, None, None)
        self.assertRaises(TypeError, call, '0\t')
        self.assertRaises(TypeError, call, '0\t', None)
        self.assertRaises(TypeError, call, '0\t', 42)
        self.assertRaises(TypeError, call, '0\t', ['copytest'])
        self.assertRaises(TypeError, call, '0\t', 'copytest', format=42)
        self.assertRaises(ValueError, call, '0\t', 'copytest', format='bad')
        self.assertRaises(TypeError, call, '0\t', 'copytest', sep=42)
        self.assertRaises(ValueError, call, '0\t', 'copytest', sep='bad')
        self.assertRaises(TypeError, call, '0\t', 'copytest', null=42)
        self.assertRaises(ValueError, call, '0\t', 'copytest', size='bad')
        self.assertRaises(TypeError, call, '0\t', 'copytest', columns=42)
        self.assertRaises(ValueError, call, b'', 'copytest',
            format='binary', sep=',')

    def test_input_string(self):
        ret = self.copy_from('42\tHello, world!')
        self.assertIs(ret, self.cursor)
        self.assertEqual(self.table_data, [(42, 'Hello, world!')])
        self.check_rowcount(1)

    def test_input_string_with_newline(self):
        self.copy_from('42\tHello, world!\n')
        self.assertEqual(self.table_data, [(42, 'Hello, world!')])
        self.check_rowcount(1)

    def test_input_string_multiple_rows(self):
        ret = self.copy_from(self.data_text)
        self.assertIs(ret, self.cursor)
        self.check_table()
        self.check_rowcount()

    if str is unicode:  # Python >= 3.0

        def test_input_bytes(self):
            self.copy_from(b'42\tHello, world!')
            self.assertEqual(self.table_data, [(42, 'Hello, world!')])
            self.truncate_table()
            self.copy_from(self.data_text.encode('utf-8'))
            self.check_table()

    else:  # Python < 3.0

        def test_input_unicode(self):
            if not self.can_encode:
                self.skipTest('database does not support utf8')
            self.copy_from(u'43\tWürstel, Käse!')
            self.assertEqual(self.table_data, [(43, 'Würstel, Käse!')])
            self.truncate_table()
            self.copy_from(self.data_text.decode('utf-8'))
            self.check_table()

    def test_input_iterable(self):
        self.copy_from(self.data_text.splitlines())
        self.check_table()
        self.check_rowcount()

    def test_input_iterable_invalid(self):
        self.assertRaises(IOError, self.copy_from, [None])

    def test_input_iterable_with_newlines(self):
        self.copy_from('%s\n' % row for row in self.data_text.splitlines())
        self.check_table()

    if str is unicode:  # Python >= 3.0

        def test_input_iterable_bytes(self):
            self.copy_from(row.encode('utf-8')
                for row in self.data_text.splitlines())
            self.check_table()

    def test_sep(self):
        stream = ('%d-%s' % row for row in self.data)
        self.copy_from(stream, sep='-')
        self.check_table()

    def test_null(self):
        self.copy_from('0\t\\N')
        self.assertEqual(self.table_data, [(0, None)])
        self.assertIsNone(self.table_data[0][1])
        self.truncate_table()
        self.copy_from('1\tNix')
        self.assertEqual(self.table_data, [(1, 'Nix')])
        self.assertIsNotNone(self.table_data[0][1])
        self.truncate_table()
        self.copy_from('2\tNix', null='Nix')
        self.assertEqual(self.table_data, [(2, None)])
        self.assertIsNone(self.table_data[0][1])
        self.truncate_table()
        self.copy_from('3\t')
        self.assertEqual(self.table_data, [(3, '')])
        self.assertIsNotNone(self.table_data[0][1])
        self.truncate_table()
        self.copy_from('4\t', null='')
        self.assertEqual(self.table_data, [(4, None)])
        self.assertIsNone(self.table_data[0][1])

    def test_columns(self):
        self.copy_from('1', columns='id')
        self.copy_from('2', columns=['id'])
        self.copy_from('3\tThree')
        self.copy_from('4\tFour', columns='id, name')
        self.copy_from('5\tFive', columns=['id', 'name'])
        self.assertEqual(self.table_data, [
            (1, None), (2, None), (3, 'Three'), (4, 'Four'), (5, 'Five')])
        self.check_rowcount(5)
        self.assertRaises(pgdb.ProgrammingError, self.copy_from,
            '6\t42', columns=['id', 'age'])
        self.check_rowcount(-1)

    def test_csv(self):
        self.copy_from(self.data_csv, format='csv')
        self.check_table()

    def test_csv_with_sep(self):
        stream = ('%d;"%s"\n' % row for row in self.data)
        self.copy_from(stream, format='csv', sep=';')
        self.check_table()
        self.check_rowcount()

    def test_binary(self):
        self.assertRaises(IOError, self.copy_from,
            b'NOPGCOPY\n', format='binary')
        self.check_rowcount(-1)

    def test_binary_with_sep(self):
        self.assertRaises(ValueError, self.copy_from,
            '', format='binary', sep='\t')

    def test_binary_with_unicode(self):
        self.assertRaises(ValueError, self.copy_from, u'', format='binary')

    def test_query(self):
        self.assertRaises(ValueError, self.cursor.copy_from, '', "select null")

    def test_file(self):
        stream = self.data_file
        ret = self.copy_from(stream)
        self.assertIs(ret, self.cursor)
        self.check_table()
        self.assertEqual(len(stream), 0)
        self.assertEqual(stream.sizes, [8192])
        self.check_rowcount()

    def test_size_positive(self):
        stream = self.data_file
        size = 7
        num_chunks = (len(stream) + size - 1) // size
        self.copy_from(stream, size=size)
        self.check_table()
        self.assertEqual(len(stream), 0)
        self.assertEqual(stream.sizes, [size] * num_chunks)
        self.check_rowcount()

    def test_size_negative(self):
        stream = self.data_file
        self.copy_from(stream, size=-1)
        self.check_table()
        self.assertEqual(len(stream), 0)
        self.assertEqual(stream.sizes, [None])
        self.check_rowcount()

    def test_size_invalid(self):
        self.assertRaises(TypeError,
            self.copy_from, self.data_file, size='invalid')


class TestCopyTo(TestCopy):
    """Test the copy_to method."""

    @classmethod
    def setUpClass(cls):
        super(TestCopyTo, cls).setUpClass()
        con = cls.connect()
        cur = con.cursor()
        cur.execute("set client_encoding=utf8")
        cur.execute("insert into copytest values (%d, %s)", cls.data)
        cur.close()
        con.commit()
        con.close()

    def copy_to(self, stream=None, **options):
        return self.cursor.copy_to(stream, 'copytest', **options)

    @property
    def data_file(self):
        return OutputStream()

    def test_bad_params(self):
        call = self.cursor.copy_to
        call(None, 'copytest')
        call(None, 'copytest',
             format='text', sep='\t', null='', columns=['id', 'name'])
        self.assertRaises(TypeError, call)
        self.assertRaises(TypeError, call, None)
        self.assertRaises(TypeError, call, None, 42)
        self.assertRaises(TypeError, call, None, ['copytest'])
        self.assertRaises(TypeError, call, 'bad', 'copytest')
        self.assertRaises(TypeError, call, None, 'copytest', format=42)
        self.assertRaises(ValueError, call, None, 'copytest', format='bad')
        self.assertRaises(TypeError, call, None, 'copytest', sep=42)
        self.assertRaises(ValueError, call, None, 'copytest', sep='bad')
        self.assertRaises(TypeError, call, None, 'copytest', null=42)
        self.assertRaises(TypeError, call, None, 'copytest', decode='bad')
        self.assertRaises(TypeError, call, None, 'copytest', columns=42)

    def test_generator(self):
        ret = self.copy_to()
        self.assertIsInstance(ret, Iterable)
        rows = list(ret)
        self.assertEqual(len(rows), 3)
        rows = ''.join(rows)
        self.assertIsInstance(rows, str)
        self.assertEqual(rows, self.data_text)
        self.check_rowcount()

    if str is unicode:  # Python >= 3.0

        def test_generator_bytes(self):
            ret = self.copy_to(decode=False)
            self.assertIsInstance(ret, Iterable)
            rows = list(ret)
            self.assertEqual(len(rows), 3)
            rows = b''.join(rows)
            self.assertIsInstance(rows, bytes)
            self.assertEqual(rows, self.data_text.encode('utf-8'))

    else:  # Python < 3.0

        def test_generator_unicode(self):
            ret = self.copy_to(decode=True)
            self.assertIsInstance(ret, Iterable)
            rows = list(ret)
            self.assertEqual(len(rows), 3)
            rows = ''.join(rows)
            self.assertIsInstance(rows, unicode)
            self.assertEqual(rows, self.data_text.decode('utf-8'))

    def test_rowcount_increment(self):
        ret = self.copy_to()
        self.assertIsInstance(ret, Iterable)
        for n, row in enumerate(ret):
            self.check_rowcount(n + 1)

    def test_decode(self):
        ret_raw = b''.join(self.copy_to(decode=False))
        ret_decoded = ''.join(self.copy_to(decode=True))
        self.assertIsInstance(ret_raw, bytes)
        self.assertIsInstance(ret_decoded, unicode)
        self.assertEqual(ret_decoded, ret_raw.decode('utf-8'))
        self.check_rowcount()

    def test_sep(self):
        ret = list(self.copy_to(sep='-'))
        self.assertEqual(ret, ['%d-%s\n' % row for row in self.data])

    def test_null(self):
        data = ['%d\t%s\n' % row for row in self.data]
        self.cursor.execute('insert into copytest values(4, null)')
        try:
            ret = list(self.copy_to())
            self.assertEqual(ret, data + ['4\t\\N\n'])
            ret = list(self.copy_to(null='Nix'))
            self.assertEqual(ret, data + ['4\tNix\n'])
            ret = list(self.copy_to(null=''))
            self.assertEqual(ret, data + ['4\t\n'])
        finally:
            self.cursor.execute('delete from copytest where id=4')

    def test_columns(self):
        data_id = ''.join('%d\n' % row[0] for row in self.data)
        data_name = ''.join('%s\n' % row[1] for row in self.data)
        ret = ''.join(self.copy_to(columns='id'))
        self.assertEqual(ret, data_id)
        ret = ''.join(self.copy_to(columns=['id']))
        self.assertEqual(ret, data_id)
        ret = ''.join(self.copy_to(columns='name'))
        self.assertEqual(ret, data_name)
        ret = ''.join(self.copy_to(columns=['name']))
        self.assertEqual(ret, data_name)
        ret = ''.join(self.copy_to(columns='id, name'))
        self.assertEqual(ret, self.data_text)
        ret = ''.join(self.copy_to(columns=['id', 'name']))
        self.assertEqual(ret, self.data_text)
        self.assertRaises(pgdb.ProgrammingError, self.copy_to,
            columns=['id', 'age'])

    def test_csv(self):
        ret = self.copy_to(format='csv')
        self.assertIsInstance(ret, Iterable)
        rows = list(ret)
        self.assertEqual(len(rows), 3)
        rows = ''.join(rows)
        self.assertIsInstance(rows, str)
        self.assertEqual(rows, self.data_csv)
        self.check_rowcount(3)

    def test_csv_with_sep(self):
        rows = ''.join(self.copy_to(format='csv', sep=';'))
        self.assertEqual(rows, self.data_csv.replace(',', ';'))

    def test_binary(self):
        ret = self.copy_to(format='binary')
        self.assertIsInstance(ret, Iterable)
        for row in ret:
            self.assertTrue(row.startswith(b'PGCOPY\n\377\r\n\0'))
            break
        self.check_rowcount(1)

    def test_binary_with_sep(self):
        self.assertRaises(ValueError, self.copy_to, format='binary', sep='\t')

    def test_binary_with_unicode(self):
        self.assertRaises(ValueError, self.copy_to,
            format='binary', decode=True)

    def test_query(self):
        self.assertRaises(ValueError, self.cursor.copy_to, None,
            "select name from copytest", columns='noname')
        ret = self.cursor.copy_to(None,
            "select name||'!' from copytest where id=1941")
        self.assertIsInstance(ret, Iterable)
        rows = list(ret)
        self.assertEqual(len(rows), 1)
        self.assertIsInstance(rows[0], str)
        self.assertEqual(rows[0], '%s!\n' % self.data[1][1])
        self.check_rowcount(1)

    def test_file(self):
        stream = self.data_file
        ret = self.copy_to(stream)
        self.assertIs(ret, self.cursor)
        self.assertEqual(str(stream), self.data_text)
        data = self.data_text
        if str is unicode:  # Python >= 3.0
            data = data.encode('utf-8')
        sizes = [len(row) + 1 for row in data.splitlines()]
        self.assertEqual(stream.sizes, sizes)
        self.check_rowcount()


class TestBinary(TestCopy):
    """Test the copy_from and copy_to methods with binary data."""

    def test_round_trip(self):
        # fill table from textual data
        self.cursor.copy_from(self.data_text, 'copytest', format='text')
        self.check_table()
        self.check_rowcount()
        # get data back in binary format
        ret = self.cursor.copy_to(None, 'copytest', format='binary')
        self.assertIsInstance(ret, Iterable)
        data_binary = b''.join(ret)
        self.assertTrue(data_binary.startswith(b'PGCOPY\n\377\r\n\0'))
        self.check_rowcount()
        self.truncate_table()
        # fill table from binary data
        self.cursor.copy_from(data_binary, 'copytest', format='binary')
        self.check_table()
        self.check_rowcount()


if __name__ == '__main__':
    unittest.main()
