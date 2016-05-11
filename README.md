#Slack-SQL
![screenshot](http://g.recordit.co/bpXw88G5hz.gif)

## Install PostSQL for python library

The query execution is based on PostgreSQL's python library -- [PyGreSQL](http://www.pygresql.org/), it needs to be installed on the server first.
- Install using pip:
  - On terminal, open bash
```
  $sudo bash
```
  - Adding system variables
```
  $export CFLAGS=-Qunused-arguments
  $export CPPFLAGS=-Qunused-arguments
```
  - Install using pip:
```
  $pip install PyGreSQL
```

- Install from source:
  - In the PyGreSQL folder, type ```python setup.py install```

## Set up:
1. Clone this repo
2. Config your database name, host, port, user name, and password in ```connection.py```
```python
db = DB(dbname='',host='',port= ,user='',passwd='')
```
3. Deploy this to server(For example, Heroku)
4. Add this integration to your Slack. Specify your url in the Slack integration URL.
5. All set!

## Slack example command:
- create table:
```
  /sql create table users(id primary key, name varchar, email varchar, age int)
```
- Insert data:
```
  /sql insert into users values(1, 'Seth Wang')
```
- selection:
```
  /sql select users.name from users where id=1
```
- deletion
```
  /sql delete from users where id=2
```
