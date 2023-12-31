// This file is part of libigl, a simple c++ geometry processing library.
// 
// Copyright (C) 2016 Alec Jacobson <alecjacobson@gmail.com>
// 
// This Source Code Form is subject to the terms of the Mozilla Public License 
// v. 2.0. If a copy of the MPL was not distributed with this file, You can 
// obtain one at http://mozilla.org/MPL/2.0/.
#ifndef IGL_SETXOR_H
#define IGL_SETXOR_H
#include "igl_inline.h"
#include <Eigen/Core>
namespace igl
{
  /// Set xor of elements of matrices
  ///
  /// @param[in] A  m-long vector of indices
  /// @param[in] B  n-long vector of indices
  /// @param[out] C  (k<=m)-long vector of unique elements appearing in A but not in B or
  ///               B but not in A
  /// @param[out] IA  (<k<=m)-long list of indices into A so that C = sort([A(IA);B(IB)])
  /// @param[out] IB  (<k<=m)-long list of indices into B so that C = sort([A(IA);B(IB)])
  ///
  template <
    typename DerivedA,
    typename DerivedB,
    typename DerivedC,
    typename DerivedIA,
    typename DerivedIB>
  IGL_INLINE void setxor(
    const Eigen::MatrixBase<DerivedA> & A,
    const Eigen::MatrixBase<DerivedB> & B,
    Eigen::PlainObjectBase<DerivedC> & C,
    Eigen::PlainObjectBase<DerivedIA> & IA,
    Eigen::PlainObjectBase<DerivedIB> & IB);
}

#ifndef IGL_STATIC_LIBRARY
#  include "setxor.cpp"
#endif
#endif

