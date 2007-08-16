#ifndef	_LeCroy_GenType_h_
#define	_LeCroy_GenType_h_

/* We you try to define some size-matter variables, */
/* you need these pre-defined types. */
/* If you don't care about size of variables, */
/* you can use system definitions. */

#ifndef vxWorks /* because vxWorks already defined them */
typedef unsigned char           UINT8;
typedef unsigned short int      UINT16;
typedef unsigned int            UINT32;
#endif

typedef signed char             SINT8;
typedef signed short int        SINT16;
typedef signed int              SINT32;
typedef float                   FLOAT32;
typedef double                  DOUBLE64;

#endif

