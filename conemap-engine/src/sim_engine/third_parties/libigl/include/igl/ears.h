#ifndef IGL_EARS_H
#define IGL_EARS_H
#include "igl_inline.h"
#include <Eigen/Core>
namespace igl
{
  /// Find all ears (faces with two boundary edges) in a given mesh
  /// 
  ///
  /// @param[in] F  #F by 3 list of triangle mesh indices
  /// @param[out] ears  #ears list of indices into F of ears
  /// @param[out] ear_opp  #ears list of indices indicating which edge is non-boundary
  ///     (connecting to flops)
  /// 
  template <
    typename DerivedF,
    typename Derivedear,
    typename Derivedear_opp>
  IGL_INLINE void ears(
    const Eigen::MatrixBase<DerivedF> & F,
    Eigen::PlainObjectBase<Derivedear> & ear,
    Eigen::PlainObjectBase<Derivedear_opp> & ear_opp);
}
#ifndef IGL_STATIC_LIBRARY
#  include "ears.cpp"
#endif
#endif
