LargeObject -- Large Objects
============================

.. py:currentmodule:: pg

.. class:: LargeObject

Objects that are instances of the class :class:`LargeObject` are used to handle
all the requests concerning a PostgreSQL large object. These objects embed
and hide all the "recurrent" variables (object OID and connection), exactly
in the same way :class:`Connection` instances do, thus only keeping significant
parameters in function calls. The :class:`LargeObject` instance keeps a
reference to the :class:`Connection` object used for its creation, sending
requests though with its parameters. Any modification but dereferencing the
:class:`Connection` object will thus affect the :class:`LargeObject` instance.
Dereferencing the initial :class:`Connection` object is not a problem since
Python won't deallocate it before the :class:`LargeObject` instance
dereferences it. All functions return a generic error message on call error,
whatever the exact error was. The :attr:`error` attribute of the object allows
to get the exact error message.

See also the PostgreSQL programmer's guide for more information about the
large object interface.

open -- open a large object
---------------------------

.. method:: LargeObject.open(mode)

    Open a large object

    :param int mode: open mode definition
    :rtype: None
    :raises TypeError: invalid connection, bad parameter type, or too many parameters
    :raises IOError: already opened object, or open error

This method opens a large object for reading/writing, in the same way than the
Unix open() function. The mode value can be obtained by OR-ing the constants
defined in the :mod:`pg` module (:const:`INV_READ`, :const:`INV_WRITE`).

close -- close a large object
-----------------------------

.. method:: LargeObject.close()

    Close a large object

    :rtype: None
    :raises TypeError: invalid connection
    :raises TypeError: too many parameters
    :raises IOError: object is not opened, or close error

This method closes a previously opened large object, in the same way than
the Unix close() function.

read, write, tell, seek, unlink -- file-like large object handling
------------------------------------------------------------------

.. method:: LargeObject.read(size)

    Read data from large object

    :param int size: maximal size of the buffer to be read
    :returns: the read buffer
    :rtype: bytes
    :raises TypeError: invalid connection, invalid object,
     bad parameter type, or too many parameters
    :raises ValueError: if `size` is negative
    :raises IOError: object is not opened, or read error

This function allows to read data from a large object, starting at current
position.

.. method:: LargeObject.write(string)

    Read data to large object

    :param bytes string: string buffer to be written
    :rtype: None
    :raises TypeError: invalid connection, bad parameter type, or too many parameters
    :raises IOError: object is not opened, or write error

This function allows to write data to a large object, starting at current
position.

.. method:: LargeObject.seek(offset, whence)

    Change current position in large object

    :param int offset: position offset
    :param int whence: positional parameter
    :returns: new position in object
    :rtype: int
    :raises TypeError: invalid connection or invalid object,
     bad parameter type, or too many parameters
    :raises IOError: object is not opened, or seek error

This method allows to move the position cursor in the large object.
The valid values for the whence parameter are defined as constants in the
:mod:`pg` module (:const:`SEEK_SET`, :const:`SEEK_CUR`, :const:`SEEK_END`).

.. method:: LargeObject.tell()

    Return current position in large object

    :returns: current position in large object
    :rtype: int
    :raises TypeError: invalid connection or invalid object
    :raises TypeError: too many parameters
    :raises IOError: object is not opened, or seek error

This method allows to get the current position in the large object.

.. method:: LargeObject.unlink()

    Delete large object

    :rtype: None
    :raises TypeError: invalid connection or invalid object
    :raises TypeError: too many parameters
    :raises IOError: object is not closed, or unlink error

This methods unlinks (deletes) the PostgreSQL large object.

size -- get the large object size
---------------------------------

.. method:: LargeObject.size()

    Return the large object size

    :returns: the large object size
    :rtype: int
    :raises TypeError: invalid connection or invalid object
    :raises TypeError: too many parameters
    :raises IOError: object is not opened, or seek/tell error

This (composite) method allows to get the size of a large object. It was
implemented because this function is very useful for a web interfaced
database. Currently, the large object needs to be opened first.

export -- save a large object to a file
---------------------------------------

.. method:: LargeObject.export(name)

    Export a large object to a file

    :param str name: file to be created
    :rtype: None
    :raises TypeError: invalid connection or invalid object,
     bad parameter type, or too many parameters
    :raises IOError: object is not closed, or export error

This methods allows to dump the content of a large object in a very simple
way. The exported file is created on the host of the program, not the
server host.

Object attributes
-----------------
:class:`LargeObject` objects define a read-only set of attributes that allow
to get some information about it. These attributes are:

.. attribute:: LargeObject.oid

    the OID associated with the large object (int)

.. attribute:: LargeObject.pgcnx

    the :class:`Connection` object associated with the large object

.. attribute:: LargeObject.error

    the last warning/error message of the connection (str)

.. warning::

    In multi-threaded environments, :attr:`LargeObject.error` may be modified by
    another thread using the same :class:`Connection`. Remember these object
    are shared, not duplicated. You should provide some locking to be able
    if you want to check this. The :attr:`LargeObject.oid` attribute is very
    interesting, because it allows you to reuse the OID later, creating the
    :class:`LargeObject` object with a :meth:`Connection.getlo` method call.
