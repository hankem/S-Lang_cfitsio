/* cfitsio interface */
#include "config.h"
#include <stdio.h>
#include <string.h>
#include <slang.h>

#include <errno.h>

#include <fitsio.h>

#ifdef __cplusplus
extern "C" 
{
#endif
SLANG_MODULE(cfitsio);
#ifdef __cplusplus
}
#endif

#define FITS_MODULE_VERSION    100
static char *Version_String = "0.1.0";

typedef struct
{
   fitsfile *fptr;
}
FitsFile_Type;

static int Fits_Type_Id;

static int map_fitsio_type_to_slang (int type, long *repeat, unsigned char *stype)
{
   /* Variable length objects have negative type values */
   if (type < 0)
     type = -type;

   switch (type)
     {
      case TINT:
	*stype = SLANG_INT_TYPE;
	break;
	
      case TLONG:
	*stype = SLANG_LONG_TYPE;
	break;
	
      case TSHORT:
	*stype = SLANG_SHORT_TYPE;
	break;
	
      case TDOUBLE:
	*stype = SLANG_DOUBLE_TYPE;
	break;
	
      case TFLOAT:
	*stype = SLANG_FLOAT_TYPE;
	break;
#ifdef TUINT	
      case TUINT:
	*stype = SLANG_UINT_TYPE;
	break;
#endif
      case TUSHORT:
	*stype = SLANG_USHORT_TYPE;
	break;
      case TULONG:
	*stype = SLANG_ULONG_TYPE;
	break;

      case TLOGICAL:
      case TBYTE:
	*stype = SLANG_UCHAR_TYPE;
	break;
      case TBIT:
	switch ((int)*repeat)
	  {
	   case 32:
	     *stype = SLANG_UINT_TYPE;
	     break;
	   case 16:
	     *stype = SLANG_SHORT_TYPE;
	     break;
	   case 8:
	     *stype = SLANG_UCHAR_TYPE;
	     break;
	   default:
	     SLang_verror (SL_NOT_IMPLEMENTED, "bit type %ldX is not supported", *repeat);
	     return -1;
	  }
	*repeat = 1;
	break;

      case TSTRING:
	*stype = SLANG_STRING_TYPE;
	break;

      default:
	SLang_verror (SL_NOT_IMPLEMENTED, "Fits column type %d is not supported",
		      type);
	return -1;
     }
   
   return 0;
}

static int open_file (SLang_Ref_Type *ref, char *filename, char *mode)
{
   fitsfile *fptr;
   int status;
   FitsFile_Type *ft;
   SLang_MMT_Type *mmt;

   if (-1 == SLang_assign_to_ref (ref, SLANG_NULL_TYPE, NULL))
     return -1;

   status = 0;
   switch (*mode)
     {
      case 'r':
	(void) fits_open_file (&fptr, filename, READONLY, &status);
	break;
	
      case 'w':
	(void) fits_open_file (&fptr, filename, READWRITE, &status);
	break;
	
      case 'c':
	if ((-1 == remove (filename))
	    && (errno != ENOENT))
	  {
	     SLang_verror (SL_OBJ_NOPEN, "Unable to create a new version of %s--- check permissions", filename);
	     return -1;
	  }
	(void) fits_create_file (&fptr, filename, &status);
	break;
	
      default:
	SLang_verror (SL_INVALID_PARM, "fits_open_file: iomode \"%s\" is invalid", mode);
	return -1;
     }

   if (fptr == NULL)
     return status;

   ft = (FitsFile_Type *) SLmalloc (sizeof (FitsFile_Type));
   if (ft == NULL)
     {
	fits_close_file (fptr, &status);
	return -1;
     }
   memset ((char *) ft, 0, sizeof (FitsFile_Type));
   
   ft->fptr = fptr;
   
   if (NULL == (mmt = SLang_create_mmt (Fits_Type_Id, (VOID_STAR) ft)))
     {
	fits_close_file (fptr, &status);
	SLfree ((char *) fptr);
	return -1;
     }
   
   if (-1 == SLang_assign_to_ref (ref, Fits_Type_Id, &mmt))
     {
	SLang_free_mmt (mmt);	       /* This will close the file */
	return -1;
     }

   return status;
}

static int delete_file (FitsFile_Type *ft)
{
   int status = 0;

   if (ft->fptr != NULL)
     fits_delete_file (ft->fptr, &status);
   ft->fptr = NULL;
   return status;
}

static int close_file (FitsFile_Type *ft)
{
   int status = 0;

   status = 0;
   if (ft->fptr != NULL)
     {
	(void) fits_close_file (ft->fptr, &status);
	ft->fptr = NULL;
     }
   return status;
}

static int movnam_hdu (FitsFile_Type *ft, int *hdutype, char *extname, int *extvers)
{
   int status = 0;
   if (ft->fptr == NULL)
     return -1;
   return fits_movnam_hdu (ft->fptr, *hdutype, extname, *extvers, &status);
}

static int movabs_hdu (FitsFile_Type *ft, int *n)
{
   int status = 0;
   if (ft->fptr == NULL)
     return -1;
   return fits_movabs_hdu (ft->fptr, *n, NULL, &status);
}

static int movrel_hdu (FitsFile_Type *ft, int *n)
{
   int status = 0;
   if (ft->fptr == NULL)
     return -1;
   return fits_movrel_hdu (ft->fptr, *n, NULL, &status);
}

static int get_num_hdus (FitsFile_Type *ft, SLang_Ref_Type *ref)
{
   int status = 0;
   int num;

   if (ft->fptr == NULL)
     return -1;

   if (0 == fits_get_num_hdus (ft->fptr, &num, &status))
     {
	if (-1 == SLang_assign_to_ref (ref, SLANG_INT_TYPE, &num))
	  return -1;
     }
   
   return status;
}

static int get_hdu_num (FitsFile_Type *ft)
{
   int num;
   if (ft->fptr == NULL)
     return -1;
   return fits_get_hdu_num (ft->fptr, &num);
}


static int get_hdu_type (FitsFile_Type *ft, SLang_Ref_Type *ref)
{
   int hdutype;   
   int status = 0;

   if (ft->fptr == NULL)
     return -1;
   if (0 == fits_get_hdu_type (ft->fptr, &hdutype, &status))
     {
	if (-1 == SLang_assign_to_ref (ref, SLANG_INT_TYPE, &hdutype))
	  return -1;
     }
   return status;
}

static int copy_file (FitsFile_Type *ft, FitsFile_Type *gt, 
		      int *prev, int *cur, int *next)
{
   int status = 0;

   if ((ft->fptr == NULL) || (gt->fptr == NULL))
     return -1;
#ifndef fits_copy_file
   (void) status; (void) prev; (void) cur; (void) next;
   SLang_verror (SL_NOT_IMPLEMENTED, "Not supported by this version of cfitsio");
   return -1;
#else
   return fits_copy_file (ft->fptr, gt->fptr, *prev, *cur, *next, &status);
#endif
}

static int copy_hdu (FitsFile_Type *ft, FitsFile_Type *gt, int *morekeys)
{
   int status = 0;

   if ((ft->fptr == NULL) || (gt->fptr == NULL))
     return -1;
   
   return fits_copy_hdu (ft->fptr, gt->fptr, *morekeys, &status);
}

static int copy_header (FitsFile_Type *ft, FitsFile_Type *gt)
{
   int status = 0;

   if ((ft->fptr == NULL) || (gt->fptr == NULL))
     return -1;
   
   return fits_copy_header (ft->fptr, gt->fptr, &status);
}


static int delete_hdu (FitsFile_Type *ft)
{
   int status = 0;

   if (ft->fptr == NULL)
     return -1;
   
   return fits_delete_hdu (ft->fptr, NULL, &status);
}

   
static int pop_string_or_null (char **s)
{   
   if (SLANG_NULL_TYPE == SLang_peek_at_stack ())
     {
	*s = NULL;
	return SLang_pop_null ();
     }
   
   return SLang_pop_slstring (s);
}

static int pop_array_or_null (SLang_Array_Type **a)
{	     
   if (SLANG_NULL_TYPE == SLang_peek_at_stack ())
     {
	*a = NULL;
	return SLang_pop_null ();
     }
   return SLang_pop_array (a, 1);
}


static FitsFile_Type *pop_fits_type (SLang_MMT_Type **mmt)
{
   FitsFile_Type *ft;

   if (NULL == (*mmt = SLang_pop_mmt (Fits_Type_Id)))
     return NULL;
   
   if (NULL == (ft = (FitsFile_Type *) SLang_object_from_mmt (*mmt)))
     {
	SLang_free_mmt (*mmt);
	*mmt = NULL;
     }
   return ft;
}

static int create_img (FitsFile_Type *ft, int *bitpix, 
		       SLang_Array_Type *at_naxes)
{
   long *axes;
   unsigned int i, imax;
   int status = 0;
   
   if (at_naxes->data_type != SLANG_INT_TYPE)
     {
	SLang_verror (SL_TYPE_MISMATCH,
		      "fits_create_img: naxis must be an integer array");
	return -1;
     }

   imax = at_naxes->num_elements;
   axes = (long *) SLmalloc (imax * sizeof (long));
   if (axes == NULL)
     return -1;
   for (i = 0; i < imax; i++)
     axes[i] = ((int *) at_naxes->data)[i];

   (void) fits_create_img (ft->fptr, *bitpix, imax, axes, &status);
   SLfree ((char *) axes);
   return status;
}

static int write_img (FitsFile_Type *ft, SLang_Array_Type *at)
{
   int type;
   int status = 0;

   if (ft->fptr == NULL)
     return -1;

   switch (at->data_type)
     {
      case SLANG_STRING_TYPE:
	type = TSTRING;
	break;
	
      case SLANG_DOUBLE_TYPE:
	type = TDOUBLE;
	break;

      case SLANG_FLOAT_TYPE:
	type = TFLOAT;
	break;
	
      case SLANG_SHORT_TYPE:
	type = TSHORT;
	break;
	
      case SLANG_CHAR_TYPE:
      case SLANG_UCHAR_TYPE:
	type = TBYTE;
	break;

      case SLANG_INT_TYPE:
	type = TINT;
	break;

      case SLANG_LONG_TYPE:
	type = TLONG;
	break;
	
      default:
	SLang_verror (SL_NOT_IMPLEMENTED,
		      "fits_write_img: %s not supported",
		      SLclass_get_datatype_name (at->data_type));
	return -1;
     }
   
   return fits_write_img (ft->fptr, type, 1, at->num_elements,
			  at->data, &status);
}

static int read_img (FitsFile_Type *ft, SLang_Ref_Type *ref)
{
   int status = 0;
   int anynul = 0;
   int type, stype;
   int num_dims, i;
   long ldims[SLARRAY_MAX_DIMS];
   int dims[SLARRAY_MAX_DIMS];
   SLang_Array_Type *at;

   if (ft->fptr == NULL)
     return -1;
   
   status = fits_get_img_type (ft->fptr, &type, &status);
   if (status)
     return status;

   switch (type)
     {
      case BYTE_IMG:
	stype = SLANG_UCHAR_TYPE;
	type = TBYTE;
	break;
	
      case SHORT_IMG:
	stype = SLANG_SHORT_TYPE;
	type = TSHORT;
	break;
	
      case LONG_IMG:
	stype = SLANG_LONG_TYPE;
	type = TLONG;
	break;
	
      case DOUBLE_IMG:
	stype = SLANG_DOUBLE_TYPE;
	type = TDOUBLE;
	break;

      case FLOAT_IMG:
      default:
	stype = SLANG_FLOAT_TYPE;
	type = TFLOAT;
	break;
     }

   if (fits_get_img_dim (ft->fptr, &num_dims, &status))
     return status;

   if ((num_dims > SLARRAY_MAX_DIMS) || (num_dims < 0))
     {
	SLang_verror (SL_NOT_IMPLEMENTED, "Image dimensionality is not supported");
	return -1;
     }

   if (fits_get_img_size (ft->fptr, num_dims, ldims, &status))
     return status;

#if 0
   for (i = 0; i < num_dims; i++) dims[i] = (int) ldims[i];
#else
   for (i = 0; i < num_dims; i++) dims[num_dims-1-i] = (int) ldims[i];
#endif

   if (NULL == (at = SLang_create_array (stype, 0, NULL, dims, num_dims)))
     return -1;

   status = fits_read_img (ft->fptr, type, 1, at->num_elements, NULL,
			   at->data, &anynul, &status);
   
   if (status)
     {
	SLang_free_array (at);
	return status;
     }
   
   if (-1 == SLang_assign_to_ref (ref, SLANG_ARRAY_TYPE, (VOID_STAR)&at))
     status = -1;

   SLang_free_array (at);
   return status;
}

static int create_binary_tbl (void)
{
   SLang_MMT_Type *mmt;
   FitsFile_Type *ft;
   SLang_Array_Type *at_ttype, *at_tform, *at_tunit;
   char *extname;
   int tfields, nrows;
   int status;
      
   status = -1;
   at_ttype = at_tform = at_tunit = NULL;
   mmt = NULL;
   ft = NULL;

   if (-1 == pop_string_or_null (&extname))
     return -1;

   if (-1 == pop_array_or_null (&at_tunit))
     goto free_and_return;
   
   if (-1 == SLang_pop_array (&at_tform, 1))
     goto free_and_return;
   
   if (-1 == SLang_pop_array (&at_ttype, 1))
     goto free_and_return;
   
   if (-1 == SLang_pop_integer (&nrows))
     goto free_and_return;
   
   if (NULL == (ft = pop_fits_type (&mmt)))
     goto free_and_return;
   
   if (ft->fptr == NULL)
     goto free_and_return;


   tfields = (int) at_ttype->num_elements;
   
   if (at_ttype->data_type != SLANG_STRING_TYPE)
     {
	SLang_verror (SL_TYPE_MISMATCH, 
		      "fits_create_binary_tbl: ttype must be String_Type[%d]",
		      tfields);
	goto free_and_return;
     }

   if ((tfields != (int) at_tform->num_elements)
       || (at_tform->data_type != SLANG_STRING_TYPE))
     {
	SLang_verror (SL_TYPE_MISMATCH,
		      "fits_create_binary_tbl: tform must be String_Type[%d]",
		      tfields);
	goto free_and_return;
     }

   if ((at_tunit != NULL)
       && ((tfields != (int) at_tunit->num_elements)
	   || (at_tunit->data_type != SLANG_STRING_TYPE)))
     {
	SLang_verror (SL_TYPE_MISMATCH, 
		      "fits_create_binary_tbl: tunit must be String_Type[%d]",
		      tfields);
	goto free_and_return;
     }

   status = 0;
   fits_create_tbl (ft->fptr, BINARY_TBL, nrows, tfields,
		    (char **) at_ttype->data,
		    (char **) at_tform->data,
		    ((at_tunit == NULL) ? NULL : (char **) at_tunit->data),
		    extname, &status);
   
   /* drop */

   free_and_return:
   SLang_free_array (at_ttype);
   SLang_free_array (at_tform);
   SLang_free_array (at_tunit);
   SLang_free_mmt (mmt);
   SLang_free_slstring (extname);
   
   return status;
}

static int update_key (void)
{
   SLang_MMT_Type *mmt;
   FitsFile_Type *ft;
   char *comment;
   int i;
   double d;
   char *s;
   char *key;
   int type;
   VOID_STAR v;
   int status;

   if (-1 == pop_string_or_null (&comment))
     return -1;
   
   key = s = NULL;
   mmt = NULL;
   status = -1;

   type = SLang_peek_at_stack ();
   switch (type)
     {
      case SLANG_STRING_TYPE:
	type = TSTRING;
	if (-1 == SLang_pop_slstring (&s))
	  goto free_and_return;
	v = (VOID_STAR) s;
	break;
	
      case SLANG_INT_TYPE:
	type = TINT;
	if (-1 == SLang_pop_integer (&i))
	  goto free_and_return;
	v = (VOID_STAR) &i;
	break;

      case SLANG_NULL_TYPE:
	if (-1 == SLang_pop_null ())
	  goto free_and_return;
	v = NULL;
	break;
	
      case -1:			       /* stack underflow */
	goto free_and_return;

      case SLANG_DOUBLE_TYPE:
      default:
	type = TDOUBLE;
	if (-1 == SLang_pop_double (&d, NULL, NULL))
	  goto free_and_return;
	v = (VOID_STAR) &d;
	break;
     }

   if (-1 == SLang_pop_slstring (&key))
     goto free_and_return;

   if (NULL == (ft = pop_fits_type (&mmt)))
     goto free_and_return;
   
   if (ft->fptr == NULL)
     goto free_and_return;

   status = 0;
   if (v != NULL)
     {
	if (type == TSTRING)
	  fits_update_key_longstr (ft->fptr, key, (char *)v, comment, &status);
	else
	  fits_update_key (ft->fptr, type, key, v, comment, &status);
     }
   else
     fits_update_key_null (ft->fptr, key, comment, &status);

   free_and_return:

   SLang_free_mmt (mmt);
   SLang_free_slstring (key);
   SLang_free_slstring (comment);
   SLang_free_slstring (s);
   
   return status;
}

static int update_logical (void)
{
   SLang_MMT_Type *mmt;
   FitsFile_Type *ft;
   char *comment;
   int i;
   char *key;
   int status;

   if (-1 == pop_string_or_null (&comment))
     return -1;
   
   key = NULL;
   mmt = NULL;
   status = -1;

   if ((0 == SLang_pop_integer (&i))
       && (0 == SLang_pop_slstring (&key))
       && (NULL != (ft = pop_fits_type (&mmt)))
       && (ft->fptr != NULL))
     {
	status = 0;
	fits_update_key (ft->fptr, TLOGICAL, key, 
			 (VOID_STAR) &i, comment, &status);
     }

   SLang_free_mmt (mmt);
   SLang_free_slstring (key);
   SLang_free_slstring (comment);
   
   return status;
}

static int write_comment (FitsFile_Type *ft, char *comment)
{
   int status = 0;
   if (ft->fptr == NULL)
     return -1;
   return fits_write_comment (ft->fptr, comment, &status);
}

static int write_history (FitsFile_Type *ft, char *comment)
{
   int status = 0;
   if (ft->fptr == NULL)
     return -1;
   return fits_write_history (ft->fptr, comment, &status);
}

static int write_date (FitsFile_Type *ft)
{
   int status = 0;
   if (ft->fptr == NULL)
     return -1;
   return fits_write_date (ft->fptr, &status);
}

static int write_record (FitsFile_Type *ft, char *card)
{
   int status = 0;
   
   if (ft->fptr == NULL)
     return -1;

   /* How robust is fits_write_record to cards that are not 80 characters long? */
   return fits_write_record (ft->fptr, card, &status);
}

static int modify_name (FitsFile_Type *ft, char *oldname, char *newname)
{
   int status = 0;
   if (ft->fptr == NULL)
     return -1;
   return fits_modify_name (ft->fptr, oldname, newname, &status);
}

static int do_get_keytype (fitsfile *f, char *name, int *stype)
{
   int status = 0;
   char type;
   int s;
   char card [FLEN_CARD + 1];
   char value [FLEN_CARD + 1];
   
   if (f == NULL)
     return -1;

   if (0 != fits_read_card (f, name, card, &status))
     return status;
   
   if (0 != fits_parse_value (card, value, NULL, &status))
     return status;

   if (0 != fits_get_keytype (value, &type, &status))
     return status;

   switch (type)
     {
      case 'C':
	s = SLANG_STRING_TYPE;
	break;
	
      case 'L':
	s = SLANG_INT_TYPE;
	break;
	     
      case 'F':
	s = SLANG_DOUBLE_TYPE;
	break;
	     
      case 'X':
	s = SLANG_COMPLEX_TYPE;
	break;
	     
      case 'I':
      default:
	s = SLANG_INT_TYPE;
	break;
     }
   *stype = s;

   return 0;
}

	

static int read_key (int type)
{
   SLang_MMT_Type *mmt;
   FitsFile_Type *ft;
   int status;
   char *name;
   char comment_buf [FLEN_COMMENT];
   SLang_Ref_Type *comment_ref, *v_ref;
   int ival;
   double dval;
   char *sval = NULL;
   int ftype;
   VOID_STAR v;

   v_ref = comment_ref = NULL;
   name = NULL;
   mmt = NULL;
   status = -1;
   
   if (SLANG_NULL_TYPE == SLang_peek_at_stack ())
     {
	if (-1 == SLang_pop_null ())
	  return -1;
     }
   else if (-1 == SLang_pop_ref (&comment_ref))
     return -1;
   
   if (-1 == SLang_pop_ref (&v_ref))
     goto free_and_return;
   
   if (-1 == SLang_pop_slstring (&name))
     goto free_and_return;

   if (NULL == (ft = pop_fits_type (&mmt)))
     goto free_and_return;
   
   if (ft->fptr == NULL)
     goto free_and_return;

   if (type == SLANG_VOID_TYPE)
     {
	if (0 != (status = do_get_keytype (ft->fptr, name, &type)))
	  goto free_and_return;
	
	status = -1;
     }

   switch (type)
     {
      case SLANG_INT_TYPE:
	v = (VOID_STAR) &ival;
	ftype = TINT;
	ival = 0;
	break;
	
      case SLANG_DOUBLE_TYPE:
	ftype = TDOUBLE;
	v = (VOID_STAR) &dval;
	dval = 0.0;
	break;

      case SLANG_STRING_TYPE:
	ftype = TSTRING;
	v = (VOID_STAR) &sval;
	break;
	
      default:
	SLang_verror (SL_INVALID_PARM, 
		      "fits_read_key: type %s not supported",
		      SLclass_get_datatype_name (type));
	goto free_and_return;
     }

   status = 0;
   if (ftype == TSTRING)
     fits_read_key_longstr (ft->fptr, name, &sval, comment_buf, &status);
   else
     fits_read_key (ft->fptr, ftype, name, v, comment_buf, &status);

   if (status == 0)
     {
	if (comment_ref != NULL)
	  {
	     char *cptr = comment_buf;
	     if (-1 == SLang_assign_to_ref (comment_ref, 
					    SLANG_STRING_TYPE, (VOID_STAR) &cptr))
	       {
		  status = -1;
		  goto free_and_return;
	       }
	  }
	if (-1 == SLang_assign_to_ref (v_ref, type, v))
	  {
	     status = -1;
	     goto free_and_return;
	  }
     }

   free_and_return:
   SLfree (sval);
   SLang_free_ref (comment_ref);
   SLang_free_ref (v_ref);
   SLang_free_slstring (name);
   SLang_free_mmt (mmt);
   return status;
}

static int read_key_integer (void)
{
   return read_key (SLANG_INT_TYPE);
}

static int read_key_double (void)
{
   return read_key (SLANG_DOUBLE_TYPE);
}

static int read_key_string (void)
{
   return read_key (SLANG_STRING_TYPE);
}

static int read_generic_key (void)
{
   return read_key (SLANG_VOID_TYPE);
}

static int read_record (FitsFile_Type *ft, int *keynum, SLang_Ref_Type *ref)
{
   int status = 0;
   char card[FLEN_CARD+1];

   if (ft->fptr == NULL)
     return -1;

   if (0 == fits_read_record (ft->fptr, *keynum, card, &status))
     {
	char *c = card;
	if (-1 == SLang_assign_to_ref (ref, SLANG_STRING_TYPE, &c))
	  return -1;
     }
   
   return status;
}
   
static int delete_key (FitsFile_Type *ft, char *key)
{
   int status = 0;
   if (ft->fptr == NULL)
     return -1;
   return fits_delete_key (ft->fptr, key, &status);
}


static int get_colnum (FitsFile_Type *ft, char *name, SLang_Ref_Type *ref)
{
   int status = 0;
   int col;

   if (ft->fptr == NULL)
     return -1;
   /* FIXME: fits_get_colnum may be used to get columns matching a pattern */
   col = 1;
   fits_get_colnum (ft->fptr, CASEINSEN, name, &col, &status);

   if (-1 == SLang_assign_to_ref (ref, SLANG_INT_TYPE, (VOID_STAR) &col))
     status = -1;

   return status;
}

static int insert_rows (FitsFile_Type *ft, int *first, int *num)
{
   int status = 0;
   if (ft->fptr == NULL)
     return -1;
   if ((*first <= 0) || (*num < 0))
     {
	SLang_verror (SL_INVALID_PARM, "fits_insert_rows: first and num must be positive");
	return -1;
     }

   return fits_insert_rows (ft->fptr, *first, *num, &status);
}

static int delete_rows (FitsFile_Type *ft, int *first, int *num)
{
   int status = 0;
   if (ft->fptr == NULL)
     return -1;
   if ((*first <= 0) || (*num < 0))
     {
	SLang_verror (SL_INVALID_PARM, "fits_delete_rows: first and num must be positive");
	return -1;
     }
   
   return fits_delete_rows (ft->fptr, *first, *num, &status);
}

static int insert_cols (FitsFile_Type *ft, int *colnum, 
			SLang_Array_Type *at_ttype, 
			SLang_Array_Type *at_tform)
{
   int ncols;
   char **ttype, **tform;
   int i;
   int status = 0;

   if (ft->fptr == NULL)
     return -1;
   ncols = at_ttype->num_elements;
   if ((ncols < 0) || (ncols != (int) at_tform->num_elements)
       || (at_ttype->data_type != SLANG_STRING_TYPE)
       || (at_tform->data_type != SLANG_STRING_TYPE))
     {
	SLang_verror (SL_INVALID_PARM,
		      "fits_insert_cols: ttype and tform must be string arrays of same size");
	return -1;
     }
   
   if (*colnum <= 0)
     {
	SLang_verror (SL_INVALID_PARM, "fits_insert_cols: colnum must be positive");
	return -1;
     }
   
   tform = (char **)at_tform->data;
   ttype = (char **)at_ttype->data;

   for (i = 0; i < ncols; i++)
     {
	if ((tform[i] == NULL) || (ttype[i] == NULL))
	  {
	     SLang_verror (SL_INVALID_PARM, 
			   "fits_insert_cols: ttype and tform elements muts be non NULL");
	     return -1;
	  }
     }
   return fits_insert_cols (ft->fptr, *colnum, ncols, ttype, tform, &status);
}

static int delete_col (FitsFile_Type *ft, int *col)
{
   int status = 0;
   if (ft->fptr == NULL)
     return -1;
   return fits_delete_col (ft->fptr, *col, &status);
}

static int get_num_rows (FitsFile_Type *ft, SLang_Ref_Type *ref)
{
   long nrows;
   int status = 0;

   if (ft->fptr == NULL)
     return -1;
   if (0 == fits_get_num_rows (ft->fptr, &nrows, &status))
     {
	int inrows = (int) nrows;
	if (-1 == SLang_assign_to_ref (ref, SLANG_INT_TYPE, (VOID_STAR) &inrows))
	  return -1;
     }
   return status;
}

static int get_num_cols (FitsFile_Type *ft, SLang_Ref_Type *ref)
{
   int ncols;
   int status = 0;

   if (ft->fptr == NULL)
     return -1;
   if (0 == fits_get_num_cols (ft->fptr, &ncols, &status))
     {
	if (-1 == SLang_assign_to_ref (ref, SLANG_INT_TYPE, (VOID_STAR) &ncols))
	  return -1;
     }
   return status;
}

static void byte_swap32 (unsigned char *ss, unsigned int n)
{
   unsigned char *p, *pmax, ch;
   
   p = (unsigned char *) ss;
   pmax = p + 4 * n;
   while (p < pmax)
     {
	ch = *p;
	*p = *(p + 3);
	*(p + 3) = ch;
	
	ch = *(p + 1);
	*(p + 1) = *(p + 2);
	*(p + 2) = ch;
	p += 4;
     }
}

static void byte_swap16 (unsigned char *p, unsigned int nread)
{
   unsigned char *pmax, ch;
   
   pmax = p + 2 * nread;
   while (p < pmax)
     {
	ch = *p;
	*p = *(p + 1);
	*(p + 1) = ch;
	p += 2;
     }
}

   
/* MAJOR HACK!!!! */
static int hack_write_bit_col (fitsfile *f, unsigned int col, 
			       unsigned int row, unsigned int firstelem,
			       unsigned int sizeof_type, unsigned int num_elements,
			       unsigned char *bytes)
{
   static int status = 0;
   tcolumn *colptr;
   long trepeat;
   int tcode;

   if ((f == NULL) || (f->Fptr == NULL)
       || (NULL == (colptr  = (f->Fptr)->tableptr)))
     return WRITE_ERROR;

   colptr += (col - 1);     /* offset to correct column structure */
   trepeat = colptr->trepeat;
   tcode = colptr->tdatatype;
   
   colptr->tdatatype = TBYTE;
   colptr->trepeat = sizeof_type;

   
   (void) fits_write_col (f, TBYTE, col, row, firstelem,
			  num_elements*sizeof_type, bytes, &status);
   
   colptr->tdatatype = tcode;
   colptr->trepeat = trepeat;

   return status;
}

static int write_tbit_col (fitsfile *f, unsigned int col, unsigned int row, 
			   unsigned int firstelem, unsigned int repeat,
			   unsigned int width, SLang_Array_Type *at)
{
   int status = 0;
   unsigned int num_elements;
   unsigned int sizeof_type;
   unsigned char *data, *buf;
   unsigned short s;
   void (*bs) (unsigned char *, unsigned int);

   (void) width;
   num_elements = at->num_elements;
   sizeof_type = at->sizeof_type;
   data = (unsigned char *) at->data;

   if (8 * sizeof_type != repeat)
     {
	SLang_verror (0, "Writing a %dX bit column requires the appropriately sized integer",
		      repeat);
	return -1;
     }

   s = 0x1234;
   if ((*(unsigned char *) &s == 0x12)
       || (sizeof_type == 1))
     {
	return hack_write_bit_col (f, col, row, firstelem,
				   sizeof_type, num_elements, data);
     }

   /* Sigh.  Need to byteswap */
   switch (sizeof_type)
     {
      case 2:
	bs = byte_swap16;
	break;
	
      case 4:
	bs = byte_swap32;
	break;
	
      default:
	SLang_verror (0, "writing to a %dX column is not supported", repeat);
	return -1;
     }

   buf = (unsigned char *) SLmalloc (num_elements * sizeof_type);
   if (buf == NULL)
     return -1;
   
   memcpy (buf, data, num_elements * sizeof_type);
   (*bs) (buf, num_elements);
   
   status = hack_write_bit_col (f, col, row, firstelem,
				sizeof_type, num_elements, buf);

   SLfree ((char *) buf);
   return status;
}

static int write_col (FitsFile_Type *ft, int *colnum,
		      int *firstrow, int *firstelem, SLang_Array_Type *at)
{
   int type;
   int status = 0;
   int col;
   long repeat;
   long width;

   if (ft->fptr == NULL)
     return -1;
   
   col = *colnum;
   if (0 != fits_get_coltype (ft->fptr, col, &type, &repeat, &width, &status))
     return status;

   if (type == TBIT)
     return write_tbit_col (ft->fptr, col, *firstrow, *firstelem,
			    repeat, width, at);

   switch (at->data_type)
     {
      case SLANG_USHORT_TYPE:
	type = TUSHORT;
	break;

      case SLANG_SHORT_TYPE:
	type = TSHORT;
	break;

      case SLANG_UINT_TYPE:
	type = TUINT;
	break;

      case SLANG_INT_TYPE:
	type = TINT;
	break;

      case SLANG_ULONG_TYPE:
	type = TULONG;
	break;

      case SLANG_LONG_TYPE:
	type = TLONG;
	break;

      case SLANG_DOUBLE_TYPE:
	type = TDOUBLE;
	break;
	
      case SLANG_FLOAT_TYPE:
	type = TFLOAT;
	break;
	
      case SLANG_STRING_TYPE:
	type = TSTRING;
	break;

      case SLANG_CHAR_TYPE:
      case SLANG_UCHAR_TYPE:
	type = TBYTE;
	break;

      default:
	SLang_verror (SL_NOT_IMPLEMENTED, 
		      "fits_write_col: %s not suppported",
		      SLclass_get_datatype_name (at->data_type));
	return -1;
     }

   return fits_write_col (ft->fptr, type, *colnum, *firstrow, *firstelem,
			  at->num_elements, at->data, &status);
}

static int read_string_cell (fitsfile *f, unsigned int row, unsigned int col,
			     unsigned int len, char **sp)
{
   char *s;
   char *sls;
   int status = 0;
   int anynul;

   *sp = NULL;
   
   if (f == NULL)
     return -1;

   if (NULL == (s = SLmalloc (len + 1)))
     return -1;
   
   /* Note that the number of elements passed to this must be 1 */
   if (0 != fits_read_col (f, TSTRING, col, row, 1, 1, NULL,
			   &s, &anynul, &status))
     {
	SLfree (s);
	return status;
     }
   sls = SLang_create_slstring (s);
   SLfree (s);
   if (sls == NULL)
     return -1;
   
   *sp = sls;
   return 0;
}

static int read_string_column (fitsfile *f, int is_var, long repeat,
			       int col, unsigned int firstrow, unsigned int numrows,
			       SLang_Array_Type **atp)
{
   int num_elements;
   char **ats;
   unsigned int i;
   int status = 0;
   SLang_Array_Type *at;

   *atp = NULL;
   
   if (f == NULL)
     return -1;
   
   num_elements = (int) numrows;
   at = SLang_create_array (SLANG_STRING_TYPE, 0, NULL, &num_elements, 1);
   if (at == NULL)
     return -1;

   ats = (char **) at->data;

   for (i = 0; i < numrows; i++)
     {
	long offset;
	long row;
	
	row = firstrow + i;
	if (is_var)
	  {
	     if (0 != fits_read_descript (f, col, row, &repeat, &offset, &status))
	       {
		  SLang_free_array (at);
		  return status;
	       }
	  }
	
	status = read_string_cell (f, row, col, repeat, ats+i);
	if (status != 0)
	  {
	     SLang_free_array (at);
	     return status;
	  }
     }
   
   *atp = at;
   return 0;
}



static int read_bit_column (fitsfile *f, unsigned int col, unsigned int row,
			    unsigned int firstelem, unsigned int num_elements,
			    unsigned char *data, unsigned int bytes_per_elem)
{
   int status, anynul;
   unsigned short s;
   
   if (f == NULL)
     return -1;

   status = 0;
   if (0 != fits_read_col (f, TBYTE, col, row, 
			   firstelem, num_elements*bytes_per_elem,
			   NULL, data, &anynul, &status))
     return status;
   
   s = 0x1234;
   if (*(unsigned char *) &s == 0x12)
     return status;
   
   /* Otherwise, byteswap */
   switch (bytes_per_elem)
     {
      case 1:
	break;

      case 2:
	byte_swap16 (data, num_elements);
	break;
	
      case 4:
	byte_swap32 (data, num_elements);
	break;
	
      default:
	SLang_verror (SL_NOT_IMPLEMENTED, "%u byte integers are unsupported", 
		      bytes_per_elem);
	return -1;
     }
   
   return 0;
}

			   
			    
static int read_column_values (fitsfile *f, int type, unsigned char datatype,
			       unsigned int row, unsigned int col, unsigned int num_rows,
			       int repeat, SLang_Array_Type **atp)
{
   int num_elements;
   int status = 0;
   int anynul;
   SLang_Array_Type *at;
   int dims[2];
   int num_dims;

   *atp = NULL;
   
   if (f == NULL)
     return -1;
   
   dims[0] = num_elements = num_rows;
   num_dims = 1;
   if (repeat > 1)
     {
	num_elements *= repeat;
	dims[1] = repeat;
	num_dims++;
     }

   at = SLang_create_array (datatype, 0, NULL, dims, num_dims);
   if (at == NULL)
     return -1;

   if (type == TBIT)
     status = read_bit_column (f, col, row, 1, num_elements, (unsigned char *)at->data, at->sizeof_type);
   else
     (void) fits_read_col (f, type, col, row, 1, num_elements, NULL, 
			   at->data, &anynul, &status);
   if (status)
     {
	SLang_free_array (at);
	return status;
     }
   
   *atp = at;
   return 0;
}

static int read_var_column (fitsfile *f, int ftype, unsigned char datatype, 
			    int col, unsigned int firstrow, unsigned int num_rows, 
			    SLang_Array_Type **atp)
{
   int num_elements;
   unsigned int i;
   SLang_Array_Type **ati;
   SLang_Array_Type *at;

   *atp = NULL;
   if (f == NULL)
     return -1;
   
   num_elements = (int) num_rows;

   at = SLang_create_array (SLANG_ARRAY_TYPE, 0, NULL, &num_elements, 1);
   if (at == NULL)
     return -1;

   ati = (SLang_Array_Type **) at->data;
   for (i = 0; i < num_rows; i++)
     {
	long offset;
	long repeat;
	unsigned int row;
	int status = 0;

	row = firstrow + i;
	if (0 != fits_read_descript (f, col, row, &repeat, &offset, &status))
	  {
	     SLang_free_array (at);
	     return status;
	  }
	
	status = read_column_values (f, ftype, datatype, row, col, 1, repeat, ati+i);
	if (status)
	  {
	     SLang_free_array (at);
	     return status;
	  }
     }
   
   *atp = at;
   return 0;
}

static int my_fits_get_coltype (fitsfile *fptr, int col, int *type,
				long *repeat, long *width, int *statusp)
{
   int status = *statusp;
   char tscaln[32];
   char tzeron[32];
   double tscal, tzero;
   double min_val, max_val;

   if (0 != fits_get_coltype (fptr, col, type, repeat, width, &status))
     {
	*statusp = status;
	return status;
     }
   
   sprintf (tscaln, "TSCAL%d", col);
   sprintf (tzeron, "TZERO%d", col);

   if (0 != fits_read_key (fptr, TDOUBLE, tscaln, &tscal, NULL, &status))
     {
	fits_clear_errmsg ();
	tscal = 1.0;
     }
   
   if (0 != fits_read_key (fptr, TDOUBLE, tzeron, &tzero, NULL, &status))
     {
	fits_clear_errmsg ();
	tzero = 0;
     }

   switch (*type)
     {
      case TSHORT:
	min_val = -32768.0;
	max_val = 32767.0;
	break;
	
      case TLONG:
	min_val = -2147483648.0;
	max_val = 2147483647.0;
	break;
	
      default:
	return 0;
     }

   min_val = tzero + tscal * min_val;
   max_val = tzero + tscal * max_val;

   if (min_val > max_val)
     {
	double tmp = max_val; 
	max_val = min_val;
	min_val = tmp;
     }

   if ((min_val >= -32768.0) && (max_val <= 32767.0))
     {
	*type = TSHORT;
	return 0;
     }
   
   if ((min_val >= 0) && (max_val <= 65535.0))
     {
	*type = TUSHORT;
	return 0;
     }
   
   if ((min_val >= -2147483648.0) && (min_val <= 2147483648.0))
     {
	*type = TLONG;
	return 0;
     }
   
   if ((min_val >= 0.0) && (min_val <= 4294967295.0))
     {
	*type = TULONG;
	return 0;
     }
   
   *type = TDOUBLE;
   return 0;
}

static int read_col (FitsFile_Type *ft, int *colnum, int *firstrowp,
		     int *num_rowsp, SLang_Ref_Type *ref)
{
   SLang_Array_Type *at;
   int type;
   long num_rows;
   long width;
   int status;
   unsigned char datatype;
   int num_columns;
   int firstrow;
   long repeat;
   int col;
   
   if (ft->fptr == NULL)
     return -1;

   status = 0;
   if (0 != fits_get_num_cols (ft->fptr, &num_columns, &status))
     return status;

   if (0 != fits_get_num_rows (ft->fptr, &num_rows, &status))
     return status;

   if (*num_rowsp <= 0)
     {
	SLang_verror (SL_INVALID_PARM, "Number of rows must positive");
	return -1;
     }
   
   col = *colnum;

   if ((col <= 0) || (col > num_columns))
     {
	SLang_verror (SL_INVALID_PARM, "Column number out of range");
	return -1;
     }
   firstrow = *firstrowp;
   if ((firstrow <= 0) || (firstrow > num_rows))
     {
	SLang_verror (SL_INVALID_PARM, "Row number out of range");
	return -1;
     }

   if (firstrow + *num_rowsp > num_rows + 1)
     num_rows = num_rows - (firstrow - 1);
   else
     num_rows = *num_rowsp;

   if (0 != my_fits_get_coltype (ft->fptr, col, &type, &repeat, &width, &status))
     return status;

   if (-1 == map_fitsio_type_to_slang (type, &repeat, &datatype))
     return -1;
   
   
   if (datatype == SLANG_STRING_TYPE)
     status = read_string_column (ft->fptr, (type < 0), repeat, col, firstrow, num_rows, &at);
   else if (type < 0)
     status = read_var_column (ft->fptr, -type, datatype, col, firstrow, num_rows, &at);
   else
     status = read_column_values (ft->fptr, type, datatype, firstrow, col, num_rows, repeat, &at);

   if (status)
     return status;

   if (-1 == SLang_assign_to_ref (ref, SLANG_ARRAY_TYPE, (VOID_STAR)&at))
     status = -1;

   SLang_free_array (at);
   return status;
}

static void get_errstatus (int *status)
{
   char errbuf [FLEN_ERRMSG];

   *errbuf = 0;
   fits_get_errstatus (*status, errbuf);
   (void) SLang_push_string (errbuf);
}

static int get_num_keys (FitsFile_Type *f, SLang_Ref_Type *ref)
{
   int status = 0;
   int nkeys;

   if (f->fptr == NULL)
     return -1;
   if (0 == fits_get_hdrspace (f->fptr, &nkeys, NULL, &status))
     return SLang_assign_to_ref (ref, SLANG_INT_TYPE, (VOID_STAR) &nkeys);

   return status;
}


static int get_keytype (FitsFile_Type *f, char *name, SLang_Ref_Type *v)
{
   int status = 0;
   int type;
   
   if (f->fptr == NULL)
     return -1;
   if (0 == (status = do_get_keytype (f->fptr, name, &type)))
     return SLang_assign_to_ref (v, SLANG_DATATYPE_TYPE, (VOID_STAR) &type);

   return status;
}

#if 0
static int _read_key_n (FitsFile_Type *f, SLang_Ref_Type *v, SLang_Ref_Type *c)
{
   int status = 0;

   if (fits_get_keytype (card, &type, &status))
     return status;
   switch (type)
     {
      case 'C':			       /* string */
	break;
	
      case 'L':			       /* logical */
	break;
	
      case 'I':			       /* integer */
	break;
	
      case 'F':			       /* floating point */
	break;
   
      case 'X':			       /* complex */
	break;
     }
}
#endif

static void get_version (void)
{
   float v;
   (void) fits_get_version (&v);
   (void) SLang_push_float (v);
}

static int get_keyclass (char *card)
{
   return fits_get_keyclass (card);
}

static int do_fits_fun_f(int (*fun)(fitsfile *, int *), FitsFile_Type *f)
{
   int status = 0;

   if (f->fptr == NULL)
     return -1;
   
   (void) (*fun) (f->fptr, &status);
   return status;
}

static int write_chksum (FitsFile_Type *f)
{
   return do_fits_fun_f (fits_write_chksum, f);
}
static int update_chksum (FitsFile_Type *f)
{
   return do_fits_fun_f (fits_update_chksum, f);
}

static int verify_chksum (FitsFile_Type *f, SLang_Ref_Type *dataok, SLang_Ref_Type *hduok)
{
   int status = 0;
   int dok=0, hok=0;
   
   if (f->fptr == NULL)
     return -1;

   if (0 == fits_verify_chksum (f->fptr, &dok, &hok, &status))
     {
	if ((-1 == SLang_assign_to_ref (dataok, SLANG_INT_TYPE, (VOID_STAR)&dok))
	    || (-1 == SLang_assign_to_ref (hduok, SLANG_INT_TYPE, (VOID_STAR)&hok)))
	  status = -1;
     }
   return status;
}

static int get_chksum (FitsFile_Type *f, SLang_Ref_Type *datasum, SLang_Ref_Type *hdusum)
{
   int status = 0;
   unsigned long dsum, hsum;

   if (0 == fits_get_chksum (f->fptr, &dsum, &hsum, &status))
     {
	if ((-1 == SLang_assign_to_ref (datasum, SLANG_ULONG_TYPE, (VOID_STAR)&dsum))
	    || (-1 == SLang_assign_to_ref (hdusum, SLANG_ULONG_TYPE, (VOID_STAR)&hsum)))
	  status = -1;
     }
   return status;
}

/* DUMMY_FITS_FILE_TYPE is a temporary hack that will be modified to the true
 * id once the interpreter provides it when the class is registered.  See below
 * for details.  The reason for this is simple: for a module, the type-id 
 * must be assigned dynamically.
 */
#define DUMMY_FITS_FILE_TYPE	255
#define I SLANG_INT_TYPE
#define S SLANG_STRING_TYPE
#define F DUMMY_FITS_FILE_TYPE
#define R SLANG_REF_TYPE
#define A SLANG_ARRAY_TYPE
#define T SLANG_DATATYPE_TYPE

static SLang_Intrin_Fun_Type Fits_Intrinsics [] = 
{
   MAKE_INTRINSIC_I("_fits_get_errstatus", get_errstatus, SLANG_VOID_TYPE),
   MAKE_INTRINSIC_3("_fits_open_file", open_file, I, R, S, S),
   MAKE_INTRINSIC_1("_fits_delete_file", delete_file, I, F),
   MAKE_INTRINSIC_1("_fits_close_file", close_file, SLANG_INT_TYPE, F),

   /* HDU Access Routines */
   MAKE_INTRINSIC_2("_fits_movabs_hdu", movabs_hdu, I, F, I),
   MAKE_INTRINSIC_2("_fits_movrel_hdu", movrel_hdu, I, F, I),
   MAKE_INTRINSIC_4("_fits_movnam_hdu", movnam_hdu, I, F, I, S, I),
   MAKE_INTRINSIC_2("_fits_get_num_hdus", get_num_hdus, I, F, R),
   MAKE_INTRINSIC_1("_fits_get_hdu_num", get_hdu_num, I, F),
   MAKE_INTRINSIC_2("_fits_get_hdu_type", get_hdu_type, I, F, R),
   MAKE_INTRINSIC_5("_fits_copy_file", copy_file, I, F, F, I, I, I),
   MAKE_INTRINSIC_3("_fits_copy_hdu", copy_hdu, I, F, F, I),
   MAKE_INTRINSIC_2("_fits_copy_header", copy_header, I, F, F),
   MAKE_INTRINSIC_1("_fits_delete_hdu", delete_hdu, I, F),
   
   MAKE_INTRINSIC_3("_fits_create_img", create_img, I, F, I, A),
   MAKE_INTRINSIC_2("_fits_write_img", write_img, I, F, A),
   MAKE_INTRINSIC_2("_fits_read_img", read_img, I, F, R),

   /* Keword Writing Routines */
   MAKE_INTRINSIC_0("_fits_create_binary_tbl", create_binary_tbl, I),
   MAKE_INTRINSIC_0("_fits_update_key", update_key, I),
   MAKE_INTRINSIC_0("_fits_update_logical", update_logical, I),
   MAKE_INTRINSIC_2("_fits_write_comment", write_comment, I, F, S),
   MAKE_INTRINSIC_2("_fits_write_history", write_history, I, F, S),
   MAKE_INTRINSIC_1("_fits_write_date", write_date, I, F),
   MAKE_INTRINSIC_2("_fits_write_record", &write_record, I, F, S),


   MAKE_INTRINSIC_3("_fits_modify_name", modify_name, I, F, S, S),
   MAKE_INTRINSIC_2("_fits_get_num_keys", get_num_keys, I, F, R),
   MAKE_INTRINSIC_0("_fits_read_key_integer", read_key_integer, I),
   MAKE_INTRINSIC_0("_fits_read_key_string", read_key_string, I),
   MAKE_INTRINSIC_0("_fits_read_key_double", read_key_double, I),
   MAKE_INTRINSIC_0("_fits_read_key", read_generic_key, I),
   MAKE_INTRINSIC_3("_fits_read_record", read_record, I, F, I, R),

   MAKE_INTRINSIC_2("_fits_delete_key", delete_key, I, F, S),
   MAKE_INTRINSIC_3("_fits_get_colnum", get_colnum, I, F, S, R),
   
   MAKE_INTRINSIC_3("_fits_insert_rows", insert_rows, I, F, I, I),
   MAKE_INTRINSIC_3("_fits_delete_rows", delete_rows, I, F, I, I),
   
   MAKE_INTRINSIC_4("_fits_insert_cols", insert_cols, I, F, I, A, A),
   MAKE_INTRINSIC_2("_fits_delete_col", delete_col, I, F, I),
   
   MAKE_INTRINSIC_2("_fits_get_num_cols", get_num_cols, I, F, R),
   MAKE_INTRINSIC_2("_fits_get_num_rows", get_num_rows, I, F, R),
   MAKE_INTRINSIC_5("_fits_write_col", write_col, I, F, I, I, I, A),
   MAKE_INTRINSIC_5("_fits_read_col", read_col, I, F, I, I, I, R),
   MAKE_INTRINSIC_3("_fits_get_keytype", get_keytype, I, F, S, R),
   MAKE_INTRINSIC_1("_fits_get_keyclass", get_keyclass, I, S),

   /* checksum routines */
   MAKE_INTRINSIC_1("_fits_write_chksum", write_chksum, I, F),
   MAKE_INTRINSIC_1("_fits_update_chksum", update_chksum, I, F),
   MAKE_INTRINSIC_3("_fits_verify_chksum", verify_chksum, I, F, R, R),
   MAKE_INTRINSIC_3("_fits_get_chksum", get_chksum, I, F, R, R),
   
   MAKE_INTRINSIC_0("_fits_get_version", get_version, SLANG_VOID_TYPE),
   SLANG_END_INTRIN_FUN_TABLE
};

static SLang_IConstant_Type IConst_Table [] =
{
   MAKE_ICONSTANT("_FITS_BINARY_TBL", BINARY_TBL),
   MAKE_ICONSTANT("_FITS_ASCII_TBL", ASCII_TBL),
   MAKE_ICONSTANT("_FITS_IMAGE_HDU", IMAGE_HDU),

   MAKE_ICONSTANT("_FITS_SAME_FILE",	SAME_FILE),
   MAKE_ICONSTANT("_FITS_TOO_MANY_FILES",	TOO_MANY_FILES),
   MAKE_ICONSTANT("_FITS_FILE_NOT_OPENED",	FILE_NOT_OPENED),
   MAKE_ICONSTANT("_FITS_FILE_NOT_CREATED",	FILE_NOT_CREATED),
   MAKE_ICONSTANT("_FITS_WRITE_ERROR",	WRITE_ERROR),
   MAKE_ICONSTANT("_FITS_END_OF_FILE",	END_OF_FILE),
   MAKE_ICONSTANT("_FITS_READ_ERROR",	READ_ERROR),
   MAKE_ICONSTANT("_FITS_FILE_NOT_CLOSED",	FILE_NOT_CLOSED),
   MAKE_ICONSTANT("_FITS_ARRAY_TOO_BIG",	ARRAY_TOO_BIG),
   MAKE_ICONSTANT("_FITS_READONLY_FILE",	READONLY_FILE),
   MAKE_ICONSTANT("_FITS_MEMORY_ALLOCATION",	MEMORY_ALLOCATION),
   MAKE_ICONSTANT("_FITS_BAD_FILEPTR",	BAD_FILEPTR),
   MAKE_ICONSTANT("_FITS_NULL_INPUT_PTR",	NULL_INPUT_PTR),
   MAKE_ICONSTANT("_FITS_SEEK_ERROR",	SEEK_ERROR),
   MAKE_ICONSTANT("_FITS_BAD_URL_PREFIX",	BAD_URL_PREFIX),
   MAKE_ICONSTANT("_FITS_TOO_MANY_DRIVERS",	TOO_MANY_DRIVERS),
   MAKE_ICONSTANT("_FITS_DRIVER_INIT_FAILED",	DRIVER_INIT_FAILED),
   MAKE_ICONSTANT("_FITS_NO_MATCHING_DRIVER",	NO_MATCHING_DRIVER),
   MAKE_ICONSTANT("_FITS_URL_PARSE_ERROR",	URL_PARSE_ERROR),
   MAKE_ICONSTANT("_FITS_HEADER_NOT_EMPTY",	HEADER_NOT_EMPTY),
   MAKE_ICONSTANT("_FITS_KEY_NO_EXIST",	KEY_NO_EXIST),
   MAKE_ICONSTANT("_FITS_KEY_OUT_BOUNDS",	KEY_OUT_BOUNDS),
   MAKE_ICONSTANT("_FITS_VALUE_UNDEFINED",	VALUE_UNDEFINED),
   MAKE_ICONSTANT("_FITS_NO_QUOTE",	NO_QUOTE),
   MAKE_ICONSTANT("_FITS_BAD_KEYCHAR",	BAD_KEYCHAR),
   MAKE_ICONSTANT("_FITS_BAD_ORDER",	BAD_ORDER),
   MAKE_ICONSTANT("_FITS_NOT_POS_INT",	NOT_POS_INT),
   MAKE_ICONSTANT("_FITS_NO_END",	NO_END),
   MAKE_ICONSTANT("_FITS_BAD_BITPIX",	BAD_BITPIX),
   MAKE_ICONSTANT("_FITS_BAD_NAXIS",	BAD_NAXIS),
   MAKE_ICONSTANT("_FITS_BAD_NAXES",	BAD_NAXES),
   MAKE_ICONSTANT("_FITS_BAD_PCOUNT",	BAD_PCOUNT),
   MAKE_ICONSTANT("_FITS_BAD_GCOUNT",	BAD_GCOUNT),
   MAKE_ICONSTANT("_FITS_BAD_TFIELDS",	BAD_TFIELDS),
   MAKE_ICONSTANT("_FITS_NEG_WIDTH",	NEG_WIDTH),
   MAKE_ICONSTANT("_FITS_NEG_ROWS",	NEG_ROWS),
   MAKE_ICONSTANT("_FITS_COL_NOT_FOUND",	COL_NOT_FOUND),
   MAKE_ICONSTANT("_FITS_BAD_SIMPLE",	BAD_SIMPLE),
   MAKE_ICONSTANT("_FITS_NO_SIMPLE",	NO_SIMPLE),
   MAKE_ICONSTANT("_FITS_NO_BITPIX",	NO_BITPIX),
   MAKE_ICONSTANT("_FITS_NO_NAXIS",	NO_NAXIS),
   MAKE_ICONSTANT("_FITS_NO_NAXES",	NO_NAXES),
   MAKE_ICONSTANT("_FITS_NO_XTENSION",	NO_XTENSION),
   MAKE_ICONSTANT("_FITS_NOT_ATABLE",	NOT_ATABLE),
   MAKE_ICONSTANT("_FITS_NOT_BTABLE",	NOT_BTABLE),
   MAKE_ICONSTANT("_FITS_NO_PCOUNT",	NO_PCOUNT),
   MAKE_ICONSTANT("_FITS_NO_GCOUNT",	NO_GCOUNT),
   MAKE_ICONSTANT("_FITS_NO_TFIELDS",	NO_TFIELDS),
   MAKE_ICONSTANT("_FITS_NO_TBCOL",	NO_TBCOL),
   MAKE_ICONSTANT("_FITS_NO_TFORM",	NO_TFORM),
   MAKE_ICONSTANT("_FITS_NOT_IMAGE",	NOT_IMAGE),
   MAKE_ICONSTANT("_FITS_BAD_TBCOL",	BAD_TBCOL),
   MAKE_ICONSTANT("_FITS_NOT_TABLE",	NOT_TABLE),
   MAKE_ICONSTANT("_FITS_COL_TOO_WIDE",	COL_TOO_WIDE),
   MAKE_ICONSTANT("_FITS_COL_NOT_UNIQUE",	COL_NOT_UNIQUE),
   MAKE_ICONSTANT("_FITS_BAD_ROW_WIDTH",	BAD_ROW_WIDTH),
   MAKE_ICONSTANT("_FITS_UNKNOWN_EXT",	UNKNOWN_EXT),
   MAKE_ICONSTANT("_FITS_UNKNOWN_REC",	UNKNOWN_REC),
   MAKE_ICONSTANT("_FITS_END_JUNK",	END_JUNK),
   MAKE_ICONSTANT("_FITS_BAD_HEADER_FILL",	BAD_HEADER_FILL),
   MAKE_ICONSTANT("_FITS_BAD_DATA_FILL",	BAD_DATA_FILL),
   MAKE_ICONSTANT("_FITS_BAD_TFORM",	BAD_TFORM),
   MAKE_ICONSTANT("_FITS_BAD_TFORM_DTYPE",	BAD_TFORM_DTYPE),
   MAKE_ICONSTANT("_FITS_BAD_TDIM",	BAD_TDIM),
   MAKE_ICONSTANT("_FITS_BAD_HDU_NUM",	BAD_HDU_NUM),
   MAKE_ICONSTANT("_FITS_BAD_COL_NUM",	BAD_COL_NUM),
   MAKE_ICONSTANT("_FITS_NEG_FILE_POS",	NEG_FILE_POS),
   MAKE_ICONSTANT("_FITS_NEG_BYTES",	NEG_BYTES),
   MAKE_ICONSTANT("_FITS_BAD_ROW_NUM",	BAD_ROW_NUM),
   MAKE_ICONSTANT("_FITS_BAD_ELEM_NUM",	BAD_ELEM_NUM),
   MAKE_ICONSTANT("_FITS_NOT_ASCII_COL",	NOT_ASCII_COL),
   MAKE_ICONSTANT("_FITS_NOT_LOGICAL_COL",	NOT_LOGICAL_COL),
   MAKE_ICONSTANT("_FITS_BAD_ATABLE_FORMAT",	BAD_ATABLE_FORMAT),
   MAKE_ICONSTANT("_FITS_BAD_BTABLE_FORMAT",	BAD_BTABLE_FORMAT),
   MAKE_ICONSTANT("_FITS_NO_NULL",	NO_NULL),
   MAKE_ICONSTANT("_FITS_NOT_VARI_LEN",	NOT_VARI_LEN),
   MAKE_ICONSTANT("_FITS_BAD_DIMEN",	BAD_DIMEN),
   MAKE_ICONSTANT("_FITS_BAD_PIX_NUM",	BAD_PIX_NUM),
   MAKE_ICONSTANT("_FITS_ZERO_SCALE",	ZERO_SCALE),
   MAKE_ICONSTANT("_FITS_NEG_AXIS",	NEG_AXIS),
   
   /* get_keyclass return value */
   MAKE_ICONSTANT("_FITS_TYP_STRUC_KEY",TYP_STRUC_KEY),
   MAKE_ICONSTANT("_FITS_TYP_CMPRS_KEY",TYP_CMPRS_KEY),
   MAKE_ICONSTANT("_FITS_TYP_SCAL_KEY",	TYP_SCAL_KEY),
   MAKE_ICONSTANT("_FITS_TYP_NULL_KEY",	TYP_NULL_KEY),
   MAKE_ICONSTANT("_FITS_TYP_DIM_KEY",	TYP_DIM_KEY),
   MAKE_ICONSTANT("_FITS_TYP_RANG_KEY",	TYP_RANG_KEY),
   MAKE_ICONSTANT("_FITS_TYP_UNIT_KEY",	TYP_UNIT_KEY),
   MAKE_ICONSTANT("_FITS_TYP_DISP_KEY",	TYP_DISP_KEY),
   MAKE_ICONSTANT("_FITS_TYP_HDUID_KEY",TYP_HDUID_KEY),
   MAKE_ICONSTANT("_FITS_TYP_CKSUM_KEY",TYP_CKSUM_KEY),
   MAKE_ICONSTANT("_FITS_TYP_WCS_KEY",	TYP_WCS_KEY),
   MAKE_ICONSTANT("_FITS_TYP_REFSYS_KEY",TYP_REFSYS_KEY),
   MAKE_ICONSTANT("_FITS_TYP_COMM_KEY",	TYP_COMM_KEY),
   MAKE_ICONSTANT("_FITS_TYP_CONT_KEY",	TYP_CONT_KEY),
   MAKE_ICONSTANT("_FITS_TYP_USER_KEY",	TYP_USER_KEY),

   
   MAKE_ICONSTANT("_cfitsio_module_version", FITS_MODULE_VERSION),
   
   SLANG_END_ICONST_TABLE
};

static SLang_Intrin_Var_Type Intrin_Vars[] =
{
   MAKE_VARIABLE("_cfitsio_module_version_string", &Version_String, SLANG_STRING_TYPE, 1),
   SLANG_END_INTRIN_VAR_TABLE
};

static void patchup_intrinsic_table (void)
{
   SLang_Intrin_Fun_Type *f;
   
   f = Fits_Intrinsics;
   while (f->name != NULL)
     {
	unsigned int i, nargs;
	unsigned char *args;
	
	nargs = f->num_args;
	args = f->arg_types;
	for (i = 0; i < nargs; i++)
	  {
	     if (args[i] == DUMMY_FITS_FILE_TYPE)
	       args[i] = (unsigned char) Fits_Type_Id;
	  }
	
	/* For completeness */
	if (f->return_type == DUMMY_FITS_FILE_TYPE)
	  f->return_type = (unsigned char) Fits_Type_Id;

	f++;
     }
}


static void free_fits_file_type (unsigned char type, VOID_STAR f)
{
   FitsFile_Type *ft;
   int status = 0;
   
   (void) type;

   ft = (FitsFile_Type *) f;
   if (ft->fptr != NULL)
     fits_close_file (ft->fptr, &status);

   SLfree ((char *) ft);
}


int init_cfitsio_module_ns (char *ns_name)
{
   SLang_Class_Type *cl;
   SLang_NameSpace_Type *ns;
   
   ns = SLns_create_namespace (ns_name);
   if (ns == NULL)
     return -1;

   cl = SLclass_allocate_class ("Fits_File_Type");
   if (cl == NULL) return -1;
   (void) SLclass_set_destroy_function (cl, free_fits_file_type);
   
   /* By registering as SLANG_VOID_TYPE, slang will dynamically allocate a
    * type.
    */
   if (-1 == SLclass_register_class (cl, SLANG_VOID_TYPE,
				     sizeof (FitsFile_Type), 
				     SLANG_CLASS_TYPE_MMT))
     return -1;

   Fits_Type_Id = SLclass_get_class_id (cl);
   patchup_intrinsic_table ();

   if (-1 == SLns_add_intrin_fun_table (ns, Fits_Intrinsics, "__CFITSIO__"))
     return -1;
   
   if (-1 == SLns_add_iconstant_table (ns, IConst_Table, NULL))
     return -1;
   
   if (-1 == SLadd_intrin_var_table (Intrin_Vars, NULL))
     return -1;
   
   return 0;
}
