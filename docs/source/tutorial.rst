Tutorial
========

Building the compiler/interpreter
---------------------------------

Clone the repository from GitHub::

   git clone https://github.com/vegard/v.git

Build the project::

   ./make.sh

That should be it! You can now run ``./v`` in the current directory to start
the compiler.


Hello world
-----------

Create a file called ``helloworld.v`` containing a single line::

   print "Hello world!"

To run this program in the interpreter, run::

   ./v helloworld.v
