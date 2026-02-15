/*
*	SDL Graphics Extension
*	Text/Bitmap font functions
*
*	Started 990815
*
*	License: LGPL v2+ (see the file LICENSE)
*	(c)1999-2001 Anders Lindström
*/

/*********************************************************************
 *  This library is free software; you can redistribute it and/or    *
 *  modify it under the terms of the GNU Library General Public      *
 *  License as published by the Free Software Foundation; either     *
 *  version 2 of the License, or (at your option) any later version. *
 *********************************************************************/

/*
*  Some of this is taken from SDL_DrawText by Garrett Banuk (mongoose@wpi.edu)
*  http://www.wpi.edu/~mongoose/SDL_Console
*  Thanks to Karl Bartel for the SFont format!
*/

#include "SDL.h"
#include <stdarg.h>
#include <string.h>
#include <math.h>
#include <new>
#include "sge_surface.h"
#include "sge_bm_text.h"
#include "sge_tt_text.h"

#ifdef _SGE_HAVE_IMG
#include "SDL_image.h"
#endif

using namespace std;

/* Globals used for sge_Update/sge_Lock (defined in sge_surface) */
extern Uint8 _sge_update;
extern Uint8 _sge_lock;

//==================================================================================
// Creates a new font from a surface
//==================================================================================
sge_bmpFont* sge_BF_CreateFont(SDL_Surface *surface, Uint8 flags)
{
	sge_bmpFont	*font;

	font = new(nothrow) sge_bmpFont; if(font==NULL){SDL_SetError("SGE - Out of memory");return NULL;}
	
	if(!(flags&SGE_BFNOCONVERT) && !(flags&SGE_BFSFONT)){    /* Get a converted copy */
		font->FontSurface = SDL_DisplayFormat(surface);
		if(font->FontSurface==NULL){SDL_SetError("SGE - Out of memory");return NULL;}
		
		if(flags&SGE_BFPALETTE){  //We want an 8bit surface
			SDL_Surface *tmp;
			tmp = SDL_AllocSurface(SDL_SWSURFACE, surface->w, surface->h, 8, 0, 0, 0, 0);
			if(tmp==NULL){SDL_SetError("SGE - Out of memory");SDL_FreeSurface(font->FontSurface);return NULL;}
			
			//Set the palette
			tmp->format->palette->colors[0].r = 0;
			tmp->format->palette->colors[0].g = 0;
			tmp->format->palette->colors[0].b = 0;
			tmp->format->palette->colors[1].r = 255;
			tmp->format->palette->colors[1].g = 255;
			tmp->format->palette->colors[1].b = 255;

			if (SDL_MUSTLOCK(font->FontSurface) && _sge_lock)
				if (SDL_LockSurface(font->FontSurface) < 0){
					SDL_SetError("SGE - Locking error");
					SDL_FreeSurface(font->FontSurface);
					return NULL;
				}

			//Copy the font to the 8bit surface
			Sint16 x,y;
			Uint32 bc=sge_GetPixel(font->FontSurface,0,surface->h-1);
			for(y=0; y<font->FontSurface->h; y++){
   				for(x=0; x<font->FontSurface->w; x++){
   					if(sge_GetPixel(font->FontSurface,x,y)==bc)
						*((Uint8 *)tmp->pixels + y * tmp->pitch + x)=0;
					else
						*((Uint8 *)tmp->pixels + y * tmp->pitch + x)=1;
   				}
   			}
			
			if (SDL_MUSTLOCK(font->FontSurface) && _sge_lock) {
				SDL_UnlockSurface(font->FontSurface);
			}

			//sge_Blit(surface, tmp, 0,0,0,0,surface->w, surface->h);
			SDL_FreeSurface(font->FontSurface);
			font->FontSurface=tmp;	
		}
			
		
		if((flags&SGE_FLAG8))
			SDL_FreeSurface(surface);
	}
	else if(flags&SGE_FLAG8) /* Use the source */
		font->FontSurface = surface;
	else                     /* Get a copy */
		font->FontSurface = sge_copy_surface(surface);
		
	if(font->FontSurface==NULL){SDL_SetError("SGE - Out of memory");return NULL;}
		
	SDL_Surface *fnt = font->FontSurface; //Shorthand
	font->Chars=0;
			
	if(!(flags&SGE_BFSFONT)){  /* Fixed width font */
		font->CharWidth = font->FontSurface->w/256;
		font->CharHeight = font->FontSurface->h;
		font->CharPos = NULL;
		font->yoffs = 0;
		font->Chars=256;
	}
	else{                    /* Karl Bartel's sfont */
		Sint16 x=0;
		int i=0;
		font->CharPos = new(nothrow) Sint16[512];
		if(!font->CharPos){SDL_SetError("SGE - Out of memory");sge_BF_CloseFont(font);return NULL;}
		Uint32 color = sge_GetPixel(fnt,0,0);

		while (x<fnt->w && font->Chars<256){
			if (sge_GetPixel(fnt,x,0)==color) {
				font->CharPos[i++]=x;
				
				while (x<fnt->w-1 && sge_GetPixel(fnt,x,0)==color)
					x++;
			
				font->CharPos[i++]=x;
				font->Chars++;
			}
			x++;
		}
		
		font->CharHeight = font->FontSurface->h-1;
		font->CharWidth = 0;
		font->yoffs = 1;
	}

	/* Set font as transparent if the flag is set */
	if (SDL_MUSTLOCK(font->FontSurface) && _sge_lock)
		if (SDL_LockSurface(font->FontSurface) < 0){
			return font;
		}
	font->bcolor=sge_GetPixel(font->FontSurface,0,font->FontSurface->h-1);
	if (SDL_MUSTLOCK(font->FontSurface) && _sge_lock) {
		SDL_UnlockSurface(font->FontSurface);
	}
	if(flags&SGE_BFTRANSP || flags&SGE_BFSFONT)
		#if SDL_VERSIONNUM(SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL) >= \
    		SDL_VERSIONNUM(1, 1, 4)
		SDL_SetColorKey(font->FontSurface,SDL_SRCCOLORKEY, font->bcolor);  //Some versions of SDL have a bug with SDL_RLEACCEL
		#else
		SDL_SetColorKey(font->FontSurface,SDL_SRCCOLORKEY|SDL_RLEACCEL, font->bcolor);
		#endif

	return font;
}


//==================================================================================
// Loads the font into a new struct
//==================================================================================
sge_bmpFont* sge_BF_OpenFont(const char *file, Uint8 flags)
{
	sge_bmpFont *font;
	SDL_Surface	*Temp;
	
	/* load the font bitmap */
	#ifdef _SGE_HAVE_IMG
	if(NULL ==  (Temp = IMG_Load(file)))  //We have SDL_Img lib!
	#else
	if(NULL ==  (Temp = SDL_LoadBMP(file))) //We can only load bmp files...
	#endif
	{
		sge_SetError("SGE - Couldn't load font file: %s",file);
		return NULL;
	}

	font = sge_BF_CreateFont(Temp,flags|SGE_FLAG8); //SGE_FLAG8 - no need to make a copy of the surface
	
	return font;
}


//==================================================================================
// Draws string to surface with the selected font
// Returns pos. and size of the drawn text
//==================================================================================
SDL_Rect sge_BF_textout(SDL_Surface *surface, sge_bmpFont *font, const char *string, Sint16 x, Sint16 y, int length, bool doupdate)
{
	SDL_Rect ret;  ret.x=0;ret.y=0;ret.w=0;ret.h=0;

	if(font==NULL || length==0){return ret;}

	int characters;
	Sint16 xsrc,xdest,ofs,adv=font->CharWidth;
	float diff=0;

	/* Valid coords ? */
	if(surface)
		if(x>surface->w || y>surface->h) 
			return ret;

	characters = strlen(string);

	xdest=x;

	/* Now draw it */
	for(int i=0; i<characters; i++)
	{
		if(!font->CharPos) /* Fixed width */
			xsrc = string[i] * font->CharWidth;
		else{              /* Variable width */
			unsigned char c = string[i];
			
			if(c==' ' || (c-33)>font->Chars || c<33){
				xdest += font->CharPos[2]-font->CharPos[1];
				continue;
			}
			
			ofs = ( ((unsigned int)c)-33)*2+1;
			xsrc = (font->CharPos[ofs]+font->CharPos[ofs-1])/2;
			//font->CharWidth = (font->CharPos[ofs+2]+font->CharPos[ofs+1])/2-(font->CharPos[ofs]+font->CharPos[ofs-1])/2-1;
			font->CharWidth = (font->CharPos[ofs+2]+font->CharPos[ofs+1])/2-(font->CharPos[ofs]+font->CharPos[ofs-1])/2;
			adv = font->CharPos[ofs+1]-font->CharPos[ofs];
			diff =(font->CharPos[ofs]-font->CharPos[ofs-1])/2.0;
		}
		
		if(surface)
			sge_Blit(font->FontSurface, surface, xsrc,font->yoffs, int(xdest-diff),y, font->CharWidth,font->CharHeight);	
		
		xdest += adv;
		--length;
		if (0 == length)
		{
			break;
		}
	}

	//ret.x=x; ret.y=y; ret.w=xdest-x+font->CharWidth; ret.h=font->CharHeight;
	ret.x=x; ret.y=y; ret.w=xdest-x+adv; ret.h=font->CharHeight;
	
	if(surface && doupdate)
		sge_UpdateRect(surface, x, y, ret.w, ret.h);
	
	return ret;
}


//==================================================================================
// Returns the size (w and h) of the string (if rendered with font)
//==================================================================================
SDL_Rect sge_BF_TextSize(sge_bmpFont *font, const char *string, int length)
{
	return sge_BF_textout(NULL, font, string, 0, 0, length);
}


//==================================================================================
// Draws formated text to surface with the selected font
// Returns pos. and size of the drawn text
// * just like printf(char *format, ...) *
//==================================================================================
SDL_Rect sge_BF_textoutf(SDL_Surface *surface, sge_bmpFont *font, Sint16 x, Sint16 y ,const char *format, ...)
{
	char buf[256];

	va_list ap;
	va_start(ap, format);
	vsprintf(buf, format, ap);
	va_end(ap);

	return sge_BF_textout(surface, font, buf, x, y);
}


//==================================================================================
// Returns the height of the font
// Returns 0 if the struct was invalid
//==================================================================================
Sint16 sge_BF_GetHeight(sge_bmpFont *font)
{
	if(font)
		return font->CharHeight;
	else
		return 0;
}


//==================================================================================
// Returns the width of the font (only fixed width fonts)
// Returns 0 if the struct was invalid
//==================================================================================
Sint16 sge_BF_GetWidth(sge_bmpFont *font)
{
	if(font)
		return font->CharWidth;
	else
		return 0;
}


//==================================================================================
// Removes font from memory
//==================================================================================
void sge_BF_CloseFont(sge_bmpFont *font)
{
	if(font){
		SDL_FreeSurface(font->FontSurface);
		
		if(font->CharPos)
			delete[] font->CharPos;
			
		delete font;
		font=NULL;
	}
}


//==================================================================================
// Change the font color
// Will not work on 'color' fonts!
// Doesn't like 24bpp
// slooooow!
//==================================================================================
void sge_BF_SetColor(sge_bmpFont *font, Uint8 R, Uint8 G, Uint8 B)
{
	if(font==NULL){return;}

	if(!font->FontSurface->format->palette){  //Slow truecolor version
		Sint16 x,y;
		Sint16 ypnt;
		SDL_Surface *surface=font->FontSurface;
		Uint32 c_keep=font->bcolor;
	
		Uint32 color=SDL_MapRGB(font->FontSurface->format, R, G, B);

		switch(surface->format->BytesPerPixel){
  			case 1: { /* Assuming 8-bpp */
  				Uint8 *pnt;
   				for(y=0; y<surface->h; y++){
   					ypnt=y*surface->pitch;
   					for(x=0; x<surface->w; x++){
   						pnt=((Uint8 *)surface->pixels + x + ypnt);
   						if(*pnt!=c_keep){*pnt=(Uint8)color;}
   					}
   				}
   			}
   			break;

			case 2: { /* Probably 15-bpp or 16-bpp */
				Uint16 *pnt;
    			for(y=0; y<surface->h; y++){
   					ypnt=y*surface->pitch/2;
   					for(x=0; x<surface->w; x++){
   						pnt=((Uint16 *)surface->pixels + x + ypnt);
   						if(*pnt!=c_keep){*pnt=(Uint16)color;}
   					}
   				}
    		}
			break;

			case 3: { /* Slow 24-bpp mode, usually not used */
			}
    		break;

			case 4: { /* Probably 32-bpp */
				Uint32 *pnt;
    			for(y=0; y<surface->h; y++){
   					ypnt=y*surface->pitch/4;
   					for(x=0; x<surface->w; x++){
   						pnt=((Uint32 *)surface->pixels + x + ypnt);
   						if(*pnt!=c_keep){*pnt=(Uint32)color;}
   					}
   				}	
    		}
			break;
		}
	}else{  //Fast palette version
		SDL_Color c[2];
		c[0].r=0; c[1].r=R;
		c[0].g=0; c[1].g=G;
		c[0].b=0; c[1].b=B;
		SDL_SetColors(font->FontSurface, c, 0, 2);
	}
}


//==================================================================================
// Insert a element (latin1)
//==================================================================================
void insert_char_latin1(char *string, char ch, int pos, int max)
{
	if(pos>max || pos<0){return;}
	else if(pos==max){string[pos]=ch;}
	else{
  	for(int i=max; i>=pos; i--){
			string[i+1]=string[i];	
		}
		string[pos]=ch;
	}
}


//==================================================================================
// Delete a element (latin1)
//==================================================================================
void delete_char_latin1(char *string, int pos, int max)
{
	if(pos>max || pos<0){return;}
	else if(pos==max){string[pos]=0;}
	else{
  	for(int i=pos; i<=max-1; i++){
			string[i]=string[i+1];	
		}
		string[max]=0;
	}
}


//==================================================================================
// BitmapText input
//==================================================================================
int sge_BF_input(SDL_Surface *screen,sge_bmpFont *font,char *string, Uint8 flags, int pos,int len,Sint16 x,Sint16 y)
{
	if(len<pos || pos<0 || len<0 || font==NULL){return -2;}

	char cur=124;//The character for the cursor - '|'
	int max;     //The strings size   	

	SDL_Rect ret,tmp;

	SDL_Surface *buffer=NULL;
	buffer=SDL_DisplayFormat(screen); /* Fixme: Yum! Memory! */
	if(buffer==NULL){SDL_SetError("SGE - Out of memory");return -3;}
	
	max=pos;
	string[pos+1]=0;

	SDL_EnableUNICODE(1);

	/* Init cursor */
	string[pos]=cur;
	ret=sge_BF_textout(screen, font, string, x, y);	

	SDL_Event event;
	int quit=0;
	do{
		/* Check events */
		SDL_WaitEvent(&event);
		if(event.type==SDL_QUIT){quit=-1;}
		else if(event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_ESCAPE){quit=1;}
		else if(event.type==SDL_KEYDOWN && (event.key.keysym.sym==SDLK_RETURN || event.key.keysym.sym==SDLK_KP_ENTER)){quit=1;}
		else if(event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_BACKSPACE){
    	if(pos>0){
				/* Delete char cursor-1 */
				delete_char_latin1(string,pos-1,max); pos--; max--;

				sge_Update_OFF();
				sge_Blit(buffer,screen, ret.x, ret.y, x,y, ret.w, ret.h);
				tmp=ret;
				ret=sge_BF_textout(screen, font, string, x, y);
				sge_Update_ON();
				sge_UpdateRect(screen, tmp.x, tmp.y, tmp.w, tmp.h);
				
				/* Handle keyrepeat */
				if(!(flags&SGE_FLAG3))
					if(keyrepeat(&event, 20)==-1){quit=-1;}
			}
		}	
		else if(event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_RIGHT){
			if(pos!=max && pos!=len){
				/* Move cursor right */
				delete_char_latin1(string,pos,max);pos++;
				insert_char_latin1(string,cur,pos,max);

				sge_Blit(buffer,screen, ret.x, ret.y, x,y, ret.w, ret.h);
				ret=sge_BF_textout(screen, font, string, x, y);

				/* Handle keyrepeat */
				if(!(flags&SGE_FLAG3))
					if(keyrepeat(&event, 20)==-1){quit=-1;}
			}		
		}	
		else if(event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_LEFT){
			if(pos>0){
				/* Move cursor left */
				delete_char_latin1(string,pos,max);pos--;
				insert_char_latin1(string,cur,pos,max);

				sge_Blit(buffer,screen, ret.x, ret.y, x,y, ret.w, ret.h);
				ret=sge_BF_textout(screen, font, string, x, y);	

				/* Handle keyrepeat */
				if(!(flags&SGE_FLAG3))
					if(keyrepeat(&event, 20)==-1){quit=-1;}
			}	
		}
		else if(event.type==SDL_KEYDOWN && event.key.keysym.sym==SDLK_DELETE){
			/* Delete char cursor+1 */
			if(pos!=max && pos!=len){
				delete_char_latin1(string,pos+1,max);max--;
				
				sge_Update_OFF();
				sge_Blit(buffer,screen, ret.x, ret.y, x,y, ret.w, ret.h);
				tmp=ret;
				ret=sge_BF_textout(screen, font, string, x, y);
				sge_Update_ON();
				sge_UpdateRect(screen, tmp.x, tmp.y, tmp.w, tmp.h);
	
				/* Handle keyrepeat */
				if(!(flags&SGE_FLAG3))
					if(keyrepeat(&event, 20)==-1){quit=-1;}
			}	
		}
		else if(event.type==SDL_KEYDOWN && event.key.keysym.unicode!=0){
			/* Insert new char */
			if(max!=len){
				max++;
				insert_char_latin1(string, (char)event.key.keysym.unicode, pos,max); pos++;	

				sge_Blit(buffer,screen, ret.x, ret.y, x,y, ret.w, ret.h);
				ret=sge_BF_textout(screen, font, string, x, y);

				/* Handle keyrepeat */
				if(!(flags&SGE_FLAG3))
					if(keyrepeat(&event, 40)==-1){quit=-1;}	
			}				
		}
	}while(quit==0);
	
	/* Remove the cursor from string */
	delete_char_latin1(string,pos,max);

	//Remove text/cursor from screen
	sge_Blit(buffer,screen, ret.x, ret.y, x,y, ret.w, ret.h);
	if(!(flags&SGE_FLAG2))
		ret=sge_BF_textout(screen, font, string, x, y);	
	else
		sge_UpdateRect(screen, x,y, ret.w, ret.h);

	SDL_FreeSurface(buffer);

	if(quit==-1){return -1;} //Waaa! The user killed me!
	return max;
}
