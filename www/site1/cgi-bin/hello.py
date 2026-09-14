#!/usr/bin/python3
import os
import sys

body = sys.stdin.read()

print("Content-Type: text/plain")
print()
print("Hello from CGI!")
print("REQUEST_METHOD=" + os.environ.get("REQUEST_METHOD", ""))
print("QUERY_STRING=" + os.environ.get("QUERY_STRING", ""))
print("SERVER_NAME=" + os.environ.get("SERVER_NAME", ""))
print("SERVER_PORT=" + os.environ.get("SERVER_PORT", ""))
print("REMOTE_ADDR=" + os.environ.get("REMOTE_ADDR", ""))
print("cwd=" + os.getcwd())
if body:
    print("body=" + body)
