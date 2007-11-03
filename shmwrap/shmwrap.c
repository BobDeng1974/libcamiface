#include <stdio.h>
#include <sys/time.h>
#include <stdlib.h>
#include <time.h>

#include <stdlib.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#include <netdb.h>

#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <fcntl.h>

#include "cam_iface.h"
#include "shmwrap.h"
#include "_shmwrap_state.h"

#undef max
#define max(x,y) ((x) > (y) ? (x) : (y))

double shm_floattime() {
  struct timeval t;
  if (gettimeofday(&t, (struct timezone *)NULL) == 0)
    return (double)t.tv_sec + t.tv_usec*0.000001;
  else
    return 0.0;
}

#define _check_error() {						\
    int _check_error_err;						\
    _check_error_err = cam_iface_have_error();				\
    if (_check_error_err != 0) {					\
      									\
      fprintf(stderr,"%s:%d %s\n", __FILE__,__LINE__,cam_iface_get_error_string()); \
      exit(1);								\
    }									\
  }									\

void malloc_info_buffer( CamContext *cc, char**info_buffer, int* buflen, int w, int h ) {
  int x;
  //  const char* properties_string="shutter: {'has_auto_mode':1,'max_value':24,'min_value':0,'is_present':0,'has_manual_mode': 1, 'is_scaled_quantity': 0}\r\n";


  x = 1000;
  *info_buffer=(char*)malloc(x);
  if (*info_buffer==NULL) {
    *buflen = 0;
    return;
  }
  
  char properties_string[1234];
  char str1[1234];
  char str2[1234];
  char* write_str, *read_str, *tmp_str;

  read_str = &(str2[0]);
  snprintf(read_str,1234,"[general]\r\ncameras: cam1\r\nudp_packet_size: %d\r\n[cam1]\r\nwidth: %d\r\nheight: %d\r\n",sizeof(shmwrap_msg_ready_t),w,h);

  int num_properties;
  CamContext_get_num_camera_properties(cc,&num_properties);
  _check_error();
  int i;

  CameraPropertyInfo info;
  long value;
  int autoprop;

  printf("whole info buffer (pre)\n");
  printf(read_str);

  write_str = &(str1[0]);

  for (i=0;i<num_properties;i++) {

    CamContext_get_camera_property_info(cc,i,&info);
    _check_error();
    CamContext_get_camera_property(cc,i,&value,&autoprop);
    _check_error();

    // XXX no scaled quantity support yet
    snprintf(&(properties_string[0]),1234,
	     "%s: %d, %d, {'name':'%s', 'is_present':%d, 'min_value':%d, 'max_value':%d, 'has_auto_mode':%d, 'has_manual_mode':%d, 'is_scaled_quantity':0}\r\n",
	     info.name, value, autoprop, info.name, info.is_present, info.min_value, info.max_value, info.has_auto_mode, info.has_manual_mode);

    //"shutter: {'has_auto_mode':1,'max_value':24,'min_value':0,'is_present':0,'has_manual_mode': 1, 'is_scaled_quantity': 0}\r\n";
    printf("\n\nproperty %d:\n",i);
    printf(properties_string);
    *buflen = snprintf(write_str,1234,"%s%s",read_str,properties_string);

    // swap buffers
    tmp_str = read_str;
    read_str = write_str;
    write_str = tmp_str;

    printf("whole info buffer\n");
    printf(read_str);
    printf("--------\n");
  
  }

  *buflen = snprintf(*info_buffer,x,"%s",read_str);

  printf("whole info buffer (final)\n");
  printf(*info_buffer);
  printf("--------\n");
  
}

int main() {
  CamContext *cc;

  int num_buffers;
  int num_shm_buffers;

  double last_fps_print, now, t_diff;
  double fps;
  int n_frames;
  uint8_t sendnumber;
  uint64_t framenumber;
  int buffer_size;
  int num_modes, num_props;
  const char** mode_strings;
  char mode_string[255];
  int i,mode_number;
  CameraPropertyInfo cam_props;
  long prop_value;
  int prop_auto;
  int errnum;
  int width, height;
  int max_width, max_height;
  size_t shm_chunk_size;
  key_t key;
  int shmid;
  char *data; // shared memory pointer
  unsigned char *pixels;
  shmwrap_msg_ready_t curmsg;
  FILE *fd;
  size_t offset, stride;

  // from http://www.cs.rpi.edu/courses/sysprog/sockets/sock.html
  int sock_udp_fast, n;
  socklen_t length;
  struct sockaddr_in udp_target_addr;
  struct hostent *hp;
  shmwrap_state_t *mystate;
  shmwrap_command_t incoming_command;
  char* info_buffer=NULL;
  int buflen;

  double last_timestamp;

  sock_udp_fast= socket(AF_INET, SOCK_DGRAM, 0);
  if (sock_udp_fast < 0) SHM_FATAL_PERROR(__FILE__,__LINE__);

  mystate = create_state();
  if (mystate==NULL)
    SHM_FATAL_PERROR(__FILE__,__LINE__);

  udp_target_addr.sin_family = AF_INET;
  hp = gethostbyname("127.0.0.1");
  if (hp==0) 
    SHM_FATAL_PERROR(__FILE__,__LINE__);

  bcopy((char *)hp->h_addr, 
        (char *)&udp_target_addr.sin_addr,
	hp->h_length);
  udp_target_addr.sin_port = htons(shmwrap_frame_ready_port);
  length=sizeof(struct sockaddr_in);

  // Create path if it doesn't exist
  fd = fopen(shmwrap_ftok_path,"a");
  if (!fd) SHM_FATAL_PERROR(__FILE__,__LINE__);

  if (fclose(fd)) SHM_FATAL_PERROR(__FILE__,__LINE__);

  // Get key
  if ((key = ftok(shmwrap_ftok_path, shmwrap_shm_name)) == -1) 
    SHM_FATAL_PERROR(__FILE__,__LINE__);


  cam_iface_startup_with_version_check();
  _check_error();

  printf("using driver %s\n",cam_iface_get_driver_name());

  if (cam_iface_get_num_cameras()<1) {
    _check_error();

    printf("no cameras found, will now exit\n");

    cam_iface_shutdown();
    _check_error();

    exit(1);
  }
  _check_error();

  cam_iface_get_num_modes(0, &num_modes);
  _check_error();

  printf("%d mode(s) available\n",num_modes);
  
  for (i=0; i<num_modes; i++) {
    cam_iface_get_mode_string(0,i,mode_string,255);
    printf("%d: %s\n",i,mode_string);
  }

  mode_number = 0;
  printf("\nChoosing mode %d\n",mode_number);

  num_buffers = 50;

  cc = new_CamContext(0,num_buffers,mode_number);
  _check_error();

  CamContext_get_frame_size(cc, &width, &height);
  _check_error();

  CamContext_get_max_frame_size(cc, &max_width, &max_height);
  _check_error();

  CamContext_get_num_framebuffers(cc,&num_buffers);
  printf("allocated %d buffers\n",num_buffers);

  shm_chunk_size = max_width*max_height;

  num_shm_buffers = shm_size/shm_chunk_size;
  printf("shm_size %d\n",shm_size);

  shmid = shmget(key, shm_size, 0644 | IPC_CREAT);
  if (shmid==-1)
    SHM_FATAL_PERROR(__FILE__,__LINE__);

  data = shmat(shmid, (void *)0, 0);
  if (data == (char *)(-1)) 
    SHM_FATAL_PERROR(__FILE__,__LINE__);

  CamContext_get_num_camera_properties(cc,&num_props);
  _check_error();


  for (i=0; i<num_props; i++) {
    CamContext_get_camera_property_info(cc,i,&cam_props);
    _check_error();

    if (strcmp(cam_props.name,"white balance")==0) {
      fprintf(stderr,"WARNING: ignoring white balance property\n");
      continue;
    }

    if (cam_props.is_present) {
      CamContext_get_camera_property(cc,i,&prop_value,&prop_auto);
      _check_error();
      printf("  %s: %d\n",cam_props.name,prop_value);
    } else {
      printf("  %s: not present\n");
    }
  }

  CamContext_get_buffer_size(cc,&buffer_size);
  _check_error();

  if (buffer_size == 0) {
    fprintf(stderr,"buffer size was 0 in %s, line %d\n",__FILE__,__LINE__);
    exit(1);
  }

  CamContext_start_camera(cc);
  _check_error();

  last_fps_print = shm_floattime();
  n_frames = 0;
  framenumber = 0;

  printf("will now run forever...\n");

  while (1) {

    handle_network(mystate,&incoming_command);
    switch (incoming_command.type) {
    case SHMWRAP_CMD_NOCOMMAND:
      // no command
      break;
    case SHMWRAP_CMD_HELLO:
      printf("got hello command\n");
      break;
    case SHMWRAP_CMD_REQUEST_INFO:
      printf("got info request command\n");
      malloc_info_buffer( cc, &info_buffer, &buflen, max_width, max_height );
      send_buf(mystate, info_buffer, buflen);
      free((void*)info_buffer);
      info_buffer=NULL;
      break;
    default:
      printf("got unknown command\n");
      break;
    }
    offset = shm_chunk_size*(sendnumber % num_shm_buffers);
    pixels = data + offset;
    //CamContext_grab_next_frame_blocking(cc,pixels,0.02f); // block for 20 msec
    CamContext_grab_next_frame_blocking(cc,pixels,-1.0f); // block forever
    errnum = cam_iface_have_error();
    if (errnum == CAM_IFACE_FRAME_DATA_MISSING_ERROR) {
      cam_iface_clear_error();
      fprintf(stdout,"M");
      fflush(stdout);
      framenumber++;
    } else if (errnum == CAM_IFACE_FRAME_TIMEOUT) {
      cam_iface_clear_error();
      fprintf(stdout,"T");
      fflush(stdout);
    } else {
      _check_error();
      fprintf(stdout,".");
      fflush(stdout);
      framenumber++;
    }

    if ((errnum == CAM_IFACE_FRAME_DATA_MISSING_ERROR) | (errnum == CAM_IFACE_FRAME_TIMEOUT))
      continue;

    //printf(" %02x %02x %02x %02x %02x\n",pixels[0],pixels[1],pixels[2],pixels[3],pixels[4]);

    now = shm_floattime();
    n_frames += 1;

    CamContext_get_last_timestamp(cc,&last_timestamp);
    _check_error();

    curmsg.sendnumber = sendnumber++;
    curmsg.framenumber = framenumber;
    curmsg.width = width;
    curmsg.height = height;
    curmsg.roi_x = 0;
    curmsg.roi_y = 0;
    curmsg.timestamp = last_timestamp;
    curmsg.start_offset = offset;
    curmsg.stride = width;
    
    sendnumber++;

    n=sendto(sock_udp_fast,&curmsg,
	     sizeof(curmsg),0,(struct sockaddr *)&udp_target_addr,length);
    if (n < 0) 
      SHM_FATAL_PERROR(__FILE__,__LINE__);
    if (n!=sizeof(curmsg)) {
      printf("n!=sizeof(curmsg)\n");
      exit(2);
    }

    t_diff = now-last_fps_print;
    if (t_diff > 5.0) {
      fps = n_frames/t_diff;
      fprintf(stdout,"approx. %.1f fps\n",fps);
      last_fps_print = now;
      n_frames = 0;
    }
  }
  printf("\n");
  delete_CamContext(cc);
  _check_error();

  cam_iface_shutdown();
  _check_error();

  return 0;
}
