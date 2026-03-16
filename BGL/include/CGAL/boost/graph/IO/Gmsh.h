// Copyright (c) 2026
// GeometryFactory (France).  All rights reserved.
//
// This file is part of CGAL (www.cgal.org)
//
// SPDX-License-Identifier: LGPL-3.0-or-later OR LicenseRef-Commercial
//
// Author(s): Generated integration for Gmsh polygon mesh import.

#ifndef CGAL_BGL_IO_GMSH_H
#define CGAL_BGL_IO_GMSH_H

#include <CGAL/IO/Gmsh.h>
#include <CGAL/boost/graph/IO/Generic_facegraph_builder.h>
#include <CGAL/IO/helpers.h>
#include <CGAL/Named_function_parameters.h>
#include <CGAL/boost/graph/named_params_helper.h>

#include <fstream>
#include <string>

namespace CGAL {

namespace IO {
namespace internal {

// Builder that turns the Gmsh polygon soup into a FaceGraph using the generic builder.
template <typename Graph, typename Point>
class MSH_builder
  : public Generic_facegraph_builder<Graph, Point, MSH_builder<Graph, Point> >
{
  typedef MSH_builder<Graph, Point>                                         Self;
  typedef Generic_facegraph_builder<Graph, Point, Self>                     Base;

  typedef typename Base::Point_container                                    Point_container;
  typedef typename Base::Face                                               Face;
  typedef typename Base::Face_container                                     Face_container;

public:
  MSH_builder(std::istream& is) : Base(is) { }

  template <typename NamedParameters>
  bool read(std::istream& is,
            Point_container& points,
            Face_container& faces,
            const NamedParameters& /*np*/)
  {
    std::vector<Face> cells; // unused for surface mesh import
    return read_MSH(is, points, faces, cells);
  }
};

// Because some packages can provide overloads with the same signature to automatically initialize
// property maps (see Surface_mesh/IO/ for example)
template <typename Graph, typename CGAL_NP_TEMPLATE_PARAMETERS>
bool read_MSH_BGL(std::istream& is,
                  Graph& g,
                  const CGAL_NP_CLASS& np)
{
  typedef typename CGAL::GetVertexPointMap<Graph, CGAL_NP_CLASS>::type  VPM;
  typedef typename boost::property_traits<VPM>::value_type              Point;

  internal::MSH_builder<Graph, Point> builder(is);
  return builder(g, np);
}

} // namespace internal

// Public API overloads
template <typename Graph, typename CGAL_NP_TEMPLATE_PARAMETERS>
bool read_MSH(std::istream& is,
              Graph& g,
              const CGAL_NP_CLASS& np = parameters::default_values()
#ifndef DOXYGEN_RUNNING
              , std::enable_if_t<!internal::is_Point_set_or_Range_or_Iterator<Graph>::value>* = nullptr
#endif
              )
{
  return internal::read_MSH_BGL(is, g, np);
}

template <typename Graph, typename CGAL_NP_TEMPLATE_PARAMETERS>
bool read_MSH(const std::string& fname,
              Graph& g,
              const CGAL_NP_CLASS& np = parameters::default_values()
#ifndef DOXYGEN_RUNNING
              , std::enable_if_t<!internal::is_Point_set_or_Range_or_Iterator<Graph>::value>* = nullptr
#endif
              )
{
  std::ifstream is(fname);
  if(!is) return false;
  return internal::read_MSH_BGL(is, g, np);
}

} // namespace IO

} // namespace CGAL

#endif // CGAL_BGL_IO_GMSH_H
