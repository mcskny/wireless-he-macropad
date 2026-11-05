#include "lighting.h"
#include <math.h>
#include <string.h>

#define GOLDEN 0.61803398875
static float clampf(float x, float a, float b) { return x<a?a:x>b?b:x; }
static double frac(double x) { return x-floor(x); }
static double duration(const light_profile_t *p) { return fmax(0.3, 3.0-(double)p->speed*0.025); }
static void hsv(double h, float sat, float v, float out[3]) {
    double q=frac(h)*6.0; int sector=(int)q; float f=(float)(q-sector);
    float a=v*(1-sat), b=v*(1-sat*f), c=v*(1-sat*(1-f));
    switch(sector) {
        case 0:out[0]=v;out[1]=c;out[2]=a;break;
        case 1:out[0]=b;out[1]=v;out[2]=a;break;
        case 2:out[0]=a;out[1]=v;out[2]=c;break;
        case 3:out[0]=a;out[1]=b;out[2]=v;break;
        case 4:out[0]=c;out[1]=a;out[2]=v;break;
        default:out[0]=v;out[1]=a;out[2]=b;break;
    }
}
bool light_profile_valid(const light_profile_t *p) {
    return p && p->effect<EFFECT_COUNT && p->brightness<=100 && p->speed>=1 && p->speed<=100 && !(p->flags & ~(FLAG_COLORFUL|FLAG_DEPTH|FLAG_SWEEP_TURN)) && (!(p->flags & FLAG_DEPTH) || p->effect==EFFECT_REACTIVE || p->effect==EFFECT_RIPPLE) && (!(p->flags & FLAG_SWEEP_TURN) || p->effect==EFFECT_SWEEP);
}
void light_engine_init(light_engine_t *s, const light_profile_t *p) { memset(s,0,sizeof(*s)); if(light_profile_valid(p)) s->profile=*p; }
bool light_engine_set_profile(light_engine_t *s, const light_profile_t *p) {
    if(!s || !light_profile_valid(p)) return false;
    s->profile=*p;
    /* Remove stale events while retaining current physical depth and pressed state. */
    memset(s->waves,0,sizeof(s->waves)); memset(s->hit_valid,0,sizeof(s->hit_valid));
    return true;
}
void light_engine_input(light_engine_t *s, uint8_t key, float value, uint64_t now_ms) {
    if(!s || key>=LIGHT_KEYS || !isfinite(value)) return;
    float depth=clampf(value,0,1); s->depth[key]=depth;
    bool pressed=s->pressed[key] ? depth>0.05f : depth>=0.10f;
    if(pressed && !s->pressed[key]) {
        s->hit_count++; float hue=(float)frac(s->hit_count*GOLDEN);
        s->hue[key]=hue; s->hit_ms[key]=now_ms; s->hit_valid[key]=true;
        if(s->profile.effect==EFFECT_RIPPLE) {
            light_wave_t *w=&s->waves[s->next_wave++ % LIGHT_WAVES];
            *w=(light_wave_t){ .start_ms=now_ms, .duration=duration(&s->profile), .hue=hue, .peak=depth, .key=key, .held=true, .active=true };
        }
    }
    s->pressed[key]=pressed;
    for(size_t i=0;i<LIGHT_WAVES;i++) {
        light_wave_t *w=&s->waves[i];
        if(w->active && w->held && w->key==key) {
            if(depth<=0.05f) w->held=false; else if(depth>w->peak) w->peak=depth;
        }
    }
}
void light_engine_virtual_hit(light_engine_t *s,uint8_t key,uint32_t id,uint64_t now_ms){
    if(!s||key>=LIGHT_KEYS||!id)return;
    float hue=(float)frac(id*GOLDEN);s->hit_count=id;s->hue[key]=hue;s->hit_ms[key]=now_ms;s->hit_valid[key]=true;
    if(s->profile.effect==EFFECT_RIPPLE){light_wave_t *w=&s->waves[s->next_wave++ % LIGHT_WAVES];
        *w=(light_wave_t){.start_ms=now_ms,.duration=duration(&s->profile),.hue=hue,.peak=1,.key=key,.held=false,.active=true};}
}
void light_engine_sync_physical_hit(light_engine_t *s,uint8_t key,uint32_t id,uint64_t now_ms,bool awaiting_local_press){
    if(!s||key>=LIGHT_KEYS||!id)return;
    if(awaiting_local_press){s->hit_count=id-1;return;}
    s->hit_count=id;float hue=(float)frac(id*GOLDEN);
    if(s->hit_valid[key] && now_ms>=s->hit_ms[key] && now_ms-s->hit_ms[key]<=300){
        s->hue[key]=hue;light_wave_t *latest=NULL;
        for(size_t i=0;i<LIGHT_WAVES;i++){light_wave_t *w=&s->waves[i];
            if(w->active && w->key==key && now_ms>=w->start_ms && now_ms-w->start_ms<=300 && (!latest||w->start_ms>latest->start_ms))latest=w;}
        if(latest)latest->hue=hue;
    }
}
static float wave_weight(const light_wave_t *w, uint8_t key, uint64_t now_ms, bool depth) {
    if(!w->active || now_ms<w->start_ms) return 0;
    float dx=(float)((int)(key/4)-(int)(w->key/4)), dy=(float)((int)(key%4)-(int)(w->key%4));
    float elapsed=(float)(now_ms-w->start_ms)/1000.0f-sqrtf(dx*dx+dy*dy)*w->duration*0.16f;
    if(elapsed<0) return 0;
    return (depth?w->peak:1.0f)*fmaxf(0,1-elapsed/(w->duration*0.45f));
}
void light_engine_render(light_engine_t *s, uint64_t now_ms, light_rgb_t out[LIGHT_KEYS]) {
    if(!s || !out) return;
    const light_profile_t *p=&s->profile;
    double d=duration(p),t=(double)now_ms/1000.0; bool colorful=(p->flags&FLAG_COLORFUL)!=0, depth=(p->flags&FLAG_DEPTH)!=0;
    for(size_t w=0;w<LIGHT_WAVES;w++) if(s->waves[w].active && now_ms>=s->waves[w].start_ms && (double)(now_ms-s->waves[w].start_ms)>s->waves[w].duration*1100.0) s->waves[w].active=false;
    for(uint8_t i=0;i<LIGHT_KEYS;i++) {
        float rgb[3]={p->r/255.0f,p->g/255.0f,p->b/255.0f}, intensity=1;
        double delay=p->effect==EFFECT_FLOW ? i*d/18.0 : 0, phase=frac((t-delay)/d);
        switch(p->effect) {
            case EFFECT_OFF: intensity=0; break;
            case EFFECT_STATIC: break;
            case EFFECT_BREATHE: case EFFECT_FLOW:
                intensity=(float)fabs(2*phase-1);
                if(colorful) hsv(floor((t-delay)/d+0.5)*GOLDEN,0.8f,1,rgb);
                break;
            case EFFECT_RAINBOW: hsv(t/(d*6)+i/12.0,0.75f,1,rgb); break;
            case EFFECT_SWEEP: {
                double a=frac(t/d-i/12.0); intensity=fmaxf(0.0f,1-(float)fabs(a-0.18)/0.18f);
                if(colorful && (p->flags&FLAG_SWEEP_TURN)){
                    const double period=11.0/12.0+0.36;double x=frac(t/d/period)*period;
                    intensity=fmaxf(0,1-(float)fabs(x-0.18-i/12.0)/0.18f);
                    hsv(floor(t/d/period)*GOLDEN,0.8f,1,rgb);
                }else if(colorful)hsv(t/(d*6)+i/12.0,0.8f,1,rgb);
                break;
            }
            case EFFECT_REACTIVE:
                if(depth) intensity=s->depth[i];
                else if(s->hit_valid[i] && now_ms>=s->hit_ms[i]) intensity=fmaxf(0,1-(float)(now_ms-s->hit_ms[i])/(d*450.0f));
                else intensity=0;
                if(colorful) hsv(s->hue[i],0.8f,1,rgb);
                break;
            case EFFECT_RIPPLE: {
                float total=0, mixed[3]={0};
                for(size_t w=0;w<LIGHT_WAVES;w++) {
                    float weight=wave_weight(&s->waves[w],i,now_ms,depth);
                    if(weight<=0) continue;
                    total+=weight;
                    if(colorful) {float c[3];hsv(s->waves[w].hue,0.8f,1,c);for(int n=0;n<3;n++) mixed[n]+=c[n]*weight;}
                }
                intensity=fminf(1,total);
                if(colorful && total>0) for(int n=0;n<3;n++) rgb[n]=mixed[n]/total;
                break;
            }
            case EFFECT_SNAKE: {
                static const uint8_t order[12]={0,1,2,3,7,6,5,4,8,9,10,11};unsigned pos=0;while(pos<12 && order[pos]!=i)pos++;
                double progress=frac(t/(d*15.0/4.0))*15.0;
                intensity=fmaxf(0,1-(float)fabs(progress-2.0-pos)/2.0f);
                if(colorful)hsv(floor(t/(d*15.0/4.0))*GOLDEN,0.75f,1,rgb);
                break;
            }
            case EFFECT_STARLIGHT: {
                static const uint8_t groups[2][6]={{0,2,5,7,8,10},{1,3,4,6,9,11}};
                uint64_t cycle=(uint64_t)floor(t/d),hash=cycle+UINT64_C(0x9E3779B97F4A7C15);
                hash=(hash^(hash>>30))*UINT64_C(0xBF58476D1CE4E5B9);hash=(hash^(hash>>27))*UINT64_C(0x94D049BB133111EB);hash^=hash>>31;
                unsigned n=(unsigned)(hash%15),a=0,b=1;
                for(unsigned x=0;x<5;x++){unsigned count=5-x;if(n<count){a=x;b=x+1+n;break;}n-=count;}
                int slot=-1;unsigned rank=0;for(unsigned j=0;j<6;j++){if(j==a||j==b)continue;if(groups[cycle%2][j]==i)slot=(int)rank;rank++;}
                intensity=0;if(slot>=0){double p=(frac(t/d)-slot*0.08)/0.72;if(p>0&&p<1)intensity=(float)((1-cos(6.283185307179586*p))/2);hsv(cycle*GOLDEN+slot*0.25,0.75f,1,rgb);}
                break;
            }
            default: intensity=0; break;
        }
        float scale=intensity*p->brightness/100.0f;
        out[i]=(light_rgb_t){(uint8_t)lroundf(clampf(rgb[0]*scale,0,1)*255),(uint8_t)lroundf(clampf(rgb[1]*scale,0,1)*255),(uint8_t)lroundf(clampf(rgb[2]*scale,0,1)*255)};
    }
}
