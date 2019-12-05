// Copyright (c) 2009-2019 The Regents of the University of Michigan
// This file is part of the HOOMD-blue project, released under the BSD 3-Clause License.

// Include the defined classes that are to be exported to python
#include "IntegratorHPMC.h"
#include "IntegratorHPMCMono.h"

#include "ShapeSphere.h"
#include "ShapeConvexPolygon.h"
#include "ShapePolyhedron.h"
#include "ShapeConvexPolyhedron.h"
#include "ShapeSpheropolyhedron.h"
#include "ShapeSpheropolygon.h"
#include "ShapeSimplePolygon.h"
#include "ShapeEllipsoid.h"
#include "ShapeFacetedEllipsoid.h"
#include "ShapeSphinx.h"
#include "ShapeUnion.h"
#include "AnalyzerSDF.h"
#include "UpdaterBoxMC.h"
#include "UpdaterClusters.h"

#include "GPUTree.h"

#ifdef ENABLE_CUDA
#include "IntegratorHPMCMonoGPU.h"
#endif

#include "modules.h"

/*! \file module.cc
    \brief Export classes to python
*/
using namespace hpmc;
using namespace std;
namespace py = pybind11;

namespace hpmc
{

//! HPMC implementation details
/*! The detail namespace contains classes and functions that are not part of the HPMC public interface. These are
    subject to change without notice and are designed solely for internal use within HPMC.
*/
namespace detail
{

// could move the make_param functions back??

}; // end namespace detail

}; // end namespace hpmc

using namespace hpmc::detail;

//! Define the _hpmc python module exports
PYBIND11_MODULE(_hpmc, m)
    {
    export_IntegratorHPMC(m);

    export_UpdaterBoxMC(m);
    export_external_fields(m);

    export_sphere(m);
    export_convex_polygon(m);
    export_simple_polygon(m);
    export_spheropolygon(m);
    export_polyhedron(m);
    export_ellipsoid(m);
    export_faceted_ellipsoid(m);
    export_sphinx(m);
    export_union_convex_polyhedron(m);
    export_union_faceted_ellipsoid(m);
    export_union_sphere(m);
    export_convex_polyhedron(m);
    export_convex_spheropolyhedron(m);

    py::class_<SphereParams, std::shared_ptr<SphereParams> >(m, "SphereParams")
        .def(pybind11::init< pybind11::dict >())
        .def("asDict", &SphereParams::asDict)
        ;
    py::class_<EllipsoidParams, std::shared_ptr<EllipsoidParams> >(m, "EllipsoidParams")
        .def(pybind11::init< pybind11::dict >())
        .def("asDict", &EllipsoidParams::asDict)
        ;
    py::class_<PolygonVertices, std::shared_ptr<PolygonVertices> >(m, "PolygonVertices")
        .def(pybind11::init< pybind11::dict >())
        .def("asDict", &PolygonVertices::asDict)
        ;
    py::class_<poly3d_data, std::shared_ptr<poly3d_data> >(m, "poly3d_data")
        .def(pybind11::init< pybind11::dict >())
        .def("asDict", &poly3d_data::asDict)
        ;
    py::class_< PolyhedronVertices, std::shared_ptr< PolyhedronVertices > >(m, "PolyhedronVertices")
        .def(pybind11::init< pybind11::dict >())
        .def("asDict", &PolyhedronVertices::asDict)
        ;
    py::class_<faceted_ellipsoid_params, std::shared_ptr<faceted_ellipsoid_params> >(m, "faceted_ellipsoid_params")
        .def(pybind11::init< pybind11::dict >())
        .def("asDict", &faceted_ellipsoid_params::asDict)
        ;
    py::class_<sphinx3d_params, std::shared_ptr<sphinx3d_params> >(m, "sphinx3d_params")
        .def_readwrite("circumsphereDiameter",&sphinx3d_params::circumsphereDiameter)
        .def(pybind11::init< pybind11::dict >())
        .def("asDict", &sphinx3d_params::asDict)
        ;
    py::class_< ShapeUnion<ShapeSphere>::param_type, std::shared_ptr< ShapeUnion<ShapeSphere>::param_type> >(m, "SphereUnionParams")
        .def(pybind11::init< pybind11::dict >())
        .def("asDict", &ShapeUnion<ShapeSphere>::param_type::asDict)
        ;

    py::class_< ShapeUnion<ShapeSpheropolyhedron>::param_type, std::shared_ptr< ShapeUnion<ShapeSpheropolyhedron>::param_type> >(m, "mpoly3d_params")
        .def(pybind11::init< pybind11::dict >())
        .def("asDict", &ShapeUnion<ShapeSpheropolyhedron>::param_type::asDict)
        ;
    py::class_< ShapeUnion<ShapeFacetedEllipsoid>::param_type, std::shared_ptr< ShapeUnion<ShapeFacetedEllipsoid>::param_type> >(m, "mfellipsoid_params")
        .def(pybind11::init< pybind11::dict >())
        .def("asDict", &ShapeUnion<ShapeFacetedEllipsoid>::param_type::asDict)
        ;

    m.def("make_overlapreal3", &make_overlapreal3);
    m.def("make_overlapreal4", &make_overlapreal4);

    // export counters
    export_hpmc_implicit_counters(m);

    export_hpmc_clusters_counters(m);
    }

/*! \defgroup hpmc_integrators HPMC integrators
*/

/*! \defgroup hpmc_analyzers HPMC analyzers
*/

/*! \defgroup shape Shapes
    Shape classes define the geometry of shapes and associated overlap checks
*/

/*! \defgroup vecmath Vector Math
    Vector, matrix, and quaternion math routines
*/

/*! \defgroup hpmc_detail Details
    HPMC implementation details
    @{
*/

/*! \defgroup hpmc_data_structs Data structures
    HPMC internal data structures
*/

/*! \defgroup hpmc_kernels HPMC kernels
    HPMC GPU kernels
*/

/*! \defgroup minkowski Minkowski methods
    Classes and functions related to Minkowski overlap detection methods
*/

/*! \defgroup overlap Other overlap methods
    Classes and functions related to other (brute force) overlap methods
*/

/*! @} */
