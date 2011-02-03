SHET C Client Library
=====================

A simple C library for interfacing with [SHET](https://github.com/18sg/SHET/). This was mainly written for the new shet command line client [SHETCClient](https://github.com/18sg/SHETCClient).


Requirements
------------

- [json-c](http://oss.metaparadigm.com/json-c/)
- [scons](http://www.scons.org/)


Installation
------------

	$ git clone git://github.com/18sg/SHETC.git
	$ cd SHETC
	$ scons
	$ sudo scons install

Alternatively, Arch users can install the [shet-c-git](https://aur.archlinux.org/packages.php?ID=46039) package from the AUR. Debian users can first switch to Arch, then install the [shet-c-git](https://aur.archlinux.org/packages.php?ID=46039) package from the AUR.

Usage
-----

The best 'documentation' currently available is the command line [client](https://github.com/18sg/SHETCClient), which exercises pretty much all of the API.
