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

// $Id$
// $URL$
// Maintainer: joaander

#include "CellListGPU.cuh"

/*! \file CellListGPU.cu
    \brief Defines GPU kernel code for cell list generation on the GPU
*/

//! Kernel that computes the cell list on the GPU
/*! \param d_cell_size Number of particles in each cell
    \param d_xyzf Cell XYZF data array
    \param d_tdb Cell TDB data array
    \param d_conditions Conditions flags for detecting overflow and other error conditions
    \param d_pos Particle position array
    \param d_charge Particle charge array
    \param d_diameter Particle diameter array
    \param d_body Particle body array
    \param N Number of particles
    \param Nmax Maximum number of particles that can be placed in a single cell
    \param flag_charge Set to true to store chage in the flag position in \a d_xyzf
    \param scale Multipler to convert from particle coordinates to grid coordinates
    \param box Box dimensions
    \param ci Indexer to compute cell id from cell grid coords
    \param cli Indexer to index into \a d_xyzf and \a d_tdb
    
    \note Optimized for Fermi
*/
__global__ void gpu_compute_cell_list_kernel(unsigned int *d_cell_size,
                                             float4 *d_xyzf,
                                             float4 *d_tdb,
                                             unsigned int *d_conditions,
                                             const float4 *d_pos,
                                             const float *d_charge,
                                             const float *d_diameter,
                                             const unsigned int *d_body,
                                             const unsigned int N,
                                             const unsigned int Nmax,
                                             const bool flag_charge,
                                             const Scalar3 scale,
                                             const gpu_boxsize box,
                                             const Index3D ci,
                                             const Index2D cli)
    {
    // read in the particle that belongs to this thread
    unsigned int idx = blockDim.x * blockIdx.x + threadIdx.x;
    if (idx >= N)
        return;
        
    float4 pos = d_pos[idx];
    float flag = 0.0f;
    float diameter = 0.0f;
    float body = 0;
    float type = 0;
    if (d_tdb != NULL)
        {
        diameter = d_diameter[idx];
        body = __int_as_float(d_body[idx]);
        type = pos.w;
        }
        
    if (flag_charge)
        flag = d_charge[idx];
    else
        flag = __int_as_float(idx);

    // check for nan pos
    if (isnan(pos.x) || isnan(pos.y) || isnan(pos.z))
        {
        d_conditions[1] = idx+1;
        return;
        }
    
    // determine which bin it belongs in
    unsigned int ib = (unsigned int)((pos.x+box.Lx/2.0f)*scale.x);
    unsigned int jb = (unsigned int)((pos.y+box.Ly/2.0f)*scale.y);
    unsigned int kb = (unsigned int)((pos.z+box.Lz/2.0f)*scale.z);
    
    // need to handle the case where the particle is exactly at the box hi
    if (ib == ci.getW())
        ib = 0;
    if (jb == ci.getH())
        jb = 0;
    if (kb == ci.getD())
        kb = 0;
        
    unsigned int bin = ci(ib, jb, kb);

    // check if the particle is inside the dimensions
    if (bin >= ci.getNumElements())
        {
        d_conditions[2] = idx+1;
        return;
        }
    
    unsigned int size = atomicInc(&d_cell_size[bin], 0xffffffff);
    if (size < Nmax)
        {
        unsigned int write_pos = cli(size, bin);
        d_xyzf[write_pos] = make_float4(pos.x, pos.y, pos.z, flag);
        if (d_tdb != NULL)
            d_tdb[write_pos] = make_float4(type, diameter, body, 0.0f);
        }
    else
        {
        // handle overflow
        atomicMax(&d_conditions[0], size+1);
        }
    }

cudaError_t gpu_compute_cell_list(unsigned int *d_cell_size,
                                  float4 *d_xyzf,
                                  float4 *d_tdb,
                                  unsigned int *d_conditions,
                                  const float4 *d_pos,
                                  const float *d_charge,
                                  const float *d_diameter,
                                  const unsigned int *d_body,
                                  const unsigned int N,
                                  const unsigned int Nmax,
                                  const bool flag_charge,
                                  const Scalar3& scale,
                                  const gpu_boxsize& box,
                                  const Index3D& ci,
                                  const Index2D& cli)
    {
    unsigned int block_size = 256;
    int n_blocks = (int)ceil(float(N)/(float)block_size);
    
    cudaError_t err;
    err = cudaMemset(d_cell_size, 0, sizeof(unsigned int)*ci.getNumElements());
    
    if (err != cudaSuccess)
        return err;
    
    gpu_compute_cell_list_kernel<<<n_blocks, block_size>>>(d_cell_size,
                                                           d_xyzf,
                                                           d_tdb,
                                                           d_conditions,
                                                           d_pos,
                                                           d_charge,
                                                           d_diameter,
                                                           d_body,
                                                           N,
                                                           Nmax,
                                                           flag_charge,
                                                           scale,
                                                           box,
                                                           ci,
                                                           cli);
    
    return cudaSuccess;
    }

// ********************* Following are helper functions, structs, etc for the 1x optimized cell list build
//! \internal
/*! \param a First element
    \param b Second element
    The two elements are swapped
*/
template<class T> __device__ inline void swap(T & a, T & b)
    {
    T tmp = a;
    a = b;
    b = tmp;
    }

//! \internal
/*! \param shared Pointer to shared memory to bitonic sort
*/
template<class T, unsigned int block_size> __device__ inline void bitonic_sort(T *shared)
    {
    unsigned int tid = threadIdx.x;
    
    // Parallel bitonic sort.
    for (int k = 2; k <= block_size; k *= 2)
        {
        // Bitonic merge:
        for (int j = k / 2; j>0; j /= 2)
            {
            int ixj = tid ^ j;
            
            if (ixj > tid)
                {
                if ((tid & k) == 0)
                    {
                    if (shared[tid] > shared[ixj])
                        {
                        swap(shared[tid], shared[ixj]);
                        }
                    }
                else
                    {
                    if (shared[tid] < shared[ixj])
                        {
                        swap(shared[tid], shared[ixj]);
                        }
                    }
                }
                
            __syncthreads();
            }
        }
    }

//! \internal
struct bin_id_pair
    {
    unsigned int bin;   //!< Cell index
    unsigned int id;    //!< Particle id
    unsigned int start_offset;  //!< Write offset
    };

//! \internal
/*! \param bin Cell index
    \param id Particle id
*/
__device__ inline bin_id_pair make_bin_id_pair(unsigned int bin, unsigned int id)
    {
    bin_id_pair res;
    res.bin = bin;
    res.id = id;
    res.start_offset = 0;
    return res;
    }

//! \internal
/*! \param a First element
    \param b Second element
*/
__device__ inline bool operator< (const bin_id_pair& a, const bin_id_pair& b)
    {
    if (a.bin == b.bin)
        return (a.id < b.id);
    else
        return (a.bin < b.bin);
    }

//! \internal
/*! \param a First element
    \param b Second element
*/
__device__ inline bool operator> (const bin_id_pair& a, const bin_id_pair& b)
    {
    if (a.bin == b.bin)
        return (a.id > b.id);
    else
        return (a.bin > b.bin);
    }

//! \internal
/*! \param temp Temporary array in shared memory to scan
*/
template<class T, unsigned int block_size> __device__ inline void scan_naive(T *temp)
    {
    int thid = threadIdx.x;
    
    int pout = 0;
    int pin = 1;
    
    for (int offset = 1; offset < block_size; offset *= 2)
        {
        pout = 1 - pout;
        pin  = 1 - pout;
        __syncthreads();
        
        temp[pout*block_size+thid] = temp[pin*block_size+thid];
        
        if (thid >= offset)
            temp[pout*block_size+thid] += temp[pin*block_size+thid - offset];
        }
        
    __syncthreads();
    // bring the data back to the initial array
    if (pout == 1)
        {
        pout = 1 - pout;
        pin  = 1 - pout;
        temp[pout*block_size+thid] = temp[pin*block_size+thid];
        __syncthreads();
        }
    }

//! Kernel that computes the cell list on the GPU
/*! \param d_cell_size Number of particles in each cell
    \param d_xyzf Cell XYZF data array
    \param d_tdb Cell TDB data array
    \param d_conditions Conditions flags for detecting overflow and other error conditions
    \param d_pos Particle position array
    \param d_charge Particle charge array
    \param d_diameter Particle diameter array
    \param d_body Particle body array
    \param N Number of particles
    \param Nmax Maximum number of particles that can be placed in a single cell
    \param flag_charge Set to true to store chage in the flag position in \a d_xyzf
    \param scale Multipler to convert from particle coordinates to grid coordinates
    \param box Box dimensions
    \param ci Indexer to compute cell id from cell grid coords
    \param cli Indexer to index into \a d_xyzf and \a d_tdb
    
    \note Optimized for compute 1.x hardware
*/
template<unsigned int block_size>
__global__ void gpu_compute_cell_list_1x_kernel(unsigned int *d_cell_size,
                                                float4 *d_xyzf,
                                                float4 *d_tdb,
                                                unsigned int *d_conditions,
                                                const float4 *d_pos,
                                                const float *d_charge,
                                                const float *d_diameter,
                                                const unsigned int *d_body,
                                                const unsigned int N,
                                                const unsigned int Nmax,
                                                const bool flag_charge,
                                                const Scalar3 scale,
                                                const gpu_boxsize box,
                                                const Index3D ci,
                                                const Index2D cli)
    {
    // sentinel to label a bin as invalid
    const unsigned int INVALID_BIN = 0xffffffff;

    // read in the particle that belongs to this thread
    unsigned int idx = blockDim.x * blockIdx.x + threadIdx.x;
        
    float4 pos = make_float4(0.0f, 0.0f, 0.0f, 0.0f);
    if (idx < N)
        pos = d_pos[idx];

    // determine which bin it belongs in
    unsigned int ib = (unsigned int)((pos.x+box.Lx/2.0f)*scale.x);
    unsigned int jb = (unsigned int)((pos.y+box.Ly/2.0f)*scale.y);
    unsigned int kb = (unsigned int)((pos.z+box.Lz/2.0f)*scale.z);
    
    // need to handle the case where the particle is exactly at the box hi
    if (ib == ci.getW())
        ib = 0;
    if (jb == ci.getH())
        jb = 0;
    if (kb == ci.getD())
        kb = 0;
        
    unsigned int bin = ci(ib, jb, kb);

    // check if the particle is inside the dimensions
    if (bin >= ci.getNumElements())
        {
        d_conditions[2] = idx+1;
        bin = INVALID_BIN;
        }
    // check for nan pos
    if (isnan(pos.x) || isnan(pos.y) || isnan(pos.z))
        {
        d_conditions[1] = idx+1;
        bin = INVALID_BIN;
        }
    // if we are past the end of the array, mark the bin as invalid
    if (idx >= N)
        bin = INVALID_BIN;


    // now, perform the fun sorting and bin entry
    // load up shared memory
    __shared__ bin_id_pair bin_pairs[block_size];
    bin_pairs[threadIdx.x] = make_bin_id_pair(bin, idx);
    __syncthreads();
    
    // sort it
    bitonic_sort<bin_id_pair, block_size>(bin_pairs);
    
    // identify the breaking points
    __shared__ unsigned int unique[block_size*2+1];
    
    bool is_unique = false;
    if (threadIdx.x > 0 && bin_pairs[threadIdx.x].bin != bin_pairs[threadIdx.x-1].bin)
        is_unique = true;
        
    unique[threadIdx.x] = 0;
    if (is_unique)
        unique[threadIdx.x] = 1;
        
    // threadIdx.x = 0 is unique: but we don't want to count it in the scan
    if (threadIdx.x == 0)
        is_unique = true;
        
    __syncthreads();
    
    // scan to find addresses to write to
    scan_naive<unsigned int, block_size>(unique);
    
    // determine start location of each unique value in the array
    // save shared memory by reusing the temp data in the unique[] array
    unsigned int *start = &unique[block_size];
    
    if (is_unique)
        start[unique[threadIdx.x]] = threadIdx.x;
        
    // boundary condition: need one past the end
    if (threadIdx.x == 0)
        start[unique[block_size-1]+1] = block_size;
        
    __syncthreads();
    
    bool is_valid = (bin_pairs[threadIdx.x].bin < ci.getNumElements());
    
    // now: each unique start point does it's own atomicAdd to find the starting offset
    // the is_valid check is to prevent writing to out of bounds memory at the tail end of the array
    if (is_unique && is_valid)
        bin_pairs[unique[threadIdx.x]].start_offset = atomicAdd(&d_cell_size[bin_pairs[threadIdx.x].bin], start[unique[threadIdx.x]+1] - start[unique[threadIdx.x]]);
        
    __syncthreads();
    
    // finally! we can write out all the particles
    // the is_valid check is to prevent writing to out of bounds memory at the tail end of the array
    unsigned int offset = bin_pairs[unique[threadIdx.x]].start_offset;
    unsigned int size = offset + threadIdx.x - start[unique[threadIdx.x]];
    if (size < Nmax)
        {
        if (is_valid)
            {
            unsigned int write_id = bin_pairs[threadIdx.x].id;
            unsigned int write_location = cli(size, bin_pairs[threadIdx.x].bin);
            
            float4 write_pos = d_pos[write_id];
            float flag = 0.0f;
            float diameter = 0.0f;
            float body = 0;
            float type = 0;
            if (d_tdb != NULL)
                {
                diameter = d_diameter[write_id];
                body = __int_as_float(d_body[write_id]);
                type = write_pos.w;
                }
                
            if (flag_charge)
                flag = d_charge[write_id];
            else
                flag = __int_as_float(write_id);
            
            d_xyzf[write_location] = make_float4(write_pos.x, write_pos.y, write_pos.z, flag);
            if (d_tdb != NULL)
                d_tdb[write_location] = make_float4(type, diameter, body, 0.0f);
            }
        }
    else
        {
        // handle overflow
        atomicMax(&d_conditions[0], size+1);
        }
    }

cudaError_t gpu_compute_cell_list_1x(unsigned int *d_cell_size,
                                     float4 *d_xyzf,
                                     float4 *d_tdb,
                                     unsigned int *d_conditions,
                                     const float4 *d_pos,
                                     const float *d_charge,
                                     const float *d_diameter,
                                     const unsigned int *d_body,
                                     const unsigned int N,
                                     const unsigned int Nmax,
                                     const bool flag_charge,
                                     const Scalar3& scale,
                                     const gpu_boxsize& box,
                                     const Index3D& ci,
                                     const Index2D& cli)
    {
    const unsigned int block_size = 64;
    int n_blocks = (int)ceil(float(N)/(float)block_size);
    
    cudaError_t err;
    err = cudaMemset(d_cell_size, 0, sizeof(unsigned int)*ci.getNumElements());
    
    if (err != cudaSuccess)
        return err;
    
    gpu_compute_cell_list_1x_kernel<block_size>
                                   <<<n_blocks, block_size>>>(d_cell_size,
                                                              d_xyzf,
                                                              d_tdb,
                                                              d_conditions,
                                                              d_pos,
                                                              d_charge,
                                                              d_diameter,
                                                              d_body,
                                                              N,
                                                              Nmax,
                                                              flag_charge,
                                                              scale,
                                                              box,
                                                              ci,
                                                              cli);
    
    return cudaSuccess;
    }
