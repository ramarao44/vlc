/*****************************************************************************
 * intf_sdl.c: SDL interface plugin
 *****************************************************************************
 * Copyright (C) 1999, 2000 VideoLAN
 *
 * Authors:
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

/*****************************************************************************
 * Preamble
 *****************************************************************************/
#include "defs.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <SDL/SDL.h>	        		       /* for all the SDL stuff	     */
#include <sys/types.h>                        /* on BSD, uio.h needs types.h */
#include <sys/uio.h>                                          /* for input.h */

#include "config.h"
#include "common.h"
#include "threads.h"
#include "mtime.h"
#include "plugins.h"

#include "input.h"
#include "video.h"
#include "video_output.h"


#include "interface.h"
#include "intf_msg.h"
#include "keystrokes.h"

#include "main.h"

/*****************************************************************************
 * intf_sys_t: description and status of SDL interface
 *****************************************************************************/
typedef struct intf_sys_s
{
    /* SDL system information */
    SDL_Surface * p_display;
    boolean_t b_Fullscreen;	
} intf_sys_t;

typedef struct vout_sys_s
{
    SDL_Surface *   p_display;                             /* display device */
    Uint8   *   p_buffer[2];
                                                     /* Buffers informations */
    boolean_t   b_must_acquire;           /* must be acquired before writing */
}   vout_sys_t;


/* local prototype */
void    intf_SDL_Keymap( intf_thread_t * p_intf );
void intf_SDL_Fullscreen(intf_thread_t * p_intf);
   

/*****************************************************************************
 * intf_SDLCreate: initialize and create SDL interface
 *****************************************************************************/
int intf_SDLCreate( intf_thread_t *p_intf )
{
    /* Check that b_video is set */
    if( !p_main->b_video )
    {
        intf_ErrMsg( "error: SDL interface requires a video output thread\n" );
        return( 1 );
    }

    /* Allocate instance and initialize some members */
    p_intf->p_sys = malloc( sizeof( intf_sys_t ) );
    if( p_intf->p_sys == NULL )
    {
        intf_ErrMsg("error: %s\n", strerror(ENOMEM) );
        return( 1 );
    }

    /* Spawn video output thread */
    p_intf->p_vout = vout_CreateThread( main_GetPszVariable( VOUT_DISPLAY_VAR,
                                                             NULL), 0,
                                        main_GetIntVariable( VOUT_WIDTH_VAR,
                                                         VOUT_WIDTH_DEFAULT ),
                                        main_GetIntVariable( VOUT_HEIGHT_VAR,
                                                        VOUT_HEIGHT_DEFAULT ),
                                        NULL, 0,
                                        (void *)&p_intf->p_sys->p_display );

    if( p_intf->p_vout == NULL )                                  /* error */
    {
        intf_ErrMsg( "error: can't create video output thread\n" );
        free( p_intf->p_sys );
        return( 1 );
    }
    intf_SDL_Keymap( p_intf );
    return( 0 );
}

/*****************************************************************************
 * intf_SDLDestroy: destroy interface
 *****************************************************************************/
void intf_SDLDestroy( intf_thread_t *p_intf )
{
    /* Close input thread, if any (blocking) */
    if( p_intf->p_input )
    {
        input_DestroyThread( p_intf->p_input, NULL );
    }

    /* Close video output thread, if any (blocking) */
    if( p_intf->p_vout )
    {
        vout_DestroyThread( p_intf->p_vout, NULL );
    }

    /* Destroy structure */
    
    SDL_FreeSurface( p_intf->p_sys->p_display );     /* destroy the "screen" */
    SDL_Quit();
    free( p_intf->p_sys );
}


/*****************************************************************************
 * intf_SDLManage: event loop
 *****************************************************************************/
void intf_SDLManage( intf_thread_t *p_intf )
{
	SDL_Event event; 										/*	SDL event	 */
    Uint8   i_key;
    
    while ( SDL_PollEvent(&event) ) 
    {
        i_key = event.key.keysym.sym;                          /* forward it */
        
        switch (event.type) {           
            case SDL_KEYDOWN:                         /* if a key is pressed */
                switch(i_key) {
                                                    /* switch to fullscreen  */
                    case SDLK_f:
                        intf_SDL_Fullscreen(p_intf);
                        break;
                       
                  default :
                        if( intf_ProcessKey( p_intf, (char ) i_key ) )
                        {
                            intf_DbgMsg( "unhandled key '%c' (%i)\n", 
                                         (char) i_key, i_key );
                        }
                        break;
                }
                break;
                
            case SDL_QUIT:
                intf_ProcessKey( p_intf, INTF_KEY_QUIT ); 
                break;
           default:
                break;
        }
    }
}

void intf_SDL_Fullscreen(intf_thread_t * p_intf)
{
    SDL_FreeSurface( p_intf->p_vout->p_sys->p_display );
    
    if(p_intf->p_sys->b_Fullscreen == 0)
    {
        p_intf->p_vout->p_sys->p_display = 
                SDL_SetVideoMode(
                                 p_intf->p_vout->i_width,
                                 p_intf->p_vout->i_height,
                                 15,
                                 SDL_ANYFORMAT |
                                 SDL_HWSURFACE |
                                 SDL_DOUBLEBUF);
        p_intf->p_sys->b_Fullscreen = 1;
    }
    else
    {
        p_intf->p_vout->p_sys->p_display = 
                SDL_SetVideoMode(
                                 p_intf->p_vout->i_width,
                                 p_intf->p_vout->i_height,
                                 15,
                                 SDL_ANYFORMAT |
                                 SDL_HWSURFACE |
                                 SDL_DOUBLEBUF |
                                 SDL_FULLSCREEN );
        p_intf->p_sys->b_Fullscreen = 0;                        
    }
    SDL_EventState(SDL_KEYUP , SDL_IGNORE);
    p_intf->p_vout->p_sys->p_buffer[ 0 ] = p_intf->p_vout->p_sys->p_display->pixels;
    
    SDL_Flip(p_intf->p_vout->p_sys->p_display);
    p_intf->p_vout->p_sys->p_buffer[ 1 ] = p_intf->p_vout->p_sys->p_display->pixels;
    
    SDL_Flip(p_intf->p_vout->p_sys->p_display);
    SDL_SetClipping(p_intf->p_vout->p_sys->p_display, 0, 0,
            p_intf->p_vout->p_sys->p_display->w,
            p_intf->p_vout->p_sys->p_display->h );
    
    p_intf->p_vout->i_width =           p_intf->p_vout->p_sys->p_display->w;
    p_intf->p_vout->i_height =          p_intf->p_vout->p_sys->p_display->h;

    p_intf->p_vout->i_bytes_per_line = 
        p_intf->p_vout->p_sys->p_display->format->BytesPerPixel 
        * 
        p_intf->p_vout->p_sys->p_display->w ;

    p_intf->p_vout->i_screen_depth =    
        p_intf->p_vout->p_sys->p_display->format->BitsPerPixel;
    p_intf->p_vout->i_bytes_per_pixel = 
        p_intf->p_vout->p_sys->p_display->format->BytesPerPixel;
    p_intf->p_vout->i_red_mask =        
        p_intf->p_vout->p_sys->p_display->format->Rmask;
    p_intf->p_vout->i_green_mask =      
        p_intf->p_vout->p_sys->p_display->format->Gmask;
    p_intf->p_vout->i_blue_mask =       
        p_intf->p_vout->p_sys->p_display->format->Bmask;
} 
    



void intf_SDL_Keymap(intf_thread_t * p_intf )
{
    //p_intf->p_intf_getKey = intf_getKey; 
    intf_AssignKey(p_intf, SDLK_q,      INTF_KEY_QUIT, 0);
    intf_AssignKey(p_intf, SDLK_ESCAPE, INTF_KEY_QUIT, 0);
    /* intf_AssignKey(p_intf,3,'Q'); */
    intf_AssignKey(p_intf, SDLK_0,      INTF_KEY_SET_CHANNEL,0);
    intf_AssignKey(p_intf, SDLK_1,      INTF_KEY_SET_CHANNEL,1);
    intf_AssignKey(p_intf, SDLK_2,      INTF_KEY_SET_CHANNEL,2);
    intf_AssignKey(p_intf, SDLK_3,      INTF_KEY_SET_CHANNEL,3);
    intf_AssignKey(p_intf, SDLK_4,      INTF_KEY_SET_CHANNEL,4);
    intf_AssignKey(p_intf, SDLK_5,      INTF_KEY_SET_CHANNEL,5);
    intf_AssignKey(p_intf, SDLK_6,      INTF_KEY_SET_CHANNEL,6);
    intf_AssignKey(p_intf, SDLK_7,      INTF_KEY_SET_CHANNEL,7);
    intf_AssignKey(p_intf, SDLK_8,      INTF_KEY_SET_CHANNEL,8);
    intf_AssignKey(p_intf, SDLK_9,      INTF_KEY_SET_CHANNEL,9);
    intf_AssignKey(p_intf, SDLK_PLUS,   INTF_KEY_INC_VOLUME, 0);
    intf_AssignKey(p_intf, SDLK_MINUS,  INTF_KEY_DEC_VOLUME, 0);
    intf_AssignKey(p_intf, SDLK_m,      INTF_KEY_TOGGLE_VOLUME, 0);
    /* intf_AssignKey(p_intf,'M','M'); */
    intf_AssignKey(p_intf, SDLK_g,      INTF_KEY_DEC_GAMMA, 0);
    /* intf_AssignKey(p_intf,'G','G'); */
    intf_AssignKey(p_intf, SDLK_c,      INTF_KEY_TOGGLE_GRAYSCALE, 0);
    intf_AssignKey(p_intf, SDLK_SPACE,  INTF_KEY_TOGGLE_INTERFACE, 0);
    intf_AssignKey(p_intf, 'i',         INTF_KEY_TOGGLE_INFO, 0);
    intf_AssignKey(p_intf, SDLK_s,      INTF_KEY_TOGGLE_SCALING, 0);

}

