/*
 *  model.c
 *  This file is part of LIME, the versatile line modeling engine
 *
 *  Copyright (C) 2006-2014 Christian Brinch
 *  Copyright (C) 2015-2016 The LIME development team
 *
 */

#include "lime.h"

/******************************************************************************/

void
input(inputPars *par, image *img){
  int i;

  /*
   * Basic parameters. See cheat sheet for details.
   */
  par->radius                   = 2000*AU;
  par->minScale                 = 0.5*AU;
  par->pIntensity               = 4000;
  par->sinkPoints               = 3000;
  par->dust                     = "jena_thin_e6.tab";
  par->moldatfile[0]            = "hco+@xpol.dat";
  par->antialias                = 4;
  par->sampling                 = 2; // log distr. for radius, directions distr. uniformly on a sphere.
  par->nSolveIters              = 14;
  par->resetRNG	                = 0;

  /*
    Setting elements of the following three arrays is optional. NOTE
    that, if you do set any of their values, you should set as many as
    the number of elements returned by your function density(). The
    ith element of the array in question will then be assumed to refer
    to the ith element in the density function return. The current
    maximum number of elements allowed is 7, which is the number of
    types of collision partner recognized in the LAMBDA database.

    Note that there is no (longer) a hard connection between the
    number of density elements and the number of collision-partner
    species named in the moldata files. This means in practice that,
    if you set the values for par->collPartIds, you can, if you like,
    set some for which there are no transition rates supplied in the
    moldatfiles. This might happen for example if there is a molecule
    which contributes significantly to the total molecular density but
    for which there are no measured collision rates for the radiating
    species you are interested in.

    You may also omit to mention in par->collPartIds a collision
    partner which is specified in the moldatfiles. In this case LIME
    will assume the density of the respective molecules is zero.

    If you don't set any values for any or all of these arrays,
    i.e. if you omit any mention of them here (we preserve this
    possibility for purposes of backward compatibility), LIME will
    attempt to replicate the algorithms employed in version 1.5, which
    involve guessing which collision partner corresponds to which
    density element. Since this was not exactly a rigorous procedure,
    we recommend use of the arrays.

    par->nMolWeights: this specifies how you want the number density
    of each radiating species to be calculated. At each grid point a
    sum (weighted by par->nMolWeights) of the density values is made,
    then this is multiplied by the abundance to return the number
    density.

    par->dustWeights: this is similar, but the weighted sum of
    densities now feeds into the calculation of the dust opacity.

    Note that there are convenient macros defined in ../src/lime.h for
    7 types of collision partner.

    Below is an example of how you might use these parameters:
  */

  par->collPartIds[0]           = CP_H2;
  par->nMolWeights[0]           = 1.0;
  par->dustWeights[0]           = 1.0;

  /* All output filenames can be custom-set or LIME can automatically assign filenames using the number of grid and
     sink points and an appropriate filename extension. The user can also set a prefix to be added to all outputted
     files if "[prefix]" is contained within the output filename. Note if "[prefix]" is specified then par->fileprefix
     must also be specified.

     In the case of images they are automatically named using the number of grid and sink points and image inclination
     information (in degrees). If it is a continuum image then the frequency is added (in GHz), otherwise for a line
     image the molecule and transition are added. The physical name of the unit is then appended, and finally, as with
     all other output filenames, an appropriate extension is appended.

            Example:
                par->pIntensity = 25000
                par->sinkPoints = 2500

                par->fileprefix = "simulation"
                par->outputfile = "[prefix]_custom_identifier"
                par->binoutputfile = ""
                par->gridfile = "custom_gridfile_name"

                img[0].freq = 300E9; img[0].posang = PI/2; img[0].incl = -PI/2; img[0].azimuth = 0;
                img[0].unit = 0;
                img[0].filename = ""

            This would output the following the files:
                "simulation_custom_identifier_25000_2500.pop"
                "25000_2500_restart.pop"
                "custom_gridfile_name.vtk"
                "25000_2500_300GHz_PA=-90_i=-90_AZ=0_Kelvin.fits
      */

  par->fileprefix               = "custom_prefix";

  par->outputfile               = "populations.pop";
  par->binoutputfile            = "restart.pop";
  par->gridfile                 = "grid.vtk";

  /* Set one or more of the following parameters for full output of the grid-specific data at any of 4 stages during the processing. (See the header of gridio.c for information about the stages.)
  par->gridOutFiles[0] = "grid_1.ds";
  par->gridOutFiles[1] = "grid_2.ds";
  par->gridOutFiles[2] = "grid_3.ds";
  par->gridOutFiles[3] = "grid_4.ds";
  */

  /* You can also optionally read in a FITS file stored via the previous parameters, or prepared externally. See the header of grid2fits.c for information about the correct file format. LIME can cope with almost any sensible subset of the recognized columns; it will use the file values if they are present, then calculate the missing ones.
  par->gridInFile = "grid_4.ds";
  */


  /*
   * Definitions for image #0. Add blocks with successive values of i for additional images.
   */
  i=0;
  img[i].nchan                  = 61;             // Number of channels
  img[i].velres                 = 500.;           // Channel resolution in m/s
  img[i].trans                  = 3;              // zero-indexed J quantum number
  img[i].pxls                   = 100;            // Pixels per dimension
  img[i].imgres                 = 0.1;            // Resolution in arc seconds
  img[i].distance               = 140*PC;         // source distance in m
  img[i].source_vel             = 0;              // source velocity in m/s
  img[i].azimuth                = 0.0;
  img[i].incl                   = 0.0;
  img[i].posang                 = 0.0;
  /* For each set of image parameters above, numerous images with different units can be outputted. This is done by
   * setting img[].units to a delimited (space, comma, colon, underscore) string of the required outputs, where:
   *        0:Kelvin
   *        1:Jansky/pixel
   *        2:SI
   *        3:Lsun/pixel
   *        4:tau
   * The unit name and file extension is then added automatically.
  */
  img[i].units                  = "0 1";
  img[i].filename               = "image0";  // Output filename
}

/******************************************************************************/

void
density(double x, double y, double z, double *density){
  /*
   * Define variable for radial coordinate
   */
  double r, rToUse;
  const double rMin = 0.1*AU; /* This cutoff should be chosen smaller than par->minScale but greater than zero (to avoid a singularity at the origin). */

  /*
   * Calculate radial distance from origin
   */
  r=sqrt(x*x+y*y+z*z);
  /*
   * Calculate a spherical power-law density profile
   * (Multiply with 1e6 to go to SI-units)
   */
  if(r>rMin)
    rToUse = r;
  else
    rToUse = rMin; /* Just to prevent overflows at r==0! */

  density[0] = 1.5e6*pow(rToUse/(300*AU),-1.5)*1e6;
}

/******************************************************************************/

void
temperature(double x, double y, double z, double *temperature){
  int i,x0=0;
  double r;
  /*
   * Array containing temperatures as a function of radial
   * distance from origin (this is an example of a tabulated model)
   */
  double temp[2][10] = {
      {2.0e13, 5.0e13, 8.0e13, 1.1e14, 1.4e14, 1.7e14, 2.0e14, 2.3e14, 2.6e14, 2.9e14},
      {44.777, 31.037, 25.718, 22.642, 20.560, 19.023, 17.826, 16.857, 16.050, 15.364}
  };
  /*
   * Calculate radial distance from origin
   */
  r=sqrt(x*x+y*y+z*z);
  /*
   * Linear interpolation in temperature input
   */
  if(r > temp[0][0] && r<temp[0][9]){
    for(i=0;i<9;i++){
      if(r>temp[0][i] && r<temp[0][i+1]) x0=i;
    }
  }
  if(r<temp[0][0])
    temperature[0]=temp[1][0];
  else if (r>temp[0][9])
    temperature[0]=temp[1][9];
  else
    temperature[0]=temp[1][x0]+(r-temp[0][x0])*(temp[1][x0+1]-temp[1][x0])/(temp[0][x0+1]-temp[0][x0]);
}

/******************************************************************************/

void
abundance(double x, double y, double z, double *abundance){
  /*
   * Here we use a constant abundance. Could be a
   * function of (x,y,z).
   */
  abundance[0] = 1.e-9;
}

/******************************************************************************/

void
doppler(double x, double y, double z, double *doppler){
  /*
   * 200 m/s as the doppler b-parameter. This
   * can be a function of (x,y,z) as well.
   * Note that *doppler is a pointer, not an array.
   * Remember the * in front of doppler.
   */
  *doppler = 200.;
}

/******************************************************************************/

void
velocity(double x, double y, double z, double *vel){
  double r, rToUse, ffSpeed;
  const double rMin = 0.1*AU; /* This cutoff should be chosen smaller than par->minScale but greater than zero (to avoid a singularity at the origin). */

  /*
   * Calculate radial distance from origin
   */
  r = sqrt(x*x+y*y+z*z);
  if(r>rMin)
    rToUse = r;
  else
    rToUse = rMin;

  /*
   * Free-fall velocity in the radial direction onto a central 
   * mass of 1.0 solar mass
   */  
  ffSpeed = sqrt(2*GRAV*1.989e30/rToUse);

  vel[0] = -x*ffSpeed/rToUse;
  vel[1] = -y*ffSpeed/rToUse;
  vel[2] = -z*ffSpeed/rToUse;
}

/******************************************************************************/


