#ifndef SC_STATUS_BOX_H
#define SC_STATUS_BOX_H

#include <stdint.h>
#include <string.h>

typedef struct {
    int width;
    int height;
    uint32_t background_argb;
    uint32_t foreground_argb;
} SCStatusBox;

typedef struct {
    int x;
    int y;
    int row_height;
    int detail_width;
} SCStatusPlacement;

/* Keep owned messages below the game's top-centre cockpit display. */
static int sc_status_placement(int screen_width, int screen_height,
                               SCStatusPlacement *out)
{
    if(!out||screen_width<640||screen_width>7680||
       screen_height<480||screen_height>4320)return 0;
    out->x=screen_width/2;
    out->y=(screen_height*17)/100;
    out->row_height=(screen_height*35)/1000;
    if(out->row_height<16)out->row_height=16;
    out->detail_width=screen_width/4;
    if(out->detail_width<360)out->detail_width=360;
    if(out->detail_width>screen_width-32)out->detail_width=screen_width-32;
    return out->y+3*out->row_height<screen_height;
}

static int sc_status_detail_width(const char *text,
                                  const SCStatusPlacement *placement,
                                  int screen_width)
{
    int width;
    if(!text||!placement)return 0;
    width=(int)strlen(text)*placement->row_height*3/5+
          placement->row_height;
    if(width<placement->detail_width)width=placement->detail_width;
    if(width>screen_width-32)width=screen_width-32;
    return width;
}

/* Render only the owned headline; every other native HUD message keeps the
 * game's normal text path. Sizes follow its 3.5%-of-screen line spacing. */
static int sc_status_box_spec(const char *text, uint32_t native_color,
                              int screen_height, SCStatusBox *out)
{
    int line;
    if(!text||!out||screen_height<480||screen_height>4320)return 0;
    if(!strcmp(text,"SC")){
        if(native_color!=0xFFFFFF00u)return 0;
    }else if(!strcmp(text,"GREEN")){
        if(native_color!=0xFF00BC1Cu)return 0;
    }else return 0;
    line=(screen_height*35)/1000;
    if(line<16)return 0;
    out->height=line;
    out->width=!strcmp(text,"SC")?(line*22)/10:(line*42)/10;
    out->background_argb=native_color;
    out->foreground_argb=0xFF000000u;
    return 1;
}

#endif
