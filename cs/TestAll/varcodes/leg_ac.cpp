#include "stdafx.h"

// Define constants of model: GM and Mean Earth radius (currently EGM2008)
double _gm = 3986004.415e+8; // m**3/s**2
double _ae = 6378136.3;	     // m


double *_c, *_s, *_pn, *_qn;
int *_ip;
int _nmax;



/*
      Purpose: To read and load EGM 2008 gravity model (available at
	          "http://earth-info.nga.mil/GandG/wgs84/gravitymod/
	           egm2008/first_release.html" 
	           with no separation spaces between slashes); and to 
		       initialize the global values
      
       Input:
		  name  File name with the C(n,m) and S(n,m) polynomial coefficients
          nmax  desired order and degree of model (max = 2190)
      	
       Author: Helio Koiti Kuga Jan-2011 Fortran Version 1.0 Created
		       Valdemir Carrara Oct-2011 C version
	
       Refs.: 
	    Pavlis, N.K.; Holmes, S.A.; Kenyon, S.C.; Factor, J.K. "An Earth 
	      Gravitational Model to Degree2160: EGM2008," presented at 
	      the 2008 General Assembly of the European Geosciences Union, 
	      Vienna, Austria, April 13-18, 2008.
*/

void leg_initialize(char *name, int nmax)

{
	static int i, k, n, n_m, ic, nn, mm;
	static double cnm, snm, d1, d2;
	char ch_line[130];

	FILE *nfile;

	_nmax = nmax;

	_ip    = (int *)malloc((nmax+1)*sizeof(int));

	_ip[0] = 0;

    // prepare pointer to store one-dimensionally C(n,m) and S(n,m)
	for (n = 1; n <= nmax; n++) _ip[n] = _ip[n-1] + n;

    // maximum one-dimensional array size
	n_m  = _ip[nmax] + nmax + 1;

	// allocate
	_c   = (double *)malloc(n_m*sizeof(double));
	_s   = (double *)malloc(n_m*sizeof(double));

	// First harmonics zeroed
	_c[0] = 0.;
	_s[0] = 0.;
	_c[1] = 0.;
	_s[1] = 0.;
	_c[2] = 0.;
	_s[2] = 0.;

	// load file with geopotential model
	nfile = fopen(name, "r");

	// Loop for reading and loading harmonic coefficients
	for (i = 3; i < n_m; i++)
	{
		fgets (ch_line, 130, nfile);                 // record assumed fixed 130 chars
		for (k = 0; k < 130; k++) 
		{
			if (ch_line[k] == 'D') ch_line[k] = 'e'; // trick: C does not accept exponent D+XX
			if (ch_line[k] == 'd') ch_line[k] = 'e'; // replace by e+XX
		}
//		ic = fscanf_s (nfile, "%d %d %e %e %e %e \n", &nn, &mm, &cnm, &snm, &d1, &d2);
		ic = sscanf (ch_line, "%d %d %lf %lf %lf %lf \n", &nn, &mm, &cnm, &snm, &d1, &d2);
		_c[i] = cnm;
		_s[i] = snm;
	}

	_pn = (double *)malloc(nmax*sizeof(double));
	_qn = (double *)malloc(nmax*sizeof(double));

	return;
}

/*
	Purpose:
		To compute geopotential acceleration

	Inputs:
		nm
			Maximum polynomial order and degree loaded
		x
			ECEF cartesian coordinates [m]
	Outputs:
		acel
			geopotential (x, y, z) accelerations [m/s**2]

	Authors:
		H�lio Koiti Kuga		Fortran version
		Valdemir Carrara		May/2012	Ansi_C version

    Refs.: 
	
	    Kuga, H.K.; Carrara, V. "Fortran- and C-codes for higher order
	      and degree geopotential computation." 
	      www.dem.inpe.br/~hkk/software/high_geopot.html
	      Last access May 2012.
*/

void leg_forcol_ac (int nm, double x[], double acel[])
{
	// c(0:nmax,0:nm), s(0:nmax,0:nm)

	// locals
	int n, m;
	double r, q, t, u, tf, al, sl, cl, gmr;
	double pnm, dpnm, anm, bnm, fnm;
	double am, an, pnm1m, pnm2m, sm, cm, sml, cml;
	double qc, qs, xc, xs, xcf, xsf, xcr, xsr, vl, vf, vr;
//	double Omega;
	
	if (nm > _nmax) nm = _nmax;

	// auxiliary variables
	r     = sqrt(x[0]*x[0] + x[1]*x[1] + x[2]*x[2]);
	q     = _ae/r;
	t     = x[2]/r;		// sin (lat)
	u     = sqrt(1. - t*t);
	tf    = t/u;		// tan (lat)
	al    = atan2(x[1], x[0]);
	sl    = sin(al);	// sin (long)
	cl    = cos(al);	// cos (long)
	gmr   = _gm/r;
      
	// summation initialization
	// omega = 0.d0;
	vl    = 0.0;
	vf    = 0.0;
	vr    = 0.0;

	// store sectoral
	_pn[0] = 1.0;
	_pn[1] = 1.73205080756887730*u;	// sqrt(3) * cos (lat)
	_qn[0] = 1.0;
	_qn[1] = q;

	for (m = 2; m <= nm; m++)
	{
		am     = m;
		_pn[m]  = u*sqrt(1.0 + 0.50/am)*_pn[m-1];
		_qn[m]  = q*_qn[m-1];
	}

	// initialize sin and cos recursions
	sm    = 0.0;
	cm    = 1.0;

	// outer n loop'
	for (m = 0; m <= nm; m++)
	{
		// init 
		am     = m;
		pnm    = _pn[m];			// m=n sectoral
		dpnm   = -am*pnm*tf;
		pnm1m  = pnm;
		pnm2m  = 0.0;

		// init  horner's scheme
		qc     = _qn[m]*_c[_ip[m] + m];
		qs     = _qn[m]*_s[_ip[m] + m];
		xc     = qc*pnm;
		xs     = qs*pnm;
		xcf    = qc*dpnm;
		xsf    = qs*dpnm;
		xcr    = (am + 1.0)*qc*pnm;
		xsr    = (am + 1.0)*qs*pnm;

		// inner m loop 
		for (n = m + 1; n <= nm; n++)
		{
			an      = n;
			anm     = sqrt(((an + an - 1.0)*(an + an + 1.0))
					/((an - am)*(an + am)));
			bnm     = sqrt(((an + an + 1.0)*(an + am - 1.0)*
					(an - am - 1.0))/((an - am)*(an + am)*(an + an - 3.0)));
			fnm     = sqrt(((an*an - am*am)*(an + an + 1.0))/(an + an - 1.0));
			// recursion p and dp
			pnm     = anm*t*pnm1m - bnm*pnm2m;
			dpnm    = -an*pnm*tf + fnm*pnm1m/u;		// signal opposite to paper
			// store
			pnm2m   = pnm1m;
			pnm1m   = pnm;

			// inner sum
			if (n >= 2)
			{
				qc     = _qn[n]*_c[_ip[n] + m];
				qs     = _qn[n]*_s[_ip[n] + m];
				xc     = (xc + qc*pnm);
				xs     = (xs + qs*pnm);
				xcf    = (xcf + qc*dpnm);
				xsf    = (xsf + qs*dpnm);
				xcr    = (xcr + (an + 1.0)*qc*pnm);
				xsr    = (xsr + (an + 1.0)*qs*pnm);
			}
		}

		// outer sum
		//	omega = omega + (xc*cm + xs*sm);
		vl   = vl + am*(xc*sm - xs*cm);
		vf   = vf + (xcf*cm + xsf*sm);
		vr   = vr + (xcr*cm + xsr*sm);

		// sin and cos recursions to next m
		cml  = cl*cm - sm*sl;
		sml  = cl*sm + cm*sl;
		cm   = cml;			// save to next m
		sm   = sml;			// save to next m
	}

	// finalization, include n=0 (p00=1), 
	// for n=1 all terms are zero: c,s(1,1), c,s(1,0) = 0

	// potential
	// omega =  gmr * (1.+omega);

	// gradient 
	vl    = -gmr*vl;
	vf    =  gmr*vf;
	vr    = -(gmr/r)*(1.0 + vr);
	
	// body x, y, z accelerations 
	acel[0] = u*cl*vr - t*cl*vf/r - sl*vl/(u*r);
	acel[1] = u*sl*vr - t*sl*vf/r + cl*vl/(u*r);
	acel[2] = t*vr + u*vf/r;
	
    return;
}

