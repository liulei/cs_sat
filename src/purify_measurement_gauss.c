/*! 
 * \file purify_measurement.c
 * Functionality to define measurement operators.
 */

#include <stdio.h>
#include <stdlib.h>
#include <math.h> 
#include <complex.h>  // Must be before fftw3.h
#include <fftw3.h>
#ifdef __APPLE__
  #include <Accelerate/Accelerate.h>
#elif __unix__
  #include <cblas.h>
#else
  #include <cblas.h>
#endif 
#include "purify_image.h"
#include "purify_sparsemat.h"
#include "purify_visibility.h"
#include "purify_error.h"
#include "purify_types.h"
#include "purify_utils.h" 
#include "purify_measurement.h" 
#include "purify_ran.h"  

#define NGCF 301

/*!
 * Compute forward Fouier transform of real signal.  A real-to-complex
 * FFT is used (for speed optimisation) but the complex output signal
 * is filled to its full size through conjugate symmetry.
 * 
 * \param[out] out (complex double*) Forward Fourier transform of input signal.
 * \param[in] in (double*) Real input signal.
 * \param[in] data 
 * - data[0] (fftw_plan*): The real-to-complex FFTW plan to use when
 *      computing the Fourier transform (passed as an input so that the
 *      FFTW can be FFTW_MEASUREd beforehand).
 * - data[1] (purify_image*): The image defining the size of the Fourier
 *      transform.
 *
 * \authors <a href="http://www.jasonmcewen.org">Jason McEwen</a>
 */
void purify_measurement_fft_real(void *out, void *in, 
				 void **data) {

  fftw_plan *plan;
  int iu, iv, ind, ind_half;
  int iu_neg, iv_neg, ind_neg;
  double complex *y, *y_half;
  purify_image *img;

  // Cast intput pointers.
  y = (double complex*)out;
  plan = (fftw_plan*)data[0];
  img = (purify_image*)data[1];

  // Allocate space for output of real-to-complex FFT before compute
  // full plane through conjugate symmetry.
  y_half = (complex double*)malloc(img->nx*img->ny*sizeof(complex double));
  PURIFY_ERROR_MEM_ALLOC_CHECK(y_half);

  // Perform real-to-complex FFT.
  fftw_execute_dft_r2c(*plan, 
		       (double*)in, 
		       y_half);

  // Compute other half of complex plane through conjugate symmetry.
  for (iu = 0; iu < img->nx; iu++) {
    for (iv = 0; iv < img->ny/2+1; iv++) {

      ind_half = iu*(img->ny/2+1) + iv;
      purify_visibility_iuiv2ind(&ind, iu, iv, 
				 img->nx, img->ny);

      // Copy current data element.
      y[ind] = y_half[ind_half];

      // Compute reflected data through conjugate symmetry if
      // necessary.
      if (iu == 0 && iv == 0) {
	// Do nothing for DC component.
      } 
      else if (iu == 0) {
	// Reflect along line iu = 0.
	iv_neg = img->ny - iv;
	purify_visibility_iuiv2ind(&ind_neg, iu, iv_neg, 
				 img->nx, img->ny);
	if (ind != ind_neg) y[ind_neg] = conj(y_half[ind_half]);
      }
      else if (iv == 0) {
	// Reflect along line iu = 0.
	iu_neg = img->nx - iu;
	purify_visibility_iuiv2ind(&ind_neg, iu_neg, iv, 
				 img->nx, img->ny);
	if (ind != ind_neg) y[ind_neg] = conj(y_half[ind_half]);
      }
      else {
	// Reflect along diagonal.
	iv_neg = img->ny - iv;
	iu_neg = img->nx - iu;
	purify_visibility_iuiv2ind(&ind_neg, iu_neg, iv_neg, 
				 img->nx, img->ny);
	if (ind != ind_neg) y[ind_neg] = conj(y_half[ind_half]);
      }
    }
  }
  
  // Free temporary memory.
  free(y_half);

}


/*!
 * Compute forward Fouier transform of complex signal.
 *
 * \param[out] out (complex double*) Forward Fourier transform of input signal.
 * \param[in] in (complex double*) Complex input signal.
 * \param[in] data 
 * - data[0] (fftw_plan*): The real-to-complex FFTW plan to use when
 *      computing the Fourier transform (passed as an input so that the
 *      FFTW can be FFTW_MEASUREd beforehand).
 *
 * \authors <a href="http://www.jasonmcewen.org">Jason McEwen</a>
 */
void purify_measurement_fft_complex(void *out, void *in, 
				    void **data) {

  fftw_plan *plan;

  plan = (fftw_plan*)data[0];
  fftw_execute_dft(*plan, 
		   (complex double*)in, 
		   (complex double*)out);

}


/*!
 * Forward visibility masking operator.
 * 
 * \param[out] out (complex double*) Output vector of masked visibilities.
 * \param[in] in (complex double*) Input vector of full visibilities to mask.
 * \param[in] data 
 * - data[0] (purify_sparsemat*): The sparse matrix defining the
 *      masking operator.
 *
 * \authors <a href="http://www.jasonmcewen.org">Jason McEwen</a>
 */
void purify_measurement_mask_opfwd(void *out, 
				  void *in, 
				  void **data) {

  purify_sparsemat_fwd_complex((complex double*)out, 
			       (complex double*)in,
			       (purify_sparsemat*)data[0]);

}


/*!
 * Adjoint visibility masking operator.
 * 
 * \param[out] out (complex double*) Output vector of full
 * visibilities after adjoint of masking.
 * \param[in] in (complex double*) Input vector of masked visibilities
 * prior to adjoint of masking.
 * \param[in] data 
 * - data[0] (purify_sparsemat*): The sparse matrix defining the
 *      masking operator.
 *
 * \authors <a href="http://www.jasonmcewen.org">Jason McEwen</a>
 */
void purify_measurement_mask_opadj(void *out, 
				 void *in, 
				 void **data) {

  purify_sparsemat_adj_complex((complex double*)out, 
			       (complex double*)in,
			       (purify_sparsemat*)data[0]);

}


/*!
 * Define measurement operator (currently includes Fourier transform
 * and masking only).
 *
 * \param[out] out (complex double*) Measured visibilities.
 * \param[in] in (double*) Real input image.
 * \param[in] data 
 * - data[0] (fftw_plan*): The real-to-complex FFTW plan to use when
 *      computing the Fourier transform (passed as an input so that the
 *      FFTW can be FFTW_MEASUREd beforehand).
 * - data[1] (purify_image*): The image defining the size of the Fourier
 *      transform.
 * - data[2] (purify_sparsemat*): The sparse matrix defining the
 *      masking operator.
 *
 * \authors <a href="http://www.jasonmcewen.org">Jason McEwen</a>
 */
void purify_measurement_opfwd(void *out, void *in, void **data) {

  fftw_plan *plan;
  purify_image *img;
  purify_sparsemat *mask;

  void *data_fft[2];
  void *data_mask[1];  
  double complex* vis_full;

  plan = (fftw_plan *)data[0];
  img = (purify_image *)data[1];
  mask = (purify_sparsemat*)data[2];

  vis_full = malloc(img->nx * img->ny * sizeof(double complex));
  PURIFY_ERROR_MEM_ALLOC_CHECK(vis_full);

  data_fft[0] = (void *)plan;
  data_fft[1] = (void *)img;
  purify_measurement_fft_real((void *)vis_full, in, data_fft);

  data_mask[0] = (void *)mask;
  purify_measurement_mask_opfwd(out, (void *)vis_full, data_mask);

  free(vis_full);

}

/*!
 * Initialization for the continuos Fourier transform operator.
 * 
 * \param[out] mat (purify_sparsemat_row*) Sparse matrix containing
 * the interpolation kernels for each visibility. The matrix is 
 * stored in compressed row storage format.
 * \param[out] deconv (double*) Deconvolution kernel in real space
 * \param[in] u (double*) u coodinates between -pi and pi
 * \param[in] v (double*) v coodinates between -pi and pi
 * \param[in] param structure storing information for the operator
 *
 * \authors Rafael Carrillo
 */
void purify_measurement_init_cft(purify_sparsemat_row *mat, 
                                 double *deconv, double *u, double *v, 
                                 purify_measurement_cparam *param) {

    int i, j;
    int nx2, ny2;
    int row, numel;
    double uinc, vinc;

// initialize gauss kernel, according to difmap
    double convfn[NGCF];
    int nmask = 2;
    double hwhm = 0.7;
    double tgtocg = (NGCF - 1) / (nmask + 0.5);
    double cghwhm = tgtocg * hwhm;
    double recvar = log(2.0) / cghwhm / cghwhm;
    FILE *fc = fopen("convdump.dat", "w");
    for(i = 0; i < NGCF; ++i){
        convfn[i] = exp(-recvar * i * i);
        fprintf(fc, "%d\t%.3e\n", i, convfn[i]);
    }
    fclose(fc);

    //Sparse matrix initialization
    nx2 = param->ofx*param->nx1;
    ny2 = param->ofy*param->ny1;

    mat->nrows = param->nmeas;
    mat->ncols = nx2*ny2;
//    mat->nvals = param->kx*param->ky*param->nmeas;
    mat->nvals = param->nmeas * (2 * nmask + 1) * (2 * nmask + 1);
    mat->real = 1;
    mat->cvals = NULL;
//    numel = param->kx*param->ky;
    numel = (2 * nmask + 1) * (2 * nmask + 1);
    
 
    mat->vals = (double*)malloc(mat->nvals * sizeof(double));
    PURIFY_ERROR_MEM_ALLOC_CHECK(mat->vals);
    mat->colind = (int*)malloc(mat->nvals * sizeof(int));
    PURIFY_ERROR_MEM_ALLOC_CHECK(mat->colind);
    mat->rowptr = (int*)malloc((mat->nrows + 1) * sizeof(int));
    PURIFY_ERROR_MEM_ALLOC_CHECK(mat->rowptr);

    uinc = param->umax / (nx2 / 2);
    vinc = param->vmax / (ny2 / 2);

//    sigmax = 1.0 / (double)param->nx1;
//    sigmay = 1.0 / (double)param->ny1;

//    uinc = 4.0 * M_PI / nx2;
//    vinc = 4.0 * M_PI / ny2;

// Row pointer vector
    for (j = 0; j < mat->nrows + 1; j++){
        mat->rowptr[j] = j*numel;
    }

    int idu, idv, iv, iu, iv2, iu2, counter; 
    double ufrc, vfrc;
    double fv, fu;
  //Main loop
    for (i=0; i < param->nmeas; i++){

        idu = floor(u[i] / uinc + 0.5);
        idv = floor(v[i] / vinc + 0.5);
        vfrc = v[i] / vinc;
        ufrc = u[i] / uinc;
        row = i * numel;

        counter = 0;
        for(iv = idv - nmask; iv <= idv + nmask; ++iv){
            fv = convfn[(int) (tgtocg * fabs(iv - vfrc) + 0.5)];
            for(iu = idu - nmask; iu <= idu + nmask; ++iu){
                fu = convfn[(int) (tgtocg * fabs(iu - ufrc) + 0.5)];
                
                iu2 = iu; iv2 = iv;
                if(iu2 < 0) iu2 += nx2;
                if(iu2 >= nx2) iu2 -= nx2;

                if(iv2 < 0) iv2 += ny2;
                if(iv2 >= ny2) iv2 -= ny2;

                mat->vals[row + counter] = fv * fu;
//                mat->vals[row + counter] = 1.0;
                mat->colind[row + counter] = iv2 * nx2 + iu2;

                counter++;
            } // for iu
        } // for iv
    } // for nmeas

    for(i = 0; i < param->nx1 * param->ny1; ++i){
        deconv[i] = 1.0;
    }
}

/*!
 * Define measurement operator for continuos visibilities
 * (currently includes continuos Fourier transform only).
 *
 * \param[out] out (complex double*) Measured visibilities.
 * \param[in] in (complex double*) Input image.
 * \param[in] data 
 * - data[0] (purify_measurement_cparam*): Parameters for the continuos
 *            Fourier transform.
 * - data[1] (double*): Matrix with the deconvolution kernel in image
 *            space.
 * - data[2] (purify_sparsemat_row*): The sparse matrix defining the
 *            convolution operator for the the interpolation.
 * - data[3] (fftw_plan*): The complex-to-complex FFTW plan to use when
 *      computing the Fourier transform (passed as an input so that the
 *      FFTW can be FFTW_MEASUREd beforehand).
 * - data[4] (complex double*) Temporal memory for the zero padding.
 *
 * \authors Rafael Carrillo
 */

void purify_measurement_cftfwd(void *out, void *in, void **data){

  int i, j, nx2, ny2;
  int st1, st2;
  double scale;
  purify_measurement_cparam *param;
  double *deconv;
  purify_sparsemat_row *mat;
  fftw_plan *plan;
  complex double *temp;
  complex double *xin;
  complex double *yout;
  complex double alpha;

  //Cast input pointers
  param = (purify_measurement_cparam*)data[0];
  deconv = (double*)data[1];
  mat = (purify_sparsemat_row*)data[2];
  plan = (fftw_plan*)data[3];
  temp = (complex double*)data[4];

  xin = (complex double*)in;
  yout = (complex double*)out;

  nx2 = param->ofx*param->nx1;
  ny2 = param->ofy*param->ny1;
  
  alpha = 0.0 + 0.0*I;
  //Zero padding and decovoluntion. 
  //Original image in the center.
  for (i=0; i < nx2*ny2; i++){
    *(temp + i) = alpha;
  }

  //Scaling
  scale = 1/sqrt((double)(nx2*ny2));

  int npadx = nx2 / 4;
  int npady = ny2 / 4;

  for (j=0; j < param->ny1; j++){
    st1 = j * param->nx1;
    st2 = (j + npady) * nx2;
    for (i=0; i < param->nx1; i++){
//      *(temp + st2 + i) = *(xin + st1 + i)**(deconv + st1 + i)*scale;
        *(temp + st2 + i + npadx) = *(xin + st1 + i) * scale;
        *(temp + st2 + i + npadx) *= *(deconv + st1 + i);
    }
  }

  purify_utils_fftshift_2d_c(temp, nx2, ny2);

  //FFT
  fftw_execute_dft(*plan, temp, temp);

  //Multiplication by the sparse matrix storing the interpolation kernel
  purify_sparsemat_fwd_complexr(yout, temp, mat);

}

/*!
 * Define adjoint measurement operator for continuos visibilities
 * (currently includes adjoint continuos Fourier transform only).
 *
 * \param[out] out (complex double*) Output image.
 * \param[in] in (complex double*) Input visibilities.
 * \param[in] data 
 * - data[0] (purify_measurement_cparam*): Parameters for the continuos
 *            Fourier transform.
 * - data[1] (double*): Matrix with the deconvolution kernel in image
 *            space.
 * - data[2] (purify_sparsemat_row*): The sparse matrix defining the
 *            convolution operator for the the interpolation.
 * - data[3] (fftw_plan*): The complex-to-complex FFTW plan to use when
 *            computing the inverse Fourier transform (passed as an input so 
 *            that the FFTW can be FFTW_MEASUREd beforehand).
 * - data[4] (complex double*) Temporal memory for the zero padding.
 *
 * \authors Rafael Carrillo
 */

void purify_measurement_cftadj(void *out, void *in, void **data){

  int i, j, nx2, ny2;
  int st1, st2;
  double scale;
  purify_measurement_cparam *param;
  double *deconv;
  purify_sparsemat_row *mat;
  fftw_plan *plan;
  complex double *temp;
  complex double *yin;
  complex double *xout;

  //Cast input pointers
  param = (purify_measurement_cparam*)data[0];
  deconv = (double*)data[1];
  mat = (purify_sparsemat_row*)data[2];
  plan = (fftw_plan*)data[3];
  temp = (complex double*)data[4];

  yin = (complex double*)in;
  xout = (complex double*)out;

  nx2 = param->ofx*param->nx1;
  ny2 = param->ofy*param->ny1;

  //Multiplication by the adjoint of the 
  //sparse matrix storing the interpolation kernel
  purify_sparsemat_adj_complexr(temp, yin, mat);

  //Inverse FFT
  fftw_execute_dft(*plan, temp, temp);
  //Scaling
  scale = 1/sqrt((double)(nx2*ny2));

  purify_utils_fftshift_2d_c(temp, nx2, ny2);
  
  //Cropping and decovoluntion. 
  //Top left corner of the image corresponf to the original image.

  int npadx = nx2 / 4;
  int npady = ny2 / 4;

  for (j=0; j < param->ny1; j++){
    st1 = j * param->nx1;
    st2 = (j + npady) * nx2;
    for (i=0; i < param->nx1; i++){
//      *(xout + st1 + i) = *(temp + st2 + i)**(deconv + st1 + i)*scale;
      *(xout + st1 + i) = *(temp + st2 + i + npadx) * scale;
      *(xout + st1 + i) *= *(deconv + st1 + i);
    }
  }

}

/*!
 * Power method to compute the norm of the operator A.
 * 
 * \retval bound upper bound on norm of the continuos 
 * Fourier transform operator (double).
 * \param[in] A Pointer to the measurement operator.
 * \param[in] A_data Data structure associated to A.
 * \param[in] At Pointer to the the adjoint of the measurement operator.
 * \param[in] At_data Data structure associated to At.
 *
 * \authors Rafael Carrillo
 */
double purify_measurement_pow_meth(void (*A)(void *out, void *in, void **data), 
                                   void **A_data,
                                   void (*At)(void *out, void *in, void **data), 
                                   void **At_data) {

  int i, iter, nx, ny;
  int seedn = 51;
  double bound, norm, rel_ob;
  purify_measurement_cparam *param;
  complex double *y;
  complex double *x;

  
  //Cast input pointers
  param = (purify_measurement_cparam*)A_data[0];
  nx = param->nx1*param->ny1;
  ny = param->nmeas;
  iter = 0;

  y = (complex double*)malloc((ny) * sizeof( complex double));
  PURIFY_ERROR_MEM_ALLOC_CHECK(y);
  x = (complex double*)malloc((nx) * sizeof( complex double));
  PURIFY_ERROR_MEM_ALLOC_CHECK(x);

  if (param->nmeas > nx){
    for (i=0; i < nx; i++) {
        x[i] = purify_ran_gasdev2(seedn) + purify_ran_gasdev2(seedn)*I;
    }
    norm = cblas_dznrm2(nx, (void*)x, 1);
    for (i=0; i < nx; i++) {
        x[i] = x[i]/norm;
    }
    norm = 1.0;

    //main loop
    while (iter < 200){
      A((void*)y, (void*)x, A_data);
      At((void*)x, (void*)y, At_data);
      bound = cblas_dznrm2(nx, (void*)x, 1);
      rel_ob = (bound - norm)/norm;
      if (rel_ob <= 0.001)
        break;
      norm = bound;
      for (i=0; i < nx; i++) {
          x[i] = x[i]/norm;
      }
      iter++;
    }

  }
  else{
    for (i=0; i < ny; i++) {
        y[i] = purify_ran_gasdev2(seedn) + purify_ran_gasdev2(seedn)*I;
    }
    norm = cblas_dznrm2(ny, (void*)y, 1);
    for (i=0; i < ny; i++) {
        y[i] = y[i]/norm;
    }
    norm = 1.0;

    //main loop
    while (iter < 200){
      At((void*)x, (void*)y, At_data);
      A((void*)y, (void*)x, A_data);
      bound = cblas_dznrm2(ny, (void*)y, 1);
      rel_ob = (bound - norm)/norm;
      if (rel_ob <= 0.001)
        break;
      norm = bound;
      for (i=0; i < ny; i++) {
          y[i] = y[i]/norm;
      }
      iter++;
    }

  }

  free(y);
  free(x);

  return bound;

}

/*!
 * Define forward measurement operator for continuos visibilities
 * It takes advantage of signal reality and conjugate symmetry.
 * (currently includes adjoint continuos Fourier transform only).
 *
 * \param[out] out (complex double*) Output visibilities. 
 * \param[in] in (complex double*) Input image. Assumed real. Imaginary part
 *             set to zero.
 * \param[in] data 
 * - data[0] (purify_measurement_cparam*): Parameters for the continuos
 *            Fourier transform.
 * - data[1] (double*): Matrix with the deconvolution kernel in image
 *            space.
 * - data[2] (purify_sparsemat_row*): The sparse matrix defining the
 *            convolution operator for the the interpolation.
 * - data[3] (fftw_plan*): The complex-to-complex FFTW plan to use when
 *            computing the inverse Fourier transform (passed as an input so 
 *            that the FFTW can be FFTW_MEASUREd beforehand).
 * - data[4] (complex double*) Temporal memory for the zero padding.
 *
 * \authors Rafael Carrillo
 */

void purify_measurement_symcftfwd(void *out, void *in, void **data){

  int i, ny;
  purify_measurement_cparam *param;
  complex double *yout;

  purify_measurement_cftfwd(out, in, data);

  //Cast input pointers
  param = (purify_measurement_cparam*)data[0];
  yout = (complex double*)out;
  ny = param->nmeas;
 
   
  //Take real part and multiply by two.

  for (i=0; i < ny; i++){
    yout[i + ny] = conj(yout[i]);
    
  }

}

/*!
 * Define adjoint measurement operator for continuos visibilities
 * It takes advantage of signal reality and conjugate symmetry.
 * (currently includes adjoint continuos Fourier transform only).
 *
 * \param[out] out (complex double*) Output image. Imaginary part
 *             set to zero.
 * \param[in] in (complex double*) Input visibilities.
 * \param[in] data 
 * - data[0] (purify_measurement_cparam*): Parameters for the continuos
 *            Fourier transform.
 * - data[1] (double*): Matrix with the deconvolution kernel in image
 *            space.
 * - data[2] (purify_sparsemat_row*): The sparse matrix defining the
 *            convolution operator for the the interpolation.
 * - data[3] (fftw_plan*): The complex-to-complex FFTW plan to use when
 *            computing the inverse Fourier transform (passed as an input so 
 *            that the FFTW can be FFTW_MEASUREd beforehand).
 * - data[4] (complex double*) Temporal memory for the zero padding.
 *
 * \authors Rafael Carrillo
 */

void purify_measurement_symcftadj(void *out, void *in, void **data){

  int i, nx;
  purify_measurement_cparam *param;
  complex double *xout;

  purify_measurement_cftadj(out, in, data);

  //Cast input pointers
  param = (purify_measurement_cparam*)data[0];
  xout = (complex double*)out;
  nx = param->nx1*param->ny1;
 
   
  //Take real part and multiply by two.

  for (i=0; i < nx; i++){
    xout[i] = 2*creal(xout[i]) + 0.0*I;
    
  }

}



