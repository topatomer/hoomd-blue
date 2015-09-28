# -- start license --
# Highly Optimized Object-oriented Many-particle Dynamics -- Blue Edition
# (HOOMD-blue) Open Source Software License Copyright 2009-2015 The Regents of
# the University of Michigan All rights reserved.

# HOOMD-blue may contain modifications ("Contributions") provided, and to which
# copyright is held, by various Contributors who have granted The Regents of the
# University of Michigan the right to modify and/or distribute such Contributions.

# You may redistribute, use, and create derivate works of HOOMD-blue, in source
# and binary forms, provided you abide by the following conditions:

# * Redistributions of source code must retain the above copyright notice, this
# list of conditions, and the following disclaimer both in the code and
# prominently in any materials provided with the distribution.

# * Redistributions in binary form must reproduce the above copyright notice, this
# list of conditions, and the following disclaimer in the documentation and/or
# other materials provided with the distribution.

# * All publications and presentations based on HOOMD-blue, including any reports
# or published results obtained, in whole or in part, with HOOMD-blue, will
# acknowledge its use according to the terms posted at the time of submission on:
# http://codeblue.umich.edu/hoomd-blue/citations.html

# * Any electronic documents citing HOOMD-Blue will link to the HOOMD-Blue website:
# http://codeblue.umich.edu/hoomd-blue/

# * Apart from the above required attributions, neither the name of the copyright
# holder nor the names of HOOMD-blue's contributors may be used to endorse or
# promote products derived from this software without specific prior written
# permission.

# Disclaimer

# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
# WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND/OR ANY
# WARRANTIES THAT THIS SOFTWARE IS FREE OF INFRINGEMENT ARE DISCLAIMED.

# IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
# BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
# LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
# ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
# -- end license --

# Maintainer: joaander / All Developers are free to add commands for new features

from hoomd_script import force;
from hoomd_script import globals;
import hoomd;
import sys;
import math;
from hoomd_script import util;

## \package hoomd_script.wall
# \brief Commands that specify %wall forces
#
# Walls can add forces to any particles within a certain distance of the wall. Walls are created
# when an input XML file is read (read.xml).
#
# By themselves, walls that have been specified in an input file do nothing. Only when you
# specify a wall force (index.e. wall.lj), are forces actually applied between the wall and the
# particle.

## Lennard-Jones %wall %force
#
# The command wall.lj specifies that a Lennard-Jones type %wall %force should be added to every
# particle in the simulation.
#
# The %force \f$ \vec{F}\f$ is
# \f{eqnarray*}
#    \vec{F}  = & -\nabla V(r) & r < r_{\mathrm{cut}} \\
#             = & 0            & r \ge r_{\mathrm{cut}} \\
# \f}
# where
# \f[ V(r) = 4 \varepsilon \left[ \left( \frac{\sigma}{r} \right)^{12} -
#                                        \alpha \left( \frac{\sigma}{r} \right)^{6} \right] \f]
# and \f$ \vec{r} \f$ is the vector pointing from the %wall to the particle parallel to the wall's normal.
#
# The following coefficients must be set for each particle type using set_coeff().
# - \f$ \varepsilon \f$ - \c epsilon (in energy units)
# - \f$ \sigma \f$ - \c sigma (in distance units)
# - \f$ \alpha \f$ - \c alpha (unitless)
#
# \b Example:
# \code
# lj.set_coeff('A', epsilon=1.0, sigma=1.0, alpha=1.0)
# \endcode
#
# This interaction is applied between every particle and every wall defined in the simulation box. Walls are specified
# specified in file given to init.read_xml(). See the page \ref page_xml_file_format for information on creating walls,
# specifically the section \ref sec_xml_wall.
#
# The cutoff radius \f$ r_{\mathrm{cut}} \f$ is set once when wall.lj is specified (see __init__())
#
# \MPI_NOT_SUPPORTED
class lj(force._force):
    ## Specify the Lennard-Jones %wall %force
    #
    # \param r_cut Cutoff radius (in distance units)
    #
    # \b Example:
    # \code
    # lj_wall = wall.lj(r_cut=3.0);
    # \endcode
    #
    # \note Coefficients must be set with set_coeff() before the simulation can be run().
    def __init__(self, r_cut):
        util.print_status_line();

        # Error out in MPI simulations
        if (hoomd.is_MPI_available()):
            if globals.system_definition.getParticleData().getDomainDecomposition():
                globals.msg.error("wall.lj is not supported in multi-processor simulations.\n\n")
                raise RuntimeError("Error setting up wall potential.")

        # initialize the base class
        force._force.__init__(self);

        # create the c++ mirror class
        self.cpp_force = hoomd.LJWallForceCompute(globals.system_definition, r_cut);

        # variable for tracking which particle type coefficients have been set
        self.particle_types_set = [];

        globals.system.addCompute(self.cpp_force, self.force_name);

        # store metadata
        self.metadata_fields = ['r_cut']
        self.r_cut = r_cut

    ## Sets the particle-wall interaction coefficients for a particular particle type
    #
    # \param particle_type Particle type to set coefficients for
    # \param epsilon Coefficient \f$ \varepsilon \f$ in the %force (in energy units)
    # \param sigma Coefficient \f$ \sigma \f$ in the %force (in distance units)
    # \param alpha Coefficient \f$ \alpha \f$ in the %force (unitless)
    #
    # Using set_coeff() requires that the specified %wall %force has been saved in a variable. index.e.
    # \code
    # lj_wall = wall.lj(r_cut=3.0)
    # \endcode
    #
    # \b Examples:
    # \code
    # lj_wall.set_coeff('A', epsilon=1.0, sigma=1.0, alpha=1.0)
    # lj_wall.set_coeff('B', epsilon=1.0, sigma=2.0, alpha=0.0)
    # \endcode
    #
    # The coefficients for every particle type in the simulation must be set
    # before the run() can be started.
    def set_coeff(self, particle_type, epsilon, sigma, alpha):
        util.print_status_line();

        # calculate the parameters
        lj1 = 4.0 * epsilon * math.pow(sigma, 12.0);
        lj2 = alpha * 4.0 * epsilon * math.pow(sigma, 6.0);
        # set the parameters for the appropriate type
        self.cpp_force.setParams(globals.system_definition.getParticleData().getTypeByName(particle_type), lj1, lj2);

        # track which particle types we have set
        if not particle_type in self.particle_types_set:
            self.particle_types_set.append(particle_type);

    def update_coeffs(self):
        # get a list of all particle types in the simulation
        ntypes = globals.system_definition.getParticleData().getNTypes();
        type_list = [];
        for index in range(0,ntypes):
            type_list.append(globals.system_definition.getParticleData().getNameByType(index));

        # check to see if all particle types have been set
        for cur_type in type_list:
            if not cur_type in self.particle_types_set:
                globals.msg.error(str(cur_type) + " coefficients missing in wall.lj\n");
                raise RuntimeError("Error updating coefficients");


class group():
    # Max number of each wall geometry must match c++ defintions
    _max_n_sphere_walls=20;
    _max_n_cylinder_walls=20;
    _max_n_plane_walls=60;
    def __init__(self):
        self.num_spheres=0;
        self.num_cylinders=0;
        self.num_planes=0;
        self.spheres=[];
        self.cylinders=[];
        self.planes=[];
    def add_sphere(self, r, origin, inside=True):
        if self.num_spheres<group._max_n_sphere_walls:
            self.spheres.append(make_sphere_wall(r,origin,inside));
            self.num_spheres+=1;
        else:
            globals.msg.error("Trying to specify more than the maximum allowable number of sphere walls.\n");
            raise RuntimeError('Maximum number of sphere walls already used.');
    def add_cylinder(self, r, origin, axis, inside=True):
        if self.num_cylinders<group._max_n_cylinder_walls:
            self.cylinders.append(make_cylinder_wall(r, origin, axis, inside));
            self.num_cylinders+=1;
        else:
            globals.msg.error("Trying to specify more than the maximum allowable number of cylinder walls.\n");
            raise RuntimeError('Maximum number of cylinder walls already used.');
    def add_plane(self, normal, origin):
        if self.num_planes<group._max_n_plane_walls:
            self.planes.append(make_plane_wall(normal, origin));
            self.num_planes+=1;
        else:
            globals.msg.error("Trying to specify more than the maximum allowable number of plane walls.\n");
            raise RuntimeError('Maximum number of plane walls already used.');
    def del_sphere(self, index):
        if type(index) is list: index = set(index);
        elif type(index) is range: index = set(list(index));
        elif type(index) is not set: index = set([index]);
        if (self.num_spheres-index)>0:
            del(self.spheres[index]);
            self.num_spheres-=1;
        else:
            globals.msg.error("Specified index for deletion is not available.\n");
            raise RuntimeError("del_sphere failed")
    def del_cylinder(self, index):
        if type(index) is list: index = set(index);
        elif type(index) is range: index = set(list(index));
        elif type(index) is not set: index = set([index]);
        if (self.num_cylinders-index)>0:
            del(self.cylinders[index]);
            self.num_cylinders-=1;
        else:
            globals.msg.error("Specified index for deletion is not valid.\n");
            raise RuntimeError("del_cylinder failed")
    def del_plane(self, index):
        if type(index) is list: index = set(index);
        elif type(index) is range: index = set(list(index));
        elif type(index) is not set: index = set([index]);
        if (self.num_planes-index)>0:
            del(self.planes[index]);
            self.num_planes-=1;
        else:
            globals.msg.error("Specified index for deletion is not valid.\n");
            raise RuntimeError("del_plane failed")
    def __repr__(self):
        output="Wall_Data_Sturucture:\nSpheres:%s{"%(self.num_spheres);
        for index in range(self.num_spheres):
            output+="\n[%s:\t%s]"%(repr(index), repr(self.spheres[index]));

        output+="}\nCylinders:%s{"%(self.num_cylinders);
        for index in range(self.num_cylinders):
            output+="\n[%s:\t%s]"%(repr(index), repr(self.cylinders[index]));

        output+="}\nPlanes:%s{"%(self.num_planes);
        for index in range(self.num_planes):
            output+="\n[%s:\t%s]"%(repr(index), repr(self.planes[index]));

        output+="}";
        return output;

class sphere_wall:
    r=0.0;
    origin=hoomd.make_scalar3(0.0,0.0,0.0);
    inside = True;
    def __repr__(self):
        return "Radius=%s\tOrigin=(%d, %d, %d)\tInside=%s" % (str(self.r), self.origin.x, self.origin.y, self.origin.z, str(self.inside));

class cylinder_wall:
    r=0.0;
    origin=hoomd.make_scalar3(0.0,0.0,0.0);
    axis=hoomd.make_scalar3(1.0,0.0,0.0);
    inside = True;
    def __repr__(self):
        return "Radius=%s\tOrigin=(%d, %d, %d)\tAxis=(%d, %d, %d)\tInside=%s" % (str(self.r), self.origin.x, self.origin.y, self.origin.z, self.axis.x, self.axis.y, self.axis.z, str(self.inside));

class plane_wall:
    normal=hoomd.make_scalar3(1.0,0.0,0.0);
    origin=hoomd.make_scalar3(0.0,0.0,0.0);
    def __repr__(self):
        return "Normal=(%d, %d, %d)\tOrigin=(%d, %d, %d)" % (self.normal.x, self.normal.y, self.normal.z, self.origin.x self.origin.y self.origin.z);

def make_sphere_wall(r, origin, inside):
    sphere = sphere_wall();
    sphere.r = r;
    sphere.origin = hoomd.make_scalar3(*origin);
    sphere.inside = inside;
    return sphere;

def make_cylinder_wall(r, origin, axis, inside):
    Cylinder = cylinder_wall();
    Cylinder.r = r;
    Cylinder.origin = hoomd.make_scalar3(*origin);
    Cylinder.axis = hoomd.make_scalar3(*axis);
    Cylinder.inside = inside;
    return Cylinder;

def make_plane_wall(normal, origin):
    Plane = plane_wall();
    Plane.normal = hoomd.make_scalar3(*normal);
    Plane.origin = hoomd.make_scalar3(*origin);
    return Plane;
