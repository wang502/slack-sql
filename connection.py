from pg import DB
import os
from flask import Flask, request, Response, redirect

db = DB(dbname='d4pvr81kbkvo46',host='ec2-23-21-157-223.compute-1.amazonaws.com', port=5432, user='oqjvqwymcazkhw',passwd='cQb5tvoVzhfr8yZNYA6B0dSdJq')
app = Flask(__name__)

@app.route("/", methods=['post'])
def hello():
    q = request.values.get('text')
    result = str(db.query(q))
    return result

if __name__ == "__main__":
    port = int(os.environ.get('PORT', 5000))
    app.run(host='0.0.0.0', port=port)
