// This file is part of libigl, a simple c++ geometry processing library.
// 
// Copyright (C) 2021 Alec Jacobson <alecjacobson@gmail.com>
// 
// This Source Code Form is subject to the terms of the Mozilla Public License 
// v. 2.0. If a copy of the MPL was not distributed with this file, You can 
// obtain one at http://mozilla.org/MPL/2.0/.
#include "marching_cubes.h"
#include "march_cube.h"

// Adapted from public domain code at
// http://paulbourke.net/geometry/polygonise/marchingsource.cpp

#include <unordered_map>
#include <iostream>

template <typename DerivedS, typename DerivedGV, typename DerivedV, typename DerivedF>
IGL_INLINE void igl::marching_cubes(
    const Eigen::MatrixBase<DerivedS> &S,
    const Eigen::MatrixBase<DerivedGV> &GV,
    const unsigned nx,
    const unsigned ny,
    const unsigned nz,
    const typename DerivedS::Scalar isovalue,
    Eigen::PlainObjectBase<DerivedV> &V,
    Eigen::PlainObjectBase<DerivedF> &F)
{
  typedef typename DerivedS::Scalar Scalar;
  typedef unsigned Index;
  // use same order as a2fVertexOffset
  const unsigned ioffset[8] = {0,1,1+nx,nx,nx*ny,1+nx*ny,1+nx+nx*ny,nx+nx*ny};


  std::unordered_map<int64_t,int> E2V;
  V.resize(std::pow(nx*ny*nz,2./3.),3);
  F.resize(std::pow(nx*ny*nz,2./3.),3);
  Index n = 0;
  Index m = 0;

  const auto xyz2i = [&nx,&ny]
    (const int & x, const int & y, const int & z)->unsigned
  {
    return x+nx*(y+ny*(z));
  };
  const auto cube = 
    [
      &GV,&S,&V,&n,&F,&m,&isovalue,
      &E2V,&xyz2i,&ioffset
    ]
    (const int x, const int y, const int z)
  {
    const unsigned i = xyz2i(x,y,z);

    //Make a local copy of the values at the cube's corners
    Eigen::Matrix<Scalar,8,1> cS;
    Eigen::Matrix<Index,8,1> cI;
    //Find which vertices are inside of the surface and which are outside
    for(int c = 0; c < 8; c++)
    {
      const unsigned ic = i + ioffset[c];
      cI(c) = ic;
      cS(c) = S(ic);
    }

    march_cube(GV,cS,cI,isovalue,V,n,F,m,E2V);

  };

  // march over all cubes (loop order chosen to match memory)
  //
  // Should be possible to parallelize safely if threads are "well separated".
  // Like red-black Gauss Seidel. Probably each thread need's their own E2V,V,F,
  // and then merge at the end. Annoying part are the edges lying on the
  // interface between chunks.
  for(int z=0;z<nz-1;z++)
  {
    for(int y=0;y<ny-1;y++)
    {
      for(int x=0;x<nx-1;x++)
      {
        cube(x,y,z);
      }
    }
  }
  V.conservativeResize(n,3);
  F.conservativeResize(m,3);
}

template <
  typename DerivedS, 
  typename DerivedGV, 
  typename DerivedGI, 
  typename DerivedV, 
  typename DerivedF>
IGL_INLINE void igl::marching_cubes(
  const Eigen::MatrixBase<DerivedS> & S,
  const Eigen::MatrixBase<DerivedGV> & GV,
  const Eigen::MatrixBase<DerivedGI> & GI,
  const typename DerivedS::Scalar isovalue,
  Eigen::PlainObjectBase<DerivedV> &V,
  Eigen::PlainObjectBase<DerivedF> &F)
{
  typedef Eigen::Index Index;
  typedef typename DerivedV::Scalar Scalar;

  std::unordered_map<int64_t,int> E2V;
  V.resize(4*GV.rows(),3);
  F.resize(4*GV.rows(),3);
  Index n = 0;
  Index m = 0;

  // march over cubes

  //Make a local copy of the values at the cube's corners
  Eigen::Matrix<Scalar, 8, 1> cS;
  Eigen::Matrix<Index, 8, 1> cI;
  for(Index c = 0;c<GI.rows();c++)
  {
    for(int v = 0; v < 8; v++)
    {
      cI(v) = GI(c,v);
      cS(v) = S(GI(c,v));
    }
    march_cube(GV,cS,cI,isovalue,V,n,F,m,E2V);
  }
  V.conservativeResize(n,3);
  F.conservativeResize(m,3);
}

#ifdef IGL_STATIC_LIBRARY
// Explicit template instantiation
// generated by autoexplicit.sh
template void igl::marching_cubes<Eigen::Matrix<float, -1, 1, 0, -1, 1>, Eigen::Matrix<float, -1, -1, 0, -1, -1>, Eigen::Matrix<float, -1, 3, 1, -1, 3>, Eigen::Matrix<int, -1, 3, 1, -1, 3> >(Eigen::MatrixBase<Eigen::Matrix<float, -1, 1, 0, -1, 1> > const&, Eigen::MatrixBase<Eigen::Matrix<float, -1, -1, 0, -1, -1> > const&, unsigned int, unsigned int, unsigned int, Eigen::Matrix<float, -1, 1, 0, -1, 1>::Scalar, Eigen::PlainObjectBase<Eigen::Matrix<float, -1, 3, 1, -1, 3> >&, Eigen::PlainObjectBase<Eigen::Matrix<int, -1, 3, 1, -1, 3> >&);
// generated by autoexplicit.sh
template void igl::marching_cubes<Eigen::Matrix<double, -1, 1, 0, -1, 1>, Eigen::Matrix<double, -1, -1, 0, -1, -1>, Eigen::Matrix<double, -1, 3, 1, -1, 3>, Eigen::Matrix<int, -1, 3, 1, -1, 3> >(Eigen::MatrixBase<Eigen::Matrix<double, -1, 1, 0, -1, 1> > const&, Eigen::MatrixBase<Eigen::Matrix<double, -1, -1, 0, -1, -1> > const&, unsigned int, unsigned int, unsigned int, Eigen::Matrix<double, -1, 1, 0, -1, 1>::Scalar, Eigen::PlainObjectBase<Eigen::Matrix<double, -1, 3, 1, -1, 3> >&, Eigen::PlainObjectBase<Eigen::Matrix<int, -1, 3, 1, -1, 3> >&);
// generated by autoexplicit.sh
template void igl::marching_cubes<Eigen::Matrix<double, -1, 1, 0, -1, 1>, Eigen::Matrix<double, -1, -1, 0, -1, -1>, Eigen::Matrix<double, -1, -1, 0, -1, -1>, Eigen::Matrix<int, -1, -1, 0, -1, -1> >(Eigen::MatrixBase<Eigen::Matrix<double, -1, 1, 0, -1, 1> > const&, Eigen::MatrixBase<Eigen::Matrix<double, -1, -1, 0, -1, -1> > const&, unsigned int, unsigned int, unsigned int, Eigen::Matrix<double, -1, 1, 0, -1, 1>::Scalar, Eigen::PlainObjectBase<Eigen::Matrix<double, -1, -1, 0, -1, -1> >&, Eigen::PlainObjectBase<Eigen::Matrix<int, -1, -1, 0, -1, -1> >&);
template void igl::marching_cubes<Eigen::Matrix<double, -1, 1, 0, -1, 1>, Eigen::Matrix<double, -1, -1, 0, -1, -1>, Eigen::Matrix<int, -1, -1, 0, -1, -1>, Eigen::Matrix<double, -1, -1, 0, -1, -1>, Eigen::Matrix<int, -1, -1, 0, -1, -1> >(Eigen::MatrixBase<Eigen::Matrix<double, -1, 1, 0, -1, 1> > const&, Eigen::MatrixBase<Eigen::Matrix<double, -1, -1, 0, -1, -1> > const&, Eigen::MatrixBase<Eigen::Matrix<int, -1, -1, 0, -1, -1> > const&, Eigen::Matrix<double, -1, 1, 0, -1, 1>::Scalar, Eigen::PlainObjectBase<Eigen::Matrix<double, -1, -1, 0, -1, -1> >&, Eigen::PlainObjectBase<Eigen::Matrix<int, -1, -1, 0, -1, -1> >&);
template void igl::marching_cubes<Eigen::Matrix<double, -1, 1, 0, -1, 1>, Eigen::Matrix<double, -1, -1, 0, -1, -1>, Eigen::Matrix<int, -1, 8, 0, -1, 8>, Eigen::Matrix<double, -1, -1, 0, -1, -1>, Eigen::Matrix<int, -1, -1, 0, -1, -1> >(Eigen::MatrixBase<Eigen::Matrix<double, -1, 1, 0, -1, 1> > const&, Eigen::MatrixBase<Eigen::Matrix<double, -1, -1, 0, -1, -1> > const&, Eigen::MatrixBase<Eigen::Matrix<int, -1, 8, 0, -1, 8> > const&, Eigen::Matrix<double, -1, 1, 0, -1, 1>::Scalar, Eigen::PlainObjectBase<Eigen::Matrix<double, -1, -1, 0, -1, -1> >&, Eigen::PlainObjectBase<Eigen::Matrix<int, -1, -1, 0, -1, -1> >&);
#endif
