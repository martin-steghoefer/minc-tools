#ifndef  _DEF_SYSTEM_DEPENDENT_H

/* ----------------------------------------------------------------------------
@COPYRIGHT  :
              Copyright 1993,1994,1995 David MacDonald,
              McConnell Brain Imaging Centre,
              Montreal Neurological Institute, McGill University.
              Permission to use, copy, modify, and distribute this
              software and its documentation for any purpose and without
              fee is hereby granted, provided that the above copyright
              notice appear in all copies.  The author and McGill University
              make no representations about the suitability of this
              software for any purpose.  It is provided "as is" without
              express or implied warranty.
---------------------------------------------------------------------------- */

#ifdef NO_DBL_MAX
#ifndef DBL_MAX
#include <values.h>
#define DBL_MAX  MAXDOUBLE
#endif
#endif  /* NO_DBL_MAX */

#include <stdlib.h>

#ifndef  EXIT_FAILURE
#define  EXIT_FAILURE  1
#endif

#ifndef  EXIT_SUCCESS
#define  EXIT_SUCCESS  0
#endif

#endif