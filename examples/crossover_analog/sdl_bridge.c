// SPDX-License-Identifier: MIT
// Experimental CrossOver-local SDL2 bridge. Does not patch CrossOver files.
#include <SDL.h>
#include <arpa/inet.h>
#include <dlfcn.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define REAL_SDL "/Applications/CrossOver.app/Contents/SharedSupport/CrossOver/lib64/libSDL2-2.0.0.dylib"
#define PORT 27861
static void *real_lib;
static pthread_once_t once=PTHREAD_ONCE_INIT;
static int (*real_init)(Uint32);
static int (*attach)(SDL_JoystickType,int,int,int);
static SDL_Joystick *(*open_joy)(int);
static int (*set_axis)(SDL_Joystick *,int,Sint16);
static void (*update_joy)(void);
static SDL_Joystick *joy;
static void load_real(void) {
    real_lib=dlopen(REAL_SDL,RTLD_NOW|RTLD_LOCAL);
    if (!real_lib) return;
    real_init=dlsym(real_lib,"SDL_Init");attach=dlsym(real_lib,"SDL_JoystickAttachVirtual");
    open_joy=dlsym(real_lib,"SDL_JoystickOpen");set_axis=dlsym(real_lib,"SDL_JoystickSetVirtualAxis");
    update_joy=dlsym(real_lib,"SDL_JoystickUpdate");
}
static int16_t get16(const uint8_t *p){return (int16_t)((uint16_t)p[0]|((uint16_t)p[1]<<8));}
static void axes(int16_t steering,int16_t throttle,int16_t brake) {
    set_axis(joy,0,steering);set_axis(joy,4,throttle);set_axis(joy,5,brake);update_joy();
}
static void *receive_loop(void *unused) {
    (void)unused;
    int fd=socket(AF_INET,SOCK_DGRAM,0);if(fd<0)return NULL;
    struct timeval tv={.tv_sec=0,.tv_usec=250000};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&tv,sizeof(tv));
    struct sockaddr_in addr={0};addr.sin_family=AF_INET;addr.sin_port=htons(PORT);addr.sin_addr.s_addr=htonl(INADDR_LOOPBACK);
    if(bind(fd,(struct sockaddr *)&addr,sizeof(addr))<0){close(fd);return NULL;}
    axes(0,INT16_MIN,INT16_MIN);
    for(;;) {
        uint8_t data[10];struct sockaddr_in peer;socklen_t peer_len=sizeof(peer);
        ssize_t n=recvfrom(fd,data,sizeof(data),0,(struct sockaddr *)&peer,&peer_len);
        if(n==10 && !memcmp(data,"WHE1",4)) {
            axes(get16(data+4),get16(data+6),get16(data+8));
            (void)sendto(fd,"WHEA",4,0,(struct sockaddr *)&peer,peer_len);
        } else axes(0,INT16_MIN,INT16_MIN);
    }
}
int SDL_Init(Uint32 flags) {
    pthread_once(&once,load_real);
    if(!real_init)return -1;
    int rc=real_init(flags);
    if(rc==0 && (flags & (SDL_INIT_JOYSTICK|SDL_INIT_GAMECONTROLLER)) && !joy && attach && open_joy && set_axis && update_joy) {
        int index=attach(SDL_JOYSTICK_TYPE_GAMECONTROLLER,6,15,1);
        if(index>=0) {
            joy=open_joy(index);
            if(joy){pthread_t t;if(!pthread_create(&t,NULL,receive_loop,NULL))pthread_detach(t);}
        }
    }
    return rc;
}
