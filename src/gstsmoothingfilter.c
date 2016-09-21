/*
 * Smoothing Filter GStreamer Plugin
 * Copyright (C) 2015-2016 Gray Cancer Institute
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

/**
 * SECTION:element-smoothing
 *
 * This element smooths the image by applying some kind of low-pass spatial filter such as a Gaussian convolution kernel.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch-1.0 videotestsrc smoothingfilter ! videoconvert ! xvimagesink
 *
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gst/gst.h>
#include <gst/video/video.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

#include "gstsmoothingfilter.h"

GST_DEBUG_CATEGORY_STATIC (gst_smoothingfilter_debug);
#define GST_CAT_DEFAULT gst_smoothingfilter_debug

/* Filter signals and args */
enum
{
	/* FILL ME */
	LAST_SIGNAL
};

enum
{
	PROP_0,
	PROP_KERNELSIZE,
	PROP_SIGMA
};

#define DEFAULT_PROP_KERNELSIZE 1       // The size index (n) of the kernel, kernel will be square 2n+1 in size
#define DEFAULT_PROP_SIGMA      1.5     // The sigma used for Gaussian kernel, e^(-r^2/sigma^2) where r is distance from central pixel

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
		GST_PAD_SINK,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
				("{ BGR, RGB }"))
);

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
		GST_PAD_SRC,
		GST_PAD_ALWAYS,
		GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE
				("{ BGR, RGB }"))
);

#define gst_smoothingfilter_parent_class parent_class
G_DEFINE_TYPE (Gstsmoothingfilter, gst_smoothingfilter, GST_TYPE_ELEMENT);

static void gst_smoothingfilter_set_property (GObject * object, guint prop_id,
		const GValue * value, GParamSpec * pspec);
static void gst_smoothingfilter_get_property (GObject * object, guint prop_id,
		GValue * value, GParamSpec * pspec);

static gboolean gst_smoothingfilter_sink_event (GstPad * pad, GstObject * parent, GstEvent * event);
static GstFlowReturn gst_smoothingfilter_chain (GstPad * pad, GstObject * parent, GstBuffer * buf);


void
create_gamma_lut(Gstsmoothingfilter *filter)
{
	unsigned int i;
	double invgamma = 1.0/GAMMA;

	filter->forward_gamma = g_new(double, IN_RANGE);
	filter->inverse_gamma = g_new(unsigned int, OUT_RANGE);

//	GST_DEBUG_OBJECT (filter, "create_gamma_lut NOW !!!!!!!!!");

	for (i=0;i<IN_RANGE;i++){
		filter->forward_gamma[i] = (double)((double)OUT_RANGE * pow(((double)i/(double)FACTOR) + OFFSET, (double)GAMMA));
//		filter->forward_gamma[i] = (double)((double)OUT_RANGE * pow(((double)i/(double)IN_RANGE), (double)GAMMA));
//		GST_DEBUG_OBJECT (filter, "forward_gamma: %d - %.2f", i, filter->forward_gamma[i]);
	}

    // NB Not applying the output offset, OFFSET, here since not adding a linear portion to the gamma curve (see flycapsrc LUT))!!! TODO: Is this OK?
	for (i=0;i<OUT_RANGE;i++){
		filter->inverse_gamma[i] = (unsigned int)(IN_RANGE * pow(((double)i/OUT_RANGE), invgamma));
//		GST_DEBUG_OBJECT (filter, "inverse_gamma: %d - %d", i, filter->inverse_gamma[i]);
	}
}

/* GObject vmethod implementations */

/* initialize the smoothingfilter's class */
static void
gst_smoothingfilter_class_init (GstsmoothingfilterClass * klass)
{
	GObjectClass *gobject_class;
	GstElementClass *gstelement_class;

	gobject_class = (GObjectClass *) klass;
	gstelement_class = (GstElementClass *) klass;

	gobject_class->set_property = gst_smoothingfilter_set_property;
	gobject_class->get_property = gst_smoothingfilter_get_property;

	// properties
	g_object_class_install_property (gobject_class, PROP_KERNELSIZE,
			g_param_spec_int("kernelsize", "Kernel Size", "The size index (n) of the kernel, kernel will be square 2n+1 in size.", 0, 2, DEFAULT_PROP_KERNELSIZE,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));
	g_object_class_install_property (gobject_class, PROP_SIGMA,
			g_param_spec_float("sigma", "Gaussian Sigma", "The sigma used for Gaussian kernel, e^(r^2/sigma^2) where r is distance from central pixel.", 0.1, 100.0, DEFAULT_PROP_SIGMA,
					G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | GST_PARAM_MUTABLE_PLAYING));

	gst_element_class_set_details_simple(gstelement_class,
			"smoothingfilter",
			"Filter",
			"Smoothes the image by applying some kind of low-pass spatial filter such as a Gaussian convolution kernel.",
			"Paul R Barber <<paul.barber@oncology.ox.ac.uk>>");

	gst_element_class_add_pad_template (gstelement_class,
			gst_static_pad_template_get (&src_factory));
	gst_element_class_add_pad_template (gstelement_class,
			gst_static_pad_template_get (&sink_factory));
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_smoothingfilter_init (Gstsmoothingfilter * filter)
{
	filter->sinkpad = gst_pad_new_from_static_template (&sink_factory, "sink");
	gst_pad_set_event_function (filter->sinkpad,
			GST_DEBUG_FUNCPTR(gst_smoothingfilter_sink_event));
	gst_pad_set_chain_function (filter->sinkpad,
			GST_DEBUG_FUNCPTR(gst_smoothingfilter_chain));
	GST_PAD_SET_PROXY_CAPS (filter->sinkpad);
	gst_element_add_pad (GST_ELEMENT (filter), filter->sinkpad);

	filter->srcpad = gst_pad_new_from_static_template (&src_factory, "src");
	GST_PAD_SET_PROXY_CAPS (filter->srcpad);
	gst_element_add_pad (GST_ELEMENT (filter), filter->srcpad);

	filter->kernelsize = DEFAULT_PROP_KERNELSIZE;
	filter->sigma = DEFAULT_PROP_SIGMA;

	filter->smoothing_buffer = NULL;

	filter->valchanged = 1;

	create_gamma_lut(filter);
}

static void
gst_smoothingfilter_set_property (GObject * object, guint prop_id,
		const GValue * value, GParamSpec * pspec)
{
	Gstsmoothingfilter *filter = GST_SMOOTHINGFILTER (object);
	float val;

	switch (prop_id) {
	case PROP_KERNELSIZE:
		val = g_value_get_int(value);
		if(filter->kernelsize != val){
			filter->kernelsize = val;
			filter->valchanged=1;
			GST_DEBUG_OBJECT(filter, "valchanged");
		}
		break;
	case PROP_SIGMA:
		val = g_value_get_float(value);
		if(filter->sigma != val){
			filter->sigma = val;
			filter->valchanged=1;
		}
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
gst_smoothingfilter_get_property (GObject * object, guint prop_id,
		GValue * value, GParamSpec * pspec)
{
	Gstsmoothingfilter *filter = GST_SMOOTHINGFILTER (object);

	switch (prop_id) {
	case PROP_KERNELSIZE:
		g_value_set_int(value, filter->kernelsize);
		break;
	case PROP_SIGMA:
		g_value_set_float(value, filter->sigma);
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

/* GstElement vmethod implementations */

/* this function handles sink events */
static gboolean
gst_smoothingfilter_sink_event (GstPad * pad, GstObject * parent, GstEvent * event)
{
	gboolean ret;
	Gstsmoothingfilter *filter;
//	gchar *format;

	filter = GST_SMOOTHINGFILTER (parent);

	switch (GST_EVENT_TYPE (event)) {
	case GST_EVENT_CAPS:
	{
		GstCaps *caps;
		GstStructure *structure;

		gst_event_parse_caps (event, &caps);
		/* do something with the caps */

		if(gst_caps_is_fixed (caps)){

			structure = gst_caps_get_structure (caps, 0);
			if (!gst_structure_get_int (structure, "width", &(filter->width)) ||
					!gst_structure_get_int (structure, "height", &(filter->height))) {
				GST_ERROR_OBJECT (filter, "No width/height available\n");
			}

			filter->stride = filter->width * 3;  // TODO Not sure how to get this properly, but we have BGR or RGB data

//			Currently it does not matter if the data is RGB or BGR, but this would tells us which
//			format = gst_structure_get_string (structure, "format");
//			if (!format) {
//				GST_ERROR_OBJECT (filter, "No format available\n");
//			}
//			if (strcmp(format, "RGB")==0){
//				filter->format_is_RGB = TRUE;
//				GST_DEBUG_OBJECT (filter, "Format is RGB");
//			}

			GST_DEBUG_OBJECT (filter, "The video size of this set of capabilities is %dx%d, %d\n",
					filter->width, filter->height, filter->stride);
		}
		else {
			GST_ERROR_OBJECT (filter, "Caps not fixed.\n");
		}

		/* and forward */
		ret = gst_pad_event_default (pad, parent, event);
		break;
	}
	case GST_EVENT_EOS:

		/* and forward */
		ret = gst_pad_event_default (pad, parent, event);
		break;
	default:
		ret = gst_pad_event_default (pad, parent, event);
		break;
	}
	return ret;
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_smoothingfilter_chain (GstPad * pad, GstObject * parent, GstBuffer * buf)
{
	Gstsmoothingfilter *filter;
	GstMapInfo minfo;

	filter = GST_SMOOTHINGFILTER (parent);

	double *forward_gamma = filter->forward_gamma;
	unsigned int *inverse_gamma = filter->inverse_gamma;

	unsigned int out_limit = OUT_RANGE - 1;
	unsigned int in_limit = IN_RANGE - 1;

	if (filter->kernelsize==0){
//		GST_DEBUG_OBJECT (filter, "No smoothing to do, push source buffer.");
	}
	else{

		// Access the buffer - READ AND WRITE
		gst_buffer_map (buf, &minfo, GST_MAP_READWRITE);

		bgr_pixel *ptr=NULL;
		gint x, y, i, j;
		gint s = 2*filter->kernelsize+1;
		gint start_y = 0;
		gint stop_y  = filter->height;
		gint start_x = 0;
		gint stop_x  = filter->width;

		guint8 *img_ptr = minfo.data;
		gint pitch = filter->stride / 3;  // want the number of pixels to next line

//		GST_DEBUG_OBJECT(filter, "filter->valchanged = %d", filter->valchanged);
		// If a parameter has changed, recalculate and store the kernel
		if (filter->valchanged){
			GST_DEBUG_OBJECT(filter, "malloc kernel");

			g_free(filter->smoothing_buffer);   // no need to check for NULL
			filter->smoothing_buffer = (float *)g_malloc(s*s*sizeof(float));

			if(!filter->smoothing_buffer){
				GST_ERROR_OBJECT(filter, "malloc kernel failed.");
				return gst_pad_push (filter->srcpad, buf);
			}

			GST_DEBUG_OBJECT(filter, "Smoothing kernel calculations: kernelsize %d sigma %f", filter->kernelsize, filter->sigma);

			// calculate kernel values according to 2d Gaussian curve
			double sum=0;
			for(i=0; i<s; i++){
				for(j=0; j<s; j++){
					gint ii = i-filter->kernelsize;
					gint jj = j-filter->kernelsize;
					double f = exp(-(ii*ii+jj*jj)/(filter->sigma*filter->sigma));
					filter->smoothing_buffer[i*s+j] = f;
					sum += f;
				}
			}
			// We do not want the brightness to change so normalise the kernel to sum to 1
			for(i=0; i<s; i++){
				for(j=0; j<s; j++){
					filter->smoothing_buffer[i*s+j] /= sum;
					GST_DEBUG_OBJECT(filter, "Smoothing kernel %d %d: %f", i, j, filter->smoothing_buffer[i*s+j]);
				}
			}
			filter->valchanged=0;
		}

		// Perform the convolution on the image, in-place so always apply kernel down and to the right
//		GST_DEBUG_OBJECT(filter, "Perform the convolution");
		if (filter->kernelsize==1){  // fast 3x3 implementation
			bgr_pixel *ptr2=NULL, *ptr3=NULL, *ptr4=NULL;
			gint valr, valg, valb;
			stop_y  = filter->height-s;
			stop_x  = filter->width-s;
			for(y=start_y; y<stop_y; y++){
				ptr = (bgr_pixel *)img_ptr + pitch * y + start_x; // ptr to start of line
				for(x=start_x; x<stop_x; x++){
					valb = valg = valr = 0;

					ptr2 = ptr;
					ptr3 = ptr2+pitch;
					ptr4 = ptr3+pitch;
					valb += forward_gamma[CLAMP((ptr2)->b, 0, in_limit)] * filter->smoothing_buffer[0];
					valg += forward_gamma[CLAMP((ptr2)->g, 0, in_limit)] * filter->smoothing_buffer[0];
					valr += forward_gamma[CLAMP((ptr2)->r, 0, in_limit)] * filter->smoothing_buffer[0];

					valb += forward_gamma[CLAMP((ptr3)->b, 0, in_limit)] * filter->smoothing_buffer[3];
					valg += forward_gamma[CLAMP((ptr3)->g, 0, in_limit)] * filter->smoothing_buffer[3];
					valr += forward_gamma[CLAMP((ptr3)->r, 0, in_limit)] * filter->smoothing_buffer[3];

					valb += forward_gamma[CLAMP((ptr4)->b, 0, in_limit)] * filter->smoothing_buffer[6];
					valg += forward_gamma[CLAMP((ptr4)->g, 0, in_limit)] * filter->smoothing_buffer[6];
					valr += forward_gamma[CLAMP((ptr4)->r, 0, in_limit)] * filter->smoothing_buffer[6];

					ptr2++;
					ptr3 = ptr2+pitch;
					ptr4 = ptr3+pitch;
					valb += forward_gamma[CLAMP((ptr2)->b, 0, in_limit)] * filter->smoothing_buffer[1];
					valg += forward_gamma[CLAMP((ptr2)->g, 0, in_limit)] * filter->smoothing_buffer[1];
					valr += forward_gamma[CLAMP((ptr2)->r, 0, in_limit)] * filter->smoothing_buffer[1];

					valb += forward_gamma[CLAMP((ptr3)->b, 0, in_limit)] * filter->smoothing_buffer[4];
					valg += forward_gamma[CLAMP((ptr3)->g, 0, in_limit)] * filter->smoothing_buffer[4];
					valr += forward_gamma[CLAMP((ptr3)->r, 0, in_limit)] * filter->smoothing_buffer[4];

					valb += forward_gamma[CLAMP((ptr4)->b, 0, in_limit)] * filter->smoothing_buffer[7];
					valg += forward_gamma[CLAMP((ptr4)->g, 0, in_limit)] * filter->smoothing_buffer[7];
					valr += forward_gamma[CLAMP((ptr4)->r, 0, in_limit)] * filter->smoothing_buffer[7];

					ptr2++;
					ptr3 = ptr2+pitch;
					ptr4 = ptr3+pitch;
					valb += forward_gamma[CLAMP((ptr2)->b, 0, in_limit)] * filter->smoothing_buffer[2];
					valg += forward_gamma[CLAMP((ptr2)->g, 0, in_limit)] * filter->smoothing_buffer[2];
					valr += forward_gamma[CLAMP((ptr2)->r, 0, in_limit)] * filter->smoothing_buffer[2];

					valb += forward_gamma[CLAMP((ptr3)->b, 0, in_limit)] * filter->smoothing_buffer[5];
					valg += forward_gamma[CLAMP((ptr3)->g, 0, in_limit)] * filter->smoothing_buffer[5];
					valr += forward_gamma[CLAMP((ptr3)->r, 0, in_limit)] * filter->smoothing_buffer[5];

					valb += forward_gamma[CLAMP((ptr4)->b, 0, in_limit)] * filter->smoothing_buffer[8];
					valg += forward_gamma[CLAMP((ptr4)->g, 0, in_limit)] * filter->smoothing_buffer[8];
					valr += forward_gamma[CLAMP((ptr4)->r, 0, in_limit)] * filter->smoothing_buffer[8];

					ptr->b = inverse_gamma[(unsigned int)CLAMP(valb, 0, out_limit)];
					ptr->g = inverse_gamma[(unsigned int)CLAMP(valg, 0, out_limit)];
					ptr->r = inverse_gamma[(unsigned int)CLAMP(valr, 0, out_limit)];

					ptr++;  // next pixel, 3 bytes on
				}
			}
		}
		else {  // generic implementation
			gint valr, valg, valb;
			stop_y  = filter->height-s;
			stop_x  = filter->width-s;
			for(y=start_y; y<stop_y; y++){
				ptr = (bgr_pixel *)img_ptr + pitch * y + start_x; // ptr to start of line
				for(x=start_x; x<stop_x; x++){
					valb = valg = valr = 0;
					for(i=0; i<s; i++){
						for(j=0; j<s; j++){
							valb += forward_gamma[CLAMP((ptr+i*pitch+j)->b, 0, in_limit)] * filter->smoothing_buffer[i*s+j];
							valg += forward_gamma[CLAMP((ptr+i*pitch+j)->g, 0, in_limit)] * filter->smoothing_buffer[i*s+j];
							valr += forward_gamma[CLAMP((ptr+i*pitch+j)->r, 0, in_limit)] * filter->smoothing_buffer[i*s+j];
						}
					}

					ptr->b = inverse_gamma[(unsigned int)CLAMP(valb, 0, out_limit)];
					ptr->g = inverse_gamma[(unsigned int)CLAMP(valg, 0, out_limit)];
					ptr->r = inverse_gamma[(unsigned int)CLAMP(valr, 0, out_limit)];

					ptr++;  // next pixel, 3 bytes on
				}
			}
		}

		gst_buffer_unmap (buf, &minfo);
	}

	return gst_pad_push (filter->srcpad, buf);  // push out the buffer
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
static gboolean
smoothingfilter_init (GstPlugin * smoothingfilter)
{
	/* debug category for fltering log messages
	 *
	 * exchange the string 'Template smoothingfilter' with your description
	 */
	GST_DEBUG_CATEGORY_INIT (gst_smoothingfilter_debug, "smoothingfilter",
			1, "Template smoothingfilter");

	return gst_element_register (smoothingfilter, "smoothingfilter", GST_RANK_NONE,
			GST_TYPE_SMOOTHINGFILTER);
}

/* PACKAGE: this is usually set by autotools depending on some _INIT macro
 * in configure.ac and then written into and defined in config.h, but we can
 * just set it ourselves here in case someone doesn't use autotools to
 * compile this code. GST_PLUGIN_DEFINE needs PACKAGE to be defined.
 */
#ifndef PACKAGE
#define PACKAGE "smoothingfilter"
#endif

/* gstreamer looks for this structure to register smoothingfilters
 *
 * exchange the string 'Template smoothingfilter' with your smoothingfilter description
 */
GST_PLUGIN_DEFINE (
		GST_VERSION_MAJOR,
		GST_VERSION_MINOR,
		smoothingfilter,
		"Smoothes the image by applying some kind of low-pass spatial filter such as a Gaussian convolution kernel.",
		smoothingfilter_init,
		VERSION,
		"LGPL",
		PACKAGE_NAME,
		"http://users.ox.ac.uk/~atdgroup"
)
