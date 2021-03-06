/* ----------------------------- MNI Header -----------------------------------
@NAME       : gcomtodicom.c
@DESCRIPTION: Program to receive GYROCOM images and retransmit them to a 
              remote DICOM server
@GLOBALS    : 
@CREATED    : June 9, 1997 (Peter Neelin)
@MODIFIED   : $Log: gcomtodicom.c,v $
@MODIFIED   : Revision 6.5  2008-01-17 02:33:02  rotor
@MODIFIED   :  * removed all rcsids
@MODIFIED   :  * removed a bunch of ^L's that somehow crept in
@MODIFIED   :  * removed old (and outdated) BUGS file
@MODIFIED   :
@MODIFIED   : Revision 6.4  2008/01/12 19:08:14  stever
@MODIFIED   : Add __attribute__ ((unused)) to all rcsid variables.
@MODIFIED   :
@MODIFIED   : Revision 6.3  2001/04/21 12:50:47  neelin
@MODIFIED   : Added in -d (dump) and -t (trace) options.
@MODIFIED   :
@MODIFIED   : Revision 6.2  2001/04/09 23:02:49  neelin
@MODIFIED   : Modified copyright notice, removing permission statement since copying,
@MODIFIED   : etc. is probably not permitted by our non-disclosure agreement with
@MODIFIED   : Philips.
@MODIFIED   :
@MODIFIED   : Revision 6.1  2001/03/19 18:39:17  neelin
@MODIFIED   : Program to convert captured gyrocom files to dicom and forward them to
@MODIFIED   : a dicom server.
@MODIFIED   :
@COPYRIGHT  :
              Copyright 1997 Peter Neelin, McConnell Brain Imaging Centre, 
              Montreal Neurological Institute, McGill University.
---------------------------------------------------------------------------- */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <ctype.h>
#include <time.h>
#include <acr_nema.h>
#include <spi_element_defs.h>

#ifndef public
#  define public
#endif
#ifndef private
#  define private static
#endif

#ifndef TRUE
#  define TRUE 1
#endif
#ifndef FALSE
#  define TRUE 0
#endif

/* Function prototypes */
public int send_file(Acr_File *afpin, Acr_File *afpout, char *filename);
public void convert_to_dicom(Acr_Group group_list, char *uid_prefix,
                             int use_safe_orientations);

/* Globals */
int Use_safe_orientations = FALSE;
int No_network = FALSE;
int Do_trace = FALSE;
char *Uid_prefix = "";

/* Main program */

int main(int argc, char *argv[])
{
   char *pname;
   char *host, *port, *AE_title;
   char **file_list;
   int ifile, num_files;
   Acr_File *afpin, *afpout;
   char implementation_uid[128];
   int optc;
   extern char *optarg;
   extern int optind;
   char *optlist = "hsdtu:";
   char *usage = 
      "Usage: %s [-h] [-s] [-d] [-t] [-u <uid prefix>] host port AE-title files ...\n";
   char *abstract_syntax_list[] =
      {
         ACR_MR_IMAGE_STORAGE_UID,
         NULL
      };
   char *transfer_syntax_list[] =
      {
         ACR_IMPLICIT_VR_LITTLE_END_UID,
         NULL
      };

   /* Check arguments */
   pname = argv[0];
   while ((optc = getopt(argc, argv, optlist)) != EOF) {
      switch(optc) {
         case 'h':
            (void) fprintf(stderr, "Options:\n");
            (void) fprintf(stderr, "   -h:\tPrint this message\n");
            (void) fprintf(stderr, "   -s:\tUse safe orientations\n");
            (void) fprintf(stderr, "   -d:\tOnly dump the dicom data\n");
            (void) fprintf(stderr, "   -t:\tDo trace of i/o\n");
            (void) fprintf(stderr, "   -u:\tSpecify uid prefix\n");
            (void) fprintf(stderr, "\n");
            (void) fprintf(stderr, usage, pname);
            exit(EXIT_FAILURE);
            break;
         case 's':
            Use_safe_orientations = TRUE;
            break;
         case 'd':
            No_network = TRUE;
            break;
         case 't':
            Do_trace = TRUE;
            break;
         case 'u':
            Uid_prefix = optarg;
            break;
      }
   }
   if (argc-optind < 4) {
      (void) fprintf(stderr, usage, pname);
      return EXIT_FAILURE;
   }
   host = argv[optind++];
   port = argv[optind++];
   AE_title = argv[optind++];
   file_list = &argv[optind++];
   num_files = argc - optind + 1;

   /* Set the software implementation uid */
   if (strlen(Uid_prefix) > (size_t) 0) {
      (void) sprintf(implementation_uid, "%s.100.1.1", Uid_prefix);
      acr_set_implementation_uid(implementation_uid);
   }

   /* Make network connection */
   if (!No_network) {

      if (!acr_open_dicom_connection(host, port, AE_title, "GCOM_TEST",
                                     ACR_MR_IMAGE_STORAGE_UID,
                                     ACR_IMPLICIT_VR_LITTLE_END_UID,
                                     &afpin, &afpout)) {
         (void) fprintf(stderr, "Unable to connect to host %s", host);
         return EXIT_FAILURE;
      }
      if (Do_trace) {
         acr_dicom_enable_trace(afpin);
         acr_dicom_enable_trace(afpout);
      }

   }
   else {          /* No network connection - dump only */

      afpin = NULL;
      afpout = NULL;

   }

   /* Loop over the input files, sending them one at a time */
   for (ifile = 0; ifile < num_files; ifile++) {
      (void) fprintf(stderr, "Sending file %s\n", file_list[ifile]);
      if (!send_file(afpin, afpout, file_list[ifile])) {
         (void) fprintf(stderr, "Error sending dicom image\n");
         return EXIT_FAILURE;
      }
   }

   /* Close the connection */
   if (!No_network) {
      acr_close_dicom_connection(afpin, afpout);
   }

   (void) fprintf(stderr, "Success!\n");

   return EXIT_SUCCESS;
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : send_file
@INPUT      : afpin - input stream
              afpout - output stream
              filename - name of file to send
@OUTPUT     : (none)
@RETURNS    : TRUE if send went smoothly.
@DESCRIPTION: Routine to read in an ACR-NEMA format file and send it over
              a dicom connection.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : May 9, 1997 (Peter Neelin)
@MODIFIED   : 
---------------------------------------------------------------------------- */
public int send_file(Acr_File *afpin, Acr_File *afpout, char *filename)
{
   FILE *fp;
   Acr_File *file_afp;
   Acr_Status status;
   Acr_Group data_group_list;

   /* Open input file */
   fp = fopen(filename, "r");
   if (fp == NULL) {
      (void) fprintf(stderr, "Error opening file %s\n", filename);
      return FALSE;
   }

   /* Connect to input stream */
   file_afp = acr_file_initialize(fp, 0, acr_stdio_read);
   (void) acr_test_byte_order(file_afp);

   /* Read in group list up to group */
   status =  acr_input_group_list(file_afp, &data_group_list, 0);
   if ((status != ACR_OK) && (status != ACR_END_OF_INPUT)) {
      (void) fprintf(stderr, "Error reading file \"%s\"", filename);
      acr_dicom_error(status, "");
      return FALSE;
   }

   /* Free the afp */
   acr_file_free(file_afp);

   /* Check for non-image data */
   if (acr_find_group_element(data_group_list, ACR_Pixel_data) == NULL) {
      return TRUE;
   }

   /* Modify group list to be dicom conformant */
   convert_to_dicom(data_group_list, Uid_prefix, Use_safe_orientations);

   /* Send or dump the group list */
   if (!No_network) {
      if (!acr_send_group_list(afpin, afpout, data_group_list,
                               ACR_MR_IMAGE_STORAGE_UID)) {
         (void) fprintf(stderr, "Error sending image\n");
         return FALSE;
      }
   }
   else {
      acr_dump_group_list(stdout, data_group_list);
   }

   /* Delete the group list */
   acr_delete_group_list(data_group_list);

   return TRUE;
}

