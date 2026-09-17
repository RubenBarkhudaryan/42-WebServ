#!/usr/bin/env python3
import os
import sys

query = os.environ.get('QUERY_STRING', '')
name  = 'World'
for part in query.split('&'):
    if part.startswith('name='):
        name = part[5:] or 'World'
        break

print("Content-Type: text/html")
print("")
print("<!DOCTYPE html>")
print("<html><head><title>Hello CGI</title></head><body>")
print("<h1>Hello, " + name + "!</h1>")
print("<p>Method: "  + os.environ.get('REQUEST_METHOD', '') + "</p>")
print("<p>Query: "   + os.environ.get('QUERY_STRING', '')   + "</p>")
print("<p>Script: "  + os.environ.get('SCRIPT_NAME', '')    + "</p>")
print("<form method='get'>")
print("  Name: <input name='name' value='" + name + "'>")
print("  <button type='submit'>Say hello</button>")
print("</form>")
print("</body></html>")
