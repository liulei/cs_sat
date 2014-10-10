
#ifndef PURIFY_IMAGE
#define PURIFY_IMAGE

/*!  
 * Definition of image.
 */
typedef struct  {
  /*! Field-of-view of image in first dimension. */
 double fov_x;
  /*! Field-of-view of image in second dimension. */
 double fov_y;
  /*! Number of image pixels in first dimension. */
 int nx;
  /*! Number of image pixels in second dimension. */
 int ny;
  /*! Image pixel values. */
 double *pix;
} purify_image;


/*! Image filetypes supported by PURIFY. */
typedef enum
{
  /*! Fits file format. */
  PURIFY_IMAGE_FILETYPE_FITS = 0,
  PURIFY_IMAGE_FILETYPE_FITS_BYTE = 1,
} purify_image_filetype;


inline void purify_image_ixiy2ind(int *ind, int ix, int iy, 
				  purify_image *img);

inline void purify_image_ind2iuiv(int *ix, int *iy, int ind, 
				  purify_image *img);

void purify_image_free(purify_image *img);

int purify_image_compare(purify_image *img1, 
			 purify_image *img2, 
			 double tol);

int purify_image_readfile(purify_image *img,
			  const char *filename,
			  purify_image_filetype filetype);

int purify_image_writefile(purify_image *img,
			   const char *filename,
			   purify_image_filetype filetype);

#endif
