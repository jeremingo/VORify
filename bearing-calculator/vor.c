#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

#include <signal.h>
#include <getopt.h>
#include <math.h>
#include <complex.h>
#include "vorify.h"

extern int interval;

typedef struct {
 complex double xv[8], yv[8];
} filterstate_t;

VORStation vorStations[] = {
    {"BEN GURION", 113.50, 32.0131, 34.8752, 100.0},
    {"BEER-SHEBA", 114.30, 31.2862, 34.7213, 853.0},
    {"METZADA", 115, 31.3308, 35.39, -1200.0},
    {"NATANIA", 112.40, 32.3323, 34.9677, 100.0},
    {"OVDA", 114.10, 29.9656, 34.9469, 1513.0},
    {"ROSH PINA", 115.30, 32.9822, 35.5733, 897.0},
    {"ZOFAR", 115.60, 30.5573, 35.1608, 100.0}
};

int numStations = sizeof(vorStations) / sizeof(vorStations[0]);

static complex double filter510(complex double V , filterstate_t *st) {
	st->xv[0] = st->xv[1]; st->xv[1] = st->xv[2]; st->xv[2] = st->xv[3]; st->xv[3] = st->xv[4]; 
        st->xv[4] = V ;
        st->yv[0] = st->yv[1]; st->yv[1] = st->yv[2]; st->yv[2] = st->yv[3]; st->yv[3] = st->yv[4]; 
        st->yv[4] =   (st->xv[0] + st->xv[4]) +   1.3724127962 * (st->xv[1] + st->xv[3]) +   0.7448255925 * st->xv[2]
                     + ( -0.9133512299 * st->yv[2]) + (  1.9094231878 * st->yv[3]);
        return(st->yv[4]);
}

static complex double filterlow(complex double V, filterstate_t *st) {

	st->xv[0] = st->xv[1]; st->xv[1] = st->xv[2]; st->xv[2] = st->xv[3]; st->xv[3] = st->xv[4]; 
        st->xv[4] = V ;
        st->yv[0] = st->yv[1]; st->yv[1] = st->yv[2]; st->yv[2] = st->yv[3]; st->yv[3] = st->yv[4]; 
        st->yv[4] =   (st->xv[0] + st->xv[4]) +   0.0000142122 * st->xv[1] +   0.0000142122 * st->xv[3]
                     -   1.9999715756 * st->xv[2]
                     + ( -0.9972352026 * st->yv[2]) + (  1.9972326511 * st->yv[3]);
        return(st->yv[4]);
}

void vor(float S)
{
	static filterstate_t flt_r;
	static filterstate_t flt_s;
	static filterstate_t flt_f;
	static double phase=0,sum=0,pA,uw=0;
	static complex double fpr;
	static int n=-FSINT/10;

	double A,fp,F;
	complex double ref30,fmcar,sig30;

	const double W30=2.0*M_PI*30/FSINT;

	phase+=W30;
	if(phase>M_PI) phase-=2.0*M_PI;

	ref30=cexp(phase*-I)*S;	
	ref30=filterlow(ref30,&flt_r);

	fmcar=filter510(cexp(9960/30*phase*-I)*S,&flt_f);
	F=carg(fmcar*conj(fpr));
	fpr=fmcar;
	if(F>2.0*M_PI*510/FSINT) F=2.0*M_PI*510/FSINT;
	if(F<-2.0*M_PI*510/FSINT) F=-2.0*M_PI*510/FSINT;

	sig30=cexp(phase*-I)*F;
	sig30=filterlow(sig30,&flt_s);

	A=carg(sig30*conj(ref30))+26*2.0*M_PI*30/FSINT;
	if(n>0) {
		if((A-pA)>M_PI) uw-=2.0*M_PI;
		if((A-pA)<-M_PI) uw+=2.0*M_PI;
		sum+=A+uw;
	}
	pA=A;

	n++;
	if(n>interval*FSINT) {
		double avg=fmod(180.0/M_PI*sum/n,360.0);
		if(avg<0) avg+=360;
		printf("%5.1f\n",avg);
    fflush(stdout);
		n=0;sum=0;
	}
}	

