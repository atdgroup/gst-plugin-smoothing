/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) 2014 user <<user@hostname.org>>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __GST_SMOOTHINGFILTER_H__
#define __GST_SMOOTHINGFILTER_H__

#include <gst/gst.h>

G_BEGIN_DECLS

/* #defines don't like whitespacey bits */
#define GST_TYPE_SMOOTHINGFILTER \
  (gst_smoothingfilter_get_type())
#define GST_SMOOTHINGFILTER(obj) \
  (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_SMOOTHINGFILTER,Gstsmoothingfilter))
#define GST_SMOOTHINGFILTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_SMOOTHINGFILTER,GstsmoothingfilterClass))
#define GST_IS_SMOOTHINGFILTER(obj) \
  (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_SMOOTHINGFILTER))
#define GST_IS_SMOOTHINGFILTER_CLASS(klass) \
  (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_SMOOTHINGFILTER))

typedef struct _Gstsmoothingfilter      Gstsmoothingfilter;
typedef struct _GstsmoothingfilterClass GstsmoothingfilterClass;

// Calculate in linear intensity space, we expect the camera to have applied a 0.45 gamma
// So linearise with a 2.22 gamma, bin and then re-gamma with 0.45
// We create a gamma luts for speed, with integers
// To apply the 2.22 gamma to the int input value i, use v=gamma[i]
// To apply the 0.45 gamma to the int calculated value v, use i=inverse_gamma[v]
#define GAMMA 2.22
#define OFFSET 0.099   // from Rec. 709 standard
#define FACTOR 283.02  // Factor to divide input by so that it's never >1 when 0.099 is added
#define IN_RANGE 256
#define OUT_RANGE 4096     // an higher bit lut for reverse lookup, 18 bit (262144) guarantees every level preserved, 12 (4096) may be ok

typedef struct {
	guint8 b, g, r;
} bgr_pixel;

struct _Gstsmoothingfilter
{
  GstElement element;

  GstPad *sinkpad, *srcpad;

  gint kernelsize;
  gfloat sigma;

  float *smoothing_buffer;   // Memory to store the kernel
  gint width, height; // image size
  gint stride;    // bytes to next line
  gint valchanged; // flag for something has changed and the kernel should be recalculated

  double *forward_gamma;
  unsigned int *inverse_gamma;
};

struct _GstsmoothingfilterClass 
{
  GstElementClass parent_class;
};

GType gst_smoothingfilter_get_type (void);

G_END_DECLS

#endif /* __GST_SMOOTHINGFILTER_H__ */
