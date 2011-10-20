# Concurrent Revisions in C++ #

# About

This is a C++ implementation of [Concurrent Revisions][1]

Supported features:

* Versioned variables
* Cumlative variables
* fork/join

Future work:

* Fix Memory Leaks
* Optimize concurrent_int_map
* Use LWP/LWT

# Install

    $ ./waf configure
    $ ./waf build --check
    $ sudo ./waf install

# Usage

Please see test.cpp and bench.cpp

[1]: http://research.microsoft.com/apps/pubs/default.aspx?id=132619
