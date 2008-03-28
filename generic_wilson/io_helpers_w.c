/********************** io_helpers_w.c **********************************/
/* MIMD version 7 */
/* CD 10/97 from io_helpers.c DT 8/97 
   General purpose high level propagator I/O routines, 
   to be used by any application that wants them.
   */

#include "generic_wilson_includes.h"
#include "../include/io_lat.h"
#include "../include/io_wprop.h"
#include "../include/file_types.h"
#include <string.h>
#ifdef HAVE_QIO
#include <qio.h>
#include "../include/io_scidac.h"
#include "../include/io_scidac_w.h"
#endif

#ifdef HAVE_QIO
/*---------------------------------------------------------------*/
/* Set up a USQCD Wilson propagator file for reading */

static void
setup_input_usqcd_prop_file(w_prop_file *wpf)
{

  /* Open the file and read the header */
  wpf->infile = r_open_usqcd_wprop_file(wpf->filename,QIO_SERIAL);
} /* setup_input_usqcd_prop_file */

/*---------------------------------------------------------------*/
/* Read a USQCD Wilson propagator source and record according to file type */
static int 
read_usqcd_wprop_record(w_prop_file *wpf, 
			int spin, int color, wilson_vector *dest,
			wilson_quark_source *wqs )
{
  int status = 0;
  int input_spin, input_color;
  char myname[] = "read_usqcd_wprop_record";
  int file_type = wpf->file_type;

  if(file_type == FILE_TYPE_W_USQCD_CD_PAIRS || 
     (file_type == FILE_TYPE_W_USQCD_C1D12 && spin == 0 && color == 0)){
    /* Read a complex source field */
    alloc_wqs_c_src(wqs);
    status = qio_status(read_wpropsource_C_usqcd(wpf->infile, wqs->descrp, 
						 MAXDESCRP, wqs->c_src));
    if(status == 0)node0_printf("Read prop source %s\n",wqs->descrp);
    wqs->type = COMPLEX_FIELD_STORE;
  }
  else if(file_type == FILE_TYPE_W_USQCD_DD_PAIRS){
    /* Read a Wilson vector source field */
    alloc_wqs_wv_src(wqs);
    status = qio_status(read_wpropsource_D_usqcd(wpf->infile, wqs->descrp, 
						MAXDESCRP, wqs->wv_src));
    if(status == 0)node0_printf("Read prop source %s\n",wqs->descrp);
    wqs->type = DIRAC_FIELD_STORE;
  }

  /* Next, read the solution vector */
  if(status == 0)
    status = qio_status(read_wproprecord_usqcd(wpf->infile, 
					      &input_spin, &input_color, 
					      dest));

  /* Check spin and color */
  if(status == 0 && (input_spin != spin || input_color != color)){
    node0_printf("%s: input spin %d and color %d do not match requested spin %d and color %d\n",
		 myname, input_spin, input_color, spin, color);
    status = 1;
  }

  return status;
} /* read_usqcd_wprop_record */

/*---------------------------------------------------------------*/
/* Get the SciDAC format and mode parameters from the MILC I/O flag */
void 
interpret_usqcd_w_save_flag(int *volfmt, int *serpar, int flag)
{
  switch(flag){
  case SAVE_PARALLEL_SCIDAC:   
    *serpar = QIO_PARALLEL;
    break;
  default:
    *serpar = QIO_SERIAL;
  }
  
  switch(flag){
  case SAVE_MULTIFILE_SCIDAC: 
    *volfmt = QIO_MULTIFILE;
    break;
  case SAVE_PARTITION_SCIDAC:
    *volfmt = QIO_PARTFILE;
    break;
  default:
    *volfmt = QIO_SINGLEFILE;
  }
} /* interpret_usqcd_w_save_flag */

#endif  

/*---------------------------------------------------------------*/
/* read the lattice dimensions from a binary Wilson prop file */

int 
read_lat_dim_wprop(char *filename, int file_type, int *ndim, int dims[])
{
  w_prop_file *wpf;
  int i;

  switch(file_type){
  case FILE_TYPE_W_FMPROP:
    *ndim = 4;
    nx = -1; ny = -1; nz = -1; nt = -1;
    wpf = r_serial_w_fm_i(filename);
    for(i = 0; i < *ndim; i++)
      dims[i] = wpf->header->dims[i];
    r_serial_w_fm_f(wpf);
    break;

  case FILE_TYPE_W_USQCD_C1D12:
  case FILE_TYPE_W_USQCD_DD_PAIRS:
  case FILE_TYPE_W_USQCD_CD_PAIRS:
  case FILE_TYPE_W_USQCD_LHPC:
#ifdef HAVE_QIO
    read_lat_dim_scidac(filename, ndim, dims);
#else
    node0_printf("read_lat_dim_wprop(%d): This looks like a QIO file, but to read it requires QIO compilation\n", this_node);
    return 1;
#endif
    break;
  default:
    node0_printf("read_lat_dim_wprop(%d): Unsupported file type %d\n",
		 this_node, file_type);
    return 1;
  }
  return 0;
} /* read_lat_dim_wprop */

/*---------------------------------------------------------------*/
/* Open Wilson propagator file for reading one source spin-color at a time */

w_prop_file *
r_open_wprop(int flag, char *filename)
{
  w_prop_file *wpf = NULL;
  int file_type;
  wilson_propagator *wp;
  char myname[] = "r_open_wprop";

  if(flag == RELOAD_ASCII){
    wpf = r_ascii_w_i(filename);
    return wpf;
  }

  /* No file */
  if(flag == FRESH)return NULL;

  /* Interpret non-ASCII file type */
  file_type = get_file_type(filename);
  if(file_type == FILE_TYPE_UNKNOWN){
    node0_printf("%s: unrecognized type file %s\n", myname, filename);
    return NULL;
  }

  /* If it is an FNAL propagator file, read the full prop and cache
     it.  (We have to do a spin rotation on source and sink spins to
     convert to MILC spin conventions.) */

  /* There are two types of Fermilab Wilson propagator files.  One is
     sorted by source color and spin, so the site datum is a Dirac
     vector.  The other has the full propagator on each site. */

  if(file_type == FILE_TYPE_W_FMPROP){
    if(flag == RELOAD_PARALLEL){
      node0_printf("%s: Can't read FNAL files in parallel\n",myname);
      node0_printf("Reading serially instead.\n");
    }
    wpf = r_serial_w_fm_i(filename);
    wpf->file_type = file_type;
    wpf->prop = (wilson_propagator *)
      malloc(sites_on_node*sizeof(wilson_propagator));
    wp = wpf->prop;
    if(wp == NULL){
      printf("%s(%d): Can't malloc for full input propagator\n",
	     myname, this_node);
      terminate(1);
    }
    /* For either FNAL format we read the entire propagator */
    r_serial_w_fm_to_field(wpf, wp);
    
    /* Convert from FNAL to MILC spin basis */
    convert_wprop_fnal_to_milc_field(wp);
    
    /* Indicate propagator data has been read and cached */
    wpf->file_type = file_type = FILE_TYPE_W_STORE;
  }
#ifdef HAVE_QIO
  /* Other SciDAC propagator formats */

  else if(file_type == FILE_TYPE_W_USQCD_C1D12 ||
	  file_type == FILE_TYPE_W_USQCD_DD_PAIRS ||
	  file_type == FILE_TYPE_W_USQCD_CD_PAIRS){
    
    wpf = setup_input_w_prop_file(filename);
    wpf->file_type = file_type;
    setup_input_usqcd_prop_file(wpf);
  }
#endif

  return wpf;
} /* r_open_wprop */

/*---------------------------------------------------------------*/
/* Open propagator file for writing a Wilson vector for one
   source spin and color at a time. */

w_prop_file *
w_open_wprop(int flag, char *filename, int source_type)
{
  w_prop_file *wpf = NULL;
  wilson_propagator *wp;
#ifdef HAVE_QIO
  char *fileinfo;
  int volfmt, serpar, file_type;
#endif

  if(flag == SAVE_ASCII){
    wpf = w_ascii_w_i(filename);
    return wpf;
  }

  switch(flag){
  case SAVE_SERIAL_FM:
    wpf = w_serial_w_fm_i(filename);
    /* Allocate space for the entire propagator so we can do the spin
       basis conversion. */
    wpf->prop = (wilson_propagator *)
      malloc(sites_on_node*sizeof(wilson_propagator));
    wp = wpf->prop;
    if(wp == NULL){
      printf("Can't malloc for full output propagator\n");
      terminate(1);
    }
    break;

  case SAVE_SERIAL_FM_SC:
    wpf = w_serial_w_fm_sc_i(filename);
    /* Allocate space for the entire propagator so we can do the spin
       basis conversion. */
    wpf->prop = (wilson_propagator *)
      malloc(sites_on_node*sizeof(wilson_propagator));
    wp = wpf->prop;
    if(wp == NULL){
      printf("Can't malloc for full output propagator\n");
      terminate(1);
    }
    break;

  case SAVE_SERIAL_SCIDAC:
  case SAVE_PARALLEL_SCIDAC:   
  case SAVE_MULTIFILE_SCIDAC: 
  case SAVE_PARTITION_SCIDAC:

#ifdef HAVE_QIO
    wpf = setup_output_w_prop_file();
    interpret_usqcd_w_save_flag(&volfmt, &serpar, flag);
    file_type = choose_usqcd_w_file_type(source_type);
    wpf->file_type = file_type;
    fileinfo = create_w_QCDML();
    wpf->outfile = w_open_usqcd_wprop_file(filename, volfmt, serpar, QIO_ILDGNO,
					   NULL, file_type, fileinfo);
    free_w_QCDML(fileinfo);
    
#else
    node0_printf("w_open_wprop: SciDAC formats require QIO compilation\n");
    terminate(1);
#endif
    break;

  default:
    wpf = NULL;
  }

  return wpf;
} /* w_open_wprop */

/*---------------------------------------------------------------*/
void 
r_close_wprop(int flag, w_prop_file *wpf)
{

  if(wpf == NULL)return;

  switch(flag){
  case RELOAD_ASCII:
    r_ascii_w_f(wpf);
    break;
  case RELOAD_SERIAL:
  case RELOAD_PARALLEL:
    if(wpf->prop != NULL)
      free(wpf->prop); wpf->prop = NULL;
    clear_input_w_prop_file(wpf);
    break;
  default:
    node0_printf("r_close_wprop: Unrecognized read flag %d",flag);
  }
}
/*---------------------------------------------------------------*/
void 
w_close_wprop(int flag, w_prop_file *wpf)
{
  wilson_propagator *wp;

  if(wpf == NULL)return;

  switch(flag){
  case FORGET:
    break;
  case SAVE_ASCII:
    w_ascii_w_f(wpf);
    break;
  case SAVE_SERIAL_FM:
  case SAVE_SERIAL_FM_SC:
    /* Dump accumulated propagator and free memory */
    wp = wpf->prop;
    if(wp != NULL){
      /* Convert from MILC to FNAL */
      convert_wprop_milc_to_fnal_field(wp);
      w_serial_w_fm_from_field(wpf, wp);
      free(wp);  wp = NULL;
    }
    w_serial_w_fm_f(wpf); 
    break;
  case SAVE_SERIAL_SCIDAC:
  case SAVE_PARALLEL_SCIDAC:   
  case SAVE_MULTIFILE_SCIDAC: 
  case SAVE_PARTITION_SCIDAC:
#ifdef HAVE_QIO
    w_close_usqcd_wprop_file(wpf->outfile);
    if(wpf->prop != NULL)
      free(wpf->prop); 
    free(wpf);
#endif
    break;
  default:
    node0_printf("w_close_wprop: Unrecognized save flag %d\n",flag);
  }
}

/*---------------------------------------------------------------*/
/* reload a Wilson vector field for a single source color and spin in
   FNAL or USQCD format, or cold propagator, or keep current
   propagator: FRESH, CONTINUE, RELOAD_ASCII, RELOAD_SERIAL,
   RELOAD_PARALLEL

   Return value is 

  -1 end of file
   0 normal exit value
   1 read error
*/

int 
reload_wprop_sc_to_field( int flag, w_prop_file *wpf, 
			  wilson_quark_source *wqs, int spin, int color, 
			  wilson_vector *dest, int timing)
{

  double dtime = 0;
  int i,status;
  site *s;
  wilson_propagator *wp;
  wilson_vector *wv;
  int s0, c0;
  int file_type = FILE_TYPE_UNKNOWN;  /* So the compiler doesn't say uninit */
  char myname[] = "reload_wprop_sc_to_field";

  if(timing)dtime = -dclock();
  status = 0;
  switch(flag){
  case CONTINUE:  /* do nothing */
    break;
  case FRESH:     /* zero initial guess */
    FORALLSITES(i,s)clear_wvec( &(dest[i]) );
    break;
  case RELOAD_ASCII:
    node0_printf("Reloading ASCII to wprop field not supported\n");
    terminate(1);
    break;
  case RELOAD_SERIAL:
    wp = wpf->prop;
    file_type = wpf->file_type;
    /* Special treatment for a cached propagator */
    if(file_type == FILE_TYPE_W_STORE){
    /* Copy input Wilson vector for this color and spin from buffer */
      FORALLSITES(i,s){
	wv = dest + i;
	for(s0=0;s0<4;s0++)for(c0=0;c0<3;c0++)
	  {
	    wv->d[s0].c[c0].real = wp[i].c[color].d[spin].d[s0].c[c0].real;
	    wv->d[s0].c[c0].imag = wp[i].c[color].d[spin].d[s0].c[c0].imag;
	  }
      }
      status = 0;
    }
#ifdef HAVE_QIO
    else if(file_type == FILE_TYPE_W_USQCD_C1D12 ||
	    file_type == FILE_TYPE_W_USQCD_DD_PAIRS ||
	    file_type == FILE_TYPE_W_USQCD_CD_PAIRS){

      /* Read the propagator record */
      status = read_usqcd_wprop_record(wpf, spin, color, dest, wqs);
    }
#endif
    else {
      node0_printf("%s: File type not supported\n",myname);
    }
    break;
  case RELOAD_PARALLEL:
#ifdef HAVE_QIO
    if(file_type == FILE_TYPE_W_USQCD_C1D12 ||
       file_type == FILE_TYPE_W_USQCD_DD_PAIRS ||
       file_type == FILE_TYPE_W_USQCD_CD_PAIRS){
      status = read_usqcd_wprop_record(wpf, spin, color, dest, wqs);
    }
#endif
    /* If not SciDAC */
    if(file_type != FILE_TYPE_W_USQCD_C1D12 &&
       file_type != FILE_TYPE_W_USQCD_DD_PAIRS &&
       file_type != FILE_TYPE_W_USQCD_CD_PAIRS){
      node0_printf("%s: Parallel reading with this file type not supported\n",
		   myname);
    }
    break;
  default:
    node0_printf("%s: Unrecognized reload flag.\n", myname);
    terminate(1);
  }
  
  if(timing)
    {
      dtime += dclock();
      if(flag != FRESH && flag != CONTINUE)
	node0_printf("Time to reload wprop spin %d color %d %e\n",
		     spin,color,dtime);
    }

  return status;

} /* reload_wprop_sc_to_field */

/*---------------------------------------------------------------*/
/* save a propagator one source color and spin at a time */
/* recinfo is for USQCD formats */
int 
save_wprop_sc_from_field( int flag, w_prop_file *wpf, 
			  wilson_quark_source *wqs,
			  int spin, int color, wilson_vector *src, 
			  char *recinfo, int timing)
{
  double dtime = 0;
  int status;
  int i; site *s;
  wilson_propagator *wp;
  wilson_vector *wv;
  int s0, c0;
#ifdef HAVE_QIO
  int file_type;
#endif
  char myname[] = "save_wprop_sc_from_field";
  
  if(timing)dtime = -dclock();
  status = 0;
  switch(flag){
  case FORGET:
    break;
  case SAVE_ASCII:
    node0_printf("Reading to field from ASCII is not supported\n");
    terminate(1);
    break;
  case SAVE_SERIAL_FM:
  case SAVE_SERIAL_FM_SC:
    wp = wpf->prop;
    if(wp == NULL){
      printf("%s{%d): Propagator field not allocated\n", myname, this_node);
      terminate(1);
    }
    /* Add output Wilson vector to propagator buffer */
    FORALLSITES(i,s){
      wv = src + i;
      for(s0=0;s0<4;s0++)for(c0=0;c0<3;c0++)
	{
	  wp[i].c[color].d[spin].d[s0].c[c0].real = wv->d[s0].c[c0].real;
	  wp[i].c[color].d[spin].d[s0].c[c0].imag = wv->d[s0].c[c0].imag;
	}
    }
    break;
  case SAVE_SERIAL_SCIDAC:
  case SAVE_PARALLEL_SCIDAC:   
  case SAVE_MULTIFILE_SCIDAC: 
  case SAVE_PARTITION_SCIDAC:

#ifdef HAVE_QIO
    file_type = wpf->file_type;
    /* Save color source field */
    if(file_type == FILE_TYPE_W_USQCD_CD_PAIRS ||
       (file_type == FILE_TYPE_W_USQCD_C1D12 && spin == 0 && color == 0))
      {
	/* If we don't have a complex source, yet, get it */
	if(wqs->c_src == NULL){
	  wilson_vector *wvtmp = (wilson_vector *)malloc(sizeof(wilson_vector)*sites_on_node);
	  if(wvtmp == NULL){
	    printf("%s(%d) no room for tmp\n",myname,this_node);
	    terminate(1);
	  }
	  w_source_field(wvtmp, wqs);
	  free(wvtmp);
	}
	status = write_wpropsource_C_usqcd(wpf->outfile, wqs->descrp, 
					   wqs->c_src, wqs->t0);
      }
    /* Save Dirac source field */
    else if(file_type == FILE_TYPE_W_USQCD_DD_PAIRS){
	if(wqs->wv_src == NULL){
	  wilson_vector *wvtmp = (wilson_vector *)malloc(sizeof(wilson_vector)*sites_on_node);
	  if(wvtmp == NULL){
	    printf("%s(%d) no room for tmp\n",myname,this_node);
	    terminate(1);
	  }
	  w_source_field(wvtmp, wqs);
	  free(wvtmp);
	}
      status = write_wpropsource_D_usqcd(wpf->outfile, wqs->descrp, 
					 wqs->wv_src, wqs->t0);
      if(status != 0)break;
    }
    /* Save solution field */
    if(status == 0)
      status = write_prop_usqcd_sc(wpf->outfile, src, spin, color, recinfo);
#else
    node0_printf("%s: SciDAC formats require QIO compilation\n",myname);
    terminate(1);
#endif
    
    break;
  default:
    node0_printf("%s: Unrecognized save flag.\n", myname);
    terminate(1);
  }
  
  if(timing)
    {
      dtime += dclock();
      if(flag != FORGET)
	node0_printf("Time to save prop spin %d color %d = %e\n",
		     spin,color,dtime);
    }
  return status;
} /* save_wprop_sc_from_field */

/*---------------------------------------------------------------*/
/* Reload a spin_wilson_vector field for a single source color and 4
   source spins in FNAL or USQCD format, or cold propagator, or keep
   current propagator: FRESH, CONTINUE, RELOAD_ASCII, RELOAD_SERIAL,
   RELOAD_PARALLEL

   Also reads a staggered propagator and converts to naive.

   Return value is 

  -1 end of file
   0 normal exit value
   1 read error
*/

int 
reload_wprop_c_to_field( int flag, w_prop_file *wpf, 
			 wilson_quark_source *wqs, int spin, int color, 
			 spin_wilson_vector *dest, int timing)
{

  double dtime = 0;
  int i,status;
  site *s;
  wilson_propagator *wp;
  spin_wilson_vector *swv;
  wilson_vector *wv;
  int s0, s1;
  int file_type = FILE_TYPE_UNKNOWN;  /* So the compiler doesn't say uninit */
  char myname[] = "reload_wprop_sc_to_field";

  if(timing)dtime = -dclock();
  status = 0;
  switch(flag){
  case CONTINUE:  /* do nothing */
    break;
  case FRESH:     /* zero initial guess */
    for(s0 = 0; s0 < 4; s0++)
      FORALLSITES(i,s)clear_wvec( &(dest[i].d[s0]) );
    break;
  case RELOAD_ASCII:
    node0_printf("Reloading ASCII to wprop field not supported\n");
    terminate(1);
    break;
  case RELOAD_SERIAL:
  case RELOAD_PARALLEL:
    file_type = wpf->file_type;
    /* Treatment for a cached propagator */
    /* An FNAL propagator should have been read and cached already */
    if(file_type == FILE_TYPE_W_STORE){
      /* Copy input spin Wilson vector for this color from buffer */
      wp = wpf->prop;
      FORALLSITES(i,s){
	swv = dest + i;
	for(s1=0;s1<4;s1++)
	  copy_wvec(&wp[i].c[color].d[s1],&swv->d[s1]);
      }
      status = 0;
    }
#ifdef HAVE_QIO
    else if(file_type == FILE_TYPE_W_USQCD_C1D12 ||
	    file_type == FILE_TYPE_W_USQCD_DD_PAIRS ||
	    file_type == FILE_TYPE_W_USQCD_CD_PAIRS){

      /* Read the propagator record for four source spins */
      wv = (wilson_vector *)malloc(sites_on_node * sizeof(wilson_vector));
      if(wv == NULL){
	printf("reload_wprop_c_to_field(%d): No room for wv\n",this_node);
	terminate(1);
      }
      for(s1=0;s1<4;s1++){
	status = read_usqcd_wprop_record(wpf, s1, color, wv, wqs);
	FORALLSITES(i,s){
	  copy_wvec(&wv[i],&dest[i].d[s1]);
	}
      }
      free(wv);
    }
#endif
    else {
      node0_printf("%s: File type not supported\n",myname);
    }
    break;
  default:
    node0_printf("%s: Unrecognized reload flag.\n", myname);
    terminate(1);
  }
  
  if(timing)
    {
      dtime += dclock();
      if(flag != FRESH && flag != CONTINUE)
	node0_printf("Time to reload wprop spin %d color %d %e\n",
		     spin,color,dtime);
    }

  return status;

} /* reload_wprop_sc_to_field */

/*---------------------------------------------------------------*/
/* Reload a full propagator (3 source colors and 4 source spins) in
   most of the formats, or cold propagator, or keep current propagator.
   Destination field is a wilson_propagator

   FRESH, CONTINUE,
   RELOAD_ASCII, RELOAD_SERIAL, RELOAD_PARALLEL, RELOAD_MULTIDUMP

   Return value is 

  -1 end of file
   0 normal exit value
   1 read error

*/
int 
reload_wprop_to_field( int flag, char *filename, wilson_quark_source *wqs,
		       wilson_propagator *dest, int timing)
{

  double dtime = 0;
  int i,status;
  site *s;
  wilson_propagator *wp;
  int spin, color;
  w_prop_file *wpf;
  wilson_vector *psi;
  char myname[] = "reload_wprop_to_field";
  
  status = 0;
  switch(flag){
    
  case CONTINUE:  /* do nothing */
    break;
    
  case FRESH:     /* zero initial guess */
    FORALLSITES(i,s){
      wp = dest + i;
      for(color = 0; color < 3; color++)
	for(spin = 0; spin < 4; spin++)
	  clear_wvec( &wp->c[color].d[spin] );
    }
    break;
    
  case RELOAD_ASCII:
    node0_printf("reload_wprop_to_field: ASCII to field not supported\n");
    terminate(1);
    break;
  case RELOAD_SERIAL:
  case RELOAD_PARALLEL:
    psi = (wilson_vector *)malloc(sites_on_node*sizeof(wilson_vector));
    if(psi == NULL){
      printf("%s(%d): Can't allocate psi\n",myname,this_node);
      terminate(1);
    }
    
    wpf = r_open_wprop(flag, filename);
    
    /* Loop over source spins */
    for(spin=0;spin<4;spin++){
      /* Loop over source colors */
      for(color=0;color<3;color++){
	reload_wprop_sc_to_field(flag, wpf, wqs, spin, color, psi, timing);
	FORALLSITES(i,s){
	  copy_wvec(&dest[i].c[color].d[spin], psi);
	}
      }
    }
    
    r_close_wprop(flag, wpf);
    
    free(psi);
    
    break;
    
  default:
    node0_printf("%s: Bad reload flag\n",myname);
  }
    
  if(timing)
    {
      dtime += dclock();
      if(flag != FRESH && flag != CONTINUE)
	node0_printf("Time to reload wprop %e\n",dtime);
    }
  
  return status;
  
} /* reload_wprop_to_field */

/*---------------------------------------------------------------*/
/* Reload a full propagator (3 source colors and 4 source spins) in
   most of the formats, or cold propagator, or keep current propagator.
   Destination field is a wilson_prop_field.

   FRESH, CONTINUE,
   RELOAD_ASCII, RELOAD_SERIAL, RELOAD_PARALLEL, RELOAD_MULTIDUMP

   Return value is 

  -1 end of file
   0 normal exit value
   1 read error

*/
int 
reload_wprop_to_wp_field( int flag, char *filename, wilson_quark_source *wqs,
			  wilson_prop_field dest, int timing)
{

  double dtime = 0;
  int status;
  int spin, color;
  w_prop_file *wpf;
  wilson_vector *psi;
  char myname[] = "reload_wprop_to_field";
  
  status = 0;
  switch(flag){
    
  case CONTINUE:  /* do nothing */
    break;
    
  case FRESH:     /* zero initial guess */
    clear_wp_field(dest);
    break;
    
  case RELOAD_ASCII:
    node0_printf("reload_wprop_to_field: ASCII to field not supported\n");
    terminate(1);
    break;
  case RELOAD_SERIAL:
  case RELOAD_PARALLEL:
    psi = create_wv_field();
    
    wpf = r_open_wprop(flag, filename);
    
    /* Loop over source spins */
    for(spin=0;spin<4;spin++){
      /* Loop over source colors */
      for(color=0;color<3;color++){
	reload_wprop_sc_to_field(flag, wpf, wqs, spin, color, psi, timing);
	copy_wp_from_wv(dest, psi, color, spin);
      }
    }
    
    r_close_wprop(flag, wpf);
    
    destroy_wv_field(psi);
    
    break;
    
  default:
    node0_printf("%s: Bad reload flag\n",myname);
  }
    
  if(timing)
    {
      dtime += dclock();
      if(flag != FRESH && flag != CONTINUE)
	node0_printf("Time to reload wprop %e\n",dtime);
    }
  
  return status;
  
} /* reload_wprop_to_wp_field */

/*---------------------------------------------------------------*/
/* save the full propagator (src is wilson_propagator type)
   FORGET,
   SAVE_ASCII, 
   SAVE_SERIAL_SCIDAC, SAVE_PARALLEL_SCIDAC, SAVE_PARTITION_SCIDAC,
   SAVE_MULTFILE_SCIDAC
*/
int 
save_wprop_from_field( int flag, char *filename, wilson_quark_source *wqs,
		       wilson_propagator *src, char *recxml, int timing)
{
  int spin, color;
  w_prop_file *wpf;
  wilson_vector *wv;
  int status;
  
  status = 0;

  if(flag == FORGET)return status;
  
  wv = create_wv_field();
  wpf = w_open_wprop(flag, filename, wqs->type);

  for(spin = 0; spin < 4; spin++)
    for(color = 0; color < 3; color++)
      {
	copy_wv_from_wprop(wv, src, color, spin);
	if( save_wprop_sc_from_field (flag, wpf, wqs, spin, color, 
				      wv, recxml, timing) != 0)
	  status = 1;
      }

  w_close_wprop(flag, wpf);
  destroy_wv_field(wv);

  return status;

} /* save_wprop_from_field */

/*---------------------------------------------------------------*/
/* save the full propagator (src is wilson_prop_field type)
   FORGET,
   SAVE_ASCII, 
   SAVE_SERIAL_SCIDAC, SAVE_PARALLEL_SCIDAC, SAVE_PARTITION_SCIDAC,
   SAVE_MULTFILE_SCIDAC
*/
int 
save_wprop_from_wp_field( int flag, char *filename, wilson_quark_source *wqs,
			  wilson_prop_field src, char *recxml, int timing)
{
  int spin, color;
  w_prop_file *wpf;
  wilson_vector *wv;
  int status;
  
  status = 0;

  if(flag == FORGET)return status;
  
  wv = create_wv_field();
  wpf = w_open_wprop(flag, filename, wqs->type);

  for(color = 0; color < 3; color++)
    for(spin = 0; spin < 4; spin++)
      {
	copy_wv_from_wp(wv, src, color, spin);
	if( save_wprop_sc_from_field (flag, wpf, wqs, spin, color, 
				      wv, recxml, timing) != 0)
	  status = 1;
      }

  w_close_wprop(flag, wpf);
  destroy_wv_field(wv);

  return status;

} /* save_wprop_from_field */

/*---------------------------------------------------------------*/
/* Temporary procedure to support legacy applications that read to the
   site structure */
/* reload a propagator for a single source color and spin

   Return value is 

  -1 end of file
   0 normal exit value
   1 read error
*/

int 
reload_wprop_sc_to_site( int flag, w_prop_file *wpf,
			 wilson_quark_source *wqs, int spin, int color, 
			 field_offset dest, int timing)
{
  int i,status = 0;
  site *s;
  wilson_vector *wv;

  if(flag == CONTINUE)return 0;
  if(flag == FRESH){
    FORALLSITES(i,s)clear_wvec( (wilson_vector *)F_PT(s,dest) );
    return 0;
  }

  wv = (wilson_vector *)malloc(sites_on_node*sizeof(wilson_vector));
  if(wv == NULL){
    printf("reload_wprop_sc_to_site(%d): Can't allocate wv\n", this_node);
    terminate(1);
  }

  status = reload_wprop_sc_to_field( flag, wpf, wqs, spin, color, wv, timing);
  if(status)return status;

  FORALLSITES(i,s){
    copy_wvec(wv+i, (wilson_vector *)F_PT(s,dest));
  }

  free(wv);
  return status;

} /* reload_wprop_sc_to_site */

/*---------------------------------------------------------------*/
/* Temporary procedure to support legacy applications that read to the
   site structure */
/* save a propagator one source color and spin at a time */
 
int 
save_wprop_sc_from_site( int flag, w_prop_file *wpf, wilson_quark_source *wqs, 
			 int spin, int color, field_offset src, int timing)
{
  int status;
  int i; site *s;
  wilson_vector *wv;
  char recinfo[] = "Dummy record info";

  wv = (wilson_vector *)malloc(sites_on_node*sizeof(wilson_vector));
  if(wv == NULL){
    printf("save_wprop_sc_from_site(%d): Can't allocate wv\n", this_node);
    terminate(1);
  }

  FORALLSITES(i,s){
    copy_wvec((wilson_vector *)F_PT(s,src),wv+i);
  }

  status = save_wprop_sc_from_field( flag, wpf, wqs, spin, color, wv, 
				     recinfo, timing);

  free(wv);
  return status;

} /* save_wprop_sc_from_site */

/*---------------------------------------------------------------*/
/* Temporary procedure to support legacy applications that read to the
   site structure */
/* reload a full propagator (all source colors and spins) in any of
   the formats, or cold propagator, or keep current propagator:

   FRESH, CONTINUE,
   RELOAD_ASCII, RELOAD_SERIAL, RELOAD_PARALLEL, RELOAD_MULTIDUMP

   Return value is 

  -1 end of file
   0 normal exit value
   1 read error

*/
int 
reload_wprop_to_site( int flag, char *filename, wilson_quark_source *wqs,
		      field_offset dest, int timing )
{
  int i,status;
  site *s;
  wilson_propagator *wp;
  int spin, color;
  field_offset destcs;
  w_prop_file *wpf = NULL;
  
  status = 0;

  switch(flag){

  case CONTINUE:  /* do nothing */
    break;

  case FRESH:     /* zero initial guess */
    FORALLSITES(i,s){
      wp = (wilson_propagator *)F_PT(s,dest);
      for(color = 0; color < 3; color++)for(spin = 0; spin < 4; spin++)
	clear_wvec( &wp->c[color].d[spin] );
    }
    break;
    
  case RELOAD_ASCII:
    wpf = r_ascii_w_i(filename);
    for(color = 0; color < 3; color++)for(spin = 0; spin < 4; spin++)
      {
	destcs = dest + color*sizeof(spin_wilson_vector) + 
	  spin*sizeof(wilson_vector);
	if( r_ascii_w(wpf,spin,color,destcs) != 0)status = 1;
      }
    r_ascii_w_f(wpf);
    if(status == 0)
      node0_printf("Read wprop in ASCII format from file %s\n",wpf->filename);
    break;

  case RELOAD_SERIAL:
  case RELOAD_PARALLEL:
    wpf = r_open_wprop(flag, filename);
    for(color = 0; color < 3; color++)for(spin = 0; spin < 4; spin++)
      {
	destcs = dest + color*sizeof(spin_wilson_vector) + 
	  spin*sizeof(wilson_vector);
	if( reload_wprop_sc_to_site(flag, wpf, wqs, spin, color, destcs, timing)
	    != 0) status = 1;
      }
    r_close_wprop(flag, wpf);
    break;
    
  default:
    node0_printf("Bad reload flag %d\n",flag);
    terminate(1);
  }

  return status;

} /* reload_wprop_to_site */

/*---------------------------------------------------------------*/
/* Temporary procedure to support legacy applications that read to the
   site structure */
/* save the full propagator
   FORGET,
   SAVE_ASCII, SAVE_SERIAL, SAVE_PARALLEL, SAVE_MULTIDUMP, SAVE_CHECKPOINT
*/
int 
save_wprop_from_site( int flag, char *filename, wilson_quark_source *wqs,
		      field_offset src, char *recxml, int timing)
{
  int spin, color;
  int source_type;
  w_prop_file *wpf;
  field_offset srccs;
  int status;

  status = 0;

  switch(flag){

  case FORGET:
    break;

  case SAVE_ASCII:
    wpf = w_ascii_w_i(filename);
    for(color = 0; color < 3; color++)for(spin = 0; spin < 4; spin++)
      {
	srccs = src + color*sizeof(spin_wilson_vector) + 
	  spin*sizeof(wilson_vector);
	w_ascii_w(wpf,spin,color,srccs);
      }
    w_ascii_w_f(wpf);
    if(status == 0)
      node0_printf("Saved wprop in ASCII format to file %s\n",filename);
    break;

  case SAVE_SERIAL:
  case SAVE_PARALLEL:
    source_type = POINT;  /* For legacy code, this is the most likely choice */
    wpf = w_open_wprop(flag, filename, source_type);
    for(color = 0; color < 3; color++)for(spin = 0; spin < 4; spin++)
      {
	srccs = src + color*sizeof(spin_wilson_vector) + 
	  spin*sizeof(wilson_vector);
	save_wprop_sc_from_site(flag, wpf, wqs, spin, color, srccs, timing);
      }
    w_close_wprop(flag, wpf);
    if(status == 0)
      node0_printf("Saved wprop serially to file %s\n",filename);
    break;
  default:
    node0_printf("save_wprop_from_site: Unrecognized save flag.\n");
    terminate(1);
  }
  
  return status;

} /* save_wprop_from_site */
/*---------------------------------------------------------------*/
/* find out what kind of starting propagator to use, 
   and propagator name if necessary.  This routine is only 
   called by node 0.
   */
int 
ask_starting_wprop( FILE *fp, int prompt, int *flag, char *filename )
{
  char *savebuf;
  int status;
  char myname[] = "ask_starting_wprop";
  
  if (prompt!=0) {
    printf("loading wilson propagator:\n enter 'fresh_wprop', ");
    printf("'continue_wprop', 'reload_ascii_wprop', ");
    printf("'reload_serial_wprop', 'reload_parallel_wprop', ");
    printf("\n");
  }

  savebuf = get_next_tag(fp, "read wprop command", myname);
  if (savebuf == NULL)return 1;

  printf("%s ",savebuf);
  if(strcmp("fresh_wprop",savebuf) == 0 ){
    *flag = FRESH;
    printf("\n");
  }
  else if(strcmp("continue_wprop",savebuf) == 0 ) {
    *flag = CONTINUE;
    printf("(if possible)\n");
  }
  else if(strcmp("reload_ascii_wprop",savebuf) == 0 ) {
    *flag = RELOAD_ASCII;
  }
  else if(strcmp("reload_serial_wprop",savebuf) == 0 ) {
    *flag = RELOAD_SERIAL;
  }
  else if(strcmp("reload_parallel_wprop",savebuf) == 0 ) {
    *flag = RELOAD_PARALLEL;
  }
  else{
    printf("ERROR IN INPUT: propagator_command is invalid\n"); return(1);
  }
  
  /*read name of file and load it */
  if( *flag != FRESH && *flag != CONTINUE ){
    if(prompt!=0)printf("enter name of file containing props\n");
    status=scanf("%s",filename);
    if(status !=1) {
      printf("\n%s(%d): ERROR IN INPUT: Can't read file name.\n",
	     myname, this_node);
      return(1);
    }
    printf("%s\n",filename);
  }
  return(0);
} /* ask_starting_wprop */

/*---------------------------------------------------------------*/
/* find out what do to with propagator at end, and propagator name if
   necessary.  This routine is only called by node 0.
   */
int 
ask_ending_wprop( FILE *fp, int prompt, int *flag, char *filename ){
  char *savebuf;
  int status;
  char myname[] = "ask_ending_wprop";
  
  if (prompt!=0) {
    printf("save wilson propagator:\n enter ");
    printf("'forget_wprop', 'save_ascii_wprop', ");
    printf("'save_serial_fm_wprop', 'save_serial_fm_sc_wprop', ");
    printf("'save_serial_scidac_wprop', 'save_parallel_scidac_wprop', ");
    printf("'save_partfile_scidac_wprop', 'save_multifile_scidac_wprop', ");
    printf("\n");
  }

  savebuf = get_next_tag(fp, "write wprop command", myname);
  if (savebuf == NULL)return 1;

  printf("%s ",savebuf);

  if(strcmp("save_ascii_wprop",savebuf) == 0 )  {
    *flag=SAVE_ASCII;
  }
  else if(strcmp("save_serial_fm_wprop",savebuf) == 0 ) {
    *flag=SAVE_SERIAL_FM;
  }
  else if(strcmp("save_serial_fm_sc_wprop",savebuf) == 0 ) {
    *flag=SAVE_SERIAL_FM_SC;
  }
  else if(strcmp("save_serial_scidac_wprop",savebuf) == 0 ) {
    *flag=SAVE_SERIAL_SCIDAC;
  }
  else if(strcmp("save_parallel_scidac_wprop",savebuf) == 0 ) {
    *flag=SAVE_PARALLEL_SCIDAC;
  }
  else if(strcmp("save_partfile_scidac_wprop",savebuf) == 0 ) {
    *flag=SAVE_PARTITION_SCIDAC;
  }
  else if(strcmp("save_multifile_scidac_wprop",savebuf) == 0 ) {
    *flag=SAVE_MULTIFILE_SCIDAC;
  }
  else if(strcmp("forget_wprop",savebuf) == 0 ) {
    *flag=FORGET;
    printf("\n");
  }
  else {
    printf(" is not a valid save wprop command. INPUT ERROR.\n");
    return(1);
  }
  
  if( *flag != FORGET ){
    if(prompt!=0)printf("enter filename\n");
    status=scanf("%s",filename);
    if(status !=1){
      printf("\n%s(%d): ERROR IN INPUT. Can't read filename\n",
	     myname, this_node); 
      return(1);
    }
    printf("%s\n",filename);
  }
  return(0);
} /* ask_ending_wprop */
