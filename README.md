***XtalComp with spglib compilation removed***

======================================================================
What is XtalComp for?

Computationally identifying duplicate crystal structures taken from
the output of modern solid state calculations is a non-trivial
exercise for many reasons. The translation vectors in the description
are not unique -- they may be transformed into linear combinations of
themselves and continue to describe the same extended structure. The
coordinates and cell parameters contain numerical noise. The periodic
boundary conditions at the unit cell faces, edges, and corners can
cause very small displacements of atomic coordinates to result in very
different representations. The positions of all atoms may be uniformly
translated by an arbitrary vector without modifying the underlying
structure. Additionally, certain applications may consider
enantiomorphic structures to be identical.

The XtalComp algorithm overcomes these issues to detect duplicate
structures regardless of differences in representation. It begins by
performing a Niggli reduction on the inputs, standardizing the
translation vectors and orientations. A transform search is performed
to identify candidate sets of rotations, reflections, and translations
that potentially map the description of one crystal onto the other,
solving the problems of enantiomorphs and rotationally degenerate
lattices. The atomic positions resulting from each candidate transform
are then compared, using a cell-expansion technique to remove periodic
boundary issues. Computational noise is treated by comparing
non-integer quantities using a specified tolerance.


======================================================================
How to use XtalComp

mkdir build
cd build
ccmake ..
make

This will build the XtalComp static library and a sample testing
script. To use XtalComp in your application, link to the statically
built XtalComp library in the "build" directory and include
xtalcomp.h in your application. See test.cpp for example usage.

To run the sample test script, execute

./test

The output will indicate if any tests have failed.


======================================================================
Files included:

CMakeLists.txt     : Build system config
xtalcomp.*         : Implementation of the XtalComp algorithm
stablecomparison.* : Convenience functions for comparing floating
                       point number to a tolerance.
xcvector.*         : Simple column 3-vector class
xcmatrix.*         : Simple 3x3 matrix class
xctransform.*      : Simple class transform class
test.*             : Sample test script
