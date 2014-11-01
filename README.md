uSHET: A Malloc-Free SHET Client Library for Microcontrollers
=============================================================

A simple microcontroller-friendly C library for interfacing with
[SHET](https://github.com/18sg/SHET/) targeted at microcontrollers forked from
[SHETC](https://github.com/18sg/SHETC) and extended to support the complete SHET
client protocol. uSHET intends to replace
[SHETSource](https://github.com/18sg/SHETSource) as the de-facto microcontroller
SHET interface.

uSHET supports the following key features:

* **Transport-independent**: The library simply expects to be fed strings
  received from a network device and will call a user-defined callback with
  strings to transmit. This means the library is suitable for use with any
  network interface/library.
* **Malloc-free**: The library does not use dynamic memory allocation and thus can
  have deterministic memory requirements and use only statically-allocated
  storage. This has the obvious side-effect that the library cannot handle
  arbitrarily large commands but this should not impact most applications.
* **Small-ish**: Though uSHET has not received any serious space optimisation
  passes, it is based on the compact jsmn JSON parser and avoids needlessly
  duplicating JSON strings.
* **Complete SHET client protocol support**: uSHET supports the full SHET client
  protocol and exposes a fairly sane high-level interface, unlike SHETSource.
  This comes with the trade-off that uSHET is heavier-weight than SHETSource in
  terms of network requirements, memory requirements and execution overhead
  since it deals with full JSON strings rather than a compact byte-stream.
* **(Optional) High-Level Wrapper Macros**: Includes `EZSHET`, a library of
  macros which hide much of the boilerplate required in handling dynamically
  typed input from SHET.


Documentation
-------------

The basic SHET library contains fairly extensive API documentation within its
header file, `shet.h`.

The EZSHET library contains fairly extensive API documentation within its header
file, `ezshet.h`.


Test Suite
----------

A test suite is included. It can be compiled and run as follows:

	gcc -Wall -Wextra -Werror test.c -o test && ./test

Note that the testbench actually `#includes` the library sources in order to
test a number of internal functions.
