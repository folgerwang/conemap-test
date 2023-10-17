// This file is part of libigl, a simple c++ geometry processing library.
// 
// Copyright (C) 2013 Alec Jacobson <alecjacobson@gmail.com>
// 
// This Source Code Form is subject to the terms of the Mozilla Public License 
// v. 2.0. If a copy of the MPL was not distributed with this file, You can 
// obtain one at http://mozilla.org/MPL/2.0/.
#include "create_vector_vbo.h"

#include <cassert>

// http://www.songho.ca/opengl/gl_vbo.html#create
template <typename T>
IGL_INLINE void igl::opengl::create_vector_vbo(
  const Eigen::Matrix<T,Eigen::Dynamic,Eigen::Dynamic> & V,
  GLuint & V_vbo_id)
{
  //// Expects that input is list of 3D vectors along rows
  //assert(V.cols() == 3);

  // Generate Buffers
  glGenBuffers(1,&V_vbo_id);
  // Bind Buffers
  glBindBuffer(GL_ARRAY_BUFFER,V_vbo_id);
  // Copy data to buffers
  // We expect a matrix with each vertex position on a row, we then want to
  // pass this data to OpenGL reading across rows (row-major)
  if(V.Options & Eigen::RowMajor)
  {
    glBufferData(
      GL_ARRAY_BUFFER,
      sizeof(T)*V.size(),
      V.data(),
      GL_STATIC_DRAW);
  }else
  {
    // Create temporary copy of transpose
    Eigen::Matrix<T,Eigen::Dynamic,Eigen::Dynamic> VT = V.transpose();
    // If its column major then we need to temporarily store a transpose
    glBufferData(
      GL_ARRAY_BUFFER,
      sizeof(T)*V.size(),
      VT.data(),
      GL_STATIC_DRAW);
  }
  // bind with 0, so, switch back to normal pointer operation
  glBindBuffer(GL_ARRAY_BUFFER, 0);
}

#ifdef IGL_STATIC_LIBRARY
// Explicit template instantiation
// generated by autoexplicit.sh
template void igl::opengl::create_vector_vbo<int>(Eigen::Matrix<int, -1, -1, 0, -1, -1> const&, unsigned int&);
// generated by autoexplicit.sh
template void igl::opengl::create_vector_vbo<double>(Eigen::Matrix<double, -1, -1, 0, -1, -1> const&, unsigned int&);
#endif

