/*
Highly Optimized Object-oriented Many-particle Dynamics -- Blue Edition
(HOOMD-blue) Open Source Software License Copyright 2009-2016 The Regents of
the University of Michigan All rights reserved.

HOOMD-blue may contain modifications ("Contributions") provided, and to which
copyright is held, by various Contributors who have granted The Regents of the
University of Michigan the right to modify and/or distribute such Contributions.

You may redistribute, use, and create derivate works of HOOMD-blue, in source
and binary forms, provided you abide by the following conditions:

* Redistributions of source code must retain the above copyright notice, this
list of conditions, and the following disclaimer both in the code and
prominently in any materials provided with the distribution.

* Redistributions in binary form must reproduce the above copyright notice, this
list of conditions, and the following disclaimer in the documentation and/or
other materials provided with the distribution.

* All publications and presentations based on HOOMD-blue, including any reports
or published results obtained, in whole or in part, with HOOMD-blue, will
acknowledge its use according to the terms posted at the time of submission on:
http://codeblue.umich.edu/hoomd-blue/citations.html

* Any electronic documents citing HOOMD-Blue will link to the HOOMD-Blue website:
http://codeblue.umich.edu/hoomd-blue/

* Apart from the above required attributions, neither the name of the copyright
holder nor the names of HOOMD-blue's contributors may be used to endorse or
promote products derived from this software without specific prior written
permission.

Disclaimer

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS ``AS IS'' AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND/OR ANY
WARRANTIES THAT THIS SOFTWARE IS FREE OF INFRINGEMENT ARE DISCLAIMED.

IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Maintainer: joaander / Anyone is free to add their own pair potentials here

#ifndef __ALL_EXTERNAL_POTENTIALS__H__
#define __ALL_EXTERNAL_POTENTIALS__H__

#include "PotentialExternal.h"
#include "EvaluatorExternalPeriodic.h"
#include "EvaluatorExternalElectricField.h"
#include "EvaluatorWalls.h"
#include "AllPairPotentials.h"

#ifdef ENABLE_CUDA
#include "PotentialExternalGPU.h"
#endif

/*! \file AllExternalPotentials.h
    \brief Handy list of typedefs for all of the templated external potentials in hoomd
*/

#ifdef NVCC
#error This header cannot be compiled by nvcc
#endif

//! External potential to impose periodic structure
typedef PotentialExternal<EvaluatorExternalPeriodic> PotentialExternalPeriodic;

//! Electric field
typedef PotentialExternal<EvaluatorExternalElectricField> PotentialExternalElectricField;

typedef PotentialExternal<EvaluatorWalls<EvaluatorPairLJ> > WallsPotentialLJ;
typedef PotentialExternal<EvaluatorWalls<EvaluatorPairSLJ> > WallsPotentialSLJ;
typedef PotentialExternal<EvaluatorWalls<EvaluatorPairForceShiftedLJ> > WallsPotentialForceShiftedLJ;
typedef PotentialExternal<EvaluatorWalls<EvaluatorPairMie> > WallsPotentialMie;
typedef PotentialExternal<EvaluatorWalls<EvaluatorPairGauss> > WallsPotentialGauss;
typedef PotentialExternal<EvaluatorWalls<EvaluatorPairYukawa> > WallsPotentialYukawa;
typedef PotentialExternal<EvaluatorWalls<EvaluatorPairMorse> > WallsPotentialMorse;


#ifdef ENABLE_CUDA
//! External potential to impose periodic structure on the GPU
typedef PotentialExternalGPU<EvaluatorExternalPeriodic> PotentialExternalPeriodicGPU;
typedef PotentialExternalGPU<EvaluatorExternalElectricField> PotentialExternalElectricFieldGPU;
typedef PotentialExternalGPU<EvaluatorWalls<EvaluatorPairLJ> > WallsPotentialLJGPU;
typedef PotentialExternalGPU<EvaluatorWalls<EvaluatorPairSLJ> > WallsPotentialSLJGPU;
typedef PotentialExternalGPU<EvaluatorWalls<EvaluatorPairForceShiftedLJ> > WallsPotentialForceShiftedLJGPU;
typedef PotentialExternalGPU<EvaluatorWalls<EvaluatorPairMie> > WallsPotentialMieGPU;
typedef PotentialExternalGPU<EvaluatorWalls<EvaluatorPairGauss> > WallsPotentialGaussGPU;
typedef PotentialExternalGPU<EvaluatorWalls<EvaluatorPairYukawa> > WallsPotentialYukawaGPU;
typedef PotentialExternalGPU<EvaluatorWalls<EvaluatorPairMorse> > WallsPotentialMorseGPU;

#endif

#endif // __EXTERNAL_POTENTIALS_H__