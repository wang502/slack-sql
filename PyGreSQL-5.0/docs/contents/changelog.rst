ChangeLog
=========

Version 5.0 (2016-03-20)
------------------------
- This version now runs on both Python 2 and Python 3.
- The supported versions are Python 2.6 to 2.7, and 3.3 to 3.5.
- PostgreSQL is supported in all versions from 9.0 to 9.5.
- Changes in the classic PyGreSQL module (pg):
    - The classic interface got two new methods get_as_list() and get_as_dict()
      returning a database table as a Python list or dict. The amount of data
      returned can be controlled with various parameters.
    - A method upsert() has been added to the DB wrapper class that utilitses
      the "upsert" feature that is new in PostgreSQL 9.5. The new method nicely
      complements the existing get/insert/update/delete() methods.
    - When using insert/update/upsert(), you can now pass PostgreSQL arrays as
      lists and PostgreSQL records as tuples in the classic module.
    - Conversely, when the query method returns a PostgreSQL array, it is passed
      to Python as a list. PostgreSQL records are converted to named tuples as
      well, but only if you use one of the get/insert/update/delete() methods.
      PyGreSQL uses a new fast built-in parser to achieve this.  The automatic
      conversion of arrays to lists can be disabled with set_array(False).
    - The pkey() method of the classic interface now returns tuples instead
      of frozenset. The order of the tuples is like in the primary key index.
    - Like the DB-API 2 module, the classic module now also returns bool values
      from the database as Python bool objects instead of strings.  You can
      still restore the old behavior by calling set_bool(False).
    - Like the DB-API 2 module, the classic module now also returns bytea
      data fetched from the database as byte strings, so you don't need to
      call unescape_bytea() any more.  This has been made configurable though,
      and you can restore the old behavior by calling set_bytea_escaped(True).
    - A method set_jsondecode() has been added for changing or removing the
      function that automatically decodes JSON data coming from the database.
      By default, decoding JSON is now enabled and uses the decoder function
      in the standard library with its default parameters.
    - The table name that is affixed to the name of the OID column returned
      by the get() method of the classic interface will not automatically
      be fully qualified any more. This reduces overhead from the interface,
      but it means you must always write the table name in the same way when
      you call the methods using it and you are using tables with OIDs.
      Also, OIDs are now only used when access via primary key is not possible.
      Note that OIDs are considered deprecated anyway, and they are not created
      by default any more in PostgreSQL 8.1 and later.
    - The internal caching and automatic quoting of class names in the classic
      interface has been simplified and improved, it should now perform better
      and use less memory. Also, overhead for quoting values in the DB wrapper
      methods has been reduced and security has been improved by passing the
      values to libpq separately as parameters instead of inline.
    - It is now possible to use regular type names instead of the simpler
      type names that are used by default in PyGreSQL, without breaking any
      of the mechanisms for quoting and typecasting, which rely on the type
      information. This is achieved while maintaining simplicity and backward
      compatibility by augmenting the type name string objects with all the
      necessary information under the cover. To switch regular type names on
      or off (this is the default), call the DB wrapper method use_regtypes().
    - A new method query_formatted() has been added to the DB wrapper class
      that allows using the format specifications from Python.  A flag "inline"
      can be set to specify whether parameters should be sent to the database
      separately or formatted into the SQL.
    - A new type helper Bytea() has been added.
- Changes in the DB-API 2 module (pgdb):
    - The DB-API 2 module now always returns result rows as named tuples
      instead of simply lists as before. The documentation explains how
      you can restore the old behavior or use custom row objects instead.
    - The names of the various classes used by the classic and DB-API 2
      modules have been renamed to become simpler, more intuitive and in
      line with the names used in the DB-API 2 documentation.
      Since the API provides only objects of these types through constructor
      functions, this should not cause any incompatibilities.
    - The DB-API 2 module now supports the callproc() cursor method. Note
      that output parameters are currently not replaced in the return value.
    - The DB-API 2 module now supports copy operations between data streams
      on the client and database tables via the COPY command of PostgreSQL.
      The cursor method copy_from() can be used to copy data from the database
      to the client, and the cursor method copy_to() can be used to copy data
      from the client to the database.
    - The 7-tuples returned by the description attribute of a pgdb cursor
      are now named tuples, i.e. their elements can be also accessed by name.
      The column names and types can now also be requested through the
      colnames and coltypes attributes, which are not part of DB-API 2 though.
      The type_code provided by the description attribute is still equal to
      the PostgreSQL internal type name, but now carries some more information
      in additional attributes. The size, precision and scale information that
      is part of the description is now properly set for numeric types.
    - If you pass a Python list as one of the parameters to a DB-API 2 cursor,
      it is now automatically bound using an ARRAY constructor. If you pass a
      Python tuple, it is bound using a ROW constructor. This is useful for
      passing records as well as making use of the IN syntax.
    - Inversely, when a fetch method of a DB-API 2 cursor returns a PostgreSQL
      array, it is passed to Python as a list, and when it returns a PostgreSQL
      composite type, it is passed to Python as a named tuple. PyGreSQL uses
      a new fast built-in parser to achieve this. Anonymous composite types are
      also supported, but yield only an ordinary tuple containing text strings.
    - New type helpers Interval() and Uuid() have been added.
    - The connection has a new attribute "closed" that can be used to check
      whether the connection is closed or broken.
    - SQL commands are always handled as if they include parameters, i.e.
      literal percent signs must always be doubled. This consistent behavior
      is necessary for using pgdb with wrappers like SQLAlchemy.
    - PyGreSQL 5.0 will be supported as a database driver by SQLAlchemy 1.1.
- Changes concerning both modules:
    - PyGreSQL now tries to raise more specific and appropriate subclasses of
      DatabaseError than just ProgrammingError. Particularly, when database
      constraints are violated, it raises an IntegrityError now.
    - The modules now provide get_typecast() and set_typecast() methods
      allowing to control the typecasting on the global level.  The connection
      objects have got type caches with the same methods which give control
      over the typecasting on the level of the current connection.
      See the documentation on details about the type cache and the typecast
      mechanisms provided by PyGreSQL.
    - Dates, times, timestamps and time intervals are now returned as the
      corresponding Python objects from the datetime module of the standard
      library.  In earlier versions of PyGreSQL they had been returned as
      strings.  You can restore the old behavior by deactivating the respective
      typecast functions, e.g. set_typecast('date', None).
    - PyGreSQL now support the "uuid" data type, converting such columns
      automatically to and from Python uuid.UUID objects.
    - PyGreSQL now supports the "hstore" data type, converting such columns
      automatically to and from Python dictionaries.  If you want to insert
      Python objects as JSON data using DB-API 2, you should wrap them in the
      new HStore() type constructor as a hint to PyGreSQL.
    - PyGreSQL now supports the "json" and "jsonb" data types, converting such
      columns automatically to and from Python objects. If you want to insert
      Python objects as JSON data using DB-API 2, you should wrap them in the
      new Json() type constructor as a hint to PyGreSQL.
    - A new type helper Literal() for inserting parameters literally as SQL
      has been added.  This is useful for table names, for instance.
    - Fast parsers cast_array(), cast_record() and cast_hstore for the input
      and output syntax for PostgreSQL arrays, composite types and the hstore
      type have been added to the C extension module. The array parser also
      allows using multi-dimensional arrays with PyGreSQL.
    - The tty parameter and attribute of database connections has been
      removed since it is not supported any more since PostgreSQL 7.4.

Version 4.2.2 (2016-03-18)
--------------------------
- The get_relations() and get_tables() methods now also return system views
  and tables if you set the optional "system" parameter to True.
- Fixed a regression when using temporary tables with DB wrapper methods
  (thanks to Patrick TJ McPhee for reporting).

Version 4.2.1 (2016-02-18)
--------------------------
- Fixed a small bug when setting the notice receiver.
- Some more minor fixes and re-packaging with proper permissions.

Version 4.2 (2016-01-21)
------------------------
- The supported Python versions are 2.4 to 2.7.
- PostgreSQL is supported in all versions from 8.3 to 9.5.
- Set a better default for the user option "escaping-funcs".
- Force build to compile with no errors.
- New methods get_parameters() and set_parameters() in the classic interface
  which can be used to get or set run-time parameters.
- New method truncate() in the classic interface that can be used to quickly
  empty a table or a set of tables.
- Fix decimal point handling.
- Add option to return boolean values as bool objects.
- Add option to return money values as string.
- get_tables() does not list information schema tables any more.
- Fix notification handler (Thanks Patrick TJ McPhee).
- Fix a small issue with large objects.
- Minor improvements of the NotificationHandler.
- Converted documentation to Sphinx and added many missing parts.
- The tutorial files have become a chapter in the documentation.
- Greatly improved unit testing, tests run with Python 2.4 to 2.7 again.

Version 4.1.1 (2013-01-08)
--------------------------
- Add NotificationHandler class and method.  Replaces need for pgnotify.
- Sharpen test for inserting current_timestamp.
- Add more quote tests.  False and 0 should evaluate to NULL.
- More tests - Any number other than 0 is True.
- Do not use positional parameters internally.
  This restores backward compatibility with version 4.0.
- Add methods for changing the decimal point.

Version 4.1 (2013-01-01)
------------------------
- Dropped support for Python below 2.5 and PostgreSQL below 8.3.
- Added support for Python up to 2.7 and PostgreSQL up to 9.2.
- Particularly, support PQescapeLiteral() and PQescapeIdentifier().
- The query method of the classic API now supports positional parameters.
  This an effective way to pass arbitrary or unknown data without worrying
  about SQL injection or syntax errors (contribution by Patrick TJ McPhee).
- The classic API now supports a method namedresult() in addition to
  getresult() and dictresult(), which returns the rows of the result
  as named tuples if these are supported (Python 2.6 or higher).
- The classic API has got the new methods begin(), commit(), rollback(),
  savepoint() and release() for handling transactions.
- Both classic and DBAPI 2 connections can now be used as context
  managers for encapsulating transactions.
- The execute() and executemany() methods now return the cursor object,
  so you can now write statements like "for row in cursor.execute(...)"
  (as suggested by Adam Frederick).
- Binary objects are now automatically escaped and unescaped.
- Bug in money quoting fixed.  Amounts of $0.00 handled correctly.
- Proper handling of date and time objects as input.
- Proper handling of floats with 'nan' or 'inf' values as input.
- Fixed the set_decimal() function.
- All DatabaseError instances now have a sqlstate attribute.
- The getnotify() method can now also return payload strings (#15).
- Better support for notice processing with the new methods
  set_notice_receiver() and get_notice_receiver()
  (as suggested by Michael Filonenko, see #37).
- Open transactions are rolled back when pgdb connections are closed
  (as suggested by Peter Harris, see #46).
- Connections and cursors can now be used with the "with" statement
  (as suggested by Peter Harris, see #46).
- New method use_regtypes() that can be called to let getattnames()
  return regular type names instead of the simplified classic types (#44).

Version 4.0 (2009-01-01)
------------------------
- Dropped support for Python below 2.3 and PostgreSQL below 7.4.
- Improved performance of fetchall() for large result sets
  by speeding up the type casts (as suggested by Peter Schuller).
- Exposed exceptions as attributes of the connection object.
- Exposed connection as attribute of the cursor object.
- Cursors now support the iteration protocol.
- Added new method to get parameter settings.
- Added customizable row_factory as suggested by Simon Pamies.
- Separated between mandatory and additional type objects.
- Added keyword args to insert, update and delete methods.
- Added exception handling for direct copy.
- Start transactions only when necessary, not after every commit().
- Release the GIL while making a connection
  (as suggested by Peter Schuller).
- If available, use decimal.Decimal for numeric types.
- Allow DB wrapper to be used with DB-API 2 connections
  (as suggested by Chris Hilton).
- Made private attributes of DB wrapper accessible.
- Dropped dependence on mx.DateTime module.
- Support for PQescapeStringConn() and PQescapeByteaConn();
  these are now also used by the internal _quote() functions.
- Added 'int8' to INTEGER types. New SMALLINT type.
- Added a way to find the number of rows affected by a query()
  with the classic pg module by returning it as a string.
  For single inserts, query() still returns the oid as an integer.
  The pgdb module already provides the "rowcount" cursor attribute
  for the same purpose.
- Improved getnotify() by calling PQconsumeInput() instead of
  submitting an empty command.
- Removed compatibility code for old OID munging style.
- The insert() and update() methods now use the "returning" clause
  if possible to get all changed values, and they also check in advance
  whether a subsequent select is possible, so that ongoing transactions
  won't break if there is no select privilege.
- Added "protocol_version" and "server_version" attributes.
- Revived the "user" attribute.
- The pg module now works correctly with composite primary keys;
  these are represented as frozensets.
- Removed the undocumented and actually unnecessary "view" parameter
  from the get() method.
- get() raises a nicer ProgrammingError instead of a KeyError
  if no primary key was found.
- delete() now also works based on the primary key if no oid available
  and returns whether the row existed or not.

Version 3.8.1 (2006-06-05)
--------------------------
- Use string methods instead of deprecated string functions.
- Only use SQL-standard way of escaping quotes.
- Added the functions escape_string() and escape/unescape_bytea()
  (as suggested by Charlie Dyson and Kavous Bojnourdi a long time ago).
- Reverted code in clear() method that set date to current.
- Added code for backwards compatibility in OID munging code.
- Reorder attnames tests so that "interval" is checked for before "int."
- If caller supplies key dictionary, make sure that all has a namespace.

Version 3.8 (2006-02-17)
------------------------
- Installed new favicon.ico from Matthew Sporleder <mspo@mspo.com>
- Replaced snprintf by PyOS_snprintf.
- Removed NO_SNPRINTF switch which is not needed any longer
- Clean up some variable names and namespace
- Add get_relations() method to get any type of relation
- Rewrite get_tables() to use get_relations()
- Use new method in get_attnames method to get attributes of views as well
- Add Binary type
- Number of rows is now -1 after executing no-result statements
- Fix some number handling
- Non-simple types do not raise an error any more
- Improvements to documentation framework
- Take into account that nowadays not every table must have an oid column
- Simplification and improvement of the inserttable() function
- Fix up unit tests
- The usual assortment of minor fixes and enhancements

Version 3.7 (2005-09-07)
------------------------
Improvement of pgdb module:

- Use Python standard `datetime` if `mxDateTime` is not available

Major improvements and clean-up in classic pg module:

- All members of the underlying connection directly available in `DB`
- Fixes to quoting function
- Add checks for valid database connection to methods
- Improved namespace support, handle `search_path` correctly
- Removed old dust and unnessesary imports, added docstrings
- Internal sql statements as one-liners, smoothed out ugly code

Version 3.6.2 (2005-02-23)
--------------------------
- Further fixes to namespace handling

Version 3.6.1 (2005-01-11)
--------------------------
- Fixes to namespace handling

Version 3.6 (2004-12-17)
------------------------
- Better DB-API 2.0 compliance
- Exception hierarchy moved into C module and made available to both APIs
- Fix error in update method that caused false exceptions
- Moved to standard exception hierarchy in classic API
- Added new method to get transaction state
- Use proper Python constants where appropriate
- Use Python versions of strtol, etc. Allows Win32 build.
- Bug fixes and cleanups

Version 3.5 (2004-08-29)
------------------------
Fixes and enhancements:

- Add interval to list of data types
- fix up method wrapping especially close()
- retry pkeys once if table missing in case it was just added
- wrap query method separately to handle debug better
- use isinstance instead of type
- fix free/PQfreemem issue - finally
- miscellaneous cleanups and formatting

Version 3.4 (2004-06-02)
------------------------
Some cleanups and fixes.
This is the first version where PyGreSQL is moved back out of the
PostgreSQL tree. A lot of the changes mentioned below were actually
made while in the PostgreSQL tree since their last release.

- Allow for larger integer returns
- Return proper strings for true and false
- Cleanup convenience method creation
- Enhance debugging method
- Add reopen method
- Allow programs to preload field names for speedup
- Move OID handling so that it returns long instead of int
- Miscellaneous cleanups and formatting

Version 3.3 (2001-12-03)
------------------------
A few cleanups.  Mostly there was some confusion about the latest version
and so I am bumping the number to keep it straight.

- Added NUMERICOID to list of returned types. This fixes a bug when
  returning aggregates in the latest version of PostgreSQL.

Version 3.2 (2001-06-20)
------------------------
Note that there are very few changes to PyGreSQL between 3.1 and 3.2.
The main reason for the release is the move into the PostgreSQL
development tree.  Even the WIN32 changes are pretty minor.

- Add Win32 support (gerhard@bigfoot.de)
- Fix some DB-API quoting problems (niall.smart@ebeon.com)
- Moved development into PostgreSQL development tree.

Version 3.1 (2000-11-06)
------------------------
- Fix some quoting functions.  In particular handle NULLs better.
- Use a method to add primary key information rather than direct
  manipulation of the class structures
- Break decimal out in `_quote` (in pg.py) and treat it as float
- Treat timestamp like date for quoting purposes
- Remove a redundant SELECT from the `get` method speeding it,
  and `insert` (since it calls `get`) up a little.
- Add test for BOOL type in typecast method to `pgdbTypeCache` class
  (tv@beamnet.de)
- Fix pgdb.py to send port as integer to lower level function
  (dildog@l0pht.com)
- Change pg.py to speed up some operations
- Allow updates on tables with no primary keys

Version 3.0 (2000-05-30)
------------------------
- Remove strlen() call from pglarge_write() and get size from object
  (Richard@Bouska.cz)
- Add a little more error checking to the quote function in the wrapper
- Add extra checking in `_quote` function
- Wrap query in pg.py for debugging
- Add DB-API 2.0 support to pgmodule.c (andre@via.ecp.fr)
- Add DB-API 2.0 wrapper pgdb.py (andre@via.ecp.fr)
- Correct keyword clash (temp) in tutorial
- Clean up layout of tutorial
- Return NULL values as None (rlawrence@lastfoot.com)
  (WARNING: This will cause backwards compatibility issues)
- Change None to NULL in insert and update
- Change hash-bang lines to use /usr/bin/env
- Clearing date should be blank (NULL) not TODAY
- Quote backslashes in strings in `_quote` (brian@CSUA.Berkeley.EDU)
- Expanded and clarified build instructions (tbryan@starship.python.net)
- Make code thread safe (Jerome.Alet@unice.fr)
- Add README.distutils (mwa@gate.net & jeremy@cnri.reston.va.us)
- Many fixes and increased DB-API compliance by chifungfan@yahoo.com,
  tony@printra.net, jeremy@alum.mit.edu and others to get the final
  version ready to release.

Version 2.4 (1999-06-15)
------------------------
- Insert returns None if the user doesn't have select permissions
  on the table.  It can (and does) happen that one has insert but
  not select permissions on a table.
- Added ntuples() method to query object (brit@druid.net)
- Corrected a bug related to getresult() and the money type
- Corrected a bug related to negative money amounts
- Allow update based on primary key if munged oid not available and
  table has a primary key
- Add many __doc__ strings (andre@via.ecp.fr)
- Get method works with views if key specified

Version 2.3 (1999-04-17)
------------------------
- connect.host returns "localhost" when connected to Unix socket
  (torppa@tuhnu.cutery.fi)
- Use `PyArg_ParseTupleAndKeywords` in connect() (torppa@tuhnu.cutery.fi)
- fixes and cleanups (torppa@tuhnu.cutery.fi)
- Fixed memory leak in dictresult() (terekhov@emc.com)
- Deprecated pgext.py - functionality now in pg.py
- More cleanups to the tutorial
- Added fileno() method - terekhov@emc.com (Mikhail Terekhov)
- added money type to quoting function
- Compiles cleanly with more warnings turned on
- Returns PostgreSQL error message on error
- Init accepts keywords (Jarkko Torppa)
- Convenience functions can be overridden (Jarkko Torppa)
- added close() method

Version 2.2 (1998-12-21)
------------------------
- Added user and password support thanks to Ng Pheng Siong (ngps@post1.com)
- Insert queries return the inserted oid
- Add new `pg` wrapper (C module renamed to _pg)
- Wrapped database connection in a class
- Cleaned up some of the tutorial.  (More work needed.)
- Added `version` and `__version__`.
  Thanks to thilo@eevolute.com for the suggestion.

Version 2.1 (1998-03-07)
------------------------
- return fields as proper Python objects for field type
- Cleaned up pgext.py
- Added dictresult method

Version 2.0  (1997-12-23)
-------------------------
- Updated code for PostgreSQL 6.2.1 and Python 1.5
- Reformatted code and converted to use full ANSI style prototypes
- Changed name to PyGreSQL (from PyGres95)
- Changed order of arguments to connect function
- Created new type `pgqueryobject` and moved certain methods to it
- Added a print function for pgqueryobject
- Various code changes - mostly stylistic

Version 1.0b (1995-11-04)
-------------------------
- Keyword support for connect function moved from library file to C code
  and taken away from library
- Rewrote documentation
- Bug fix in connect function
- Enhancements in large objects interface methods

Version 1.0a (1995-10-30)
-------------------------
A limited release.

- Module adapted to standard Python syntax
- Keyword support for connect function in library file
- Rewrote default parameters interface (internal use of strings)
- Fixed minor bugs in module interface
- Redefinition of error messages

Version 0.9b (1995-10-10)
-------------------------
The first public release.

- Large objects implementation
- Many bug fixes, enhancements, ...

Version 0.1a (1995-10-07)
-------------------------
- Basic libpq functions (SQL access)
