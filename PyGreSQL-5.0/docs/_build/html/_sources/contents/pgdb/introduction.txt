Introduction
============

You may either choose to use the "classic" PyGreSQL interface provided by
the :mod:`pg` module or else the newer DB-API 2.0 compliant interface
provided by the :mod:`pgdb` module.

The following part of the documentation covers only the newer :mod:`pgdb` API.

**DB-API 2.0** (Python Database API Specification v2.0)
is a specification for connecting to databases (not only PostGreSQL)
from Python that has been developed by the Python DB-SIG in 1999.
The authoritative programming information for the DB-API is :pep:`0249`.

.. seealso::

    A useful tutorial-like `introduction to the DB-API
    <http://www2.linuxjournal.com/lj-issues/issue49/2605.html>`_
    has been written by Andrew M. Kuchling for the LINUX Journal in 1998.
