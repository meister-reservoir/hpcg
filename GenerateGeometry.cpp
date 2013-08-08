//@HEADER
// ************************************************************************
//
//               HPCG: Simple Conjugate Gradient Benchmark Code
// Questions? Contact Michael A. Heroux (maherou@sandia.gov)
//
// ************************************************************************
//@HEADER

// Input
// size - Number of MPI processes
// rank - My process id
// nx, ny, nz - Number of grid points for each local block in the x, y, z dimensions, resp.
//
// Output
// npx, npy, npz - Factoring of np into 3D cubed
// ipx, ipy, ipz - x, y, z coordinate of this process in the npx by npy by npz processor cube.

#include <cmath>
#include <cstdlib>

#include "GenerateGeometry.hpp"
#ifdef DEBUG
#include <iostream>
using std::cout;
using std::cerr;
using std::endl;

#include <cassert>
#endif

typedef struct Counter_s {
  int length;
  int max_counts[32+1];
  int cur_counts[32+1];
} Counter;

static void
Counter_new(Counter *this_, int *counts, int length) {
  int i;

  this_->length = length;

  for (i = 0; i < 32; ++i) {
    this_->max_counts[i] = counts[i];
    this_->cur_counts[i] = 0;
  }
  /* terminate with 0's */
  this_->max_counts[i] = this_->cur_counts[i] = 0;
  this_->max_counts[length] = this_->cur_counts[length] = 0;
}

static void
Counter_next(Counter *this_) {
  int i;

  for (i = 0; i < this_->length; ++i) {
    this_->cur_counts[i]++;
    if (this_->cur_counts[i] > this_->max_counts[i]) {
      this_->cur_counts[i] = 0;
      continue;
    }
    break;
  }
}

static int
Counter_is_zero(Counter *this_) {
  int i;
  for (i = 0; i < this_->length; ++i)
    if (this_->cur_counts[i]) return 0;
  return 1;
}

static int
Counter_product(Counter *this_, int *multipliers) {
  int i, j, k=0, x=1;

  for (i = 0; i < this_->length; ++i)
    for (j = 0; j < this_->cur_counts[i]; ++j) {
      k = 1;
      x *= multipliers[i];
    }

  return x * k;
}

static void
Counter_max_cur_sub(Counter *this_, Counter *that, Counter *res) {
  int i;

  res->length = this_->length;
  for (i = 0; i < this_->length; ++i) {
    res->max_counts[i] = this_->max_counts[i] - that->cur_counts[i];
    res->cur_counts[i] = 0;
  }
}

static void
primefactor_i(int x, int *factors) {
  int i, d, sq=(int)(sqrt(x))+1L;
  div_t r;

  /* remove 2 as a factor with shifts */
  for (i = 0; x > 1 && (x & 1) == 0; x >>= 1) {
    factors[i++] = 2;
  }

  /* keep removing subsequent odd numbers */
  for (d = 3; d <= sq; d += 2) {
    while (1) {
      r = div(x, d);
      if (r.rem == 0) {
        factors[i++] = d;
        x = r.quot;
        continue;
      }
      break;
    }
  }
  if (x > 1 || i == 0)  /* left with a prime or x==1 */
    factors[i++] = x;

  factors[i] = 0; /* terminate with 0 */
}
static void
gen_min_area3(int n, int *f1, int *f2, int *f3) {
  int i, j, df_cnt;
  int tf1, tf2, tf3;
  int factors[32+1], distinct_factors[32+1], count_factors[32+1];
  Counter c_main, c1, c2;

  /* at the beginning, minimum area is the maximum area */
  double area, min_area = 2.0 * n + 1.0;

  primefactor_i( n, factors ); /* factors are sorted: ascending order */

  if (1 == n || factors[1] == 0) { /* prime number */
    *f1 = n;
    *f2 = 1;
    *f3 = 1;
    return;
  } else if (factors[2] == 0) { /* two prime factors */
    *f1 = factors[0];
    *f2 = factors[1];
    *f3 = 1;
    return;
  } else if (factors[3] == 0) { /* three prime factors */
    *f1 = factors[0];
    *f2 = factors[1];
    *f3 = factors[2];
    return;
  }

  /* we have more than 3 prime factors so we need to try all possible combinations */

  for (j = 0, i = 0; factors[i];) {
    distinct_factors[j++] = factors[i];
    count_factors[j-1] = 0;
    do {
      count_factors[j-1]++;
    } while (distinct_factors[j-1] == factors[++i]);
  }
  df_cnt = j;

  Counter_new( &c_main, count_factors, df_cnt );

  Counter_new( &c1, count_factors, df_cnt );

  for (Counter_next( &c1 ); ! Counter_is_zero( &c1 ); Counter_next( &c1 )) {

    Counter_max_cur_sub( &c_main, &c1, &c2 );
    for (Counter_next( &c2 ); ! Counter_is_zero( &c2 ); Counter_next( &c2 )) {
      tf1 = Counter_product( &c1, distinct_factors );
      tf2 = Counter_product( &c2, distinct_factors );
      tf3 = n / tf1/ tf2;

      area = tf1 * (double)tf2 + tf2 * (double)tf3 + tf1 * (double)tf3;
      if (area < min_area) {
        min_area = area;
        *f1 = tf1;
        *f2 = tf2;
        *f3 = tf3;
      }
    }
  }
}

void GenerateGeometry(int size, int rank, int nx, int ny, int nz, Geometry & geom) {

	int npx, npy, npz;

	gen_min_area3( size, &npx, &npy, &npz );

	// Now compute this process's indices in the 3D cube
	int ipz = rank/(npx*npy);
	int ipy = (rank-ipz*npx*npy)/npx;
	int ipx = rank%npx;

#ifdef DEBUG
	if (rank==0)
	cout 	<< "size = "<< size << endl
			<< "nx  = " << nx << endl
			<< "ny  = " << ny << endl
			<< "nz  = " << nz << endl
			<< "npx = " << npx << endl
			<< "npy = " << npy << endl
			<< "npz = " << npz << endl;

	cout    << "For rank = " << rank << endl
			<< "ipx = " << ipx << endl
			<< "ipy = " << ipy << endl
			<< "ipz = " << ipz << endl;

	assert(size==npx*npy*npz);
#endif
	geom.size = size;
	geom.rank = rank;
	geom.nx = nx;
	geom.ny = ny;
	geom.nz = nz;
	geom.npx = npx;
	geom.npy = npy;
	geom.npz = npz;
	geom.ipx = ipx;
	geom.ipy = ipy;
	geom.ipz = ipz;
	return;
}
