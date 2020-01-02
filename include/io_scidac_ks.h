#ifndef _IO_SCIDAC_KS_H
#define _IO_SCIDAC_KS_H

#include <qio.h>
#include "../include/macros.h"

/**********************************************************************/
/* In io_scidac_ks.c */

void close_usqcd_ksprop_read(QIO_Reader *infile);
void close_usqcd_ksprop_write(QIO_Writer *outfile);
QIO_Reader *open_usqcd_ksprop_read(char *filename, int serpar, char **fileinfo);
QIO_Writer *open_usqcd_ksprop_write(char *filename, int volfmt, 
				    int serpar, int ildgstyle, 
				    char *stringLFN, int milc_type,
				    char *fileinfo);
int read_ks_vector_scidac(QIO_Reader *infile, su3_vector *dest, int count);
int read_ks_vector_scidac_xml(QIO_Reader *infile, su3_vector *dest, int count,
			      QIO_String *recxml);
int read_kspropsource_C_usqcd(QIO_Reader *infile, char *srcinfo, int n,
			      complex *dest);
int read_kspropsource_V_usqcd(QIO_Reader *infile, char *srcinfo, int n,
			      su3_vector *dest);
int read_ksproprecord_usqcd(QIO_Reader *infile, int *color, su3_vector *dest);
int save_ks_vector_scidac(QIO_Writer *outfile, char *filename, char *recinfo,
			  int volfmt, su3_vector *src, int count, int prec);
void restore_ks_vector_scidac_to_field(char *filename, int serpar, 
				       su3_vector *dest, int count);
void restore_ks_vector_scidac_to_site(char *filename, int serpar,
				      field_offset dest, int count);
void save_ks_vector_scidac_from_field(char *filename, char *fileinfo,
				      char *recinfo, 
				      int volfmt, int serpar, 
				      su3_vector *src, int count, int prec);
void save_ks_vector_scidac_from_site(char *filename, char *fileinfo,
				     char *recinfo, 
				     int volfmt, int serpar, 
				     field_offset src, int count, int prec);
int write_kspropsource_C_usqcd(QIO_Writer *outfile, char *srcinfo, 
			       complex *src, int t0);
int write_kspropsource_C_usqcd_xml(QIO_Writer *outfile, QIO_String *recxml, 
				   complex *src, int t0);
int write_kspropsource_V_usqcd(QIO_Writer *outfile, char *srcinfo, 
			       su3_vector *src, int t0);
int write_kspropsource_V_usqcd_xml(QIO_Writer *outfile, QIO_String *recxml,
				   su3_vector *src, int t0);
int write_ksprop_usqcd_c(QIO_Writer *outfile, su3_vector *src, 
			 int color, char *recinfo);

/**********************************************************************/
/* In io_scidac_ks_eigen.c */

QIO_Writer *open_ks_eigen_outfile(char *filename, int Nvecs, int volfmt, int serpar);
int write_ks_eigenvector(QIO_Writer *outfile, su3_vector *eigVec, double eigVal, double resid);
void close_ks_eigen_outfile(QIO_Writer *outfile);
QIO_Reader *open_ks_eigen_infile(char *filename, int *Nvecs, int *packed, int *file_type, int serpar);
int read_ks_eigenvector(QIO_Reader *infile, int packed, su3_vector *eigVec, double *eigVal);
int read_quda_ks_eigenvectors(QIO_Reader *infile, su3_vector *eigVec[], double *eigVal, int *Nvecs,
			      int parity, imp_ferm_links_t *fn);
void close_ks_eigen_infile(QIO_Reader *infile);

/**********************************************************************/
/* In ks_info.c (application dependent) */

char *create_ks_XML(void);
void free_ks_XML(char *xml);

#endif /* _IO_SCIDAC_KS_H */
