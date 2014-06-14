About
=====

Trivially copy a file -- my solution of a homework problem from CS631 at Stevens Institute of Technology.
http://www.cs.stevens.edu/~jschauma/631/f13-hw2.html

Installation
============

Run ''make'' to compile the code. It will create executables named ''tcp''
and ''tcpm''.

Behavior
========

  * The program only handles regular source files.
  * Existing targets are truncated and overwritten, provided that we have proper
    file permissions. Permissions are retained, if possible.
  * The permission of a created copy will be ''source & ~umask'' where ''source''
    are the permissions of the source file.
