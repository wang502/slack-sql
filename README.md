#Slack-SQL
![screenshot](http://g.recordit.co/bpXw88G5hz.gif)

## Install PyGreSQL using pip

The query execution is based on PostgreSQL's python library -- [PyGreSQL](http://www.pygresql.org/), it needs to be installed on the server first.

- On terminal, open bash
```
  $sudo bash
```
- Adding system variables
```
  $export CFLAGS=-Qunused-arguments
  $export CPPFLAGS=-Qunused-arguments
```
- Install
```
  $pip install PyGreSQL
```

## Install PyGreSQL from source
go into PostgreSQL-5.0 folder, type the folowing commands
```
python setup.py build
python setup.py install
```
## Set up:
1. Clone this repo
2. Config your database name, host, port, user name, and password in ```connection.py```
```python
db = DB(dbname='',host='',port= ,user='',passwd='')
```
3. Deploy this to server(For example, Heroku).
4. Add this integration to your Slack. Specify your url in the Slack integration URL.
5. All set!

## Deploy to Heroku
[![Deploy](https://www.herokucdn.com/deploy/button.svg)](https://heroku.com/deploy)

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
