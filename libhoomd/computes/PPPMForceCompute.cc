/*
Highly Optimized Object-oriented Many-particle Dynamics -- Blue Edition
(HOOMD-blue) Open Source Software License Copyright 2008, 2009 Ames Laboratory
Iowa State University and The Regents of the University of Michigan All rights
reserved.

HOOMD-blue may contain modifications ("Contributions") provided, and to which
copyright is held, by various Contributors who have granted The Regents of the
University of Michigan the right to modify and/or distribute such Contributions.

Redistribution and use of HOOMD-blue, in source and binary forms, with or
without modification, are permitted, provided that the following conditions are
met:

* Redistributions of source code must retain the above copyright notice, this
list of conditions, and the following disclaimer.

* Redistributions in binary form must reproduce the above copyright notice, this
list of conditions, and the following disclaimer in the documentation and/or
other materials provided with the distribution.

* Neither the name of the copyright holder nor the names of HOOMD-blue's
contributors may be used to endorse or promote products derived from this
software without specific prior written permission.

Disclaimer

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER AND CONTRIBUTORS ``AS IS''
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE, AND/OR
ANY WARRANTIES THAT THIS SOFTWARE IS FREE OF INFRINGEMENT ARE DISCLAIMED.

IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// $Id: PPPMForceCompute.cc 2927 2010-04-14 13:31:03Z joaander $
// $URL: https://codeblue.umich.edu/hoomd-blue/svn/trunk/libhoomd/computes/PPPMForceCompute.cc $
// Maintainer: joaander

#ifdef WIN32
#pragma warning( push )
#pragma warning( disable : 4244 )
#endif

#include <boost/python.hpp>
#include <boost/bind.hpp>

#include "PPPMForceCompute.h"

#include <iostream>
#include <sstream>
#include <stdexcept>
#include <math.h>

#ifdef ENABLE_OPENMP
#include <omp.h>
#endif

using namespace boost;
using namespace boost::python;
using namespace std;


/*! \file PPPMForceCompute.cc
    \brief Contains code for the PPPMForceCompute class
*/

/*! \param sysdef System to compute forces on
    \param nlist Neighbor list
    \post Memory is allocated, and forces are zeroed.
*/

int PPPMData::compute_pppm_flag = 0;
Scalar PPPMData::Nx;                               
Scalar PPPMData::Ny;                               
Scalar PPPMData::Nz;                               
Scalar PPPMData::q2;                               
Scalar PPPMData::q;                               
Scalar PPPMData::kappa;                            
Scalar PPPMData::energy_virial_factor;
Scalar PPPMData::pppm_energy;
GPUArray<cufftComplex> PPPMData::m_rho_real_space; 
GPUArray<Scalar> PPPMData::m_green_hat;           
GPUArray<Scalar3> PPPMData::m_vg;                  
GPUArray<Scalar2> PPPMData::o_data;                
GPUArray<Scalar2> PPPMData::i_data;                

PPPMForceCompute::PPPMForceCompute(boost::shared_ptr<SystemDefinition> sysdef, 
                                   boost::shared_ptr<NeighborList> nlist)
    : ForceCompute(sysdef), m_nlist(nlist)
    {
    assert(m_pdata);
    assert(m_nlist);

    m_box_changed = false;
    m_boxchange_connection = m_pdata->connectBoxChange(bind(&PPPMForceCompute::slotBoxChanged, this));
    }

PPPMForceCompute::~PPPMForceCompute()
    {
    m_boxchange_connection.disconnect();
    }

/*! \param Nx Number of grid points in x direction
  \param Ny Number of grid points in y direction
  \param Nz Number of grid points in z direction
  \param order Number of grid points in each direction to assign charges to
  \param kappa Screening parameter in erfc
  \param rcut Short-ranged cutoff, used for computing the relative force error

  Sets parameters for the long-ranged part of the electrostatics calculation
*/
void PPPMForceCompute::setParams(int Nx, int Ny, int Nz, int order, Scalar kappa, Scalar rcut)
    {
	m_Nx = Nx;
	m_Ny = Ny;
	m_Nz= Nz;
	m_order = order;
	m_kappa = kappa;
	m_rcut = rcut;

	PPPMData::compute_pppm_flag = 1;
	if(!(m_Nx == 2)&& !(m_Nx == 4)&& !(m_Nx == 8)&& !(m_Nx == 16)&& !(m_Nx == 32)&& !(m_Nx == 64)&& !(m_Nx == 128)&& !(m_Nx == 256)&& !(m_Nx == 512)&& !(m_Nx == 1024))
        {
	    cout << endl << endl <<"------ ATTENTION X gridsize should be a power of 2 ------" << endl << endl;
        }
	if(!(m_Ny == 2)&& !(m_Ny == 4)&& !(m_Ny == 8)&& !(m_Ny == 16)&& !(m_Ny == 32)&& !(m_Ny == 64)&& !(m_Ny == 128)&& !(m_Ny == 256)&& !(m_Ny == 512)&& !(m_Ny == 1024))
        {
	    cout << endl << endl <<"------ ATTENTION Y gridsize should be a power of 2 ------" << endl << endl;
        }
	if(!(m_Nz == 2)&& !(m_Nz == 4)&& !(m_Nz == 8)&& !(m_Nz == 16)&& !(m_Nz == 32)&& !(m_Nz == 64)&& !(m_Nz == 128)&& !(m_Nz == 256)&& !(m_Nz == 512)&& !(m_Nz == 1024))
        {
	    cout << endl << endl <<"------ ATTENTION Z gridsize should be a power of 2 ------" << endl << endl;
        }
	if (m_order * (2*m_order +1) > CONSTANT_SIZE)
        {
	    cerr << endl << "***Error! interpolation order too high, doesn't fit into constant array" << endl << endl;
	    throw std::runtime_error("Error initializing PPPMComputeGPU");
        }
	if (m_order > MaxOrder)
        {
	    cerr << endl << "interpolation order too high, max is " << MaxOrder << endl << endl;
	    throw std::runtime_error("Error initializing PPPMComputeGPU");
        }

	GPUArray<cufftComplex> n_rho_real_space(Nx*Ny*Nz, exec_conf);
	PPPMData::m_rho_real_space.swap(n_rho_real_space);
	GPUArray<Scalar> n_green_hat(Nx*Ny*Nz, exec_conf);
	PPPMData::m_green_hat.swap(n_green_hat);
	GPUArray<Scalar3> n_vg(Nx*Ny*Nz, exec_conf);
	PPPMData::m_vg.swap(n_vg);

	GPUArray<Scalar3> n_kvec(Nx*Ny*Nz, exec_conf);
	m_kvec.swap(n_kvec);
	GPUArray<cufftComplex> n_Ex(Nx*Ny*Nz, exec_conf);
	m_Ex.swap(n_Ex);
	GPUArray<cufftComplex> n_Ey(Nx*Ny*Nz, exec_conf);
	m_Ey.swap(n_Ey);
	GPUArray<cufftComplex> n_Ez(Nx*Ny*Nz, exec_conf);
	m_Ez.swap(n_Ez);
	GPUArray<Scalar> n_gf_b(order, exec_conf);
	m_gf_b.swap(n_gf_b);
	GPUArray<Scalar> n_rho_coeff(order, exec_conf);
	m_rho_coeff.swap(n_rho_coeff);
	GPUArray<Scalar3> n_field(Nx*Ny*Nz, exec_conf);
	m_field.swap(n_field);
	const BoxDim& box = m_pdata->getBox();
	const ParticleDataArraysConst& arrays = m_pdata->acquireReadOnly();

	Scalar Lx = box.xhi - box.xlo;
	Scalar Ly = box.yhi - box.ylo;
	Scalar Lz = box.zhi - box.zlo;

    // get system charge
	m_q = 0.f;
	m_q2 = 0.0;
	for(int i = 0; i < (int)arrays.nparticles; i++) {
	    m_q += arrays.charge[i];
	    m_q2 += arrays.charge[i]*arrays.charge[i];
        }
	PPPMData::q = m_q;
	if(fabs(m_q) > 0.0) printf("WARNING system in not neutral, the net charge is %g\n", m_q);

    // compute RMS force error
	Scalar hx =  Lx/(Scalar)Nx;
	Scalar hy =  Ly/(Scalar)Ny;
	Scalar hz =  Lz/(Scalar)Nz;
	Scalar lprx = PPPMForceCompute::rms(hx, Lx, (int)arrays.nparticles); 
	Scalar lpry = PPPMForceCompute::rms(hy, Lz, (int)arrays.nparticles);
	Scalar lprz = PPPMForceCompute::rms(hz, Lz, (int)arrays.nparticles);
	Scalar lpr = sqrt(lprx*lprx + lpry*lpry + lprz*lprz) / sqrt(3.0);
	Scalar spr = 2.0*m_q2*exp(-m_kappa*m_kappa*m_rcut*m_rcut) / sqrt((int)arrays.nparticles*m_rcut*Lx*Ly*Lz);

	double RMS_error = MAX(lpr,spr);
	if(RMS_error > 0.1) {
	    printf("!!!!!!!\n!!!!!!!\n!!!!!!!\nWARNING RMS error of %g is probably too high %f %f\n!!!!!!!\n!!!!!!!\n!!!!!!!\n", RMS_error, lpr, spr);
        }
	else{
	    printf("RMS error: %g\n", RMS_error);
        }

 	PPPMForceCompute::compute_rho_coeff();

	Scalar3 inverse_lattice_vector;
	Scalar invdet = 2.0f*M_PI/(Lx*Lz*Lz);
	inverse_lattice_vector.x = invdet*Ly*Lz;
	inverse_lattice_vector.y = invdet*Lx*Lz;
	inverse_lattice_vector.z = invdet*Lx*Ly;

	ArrayHandle<Scalar3> h_kvec(m_kvec, access_location::host, access_mode::readwrite);
	// Set up the k-vectors
	int ix, iy, iz, kper, lper, mper, k, l, m;
	for (ix = 0; ix < Nx; ix++) {
	    Scalar3 j;
	    j.x = ix > Nx/2 ? ix - Nx : ix;
	    for (iy = 0; iy < Ny; iy++) {
            j.y = iy > Ny/2 ? iy - Ny : iy;
            for (iz = 0; iz < Nz; iz++) {
                j.z = iz > Nz/2 ? iz - Nz : iz;
                h_kvec.data[iz + Nz * (iy + Ny * ix)].x =  j.x*inverse_lattice_vector.x;
                h_kvec.data[iz + Nz * (iy + Ny * ix)].y =  j.y*inverse_lattice_vector.y;
                h_kvec.data[iz + Nz * (iy + Ny * ix)].z =  j.z*inverse_lattice_vector.z;
                }
            }
        }
 
	// Set up constants for virial calculation
	ArrayHandle<Scalar3> h_vg(PPPMData::m_vg, access_location::host, access_mode::readwrite);;
	for(int x = 0; x < Nx; x++)
        {
	    for(int y = 0; y < Ny; y++)
            {
            for(int z = 0; z < Nz; z++)
                {
                Scalar3 kvec = h_kvec.data[z + Nz * (y + Ny * x)];
                Scalar sqk =  kvec.x*kvec.x;
                sqk += kvec.y*kvec.y;
                sqk += kvec.z*kvec.z;
	
                if (sqk == 0.0) 
                    {
                    h_vg.data[z + Nz * (y + Ny * x)].x = 0.0f;
                    h_vg.data[z + Nz * (y + Ny * x)].y = 0.0f;
                    h_vg.data[z + Nz * (y + Ny * x)].z = 0.0f;
                    }
                else
                    {
                    Scalar vterm = -2.0 * (1.0/sqk + 0.25/(kappa*kappa));
                    h_vg.data[z + Nz * (y + Ny * x)].x =  1.0 + vterm*kvec.x*kvec.x;
                    h_vg.data[z + Nz * (y + Ny * x)].y =  1.0 + vterm*kvec.y*kvec.y;
                    h_vg.data[z + Nz * (y + Ny * x)].z =  1.0 + vterm*kvec.z*kvec.z;
                    }
                } 
            } 
        }


	// Set up the grid based Green's function
	ArrayHandle<Scalar> h_green_hat(PPPMData::m_green_hat, access_location::host, access_mode::readwrite);
	Scalar snx, sny, snz, snx2, sny2, snz2;
	Scalar argx, argy, argz, wx, wy, wz, sx, sy, sz, qx, qy, qz;
	Scalar sum1, dot1, dot2;
	Scalar numerator, denominator, sqk;

	Scalar unitkx = (2.0*M_PI/Lx);
	Scalar unitky = (2.0*M_PI/Ly);
	Scalar unitkz = (2.0*M_PI/Lz);
   
    
	Scalar xprd = Lx; 
	Scalar yprd = Ly; 
	Scalar zprd_slab = Lz; 
    
	Scalar form = 1.0;

	PPPMForceCompute::compute_gf_denom();

	Scalar temp = floor(((kappa*xprd/(M_PI*Nx)) * 
                         pow(-log(EPS_HOC),0.25)));
	int nbx = (int)temp;

	temp = floor(((kappa*yprd/(M_PI*Ny)) * 
                  pow(-log(EPS_HOC),0.25)));
	int nby = (int)temp;

	temp =  floor(((kappa*zprd_slab/(M_PI*Nz)) * 
                   pow(-log(EPS_HOC),0.25)));
	int nbz = (int)temp;

    
	for (m = 0; m < Nz; m++) {
	    mper = m - Nz*(2*m/Nz);
	    snz = sin(0.5*unitkz*mper*zprd_slab/Nz);
	    snz2 = snz*snz;

	    for (l = 0; l < Ny; l++) {
            lper = l - Ny*(2*l/Ny);
            sny = sin(0.5*unitky*lper*yprd/Ny);
            sny2 = sny*sny;

            for (k = 0; k < Nx; k++) {
                kper = k - Nx*(2*k/Nx);
                snx = sin(0.5*unitkx*kper*xprd/Nx);
                snx2 = snx*snx;
      
                sqk = pow(unitkx*kper,2.0f) + pow(unitky*lper,2.0f) + 
                    pow(unitkz*mper,2.0f);
                if (sqk != 0.0) {
                    numerator = form*12.5663706/sqk;
                    denominator = gf_denom(snx2,sny2,snz2);  

                    sum1 = 0.0;
                    for (ix = -nbx; ix <= nbx; ix++) {
                        qx = unitkx*(kper+(Scalar)(Nx*ix));
                        sx = exp(-.25*pow(qx/kappa,2.0f));
                        wx = 1.0;
                        argx = 0.5*qx*xprd/(Scalar)Nx;
                        if (argx != 0.0) wx = pow(sin(argx)/argx,order);
                        for (iy = -nby; iy <= nby; iy++) {
                            qy = unitky*(lper+(Scalar)(Ny*iy));
                            sy = exp(-.25*pow(qy/kappa,2.0f));
                            wy = 1.0;
                            argy = 0.5*qy*yprd/(Scalar)Ny;
                            if (argy != 0.0) wy = pow(sin(argy)/argy,order);
                            for (iz = -nbz; iz <= nbz; iz++) {
                                qz = unitkz*(mper+(Scalar)(Nz*iz));
                                sz = exp(-.25*pow(qz/kappa,2.0f));
                                wz = 1.0;
                                argz = 0.5*qz*zprd_slab/(Scalar)Nz;
                                if (argz != 0.0) wz = pow(sin(argz)/argz,order);

                                dot1 = unitkx*kper*qx + unitky*lper*qy + unitkz*mper*qz;
                                dot2 = qx*qx+qy*qy+qz*qz;
                                sum1 += (dot1/dot2) * sx*sy*sz * pow(wx*wy*wz,2.0f);
                                }
                            }
                        }
                    h_green_hat.data[m + Nz * (l + Ny * k)] = numerator*sum1/denominator;
                    } else h_green_hat.data[m + Nz * (l + Ny * k)] = 0.0;
                }
            }
        }
	Scalar scale = 1.0f/((Scalar)(Nx * Ny * Nz));
	m_energy_virial_factor = 0.5 * Lx * Ly * Lz * scale * scale;

	PPPMData::Nx = m_Nx;
	PPPMData::Ny = m_Ny;
	PPPMData::Nz = m_Nx;
	PPPMData::q2 = m_q2;
	PPPMData::kappa = m_kappa;
	PPPMData::energy_virial_factor = m_energy_virial_factor;

	m_data_location = cpu;

	m_pdata->release();
        }

std::vector< std::string > PPPMForceCompute::getProvidedLogQuantities()
    {
    vector<string> list;
    list.push_back("pppm_energy");
    return list;
    }

/*! \param quantity Name of the quantity to get the log value of
  \param timestep Current time step of the simulation
*/
Scalar PPPMForceCompute::getLogValue(const std::string& quantity, unsigned int timestep)
    {
    if (quantity == string("pppm_energy"))
        {
        compute(timestep);
        Scalar energy = calcEnergySum();
        energy += PPPMData::pppm_energy;
        return energy;
        }
    else
        {
        cerr << endl << "***Error! " << quantity << " is not a valid log quantity for PPPMForceCompute" << endl << endl;
        throw runtime_error("Error getting log value");
        }
    }

/*! Actually perform the force computation
  \param timestep Current time step
*/
void PPPMForceCompute::computeForces(unsigned int timestep)
    {

    if (m_prof) m_prof->push("PPPM");
    
    if(m_box_changed) {
        const BoxDim& box = m_pdata->getBox();
        Scalar Lx = box.xhi - box.xlo;
        Scalar Ly = box.yhi - box.ylo;
        Scalar Lz = box.zhi - box.zlo;
        PPPMForceCompute::reset_kvec_green_hat_cpu();
        Scalar scale = 1.0f/((Scalar)(m_Nx * m_Ny * m_Nz));
        m_energy_virial_factor = 0.5 * Lx * Ly * Lz * scale * scale;
        PPPMData::energy_virial_factor = m_energy_virial_factor;
        }

	PPPMForceCompute::assign_charges_to_grid();

//FFTs go next

	PPPMForceCompute::combined_green_e();
//More FFTs

	PPPMForceCompute::calculate_forces();

    m_pdata->release();
  


#ifdef ENABLE_CUDA
    // the data is now only up to date on the CPU
    m_data_location = cpu;
#endif
    
//    int64_t flops = size*(3 + 9 + 14 + 2 + 16)1;
//   int64_t mem_transfer = m_pdata->getN() * 5 * sizeof(Scalar) + size * ( (4)*sizeof(unsigned int) + (6+2+20)*sizeof(Scalar) );
    if (m_prof) m_prof->pop(1, 1);
    }

Scalar PPPMForceCompute::rms(Scalar h, Scalar prd, Scalar natoms)
    {
    int m;
    Scalar sum = 0.0;
    Scalar acons[8][7]; 

    acons[1][0] = 2.0 / 3.0;
    acons[2][0] = 1.0 / 50.0;
    acons[2][1] = 5.0 / 294.0;
    acons[3][0] = 1.0 / 588.0;
    acons[3][1] = 7.0 / 1440.0;
    acons[3][2] = 21.0 / 3872.0;
    acons[4][0] = 1.0 / 4320.0;
    acons[4][1] = 3.0 / 1936.0;
    acons[4][2] = 7601.0 / 2271360.0;
    acons[4][3] = 143.0 / 28800.0;
    acons[5][0] = 1.0 / 23232.0;
    acons[5][1] = 7601.0 / 13628160.0;
    acons[5][2] = 143.0 / 69120.0;
    acons[5][3] = 517231.0 / 106536960.0;
    acons[5][4] = 106640677.0 / 11737571328.0;
    acons[6][0] = 691.0 / 68140800.0;
    acons[6][1] = 13.0 / 57600.0;
    acons[6][2] = 47021.0 / 35512320.0;
    acons[6][3] = 9694607.0 / 2095994880.0;
    acons[6][4] = 733191589.0 / 59609088000.0;
    acons[6][5] = 326190917.0 / 11700633600.0;
    acons[7][0] = 1.0 / 345600.0;
    acons[7][1] = 3617.0 / 35512320.0;
    acons[7][2] = 745739.0 / 838397952.0;
    acons[7][3] = 56399353.0 / 12773376000.0;
    acons[7][4] = 25091609.0 / 1560084480.0;
    acons[7][5] = 1755948832039.0 / 36229939200000.0;
    acons[7][6] = 4887769399.0 / 37838389248.0;

    for (m = 0; m < m_order; m++) 
        sum += acons[m_order][m] * pow(h*m_kappa,2.0f*(Scalar)m);
    Scalar value = m_q2 * pow(h*m_kappa,(Scalar)m_order) *
        sqrt(m_kappa*prd*sqrt(2.0*M_PI)*sum/natoms) / (prd*prd);
    return value;
    }


void PPPMForceCompute::compute_rho_coeff()
    {
    int j, k, l, m;
    Scalar s;
    Scalar *a = (Scalar*)malloc(m_order * (2*m_order+1) * sizeof(Scalar)); 
    ArrayHandle<Scalar> h_rho_coeff(m_rho_coeff, access_location::host, access_mode::readwrite);

    //    usage: a[x][y] = a[y + x*(2*m_order+1)]
    
    for(l=0; l<m_order; l++)
        {
        for(m=0; m<(2*m_order+1); m++)
            {
            h_rho_coeff.data[m + l*(2*m_order +1)] = 0.0f;
            }
        }

    for (k = -m_order; k <= m_order; k++) 
        for (l = 0; l < m_order; l++) {
            a[(k+m_order) + l * (2*m_order+1)] = 0.0f;
            }

    a[m_order + 0 * (2*m_order+1)] = 1.0f;
    for (j = 1; j < m_order; j++) {
        for (k = -j; k <= j; k += 2) {
            s = 0.0;
            for (l = 0; l < j; l++) {
                a[(k + m_order) + (l+1)*(2*m_order+1)] = (a[(k+1+m_order) + l * (2*m_order + 1)] - a[(k-1+m_order) + l * (2*m_order + 1)]) / (l+1);
                s += pow(0.5,(double) (l+1)) * (a[(k-1+m_order) + l * (2*m_order + 1)] + pow(-1.0,(double) l) * a[(k+1+m_order) + l * (2*m_order + 1)] ) / (double)(l+1);
                }
            a[k+m_order + 0 * (2*m_order+1)] = s;
            }
        }

    m = 0;
    for (k = -(m_order-1); k < m_order; k += 2) {
        for (l = 0; l < m_order; l++) {
            h_rho_coeff.data[m + l*(2*m_order +1)] = a[k+m_order + l * (2*m_order + 1)];
            }
        m++;
        }
    free(a);
    }

void PPPMForceCompute::compute_gf_denom()
    {
	int k,l,m;
  	ArrayHandle<Scalar> h_gf_b(m_gf_b, access_location::host, access_mode::readwrite);
	for (l = 1; l < m_order; l++) h_gf_b.data[l] = 0.0;
	h_gf_b.data[0] = 1.0;
  
	for (m = 1; m < m_order; m++) {
	    for (l = m; l > 0; l--) {
            h_gf_b.data[l] = 4.0 * (h_gf_b.data[l]*(l-m)*(l-m-0.5)-h_gf_b.data[l-1]*(l-m-1)*(l-m-1));
            }
	    h_gf_b.data[0] = 4.0 * (h_gf_b.data[0]*(l-m)*(l-m-0.5));
    }

	int ifact = 1;
	for (k = 1; k < 2*m_order; k++) ifact *= k;
	Scalar gaminv = 1.0/ifact;
	for (l = 0; l < m_order; l++) h_gf_b.data[l] *= gaminv;
    }

Scalar PPPMForceCompute::gf_denom(Scalar x, Scalar y, Scalar z)
    {
    int l ;
    Scalar sx,sy,sz;
    ArrayHandle<Scalar> h_gf_b(m_gf_b, access_location::host, access_mode::readwrite);
    sz = sy = sx = 0.0;
    for (l = m_order-1; l >= 0; l--) {
        sx = h_gf_b.data[l] + sx*x;
        sy = h_gf_b.data[l] + sy*y;
        sz = h_gf_b.data[l] + sz*z;
        }
    Scalar s = sx*sy*sz;
    return s*s;
    }


void PPPMForceCompute::reset_kvec_green_hat_cpu()
    {
	ArrayHandle<Scalar3> h_kvec(m_kvec, access_location::host, access_mode::readwrite);
	const BoxDim& box = m_pdata->getBox();
	Scalar Lx = box.xhi - box.xlo;
	Scalar Ly = box.yhi - box.ylo;
	Scalar Lz = box.zhi - box.zlo;

	Scalar3 inverse_lattice_vector;
	Scalar invdet = 2.0f*M_PI/(Lx*Lz*Lz);
	inverse_lattice_vector.x = invdet*Ly*Lz;
	inverse_lattice_vector.y = invdet*Lx*Lz;
	inverse_lattice_vector.z = invdet*Lx*Ly;

	// Set up the k-vectors
	int ix, iy, iz, kper, lper, mper, k, l, m;
	for (ix = 0; ix < m_Nx; ix++) {
	    Scalar3 j;
	    j.x = ix > m_Nx/2 ? ix - m_Nx : ix;
	    for (iy = 0; iy < m_Ny; iy++) {
            j.y = iy > m_Ny/2 ? iy - m_Ny : iy;
            for (iz = 0; iz < m_Nz; iz++) {
                j.z = iz > m_Nz/2 ? iz - m_Nz : iz;
                h_kvec.data[iz + m_Nz * (iy + m_Ny * ix)].x =  j.x*inverse_lattice_vector.x;
                h_kvec.data[iz + m_Nz * (iy + m_Ny * ix)].y =  j.y*inverse_lattice_vector.y;
                h_kvec.data[iz + m_Nz * (iy + m_Ny * ix)].z =  j.z*inverse_lattice_vector.z;
                }
            }
        }
 
	// Set up constants for virial calculation
	ArrayHandle<Scalar3> h_vg(PPPMData::m_vg, access_location::host, access_mode::readwrite);;
	for(int x = 0; x < m_Nx; x++)
        {
	    for(int y = 0; y < m_Ny; y++)
            {
            for(int z = 0; z < m_Nz; z++)
                {
                Scalar3 kvec = h_kvec.data[z + m_Nz * (y + m_Ny * x)];
                Scalar sqk =  kvec.x*kvec.x;
                sqk += kvec.y*kvec.y;
                sqk += kvec.z*kvec.z;
	
                if (sqk == 0.0) 
                    {
                    h_vg.data[z + m_Nz * (y + m_Ny * x)].x = 0.0f;
                    h_vg.data[z + m_Nz * (y + m_Ny * x)].y = 0.0f;
                    h_vg.data[z + m_Nz * (y + m_Ny * x)].z = 0.0f;
                    }
                else
                    {
                    Scalar vterm = -2.0 * (1.0/sqk + 0.25/(m_kappa*m_kappa));
                    h_vg.data[z + m_Nz * (y + m_Ny * x)].x =  1.0 + vterm*kvec.x*kvec.x;
                    h_vg.data[z + m_Nz * (y + m_Ny * x)].y =  1.0 + vterm*kvec.y*kvec.y;
                    h_vg.data[z + m_Nz * (y + m_Ny * x)].z =  1.0 + vterm*kvec.z*kvec.z;
                    }
                } 
            } 
        }


	// Set up the grid based Green's function
	ArrayHandle<Scalar> h_green_hat(PPPMData::m_green_hat, access_location::host, access_mode::readwrite);
	Scalar snx, sny, snz, snx2, sny2, snz2;
	Scalar argx, argy, argz, wx, wy, wz, sx, sy, sz, qx, qy, qz;
	Scalar sum1, dot1, dot2;
	Scalar numerator, denominator, sqk;

	Scalar unitkx = (2.0*M_PI/Lx);
	Scalar unitky = (2.0*M_PI/Ly);
	Scalar unitkz = (2.0*M_PI/Lz);
   
    
	Scalar xprd = Lx; 
	Scalar yprd = Ly; 
	Scalar zprd_slab = Lz; 
    
	Scalar form = 1.0;

	PPPMForceCompute::compute_gf_denom();

	Scalar temp = floor(((m_kappa*xprd/(M_PI*m_Nx)) * 
                         pow(-log(EPS_HOC),0.25)));
	int nbx = (int)temp;

	temp = floor(((m_kappa*yprd/(M_PI*m_Ny)) * 
                  pow(-log(EPS_HOC),0.25)));
	int nby = (int)temp;

	temp =  floor(((m_kappa*zprd_slab/(M_PI*m_Nz)) * 
                   pow(-log(EPS_HOC),0.25)));
	int nbz = (int)temp;

    
	for (m = 0; m < m_Nz; m++) {
	    mper = m - m_Nz*(2*m/m_Nz);
	    snz = sin(0.5*unitkz*mper*zprd_slab/m_Nz);
	    snz2 = snz*snz;

	    for (l = 0; l < m_Ny; l++) {
            lper = l - m_Ny*(2*l/m_Ny);
            sny = sin(0.5*unitky*lper*yprd/m_Ny);
            sny2 = sny*sny;

            for (k = 0; k < m_Nx; k++) {
                kper = k - m_Nx*(2*k/m_Nx);
                snx = sin(0.5*unitkx*kper*xprd/m_Nx);
                snx2 = snx*snx;
      
                sqk = pow(unitkx*kper,2.0f) + pow(unitky*lper,2.0f) + 
                    pow(unitkz*mper,2.0f);
                if (sqk != 0.0) {
                    numerator = form*12.5663706/sqk;
                    denominator = gf_denom(snx2,sny2,snz2);  

                    sum1 = 0.0;
                    for (ix = -nbx; ix <= nbx; ix++) {
                        qx = unitkx*(kper+(Scalar)(m_Nx*ix));
                        sx = exp(-.25*pow(qx/m_kappa,2.0f));
                        wx = 1.0;
                        argx = 0.5*qx*xprd/(Scalar)m_Nx;
                        if (argx != 0.0) wx = pow(sin(argx)/argx,m_order);
                        for (iy = -nby; iy <= nby; iy++) {
                            qy = unitky*(lper+(Scalar)(m_Ny*iy));
                            sy = exp(-.25*pow(qy/m_kappa,2.0f));
                            wy = 1.0;
                            argy = 0.5*qy*yprd/(Scalar)m_Ny;
                            if (argy != 0.0) wy = pow(sin(argy)/argy,m_order);
                            for (iz = -nbz; iz <= nbz; iz++) {
                                qz = unitkz*(mper+(Scalar)(m_Nz*iz));
                                sz = exp(-.25*pow(qz/m_kappa,2.0f));
                                wz = 1.0;
                                argz = 0.5*qz*zprd_slab/(Scalar)m_Nz;
                                if (argz != 0.0) wz = pow(sin(argz)/argz,m_order);

                                dot1 = unitkx*kper*qx + unitky*lper*qy + unitkz*mper*qz;
                                dot2 = qx*qx+qy*qy+qz*qz;
                                sum1 += (dot1/dot2) * sx*sy*sz * pow(wx*wy*wz,2.0f);
                                }
                            }
                        }
                    h_green_hat.data[m + m_Nz * (l + m_Ny * k)] = numerator*sum1/denominator;
                    } else h_green_hat.data[m + m_Nz * (l + m_Ny * k)] = 0.0;
                }
            }
        }
    }	

void PPPMForceCompute::assign_charges_to_grid()
    {

    const BoxDim& box = m_pdata->getBox();

    Scalar Lx = box.xhi - box.xlo;
    Scalar Ly = box.yhi - box.ylo;
    Scalar Lz = box.zhi - box.zlo;
    
    ParticleDataArraysConst arrays = m_pdata->acquireReadOnly();
    ArrayHandle<Scalar> h_rho_coeff(m_rho_coeff, access_location::host, access_mode::read);
    ArrayHandle<cufftComplex> h_rho_real_space(PPPMData::m_rho_real_space, access_location::host, access_mode::readwrite);

    memset(h_rho_real_space.data, 0.0, sizeof(cufftComplex)*m_Nx*m_Ny*m_Nz);

    for(int i = 0; i < (int)arrays.nparticles; i++)
        {
        Scalar qi = arrays.charge[i];
        Scalar4 posi;
        posi.x = arrays.x[i];
        posi.y = arrays.y[i];
        posi.z = arrays.z[i];

        Scalar box_dx = Lx / ((Scalar)m_Nx);
        Scalar box_dy = Ly / ((Scalar)m_Ny);
        Scalar box_dz = Lz / ((Scalar)m_Nz);
 
        //normalize position to gridsize:
        posi.x += Lx / 2.0f;
        posi.y += Ly / 2.0f;
        posi.z += Lz / 2.0f;
   
        posi.x /= box_dx;
        posi.y /= box_dy;
        posi.z /= box_dz;


        Scalar shift, shiftone, x0, y0, z0, dx, dy, dz;
        int nlower, nupper, mx, my, mz, nxi, nyi, nzi; 
    
        nlower = -(m_order-1)/2;
        nupper = m_order/2;
    
        if (m_order % 2) 
            {
            shift =0.5;
            shiftone = 0.0;
            }
        else 
            {
            shift = 0.0;
            shiftone = 0.5;
            }

        nxi = (int)(posi.x + shift);
        nyi = (int)(posi.y + shift);
        nzi = (int)(posi.z + shift);
 
        dx = shiftone+(Scalar)nxi-posi.x;
        dy = shiftone+(Scalar)nyi-posi.y;
        dz = shiftone+(Scalar)nzi-posi.z;

        int n,m,l,k;
        Scalar result;
        int mult_fact = 2*m_order+1;

        x0 = qi / (box_dx*box_dy*box_dz);
        for (n = nlower; n <= nupper; n++) {
            mx = n+nxi;
            if(mx >= m_Nx) mx -= m_Nx;
            if(mx < 0)  mx += m_Nx;
            result = 0.0f;
            for (k = m_order-1; k >= 0; k--) {
                result = h_rho_coeff.data[n-nlower + k*mult_fact] + result * dx;
                }
            y0 = x0*result;
            for (m = nlower; m <= nupper; m++) {
                my = m+nyi;
                if(my >= m_Ny) my -= m_Ny;
                if(my < 0)  my += m_Ny;
                result = 0.0f;
                for (k = m_order-1; k >= 0; k--) {
                    result = h_rho_coeff.data[m-nlower + k*mult_fact] + result * dy;
                    }
                z0 = y0*result;
                for (l = nlower; l <= nupper; l++) {
                    mz = l+nzi;
                    if(mz >= m_Nz) mz -= m_Nz;
                    if(mz < 0)  mz += m_Nz;
                    result = 0.0f;
                    for (k = m_order-1; k >= 0; k--) {
                        result = h_rho_coeff.data[l-nlower + k*mult_fact] + result * dz;
                        }
                    h_rho_real_space.data[mz + m_Nz * (my + m_Ny * mx)].x += z0*result;
                    }
                }
            }
        }
    }

void PPPMForceCompute::combined_green_e()
    {

    ArrayHandle<Scalar3> h_kvec(m_kvec, access_location::host, access_mode::readwrite);
    ArrayHandle<Scalar> h_green_hat(PPPMData::m_green_hat, access_location::host, access_mode::readwrite);
    ArrayHandle<cufftComplex> h_Ex(m_Ex, access_location::host, access_mode::readwrite);
    ArrayHandle<cufftComplex> h_Ey(m_Ey, access_location::host, access_mode::readwrite);
    ArrayHandle<cufftComplex> h_Ez(m_Ez, access_location::host, access_mode::readwrite);
    ArrayHandle<cufftComplex> h_rho_real_space(PPPMData::m_rho_real_space, access_location::host, access_mode::readwrite);

    unsigned int NNN = m_Nx*m_Ny*m_Nz;
    for(unsigned int i = 0; i < NNN; i++)
        {

        Scalar scale_times_green = h_green_hat.data[i] / ((Scalar)(NNN));
        h_rho_real_space.data[i].x *= scale_times_green;
        h_rho_real_space.data[i].y *= scale_times_green;

        h_Ex.data[i].x = h_kvec.data[i].x * h_rho_real_space.data[i].y;
        h_Ex.data[i].y = -h_kvec.data[i].x * h_rho_real_space.data[i].x;
    
        h_Ey.data[i].x = h_kvec.data[i].y * h_rho_real_space.data[i].y;
        h_Ey.data[i].y = -h_kvec.data[i].y * h_rho_real_space.data[i].x;
    
        h_Ez.data[i].x = h_kvec.data[i].z * h_rho_real_space.data[i].y;
        h_Ez.data[i].y = -h_kvec.data[i].z * h_rho_real_space.data[i].x;
        }
    }

void PPPMForceCompute::calculate_forces()
    {
    const BoxDim& box = m_pdata->getBox();

    Scalar Lx = box.xhi - box.xlo;
    Scalar Ly = box.yhi - box.ylo;
    Scalar Lz = box.zhi - box.zlo;

    ParticleDataArraysConst arrays = m_pdata->acquireReadOnly();

    memset(m_fx, 0, sizeof(Scalar)*arrays.nparticles);
    memset(m_fy, 0, sizeof(Scalar)*arrays.nparticles);
    memset(m_fz, 0, sizeof(Scalar)*arrays.nparticles);
    memset(m_pe, 0, sizeof(Scalar)*arrays.nparticles);
    memset(m_virial, 0, sizeof(Scalar)*arrays.nparticles);

    ArrayHandle<Scalar> h_rho_coeff(m_rho_coeff, access_location::host, access_mode::read);
    ArrayHandle<cufftComplex> h_Ex(m_Ex, access_location::host, access_mode::readwrite);
    ArrayHandle<cufftComplex> h_Ey(m_Ey, access_location::host, access_mode::readwrite);
    ArrayHandle<cufftComplex> h_Ez(m_Ez, access_location::host, access_mode::readwrite);

    for(int i = 0; i < (int)arrays.nparticles; i++)
        {
        Scalar qi = arrays.charge[i];
        Scalar4 posi;
        posi.x = arrays.x[i];
        posi.y = arrays.y[i];
        posi.z = arrays.z[i];

        Scalar box_dx = Lx / ((Scalar)m_Nx);
        Scalar box_dy = Ly / ((Scalar)m_Ny);
        Scalar box_dz = Lz / ((Scalar)m_Nz);
 
        //normalize position to gridsize:
        posi.x += Lx / 2.0f;
        posi.y += Ly / 2.0f;
        posi.z += Lz / 2.0f;
   
        posi.x /= box_dx;
        posi.y /= box_dy;
        posi.z /= box_dz;


        Scalar shift, shiftone, x0, y0, z0, dx, dy, dz;
        int nlower, nupper, mx, my, mz, nxi, nyi, nzi; 
    
        nlower = -(m_order-1)/2;
        nupper = m_order/2;
    
        if (m_order % 2) 
            {
            shift =0.5;
            shiftone = 0.0;
            }
        else 
            {
            shift = 0.0;
            shiftone = 0.5;
            }

        nxi = (int)(posi.x + shift);
        nyi = (int)(posi.y + shift);
        nzi = (int)(posi.z + shift);
 
        dx = shiftone+(Scalar)nxi-posi.x;
        dy = shiftone+(Scalar)nyi-posi.y;
        dz = shiftone+(Scalar)nzi-posi.z;

        int n,m,l,k;
        Scalar result;
        int mult_fact = 2*m_order+1;
        for (n = nlower; n <= nupper; n++) {
            mx = n+nxi;
            if(mx >= m_Nx) mx -= m_Nx;
            if(mx < 0)  mx += m_Nx;
            result = 0.0f;
            for (k = m_order-1; k >= 0; k--) {
                result = h_rho_coeff.data[n-nlower + k*mult_fact] + result * dx;
                }
            x0 = result;
            for (m = nlower; m <= nupper; m++) {
                my = m+nyi;
                if(my >= m_Ny) my -= m_Ny;
                if(my < 0)  my += m_Ny;
                result = 0.0f;
                for (k = m_order-1; k >= 0; k--) {
                    result = h_rho_coeff.data[m-nlower + k*mult_fact] + result * dy;
                    }
                y0 = x0*result;
                for (l = nlower; l <= nupper; l++) {
                    mz = l+nzi;
                    if(mz >= m_Nz) mz -= m_Nz;
                    if(mz < 0)  mz += m_Nz;
                    result = 0.0f;
                    for (k = m_order-1; k >= 0; k--) {
                        result = h_rho_coeff.data[l-nlower + k*mult_fact] + result * dz;
                        }
                    z0 = y0*result;
                    Scalar local_field_x = h_Ex.data[mz + m_Nz * (my + m_Ny * mx)].x;
                    Scalar local_field_y = h_Ey.data[mz + m_Nz * (my + m_Ny * mx)].x;
                    Scalar local_field_z = h_Ez.data[mz + m_Nz * (my + m_Ny * mx)].x;
                    m_fx[i] += qi*z0*local_field_x;
                    m_fy[i] += qi*z0*local_field_y;
                    m_fz[i] += qi*z0*local_field_z;
                    }
                }
            }
        }
    }

void export_PPPMForceCompute()
    {
    class_<PPPMForceCompute, boost::shared_ptr<PPPMForceCompute>, bases<ForceCompute>, boost::noncopyable >
        ("PPPMForceCompute", init< boost::shared_ptr<SystemDefinition>, boost::shared_ptr<NeighborList> >())
        .def("setParams", &PPPMForceCompute::setParams)
        ;
    }


#ifdef WIN32
#pragma warning( pop )
#endif

