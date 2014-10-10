/*! 
 * \file purify_image.c
 * Functionality to operate on images.
 */

#include <fitsio.h>
#include <stdio.h>
#include <math.h>
#include <assert.h>
#include "purify_image.h"
#include "purify_error.h"
#include "purify_types.h"


/*!
 * Compute 1D image index from 2D indices.
 * 
 * \param[out] ind 1D image index.
 * \param[in] ix First coordinate of 2D image index.
 * \param[in] iy Second coordinate of 2D image index.
 * \param[in] img Image object containing image sizes.
 *
 * \authors <a href="http://www.jasonmcewen.org">Jason McEwen</a>
 */
inline void purify_image_ixiy2ind(int *ind, int ix, int iy, 
				  purify_image *img) {

 if (ix >= img->nx || ix >= img->ny)
    PURIFY_ERROR_GENERIC("Image index too large");

  *ind = ix * img->ny + iy;

}


/*!
 * Compute 2D image indices from 1D index.
 * 
 * \param[out] ix First coordinate of 2D image index.
 * \param[out] iy Second coordinate of 2D image index.
 * \param[in] ind 1D image index.
 * \param[in] img Image object containing image sizes.
 *
 * \authors <a href="http://www.jasonmcewen.org">Jason McEwen</a>
 */
inline void purify_image_ind2iuiv(int *ix, int *iy, int ind, 
				  purify_image *img) {

  if (ind >= img->nx*img->ny)
    PURIFY_ERROR_GENERIC("Image index too large");

  *ix = ind / img->ny; // Integer division.
  *iy = ind - *ix * img->ny;

}


/*!
 * Free all memory used to store an image.
 *
 * \param[in,out] img Image object to free.
 *
 * \authors <a href="http://www.jasonmcewen.org">Jason McEwen</a>
 */
void purify_image_free(purify_image *img) {
  
  if(img->pix != NULL) free(img->pix);
  img->fov_x = 0.0;
  img->fov_y = 0.0;
  img->nx = 0;
  img->ny = 0;

}


/*!
 * Compare two image objects to see whether they hold identical
 * data.
 *
 * \param[in] img1 First image object.
 * \param[in] img2 Second img object.
 * \param[in] tol Tolerance used when comparing doubles.
 * \retval comparison Zero if objects contain identical data, non-zero
 * otherwise.
 *
 * \authors <a href="http://www.jasonmcewen.org">Jason McEwen</a>
 */
int purify_image_compare(purify_image *img1, 
			 purify_image *img2, 
			 double tol) {

  int i;

  if (fabs(img1->fov_x - img2->fov_x) > tol) return 1;
  if (fabs(img1->fov_y - img2->fov_y) > tol) return 1;
  if (img1->nx != img2->nx) return 1;
  if (img1->ny != img2->ny) return 1;
  for (i = 0; i < img1->nx*img1->ny; i++) 
    if (fabs(img1->pix[i] - img2->pix[i]) > tol) return 1;

  return 0;
}


/*!
 * Read image from file.
 *
 * \param[out] img Image read from the file.
 * \param[in] filename Name of the file to read.
 * \param[in] filetype Type of file to read.
 * \retval error Zero return indicates no errors.
 *
 * \note Memory for the image is allocated herein and must be
 * freed by the calling routine.
 * 
 * \authors <a href="http://www.jasonmcewen.org">Jason McEwen</a>
 */
int purify_image_readfile(purify_image *img,
			  const char *filename,
			  purify_image_filetype filetype) {

  char buffer[PURIFY_STRLEN];
  fitsfile *fptr;
  int fits_status = 0;
  int hdutype;
  int naxis, bitpix, anynul;
  long naxes[4], fpixel[4];

  switch (filetype) {
  case PURIFY_IMAGE_FILETYPE_FITS:

    // Open fits file.
    fits_open_file(&fptr, filename, READONLY, &fits_status);
    fits_report_error(stdout, fits_status);

    // Move to first header.
    fits_movabs_hdu(fptr, 1, &hdutype, &fits_status);
    fits_report_error(stdout, fits_status);
    if (hdutype != IMAGE_HDU)
      PURIFY_ERROR_GENERIC("Invalid fits header");
    
    // Get image size.
    fits_get_img_dim(fptr, &naxis, &fits_status);
    fits_get_img_size(fptr, naxis, naxes, &fits_status);
    fits_report_error(stdout, fits_status);
    if (naxis < 2)
      PURIFY_ERROR_GENERIC("Invalid fits image size");
    if ((naxis >= 3)&&(naxes[2] != 1))
      PURIFY_ERROR_GENERIC("Invalid fits image size");

    
    // Allocate space for image.
    img->nx = (int)naxes[0];
    img->ny = (int)naxes[1];
    img->fov_x = 0.0;
    img->fov_y = 0.0;
    img->pix = (double*)malloc(img->nx * img->ny * sizeof(double));
    PURIFY_ERROR_MEM_ALLOC_CHECK(img->pix);
    
    
    // Read image.
    fits_get_img_type(fptr, &bitpix, &fits_status);
    if (bitpix != DOUBLE_IMG && bitpix != FLOAT_IMG) 
      PURIFY_ERROR_GENERIC("Fits image does not contain doubles");
    fpixel[0] = 1;
    fpixel[1] = 1;
    fpixel[2] = 1;
    fpixel[3] = 1;
    fits_read_pix(fptr, TDOUBLE, fpixel, naxes[0]*naxes[1], 
      NULL, img->pix, &anynul, &fits_status);
    fits_report_error(stdout, fits_status);

    // Close fits file.
    fits_close_file(fptr,  &fits_status);
    fits_report_error(stdout, fits_status);

    break;

  case PURIFY_IMAGE_FILETYPE_FITS_BYTE:

    // Open fits file.
    fits_open_file(&fptr, filename, READONLY, &fits_status);
    fits_report_error(stdout, fits_status);

    // Move to first header.
    fits_movabs_hdu(fptr, 1, &hdutype, &fits_status);
    fits_report_error(stdout, fits_status);
    if (hdutype != IMAGE_HDU)
      PURIFY_ERROR_GENERIC("Invalid fits header");
    
    // Get image size.
    fits_get_img_dim(fptr, &naxis, &fits_status);
    fits_get_img_size(fptr, naxis, naxes, &fits_status);
    fits_report_error(stdout, fits_status);
    if (naxis < 2)
      PURIFY_ERROR_GENERIC("Invalid fits image size");
    if ((naxis >= 3)&&(naxes[2] != 1))
      PURIFY_ERROR_GENERIC("Invalid fits image size");

    
    // Allocate space for image.
    img->nx = (int)naxes[0];
    img->ny = (int)naxes[1];
    img->fov_x = 0.0;
    img->fov_y = 0.0;
    img->pix = (double*)malloc(img->nx * img->ny * sizeof(double));
    PURIFY_ERROR_MEM_ALLOC_CHECK(img->pix);
    
    unsigned char * buf = (unsigned char *)malloc(sizeof(unsigned char) * img->nx * img->ny);
    assert(buf != NULL);
    
    // Read image.
    fits_get_img_type(fptr, &bitpix, &fits_status);
    if (bitpix != BYTE_IMG) 
      PURIFY_ERROR_GENERIC("Fits image does not contain bytes");
    fpixel[0] = 1;
    fpixel[1] = 1;
    fpixel[2] = 1;
    fpixel[3] = 1;
    fits_read_pix(fptr, TBYTE, fpixel, naxes[0]*naxes[1], 
      NULL, buf, &anynul, &fits_status);
    fits_report_error(stdout, fits_status);

    // Close fits file.
    fits_close_file(fptr,  &fits_status);
    fits_report_error(stdout, fits_status);
    
    int i;
    for(i = 0; i < naxes[0] * naxes[1]; ++i){
        img->pix[i] = buf[i];    
    }

    break;

  default:
    sprintf(buffer, 
	    "Image filetype with id %d is not supported", 
	    filetype);
    PURIFY_ERROR_GENERIC(buffer);
    break;
    
  }

  return 0;

}


/*!
 * Write image to file.
 * 
 * \param[in] img Image to write to the file.
 * \param[in] filename Name of the file to write.
 * \param[in] filetype Type of file to write.
 * \retval error Zero return indicates no errors.
 *
 * \authors <a href="http://www.jasonmcewen.org">Jason McEwen</a>
 */
int purify_image_writefile(purify_image *img,
			   const char *filename,
			   purify_image_filetype filetype) {

  char buffer[PURIFY_STRLEN];
  fitsfile *fptr;
  int fits_status = 0;
  long naxes[2], fpixel[2];


  switch (filetype) {
  case PURIFY_IMAGE_FILETYPE_FITS:

    // Open fits file.
    fits_create_file(&fptr, filename, &fits_status);
    fits_report_error(stdout, fits_status);

    // Create primary header.
    naxes[0] = img->nx;
    naxes[1] = img->ny;
    fits_create_img(fptr, DOUBLE_IMG, 2, naxes, &fits_status);
    fits_report_error(stdout, fits_status);
    fits_write_comment(fptr, "--------------------------------------------",  &fits_status);
    fits_write_comment(fptr, "File written by PURIFY (www.jasonmcewen.org)",  &fits_status);
    fits_write_comment(fptr, "--------------------------------------------",  &fits_status);

    // Write image.
    fpixel[0] = 1;
    fpixel[1] = 1;
    fits_write_pix(fptr, TDOUBLE, fpixel, naxes[0]*naxes[1], img->pix, &fits_status);

    // Close fits file.
    fits_close_file(fptr,  &fits_status);
    fits_report_error(stdout, fits_status);

    break;

  default:
    sprintf(buffer, 
	    "Image filetype with id %d is not supported", 
	    filetype);
    PURIFY_ERROR_GENERIC(buffer);
    break;
    
  }

  return 0;

}
