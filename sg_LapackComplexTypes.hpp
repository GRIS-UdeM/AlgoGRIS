/*
 This file is part of SpatGRIS.

 Developers: Gaël Lane Lépine

 SpatGRIS is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 SpatGRIS is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with SpatGRIS.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

#if SAF_USE_OPEN_BLAS_AND_LAPACKE
    #include <complex>

    #if defined(_MSC_VER)
struct _Fcomplex {
    float _Val;
};
struct _Dcomplex {
    double _Val;
};
        #define lapack_complex_float _Fcomplex
        #define lapack_complex_double _Dcomplex
    #else
        // Linux
        #define lapack_complex_float std::complex<float>
        #define lapack_complex_double std::complex<double>
    #endif
#endif