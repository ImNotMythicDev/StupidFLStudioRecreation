/*
================================================================================
  MIRKO STEVANOVSKI MUSIC MAKER 11
  (c) 2011 Mirko Stevanovski Software Inc.
  Version 11.0.0  Build 2314

  COMPILE (MinGW/GCC on Windows):
    gcc MirkoMusicMaker11.c -o MirkoMusicMaker11.exe -lwinmm -lm -lgdi32 -lcomdlg32 -mwindows
================================================================================
*/

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <mmsystem.h>
#include <commdlg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <time.h>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comdlg32.lib")

/* ── dimensions ── */
#define WIN_W          1100
#define WIN_H           720
#define TOOLBAR_H        38
#define STATUSBAR_H      22
#define SIDEBAR_W       180
#define PIANO_H         110
#define SEQROW_H         34
#define SEQCOL_W         30
#define MIXER_CH_W       54
#define NUM_TRACKS        8
#define NUM_STEPS        16
#define SAMPLE_RATE   44100
#define BUF_SAMPLES    2048
#define NUM_BUFS          4
#define MAX_POLY          6
#define MAX_INSTR        16
#define MAX_PATTERNS     16

/* ── colours (COLORREF) ── */
#define COL_BG          RGB(30,30,35)
#define COL_PANEL       RGB(45,45,52)
#define COL_PANEL2      RGB(55,55,64)
#define COL_BORDER      RGB(20,20,24)
#define COL_ACCENT      RGB(255,120,0)
#define COL_ACCENT2     RGB(0,160,220)
#define COL_TEXT        RGB(220,220,220)
#define COL_TEXT_DIM    RGB(120,120,130)
#define COL_STEP_OFF    RGB(50,50,58)
#define COL_STEP_ON     RGB(255,130,0)
#define COL_STEP_ON2    RGB(255,200,60)
#define COL_STEP_PLAY   RGB(0,220,255)
#define COL_STEP_GHOST  RGB(80,80,90)
#define COL_PIANO_W     RGB(240,235,225)
#define COL_PIANO_B     RGB(22,22,22)
#define COL_PIANO_HI    RGB(0,160,255)
#define COL_VU_GREEN    RGB(60,220,60)
#define COL_VU_YELLOW   RGB(240,220,0)
#define COL_VU_RED      RGB(255,40,40)
#define COL_KNOB_BG     RGB(38,38,44)
#define COL_KNOB_RIM    RGB(90,90,100)
#define COL_KNOB_DOT    RGB(255,130,0)
#define COL_SEL         RGB(0,120,220)

/* track hues */
static COLORREF TRACK_COLS[8] = {
    RGB(255,100, 60), RGB( 60,180,255), RGB(130,230, 80),
    RGB(220, 80,220), RGB(255,200, 40), RGB( 60,220,200),
    RGB(255,140,180), RGB(160,140,255)
};

/* ── wave types ── */
#define WAVE_SINE      0
#define WAVE_SQUARE    1
#define WAVE_SAW       2
#define WAVE_TRIANGLE  3
#define WAVE_NOISE     4
#define WAVE_PULSE     5
#define WAVE_COUNT     6
static const char *WAVE_NAMES[WAVE_COUNT] = {"Sine","Square","Saw","Triangle","Noise","Pulse"};

/* ── IDs ── */
#define ID_TIMER_UI     1
#define ID_TIMER_BLINK  2
#define ID_BTN_PLAY     101
#define ID_BTN_STOP     102
#define ID_BTN_REC      103
#define ID_BTN_BPM_UP   104
#define ID_BTN_BPM_DN   105
#define ID_BTN_SEQ      110
#define ID_BTN_MIXER    111
#define ID_BTN_INSTR    112
#define ID_BTN_SONG     113
#define ID_BTN_SAVE     120
#define ID_BTN_LOAD     121
#define ID_BTN_EXPORT   122
#define ID_BTN_NEW      123
#define ID_MENU_NEW     200
#define ID_MENU_SAVE    201
#define ID_MENU_LOAD    202
#define ID_MENU_EXPORT  203
#define ID_MENU_EXIT    204
#define ID_MENU_ABOUT   205
#define ID_MENU_UNDO    210
#define ID_MENU_REDO    211
#define ID_MENU_CLEAR   212

/* ── views ── */
#define VIEW_SEQ    0
#define VIEW_MIXER  1
#define VIEW_INSTR  2
#define VIEW_SONG   3
#define APP_NAME "Mirko Music Maker 11"

/* ── structs ── */
typedef struct { double a,d,s,r; } ADSR;

typedef struct {
    char   name[32];
    int    wave;
    double volume, pan, detune, pw;
    ADSR   env;
    double filter_cutoff, filter_res;
    int    filter_on;
    double reverb_send, delay_send;
    int    octave;
    COLORREF color;
} Instrument;

typedef struct {
    int    grid[NUM_TRACKS][NUM_STEPS];   /* 0=off,1=on */
    int    vel [NUM_TRACKS][NUM_STEPS];   /* 0-127 */
    int    note[NUM_TRACKS][NUM_STEPS];   /* MIDI note */
    char   name[32];
} Pattern;

typedef struct {
    char    name[32];
    double volume, pan;
    double eq_lo, eq_mid, eq_hi;
    double send_rev, send_dly;
    int    mute, solo;
    double vu_l, vu_r;
    double peak_l, peak_r;
    int    peak_hold_l, peak_hold_r;
} MixerCh;

typedef struct {
    double phase;
    double env_pos;
    int    env_stage;   /* 0=A 1=D 2=S 3=R 4=off */
    double env_val;
    int    note;
    int    instr;
    int    vel;
    int    active;
    double noise_state;
} Voice;

/* ── audio ── */
static HWAVEOUT   g_waveout   = NULL;
static WAVEHDR    g_hdrs[NUM_BUFS];
static short     *g_bufs[NUM_BUFS];
static int        g_cur_buf   = 0;
static CRITICAL_SECTION g_lock;
static volatile int g_audio_ok = 0;

/* ── project state ── */
static Instrument g_instr[MAX_INSTR];
static int        g_num_instr = 0;
static Pattern    g_patterns[MAX_PATTERNS];
static int        g_num_patterns = 1;
static int        g_cur_pattern  = 0;
static MixerCh    g_mixer[NUM_TRACKS+1]; /* +1 master */
static double     g_bpm    = 128.0;
static int        g_steps  = NUM_STEPS;
static int        g_playing= 0;
static int        g_recording=0;
static double     g_beat_pos = 0.0;   /* beat within bar */
static int        g_cur_step  = 0;
static double     g_samples_per_step = 0.0;
static double     g_step_accum = 0.0;
static Voice      g_voices[MAX_POLY];
static double     g_master_vol = 0.85;
static double     g_swing = 0.0;

/* reverb / delay send buffers */
static double g_rev_buf[32768]; static int g_rev_pos=0;
static double g_dly_buf[32768]; static int g_dly_pos=0;
static double g_rev_mix=0.25, g_dly_mix=0.20, g_dly_fb=0.45;

/* ── UI state ── */
static HWND   g_hwnd        = NULL;
static int    g_view        = VIEW_SEQ;
static int    g_sel_track   = 0;
static int    g_sel_step    = -1;
static int    g_sel_instr   = 0;
static int    g_sel_pattern = 0;
static int    g_blink       = 0;
static int    g_dirty       = 0;
static char   g_status[128] = "Welcome to Mirko Stevanovski MUSIC MAKER 11";
static int    g_piano_oct   = 4;
static int    g_keys_held[256] = {0};
/* song editor */
static int    g_song[64];    /* pattern index per bar */
static int    g_song_len = 4;
static int    g_song_pos = 0;
static int    g_song_mode= 0; /* 0=pattern loop, 1=song */
/* drag state */
static int    g_drag_step = -1;
static int    g_drag_track= -1;
static int    g_drag_val  = 0;
/* knob drag */
static int    g_knob_drag    = -1;
static int    g_knob_drag_y0 = 0;
static double g_knob_drag_v0 = 0.0;
/* context menu */
static int    g_ctx_track=-1, g_ctx_step=-1;

/* ── forward declarations ── */
LRESULT CALLBACK WndProc(HWND,UINT,WPARAM,LPARAM);
static void AudioInit(void);
static void AudioStop(void);
static void CALLBACK WaveOutCallback(HWAVEOUT,UINT,DWORD_PTR,DWORD_PTR,DWORD_PTR);
static void FillBuffer(short *buf, int nsamples);
static double SynthVoice(Voice *v, Instrument *ins, double dt);
static double ApplyFilter(double x, double cut, double res, double *x1,double *x2,double *y1,double *y2);
static void NoteOn(int instr, int note, int vel);
static void NoteOff(int note);
static void StepTrigger(int track, int step);
static void InitProject(void);
static void InitDefaultInstruments(void);
static void DrawAll(HDC hdc, RECT *rc);
static void DrawToolbar(HDC hdc, RECT *rc);
static void DrawSidebar(HDC hdc, RECT *rc);
static void DrawSequencer(HDC hdc, RECT *rc);
static void DrawMixer(HDC hdc, RECT *rc);
static void DrawInstrEditor(HDC hdc, RECT *rc);
static void DrawSongEditor(HDC hdc, RECT *rc);
static void DrawPiano(HDC hdc, RECT *rc);
static void DrawStatusBar(HDC hdc, RECT *rc);
static void DrawKnob(HDC hdc, int cx, int cy, int r, double val, COLORREF dot, const char *lbl);
static void DrawVU(HDC hdc, int x, int y, int w, int h, double val, double peak);
static void DrawLED(HDC hdc, int cx, int cy, int r, int on, COLORREF col);
static void DrawButton3D(HDC hdc, RECT *r, const char *txt, int pressed, COLORREF face);
static void DrawPrettyText(HDC hdc, int x, int y, const char *txt, COLORREF col, int bold);
static void SetStatus(const char *fmt, ...);
static void SaveProject(const char *path);
static void LoadProject(const char *path);
static void ExportWAV(const char *path);
static double MidiNoteToFreq(int note);
static const char *NoteName(int note);
static HFONT MakeFont(int size, int bold);
static void FillRoundRect(HDC hdc, RECT *r, int rx, COLORREF col);
static void StrokeRoundRect(HDC hdc, RECT *r, int rx, COLORREF col, int thick);
static int  HitTestSeqGrid(int mx, int my, int *track, int *step);
static int  HitTestMixer(int mx, int my, int *ch, int *zone);
static int  HitTestInstr(int mx, int my, int *knob);
static int  HitTestPiano(int mx, int my);

/* ============================================================
   HELPERS
   ============================================================ */
static double MidiNoteToFreq(int note){
    return 440.0 * pow(2.0,(note-69)/12.0);
}
static const char *NoteName(int n){
    static const char *nm[]={"C","C#","D","D#","E","F","F#","G","G#","A","A#","B"};
    static char buf[8];
    sprintf(buf,"%s%d",nm[n%12],n/12-1);
    return buf;
}
static HFONT MakeFont(int sz, int bold){
    return CreateFont(sz,0,0,0,bold?FW_BOLD:FW_NORMAL,0,0,0,
        ANSI_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_SWISS,"Segoe UI");
}
static void SetStatus(const char *fmt,...){
    va_list va; va_start(va,fmt);
    vsnprintf(g_status,sizeof(g_status),fmt,va);
    va_end(va);
}
static void FillRoundRect(HDC hdc,RECT *r,int rx,COLORREF col){
    HBRUSH b=CreateSolidBrush(col);
    HPEN   p=CreatePen(PS_SOLID,0,col);
    HBRUSH ob=(HBRUSH)SelectObject(hdc,b);
    HPEN   op=(HPEN)SelectObject(hdc,p);
    RoundRect(hdc,r->left,r->top,r->right,r->bottom,rx,rx);
    SelectObject(hdc,ob); SelectObject(hdc,op);
    DeleteObject(b); DeleteObject(p);
}
static void StrokeRoundRect(HDC hdc,RECT *r,int rx,COLORREF col,int thick){
    HBRUSH ob=(HBRUSH)SelectObject(hdc,GetStockObject(NULL_BRUSH));
    HPEN p=CreatePen(PS_SOLID,thick,col);
    HPEN op=(HPEN)SelectObject(hdc,p);
    RoundRect(hdc,r->left,r->top,r->right,r->bottom,rx,rx);
    SelectObject(hdc,ob); SelectObject(hdc,op);
    DeleteObject(p);
}
static void DrawPrettyText(HDC hdc,int x,int y,const char *txt,COLORREF col,int bold){
    HFONT f=MakeFont(12,bold);
    HFONT of=(HFONT)SelectObject(hdc,f);
    SetBkMode(hdc,TRANSPARENT);
    SetTextColor(hdc,col);
    TextOut(hdc,x,y,txt,(int)strlen(txt));
    SelectObject(hdc,of); DeleteObject(f);
}
static void DrawButton3D(HDC hdc,RECT *r,const char *txt,int pressed,COLORREF face){
    FillRoundRect(hdc,r,5,pressed?RGB(GetRValue(face)*7/10,GetGValue(face)*7/10,GetBValue(face)*7/10):face);
    if(!pressed){
        /* highlight top-left */
        HPEN hp=CreatePen(PS_SOLID,1,RGB(200,200,210));
        HPEN op=(HPEN)SelectObject(hdc,hp);
        MoveToEx(hdc,r->left+2,r->bottom-3,NULL);
        LineTo(hdc,r->left+2,r->top+2);
        LineTo(hdc,r->right-3,r->top+2);
        SelectObject(hdc,op); DeleteObject(hp);
        hp=CreatePen(PS_SOLID,1,RGB(15,15,18));
        op=(HPEN)SelectObject(hdc,hp);
        MoveToEx(hdc,r->left+2,r->bottom-2,NULL);
        LineTo(hdc,r->right-2,r->bottom-2);
        LineTo(hdc,r->right-2,r->top+2);
        SelectObject(hdc,op); DeleteObject(hp);
    }
    /* text */
    HFONT f=MakeFont(11,1);
    HFONT of=(HFONT)SelectObject(hdc,f);
    SetBkMode(hdc,TRANSPARENT);
    SetTextColor(hdc,COL_TEXT);
    DrawText(hdc,txt,-1,(RECT*)r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    SelectObject(hdc,of); DeleteObject(f);
}
static void DrawKnob(HDC hdc,int cx,int cy,int r,double val,COLORREF dot,const char *lbl){
    /* shadow */
    RECT sr={cx-r-1,cy-r-1,cx+r+1,cy+r+1};
    HBRUSH sb=CreateSolidBrush(RGB(10,10,12));
    HPEN   np=(HPEN)GetStockObject(NULL_PEN);
    HBRUSH ob=(HBRUSH)SelectObject(hdc,sb);
    HPEN   op=(HPEN)SelectObject(hdc,np);
    Ellipse(hdc,cx-r,cy-r+2,cx+r,cy+r+2);
    SelectObject(hdc,ob); DeleteObject(sb);
    /* rim */
    HBRUSH rb=CreateSolidBrush(COL_KNOB_RIM);
    ob=(HBRUSH)SelectObject(hdc,rb);
    SelectObject(hdc,np);
    Ellipse(hdc,cx-r,cy-r,cx+r,cy+r);
    SelectObject(hdc,ob); DeleteObject(rb);
    /* body */
    HBRUSH bb=CreateSolidBrush(COL_KNOB_BG);
    ob=(HBRUSH)SelectObject(hdc,bb);
    Ellipse(hdc,cx-r+2,cy-r+2,cx+r-2,cy+r-2);
    SelectObject(hdc,ob); DeleteObject(bb);
    /* arc indicator */
    double start_ang = -225.0 * 3.14159/180.0;
    double range_ang =  270.0 * 3.14159/180.0;
    double ang = start_ang + val*range_ang;
    HPEN ap=CreatePen(PS_SOLID,2,COL_ACCENT);
    op=(HPEN)SelectObject(hdc,ap);
    /* draw arc segments from start to current */
    int segs=36;
    for(int i=0;i<segs;i++){
        double t0=start_ang + (double)i/segs * range_ang;
        double t1=start_ang + (double)(i+1)/segs * range_ang;
        if(t1>ang) break;
        int x0=cx+(int)((r-2)*cos(t0));
        int y0=cy+(int)((r-2)*sin(t0));
        int x1=cx+(int)((r-2)*cos(t1));
        int y1=cy+(int)((r-2)*sin(t1));
        MoveToEx(hdc,x0,y0,NULL);
        LineTo(hdc,x1,y1);
    }
    SelectObject(hdc,op); DeleteObject(ap);
    /* dot */
    int dx=cx+(int)((r-5)*cos(ang));
    int dy=cy+(int)((r-5)*sin(ang));
    HBRUSH db=CreateSolidBrush(dot);
    ob=(HBRUSH)SelectObject(hdc,db);
    op=(HPEN)SelectObject(hdc,np);
    Ellipse(hdc,dx-2,dy-2,dx+3,dy+3);
    SelectObject(hdc,ob); DeleteObject(db);
    SelectObject(hdc,op);
    /* label */
    if(lbl){
        HFONT f=MakeFont(9,0);
        HFONT of=(HFONT)SelectObject(hdc,f);
        SetBkMode(hdc,TRANSPARENT);
        SetTextColor(hdc,COL_TEXT_DIM);
        RECT lr={cx-r,cy+r+1,cx+r,cy+r+14};
        DrawText(hdc,lbl,-1,&lr,DT_CENTER|DT_TOP|DT_SINGLELINE);
        SelectObject(hdc,of); DeleteObject(f);
    }
}
static void DrawVU(HDC hdc,int x,int y,int w,int h,double val,double peak){
    /* background */
    RECT br={x,y,x+w,y+h};
    FillRoundRect(hdc,&br,2,RGB(15,15,18));
    /* segments */
    int segs=16;
    int seg_h=(h-segs*1)/segs;
    for(int i=0;i<segs;i++){
        double t=(double)(segs-1-i)/segs;
        int sy=y+i*(seg_h+1);
        RECT sr2={x+1,sy,x+w-1,sy+seg_h};
        COLORREF col;
        if(t<0.6)       col=COL_VU_GREEN;
        else if(t<0.85) col=COL_VU_YELLOW;
        else            col=COL_VU_RED;
        if(t<=val){
            FillRoundRect(hdc,&sr2,1,col);
        } else {
            FillRoundRect(hdc,&sr2,1,RGB(30,30,35));
        }
    }
    /* peak marker */
    if(peak>0.0){
        int pi=y+(int)((1.0-peak)*(h-(segs+1)));
        HPEN pp=CreatePen(PS_SOLID,1,COL_VU_YELLOW);
        HPEN op=(HPEN)SelectObject(hdc,pp);
        MoveToEx(hdc,x+1,pi,NULL); LineTo(hdc,x+w-1,pi);
        SelectObject(hdc,op); DeleteObject(pp);
    }
}
static void DrawLED(HDC hdc,int cx,int cy,int r,int on,COLORREF col){
    HBRUSH b=CreateSolidBrush(on?col:RGB(30,15,5));
    HPEN   p=CreatePen(PS_SOLID,1,RGB(10,10,12));
    HBRUSH ob=(HBRUSH)SelectObject(hdc,b);
    HPEN   op=(HPEN)SelectObject(hdc,p);
    Ellipse(hdc,cx-r,cy-r,cx+r+1,cy+r+1);
    if(on){
        /* glow */
        HBRUSH gb=CreateSolidBrush(RGB(
            min(255,GetRValue(col)+80),
            min(255,GetGValue(col)+80),
            min(255,GetBValue(col)+80)));
        HBRUSH ob2=(HBRUSH)SelectObject(hdc,gb);
        HPEN np=(HPEN)GetStockObject(NULL_PEN);
        HPEN op2=(HPEN)SelectObject(hdc,np);
        Ellipse(hdc,cx-r/2,cy-r/2,cx+r/2+1,cy+r/2+1);
        SelectObject(hdc,ob2); DeleteObject(gb);
        SelectObject(hdc,op2);
    }
    SelectObject(hdc,ob); SelectObject(hdc,op);
    DeleteObject(b); DeleteObject(p);
}

/* ============================================================
   AUDIO ENGINE
   ============================================================ */
static double ApplyFilter(double x,double cut,double res,
                           double *x1,double *x2,double *y1,double *y2){
    /* Simple biquad low-pass */
    double omega = 2.0*3.14159265*cut/SAMPLE_RATE;
    double sinw  = sin(omega);
    double cosw  = cos(omega);
    double alpha = sinw/(2.0*res);
    double b0=  (1.0-cosw)/2.0;
    double b1=  1.0-cosw;
    double b2=  (1.0-cosw)/2.0;
    double a0=  1.0+alpha;
    double a1= -2.0*cosw;
    double a2=  1.0-alpha;
    double out = (b0/a0)*x + (b1/a0)*(*x1) + (b2/a0)*(*x2)
                           - (a1/a0)*(*y1) - (a2/a0)*(*y2);
    *x2=*x1; *x1=x;
    *y2=*y1; *y1=out;
    return out;
}
static double EnvTick(Voice *v, Instrument *ins, double dt){
    double val=v->env_val;
    switch(v->env_stage){
        case 0: /* attack */
            val+=dt/(ins->env.a>0.001?ins->env.a:0.001);
            if(val>=1.0){val=1.0;v->env_stage=1;}
            break;
        case 1: /* decay */
            val-=dt/(ins->env.d>0.001?ins->env.d:0.001)*(1.0-ins->env.s);
            if(val<=ins->env.s){val=ins->env.s;v->env_stage=2;}
            break;
        case 2: /* sustain */
            val=ins->env.s;
            break;
        case 3: /* release */
            val-=dt/(ins->env.r>0.001?ins->env.r:0.001)*v->env_val;
            if(val<=0.0){val=0.0;v->env_stage=4;v->active=0;}
            break;
        default: val=0.0; break;
    }
    v->env_val=val;
    return val;
}
static double SynthVoice(Voice *v, Instrument *ins, double dt){
    double freq = MidiNoteToFreq(v->note + ins->octave*12) * pow(2.0,ins->detune/1200.0);
    v->phase += freq*dt;
    if(v->phase>1.0) v->phase-=1.0;
    double s=0.0;
    switch(ins->wave){
        case WAVE_SINE:     s=sin(v->phase*2.0*3.14159265); break;
        case WAVE_SQUARE:   s=v->phase<0.5?1.0:-1.0; break;
        case WAVE_SAW:      s=2.0*v->phase-1.0; break;
        case WAVE_TRIANGLE: s=v->phase<0.5?4.0*v->phase-1.0:3.0-4.0*v->phase; break;
        case WAVE_NOISE:
            v->noise_state=v->noise_state*0.9999+(double)(rand()-(RAND_MAX/2))/(RAND_MAX/2)*0.0001;
            s=(double)(rand()-(RAND_MAX/2))/(RAND_MAX/2);
            break;
        case WAVE_PULSE:    s=v->phase<ins->pw?1.0:-1.0; break;
        default:            s=sin(v->phase*2.0*3.14159265); break;
    }
    /* envelope */
    double env=EnvTick(v,ins,dt);
    s *= env * (v->vel/127.0) * ins->volume;
    return s;
}
static void NoteOn(int instr,int note,int vel){
    /* find a free or steal oldest */
    int slot=-1;
    for(int i=0;i<MAX_POLY;i++){
        if(!g_voices[i].active){slot=i;break;}
    }
    if(slot<0){
        /* steal first (oldest) */
        slot=0;
        for(int i=1;i<MAX_POLY;i++)
            if(g_voices[i].env_stage>g_voices[slot].env_stage) slot=i;
    }
    Voice *v=&g_voices[slot];
    v->phase=0.0; v->env_pos=0.0; v->env_stage=0; v->env_val=0.0;
    v->note=note; v->instr=instr; v->vel=vel; v->active=1;
    v->noise_state=0.0;
}
static void NoteOff(int note){
    for(int i=0;i<MAX_POLY;i++){
        if(g_voices[i].active && g_voices[i].note==note && g_voices[i].env_stage<3)
            g_voices[i].env_stage=3;
    }
}
static void StepTrigger(int track,int step){
    Pattern *p=&g_patterns[g_cur_pattern];
    if(!p->grid[track][step]) return;
    if(g_mixer[track].mute) return;
    int instr=track % g_num_instr;
    int note =p->note[track][step];
    int vel  =p->vel [track][step];
    NoteOn(instr,note,vel);
}
static void FillBuffer(short *buf, int nsamples){
    EnterCriticalSection(&g_lock);
    double dt=1.0/SAMPLE_RATE;
    double samples_per_step=60.0/g_bpm/4.0*SAMPLE_RATE; /* 1/16th note */
    for(int i=0;i<nsamples;i++){
        /* step advance */
        if(g_playing){
            g_step_accum++;
            double eff_spstep=samples_per_step;
            /* swing: odd steps slightly longer */
            if((g_cur_step&1)==1) eff_spstep*=(1.0+g_swing*0.33);
            if(g_step_accum>=eff_spstep){
                g_step_accum=0.0;
                g_cur_step=(g_cur_step+1)%g_steps;
                /* trigger all tracks at this step */
                for(int t=0;t<NUM_TRACKS;t++)
                    StepTrigger(t,g_cur_step);
            }
        }
        /* mix voices */
        double outL=0.0, outR=0.0;
        for(int v=0;v<MAX_POLY;v++){
            if(!g_voices[v].active) continue;
            Instrument *ins=&g_instr[g_voices[v].instr % g_num_instr];
            double s=SynthVoice(&g_voices[v],ins,dt);
            double pan=ins->pan;
            double volL=(1.0-fmax(0.0,pan))*g_mixer[g_voices[v].instr%NUM_TRACKS].volume;
            double volR=(1.0+fmin(0.0,pan))*g_mixer[g_voices[v].instr%NUM_TRACKS].volume;
            outL+=s*volL;
            outR+=s*volR;
        }
        /* simple reverb */
        int ri=(g_rev_pos+5000)&32767;
        double rev=g_rev_buf[ri]*0.7;
        g_rev_buf[g_rev_pos&32767]=(outL+outR)*0.5*g_rev_mix + rev*0.6;
        outL+=rev*g_rev_mix; outR+=rev*g_rev_mix;
        g_rev_pos++;
        /* simple delay */
        int di=(g_dly_pos+(int)(0.375*SAMPLE_RATE))&32767;
        double dly=g_dly_buf[di]*g_dly_fb;
        g_dly_buf[g_dly_pos&32767]=(outL+outR)*0.5+dly;
        outL+=dly*g_dly_mix; outR+=dly*g_dly_mix;
        g_dly_pos++;
        /* master */
        outL*=g_master_vol;
        outR*=g_master_vol;
        /* clip */
        if(outL> 1.0) outL= 1.0; if(outL<-1.0) outL=-1.0;
        if(outR> 1.0) outR= 1.0; if(outR<-1.0) outR=-1.0;
        /* write stereo interleaved */
        buf[i*2  ]=(short)(outL*30000.0);
        buf[i*2+1]=(short)(outR*30000.0);
        /* update master VU */
        double vl=fabs(outL), vr=fabs(outR);
        g_mixer[NUM_TRACKS].vu_l = g_mixer[NUM_TRACKS].vu_l*0.995 + vl*0.005;
        g_mixer[NUM_TRACKS].vu_r = g_mixer[NUM_TRACKS].vu_r*0.995 + vr*0.005;
        if(vl>g_mixer[NUM_TRACKS].peak_l){g_mixer[NUM_TRACKS].peak_l=vl;g_mixer[NUM_TRACKS].peak_hold_l=120;}
        if(vr>g_mixer[NUM_TRACKS].peak_r){g_mixer[NUM_TRACKS].peak_r=vr;g_mixer[NUM_TRACKS].peak_hold_r=120;}
    }
    LeaveCriticalSection(&g_lock);
}
static void CALLBACK WaveOutCallback(HWAVEOUT hwo,UINT msg,DWORD_PTR inst,DWORD_PTR p1,DWORD_PTR p2){
    if(msg==WOM_DONE && g_audio_ok){
        WAVEHDR *hdr=(WAVEHDR*)p1;
        FillBuffer((short*)hdr->lpData, BUF_SAMPLES);
        waveOutWrite(hwo,hdr,sizeof(WAVEHDR));
    }
}
static void AudioInit(void){
    WAVEFORMATEX wfx={0};
    wfx.wFormatTag=WAVE_FORMAT_PCM;
    wfx.nChannels=2;
    wfx.nSamplesPerSec=SAMPLE_RATE;
    wfx.wBitsPerSample=16;
    wfx.nBlockAlign=wfx.nChannels*wfx.wBitsPerSample/8;
    wfx.nAvgBytesPerSec=wfx.nSamplesPerSec*wfx.nBlockAlign;
    if(waveOutOpen(&g_waveout,WAVE_MAPPER,&wfx,(DWORD_PTR)WaveOutCallback,0,CALLBACK_FUNCTION)!=MMSYSERR_NOERROR){
        MessageBox(NULL,"Failed to open audio device.","Audio Error",MB_ICONERROR);
        return;
    }
    g_audio_ok=1;
    for(int i=0;i<NUM_BUFS;i++){
        g_bufs[i]=(short*)malloc(BUF_SAMPLES*2*sizeof(short));
        memset(g_bufs[i],0,BUF_SAMPLES*2*sizeof(short));
        memset(&g_hdrs[i],0,sizeof(WAVEHDR));
        g_hdrs[i].lpData=(LPSTR)g_bufs[i];
        g_hdrs[i].dwBufferLength=BUF_SAMPLES*2*sizeof(short);
        waveOutPrepareHeader(g_waveout,&g_hdrs[i],sizeof(WAVEHDR));
        FillBuffer(g_bufs[i],BUF_SAMPLES);
        waveOutWrite(g_waveout,&g_hdrs[i],sizeof(WAVEHDR));
    }
}
static void AudioStop(void){
    g_audio_ok=0;
    if(g_waveout){
        waveOutReset(g_waveout);
        for(int i=0;i<NUM_BUFS;i++){
            waveOutUnprepareHeader(g_waveout,&g_hdrs[i],sizeof(WAVEHDR));
            free(g_bufs[i]);
        }
        waveOutClose(g_waveout);
        g_waveout=NULL;
    }
}

/* ============================================================
   PROJECT INIT
   ============================================================ */
static void InitDefaultInstruments(void){
    /* 0 - Kick */
    strcpy(g_instr[0].name,"Kick");
    g_instr[0].wave=WAVE_SINE; g_instr[0].volume=0.9; g_instr[0].pan=0.0;
    g_instr[0].env.a=0.002; g_instr[0].env.d=0.15; g_instr[0].env.s=0.0; g_instr[0].env.r=0.10;
    g_instr[0].octave=-1; g_instr[0].filter_cutoff=400; g_instr[0].filter_res=0.7; g_instr[0].filter_on=1;
    g_instr[0].color=TRACK_COLS[0];
    /* 1 - Snare */
    strcpy(g_instr[1].name,"Snare");
    g_instr[1].wave=WAVE_NOISE; g_instr[1].volume=0.75; g_instr[1].pan=0.0;
    g_instr[1].env.a=0.003; g_instr[1].env.d=0.08; g_instr[1].env.s=0.0; g_instr[1].env.r=0.05;
    g_instr[1].octave=0; g_instr[1].filter_cutoff=3000; g_instr[1].filter_res=0.5; g_instr[1].filter_on=1;
    g_instr[1].color=TRACK_COLS[1];
    /* 2 - Hi-Hat */
    strcpy(g_instr[2].name,"HiHat");
    g_instr[2].wave=WAVE_NOISE; g_instr[2].volume=0.55; g_instr[2].pan=0.2;
    g_instr[2].env.a=0.001; g_instr[2].env.d=0.03; g_instr[2].env.s=0.0; g_instr[2].env.r=0.02;
    g_instr[2].octave=1; g_instr[2].filter_cutoff=8000; g_instr[2].filter_res=0.3; g_instr[2].filter_on=1;
    g_instr[2].color=TRACK_COLS[2];
    /* 3 - Bass */
    strcpy(g_instr[3].name,"Bass");
    g_instr[3].wave=WAVE_SAW; g_instr[3].volume=0.7; g_instr[3].pan=-0.1;
    g_instr[3].env.a=0.005; g_instr[3].env.d=0.2; g_instr[3].env.s=0.6; g_instr[3].env.r=0.15;
    g_instr[3].octave=-1; g_instr[3].filter_cutoff=600; g_instr[3].filter_res=1.8; g_instr[3].filter_on=1;
    g_instr[3].color=TRACK_COLS[3];
    /* 4 - Lead Synth */
    strcpy(g_instr[4].name,"Lead");
    g_instr[4].wave=WAVE_SQUARE; g_instr[4].volume=0.6; g_instr[4].pan=0.15;
    g_instr[4].env.a=0.01; g_instr[4].env.d=0.1; g_instr[4].env.s=0.7; g_instr[4].env.r=0.2;
    g_instr[4].octave=0; g_instr[4].filter_cutoff=2000; g_instr[4].filter_res=1.2; g_instr[4].filter_on=1;
    g_instr[4].pw=0.3; g_instr[4].color=TRACK_COLS[4];
    /* 5 - Pad */
    strcpy(g_instr[5].name,"Pad");
    g_instr[5].wave=WAVE_TRIANGLE; g_instr[5].volume=0.5; g_instr[5].pan=-0.2;
    g_instr[5].env.a=0.3; g_instr[5].env.d=0.3; g_instr[5].env.s=0.8; g_instr[5].env.r=0.5;
    g_instr[5].octave=0; g_instr[5].filter_cutoff=1500; g_instr[5].filter_res=0.8; g_instr[5].filter_on=0;
    g_instr[5].color=TRACK_COLS[5];
    /* 6 - Arp */
    strcpy(g_instr[6].name,"Arp");
    g_instr[6].wave=WAVE_SAW; g_instr[6].volume=0.55; g_instr[6].pan=0.3;
    g_instr[6].env.a=0.005; g_instr[6].env.d=0.05; g_instr[6].env.s=0.5; g_instr[6].env.r=0.08;
    g_instr[6].octave=1; g_instr[6].filter_cutoff=3500; g_instr[6].filter_res=1.5; g_instr[6].filter_on=1;
    g_instr[6].color=TRACK_COLS[6];
    /* 7 - FX */
    strcpy(g_instr[7].name,"FX");
    g_instr[7].wave=WAVE_NOISE; g_instr[7].volume=0.4; g_instr[7].pan=0.0;
    g_instr[7].env.a=0.1; g_instr[7].env.d=0.4; g_instr[7].env.s=0.3; g_instr[7].env.r=0.6;
    g_instr[7].octave=0; g_instr[7].filter_cutoff=1200; g_instr[7].filter_res=2.0; g_instr[7].filter_on=1;
    g_instr[7].color=TRACK_COLS[7];
    g_num_instr=8;
}
static void InitProject(void){
    memset(&g_patterns,0,sizeof(g_patterns));
    for(int p=0;p<MAX_PATTERNS;p++){
        sprintf(g_patterns[p].name,"Pattern %d",p+1);
        for(int t=0;t<NUM_TRACKS;t++)
            for(int s=0;s<NUM_STEPS;s++){
                g_patterns[p].vel[t][s]=100;
                g_patterns[p].note[t][s]=60+(t*3);
            }
    }
    /* default beat pattern */
    /* kick on 0,4,8,12 */
    int kick[]={0,4,8,12};
    for(int i=0;i<4;i++) g_patterns[0].grid[0][kick[i]]=1;
    /* snare on 4,12 */
    g_patterns[0].grid[1][4]=1; g_patterns[0].grid[1][12]=1;
    /* hihat every 2 steps */
    for(int i=0;i<8;i++) g_patterns[0].grid[2][i*2]=1;
    /* bass riff */
    int bass[]={0,3,6,10};
    for(int i=0;i<4;i++) g_patterns[0].grid[3][bass[i]]=1;
    /* lead melody */
    int lead[]={1,5,9,13};
    for(int i=0;i<4;i++) g_patterns[0].grid[4][lead[i]]=1;

    memset(&g_mixer,0,sizeof(g_mixer));
    for(int i=0;i<=NUM_TRACKS;i++){
        g_mixer[i].volume=0.8; g_mixer[i].pan=0.0;
        g_mixer[i].eq_lo=0.0; g_mixer[i].eq_mid=0.0; g_mixer[i].eq_hi=0.0;
        sprintf(g_mixer[i].name,i<NUM_TRACKS?"Track %d":"Master",i+1);
    }
    g_mixer[NUM_TRACKS].volume=1.0;

    memset(&g_voices,0,sizeof(g_voices));
    for(int i=0;i<MAX_POLY;i++) g_voices[i].active=0;

    for(int i=0;i<64;i++) g_song[i]=i%MAX_PATTERNS;
    g_song_len=4;
    g_cur_pattern=0; g_num_patterns=4;
    memset(g_rev_buf,0,sizeof(g_rev_buf));
    memset(g_dly_buf,0,sizeof(g_dly_buf));
    SetStatus("New project created.");
}

/* ============================================================
   SAVE / LOAD (simple binary)
   ============================================================ */
static void SaveProject(const char *path){
    FILE *f=fopen(path,"wb");
    if(!f){SetStatus("Save failed: %s",path);return;}
    int magic=0x4D4D3131; /* "MM11" */
    fwrite(&magic,4,1,f);
    fwrite(g_instr,sizeof(g_instr),1,f);
    fwrite(&g_num_instr,4,1,f);
    fwrite(g_patterns,sizeof(g_patterns),1,f);
    fwrite(&g_num_patterns,4,1,f);
    fwrite(&g_cur_pattern,4,1,f);
    fwrite(g_mixer,sizeof(g_mixer),1,f);
    fwrite(&g_bpm,8,1,f);
    fwrite(&g_steps,4,1,f);
    fwrite(&g_master_vol,8,1,f);
    fwrite(&g_swing,8,1,f);
    fwrite(g_song,sizeof(g_song),1,f);
    fwrite(&g_song_len,4,1,f);
    fclose(f);
    g_dirty=0;
    SetStatus("Saved: %s",path);
}
static void LoadProject(const char *path){
    FILE *f=fopen(path,"rb");
    if(!f){SetStatus("Load failed: %s",path);return;}
    int magic=0;
    fread(&magic,4,1,f);
    if(magic!=0x4D4D3131){fclose(f);SetStatus("Invalid file format.");return;}
    fread(g_instr,sizeof(g_instr),1,f);
    fread(&g_num_instr,4,1,f);
    fread(g_patterns,sizeof(g_patterns),1,f);
    fread(&g_num_patterns,4,1,f);
    fread(&g_cur_pattern,4,1,f);
    fread(g_mixer,sizeof(g_mixer),1,f);
    fread(&g_bpm,8,1,f);
    fread(&g_steps,4,1,f);
    fread(&g_master_vol,8,1,f);
    fread(&g_swing,8,1,f);
    fread(g_song,sizeof(g_song),1,f);
    fread(&g_song_len,4,1,f);
    fclose(f);
    g_dirty=0;
    SetStatus("Loaded: %s",path);
    InvalidateRect(g_hwnd,NULL,FALSE);
}
static void ExportWAV(const char *path){
    /* render 4 bars offline */
    FILE *f=fopen(path,"wb");
    if(!f){SetStatus("Export failed.");return;}
    int bars=4;
    double spb=60.0/g_bpm;
    int total_samples=(int)(spb*4*bars*SAMPLE_RATE);
    /* WAV header */
    int data_bytes=total_samples*2*2;
    fwrite("RIFF",1,4,f);
    int chunk=36+data_bytes; fwrite(&chunk,4,1,f);
    fwrite("WAVE",1,4,f);
    fwrite("fmt ",1,4,f);
    int fmtsz=16; fwrite(&fmtsz,4,1,f);
    short pcm=1; fwrite(&pcm,2,1,f);
    short ch=2; fwrite(&ch,2,1,f);
    int sr=SAMPLE_RATE; fwrite(&sr,4,1,f);
    int br=SAMPLE_RATE*4; fwrite(&br,4,1,f);
    short ba=4; fwrite(&ba,2,1,f);
    short bps=16; fwrite(&bps,2,1,f);
    fwrite("data",1,4,f);
    fwrite(&data_bytes,4,1,f);
    short *buf=(short*)malloc(BUF_SAMPLES*2*sizeof(short));
    int rem=total_samples;
    int was_playing=g_playing;
    g_playing=1; g_cur_step=0; g_step_accum=0;
    while(rem>0){
        int n=rem<BUF_SAMPLES?rem:BUF_SAMPLES;
        FillBuffer(buf,n);
        fwrite(buf,n*2*sizeof(short),1,f);
        rem-=n;
    }
    g_playing=was_playing;
    free(buf);
    fclose(f);
    SetStatus("Exported WAV: %s",path);
}

/* ============================================================
   DRAWING
   ============================================================ */
static void DrawAll(HDC hdc,RECT *full){
    /* background */
    HBRUSH bgb=CreateSolidBrush(COL_BG);
    FillRect(hdc,full,bgb);
    DeleteObject(bgb);

    RECT toolbar={0,0,full->right,TOOLBAR_H};
    RECT statusbar={0,full->bottom-STATUSBAR_H,full->right,full->bottom};
    RECT sidebar={0,TOOLBAR_H,SIDEBAR_W,full->bottom-STATUSBAR_H};
    RECT main_area={SIDEBAR_W,TOOLBAR_H,full->right,full->bottom-STATUSBAR_H-PIANO_H};
    RECT piano_area={SIDEBAR_W,full->bottom-STATUSBAR_H-PIANO_H,full->right,full->bottom-STATUSBAR_H};

    DrawToolbar(hdc,&toolbar);
    DrawStatusBar(hdc,&statusbar);
    DrawSidebar(hdc,&sidebar);
    DrawPiano(hdc,&piano_area);

    switch(g_view){
        case VIEW_SEQ:    DrawSequencer(hdc,&main_area); break;
        case VIEW_MIXER:  DrawMixer(hdc,&main_area); break;
        case VIEW_INSTR:  DrawInstrEditor(hdc,&main_area); break;
        case VIEW_SONG:   DrawSongEditor(hdc,&main_area); break;
    }
}

static void DrawToolbar(HDC hdc,RECT *rc){
    /* gradient bar */
    for(int y=rc->top;y<rc->bottom;y++){
        double t=(double)(y-rc->top)/(rc->bottom-rc->top);
        int r2=(int)(40+t*20), g2=(int)(40+t*20), b2=(int)(48+t*20);
        HPEN p=CreatePen(PS_SOLID,1,RGB(r2,g2,b2));
        HPEN op=(HPEN)SelectObject(hdc,p);
        MoveToEx(hdc,rc->left,y,NULL); LineTo(hdc,rc->right,y);
        SelectObject(hdc,op); DeleteObject(p);
    }
    /* bottom border */
    HPEN bp=CreatePen(PS_SOLID,1,COL_BORDER);
    HPEN obp=(HPEN)SelectObject(hdc,bp);
    MoveToEx(hdc,rc->left,rc->bottom-1,NULL); LineTo(hdc,rc->right,rc->bottom-1);
    SelectObject(hdc,obp); DeleteObject(bp);

    /* LOGO */
    HFONT lf=MakeFont(14,1);
    HFONT olf=(HFONT)SelectObject(hdc,lf);
    SetBkMode(hdc,TRANSPARENT);
    SetTextColor(hdc,COL_ACCENT);
    TextOut(hdc,6,10,"Mirko Stevanovski",17);
    SelectObject(hdc,olf); DeleteObject(lf);
    HFONT lf2=MakeFont(10,0);
    olf=(HFONT)SelectObject(hdc,lf2);
    SetTextColor(hdc,COL_TEXT_DIM);
    TextOut(hdc,6,25,"MUSIC MAKER 11",14);
    SelectObject(hdc,olf); DeleteObject(lf2);

    /* transport buttons */
    int bx=200, by=5, bw=50, bh=28;
    RECT r;
    /* PLAY */
    r.left=bx; r.top=by; r.right=bx+bw; r.bottom=by+bh;
    DrawButton3D(hdc,&r,g_playing?"  ||":"  \x10",g_playing,g_playing?RGB(30,80,30):COL_PANEL2);
    bx+=bw+4;
    /* STOP */
    r.left=bx; r.top=by; r.right=bx+bw; r.bottom=by+bh;
    DrawButton3D(hdc,&r,"  \xfe",0,COL_PANEL2);
    bx+=bw+4;
    /* REC */
    r.left=bx; r.top=by; r.right=bx+bw; r.bottom=by+bh;
    DrawButton3D(hdc,&r," REC",g_recording,g_recording?RGB(100,20,20):COL_PANEL2);
    bx+=bw+12;

    /* BPM display */
    RECT bpmbg={bx,by,bx+90,by+bh};
    FillRoundRect(hdc,&bpmbg,4,RGB(15,15,18));
    StrokeRoundRect(hdc,&bpmbg,4,RGB(80,80,90),1);
    char bpmstr[32]; sprintf(bpmstr,"%.1f BPM",g_bpm);
    HFONT bf=MakeFont(13,1);
    HFONT obf=(HFONT)SelectObject(hdc,bf);
    SetTextColor(hdc,COL_ACCENT2);
    SetBkMode(hdc,TRANSPARENT);
    DrawText(hdc,bpmstr,-1,(RECT*)&bpmbg,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    SelectObject(hdc,obf); DeleteObject(bf);
    RECT bpm_up={bx+92,by,bx+108,by+13};
    RECT bpm_dn={bx+92,by+15,bx+108,by+bh};
    DrawButton3D(hdc,&bpm_up,"+",0,COL_PANEL2);
    DrawButton3D(hdc,&bpm_dn,"-",0,COL_PANEL2);
    bx+=114;

    /* swing */
    RECT swbg={bx,by,bx+70,by+bh};
    FillRoundRect(hdc,&swbg,4,RGB(15,15,18));
    char swstr[32]; sprintf(swstr,"SW %.0f%%",g_swing*100.0);
    HFONT sf=MakeFont(11,0);
    HFONT osf=(HFONT)SelectObject(hdc,sf);
    SetTextColor(hdc,COL_TEXT_DIM);
    DrawText(hdc,swstr,-1,(RECT*)&swbg,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    SelectObject(hdc,osf); DeleteObject(sf);
    bx+=78;

    /* view buttons */
    const char *vnames[]={"SEQ","MIXER","INSTR","SONG"};
    for(int i=0;i<4;i++){
        r.left=bx; r.top=by; r.right=bx+52; r.bottom=by+bh;
        DrawButton3D(hdc,&r,vnames[i],g_view==i,
            g_view==i?COL_ACCENT:COL_PANEL2);
        bx+=56;
    }
    bx+=8;
    /* save/load */
    r.left=bx; r.top=by; r.right=bx+44; r.bottom=by+bh;
    DrawButton3D(hdc,&r,"SAVE",0,RGB(30,60,100));
    bx+=48;
    r.left=bx; r.top=by; r.right=bx+44; r.bottom=by+bh;
    DrawButton3D(hdc,&r,"LOAD",0,RGB(30,60,100));
    bx+=48;
    r.left=bx; r.top=by; r.right=bx+56; r.bottom=by+bh;
    DrawButton3D(hdc,&r,"EXPORT",0,RGB(60,40,80));

    /* master VU */
    int vux=rc->right-80, vuy=5;
    DrawVU(hdc,vux,   vuy,14,28,g_mixer[NUM_TRACKS].vu_l,g_mixer[NUM_TRACKS].peak_l);
    DrawVU(hdc,vux+16,vuy,14,28,g_mixer[NUM_TRACKS].vu_r,g_mixer[NUM_TRACKS].peak_r);
    DrawPrettyText(hdc,vux+34,12,"MSTR",COL_TEXT_DIM,0);
    /* play LED */
    DrawLED(hdc,vux+64,14,6,g_playing,COL_ACCENT);
    DrawLED(hdc,vux+64,26,6,g_recording,RGB(255,40,40));
}

static void DrawSidebar(HDC hdc,RECT *rc){
    HBRUSH sb=CreateSolidBrush(COL_PANEL);
    FillRect(hdc,rc,sb); DeleteObject(sb);
    HPEN bp=CreatePen(PS_SOLID,1,COL_BORDER);
    HPEN obp=(HPEN)SelectObject(hdc,bp);
    MoveToEx(hdc,rc->right-1,rc->top,NULL); LineTo(hdc,rc->right-1,rc->bottom);
    SelectObject(hdc,obp); DeleteObject(bp);

    DrawPrettyText(hdc,rc->left+6,rc->top+6,"INSTRUMENTS",COL_ACCENT,1);

    for(int i=0;i<g_num_instr;i++){
        int ty=rc->top+24+i*24;
        RECT ir={rc->left+4,ty,rc->right-4,ty+20};
        COLORREF face=(i==g_sel_instr)?COL_SEL:COL_PANEL2;
        FillRoundRect(hdc,&ir,3,face);
        /* color swatch */
        RECT sw={rc->left+6,ty+4,rc->left+14,ty+16};
        FillRoundRect(hdc,&sw,2,g_instr[i].color);
        /* name */
        HFONT f=MakeFont(10,i==g_sel_instr);
        HFONT of=(HFONT)SelectObject(hdc,f);
        SetBkMode(hdc,TRANSPARENT);
        SetTextColor(hdc,i==g_sel_instr?COL_TEXT:COL_TEXT_DIM);
        TextOut(hdc,rc->left+18,ty+4,g_instr[i].name,(int)strlen(g_instr[i].name));
        SelectObject(hdc,of); DeleteObject(f);
        /* wave tag */
        const char *wn=WAVE_NAMES[g_instr[i].wave];
        HFONT wf=MakeFont(8,0);
        of=(HFONT)SelectObject(hdc,wf);
        SetTextColor(hdc,COL_TEXT_DIM);
        TextOut(hdc,rc->right-30,ty+5,wn,(int)strlen(wn));
        SelectObject(hdc,of); DeleteObject(wf);
    }
    /* step count chooser */
    int sy=rc->top+24+g_num_instr*24+10;
    DrawPrettyText(hdc,rc->left+6,sy,"STEPS",COL_ACCENT,1);
    sy+=16;
    int steps_opts[]={8,16,32};
    for(int i=0;i<3;i++){
        RECT sr={rc->left+4+i*56,sy,rc->left+54+i*56,sy+18};
        char st[4]; sprintf(st,"%d",steps_opts[i]);
        DrawButton3D(hdc,&sr,st,g_steps==steps_opts[i],
            g_steps==steps_opts[i]?COL_ACCENT:COL_PANEL2);
    }
    sy+=28;
    /* pattern selector */
    DrawPrettyText(hdc,rc->left+6,sy,"PATTERNS",COL_ACCENT,1);
    sy+=16;
    for(int i=0;i<g_num_patterns;i++){
        int px=(i%3)*56+rc->left+4;
        int py=sy+(i/3)*22;
        RECT pr={px,py,px+52,py+18};
        char pn[4]; sprintf(pn,"P%d",i+1);
        DrawButton3D(hdc,&pr,pn,g_cur_pattern==i,
            g_cur_pattern==i?COL_ACCENT:COL_PANEL2);
    }
    sy+=((g_num_patterns+2)/3)*22+8;
    /* add pattern */
    if(g_num_patterns<MAX_PATTERNS){
        RECT ap={rc->left+4,sy,rc->left+80,sy+18};
        DrawButton3D(hdc,&ap,"+ Pattern",0,RGB(40,60,40));
    }
    sy+=28;
    /* reverb/delay mix knobs */
    DrawPrettyText(hdc,rc->left+6,sy,"FX SEND",COL_ACCENT,1);
    sy+=18;
    DrawKnob(hdc,rc->left+30,sy+16,14,g_rev_mix,COL_ACCENT2,"REV");
    DrawKnob(hdc,rc->left+90,sy+16,14,g_dly_mix,COL_ACCENT,"DLY");
}

static void DrawSequencer(HDC hdc,RECT *rc){
    int x0=rc->left+8, y0=rc->top+8;
    int col_w=SEQCOL_W, row_h=SEQROW_H;
    int header_w=64;

    /* beat marker labels */
    for(int s=0;s<g_steps;s++){
        int sx=x0+header_w+s*col_w;
        RECT mr={sx,y0,sx+col_w,y0+14};
        char ms[4];
        if(s%4==0) sprintf(ms,"%d",s/4+1);
        else if(s%2==0) sprintf(ms,".");
        else ms[0]=0;
        if(ms[0]){
            HFONT f=MakeFont(9,s%4==0);
            HFONT of=(HFONT)SelectObject(hdc,f);
            SetBkMode(hdc,TRANSPARENT);
            SetTextColor(hdc,s%4==0?COL_TEXT:COL_TEXT_DIM);
            DrawText(hdc,ms,-1,&mr,DT_CENTER|DT_TOP);
            SelectObject(hdc,of); DeleteObject(f);
        }
    }
    y0+=16;

    for(int t=0;t<NUM_TRACKS;t++){
        int ty=y0+t*row_h;
        /* track header */
        RECT hr={x0,ty,x0+header_w-2,ty+row_h-2};
        FillRoundRect(hdc,&hr,4,
            g_mixer[t].mute ? RGB(40,40,44) : TRACK_COLS[t]);
        HFONT tf=MakeFont(10,1);
        HFONT otf=(HFONT)SelectObject(hdc,tf);
        SetBkMode(hdc,TRANSPARENT);
        SetTextColor(hdc,g_mixer[t].mute?COL_TEXT_DIM:RGB(255,255,255));
        DrawText(hdc,g_instr[t%g_num_instr].name,-1,&hr,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        SelectObject(hdc,otf); DeleteObject(tf);
        /* mute button */
        RECT mb={x0+header_w-20,ty+2,x0+header_w-4,ty+14};
        DrawButton3D(hdc,&mb,"M",g_mixer[t].mute,g_mixer[t].mute?RGB(80,20,20):COL_PANEL2);

        for(int s=0;s<g_steps;s++){
            int sx=x0+header_w+s*col_w;
            int on=g_patterns[g_cur_pattern].grid[t][s];
            int is_play=(g_playing && s==g_cur_step);
            RECT sr={sx+1,ty+1,sx+col_w-2,ty+row_h-3};

            COLORREF col;
            if(is_play && on)     col=COL_STEP_PLAY;
            else if(is_play)      col=RGB(0,80,100);
            else if(on)           col=(s%8<4)?COL_STEP_ON:COL_STEP_ON2;
            else                  col=(s%4==0)?RGB(45,45,53):COL_STEP_OFF;

            FillRoundRect(hdc,&sr,3,col);

            if(on){
                /* velocity bar */
                int vel=g_patterns[g_cur_pattern].vel[t][s];
                int vbh=(int)((row_h-6)*(vel/127.0));
                RECT vr={sx+2,ty+row_h-3-vbh,sx+5,ty+row_h-3};
                FillRoundRect(hdc,&vr,1,RGB(255,255,255));
            }
            /* beat grid lines */
            if(s%4==0 && s>0){
                HPEN gp=CreatePen(PS_SOLID,1,RGB(60,60,70));
                HPEN ogp=(HPEN)SelectObject(hdc,gp);
                MoveToEx(hdc,sx,ty,NULL); LineTo(hdc,sx,ty+row_h);
                SelectObject(hdc,ogp); DeleteObject(gp);
            }
        }
    }

    /* playhead */
    if(g_playing){
        int phx=x0+header_w+g_cur_step*col_w+col_w/2;
        HPEN php=CreatePen(PS_SOLID,2,COL_STEP_PLAY);
        HPEN ophp=(HPEN)SelectObject(hdc,php);
        MoveToEx(hdc,phx,rc->top,NULL);
        LineTo(hdc,phx,y0+NUM_TRACKS*row_h);
        SelectObject(hdc,ophp); DeleteObject(php);
    }

    /* instructions hint */
    DrawPrettyText(hdc,x0,y0+NUM_TRACKS*row_h+6,
        "Click steps to toggle. Right-click for velocity. Keyboard: Q-P / A-L plays notes.",
        COL_TEXT_DIM,0);
}

static void DrawMixer(HDC hdc,RECT *rc){
    int cx=rc->left+10;
    int channels=NUM_TRACKS+1; /* +master */
    DrawPrettyText(hdc,cx,rc->top+4,"MIXER",COL_ACCENT,1);
    int ch_y=rc->top+22;
    for(int ch=0;ch<channels;ch++){
        int is_master=(ch==NUM_TRACKS);
        COLORREF face=is_master?RGB(50,45,30):TRACK_COLS[ch%8];
        RECT chr={cx,ch_y,cx+MIXER_CH_W-2,rc->bottom-10};
        FillRoundRect(hdc,&chr,5,COL_PANEL2);
        StrokeRoundRect(hdc,&chr,5,is_master?COL_ACCENT:face,1);

        /* label */
        HFONT f=MakeFont(9,1);
        HFONT of=(HFONT)SelectObject(hdc,f);
        SetBkMode(hdc,TRANSPARENT);
        SetTextColor(hdc,is_master?COL_ACCENT:COL_TEXT);
        RECT lr={cx,ch_y+2,cx+MIXER_CH_W-2,ch_y+14};
        DrawText(hdc,is_master?"MST":g_mixer[ch].name,-1,&lr,DT_CENTER|DT_TOP);
        SelectObject(hdc,of); DeleteObject(f);

        int kcy=ch_y+28;
        /* volume knob */
        DrawKnob(hdc,cx+MIXER_CH_W/2,kcy,16,g_mixer[ch].volume,
            is_master?COL_ACCENT:face,"VOL");
        kcy+=42;
        /* pan knob */
        DrawKnob(hdc,cx+MIXER_CH_W/2,kcy,12,(g_mixer[ch].pan+1.0)*0.5,
            COL_ACCENT2,"PAN");
        kcy+=36;
        /* EQ knobs */
        DrawKnob(hdc,cx+MIXER_CH_W/2,kcy,10,(g_mixer[ch].eq_lo+1.0)*0.5,COL_ACCENT,"LO");
        kcy+=30;
        DrawKnob(hdc,cx+MIXER_CH_W/2,kcy,10,(g_mixer[ch].eq_mid+1.0)*0.5,COL_ACCENT,"MID");
        kcy+=30;
        DrawKnob(hdc,cx+MIXER_CH_W/2,kcy,10,(g_mixer[ch].eq_hi+1.0)*0.5,COL_ACCENT,"HI");
        kcy+=34;
        /* mute / solo */
        RECT mb={cx+2,kcy,cx+MIXER_CH_W/2-2,kcy+16};
        RECT sb2={cx+MIXER_CH_W/2,kcy,cx+MIXER_CH_W-4,kcy+16};
        DrawButton3D(hdc,&mb,"M",g_mixer[ch].mute,g_mixer[ch].mute?RGB(100,20,20):COL_PANEL2);
        DrawButton3D(hdc,&sb2,"S",g_mixer[ch].solo,g_mixer[ch].solo?RGB(100,80,0):COL_PANEL2);
        kcy+=22;
        /* VU meters */
        int vu_h=rc->bottom-10-kcy-4;
        if(vu_h>20){
            DrawVU(hdc,cx+4,      kcy,MIXER_CH_W/2-6,vu_h,g_mixer[ch].vu_l,g_mixer[ch].peak_l);
            DrawVU(hdc,cx+MIXER_CH_W/2,kcy,MIXER_CH_W/2-6,vu_h,g_mixer[ch].vu_r,g_mixer[ch].peak_r);
        }
        cx+=MIXER_CH_W+4;
    }
    /* master vol big knob */
    DrawKnob(hdc,rc->right-60,rc->top+60,28,g_master_vol,COL_ACCENT,"MASTER");
}

static void DrawInstrEditor(HDC hdc,RECT *rc){
    if(g_sel_instr>=g_num_instr) return;
    Instrument *ins=&g_instr[g_sel_instr];

    DrawPrettyText(hdc,rc->left+8,rc->top+4,"INSTRUMENT EDITOR",COL_ACCENT,1);

    /* instrument name */
    RECT nr={rc->left+8,rc->top+20,rc->left+200,rc->top+36};
    FillRoundRect(hdc,&nr,3,RGB(15,15,18));
    StrokeRoundRect(hdc,&nr,3,COL_ACCENT,1);
    HFONT nf=MakeFont(12,1);
    HFONT onf=(HFONT)SelectObject(hdc,nf);
    SetBkMode(hdc,TRANSPARENT);
    SetTextColor(hdc,ins->color);
    DrawText(hdc,ins->name,-1,(RECT*)&nr,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    SelectObject(hdc,onf); DeleteObject(nf);

    int row1y=rc->top+48;
    /* waveform selector */
    DrawPrettyText(hdc,rc->left+8,row1y,"WAVEFORM",COL_TEXT_DIM,0);
    for(int w=0;w<WAVE_COUNT;w++){
        RECT wr={rc->left+8+w*68,row1y+14,rc->left+70+w*68,row1y+30};
        DrawButton3D(hdc,&wr,WAVE_NAMES[w],ins->wave==w,
            ins->wave==w?ins->color:COL_PANEL2);
    }
    row1y+=44;

    /* ADSR section */
    DrawPrettyText(hdc,rc->left+8,row1y,"ENVELOPE  (ADSR)",COL_TEXT_DIM,0);
    int kx=rc->left+30;
    DrawKnob(hdc,kx,   row1y+32,18,ins->env.a,         COL_ACCENT2,"ATK"); kx+=52;
    DrawKnob(hdc,kx,   row1y+32,18,ins->env.d,         COL_ACCENT2,"DEC"); kx+=52;
    DrawKnob(hdc,kx,   row1y+32,18,ins->env.s,         COL_ACCENT2,"SUS"); kx+=52;
    DrawKnob(hdc,kx,   row1y+32,18,ins->env.r,         COL_ACCENT2,"REL"); kx+=68;
    /* volume / pan */
    DrawKnob(hdc,kx,   row1y+32,18,ins->volume,        ins->color,  "VOL"); kx+=52;
    DrawKnob(hdc,kx,   row1y+32,18,(ins->pan+1.0)*0.5, COL_ACCENT2,"PAN"); kx+=52;
    DrawKnob(hdc,kx,   row1y+32,18,(ins->detune+100)/200.0,COL_ACCENT,"DET"); kx+=52;
    if(ins->wave==WAVE_PULSE)
        DrawKnob(hdc,kx,row1y+32,18,ins->pw,           COL_ACCENT,"PW");
    row1y+=72;

    /* filter section */
    DrawPrettyText(hdc,rc->left+8,row1y,"FILTER",COL_TEXT_DIM,0);
    RECT fen={rc->left+65,row1y-1,rc->left+105,row1y+13};
    DrawButton3D(hdc,&fen,ins->filter_on?"ON":"OFF",ins->filter_on,
        ins->filter_on?COL_ACCENT:COL_PANEL2);
    kx=rc->left+30;
    double cut_norm=log10(fmax(20.0,ins->filter_cutoff)/20.0)/log10(20000.0/20.0);
    DrawKnob(hdc,kx,row1y+28,18,cut_norm, COL_ACCENT,"CUT"); kx+=52;
    DrawKnob(hdc,kx,row1y+28,18,ins->filter_res/4.0,COL_ACCENT,"RES"); kx+=68;
    /* octave */
    DrawPrettyText(hdc,kx-12,row1y,"OCT",COL_TEXT_DIM,0);
    char ocstr[8]; sprintf(ocstr,"%+d",ins->octave);
    RECT ocr={kx-12,row1y+14,kx+32,row1y+30};
    FillRoundRect(hdc,&ocr,3,RGB(15,15,18));
    HFONT of2=MakeFont(12,1);
    HFONT oof2=(HFONT)SelectObject(hdc,of2);
    SetBkMode(hdc,TRANSPARENT);
    SetTextColor(hdc,COL_TEXT);
    DrawText(hdc,ocstr,-1,(RECT*)&ocr,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    SelectObject(hdc,oof2); DeleteObject(of2);
    kx+=52;
    RECT ou={kx,row1y+14,kx+20,row1y+22}; DrawButton3D(hdc,&ou,"+",0,COL_PANEL2);
    RECT od={kx,row1y+22,kx+20,row1y+30}; DrawButton3D(hdc,&od,"-",0,COL_PANEL2);

    /* mini envelope graph */
    int ex=rc->left+380, ey=row1y-2, ew=180, eh=60;
    RECT egr={ex,ey,ex+ew,ey+eh};
    FillRoundRect(hdc,&egr,4,RGB(10,10,12));
    StrokeRoundRect(hdc,&egr,4,RGB(60,60,70),1);
    /* draw ADSR shape */
    double pw2=0.15, dw=0.2, sw=0.35, rw=0.3;
    HPEN ep=CreatePen(PS_SOLID,2,ins->color);
    HPEN oep=(HPEN)SelectObject(hdc,ep);
    int ox2=ex+2,oy2=ey+eh-2;
    int ax=(int)(ex+pw2*ew), ay=ey+2;
    int dxx=(int)(ex+(pw2+dw)*ew), dy=(int)(ey+2+(1.0-ins->env.s)*(eh-4));
    int sx2=(int)(ex+(pw2+dw+sw)*ew);
    int rxx=(int)(ex+(pw2+dw+sw+rw)*ew);
    MoveToEx(hdc,ox2,oy2,NULL);
    LineTo(hdc,ax,ay);
    LineTo(hdc,dxx,dy);
    LineTo(hdc,sx2,dy);
    LineTo(hdc,rxx,oy2);
    SelectObject(hdc,oep); DeleteObject(ep);
    DrawPrettyText(hdc,ex+2,ey+2,"ENV",RGB(80,80,90),0);
}

static void DrawSongEditor(HDC hdc,RECT *rc){
    DrawPrettyText(hdc,rc->left+8,rc->top+4,"SONG EDITOR",COL_ACCENT,1);
    DrawPrettyText(hdc,rc->left+8,rc->top+20,
        "Arrange patterns into a song. Click a cell, then press a number key to assign.",
        COL_TEXT_DIM,0);
    int bw=52, bh=28;
    int x0=rc->left+8, y0=rc->top+40;
    for(int b=0;b<g_song_len;b++){
        int bx=x0+b*(bw+4);
        RECT br={bx,y0,bx+bw,y0+bh};
        int pidx=g_song[b];
        char lbl[16]; sprintf(lbl,"P%d",pidx+1);
        COLORREF face=g_song_pos==b?COL_ACCENT:TRACK_COLS[pidx%8];
        FillRoundRect(hdc,&br,4,g_song_pos==b?COL_ACCENT:face);
        HFONT f=MakeFont(11,1);
        HFONT of=(HFONT)SelectObject(hdc,f);
        SetBkMode(hdc,TRANSPARENT);
        SetTextColor(hdc,RGB(255,255,255));
        DrawText(hdc,lbl,-1,(RECT*)&br,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        SelectObject(hdc,of); DeleteObject(f);
        /* bar number */
        char bn[4]; sprintf(bn,"%d",b+1);
        DrawPrettyText(hdc,bx+bw/2-4,y0+bh+2,bn,COL_TEXT_DIM,0);
    }
    /* add/remove bar */
    RECT ab={x0+g_song_len*(bw+4),y0,x0+g_song_len*(bw+4)+bw,y0+bh};
    DrawButton3D(hdc,&ab,"+Bar",0,RGB(40,60,40));
    if(g_song_len>1){
        RECT rb={x0+g_song_len*(bw+4)+bw+4,y0,x0+g_song_len*(bw+4)+bw*2+4,y0+bh};
        DrawButton3D(hdc,&rb,"-Bar",0,RGB(60,30,30));
    }

    /* pattern legend */
    int legy=y0+50;
    DrawPrettyText(hdc,x0,legy,"Patterns:",COL_TEXT_DIM,0);
    for(int p=0;p<g_num_patterns;p++){
        RECT pr={x0+70+p*60,legy-2,x0+124+p*60,legy+16};
        char pn[8]; sprintf(pn,"P%d",p+1);
        FillRoundRect(hdc,&pr,3,TRACK_COLS[p%8]);
        HFONT f=MakeFont(10,1);
        HFONT of=(HFONT)SelectObject(hdc,f);
        SetBkMode(hdc,TRANSPARENT);
        SetTextColor(hdc,RGB(255,255,255));
        DrawText(hdc,pn,-1,(RECT*)&pr,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        SelectObject(hdc,of); DeleteObject(f);
    }
}

static void DrawPiano(HDC hdc,RECT *rc){
    /* background */
    HBRUSH pb=CreateSolidBrush(RGB(20,20,24));
    FillRect(hdc,rc,pb); DeleteObject(pb);
    HPEN tp=CreatePen(PS_SOLID,1,COL_BORDER);
    HPEN otp=(HPEN)SelectObject(hdc,tp);
    MoveToEx(hdc,rc->left,rc->top,NULL); LineTo(hdc,rc->right,rc->top);
    SelectObject(hdc,otp); DeleteObject(tp);

    /* label */
    DrawPrettyText(hdc,rc->left+4,rc->top+4,"PIANO  (Q-P / A-L keys, Z/X = Oct)",COL_TEXT_DIM,0);

    int key_w=16, white_h=PIANO_H-20, black_h=(int)(white_h*0.6);
    int start_note=g_piano_oct*12; /* C of chosen octave */
    int num_whites=14; /* 2 octaves white keys */
    int px=rc->left+80;

    /* octave display */
    RECT ocbx={rc->left+4,rc->top+18,rc->left+76,rc->top+36};
    FillRoundRect(hdc,&ocbx,3,RGB(10,10,12));
    char ocstr[16]; sprintf(ocstr,"Oct %d",g_piano_oct);
    HFONT of3=MakeFont(11,0);
    HFONT oof3=(HFONT)SelectObject(hdc,of3);
    SetBkMode(hdc,TRANSPARENT);
    SetTextColor(hdc,COL_ACCENT2);
    DrawText(hdc,ocstr,-1,(RECT*)&ocbx,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
    SelectObject(hdc,oof3); DeleteObject(of3);

    /* draw 2 octaves */
    static const int is_black[12]={0,1,0,1,0,0,1,0,1,0,1,0};
    static const char *key_labels[14]={"Q","2","W","3","E","R","5","T","6","Y","7","U",
                                        "A","S"};
    /* white keys first */
    int wi=0;
    for(int k=0;k<24;k++){
        int note=start_note+k;
        if(!is_black[k%12]){
            int kx=px+wi*key_w;
            RECT kr={kx,rc->top+20,kx+key_w-1,rc->top+20+white_h};
            int held=g_keys_held[note%256];
            FillRoundRect(hdc,&kr,2,held?COL_PIANO_HI:COL_PIANO_W);
            HPEN kp=CreatePen(PS_SOLID,1,RGB(140,130,120));
            HPEN okp=(HPEN)SelectObject(hdc,kp);
            Rectangle(hdc,kx,rc->top+20,kx+key_w,rc->top+20+white_h);
            SelectObject(hdc,okp); DeleteObject(kp);
            /* note name at bottom */
            if(k%12==0){
                HFONT f=MakeFont(8,0);
                HFONT of=(HFONT)SelectObject(hdc,f);
                SetBkMode(hdc,TRANSPARENT);
                SetTextColor(hdc,held?COL_PIANO_HI:RGB(80,70,60));
                char nn[4]; sprintf(nn,"C%d",note/12-1);
                TextOut(hdc,kx+1,rc->top+20+white_h-11,nn,(int)strlen(nn));
                SelectObject(hdc,of); DeleteObject(f);
            }
            wi++;
        }
    }
    /* black keys */
    wi=0;
    int boff[]={1,3,-1,6,8,10,-1};
    for(int k=0;k<24;k++){
        int note=start_note+k;
        if(!is_black[k%12]){wi++; continue;}
        /* find the white key position before this black */
        int wbefore=0;
        for(int j=0;j<k;j++) if(!is_black[j%12]) wbefore++;
        int kx=px+wbefore*key_w - key_w/3;
        RECT kr={kx,rc->top+20,kx+key_w*2/3,rc->top+20+black_h};
        int held=g_keys_held[note%256];
        FillRoundRect(hdc,&kr,2,held?COL_PIANO_HI:COL_PIANO_B);
    }
}

static void DrawStatusBar(HDC hdc,RECT *rc){
    HBRUSH sb=CreateSolidBrush(COL_PANEL);
    FillRect(hdc,rc,sb); DeleteObject(sb);
    HPEN tp=CreatePen(PS_SOLID,1,COL_BORDER);
    HPEN otp=(HPEN)SelectObject(hdc,tp);
    MoveToEx(hdc,rc->left,rc->top,NULL); LineTo(hdc,rc->right,rc->top);
    SelectObject(hdc,otp); DeleteObject(tp);
    HFONT f=MakeFont(10,0);
    HFONT of=(HFONT)SelectObject(hdc,f);
    SetBkMode(hdc,TRANSPARENT);
    SetTextColor(hdc,COL_TEXT_DIM);
    TextOut(hdc,rc->left+6,rc->top+4,g_status,(int)strlen(g_status));
    /* right-side info */
    char info[64];
    sprintf(info,"Step: %02d/%02d  |  Pattern: %d  |  %s",
        g_cur_step+1,g_steps,g_cur_pattern+1,g_dirty?"[unsaved]":"[saved]");
    HFONT f2=MakeFont(10,0);
    HFONT of2=(HFONT)SelectObject(hdc,f2);
    SetTextColor(hdc,g_dirty?COL_ACCENT:COL_TEXT_DIM);
    RECT ir={rc->left,rc->top+4,rc->right-6,rc->top+18};
    DrawText(hdc,info,-1,&ir,DT_RIGHT|DT_TOP);
    SelectObject(hdc,of2); DeleteObject(f2);
    SelectObject(hdc,of); DeleteObject(f);
}

/* ============================================================
   HIT TESTING
   ============================================================ */
static int HitTestSeqGrid(int mx,int my,int *track,int *step){
    int x0=SIDEBAR_W+8+64, y0=TOOLBAR_H+8+16;
    int row_h=SEQROW_H, col_w=SEQCOL_W;
    if(mx<x0 || my<y0) return 0;
    int t=(my-y0)/row_h;
    int s=(mx-x0)/col_w;
    if(t<0||t>=NUM_TRACKS||s<0||s>=g_steps) return 0;
    *track=t; *step=s;
    return 1;
}
static int HitTestSidebarInstr(int mx,int my){
    int y0=TOOLBAR_H+24;
    for(int i=0;i<g_num_instr;i++){
        int ty=y0+i*24;
        if(mx>=4&&mx<=SIDEBAR_W-4&&my>=ty&&my<ty+20) return i;
    }
    return -1;
}
static int HitTestSidebarSteps(int mx,int my){
    int sy=TOOLBAR_H+24+g_num_instr*24+10+16;
    int steps_opts[]={8,16,32};
    for(int i=0;i<3;i++){
        int x1=4+i*56, x2=54+i*56;
        if(mx>=x1&&mx<=x2&&my>=sy&&my<sy+18) return steps_opts[i];
    }
    return -1;
}
static int HitTestSidebarPattern(int mx,int my){
    int sy=TOOLBAR_H+24+g_num_instr*24+10+16+28+16;
    for(int i=0;i<g_num_patterns;i++){
        int px=(i%3)*56+4;
        int py=sy+(i/3)*22;
        if(mx>=px&&mx<=px+52&&my>=py&&my<py+18) return i;
    }
    return -1;
}
static int HitTestToolbar(int mx,int my,int *id){
    if(my<0||my>=TOOLBAR_H) return 0;
    int bx=200,by=5,bw=50,bh=28;
    /* play */
    if(mx>=bx&&mx<bx+bw&&my>=by&&my<by+bh){*id=ID_BTN_PLAY;return 1;} bx+=bw+4;
    /* stop */
    if(mx>=bx&&mx<bx+bw&&my>=by&&my<by+bh){*id=ID_BTN_STOP;return 1;} bx+=bw+4;
    /* rec */
    if(mx>=bx&&mx<bx+bw&&my>=by&&my<by+bh){*id=ID_BTN_REC;return 1;} bx+=bw+12;
    /* bpm up/dn */
    if(mx>=bx+92&&mx<bx+108&&my>=by&&my<by+13){*id=ID_BTN_BPM_UP;return 1;}
    if(mx>=bx+92&&mx<bx+108&&my>=by+15&&my<by+bh){*id=ID_BTN_BPM_DN;return 1;}
    bx+=114+78;
    /* view buttons */
    for(int i=0;i<4;i++){
        if(mx>=bx&&mx<bx+52&&my>=by&&my<by+bh){*id=ID_BTN_SEQ+i;return 1;}
        bx+=56;
    }
    bx+=8;
    /* save/load/export */
    if(mx>=bx&&mx<bx+44&&my>=by&&my<by+bh){*id=ID_BTN_SAVE;return 1;} bx+=48;
    if(mx>=bx&&mx<bx+44&&my>=by&&my<by+bh){*id=ID_BTN_LOAD;return 1;} bx+=48;
    if(mx>=bx&&mx<bx+56&&my>=by&&my<by+bh){*id=ID_BTN_EXPORT;return 1;}
    return 0;
}

/* ============================================================
   KEYBOARD → NOTE MAPPING
   ============================================================ */
static int KeyToNote(int vk){
    /* Q-P = C4..E5, A-L = C3..G#3 */
    static const int qrow[]={60,62,64,65,67,69,71,72,74,76}; /* Q=60,W=62 etc */
    static const int arow[]={48,50,52,53,55,57,59,60,62};
    /* black keys via number row */
    static const int numrow[]={61,63,0,66,68,70,0,73,75};
    int oct_shift=(g_piano_oct-4)*12;
    switch(vk){
        case 'Q': return qrow[0]+oct_shift;
        case 'W': return qrow[1]+oct_shift;
        case 'E': return qrow[2]+oct_shift;
        case 'R': return qrow[3]+oct_shift;
        case 'T': return qrow[4]+oct_shift;
        case 'Y': return qrow[5]+oct_shift;
        case 'U': return qrow[6]+oct_shift;
        case 'I': return qrow[7]+oct_shift;
        case 'O': return qrow[8]+oct_shift;
        case 'P': return qrow[9]+oct_shift;
        case 'A': return arow[0]+oct_shift;
        case 'S': return arow[1]+oct_shift;
        case 'D': return arow[2]+oct_shift;
        case 'F': return arow[3]+oct_shift;
        case 'G': return arow[4]+oct_shift;
        case 'H': return arow[5]+oct_shift;
        case 'J': return arow[6]+oct_shift;
        case 'K': return arow[7]+oct_shift;
        case 'L': return arow[8]+oct_shift;
        case '2': return numrow[0]+oct_shift;
        case '3': return numrow[1]+oct_shift;
        case '5': return numrow[3]+oct_shift;
        case '6': return numrow[4]+oct_shift;
        case '7': return numrow[5]+oct_shift;
        case '9': return numrow[7]+oct_shift;
        case '0': return numrow[8]+oct_shift;
    }
    return -1;
}

/* ============================================================
   WINDOW PROCEDURE
   ============================================================ */
LRESULT CALLBACK WndProc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
    static int lmb_down=0, rmb_down=0;
    switch(msg){
    case WM_CREATE:
        SetTimer(hwnd,ID_TIMER_UI,16,NULL);   /* ~60fps */
        SetTimer(hwnd,ID_TIMER_BLINK,500,NULL);
        break;
    case WM_DESTROY:
        KillTimer(hwnd,ID_TIMER_UI);
        KillTimer(hwnd,ID_TIMER_BLINK);
        AudioStop();
        DeleteCriticalSection(&g_lock);
        PostQuitMessage(0);
        break;
    case WM_TIMER:
        if(wp==ID_TIMER_BLINK) g_blink=!g_blink;
        /* decay VU */
        for(int i=0;i<=NUM_TRACKS;i++){
            g_mixer[i].vu_l*=0.92; g_mixer[i].vu_r*=0.92;
            if(g_mixer[i].peak_hold_l>0) g_mixer[i].peak_hold_l--;
            else g_mixer[i].peak_l*=0.98;
            if(g_mixer[i].peak_hold_r>0) g_mixer[i].peak_hold_r--;
            else g_mixer[i].peak_r*=0.98;
        }
        InvalidateRect(hwnd,NULL,FALSE);
        break;
    case WM_PAINT:{
        PAINTSTRUCT ps;
        HDC hdc=BeginPaint(hwnd,&ps);
        RECT rc; GetClientRect(hwnd,&rc);
        /* double-buffer */
        HDC mdc=CreateCompatibleDC(hdc);
        HBITMAP bmp=CreateCompatibleBitmap(hdc,rc.right,rc.bottom);
        HBITMAP obmp=(HBITMAP)SelectObject(mdc,bmp);
        DrawAll(mdc,&rc);
        BitBlt(hdc,0,0,rc.right,rc.bottom,mdc,0,0,SRCCOPY);
        SelectObject(mdc,obmp); DeleteObject(bmp); DeleteDC(mdc);
        EndPaint(hwnd,&ps);
        break;
    }
    case WM_LBUTTONDOWN:{
        lmb_down=1;
        SetCapture(hwnd);
        int mx=LOWORD(lp), my=HIWORD(lp);
        int id=0;
        if(HitTestToolbar(mx,my,&id)){
            switch(id){
                case ID_BTN_PLAY:
                    g_playing=!g_playing;
                    if(g_playing){g_cur_step=0;g_step_accum=0;}
                    SetStatus(g_playing?"Playing...":"Stopped.");
                    break;
                case ID_BTN_STOP:
                    g_playing=0; g_cur_step=0; g_step_accum=0;
                    SetStatus("Stopped.");
                    /* note-off all */
                    for(int i=0;i<MAX_POLY;i++) g_voices[i].active=0;
                    break;
                case ID_BTN_REC:
                    g_recording=!g_recording;
                    SetStatus(g_recording?"Recording!":"Recording off.");
                    break;
                case ID_BTN_BPM_UP: g_bpm=fmin(300.0,g_bpm+1.0); SetStatus("BPM: %.1f",g_bpm); break;
                case ID_BTN_BPM_DN: g_bpm=fmax(40.0, g_bpm-1.0); SetStatus("BPM: %.1f",g_bpm); break;
                case ID_BTN_SEQ:   g_view=VIEW_SEQ;   break;
                case ID_BTN_MIXER: g_view=VIEW_MIXER; break;
                case ID_BTN_INSTR: g_view=VIEW_INSTR; break;
                case ID_BTN_SONG:  g_view=VIEW_SONG;  break;
                case ID_BTN_SAVE:{
                    OPENFILENAME ofn={0}; char path[MAX_PATH]="untitled.mm11";
                    ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=hwnd;
                    ofn.lpstrFilter="Music Maker 11 (*.mm11)\0*.mm11\0All\0*.*\0";
                    ofn.lpstrFile=path; ofn.nMaxFile=MAX_PATH;
                    ofn.lpstrDefExt="mm11";
                    ofn.Flags=OFN_OVERWRITEPROMPT;
                    if(GetSaveFileName(&ofn)) SaveProject(path);
                    break;
                }
                case ID_BTN_LOAD:{
                    OPENFILENAME ofn={0}; char path[MAX_PATH]="";
                    ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=hwnd;
                    ofn.lpstrFilter="Music Maker 11 (*.mm11)\0*.mm11\0All\0*.*\0";
                    ofn.lpstrFile=path; ofn.nMaxFile=MAX_PATH;
                    ofn.Flags=OFN_FILEMUSTEXIST;
                    if(GetOpenFileName(&ofn)) LoadProject(path);
                    break;
                }
                case ID_BTN_EXPORT:{
                    OPENFILENAME ofn={0}; char path[MAX_PATH]="export.wav";
                    ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=hwnd;
                    ofn.lpstrFilter="WAV Audio (*.wav)\0*.wav\0All\0*.*\0";
                    ofn.lpstrFile=path; ofn.nMaxFile=MAX_PATH;
                    ofn.lpstrDefExt="wav";
                    ofn.Flags=OFN_OVERWRITEPROMPT;
                    if(GetSaveFileName(&ofn)) ExportWAV(path);
                    break;
                }
            }
        } else if(mx<SIDEBAR_W){
            /* sidebar clicks */
            int instr=HitTestSidebarInstr(mx,my);
            if(instr>=0){ g_sel_instr=instr; SetStatus("Selected: %s",g_instr[instr].name); }
            int steps=HitTestSidebarSteps(mx,my);
            if(steps>0){ g_steps=steps; SetStatus("Steps: %d",steps); }
            int pat=HitTestSidebarPattern(mx,my);
            if(pat>=0){ g_cur_pattern=pat; SetStatus("Pattern: %d",pat+1); }
            /* add pattern */
            int sy=TOOLBAR_H+24+g_num_instr*24+10+16+28+16;
            sy+=((g_num_patterns+2)/3)*22+8;
            if(mx>=4&&mx<=84&&my>=sy&&my<sy+18&&g_num_patterns<MAX_PATTERNS){
                g_num_patterns++; g_dirty=1;
                SetStatus("Added Pattern %d",g_num_patterns);
            }
        } else if(g_view==VIEW_SEQ){
            int t,s;
            if(HitTestSeqGrid(mx,my,&t,&s)){
                g_patterns[g_cur_pattern].grid[t][s] ^= 1;
                g_drag_track=t; g_drag_step=s;
                g_drag_val=g_patterns[g_cur_pattern].grid[t][s];
                g_dirty=1;
                if(g_patterns[g_cur_pattern].grid[t][s])
                    NoteOn(t,g_patterns[g_cur_pattern].note[t][s],
                           g_patterns[g_cur_pattern].vel[t][s]);
                SetStatus("Track %d, Step %d: %s",t+1,s+1,
                    g_patterns[g_cur_pattern].grid[t][s]?"ON":"OFF");
            }
            /* mute button hit */
            int y0=TOOLBAR_H+8+16;
            int x0s=SIDEBAR_W+8;
            for(int t2=0;t2<NUM_TRACKS;t2++){
                int ty=y0+t2*SEQROW_H;
                if(mx>=x0s+44&&mx<=x0s+60&&my>=ty+2&&my<=ty+14){
                    g_mixer[t2].mute^=1;
                    SetStatus("Track %d: %s",t2+1,g_mixer[t2].mute?"MUTED":"unmuted");
                }
            }
        } else if(g_view==VIEW_MIXER){
            /* mixer knob interaction handled via WM_MOUSEMOVE */
        } else if(g_view==VIEW_SONG){
            /* song bar click */
            int x0=SIDEBAR_W+8, y0=TOOLBAR_H+40;
            int bw=52+4;
            if(my>=y0&&my<y0+28){
                int b=(mx-x0)/bw;
                if(b>=0&&b<g_song_len){ g_song_pos=b; SetStatus("Bar %d: Pattern %d",b+1,g_song[b]+1); }
                /* add bar */
                if(mx>=x0+g_song_len*bw&&mx<x0+g_song_len*bw+52&&g_song_len<64){
                    g_song_len++; g_dirty=1; SetStatus("Added bar %d",g_song_len);
                }
                /* remove bar */
                if(mx>=x0+(g_song_len)*bw+56&&mx<x0+(g_song_len)*bw+56+52&&g_song_len>1){
                    g_song_len--; if(g_song_pos>=g_song_len) g_song_pos=g_song_len-1; g_dirty=1;
                }
            }
        } else if(g_view==VIEW_INSTR){
            Instrument *ins=&g_instr[g_sel_instr];
            int row1y=TOOLBAR_H+48+44;
            /* wave selector */
            int wy=TOOLBAR_H+48;
            for(int w=0;w<WAVE_COUNT;w++){
                int wx=SIDEBAR_W+8+w*68, wx2=wx+62;
                if(mx>=wx&&mx<wx2&&my>=wy+14&&my<wy+30){
                    ins->wave=w; g_dirty=1;
                    SetStatus("Wave: %s",WAVE_NAMES[w]);
                }
            }
            /* filter toggle */
            if(mx>=SIDEBAR_W+65&&mx<SIDEBAR_W+105&&my>=row1y-1&&my<row1y+13){
                ins->filter_on^=1; g_dirty=1;
                SetStatus("Filter: %s",ins->filter_on?"ON":"OFF");
            }
            /* octave up/down */
            int kx=SIDEBAR_W+8+52*4+68+52*3+52;
            if(mx>=kx&&mx<kx+20&&my>=row1y+14&&my<row1y+22){ins->octave++;if(ins->octave>4)ins->octave=4;g_dirty=1;}
            if(mx>=kx&&mx<kx+20&&my>=row1y+22&&my<row1y+30){ins->octave--;if(ins->octave<-4)ins->octave=-4;g_dirty=1;}
        }
        InvalidateRect(hwnd,NULL,FALSE);
        break;
    }
    case WM_MOUSEMOVE:{
        int mx=LOWORD(lp), my=HIWORD(lp);
        if(lmb_down && g_view==VIEW_SEQ && g_drag_step>=0){
            int t,s;
            if(HitTestSeqGrid(mx,my,&t,&s)){
                if(t!=g_drag_track||s!=g_drag_step){
                    g_patterns[g_cur_pattern].grid[t][s]=g_drag_val;
                    g_drag_track=t; g_drag_step=s;
                    g_dirty=1;
                }
            }
        }
        break;
    }
    case WM_LBUTTONUP:
        lmb_down=0; g_drag_step=-1; g_drag_track=-1;
        ReleaseCapture();
        break;
    case WM_RBUTTONDOWN:{
        int mx=LOWORD(lp), my=HIWORD(lp);
        if(g_view==VIEW_SEQ){
            int t,s;
            if(HitTestSeqGrid(mx,my,&t,&s)){
                /* right-click: cycle velocity */
                if(g_patterns[g_cur_pattern].grid[t][s]){
                    int *vel=&g_patterns[g_cur_pattern].vel[t][s];
                    *vel = (*vel<50)?50:(*vel<100)?100:(*vel<127)?127:30;
                    SetStatus("Track %d, Step %d: velocity %d",t+1,s+1,*vel);
                    g_dirty=1;
                }
                /* also cycle note on alt+right */
                if(GetKeyState(VK_MENU)&0x8000){
                    int *note=&g_patterns[g_cur_pattern].note[t][s];
                    *note = ((*note-48+1)%24)+48;
                    SetStatus("Note: %s",NoteName(*note));
                    g_dirty=1;
                }
            }
        }
        InvalidateRect(hwnd,NULL,FALSE);
        break;
    }
    case WM_MOUSEWHEEL:{
        int delta=GET_WHEEL_DELTA_WPARAM(wp);
        int mx=LOWORD(lp), my=HIWORD(lp);
        POINT pt={mx,my}; ScreenToClient(hwnd,&pt);
        if(pt.x<SIDEBAR_W){
            /* scroll pattern/instrument list */
        } else {
            if(g_view==VIEW_SEQ){
                g_bpm=fmax(40,fmin(300,g_bpm+(delta>0?1:-1)));
                SetStatus("BPM: %.1f",g_bpm);
            }
        }
        InvalidateRect(hwnd,NULL,FALSE);
        break;
    }
    case WM_KEYDOWN:{
        int note=KeyToNote((int)wp);
        if(note>=0 && !g_keys_held[note%256]){
            g_keys_held[note%256]=1;
            NoteOn(g_sel_instr,note,100);
            SetStatus("Note: %s  (%d)",NoteName(note),note);
            if(g_recording && g_playing){
                /* stamp note into pattern at current step */
                g_patterns[g_cur_pattern].grid[g_sel_track][g_cur_step]=1;
                g_patterns[g_cur_pattern].note[g_sel_track][g_cur_step]=note;
                g_dirty=1;
            }
        }
        switch(wp){
            case VK_SPACE:
                g_playing=!g_playing;
                if(g_playing){g_cur_step=0;g_step_accum=0;}
                SetStatus(g_playing?"Playing...":"Stopped.");
                break;
            case 'Z': g_piano_oct=max(0,g_piano_oct-1); SetStatus("Octave: %d",g_piano_oct); break;
            case 'X': g_piano_oct=min(8,g_piano_oct+1); SetStatus("Octave: %d",g_piano_oct); break;
            case VK_F1: g_view=VIEW_SEQ;   break;
            case VK_F2: g_view=VIEW_MIXER; break;
            case VK_F3: g_view=VIEW_INSTR; break;
            case VK_F4: g_view=VIEW_SONG;  break;
            case VK_UP:   g_bpm=fmin(300,g_bpm+1); SetStatus("BPM: %.1f",g_bpm); break;
            case VK_DOWN: g_bpm=fmax(40, g_bpm-1); SetStatus("BPM: %.1f",g_bpm); break;
            case VK_LEFT:
                g_cur_pattern=max(0,g_cur_pattern-1);
                SetStatus("Pattern: %d",g_cur_pattern+1);
                break;
            case VK_RIGHT:
                g_cur_pattern=min(g_num_patterns-1,g_cur_pattern+1);
                SetStatus("Pattern: %d",g_cur_pattern+1);
                break;
            case VK_TAB:
                g_sel_instr=(g_sel_instr+1)%g_num_instr;
                SetStatus("Instrument: %s",g_instr[g_sel_instr].name);
                break;
        }
        InvalidateRect(hwnd,NULL,FALSE);
        break;
    }
    case WM_KEYUP:{
        int note=KeyToNote((int)wp);
        if(note>=0 && g_keys_held[note%256]){
            g_keys_held[note%256]=0;
            NoteOff(note);
        }
        break;
    }
    case WM_COMMAND:
        switch(LOWORD(wp)){
            case ID_MENU_NEW:
                if(g_dirty && MessageBox(hwnd,"Unsaved changes. New project?",APP_NAME,
                    MB_YESNO|MB_ICONQUESTION)==IDNO) break;
                InitProject(); g_dirty=0;
                InvalidateRect(hwnd,NULL,FALSE);
                break;
            case ID_MENU_SAVE:{
                OPENFILENAME ofn={0}; char path[MAX_PATH]="untitled.mm11";
                ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=hwnd;
                ofn.lpstrFilter="Music Maker 11 (*.mm11)\0*.mm11\0All\0*.*\0";
                ofn.lpstrFile=path; ofn.nMaxFile=MAX_PATH;
                ofn.lpstrDefExt="mm11"; ofn.Flags=OFN_OVERWRITEPROMPT;
                if(GetSaveFileName(&ofn)) SaveProject(path);
                break;
            }
            case ID_MENU_LOAD:{
                OPENFILENAME ofn={0}; char path[MAX_PATH]="";
                ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=hwnd;
                ofn.lpstrFilter="Music Maker 11 (*.mm11)\0*.mm11\0All\0*.*\0";
                ofn.lpstrFile=path; ofn.nMaxFile=MAX_PATH;
                ofn.Flags=OFN_FILEMUSTEXIST;
                if(GetOpenFileName(&ofn)) LoadProject(path);
                break;
            }
            case ID_MENU_EXPORT:{
                OPENFILENAME ofn={0}; char path[MAX_PATH]="export.wav";
                ofn.lStructSize=sizeof(ofn); ofn.hwndOwner=hwnd;
                ofn.lpstrFilter="WAV Audio (*.wav)\0*.wav\0All\0*.*\0";
                ofn.lpstrFile=path; ofn.nMaxFile=MAX_PATH;
                ofn.lpstrDefExt="wav"; ofn.Flags=OFN_OVERWRITEPROMPT;
                if(GetSaveFileName(&ofn)) ExportWAV(path);
                break;
            }
            case ID_MENU_EXIT:
                if(g_dirty && MessageBox(hwnd,"Unsaved changes. Exit?",APP_NAME,
                    MB_YESNO|MB_ICONQUESTION)==IDNO) break;
                DestroyWindow(hwnd);
                break;
            case ID_MENU_UNDO:
                SetStatus("Undo (not implemented in this build)");
                break;
            case ID_MENU_CLEAR:
                memset(g_patterns[g_cur_pattern].grid,0,sizeof(g_patterns[g_cur_pattern].grid));
                g_dirty=1; SetStatus("Pattern cleared.");
                InvalidateRect(hwnd,NULL,FALSE);
                break;
            case ID_MENU_ABOUT:
                MessageBox(hwnd,
                    "Mirko Stevanovski MUSIC MAKER 11\n"
                    "Version 11.0.0  Build 2314\n\n"
                    "(c) 2011 Mirko Stevanovski Software Inc.\n"
                    "All Rights Reserved.\n\n"
                    "KEYBOARD SHORTCUTS:\n"
                    "  SPACE      - Play / Stop\n"
                    "  Q-P / A-L  - Play notes (keyboard)\n"
                    "  Z / X      - Octave down / up\n"
                    "  Arrow UP/DN- BPM +/-\n"
                    "  Arrow L/R  - Previous/Next pattern\n"
                    "  TAB        - Next instrument\n"
                    "  F1-F4      - Switch views\n\n"
                    "MOUSE:\n"
                    "  Left-click steps to toggle ON/OFF\n"
                    "  Drag to paint/erase\n"
                    "  Right-click step to cycle velocity\n"
                    "  Scroll wheel = BPM\n\n"
                    "COMPILE:\n"
                    "  gcc MirkoMusicMaker11.c -o MirkoMusicMaker11.exe\n"
                    "      -lwinmm -lm -lgdi32 -lcomdlg32 -mwindows",
                    "About",MB_OK|MB_ICONINFORMATION);
                break;
        }
        break;
    case WM_SIZE:
        InvalidateRect(hwnd,NULL,FALSE);
        break;
    case WM_CLOSE:
        if(g_dirty && MessageBox(hwnd,"Unsaved changes. Exit anyway?",APP_NAME,
            MB_YESNO|MB_ICONQUESTION)==IDNO) return 0;
        DestroyWindow(hwnd);
        break;
    default:
        return DefWindowProc(hwnd,msg,wp,lp);
    }
    return 0;
}

/* ============================================================
   WinMain
   ============================================================ */
int WINAPI WinMain(HINSTANCE hInst,HINSTANCE hPrev,LPSTR cmdLine,int nShow){
    srand((unsigned)time(NULL));
    InitializeCriticalSection(&g_lock);

    /* init data */
    InitDefaultInstruments();
    InitProject();

    /* register window class */
    WNDCLASSEX wc={0};
    wc.cbSize=sizeof(WNDCLASSEX);
    wc.style=CS_HREDRAW|CS_VREDRAW|CS_DBLCLKS;
    wc.lpfnWndProc=WndProc;
    wc.hInstance=hInst;
    wc.hbrBackground=(HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName="MirkoMM11";
    wc.hCursor=LoadCursor(NULL,IDC_ARROW);
    wc.hIcon=LoadIcon(NULL,IDI_APPLICATION);
    RegisterClassEx(&wc);

    /* create menu */
    HMENU menu=CreateMenu();
    HMENU file=CreatePopupMenu();
    AppendMenu(file,MF_STRING,ID_MENU_NEW,  "&New\tCtrl+N");
    AppendMenu(file,MF_STRING,ID_MENU_SAVE, "&Save...\tCtrl+S");
    AppendMenu(file,MF_STRING,ID_MENU_LOAD, "&Load...\tCtrl+O");
    AppendMenu(file,MF_SEPARATOR,0,NULL);
    AppendMenu(file,MF_STRING,ID_MENU_EXPORT,"&Export WAV...");
    AppendMenu(file,MF_SEPARATOR,0,NULL);
    AppendMenu(file,MF_STRING,ID_MENU_EXIT, "E&xit\tAlt+F4");
    AppendMenu(menu,MF_POPUP,(UINT_PTR)file,"&File");

    HMENU edit=CreatePopupMenu();
    AppendMenu(edit,MF_STRING,ID_MENU_UNDO, "&Undo\tCtrl+Z");
    AppendMenu(edit,MF_SEPARATOR,0,NULL);
    AppendMenu(edit,MF_STRING,ID_MENU_CLEAR,"Clear Pattern");
    AppendMenu(menu,MF_POPUP,(UINT_PTR)edit,"&Edit");

    HMENU help=CreatePopupMenu();
    AppendMenu(help,MF_STRING,ID_MENU_ABOUT,"&About");
    AppendMenu(menu,MF_POPUP,(UINT_PTR)help,"&Help");

    /* create window */
    g_hwnd=CreateWindowEx(
        0,"MirkoMM11",
        "Mirko Stevanovski MUSIC MAKER 11 - v11.0.0 (c) 2011",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,CW_USEDEFAULT,WIN_W,WIN_H,
        NULL,menu,hInst,NULL);

    ShowWindow(g_hwnd,nShow);
    UpdateWindow(g_hwnd);

    /* start audio */
    AudioInit();

    /* message loop */
    MSG msg;
    while(GetMessage(&msg,NULL,0,0)){
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return (int)msg.wParam;
}
