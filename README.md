HLDemo
==========

This is a library for reading and writing GoldSource demo files.

#Building
####Windows
- Get [Boost](http://www.boost.org/) and [Boost.Nowide](http://cppcms.com/files/nowide/html/) and build the latter.
- Create a build directory along the *src* directory.
- Run `cmake -DBOOST_ROOT=path\to\boost\base\dir ..` from the build directory.
- Compile the **ALL_BUILD** project from the generated VS solution.

####Linux
- Get Boost.Nowide.
- Create a build directory along the *src* directory.
- Run `cmake ..` from the build directory.
- Run `make` from the build directory.
