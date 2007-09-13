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

#include  <internal_volume_io.h>
#include  <limits.h>
#include  <float.h>

#ifndef lint
static char rcsid[] = "$Header: /private-cvsroot/minc/volume_io/Volumes/volumes.c,v 1.77 2007-09-13 01:31:38 rotor Exp $";
#endif

STRING   XYZ_dimension_names[] = { MIxspace, MIyspace, MIzspace };
STRING   File_order_dimension_names[] = { "", "", "", "", "" };

static  STRING  default_dimension_names[MAX_DIMENSIONS][MAX_DIMENSIONS] =
{
    { MIxspace },
    { MIyspace, MIxspace },
    { MIzspace, MIyspace, MIxspace },
    { "", MIzspace, MIyspace, MIxspace },
    { "", "", MIzspace, MIyspace, MIxspace }
};

/* ----------------------------- MNI Header -----------------------------------
@NAME       : get_default_dim_names
@INPUT      : n_dimensions
@OUTPUT     : 
@RETURNS    : list of dimension names
@DESCRIPTION: Returns the list of default dimension names for the given
              number of dimensions.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 1993            David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  STRING  *get_default_dim_names(
    int    n_dimensions )
{
    return( default_dimension_names[n_dimensions-1] );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : convert_spatial_axis_to_dim_name
@INPUT      : axis
@OUTPUT     : 
@RETURNS    : dimension name
@DESCRIPTION: Returns the name of the dimension.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 1993            David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

static  STRING  convert_spatial_axis_to_dim_name(
    int   axis )
{
    switch( axis )
    {
    case X:  return( MIxspace );
    case Y:  return( MIyspace );
    case Z:  return( MIzspace );
    default:  handle_internal_error(
        "convert_spatial_axis_to_dim_name" ); break;
    }

    return( NULL );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : convert_dim_name_to_spatial_axis
@INPUT      : dimension_name
@OUTPUT     : axis
@RETURNS    : TRUE if axis name is a spatial dimension
@DESCRIPTION: Checks if the dimension name corresponds to a spatial dimension
              and if so, passes back the corresponding axis index.
@METHOD     :
@GLOBALS    :
@CALLS      :
@CREATED    : 1993            David MacDonald
@MODIFIED   :
---------------------------------------------------------------------------- */

VIOAPI  BOOLEAN  convert_dim_name_to_spatial_axis(
    STRING  name,
    int     *axis )
{
    *axis = -1;

    if( equal_strings( name, MIxspace ) )
        *axis = X;
    else if( equal_strings( name, MIyspace ) )
        *axis = Y;
    else if( equal_strings( name, MIzspace ) )
        *axis = Z;

    return( *axis >= 0 );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : create_volume
@INPUT      : n_dimensions      - number of dimensions (1-5)
              dimension_names   - name of dimensions for use when reading file
              data_type         - type of the data, e.g. NC_BYTE
              signed_flag       - type is signed?
              min_value         - min and max value to be stored
              max_value
@OUTPUT     : 
@RETURNS    : Volume
@DESCRIPTION: Creates a Volume structure, and initializes it.  In order to 
              later use the volume, you must call either set_volume_size()
              and alloc_volume_data(), or one of the input volume routines,
              which in turn calls these two.
              The dimension_names are used when inputting
              MINC files, in order to match with the dimension names in the
              file.  Typically, use dimension names
              { MIzspace, MIyspace, MIxspace } to read the volume from the
              file in the order it is stored, or
              { MIxspace, MIyspace, MIzspace } to read it so you can subcript
              the volume in x, y, z order.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : June, 1993       David MacDonald
@MODIFIED   : Nov. 15, 1996    D. MacDonald    - handles space type
@MODIFIED   : May  22, 1996    D. MacDonald    - now stores starts/steps
---------------------------------------------------------------------------- */

VIOAPI   Volume   create_volume(
    int         n_dimensions,
    STRING      dimension_names[],
    nc_type     nc_data_type,
    BOOLEAN     signed_flag,
    Real        voxel_min,
    Real        voxel_max )
{
    int             i, axis, sizes[MAX_DIMENSIONS];
    Status          status;
    STRING          name;
    volume_struct   *volume;
    Transform       identity;

    status = OK;

    if( n_dimensions < 1 || n_dimensions > MAX_DIMENSIONS )
    {
        print_error(
            "create_volume(): n_dimensions (%d) not in range 1 to %d.\n",
               n_dimensions, MAX_DIMENSIONS );
        status = ERROR;
    }

    if( status == ERROR )
    {
        return( (Volume) NULL );
    }

    ALLOC( volume, 1 );

    volume->is_rgba_data = FALSE;
    volume->is_cached_volume = FALSE;

    volume->real_range_set = FALSE;
    volume->real_value_scale = 1.0;
    volume->real_value_translation = 0.0;

    for_less( i, 0, N_DIMENSIONS )
        volume->spatial_axes[i] = -1;

    for_less( i, 0, n_dimensions )
    {
        volume->starts[i] = 0.0;
        volume->separations[i] = 1.0;
        volume->direction_cosines[i][X] = 0.0;
        volume->direction_cosines[i][Y] = 0.0;
        volume->direction_cosines[i][Z] = 0.0;
        volume->irregular_starts[i] = NULL;
        volume->irregular_widths[i] = NULL;

        sizes[i] = 0;

        if( dimension_names != (char **) NULL )
            name = dimension_names[i];
        else
            name = default_dimension_names[n_dimensions-1][i];

        if( convert_dim_name_to_spatial_axis( name, &axis ) )
        {
            volume->spatial_axes[axis] = i;
            volume->direction_cosines[i][axis] = 1.0;
        }

        volume->dimension_names[i] = create_string( name );
    }

    create_empty_multidim_array( &volume->array, n_dimensions, NO_DATA_TYPE );

    set_volume_type( volume, nc_data_type, signed_flag, voxel_min, voxel_max );
    set_volume_sizes( volume, sizes );

    make_identity_transform( &identity );
    create_linear_transform( &volume->voxel_to_world_transform, &identity );
    volume->voxel_to_world_transform_uptodate = TRUE;

    volume->coordinate_system_name = create_string( MI_UNKNOWN_SPACE );

    return( volume );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : set_volume_type
@INPUT      : volume
              nc_data_type
              signed_flag
              voxel_min
              voxel_max
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION: Sets the data type and valid range of the volume.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 1993            David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  void  set_volume_type(
    Volume       volume,
    nc_type      nc_data_type,
    BOOLEAN      signed_flag,
    Real         voxel_min,
    Real         voxel_max )
{
    Data_types      data_type;

    if( nc_data_type != MI_ORIGINAL_TYPE )
    {
        switch( nc_data_type )
        {
        case  NC_BYTE:
            if( signed_flag )
                data_type = SIGNED_BYTE;
            else
                data_type = UNSIGNED_BYTE;
            break;

        case  NC_SHORT:
            if( signed_flag )
                data_type = SIGNED_SHORT;
            else
                data_type = UNSIGNED_SHORT;
            break;

        case  NC_INT:
            if( signed_flag )
                data_type = SIGNED_INT;
            else
                data_type = UNSIGNED_INT;
            break;

        case  NC_FLOAT:
            data_type = FLOAT;
            break;

        case  NC_DOUBLE:
            data_type = DOUBLE;
            break;
        }

        set_multidim_data_type( &volume->array, data_type );
        volume->signed_flag = signed_flag;

        set_volume_voxel_range( volume, voxel_min, voxel_max );
    }

    volume->nc_data_type = nc_data_type;
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : get_volume_nc_data_type
@INPUT      : volume
@OUTPUT     : signed_flag
@RETURNS    : data type
@DESCRIPTION: Returns the NETCDF data type of the volume and passes back
              the signed flag.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 1993            David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  nc_type  get_volume_nc_data_type(
    Volume       volume,
    BOOLEAN      *signed_flag )
{
    if( signed_flag != (BOOLEAN *) NULL )
        *signed_flag = volume->signed_flag;
    return( volume->nc_data_type );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : get_volume_data_type
@INPUT      : volume
@OUTPUT     : 
@RETURNS    : data type
@DESCRIPTION: Returns the data type of the volume (not the NETCDF type).
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 1993            David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  Data_types  get_volume_data_type(
    Volume       volume )
{
    return( get_multidim_data_type( &volume->array ) );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : set_rgb_volume_flag
@INPUT      : volume
              flag
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION: Sets the flag indicating that the volume is an RGB volume.
              Can only set the flag to TRUE if the volume is an unsigned
              long volume.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : Nov 13, 1995    David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  void  set_rgb_volume_flag(
    Volume   volume,
    BOOLEAN  flag )
{
    if( !flag || get_volume_data_type(volume) == UNSIGNED_INT )
        volume->is_rgba_data = flag;
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : is_an_rgb_volume
@INPUT      : volume
@OUTPUT     : 
@RETURNS    : TRUE if it is an RGB volume
@DESCRIPTION: Tests if the volume is an RGB volume.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : Jun 21, 1995    David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  BOOLEAN  is_an_rgb_volume(
    Volume   volume )
{
    return( volume->is_rgba_data );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : alloc_volume_data
@INPUT      : volume
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION: Allocates the memory for the volume.  Assumes that the
              volume type and sizes have been set.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : June, 1993           David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  void  alloc_volume_data(
    Volume   volume )
{
    unsigned long   data_size;

    data_size = (unsigned long) get_volume_total_n_voxels( volume ) *
                (unsigned long) get_type_size( get_volume_data_type( volume ) );

    if( get_n_bytes_cache_threshold() >= 0 &&
        data_size > (unsigned long) get_n_bytes_cache_threshold() )
    {
        volume->is_cached_volume = TRUE;
        initialize_volume_cache( &volume->cache, volume );
    }
    else
    {
        volume->is_cached_volume = FALSE;
        alloc_multidim_array( &volume->array );
    }
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : volume_is_alloced
@INPUT      : volume
@OUTPUT     : 
@RETURNS    : TRUE if the volume is allocated
@DESCRIPTION: Checks if the volume data has been allocated.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : Sep. 1, 1995    David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  BOOLEAN  volume_is_alloced(
    Volume   volume )
{
    return( volume->is_cached_volume &&
            volume_cache_is_alloced( &volume->cache ) ||
            !volume->is_cached_volume &&
            multidim_array_is_alloced( &volume->array ) );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : free_volume_data
@INPUT      : volume
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION: Frees the memory associated with the volume multidimensional data.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : June, 1993           David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  void  free_volume_data(
    Volume   volume )
{
    if( volume->is_cached_volume )
        delete_volume_cache( &volume->cache, volume );
    else if( volume_is_alloced( volume ) )
        delete_multidim_array( &volume->array );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : delete_volume
@INPUT      : volume
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION: Frees all memory from the volume and the volume struct itself.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : June, 1993           David MacDonald
@MODIFIED   : Nov. 15, 1996    D. MacDonald    - handles space type
---------------------------------------------------------------------------- */

VIOAPI  void  delete_volume(
    Volume   volume )
{
    int   d;

    if( volume == (Volume) NULL )
    {
        print_error( "delete_volume():  cannot delete a null volume.\n" );
        return;
    }

    free_volume_data( volume );

    delete_general_transform( &volume->voxel_to_world_transform );

    for_less( d, 0, get_volume_n_dimensions(volume) ) {
        delete_string( volume->dimension_names[d] );
        if( volume->irregular_starts[d] ) FREE( volume->irregular_starts[d] );
        if( volume->irregular_widths[d] ) FREE( volume->irregular_widths[d] );
    }

    delete_string( volume->coordinate_system_name );

    FREE( volume );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : get_volume_n_dimensions
@INPUT      : volume
@OUTPUT     : 
@RETURNS    : number of dimensions
@DESCRIPTION: Returns the number of dimensions of the volume
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : June, 1993           David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  int  get_volume_n_dimensions(
    Volume   volume )
{
    return( get_multidim_n_dimensions( &volume->array ) );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : get_volume_sizes
@INPUT      : volume
@OUTPUT     : sizes
@RETURNS    : 
@DESCRIPTION: Passes back the sizes of each of the dimensions.  Assumes sizes
              has enough room for n_dimensions integers.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : June, 1993           David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  void  get_volume_sizes(
    Volume   volume,
    int      sizes[] )
{
    get_multidim_sizes( &volume->array, sizes );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : set_volume_sizes
@INPUT      : volume
              sizes
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION: Sets the sizes (number of voxels in each dimension) of the volume.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 1993            David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  void  set_volume_sizes(
    Volume       volume,
    int          sizes[] )
{
    set_multidim_sizes( &volume->array, sizes );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : get_volume_total_n_voxels
@INPUT      : volume
@OUTPUT     : 
@RETURNS    : n voxels
@DESCRIPTION: Returns the total number of voxels in the volume.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 1993            David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  unsigned int  get_volume_total_n_voxels(
    Volume    volume )
{
    unsigned  int  n;
    int       i, sizes[MAX_DIMENSIONS];

    n = 1;

    get_volume_sizes( volume, sizes );

    for_less( i, 0, get_volume_n_dimensions( volume ) )
        n *= (unsigned int) sizes[i];

    return( n );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : assign_voxel_to_world_transform
@INPUT      : volume
              transform
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION: Updates the volume's transformation from voxel to world coords.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : May  20, 1997    D. MacDonald - created from
                                          set_voxel_to_world_transform
@MODIFIED   : 
---------------------------------------------------------------------------- */

static  void  assign_voxel_to_world_transform(
    Volume             volume,
    General_transform  *transform )
{
    delete_general_transform( &volume->voxel_to_world_transform );

    volume->voxel_to_world_transform = *transform;
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : dot_vectors
@INPUT      : n
              v1
              v2
@OUTPUT     :
@RETURNS    : Dot product
@DESCRIPTION: Computes the dot product of 2 n-dimensional vectors.
              This function should be moved to some vector routines.
@METHOD     :
@GLOBALS    :
@CALLS      :
@CREATED    :         1993    David MacDonald
@MODIFIED   :
---------------------------------------------------------------------------- */

static  Real   dot_vectors(
    int    n,
    Real   v1[],
    Real   v2[] )
{
    int   i;
    Real  d;

    d = 0.0;
    for_less( i, 0, n )
        d += v1[i] * v2[i];

    return( d );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : cross_3D_vector
@INPUT      : v1
              v2
@OUTPUT     : cross
@RETURNS    : 
@DESCRIPTION: Computes the cross product of 2 n-dimensional vectors.
              This function should be moved to some vector routines.
@METHOD     :
@GLOBALS    :
@CALLS      :
@CREATED    :         1993    David MacDonald
@MODIFIED   :
---------------------------------------------------------------------------- */

static  void   cross_3D_vector(
    Real   v1[],
    Real   v2[],
    Real   cross[] )
{
    cross[0] = v1[1] * v2[2] - v1[2] * v2[1];
    cross[1] = v1[2] * v2[0] - v1[0] * v2[2];
    cross[2] = v1[0] * v2[1] - v1[1] * v2[0];
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : normalize_vector
@INPUT      : n
              v1
@OUTPUT     : v1_normalized
@RETURNS    : 
@DESCRIPTION: Normalizes the length of v1 to 1, placing result in
              v1_normalized
@METHOD     :
@GLOBALS    :
@CALLS      :
@CREATED    : May  22, 1997   D. MacDonald
@MODIFIED   :
---------------------------------------------------------------------------- */

static  void   normalize_vector(
    Real   v1[],
    Real   v1_normalized[] )
{
    int    d;
    Real   mag;

    mag = dot_vectors( N_DIMENSIONS, v1, v1 );
    if( mag <= 0.0 )
        mag = 1.0;

    mag = sqrt( mag );

    for_less( d, 0, N_DIMENSIONS )
        v1_normalized[d] = v1[d] / mag;
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : compute_world_transform
@INPUT      : spatial_axes
              separations
              translation_voxel
              world_space_for_translation_voxel
              direction_cosines
@OUTPUT     : world_transform
@RETURNS    : 
@DESCRIPTION: Computes the linear transform from the indices of the spatial
              dimensions (spatial_axes), the separations, the translation
              (translation_voxel,world_space_for_translation_voxel) and
              the direction cosines.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 1993            David MacDonald
@MODIFIED   : May  22, 1997   D. MacDonald - now uses starts/steps
---------------------------------------------------------------------------- */

VIOAPI  void  compute_world_transform(
    int                 spatial_axes[N_DIMENSIONS],
    Real                separations[],
    Real                direction_cosines[][N_DIMENSIONS],
    Real                starts[],
    General_transform   *world_transform )
{
    Transform                transform;
    Real                     separations_3D[N_DIMENSIONS];
    Real                     directions[N_DIMENSIONS][N_DIMENSIONS];
    Real                     starts_3D[N_DIMENSIONS];
    Real                     normal[N_DIMENSIONS];
    int                      dim, c, a1, a2, axis, n_axes;
    int                      axis_list[N_DIMENSIONS];

    /*--- find how many direction cosines are specified, and set the
          3d separations and starts */

    n_axes = 0;

    for_less( c, 0, N_DIMENSIONS )
    {
        axis = spatial_axes[c];
        if( axis >= 0 )
        {
            separations_3D[c] = separations[axis];
            starts_3D[c] = starts[axis];
            directions[c][X] = direction_cosines[axis][X];
            directions[c][Y] = direction_cosines[axis][Y];
            directions[c][Z] = direction_cosines[axis][Z];
            axis_list[n_axes] = c;
            ++n_axes;
        }
        else
        {
            separations_3D[c] = 1.0;
            starts_3D[c] = 0.0;
        }
    }

    if( n_axes == 0 )
    {
        print_error( "error compute_world_transform:  no axes.\n" );
        return;
    }

    /*--- convert 1 or 2 axes to 3 axes */

    if( n_axes == 1 )
    {
        a1 = (axis_list[0] + 1) % N_DIMENSIONS;
        a2 = (axis_list[0] + 2) % N_DIMENSIONS;

        /*--- create an orthogonal vector */

        directions[a1][X] = directions[axis_list[0]][Y] +
                            directions[axis_list[0]][Z];
        directions[a1][Y] = -directions[axis_list[0]][X] -
                            directions[axis_list[0]][Z];
        directions[a1][Z] = directions[axis_list[0]][Y] -
                            directions[axis_list[0]][X];

        cross_3D_vector( directions[axis_list[0]], directions[a1],
                         directions[a2] );
        normalize_vector( directions[a1], directions[a1] );
        normalize_vector( directions[a2], directions[a2] );
    }
    else if( n_axes == 2 )
    {
        a2 = N_DIMENSIONS - axis_list[0] - axis_list[1];

        cross_3D_vector( directions[axis_list[0]], directions[axis_list[1]],
               directions[a2] );

        normalize_vector( directions[a2], directions[a2] );
    }

    /*--- check to make sure that 3 axes are not a singular system */

    for_less( dim, 0, N_DIMENSIONS )
    {
        cross_3D_vector( directions[dim], directions[(dim+1)%N_DIMENSIONS],
                         normal );
        if( normal[0] == 0.0 && normal[1] == 0.0 && normal[2] == 0.0 )
            break;
    }

    if( dim < N_DIMENSIONS )
    {
        directions[0][0] = 1.0;
        directions[0][1] = 0.0;
        directions[0][2] = 0.0;
        directions[1][0] = 0.0;
        directions[1][1] = 1.0;
        directions[1][2] = 0.0;
        directions[2][0] = 0.0;
        directions[2][1] = 0.0;
        directions[2][2] = 1.0;
    }

    /*--- make the linear transformation */

    make_identity_transform( &transform );

    for_less( c, 0, N_DIMENSIONS )
    {
        for_less( dim, 0, N_DIMENSIONS )
        {
            Transform_elem(transform,dim,c) = directions[c][dim] *
                                              separations_3D[c];

            Transform_elem(transform,dim,3) += directions[c][dim] *
                                               starts_3D[c];
        }
    }

    create_linear_transform( world_transform, &transform );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : check_recompute_world_transform
@INPUT      : volume
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION: Recompute the voxel to world transform.  Called when one of
              the attributes affecting this is changed.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 1993            David MacDonald
@MODIFIED   : May  20, 1997   D. MacDonald - now checks update flag
---------------------------------------------------------------------------- */

static  void  check_recompute_world_transform(
    Volume  volume )
{
    General_transform        world_transform;

    if( !volume->voxel_to_world_transform_uptodate )
    {
        volume->voxel_to_world_transform_uptodate = TRUE;

        compute_world_transform( volume->spatial_axes,
                                 volume->separations,
                                 volume->direction_cosines,
                                 volume->starts,
                                 &world_transform );

        assign_voxel_to_world_transform( volume, &world_transform );
    }
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : convert_transform_origin_to_starts
@INPUT      : origin
              n_volume_dimensions
              spatial_axes
              dir_cosines
@OUTPUT     : starts
@RETURNS    : 
@DESCRIPTION: Converts a transform origin into starts (multiples of the
              dir_cosines).  dir_cosines need not be mutually orthogonal
@METHOD     : 
@GLOBALS    : 
@CALLS      :  
@CREATED    : May. 22, 1997    David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

static  void  convert_transform_origin_to_starts(
    Real               origin[],
    int                n_volume_dimensions,
    int                spatial_axes[],
    Real               dir_cosines[][N_DIMENSIONS],
    Real               starts[] )
{
    int         axis, dim, which[N_DIMENSIONS], n_axes, i, j;
    Real        o_dot_c, c_dot_c;
    Real        x_dot_x, x_dot_y, x_dot_v, y_dot_y, y_dot_v, bottom;
    Real        **matrix, solution[N_DIMENSIONS];

    for_less( dim, 0, n_volume_dimensions )
        starts[dim] = 0.0;

    /*--- get the list of valid axes (which) */

    n_axes = 0;
    for_less( dim, 0, N_DIMENSIONS )
    {
        axis = spatial_axes[dim];
        if( axis >= 0 )
        {
            which[n_axes] = axis;
            ++n_axes;
        }
    }

    /*--- get the starts: computed differently for 1, 2, or 3 axes */

    if( n_axes == 1 )
    {
        o_dot_c = dot_vectors( N_DIMENSIONS, origin, dir_cosines[which[0]] );
        c_dot_c = dot_vectors( N_DIMENSIONS, dir_cosines[which[0]],
                                     dir_cosines[which[0]] );

        if( c_dot_c != 0.0 )
            starts[which[0]] = o_dot_c / c_dot_c;
    }
    else if( n_axes == 2 )
    {
        x_dot_x = dot_vectors( N_DIMENSIONS, dir_cosines[which[0]],
                                     dir_cosines[which[0]] );
        x_dot_v = dot_vectors( N_DIMENSIONS, dir_cosines[which[0]], origin );
        x_dot_y = dot_vectors( N_DIMENSIONS, dir_cosines[which[0]],
                                     dir_cosines[which[1]] );
        y_dot_y = dot_vectors( N_DIMENSIONS, dir_cosines[which[1]],
                                     dir_cosines[which[1]] );
        y_dot_v = dot_vectors( N_DIMENSIONS, dir_cosines[which[1]], origin );

        bottom = x_dot_x * y_dot_y - x_dot_y * x_dot_y;

        if( bottom != 0.0 )
        {
            starts[which[0]] = (x_dot_v * y_dot_y - x_dot_y * y_dot_v) / bottom;
            starts[which[1]] = (y_dot_v * x_dot_x - x_dot_y * x_dot_v) / bottom;
        }
    }
    else if( n_axes == 3 )
    {
        /*--- this is the usual case, solve the equations to find what
              starts give the desired origin */

        ALLOC2D( matrix, N_DIMENSIONS, N_DIMENSIONS );

        for_less( i, 0, N_DIMENSIONS )
        for_less( j, 0, N_DIMENSIONS )
        {
            matrix[i][j] = dir_cosines[which[j]][i];
        }

        if( solve_linear_system( N_DIMENSIONS, matrix, origin, solution ) )
        {
            starts[which[0]] = solution[0];
            starts[which[1]] = solution[1];
            starts[which[2]] = solution[2];
        }

        FREE2D( matrix );
    }
    else
    {
        print_error(
          "Invalid number of axes in convert_transform_origin_to_starts\n");
    }
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : convert_transform_to_starts_and_steps
@INPUT      : transform
              separation_signs
@OUTPUT     : starts
              steps
              dir_cosines
              spatial_axes
@RETURNS    : 
@DESCRIPTION: Converts a linear transform to a set of 3 starts, 3 steps,
              and 3 direction cosines.  The separation signs determine
              the desired signs of each of the separations.
@METHOD     : 
@GLOBALS    : 
@CALLS      :  
@CREATED    : May. 20, 1997    David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  void  convert_transform_to_starts_and_steps(
    General_transform  *transform,
    int                n_volume_dimensions,
    Real               step_signs[],
    int                spatial_axes[],
    Real               starts[],
    Real               steps[],
    Real               dir_cosines[][N_DIMENSIONS] )
{
    Real        sign, mag;
    int         axis, dim;
    Real        axes[N_DIMENSIONS][N_DIMENSIONS];
    Real        origin[N_DIMENSIONS];
    Transform   *linear_transform;

    if( get_transform_type( transform ) != LINEAR )
    {
        print_error( "convert_transform_to_starts_and_steps(): non-linear transform found.\n" );
        return;
    }

    linear_transform = get_linear_transform_ptr( transform );

    get_transform_origin_real( linear_transform, origin );
    get_transform_x_axis_real( linear_transform, &axes[X][0] );
    get_transform_y_axis_real( linear_transform, &axes[Y][0] );
    get_transform_z_axis_real( linear_transform, &axes[Z][0] );

    /*--- assign default steps */

    for_less( dim, 0, n_volume_dimensions )
        steps[dim] = 1.0;

    /*--- assign the steps and dir_cosines for the spatial axes */

    for_less( dim, 0, N_DIMENSIONS )
    {
        axis = spatial_axes[dim];
        if( axis >= 0 )
        {
            mag = dot_vectors( N_DIMENSIONS, axes[dim], axes[dim] );

            if( mag <= 0.0 )
                mag = 1.0;
            mag = sqrt( mag );

            if( step_signs == NULL )
            {
                if( axes[dim][dim] < 0.0 )
                    sign = -1.0;
                else
                    sign = 1.0;
            }
            else  /*--- make the sign of steps match the step_signs passed in */
            {
                if( step_signs[axis] < 0.0 )
                    sign = -1.0;
                else
                    sign = 1.0;
            }

            steps[axis] = sign * mag;
            dir_cosines[axis][X] = axes[dim][X] / steps[axis];
            dir_cosines[axis][Y] = axes[dim][Y] / steps[axis];
            dir_cosines[axis][Z] = axes[dim][Z] / steps[axis];
        }
    }

    /*--- finally, get the starts */

    convert_transform_origin_to_starts( origin, n_volume_dimensions,
                                        spatial_axes, dir_cosines, starts );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : set_voxel_to_world_transform
@INPUT      : volume
              transform
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION: Sets the volume's transformation from voxel to world coords.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 1993            David MacDonald
@MODIFIED   : May  22, 1997   D. MacDonald - recomputes the starts/steps
---------------------------------------------------------------------------- */

VIOAPI  void  set_voxel_to_world_transform(
    Volume             volume,
    General_transform  *transform )
{
    assign_voxel_to_world_transform( volume, transform );
    volume->voxel_to_world_transform_uptodate = TRUE;

    if( get_transform_type( transform ) == LINEAR )
    {
        convert_transform_to_starts_and_steps( transform,
                                               get_volume_n_dimensions(volume),
                                               volume->separations,
                                               volume->spatial_axes,
                                               volume->starts,
                                               volume->separations,
                                               volume->direction_cosines );
    }
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : get_voxel_to_world_transform
@INPUT      : 
@OUTPUT     : 
@RETURNS    : transform
@DESCRIPTION: Returns a pointer to the voxel to world transform of the volume.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 1993            David MacDonald
@MODIFIED   : May  22, 1997   D. MacDonald - now delays recomputing transform
---------------------------------------------------------------------------- */

VIOAPI  General_transform  *get_voxel_to_world_transform(
    Volume   volume )
{
    check_recompute_world_transform( volume );

    return( &volume->voxel_to_world_transform );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : get_volume_dimension_names
@INPUT      : volume
@OUTPUT     : 
@RETURNS    : list of dimension names
@DESCRIPTION: Creates a copy of the dimension names of the volume.  Therefore,
              after use, the calling function must free the list, by calling
              delete_dimension_names().
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 1993            David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  STRING  *get_volume_dimension_names(
    Volume   volume )
{
    int      i;
    STRING   *names;

    ALLOC( names, get_volume_n_dimensions(volume) );

    for_less( i, 0, get_volume_n_dimensions(volume) )
        names[i] = create_string( volume->dimension_names[i] );

    for_less( i, 0, N_DIMENSIONS )
    {
        if( volume->spatial_axes[i] >= 0 )
        {
            replace_string( &names[volume->spatial_axes[i]],
                            create_string(
                                         convert_spatial_axis_to_dim_name(i)) );
        }
    }

    return( names );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : delete_dimension_names
@INPUT      : volume,
              dimension_names
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION: Frees the memory allocated to the dimension names, which came
              from the above function, get_volume_dimension_names().
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 1993            David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  void  delete_dimension_names(
    Volume   volume,
    STRING   dimension_names[] )
{
    int   i;

    for_less( i, 0, get_volume_n_dimensions(volume) )
        delete_string( dimension_names[i] );

    FREE( dimension_names );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : get_volume_space_type
@INPUT      : volume
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION: Returns a copy of the string representing the volume coordinate
              system name.  The calling function must delete_string() the
              value when done.
@METHOD     : 
@GLOBALS    : 
@CALLS      :  
@CREATED    : Nov. 15, 1996    David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  STRING  get_volume_space_type(
    Volume   volume )
{
    return( create_string( volume->coordinate_system_name ) );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : set_volume_space_type
@INPUT      : volume
              name
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION: Copies the name into the volume's coordinate system name.
              Copies the string, rather than just the pointer.
@METHOD     : 
@GLOBALS    : 
@CALLS      :  
@CREATED    : Nov. 15, 1996    David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  void  set_volume_space_type(
    Volume   volume,
    STRING   name )
{
    delete_string( volume->coordinate_system_name );
    volume->coordinate_system_name = create_string( name );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : get_volume_separations
@INPUT      : volume
@OUTPUT     : separations
@RETURNS    : 
@DESCRIPTION: Passes back the slice separations for each dimensions.  Assumes
              separations contains enough room for n_dimensions Reals.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : June, 1993           David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  void  get_volume_separations(
    Volume   volume,
    Real     separations[] )
{
    int   i;

    for_less( i, 0, get_volume_n_dimensions( volume ) )
        separations[i] = volume->separations[i];
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : set_volume_separations
@INPUT      : volume
              separations
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION: Sets the separations between slices for the given volume.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 1993            David MacDonald
@MODIFIED   : May  22, 1997   D. MacDonald - now delays recomputing transform
---------------------------------------------------------------------------- */

VIOAPI  void  set_volume_separations(
    Volume   volume,
    Real     separations[] )
{
    int   i;

    for_less( i, 0, get_volume_n_dimensions( volume ) )
        volume->separations[i] = separations[i];

    volume->voxel_to_world_transform_uptodate = FALSE;
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : set_volume_starts
@INPUT      : volume
              starts[]
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION: Sets the translation portion of the voxel to world transform,
              by specifying the start vector, as specified by the MINC format.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : May  20, 1997   David MacDonald
@MODIFIED   : May  22, 1997   D. MacDonald - now delays recomputing transform
---------------------------------------------------------------------------- */

VIOAPI  void  set_volume_starts(
    Volume  volume,
    Real    starts[] )
{
    int  c;

    for_less( c, 0, get_volume_n_dimensions( volume ) )
        volume->starts[c] = starts[c];

    volume->voxel_to_world_transform_uptodate = FALSE;
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : get_volume_starts
@INPUT      : volume
@OUTPUT     : starts
@RETURNS    : 
@DESCRIPTION: Passes back the start vector of the volume.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : May  20, 1997   David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  void  get_volume_starts(
    Volume  volume,
    Real    starts[] )
{
    int  c;

    for_less( c, 0, get_volume_n_dimensions( volume ) )
        starts[c] = volume->starts[c];
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : set_volume_direction_unit_cosine
@INPUT      : volume
              axis
              dir
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION: Sets the direction cosine for one axis, assumed to be unit
              length.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : May  20, 1997     David MacDonald
---------------------------------------------------------------------------- */

VIOAPI  void  set_volume_direction_unit_cosine(
    Volume   volume,
    int      axis,
    Real     dir[] )
{
    int    dim;

    if( axis < 0 || axis >= get_volume_n_dimensions(volume) )
    {
        print_error(
         "set_volume_direction_cosine:  cannot set dir cosine for axis %d\n",
          axis );
        return;
    }

    /*--- check if this is a spatial axis */

    for_less( dim, 0, N_DIMENSIONS )
    {
        if( volume->spatial_axes[dim] == axis )
            break;
    }

    if( dim == N_DIMENSIONS )   /* this is not a spatial axis, ignore the dir */
        return;

    volume->direction_cosines[axis][X] = dir[X];
    volume->direction_cosines[axis][Y] = dir[Y];
    volume->direction_cosines[axis][Z] = dir[Z];

    volume->voxel_to_world_transform_uptodate = FALSE;
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : set_volume_direction_cosine
@INPUT      : volume
              axis
              dir
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION: Sets the direction cosine for one axis.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 1993            David MacDonald
@MODIFIED   : May  20, 1997   D. MacDonald    - split into
                                            set_volume_direction_unit_cosine
---------------------------------------------------------------------------- */

VIOAPI  void  set_volume_direction_cosine(
    Volume   volume,
    int      axis,
    Real     dir[] )
{
    Real   len, unit_vector[N_DIMENSIONS];

    len = dir[X] * dir[X] + dir[Y] * dir[Y] + dir[Z] * dir[Z];

    if( len == 0.0 )
    {
        print_error( "Warning: zero length direction cosine in set_volume_direction_cosine()\n" );
        return;
    }

    if( len <= 0.0 )
        len = 1.0;

    len = sqrt( len );

    unit_vector[X] = dir[X] / len;
    unit_vector[Y] = dir[Y] / len;
    unit_vector[Z] = dir[Z] / len;

    set_volume_direction_unit_cosine( volume, axis, unit_vector );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : get_volume_direction_cosine
@INPUT      : volume
              axis
@OUTPUT     : dir
@RETURNS    : 
@DESCRIPTION: Passes back the direction cosine corresponding to the given
              voxel axis, which must be a spatial dimension.
@METHOD     : 
@GLOBALS    : 
@CALLS      :  
@CREATED    : Nov. 15, 1996    David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  void  get_volume_direction_cosine(
    Volume   volume,
    int      axis,
    Real     dir[] )
{
    int    d;

    if( axis < 0 || axis >= get_volume_n_dimensions(volume) )
    {
        print_error(
         "get_volume_direction_cosine:  cannot get dir cosine for axis %d\n",
          axis );
        return;
    }

    for_less( d, 0, N_DIMENSIONS )
    {
        if( volume->spatial_axes[d] == axis )
            break;
    }

    if( d == N_DIMENSIONS )   /* this is not a spatial axis, ignore the dir */
    {
        dir[X] = 0.0;
        dir[Y] = 0.0;
        dir[Z] = 0.0;
    }
    else
    {
        dir[X] = volume->direction_cosines[axis][X];
        dir[Y] = volume->direction_cosines[axis][Y];
        dir[Z] = volume->direction_cosines[axis][Z];
    }
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : set_volume_translation
@INPUT      : volume
              voxel
              world_space_voxel_maps_to
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION: Sets the translation portion of the volume so that the given
              voxel maps to the given world space position.  Rewrote this
              to provide backwards compatibility.
@METHOD     : 
@GLOBALS    : 
@CALLS      :  
@CREATED    : Aug. 31, 1997    David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  void  set_volume_translation(
    Volume  volume,
    Real    voxel[],
    Real    world_space_voxel_maps_to[] )
{
    int         dim, dim2, axis, n_axes, a1, a2;
    Real        world_space_origin[N_DIMENSIONS], len;
    Real        starts[MAX_DIMENSIONS], starts_3d[N_DIMENSIONS];
    Transform   transform, inverse;

    /*--- find the world position where ( 0, 0, 0 ) maps to by taking
          the world position - voxel[x_axis] * Xaxis - voxel[y_axis] * Yaxis
          ..., and fill in the transform defined by Xaxis, Yaxis, Zaxis */

    make_identity_transform( &transform );

    for_less( dim, 0, N_DIMENSIONS )
    {
        world_space_origin[dim] = world_space_voxel_maps_to[dim];

        for_less( dim2, 0, N_DIMENSIONS )
        {
            axis = volume->spatial_axes[dim2];
            if( axis >= 0 )
            {
                world_space_origin[dim] -= volume->separations[axis] *
                           volume->direction_cosines[axis][dim] * voxel[axis];

                Transform_elem( transform, dim, dim2 ) =
                                           volume->direction_cosines[axis][dim];
            }
        }
    }

    n_axes = 0;

    for_less( dim, 0, N_DIMENSIONS )
    {
        axis = volume->spatial_axes[dim];
        if( axis >= 0 )
            ++n_axes;
    }

    /*--- if only one spatial axis, make a second orthogonal vector */

    if( n_axes == 1 )
    {
        /*--- set dim to the spatial axis */

        if( volume->spatial_axes[0] >= 0 )
            dim = 0;
        else if( volume->spatial_axes[1] >= 0 )
            dim = 1;
        else if( volume->spatial_axes[2] >= 0 )
            dim = 2;

        /*--- set a1 to the lowest occuring non-spatial axis, and create
              a unit vector normal to that of the spatial axis */

        if( dim == 0 )
            a1 = 1;
        else
            a1 = 0;

        Transform_elem( transform, 0, a1 ) = Transform_elem(transform,1,dim) +
                                             Transform_elem(transform,2,dim);
        Transform_elem( transform, 1, a1 ) = -Transform_elem(transform,0,dim) -
                                              Transform_elem(transform,2,dim);
        Transform_elem( transform, 2, a1 ) = Transform_elem(transform,1,dim) -
                                             Transform_elem(transform,0,dim);

        len = Transform_elem(transform,0,a1)*Transform_elem(transform,0,a1) +
              Transform_elem(transform,1,a1)*Transform_elem(transform,1,a1) +
              Transform_elem(transform,2,a1)*Transform_elem(transform,2,a1);

        if( len == 0.0 )
            len = 1.0;
        else
            len = sqrt( len );

        Transform_elem(transform,0,a1) /= len;
        Transform_elem(transform,1,a1) /= len;
        Transform_elem(transform,2,a1) /= len;
    }

    /*--- if only two spatial axis, make a third orthogonal vector */

    if( n_axes == 1 || n_axes == 2 )
    {
        /*--- set dim to the one axis that does not have a vector associated
              with it yet, and make one that is the unit cross product of 
              the other two */

        if( volume->spatial_axes[2] < 0 )
            dim = 2;
        else if( volume->spatial_axes[1] < 0 )
            dim = 1;
        else if( volume->spatial_axes[0] < 0 )
            dim = 0;

        a1 = (dim + 1) % N_DIMENSIONS;
        a2 = (dim + 2) % N_DIMENSIONS;

        /*--- take cross product */

        Transform_elem( transform, 0, dim ) = Transform_elem(transform,1,a1) *
                                              Transform_elem(transform,2,a2) -
                                              Transform_elem(transform,1,a2) *
                                              Transform_elem(transform,2,a1);
        Transform_elem( transform, 1, dim ) = Transform_elem(transform,2,a1) *
                                              Transform_elem(transform,0,a2) -
                                              Transform_elem(transform,2,a2) *
                                              Transform_elem(transform,0,a1);
        Transform_elem( transform, 2, dim ) = Transform_elem(transform,0,a1) *
                                              Transform_elem(transform,1,a2) -
                                              Transform_elem(transform,0,a2) *
                                              Transform_elem(transform,1,a1);

        /*--- normalize vector */

        len = Transform_elem(transform,0,dim)*Transform_elem(transform,0,dim) +
              Transform_elem(transform,1,dim)*Transform_elem(transform,1,dim) +
              Transform_elem(transform,2,dim)*Transform_elem(transform,2,dim);

        if( len == 0.0 )
            len = 1.0;
        else
            len = sqrt( len );

        Transform_elem(transform,0,dim) /= len;
        Transform_elem(transform,1,dim) /= len;
        Transform_elem(transform,2,dim) /= len;
    }

    /*--- find the voxel that maps to the world space origin, when there is
          no translation, and this is the starts */

    compute_transform_inverse( &transform, &inverse );

    transform_point( &inverse, world_space_origin[X],
                               world_space_origin[Y],
                               world_space_origin[Z],
                               &starts_3d[X], &starts_3d[Y], &starts_3d[Z] );

    /*--- map the X Y Z starts into the arbitrary axis ordering of the volume */

    for_less( dim, 0, get_volume_n_dimensions(volume) )
        starts[dim] = 0.0;

    for_less( dim, 0, N_DIMENSIONS )
    {
        axis = volume->spatial_axes[dim];
        if( axis >= 0 )
            starts[axis] = starts_3d[dim];
    }

    set_volume_starts( volume, starts );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : get_volume_translation
@INPUT      : volume
@OUTPUT     : voxel                     - returns 0, 0, 0 ...
              world_space_voxel_maps_to - returns centre of voxel [0][0][0]...
@RETURNS    : 
@DESCRIPTION: Reinstated this old function for backward compatibility.
              Simply returns the voxel 0, 0, 0, and the world
              coordinate of its centre, to indicate the translational
              component of the transformation.
@METHOD     : 
@GLOBALS    : 
@CALLS      :  
@CREATED    : May. 23, 1998    David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  void  get_volume_translation(
    Volume  volume,
    Real    voxel[],
    Real    world_space_voxel_maps_to[] )
{
    int   dim;

    for_less( dim, 0, get_volume_n_dimensions(volume) )
        voxel[dim] = 0.0;

    convert_voxel_to_world( volume, voxel, &world_space_voxel_maps_to[X],
                                           &world_space_voxel_maps_to[Y],
                                           &world_space_voxel_maps_to[Z] );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : reorder_voxel_to_xyz
@INPUT      : volume
              voxel
@OUTPUT     : xyz
@RETURNS    : 
@DESCRIPTION: Passes back the voxel coordinates corresponding to the x, y,
              and z axes, if any.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : May 21, 1995    David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  void  reorder_voxel_to_xyz(
    Volume   volume,
    Real     voxel[],
    Real     xyz[] )
{
    int   c, axis;

    for_less( c, 0, N_DIMENSIONS )
    {
        axis = volume->spatial_axes[c];
        if( axis >= 0 )
            xyz[c] = voxel[axis];
        else
            xyz[c] = 0.0;
    }
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : reorder_xyz_to_voxel
@INPUT      : volume
              xyz
@OUTPUT     : voxel
@RETURNS    : 
@DESCRIPTION: Passes back the voxel coordinates converted from those
              corresponding to the x, y, and z axis.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : Jun 21, 1995    David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  void  reorder_xyz_to_voxel(
    Volume   volume,
    Real     xyz[],
    Real     voxel[] )
{
    int   c, axis, n_dims;

    n_dims = get_volume_n_dimensions( volume );
    for_less( c, 0, n_dims )
        voxel[c] = 0.0;

    for_less( c, 0, N_DIMENSIONS )
    {
        axis = volume->spatial_axes[c];
        if( axis >= 0 )
            voxel[axis] = xyz[c];
    }
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : convert_voxel_to_world
@INPUT      : volume
              x_voxel
              y_voxel
              z_voxel
@OUTPUT     : x_world
              y_world
              z_world
@RETURNS    : 
@DESCRIPTION: Converts the given voxel position to a world coordinate.
              Note that centre of first voxel corresponds to (0.0,0.0,0.0) in
              voxel coordinates.
@CREATED    : Mar   1993           David MacDonald
@MODIFIED   : May  22, 1997   D. MacDonald - checks to recompute transform
---------------------------------------------------------------------------- */

VIOAPI  void  convert_voxel_to_world(
    Volume   volume,
    Real     voxel[],
    Real     *x_world,
    Real     *y_world,
    Real     *z_world )
{
    Real   xyz[N_DIMENSIONS];

    check_recompute_world_transform( volume );

    reorder_voxel_to_xyz( volume, voxel, xyz );

    /* apply linear transform */

    general_transform_point( &volume->voxel_to_world_transform,
                             xyz[X], xyz[Y], xyz[Z],
                             x_world, y_world, z_world );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : convert_3D_voxel_to_world
@INPUT      : volume
              voxel1
              voxel2
              voxel3
@OUTPUT     : x_world
              y_world
              z_world
@RETURNS    : 
@DESCRIPTION: Convenience function which performs same task as
              convert_voxel_to_world(), but for 3D volumes only.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 1993            David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  void  convert_3D_voxel_to_world(
    Volume   volume,
    Real     voxel1,
    Real     voxel2,
    Real     voxel3,
    Real     *x_world,
    Real     *y_world,
    Real     *z_world )
{
    Real   voxel[MAX_DIMENSIONS];

    if( get_volume_n_dimensions(volume) != 3 )
    {
        print_error( "convert_3D_voxel_to_world:  Volume must be 3D.\n" );
        return;
    }

    voxel[0] = voxel1;
    voxel[1] = voxel2;
    voxel[2] = voxel3;

    convert_voxel_to_world( volume, voxel, x_world, y_world, z_world );
}

/* ----------------------------- MNI Header -----------------------------------@NAME       : convert_voxel_normal_vector_to_world
@INPUT      : volume
              voxel_vector0
              voxel_vector1
              voxel_vector2
@OUTPUT     : x_world
              y_world
              z_world
@RETURNS    :
@DESCRIPTION: Converts a voxel vector to world coordinates.  Assumes the
              vector is a normal vector (ie. a derivative), so transforms by
              transpose of inverse transform.
@CREATED    : Mar   1993           David MacDonald
@MODIFIED   : May  22, 1997   D. MacDonald - checks to recompute transform
---------------------------------------------------------------------------- */

VIOAPI  void  convert_voxel_normal_vector_to_world(
    Volume          volume,
    Real            voxel_vector[],
    Real            *x_world,
    Real            *y_world,
    Real            *z_world )
{
    Real        xyz[N_DIMENSIONS];
    Transform   *inverse;

    check_recompute_world_transform( volume );

    if( get_transform_type( &volume->voxel_to_world_transform ) != LINEAR )
        handle_internal_error( "Cannot get normal vector of nonlinear xforms.");

    inverse = get_inverse_linear_transform_ptr(
                                      &volume->voxel_to_world_transform );

    /* transform vector by transpose of inverse transformation */

    reorder_voxel_to_xyz( volume, voxel_vector, xyz );

    *x_world = Transform_elem(*inverse,0,0) * xyz[X] +
               Transform_elem(*inverse,1,0) * xyz[Y] +
               Transform_elem(*inverse,2,0) * xyz[Z];
    *y_world = Transform_elem(*inverse,0,1) * xyz[X] +
               Transform_elem(*inverse,1,1) * xyz[Y] +
               Transform_elem(*inverse,2,1) * xyz[Z];
    *z_world = Transform_elem(*inverse,0,2) * xyz[X] +
               Transform_elem(*inverse,1,2) * xyz[Y] +
               Transform_elem(*inverse,2,2) * xyz[Z];
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : convert_voxel_vector_to_world
@INPUT      : volume
              voxel_vector
@OUTPUT     : x_world
              y_world
              z_world
@RETURNS    : 
@DESCRIPTION: Converts a voxel vector to world coordinates.
@CREATED    : Mar   1993           David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  void  convert_voxel_vector_to_world(
    Volume          volume,
    Real            voxel_vector[],
    Real            *x_world,
    Real            *y_world,
    Real            *z_world )
{
    int         i;
    Real        origin[MAX_DIMENSIONS], x0, y0, z0, x1, y1, z1;

    for_less( i, 0, MAX_DIMENSIONS )
        origin[i] = 0.0;

    convert_voxel_to_world( volume, origin, &x0, &y0, &z0 );

    convert_voxel_to_world( volume, voxel_vector, &x1, &y1, &z1 );

    *x_world = x1 - x0;
    *y_world = y1 - y0;
    *z_world = z1 - z0;
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : convert_world_vector_to_voxel
@INPUT      : volume
              x_world
              y_world
              z_world
@OUTPUT     : voxel_vector
@RETURNS    : 
@DESCRIPTION: Converts a world vector to voxel coordinates.
@CREATED    : Mar   1993           David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  void  convert_world_vector_to_voxel(
    Volume          volume,
    Real            x_world,
    Real            y_world,
    Real            z_world,
    Real            voxel_vector[] )
{
    int         c;
    Real        voxel[MAX_DIMENSIONS], origin[MAX_DIMENSIONS];

    convert_world_to_voxel( volume, 0.0, 0.0, 0.0, origin );
    convert_world_to_voxel( volume, x_world, y_world, z_world, voxel );

    for_less( c, 0, get_volume_n_dimensions(volume) )
        voxel_vector[c] = voxel[c] - origin[c];
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : convert_world_to_voxel
@INPUT      : volume
              x_world
              y_world
              z_world
@OUTPUT     : x_voxel
              y_voxel
              z_voxel
@RETURNS    : 
@DESCRIPTION: Converts from world coordinates to voxel coordinates.
@CREATED    : Mar   1993           David MacDonald
@MODIFIED   : May  22, 1997   D. MacDonald - checks to recompute transform
---------------------------------------------------------------------------- */

VIOAPI  void  convert_world_to_voxel(
    Volume   volume,
    Real     x_world,
    Real     y_world,
    Real     z_world,
    Real     voxel[] )
{
    Real   xyz[N_DIMENSIONS];

    check_recompute_world_transform( volume );

    general_inverse_transform_point( &volume->voxel_to_world_transform,
                                     x_world, y_world, z_world,
                                     &xyz[X], &xyz[Y], &xyz[Z] );

    reorder_xyz_to_voxel( volume, xyz, voxel );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : convert_3D_world_to_voxel
@INPUT      : volume
              x_world
              y_world
              z_world
@OUTPUT     : voxel1
              voxel2
              voxel3
@RETURNS    : 
@DESCRIPTION: Convenience function that does same task as
              convert_world_to_voxel(), but only for 3D volumes.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 1993            David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  void  convert_3D_world_to_voxel(
    Volume   volume,
    Real     x_world,
    Real     y_world,
    Real     z_world,
    Real     *voxel1,
    Real     *voxel2,
    Real     *voxel3 )
{
    Real   voxel[MAX_DIMENSIONS];

    if( get_volume_n_dimensions(volume) != 3 )
    {
        print_error( "convert_3D_world_to_voxel:  Volume must be 3D.\n" );
        return;
    }

    convert_world_to_voxel( volume, x_world, y_world, z_world, voxel );

    *voxel1 = voxel[X];
    *voxel2 = voxel[Y];
    *voxel3 = voxel[Z];
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : get_volume_voxel_min
@INPUT      : volume
@OUTPUT     : 
@RETURNS    : min valid voxel 
@DESCRIPTION: Returns the minimum valid voxel value.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 1993            David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  Real  get_volume_voxel_min(
    Volume   volume )
{
    return( volume->voxel_min );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : get_volume_voxel_max
@INPUT      : volume
@OUTPUT     : 
@RETURNS    : max valid voxel 
@DESCRIPTION: Returns the maximum valid voxel value.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 1993            David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  Real  get_volume_voxel_max(
    Volume   volume )
{
    return( volume->voxel_max );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : get_volume_voxel_range
@INPUT      : volume
@OUTPUT     : voxel_min
              voxel_max
@RETURNS    : 
@DESCRIPTION: Passes back the min and max voxel values stored in the volume.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : June, 1993           David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  void  get_volume_voxel_range(
    Volume     volume,
    Real       *voxel_min,
    Real       *voxel_max )
{
    *voxel_min = get_volume_voxel_min( volume );
    *voxel_max = get_volume_voxel_max( volume );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : set_volume_voxel_range
@INPUT      : volume
              voxel_min
              voxel_max
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION: Sets the valid range of voxels.  If an invalid range is
              specified (voxel_min >= voxel_max), the full range of the
              volume's type is used.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 1993            David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  void  set_volume_voxel_range(
    Volume   volume,
    Real     voxel_min,
    Real     voxel_max )
{
    Real  real_min, real_max;

    if( voxel_min >= voxel_max )
    {
        switch( get_volume_data_type( volume ) )
        {
        case UNSIGNED_BYTE:
            voxel_min = 0.0;
            voxel_max = (Real) UCHAR_MAX;     break;
        case SIGNED_BYTE:
            voxel_min = (Real) SCHAR_MIN;
            voxel_max = (Real) SCHAR_MAX;     break;
        case UNSIGNED_SHORT:
            voxel_min = 0.0;
            voxel_max = (Real) USHRT_MAX;     break;
        case SIGNED_SHORT:
            voxel_min = (Real) SHRT_MIN;
            voxel_max = (Real) SHRT_MAX;      break;
        case UNSIGNED_INT:
            voxel_min = 0.0;
            voxel_max = (Real) UINT_MAX;     break;
        case SIGNED_INT:
            voxel_min = (Real) INT_MIN;
            voxel_max = (Real) INT_MAX;      break;
        case FLOAT:
            voxel_min = (Real) -FLT_MAX;
            voxel_max = (Real) FLT_MAX;       break;
        case DOUBLE:
            voxel_min = (Real) -DBL_MAX;
            voxel_max = (Real) DBL_MAX;       break;
        }
    }

    if( volume->real_range_set )
        get_volume_real_range( volume, &real_min, &real_max );

    volume->voxel_min = voxel_min;
    volume->voxel_max = voxel_max;

    if( volume->real_range_set )
        set_volume_real_range( volume, real_min, real_max );
    else
        cache_volume_range_has_changed( volume );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : get_volume_real_range
@INPUT      : volume
@OUTPUT     : min_value
              max_value
@RETURNS    : 
@DESCRIPTION: Passes back the minimum and maximum scaled values.  These are
              the minimum and maximum stored voxel values scaled to the
              real value domain.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : June, 1993           David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  void  get_volume_real_range(
    Volume     volume,
    Real       *min_value,
    Real       *max_value )
{
    *min_value = get_volume_real_min( volume );
    *max_value = get_volume_real_max( volume );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : get_volume_real_min
@INPUT      : volume
@OUTPUT     : 
@RETURNS    : real range minimum
@DESCRIPTION: Returns the minimum of the real range of the volume.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 1993            David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  Real  get_volume_real_min(
    Volume     volume )
{
    Real   real_min;

    real_min = get_volume_voxel_min( volume );

    if( volume->real_range_set )
        real_min = convert_voxel_to_value( volume, real_min );

    return( real_min );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : get_volume_real_max
@INPUT      : volume
@OUTPUT     : 
@RETURNS    : real range max
@DESCRIPTION: Returns the maximum of the real range of the volume.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 1993            David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  Real  get_volume_real_max(
    Volume     volume )
{
    Real   real_max;

    real_max = get_volume_voxel_max( volume );

    if( volume->real_range_set )
        real_max = convert_voxel_to_value( volume, real_max );

    return( real_max );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : set_volume_real_range
@INPUT      : volume
              real_min
              real_max
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION: Sets the range of real values to which the valid voxel
              range maps
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 1993            David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  void  set_volume_real_range(
    Volume   volume,
    Real     real_min,
    Real     real_max )
{
    Real    voxel_min, voxel_max;

    if( get_volume_data_type(volume) == FLOAT ||
        get_volume_data_type(volume) == DOUBLE )
    {
		/* as float and double use the voxel range */
        volume->real_range_set = FALSE;

        set_volume_voxel_range( volume, real_min, real_max );
        
		/* these really shouldn't be needed but let's be "safe" */
		volume->real_value_scale = 1.0;
        volume->real_value_translation = 0.0;
    }
    else
    {
        get_volume_voxel_range( volume, &voxel_min, &voxel_max );

        if( voxel_min < voxel_max )
        {
            volume->real_value_scale = (real_max - real_min) /
                                       (voxel_max - voxel_min);
            volume->real_value_translation = real_min -
                                       voxel_min * volume->real_value_scale;
        }
        else
        {
	    // FIXME: is scale = 0 correct??
            volume->real_value_scale = 0.0;
            volume->real_value_translation = real_min;
        }

        volume->real_range_set = TRUE;
    }

    if( volume->is_cached_volume )
        cache_volume_range_has_changed( volume );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : copy_volume_definition_no_alloc
@INPUT      : volume
              nc_data_type
              signed_flag
              voxel_min
              voxel_max
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION: Copies the volume to a new volume, optionally changing type
              (if nc_data_type is not MI_ORIGINAL_TYPE), but not allocating
              the volume voxel data (alloc_volume_data() must subsequently
              be called).
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 1993            David MacDonald
@MODIFIED   : Nov. 15, 1996    D. MacDonald    - handles space type
---------------------------------------------------------------------------- */

VIOAPI  Volume   copy_volume_definition_no_alloc(
    Volume   volume,
    nc_type  nc_data_type,
    BOOLEAN  signed_flag,
    Real     voxel_min,
    Real     voxel_max )
{
    int                c, sizes[MAX_DIMENSIONS];
    Real               separations[MAX_DIMENSIONS];
    Real               starts[MAX_DIMENSIONS];
    Real               dir_cosine[N_DIMENSIONS];
    Volume             copy;

    if( nc_data_type == MI_ORIGINAL_TYPE )
    {
        nc_data_type = volume->nc_data_type;
        signed_flag = volume->signed_flag;
        get_volume_voxel_range( volume, &voxel_min, &voxel_max );
    }

    copy = create_volume( get_volume_n_dimensions(volume),
                          volume->dimension_names, nc_data_type, signed_flag,
                          voxel_min, voxel_max );

    for_less( c, 0, N_DIMENSIONS )
        copy->spatial_axes[c] = volume->spatial_axes[c];

    set_volume_real_range( copy,
                           get_volume_real_min(volume),
                           get_volume_real_max(volume) );

    get_volume_sizes( volume, sizes );
    set_volume_sizes( copy, sizes );

    get_volume_separations( volume, separations );
    set_volume_separations( copy, separations );

    get_volume_starts( volume, starts );
    set_volume_starts( copy, starts );

    for_less( c, 0, get_volume_n_dimensions(volume) )
    {
        get_volume_direction_cosine( volume, c, dir_cosine );
        set_volume_direction_unit_cosine( copy, c, dir_cosine );
    }

    set_volume_space_type( copy, volume->coordinate_system_name );
    
    for_less( c, 0, get_volume_n_dimensions(volume) )
    {
        if (is_volume_dimension_irregular(volume, c)) {
            Real *irr_starts = malloc(sizeof(Real) * sizes[c]);
            Real *irr_widths = malloc(sizeof(Real) * sizes[c]);
            get_volume_irregular_starts( volume, c, sizes[c], irr_starts);
            set_volume_irregular_starts( volume, c, sizes[c], irr_starts);

            get_volume_irregular_widths( volume, c, sizes[c], irr_widths);
            set_volume_irregular_widths( volume, c, sizes[c], irr_widths);
            free( irr_starts );
            free( irr_widths );
        }
    }

    return( copy );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : copy_volume_definition
@INPUT      : volume
              nc_data_type
              signed_flag
              voxel_min
              voxel_max
@OUTPUT     : 
@RETURNS    : 
@DESCRIPTION: Copies the volume to a new volume, optionally changing type
              (if nc_data_type is not MI_ORIGINAL_TYPE), allocating
              the volume voxel data, but not initializing the data.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : 1993            David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  Volume   copy_volume_definition(
    Volume   volume,
    nc_type  nc_data_type,
    BOOLEAN  signed_flag,
    Real     voxel_min,
    Real     voxel_max )
{
    Volume   copy;

    copy = copy_volume_definition_no_alloc( volume,
                                            nc_data_type, signed_flag,
                                            voxel_min, voxel_max );
    alloc_volume_data( copy );

    return( copy );
}

/* ----------------------------- MNI Header -----------------------------------
@NAME       : copy_volume
@INPUT      : volume
@OUTPUT     : 
@RETURNS    : copy of volume
@DESCRIPTION: Creates an exact copy of a volume, including voxel values.
@METHOD     : 
@GLOBALS    : 
@CALLS      : 
@CREATED    : Jun 21, 1995    David MacDonald
@MODIFIED   : 
---------------------------------------------------------------------------- */

VIOAPI  Volume  copy_volume(
    Volume   volume )
{
    Volume   copy;
    void     *src, *dest;
    int      d, n_voxels, sizes[MAX_DIMENSIONS];

    if( volume->is_cached_volume )
    {
        print_error(
               "copy_volume():  copying cached volumes not implemented.\n" );

        return( NULL );
    }

    copy = copy_volume_definition( volume, MI_ORIGINAL_TYPE, FALSE, 0.0, 0.0 );

    /* --- find out how many voxels are in the volume */

    get_volume_sizes( volume, sizes );
    n_voxels = 1;
    for_less( d, 0, get_volume_n_dimensions(volume) )
        n_voxels *= sizes[d];

    /* --- get a pointer to the beginning of the voxels */

    GET_VOXEL_PTR( src, volume, 0, 0, 0, 0, 0 );
    GET_VOXEL_PTR( dest, copy, 0, 0, 0, 0, 0 );

    /* --- assuming voxels are contiguous, copy them in one chunk */

    (void) memcpy( dest, src, (size_t) n_voxels *
                              (size_t) get_type_size(
                                         get_volume_data_type(volume)) );

    return( copy );
}

/* These are not public functions, so they are not VIOAPI yet */
BOOLEAN 
is_volume_dimension_irregular(Volume volume, int idim)
{
    if (idim > volume->array.n_dimensions) {
        return (0);
    }
    return (volume->irregular_starts[idim] != NULL);
}

int
get_volume_irregular_starts(Volume volume, int idim, int count, Real *starts)
{
    int i;

    if (idim >= volume->array.n_dimensions) {
        return (0);
    }

    if (volume->irregular_starts[idim] == NULL) {
        return (0);
    }

    if (count > volume->array.sizes[idim]) {
        count = volume->array.sizes[idim];
    }

    for (i = 0; i < count; i++) {
        starts[i] = volume->irregular_starts[idim][i];
    }

    return (count);
}

int
get_volume_irregular_widths(Volume volume, int idim, int count, Real *widths)
{
    int i;

    if (idim >= volume->array.n_dimensions) {
        return (0);
    }

    if (volume->irregular_widths[idim] == NULL) {
        return (0);
    }

    if (count > volume->array.sizes[idim]) {
        count = volume->array.sizes[idim];
    }

    for (i = 0; i < count; i++) {
        widths[i] = volume->irregular_widths[idim][i];
    }

    return (count);
}

int
set_volume_irregular_starts(Volume volume, int idim, int count, Real *starts)
{
    int i;

    if (idim >= volume->array.n_dimensions) {
        return (0);
    }

    if (volume->irregular_starts[idim] != NULL) {
        free(volume->irregular_starts[idim]);
    }

    if (starts == NULL) {
        return (0);
    }

    if (count > volume->array.sizes[idim]) {
        count = volume->array.sizes[idim];
    }

    volume->irregular_starts[idim] = malloc(count * sizeof (Real));
    if (volume->irregular_starts[idim] == NULL) {
        return (0);
    }

    for (i = 0; i < count; i++) {
        volume->irregular_starts[idim][i] = starts[i];
    }

    return (count);
}

int
set_volume_irregular_widths(Volume volume, int idim, int count, Real *widths)
{
    int i;

    if (idim >= volume->array.n_dimensions) {
        return (0);
    }

    if (volume->irregular_widths[idim] != NULL) {
        free(volume->irregular_widths[idim]);
    }

    if (widths == NULL) {
        return (0);
    }

    if (count > volume->array.sizes[idim]) {
        count = volume->array.sizes[idim];
    }

    volume->irregular_widths[idim] = malloc(count * sizeof (Real));
    if (volume->irregular_widths[idim] == NULL) {
        return (0);
    }

    for (i = 0; i < count; i++) {
        volume->irregular_widths[idim][i] = widths[i];
    }

    return (count);
}


VIOAPI VIO_Real
nonspatial_voxel_to_world(Volume volume, int idim, int voxel)
{
    VIO_Real world;

    if (is_volume_dimension_irregular(volume, idim)) {
        if (voxel < 0) {
            world = 0.0;
        }
        else if (voxel >= volume->array.sizes[idim]) {
            /* If we are asking for a position PAST the end of the axis,
             * return the very last position on the axis, defined as the
             * last start position plus the last width.
             * NOTE! TODO: FIXME! We should take the axis alignment into
             * account here.
             */
            voxel = volume->array.sizes[idim] - 1;

            world = (volume->irregular_starts[idim][voxel] + 
                     volume->irregular_widths[idim][voxel]);
        }
        else {
            world = volume->irregular_starts[idim][voxel];
        }
    }
    else {
        world = volume->starts[idim] + (voxel * volume->separations[idim]);
    }
    return (world);
}

VIOAPI int
nonspatial_world_to_voxel(Volume volume, int idim, VIO_Real world)
{
    int voxel;
    int i;

    if (is_volume_dimension_irregular(volume, idim)) {
        voxel = volume->array.sizes[idim];
        for (i = 0; i < volume->array.sizes[idim]; i++) {
            if (world < (volume->irregular_starts[idim][i] + 
                         volume->irregular_widths[idim][i])) {
                voxel = i;
                break;
            }
        }
    }
    else {
        voxel = ROUND((world - volume->starts[idim]) / volume->separations[idim]);
    }
    return (voxel);
}
