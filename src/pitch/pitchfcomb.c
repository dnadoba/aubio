/*
   Copyright (C) 2004, 2005  Mario Lang <mlang@delysid.org>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include "aubio_priv.h"
#include "fvec.h"
#include "cvec.h"
#include "mathutils.h"
#include "spectral/fft.h"
#include "pitch/pitchfcomb.h"

#define MAX_PEAKS 8

typedef struct {
  smpl_t bin;
  smpl_t db;
} aubio_fpeak_t;

struct _aubio_pitchfcomb_t {
  uint_t fftSize;
  uint_t stepSize;
  uint_t rate;
  fvec_t * winput;
  fvec_t * win;
  cvec_t * fftOut;
  fvec_t * fftLastPhase;
  aubio_fft_t * fft;
};

aubio_pitchfcomb_t * new_aubio_pitchfcomb (uint_t bufsize, uint_t hopsize, uint_t channels)
{
  aubio_pitchfcomb_t * p = AUBIO_NEW(aubio_pitchfcomb_t);
  p->fftSize      = bufsize;
  p->stepSize     = hopsize;
  p->winput       = new_fvec(bufsize,1);
  p->fftOut       = new_cvec(bufsize,1);
  p->fftLastPhase = new_fvec(bufsize, channels);
  p->fft = new_aubio_fft(bufsize, 1);
  p->win = new_aubio_window(bufsize, aubio_win_hanning);
  return p;
}

/* input must be stepsize long */
void aubio_pitchfcomb_do (aubio_pitchfcomb_t * p, fvec_t * input, fvec_t * output)
{
  uint_t i, k, l, maxharm = 0;
  smpl_t phaseDifference = TWO_PI*(smpl_t)p->stepSize/(smpl_t)p->fftSize;
  aubio_fpeak_t peaks[MAX_PEAKS];

  for (i = 0; i < input->channels; i++) {

  for (k=0; k<MAX_PEAKS; k++) {
    peaks[k].db = -200.;
    peaks[k].bin = 0.;
  }

  for (k=0; k < input->length; k++){
    p->winput->data[0][k] = p->win->data[0][k] * input->data[i][k];
  }
  aubio_fft_do(p->fft,p->winput,p->fftOut);

  for (k=0; k<=p->fftSize/2; k++) {
    smpl_t
      magnitude = 20.*LOG10(2.*p->fftOut->norm[0][k]/(smpl_t)p->fftSize),
      phase     = p->fftOut->phas[0][k],
      tmp, bin;

    /* compute phase difference */
    tmp = phase - p->fftLastPhase->data[i][k];
    p->fftLastPhase->data[i][k] = phase;

    /* subtract expected phase difference */
    tmp -= (smpl_t)k*phaseDifference;

    /* map delta phase into +/- Pi interval */
    tmp = aubio_unwrap2pi(tmp);

    /* get deviation from bin frequency from the +/- Pi interval */
    tmp = p->fftSize/(smpl_t)p->stepSize*tmp/(TWO_PI);

    /* compute the k-th partials' true bin */
    bin = (smpl_t)k + tmp;

    if (bin > 0.0 && magnitude > peaks[0].db) { // && magnitude < 0) {
      memmove(peaks+1, peaks, sizeof(aubio_fpeak_t)*(MAX_PEAKS-1));
      peaks[0].bin = bin;
      peaks[0].db = magnitude;
    }
  }

  k = 0;
  for (l=1; l<MAX_PEAKS && peaks[l].bin > 0.0; l++) {
    sint_t harmonic;
    for (harmonic=5; harmonic>1; harmonic--) {
      if (peaks[0].bin / peaks[l].bin < harmonic+.02 &&
          peaks[0].bin / peaks[l].bin > harmonic-.02) {
        if (harmonic > (sint_t)maxharm &&
            peaks[0].db < peaks[l].db/2) {
          maxharm = harmonic;
          k = l;
        }
      }
    }
  }
  output->data[i][0] = peaks[k].bin;
  /* quick hack to clean output a bit */
  if (peaks[k].bin > 5000.) output->data[i][0] = 0.;
  }
}

void del_aubio_pitchfcomb (aubio_pitchfcomb_t * p)
{
  del_cvec(p->fftOut);
  del_fvec(p->fftLastPhase);
  del_fvec(p->win);
  del_fvec(p->winput);
  del_aubio_fft(p->fft);
  AUBIO_FREE(p);
}

