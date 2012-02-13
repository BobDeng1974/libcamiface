/*

Copyright (c) 2004-2009, John Stowers. All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.

    * Redistributions in binary form must reproduce the above
      copyright notice, this list of conditions and the following
      disclaimer in the documentation and/or other materials provided
      with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

*/

/* Backend for libaravis-0.2 */
#include "cam_iface.h"

#define ARAVIS_INCLUDE_FAKE_CAMERA 0
#define ARAVIS_DEBUG_ENABLE 1

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/select.h>
#include <errno.h>

#include <arv.h>
#include <glib.h>
#include <glib/gprintf.h>

#if ARAVIS_INCLUDE_FAKE_CAMERA
#define GET_ARAVIS_DEVICE_INDEX(i) (i)
#else
#define GET_ARAVIS_DEVICE_INDEX(i) (i+1)
#endif

#if !ARAVIS_DEBUG_ENABLE
#define DPRINTF(...)
#else
#define DPRINTF(...) printf("DEBUG:    " __VA_ARGS__); fflush(stdout);
#endif

#define DWARNF(...) fprintf(stderr, "WARN :    " __VA_ARGS__); fflush(stderr);

struct CCaravis; // forward declaration

// keep functable in sync across backends
typedef struct {
  cam_iface_constructor_func_t construct;
  void (*destruct)(struct CamContext*);

  void (*CCaravis)(struct CCaravis*,int,int,int);
  void (*close)(struct CCaravis*);
  void (*start_camera)(struct CCaravis*);
  void (*stop_camera)(struct CCaravis*);
  void (*get_num_camera_properties)(struct CCaravis*,int*);
  void (*get_camera_property_info)(struct CCaravis*,
                                   int,
                                   CameraPropertyInfo*);
  void (*get_camera_property)(struct CCaravis*,int,long*,int*);
  void (*set_camera_property)(struct CCaravis*,int,long,int);
  void (*grab_next_frame_blocking)(struct CCaravis*,
                                   unsigned char*,
                                   float);
  void (*grab_next_frame_blocking_with_stride)(struct CCaravis*,
                                               unsigned char*,
                                               intptr_t,
                                               float);
  void (*point_next_frame_blocking)(struct CCaravis*,unsigned char**,float);
  void (*unpoint_frame)(struct CCaravis*);
  void (*get_last_timestamp)(struct CCaravis*,double*);
  void (*get_last_framenumber)(struct CCaravis*,unsigned long*);
  void (*get_num_trigger_modes)(struct CCaravis*,int*);
  void (*get_trigger_mode_string)(struct CCaravis*,int,char*,int);
  void (*get_trigger_mode_number)(struct CCaravis*,int*);
  void (*set_trigger_mode_number)(struct CCaravis*,int);
  void (*get_frame_roi)(struct CCaravis*,int*,int*,int*,int*);
  void (*set_frame_roi)(struct CCaravis*,int,int,int,int);
  void (*get_max_frame_size)(struct CCaravis*,int*,int*);
  void (*get_buffer_size)(struct CCaravis*,int*);
  void (*get_framerate)(struct CCaravis*,float*);
  void (*set_framerate)(struct CCaravis*,float);
  void (*get_num_framebuffers)(struct CCaravis*,int*);
  void (*set_num_framebuffers)(struct CCaravis*,int);
} CCaravis_functable;

typedef struct CCaravis {
  CamContext inherited;

  int cam_iface_mode_number;

  int max_width;       // maximum buffer width
  int max_height;      // maximum buffer height

  int roi_left;
  int roi_top;
  int roi_width;
  int roi_height;
  int buffer_size;     // bytes per frame
  unsigned long nframe_hack;

  ArvCamera *camera;
  ArvStream *stream;
  int num_buffers;

  char **trigger_modes;
  int num_trigger_modes;

} CCaravis;

// forward declarations
CCaravis* CCaravis_construct( int device_number, int NumImageBuffers,
                              int mode_number);
void delete_CCaravis(struct CCaravis*);

void CCaravis_CCaravis(struct CCaravis*,int,int,int);
void CCaravis_close(struct CCaravis*);
void CCaravis_start_camera(struct CCaravis*);
void CCaravis_stop_camera(struct CCaravis*);
void CCaravis_get_num_camera_properties(struct CCaravis*,int*);
void CCaravis_get_camera_property_info(struct CCaravis*,
                              int,
                              CameraPropertyInfo*);
void CCaravis_get_camera_property(struct CCaravis*,int,long*,int*);
void CCaravis_set_camera_property(struct CCaravis*,int,long,int);
void CCaravis_grab_next_frame_blocking(struct CCaravis*,
                              unsigned char*,
                              float);
void CCaravis_grab_next_frame_blocking_with_stride(struct CCaravis*,
                                          unsigned char*,
                                          intptr_t,
                                          float);
void CCaravis_point_next_frame_blocking(struct CCaravis*,unsigned char**,float);
void CCaravis_unpoint_frame(struct CCaravis*);
void CCaravis_get_last_timestamp(struct CCaravis*,double*);
void CCaravis_get_last_framenumber(struct CCaravis*,unsigned long*);
void CCaravis_get_num_trigger_modes(struct CCaravis*,int*);
void CCaravis_get_trigger_mode_string(struct CCaravis*,int,char*,int);
void CCaravis_get_trigger_mode_number(struct CCaravis*,int*);
void CCaravis_set_trigger_mode_number(struct CCaravis*,int);
void CCaravis_get_frame_roi(struct CCaravis*,int*,int*,int*,int*);
void CCaravis_set_frame_roi(struct CCaravis*,int,int,int,int);
void CCaravis_get_max_frame_size(struct CCaravis*,int*,int*);
void CCaravis_get_buffer_size(struct CCaravis*,int*);
void CCaravis_get_framerate(struct CCaravis*,float*);
void CCaravis_set_framerate(struct CCaravis*,float);
void CCaravis_get_num_framebuffers(struct CCaravis*,int*);
void CCaravis_set_num_framebuffers(struct CCaravis*,int);

CCaravis_functable CCaravis_vmt = {
  (cam_iface_constructor_func_t)CCaravis_construct,
  (void (*)(CamContext*))delete_CCaravis,
  CCaravis_CCaravis,
  CCaravis_close,
  CCaravis_start_camera,
  CCaravis_stop_camera,
  CCaravis_get_num_camera_properties,
  CCaravis_get_camera_property_info,
  CCaravis_get_camera_property,
  CCaravis_set_camera_property,
  CCaravis_grab_next_frame_blocking,
  CCaravis_grab_next_frame_blocking_with_stride,
  CCaravis_point_next_frame_blocking,
  CCaravis_unpoint_frame,
  CCaravis_get_last_timestamp,
  CCaravis_get_last_framenumber,
  CCaravis_get_num_trigger_modes,
  CCaravis_get_trigger_mode_string,
  CCaravis_get_trigger_mode_number,
  CCaravis_set_trigger_mode_number,
  CCaravis_get_frame_roi,
  CCaravis_set_frame_roi,
  CCaravis_get_max_frame_size,
  CCaravis_get_buffer_size,
  CCaravis_get_framerate,
  CCaravis_set_framerate,
  CCaravis_get_num_framebuffers,
  CCaravis_set_num_framebuffers
};

// See the following for a hint on how to make thread thread-local without __thread.
// http://lists.apple.com/archives/Xcode-users/2006/Jun/msg00551.html
#ifdef __APPLE__
#define myTLS
#else
#define myTLS __thread
#endif

#ifdef MEGA_BACKEND
  #define BACKEND_GLOBAL(m) aravis_##m
#else
  #define BACKEND_GLOBAL(m) m
#endif

/* globals -- allocate space */
myTLS int BACKEND_GLOBAL(cam_iface_error) = 0;
#define CAM_IFACE_MAX_ERROR_LEN 255
myTLS char BACKEND_GLOBAL(cam_iface_error_string)[CAM_IFACE_MAX_ERROR_LEN]  = {0x00}; //...

uint32_t aravis_num_cameras = 0;
ArvCamera **aravis_cameras = NULL;
char **aravis_device_names = NULL;
guint32 *aravis_num_camera_formats = NULL;
gint64 **aravis_camera_formats = NULL;

/* one aravis thread and one mainloop per process, not per camera. I don't know
how much of libcamiface supports threading anyway, so im not sure of the gain in
making each camera threaded, or indeed if aravis already does this... */
GThread *aravis_thread = NULL;
GMainContext *aravis_context = NULL;
GMainLoop *aravis_mainloop = NULL;

#ifdef MEGA_BACKEND
#define CAM_IFACE_ERROR_FORMAT(m)                                       \
  snprintf(aravis_cam_iface_error_string,CAM_IFACE_MAX_ERROR_LEN,              \
           "%s (%d): %s\n",__FILE__,__LINE__,(m));
#else
#define CAM_IFACE_ERROR_FORMAT(m)                                       \
  snprintf(cam_iface_error_string,CAM_IFACE_MAX_ERROR_LEN,              \
           "%s (%d): %s\n",__FILE__,__LINE__,(m));
#endif

#ifdef MEGA_BACKEND
#define NOT_IMPLEMENTED                                 \
  aravis_cam_iface_error = -1;                          \
  fprintf(stderr,"WARN :    %s (%d): not yet implemented\n",__FILE__,__LINE__); fflush(stderr); \
  CAM_IFACE_ERROR_FORMAT("not yet implemented");        \
  return;
#else
#define NOT_IMPLEMENTED                                 \
  cam_iface_error = -1;                                 \
  fprintf(stderr,"WARN :    %s (%d): not yet implemented\n",__FILE__,__LINE__); fflush(stderr); \
  CAM_IFACE_ERROR_FORMAT("not yet implemented");        \
  return;
#endif

#define NOT_IMPLEMENTED_WARN fprintf(stderr,"WARN :    %s (%d): not yet implemented\n",__FILE__,__LINE__); fflush(stderr);

#include "cam_iface_aravis.h"

const char *BACKEND_METHOD(cam_iface_get_driver_name)() {
  return "aravis";
}

void BACKEND_METHOD(cam_iface_clear_error)() {
  BACKEND_GLOBAL(cam_iface_error) = 0;
}

int BACKEND_METHOD(cam_iface_have_error)() {
  return BACKEND_GLOBAL(cam_iface_error);
}

const char * BACKEND_METHOD(cam_iface_get_error_string)() {
  return BACKEND_GLOBAL(cam_iface_error_string);
}

const char* BACKEND_METHOD(cam_iface_get_api_version)() {
  return CAM_IFACE_API_VERSION;
}

static gpointer aravis_thread_func(gpointer data) {
  GMainContext *context = data;

  DPRINTF("startup thread\n");

  g_main_context_push_thread_default (context);
  aravis_mainloop = g_main_loop_new (context, FALSE);
  g_main_loop_run (aravis_mainloop);

  DPRINTF("stop thread\n");

  return 0;
}


void BACKEND_METHOD(cam_iface_startup)() {
  unsigned int i;

  DPRINTF("startup\n");

#if !GLIB_CHECK_VERSION (2, 31, 0)
    g_thread_init (NULL);
#endif

  g_type_init ();

  /* this creates an association between list index and device IDs. This association
  will not change until the next call to this function, so I consider the list
  index to be canonical */
  arv_update_device_list ();

  aravis_num_cameras = arv_get_n_devices ();
  aravis_cameras = calloc(aravis_num_cameras, sizeof(ArvCamera *));
  aravis_camera_formats = calloc(aravis_num_cameras, sizeof(gint64 *));
  aravis_num_camera_formats = calloc(aravis_num_cameras, sizeof(guint32 *));
  aravis_device_names = calloc(aravis_num_cameras, sizeof(const char *));


  if (aravis_cameras == NULL || aravis_device_names == NULL) {
    BACKEND_GLOBAL(cam_iface_error) = -1;
    CAM_IFACE_ERROR_FORMAT("error allocating memory");
    return;
  }

  for (i = 0; i < aravis_num_cameras; i++) {
    aravis_device_names[i] = g_strdup( arv_get_device_id (i) );
  }

  /* start the threading and mainloop */
  aravis_context = g_main_context_new ();
  aravis_thread = g_thread_new("aravis", aravis_thread_func, aravis_context);

}

void BACKEND_METHOD(cam_iface_shutdown)() {
  g_main_loop_quit (aravis_mainloop);
  arv_shutdown ();
}

int BACKEND_METHOD(cam_iface_get_num_cameras)() {
#if ARAVIS_INCLUDE_FAKE_CAMERA
  return aravis_num_cameras;
#else
  return aravis_num_cameras - 1;
#endif
}

static ArvCamera * _lazy_init_camera(int device_number) {
  int device_index = GET_ARAVIS_DEVICE_INDEX(device_number);

  if (aravis_cameras[device_index])
    return aravis_cameras[device_index];

  DPRINTF("laxy init device_number:%u aravis_id:%u aravis_name:%s\n", 
          device_number, GET_ARAVIS_DEVICE_INDEX(device_number),
          aravis_device_names[device_index]);

  aravis_cameras[device_index] = arv_camera_new ( aravis_device_names[device_index] );
  if (!aravis_cameras[device_index]) {
    BACKEND_GLOBAL(cam_iface_error) = CAM_IFACE_CAMERA_NOT_AVAILABLE_ERROR;
    CAM_IFACE_ERROR_FORMAT("camera not available");
    return NULL;
  }

  aravis_camera_formats[device_index] = arv_camera_get_available_pixel_formats (
                                          aravis_cameras[device_index],
                                          &(aravis_num_camera_formats[device_index]));

  return aravis_cameras[device_index];
}

void BACKEND_METHOD(cam_iface_get_camera_info)(int device_number, Camwire_id *out_camid) {
  ArvCamera *camera;

  if (out_camid==NULL) {
    BACKEND_GLOBAL(cam_iface_error) = -1;
    CAM_IFACE_ERROR_FORMAT("return structure NULL");
    return;
  }
  
  if ((camera = _lazy_init_camera(device_number)) == NULL)
    return;

  snprintf(out_camid->vendor, CAMWIRE_ID_MAX_CHARS, "%s", arv_camera_get_vendor_name(camera));
  snprintf(out_camid->model, CAMWIRE_ID_MAX_CHARS, "%s", arv_camera_get_model_name(camera));
  snprintf(out_camid->chip, CAMWIRE_ID_MAX_CHARS, "%s", arv_camera_get_device_id(camera));

}

void BACKEND_METHOD(cam_iface_get_num_modes)(int device_number, int *num_modes) {
  ArvCamera *camera;
  guint n_formats;
  gint64 *formats;
  int device_index = GET_ARAVIS_DEVICE_INDEX(device_number);

  DPRINTF("GET NUM MODES\n");

  if ((camera = _lazy_init_camera(device_number)) == NULL) {
    *num_modes = 0;
    return;
  }

  *num_modes = aravis_num_camera_formats[device_index];

}

#define CASE_FORMAT_TO_FORMAT7(_c,_s,_q,_d) case _c:  \
  *ret = "DC1394_VIDEO_MODE_FORMAT7_0 " _s;     \
  *coding = _q;                                 \
  *depth = _d;                                  \
  break;
#define CASE_FORMAT_MEH(_c) case _c:            \
  *ret = #_c;                                   \
  *coding = CAM_IFACE_UNKNOWN;                  \
  *depth = -1;                                  \
  break;

static void _aravis_format_to_camiface(ArvPixelFormat format,
                                       const char **ret,
                                       CameraPixelCoding *coding,
                                       int *depth) {
  //const char *ret;

  /* AIUI we can always set binning, ROI, etc. This makes us FORMAT7_0 */

  switch (format) {
    CASE_FORMAT_TO_FORMAT7(ARV_PIXEL_FORMAT_MONO_8, "MONO8", CAM_IFACE_MONO8, 8);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_MONO_8_SIGNED);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_MONO_10);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_MONO_10_PACKED);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_MONO_12);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_MONO_12_PACKED);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_MONO_14);
    CASE_FORMAT_TO_FORMAT7(ARV_PIXEL_FORMAT_MONO_16, "MONO16", CAM_IFACE_MONO16, 16);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_BAYER_GR_8);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_BAYER_RG_8);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_BAYER_GB_8);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_BAYER_BG_8);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_BAYER_GR_10);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_BAYER_RG_10);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_BAYER_GB_10);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_BAYER_BG_10);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_BAYER_GR_12);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_BAYER_RG_12);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_BAYER_GB_12);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_BAYER_BG_12);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_BAYER_BG_12_PACKED);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_RGB_8_PACKED);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_BGR_8_PACKED);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_RGBA_8_PACKED);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_BGRA_8_PACKED);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_RGB_10_PACKED);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_BGR_10_PACKED);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_RGB_12_PACKED);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_BGR_12_PACKED);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_YUV_411_PACKED);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_YUV_422_PACKED);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_YUV_444_PACKED);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_RGB_8_PLANAR);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_RGB_10_PLANAR);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_RGB_12_PLANAR);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_RGB_16_PLANAR);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_YUV_422_YUYV_PACKED);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_CUSTOM_BAYER_GR_12_PACKED);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_CUSTOM_BAYER_RG_12_PACKED);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_CUSTOM_BAYER_GB_12_PACKED);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_CUSTOM_BAYER_BG_12_PACKED);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_CUSTOM_YUV_422_YUYV_PACKED);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_CUSTOM_BAYER_GR_16);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_CUSTOM_BAYER_RG_16);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_CUSTOM_BAYER_GB_16);
    CASE_FORMAT_MEH(ARV_PIXEL_FORMAT_CUSTOM_BAYER_BG_16);
    default:
      *ret = "unknown color coding";
  }
}

void BACKEND_METHOD(cam_iface_get_mode_string)(int device_number,
                               int mode_number,
                               char* mode_string,
                               int mode_string_maxlen) {

  ArvCamera *camera;
  guint n_formats;
  gint64 *formats;
  gint minw,maxw,minh,maxh;
  const char *format7_mode_string;
  CameraPixelCoding coding;
  int depth;
  const char *framerate_string = "(user selectable framerate)";
  int device_index = GET_ARAVIS_DEVICE_INDEX(device_number);

  if ((camera = _lazy_init_camera(device_number)) == NULL) {
    *mode_string = '\0';
    return;
  }

  arv_camera_get_width_bounds (camera, &minw, &maxw);
  arv_camera_get_height_bounds (camera, &minh, &maxh);

  _aravis_format_to_camiface (
    aravis_camera_formats[device_index][mode_number],
    &format7_mode_string,
    &coding,
    &depth);

  snprintf(mode_string,mode_string_maxlen,
           "%d x %d %s %s",
           maxw, maxh, format7_mode_string, framerate_string);

}

cam_iface_constructor_func_t BACKEND_METHOD(cam_iface_get_constructor_func)(int device_number) {
  return (CamContext* (*)(int, int, int))CCaravis_construct;
}

CCaravis* CCaravis_construct( int device_number, int NumImageBuffers,
                              int mode_number) {
  CCaravis* this=NULL;

  this = malloc(sizeof(CCaravis));
  if (this==NULL) {
    BACKEND_GLOBAL(cam_iface_error) = -1;
    CAM_IFACE_ERROR_FORMAT("error allocating memory");
  } else {
    CCaravis_CCaravis( this,
                       device_number, NumImageBuffers,
                       mode_number);
    if (BACKEND_GLOBAL(cam_iface_error)) {
      free(this);
      return NULL;
    }
  }
  return this;
}

void delete_CCaravis( CCaravis *this ) {
  CCaravis_close(this);
  this->inherited.vmt = NULL;
  free(this);
  this = NULL;
}

void CCaravis_CCaravis( CCaravis *this,
                        int device_number, int NumImageBuffers,
                        int mode_number) {
  ArvDevice *device;
  ArvGcNode *node;
  CameraPixelCoding coding;
  int depth;
  const char *format7_mode_string;
  int device_index = GET_ARAVIS_DEVICE_INDEX(device_number);

  DPRINTF("CONSTRUCT: device: %d mode: %d\n", device_number, mode_number);

  /* call parent */
  CamContext_CamContext((CamContext*)this,device_number,NumImageBuffers,mode_number);
  this->inherited.vmt = (CamContext_functable*)&CCaravis_vmt;

  /* initialize */
  this->inherited.cam = (void *)NULL;
  this->inherited.backend_extras = (void *)NULL;
  if (!this) {
    BACKEND_GLOBAL(cam_iface_error) = -1;
    CAM_IFACE_ERROR_FORMAT("malloc failed");
    return;
  }

  this->inherited.device_number = device_number;

  this->cam_iface_mode_number = mode_number;
  this->nframe_hack=0;

  /* FIXME: take a ref here too */
  this->camera = _lazy_init_camera(device_number);

  this->num_buffers = 50;

  /* Fill out camera specific data. If this was non-const then I would cache
  it globally, but it isn't, so I store it here */
  device = arv_camera_get_device (this->camera);
  node = arv_device_get_feature (device, "TriggerSource"); 

  if (node && ARV_IS_GC_ENUMERATION (node)) {
    const GSList *childs;
    const GSList *iter;
    int i;

    this->num_trigger_modes = arv_gc_node_get_n_childs (node);
    this->trigger_modes = calloc(this->num_trigger_modes, sizeof(const char *));

    childs = arv_gc_node_get_childs (node);
    for (iter = childs, i = 0; iter != NULL; iter = iter->next, i++) {
      this->trigger_modes[i] = arv_gc_node_get_name (iter->data);
    }
  } else {
    this->num_trigger_modes = 0;
    this->trigger_modes = NULL;
  }

  _aravis_format_to_camiface (
    aravis_camera_formats[device_index][mode_number],
    &format7_mode_string,
    &coding,
    &depth);

  this->inherited.depth = depth;
  this->inherited.coding= coding;

}

void CCaravis_close(CCaravis *this) {
  arv_camera_stop_acquisition (this->camera);
}



void CCaravis_start_camera( CCaravis *this ) {
  int i;
  unsigned int payload;

  this->stream = arv_camera_create_stream (this->camera, NULL, NULL);

  payload = arv_camera_get_payload(this->camera);
  for (i = 0; i < this->num_buffers; i++)
    arv_stream_push_buffer (this->stream, arv_buffer_new (payload, NULL));

  arv_camera_set_acquisition_mode (this->camera, ARV_ACQUISITION_MODE_CONTINUOUS);
  arv_camera_start_acquisition (this->camera);

}

void CCaravis_stop_camera( CCaravis *this ) {
  arv_camera_start_acquisition (this->camera);
  DWARNF("stop camera\n");
}

void CCaravis_get_num_camera_properties(CCaravis *this,
                                        int* num_properties) {
  *num_properties = 0;
  DWARNF("get num properties\n");
}

void CCaravis_get_camera_property_info(CCaravis *this,
                                       int property_number,
                                       CameraPropertyInfo *info) {
  NOT_IMPLEMENTED;
}

void CCaravis_get_camera_property(CCaravis *this,
                                  int property_number,
                                  long* Value,
                                  int* Auto ) {
  NOT_IMPLEMENTED;
}

void CCaravis_set_camera_property(CCaravis *this,
                                  int property_number,
                                  long Value,
                                  int Auto ) {
  NOT_IMPLEMENTED;
}

void CCaravis_grab_next_frame_blocking_with_stride( CCaravis *this,
                                                    unsigned char *out_bytes,
                                                    intptr_t stride0, float timeout) {
  NOT_IMPLEMENTED;
}

void CCaravis_grab_next_frame_blocking( CCaravis *this, unsigned char *out_bytes, float timeout) {
  int ok = 0;
  ArvBuffer *buffer = arv_stream_pop_buffer(this->stream);

  if (buffer) {
    if (buffer->status == ARV_BUFFER_STATUS_SUCCESS) {
      memcpy((void*)out_bytes /*dest*/, buffer->data, buffer->size);
    }
    arv_stream_push_buffer (this->stream, buffer);
    ok = 1;
  }

  if (!ok) {
#ifdef MEGA_BACKEND
    aravis_cam_iface_error = CAM_IFACE_FRAME_DATA_MISSING_ERROR;
#else
    cam_iface_error = CAM_IFACE_FRAME_DATA_MISSING_ERROR;
#endif
    *out_bytes = '\0';
  }

}

void CCaravis_point_next_frame_blocking( CCaravis *this, unsigned char **buf_ptr, float timeout) {
  NOT_IMPLEMENTED;
}

void CCaravis_unpoint_frame( CCaravis *this){
  NOT_IMPLEMENTED;
}

void CCaravis_get_last_timestamp( CCaravis *this, double* timestamp ) {
  NOT_IMPLEMENTED;
}

void CCaravis_get_last_framenumber( CCaravis *this, unsigned long* framenumber ){
  NOT_IMPLEMENTED;
}

void CCaravis_get_num_trigger_modes( CCaravis *this,
                                     int *num_trigger_modes ) {
  *num_trigger_modes = this->num_trigger_modes;
}

void CCaravis_get_trigger_mode_string( CCaravis *this,
                                       int trigger_mode_number,
                                       char* trigger_mode_string, //output parameter
                                       int trigger_mode_string_maxlen) {
  snprintf(trigger_mode_string,trigger_mode_string_maxlen,this->trigger_modes[trigger_mode_number]);
}

void CCaravis_get_trigger_mode_number( CCaravis *this,
                                       int *trigger_mode_number ) {
  NOT_IMPLEMENTED;
}

void CCaravis_set_trigger_mode_number( CCaravis *this,
                                       int trigger_mode_number ) {
  NOT_IMPLEMENTED;
}

void CCaravis_get_frame_roi( CCaravis *this,
                             int *left, int *top, int* width, int* height ) {
  arv_camera_get_region (this->camera, left, top, width, height);
}

void CCaravis_set_frame_roi( CCaravis *this,
                             int left, int top, int width, int height ) {
  NOT_IMPLEMENTED;
}

void CCaravis_get_framerate( CCaravis *this,
                             float *framerate ) {
  NOT_IMPLEMENTED;
}

void CCaravis_set_framerate( CCaravis *this,
                             float framerate ) {
  NOT_IMPLEMENTED;
}

void CCaravis_get_max_frame_size( CCaravis *this,
                                  int *width, int *height ){
  NOT_IMPLEMENTED;
}

void CCaravis_get_buffer_size( CCaravis *this,
                               int *size) {
  guint payload = arv_camera_get_payload(this->camera);
  *size = payload;
}

void CCaravis_get_num_framebuffers( CCaravis *this,
                                    int *num_framebuffers ) {
  *num_framebuffers = this->num_buffers;
}

void CCaravis_set_num_framebuffers( CCaravis *this,
                                    int num_framebuffers ) {
  NOT_IMPLEMENTED;
}