The Notification Handler
========================

.. py:currentmodule:: pg

PyGreSQL comes with a client-side asynchronous notification handler that
was based on the ``pgnotify`` module written by Ng Pheng Siong.

.. versionadded:: 4.1.1

Instantiating the notification handler
--------------------------------------

.. class:: NotificationHandler(db, event, callback, [arg_dict], [timeout], [stop_event])

    Create an instance of the notification handler

    :param int db: the database connection
    :type db: :class:`Connection`
    :param str event: the name of an event to listen for
    :param callback: a callback function
    :param dict arg_dict: an optional dictionary for passing arguments
    :param timeout: the time-out when waiting for notifications
    :type timeout: int, float or None
    :param str stop_event: an optional different name to be used as stop event

You can also create an instance of the NotificationHandler using the
:class:`DB.connection_handler` method.  In this case you don't need to
pass a database connection because the :class:`DB` connection itself
will be used as the datebase connection for the notification handler.

You must always pass the name of an *event* (notification channel) to listen
for and a *callback* function.

You can also specify a dictionary *arg_dict* that will be passed as the
single argument to the callback function, and a *timeout* value in seconds
(a floating point number denotes fractions of seconds).  If it is absent
or *None*, the callers will never time out.  If the time-out is reached,
the callback function will be called with a single argument that is *None*.
If you set the *timeout* to ``0``, the handler will poll notifications
synchronously and return.

You can specify the name of the event that will be used to signal the handler
to stop listening as *stop_event*. By default, it will be the event name
prefixed with ``'stop_'``.

All of the parameters will be also available as attributes of the
created notification handler object.

Invoking the notification handler
---------------------------------

To invoke the notification handler, just call the instance without passing
any parameters.

The handler is a loop that listens for notifications on the event and stop
event channels.  When either of these notifications are received, its
associated *pid*, *event* and *extra* (the payload passed with the
notification) are inserted into its *arg_dict* dictionary and the callback
is invoked with this dictionary as a single argument.  When the handler
receives a stop event, it stops listening to both events and return.

In the special case that the timeout of the handler has been set to ``0``,
the handler will poll all events synchronously and return.  If will keep
listening until it receives a stop event.

.. warning::

    If you run this loop in another thread, don't use the same database
    connection for database operations in the main thread.

Sending notifications
---------------------

You can send notifications by either running ``NOTIFY`` commands on the
database directly, or using the following method:

.. method:: NotificationHandler.notify([db], [stop], [payload])

    Generate a notification

    :param int db: the database connection for sending the notification
    :type db: :class:`Connection`
    :param bool stop: whether to produce a normal event or a stop event
    :param str payload: an optional payload to be sent with the notification

This method sends a notification event together with an optional *payload*.
If you set the *stop* flag, a stop notification will be sent instead of
a normal notification.  This will cause the handler to stop listening.

.. warning::

    If the notification handler is running in another thread, you must pass
    a different database connection since PyGreSQL database connections are
    not thread-safe.

Auxiliary methods
-----------------

.. method:: NotificationHandler.listen()

    Start listening for the event and the stop event

This method is called implicitly when the handler is invoked.

.. method:: NotificationHandler.unlisten()

    Stop listening for the event and the stop event

This method is called implicitly when the handler receives a stop event
or when it is closed or deleted.

.. method:: NotificationHandler.close()

    Stop listening and close the database connection

You can call this method instead of :meth:`NotificationHandler.unlisten`
if you want to close not only the handler, but also the database connection
it was created with.