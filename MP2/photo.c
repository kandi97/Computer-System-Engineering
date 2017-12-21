/* tab:4
 *
 * photo.c - photo display functions
 *
 * "Copyright (c) 2011 by Steven S. Lumetta."
 *
 * Permission to use, copy, modify, and distribute this software and its
 * documentation for any purpose, without fee, and without written agreement is
 * hereby granted, provided that the above copyright notice and the following
 * two paragraphs appear in all copies of this software.
 *
 * IN NO EVENT SHALL THE AUTHOR OR THE UNIVERSITY OF ILLINOIS BE LIABLE TO
 * ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL
 * DAMAGES ARISING OUT  OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
 * EVEN IF THE AUTHOR AND/OR THE UNIVERSITY OF ILLINOIS HAS BEEN ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * THE AUTHOR AND THE UNIVERSITY OF ILLINOIS SPECIFICALLY DISCLAIM ANY
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE
 * PROVIDED HEREUNDER IS ON AN "AS IS" BASIS, AND NEITHER THE AUTHOR NOR
 * THE UNIVERSITY OF ILLINOIS HAS ANY OBLIGATION TO PROVIDE MAINTENANCE,
 * SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS."
 *
 * Author:        Steve Lumetta
 * Version:       3
 * Creation Date: Fri Sep 9 21:44:10 2011
 * Filename:      photo.c
 * History:
 *    SL    1    Fri Sep 9 21:44:10 2011
 *        First written(based on mazegame code).
 *    SL    2    Sun Sep 11 14:57:59 2011
 *        Completed initial implementation of functions.
 *    SL    3    Wed Sep 14 21:49:44 2011
 *        Cleaned up code for distribution.
 */


#include <string.h>

#include "assert.h"
#include "modex.h"
#include "photo.h"
#include "photo_headers.h"
#include "world.h"


/* types local to this file(declared in types.h) */

/*
 * A room photo.  Note that you must write the code that selects the
 * optimized palette colors and fills in the pixel data using them as
 * well as the code that sets up the VGA to make use of these colors.
 * Pixel data are stored as one-byte values starting from the upper
 * left and traversing the top row before returning to the left of
 * the second row, and so forth.  No padding should be used.
 */
struct photo_t {
    photo_header_t hdr;            /* defines height and width */
    uint8_t        palette[192][3];     /* optimized palette colors */
    uint8_t*       img;                 /* pixel data               */
};

/*
 * An object image.  The code for managing these images has been given
 * to you.  The data are simply loaded from a file, where they have
 * been stored as 2:2:2-bit RGB values(one byte each), including
 * transparent pixels(value OBJ_CLR_TRANSP).  As with the room photos,
 * pixel data are stored as one-byte values starting from the upper
 * left and traversing the top row before returning to the left of the
 * second row, and so forth.  No padding is used.
 */
struct image_t {
    photo_header_t hdr;  /* defines height and width */
    uint8_t*       img;  /* pixel data               */
};

/*
 * added by Sean 10/09/2017
 * data for 128 colors from level 4 (4096)
 * data for 64 colors from level 2 (64)
 */
struct select_Colors colorIndex128[NUM_IN_LEVER_4_OCTREE];
struct select_Colors colorIndex64[NUM_IN_LEVER_2_OCTREE];

/* file-scope variables */

/*
 * The room currently shown on the screen.  This value is not known to
 * the mode X code, but is needed when filling buffers in callbacks from
 * that code(fill_horiz_buffer/fill_vert_buffer).  The value is set
 * by calling prep_room.
 */
static const room_t* cur_room = NULL;


/*
 * fill_horiz_buffer
 *   DESCRIPTION: Given the(x,y) map pixel coordinate of the leftmost
 *                pixel of a line to be drawn on the screen, this routine
 *                produces an image of the line.  Each pixel on the line
 *                is represented as a single byte in the image.
 *
 *                Note that this routine draws both the room photo and
 *                the objects in the room.
 *
 *   INPUTS:(x,y) -- leftmost pixel of line to be drawn
 *   OUTPUTS: buf -- buffer holding image data for the line
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
void fill_horiz_buffer(int x, int y, unsigned char buf[SCROLL_X_DIM]) {
    int            idx;   /* loop index over pixels in the line          */
    object_t*      obj;   /* loop index over objects in the current room */
    int            imgx;  /* loop index over pixels in object image      */
    int            yoff;  /* y offset into object image                  */
    uint8_t        pixel; /* pixel from object image                     */
    const photo_t* view;  /* room photo                                  */
    int32_t        obj_x; /* object x position                           */
    int32_t        obj_y; /* object y position                           */
    const image_t* img;   /* object image                                */

    /* Get pointer to current photo of current room. */
    view = room_photo(cur_room);

    /* Loop over pixels in line. */
    for (idx = 0; idx < SCROLL_X_DIM; idx++) {
        buf[idx] = (0 <= x + idx && view->hdr.width > x + idx ? view->img[view->hdr.width * y + x + idx] : 0);
    }

    /* Loop over objects in the current room. */
    for (obj = room_contents_iterate(cur_room); NULL != obj; obj = obj_next(obj)) {
        obj_x = obj_get_x(obj);
        obj_y = obj_get_y(obj);
        img = obj_image(obj);

        /* Is object outside of the line we're drawing? */
        if (y < obj_y || y >= obj_y + img->hdr.height || x + SCROLL_X_DIM <= obj_x || x >= obj_x + img->hdr.width) {
            continue;
        }

        /* The y offset of drawing is fixed. */
        yoff = (y - obj_y) * img->hdr.width;

        /*
         * The x offsets depend on whether the object starts to the left
         * or to the right of the starting point for the line being drawn.
         */
        if (x <= obj_x) {
            idx = obj_x - x;
            imgx = 0;
        }
        else {
            idx = 0;
            imgx = x - obj_x;
        }

        /* Copy the object's pixel data. */
        for (; SCROLL_X_DIM > idx && img->hdr.width > imgx; idx++, imgx++) {
            pixel = img->img[yoff + imgx];

            /* Don't copy transparent pixels. */
            if (OBJ_CLR_TRANSP != pixel) {
                buf[idx] = pixel;
            }
        }
    }
}


/*
 * fill_vert_buffer
 *   DESCRIPTION: Given the(x,y) map pixel coordinate of the top pixel of
 *                a vertical line to be drawn on the screen, this routine
 *                produces an image of the line.  Each pixel on the line
 *                is represented as a single byte in the image.
 *
 *                Note that this routine draws both the room photo and
 *                the objects in the room.
 *
 *   INPUTS:(x,y) -- top pixel of line to be drawn
 *   OUTPUTS: buf -- buffer holding image data for the line
 *   RETURN VALUE: none
 *   SIDE EFFECTS: none
 */
void fill_vert_buffer(int x, int y, unsigned char buf[SCROLL_Y_DIM]) {
    int            idx;   /* loop index over pixels in the line          */
    object_t*      obj;   /* loop index over objects in the current room */
    int            imgy;  /* loop index over pixels in object image      */
    int            xoff;  /* x offset into object image                  */
    uint8_t        pixel; /* pixel from object image                     */
    const photo_t* view;  /* room photo                                  */
    int32_t        obj_x; /* object x position                           */
    int32_t        obj_y; /* object y position                           */
    const image_t* img;   /* object image                                */

    /* Get pointer to current photo of current room. */
    view = room_photo(cur_room);

    /* Loop over pixels in line. */
    for (idx = 0; idx < SCROLL_Y_DIM; idx++) {
        buf[idx] = (0 <= y + idx && view->hdr.height > y + idx ? view->img[view->hdr.width *(y + idx) + x] : 0);
    }

    /* Loop over objects in the current room. */
    for (obj = room_contents_iterate(cur_room); NULL != obj; obj = obj_next(obj)) {
        obj_x = obj_get_x(obj);
        obj_y = obj_get_y(obj);
        img = obj_image(obj);

        /* Is object outside of the line we're drawing? */
        if (x < obj_x || x >= obj_x + img->hdr.width ||
            y + SCROLL_Y_DIM <= obj_y || y >= obj_y + img->hdr.height) {
            continue;
        }

        /* The x offset of drawing is fixed. */
        xoff = x - obj_x;

        /*
         * The y offsets depend on whether the object starts below or
         * above the starting point for the line being drawn.
         */
        if (y <= obj_y) {
            idx = obj_y - y;
            imgy = 0;
        }
        else {
            idx = 0;
            imgy = y - obj_y;
        }

        /* Copy the object's pixel data. */
        for (; SCROLL_Y_DIM > idx && img->hdr.height > imgy; idx++, imgy++) {
            pixel = img->img[xoff + img->hdr.width * imgy];

            /* Don't copy transparent pixels. */
            if (OBJ_CLR_TRANSP != pixel) {
                buf[idx] = pixel;
            }
        }
    }
}


/*
 * image_height
 *   DESCRIPTION: Get height of object image in pixels.
 *   INPUTS: im -- object image pointer
 *   OUTPUTS: none
 *   RETURN VALUE: height of object image im in pixels
 *   SIDE EFFECTS: none
 */
uint32_t image_height(const image_t* im) {
    return im->hdr.height;
}


/*
 * image_width
 *   DESCRIPTION: Get width of object image in pixels.
 *   INPUTS: im -- object image pointer
 *   OUTPUTS: none
 *   RETURN VALUE: width of object image im in pixels
 *   SIDE EFFECTS: none
 */
uint32_t image_width(const image_t* im) {
    return im->hdr.width;
}

/*
 * photo_height
 *   DESCRIPTION: Get height of room photo in pixels.
 *   INPUTS: p -- room photo pointer
 *   OUTPUTS: none
 *   RETURN VALUE: height of room photo p in pixels
 *   SIDE EFFECTS: none
 */
uint32_t photo_height(const photo_t* p) {
    return p->hdr.height;
}


/*
 * photo_width
 *   DESCRIPTION: Get width of room photo in pixels.
 *   INPUTS: p -- room photo pointer
 *   OUTPUTS: none
 *   RETURN VALUE: width of room photo p in pixels
 *   SIDE EFFECTS: none
 */
uint32_t photo_width(const photo_t* p) {
    return p->hdr.width;
}


/*
 * prep_room
 *   DESCRIPTION: Prepare a new room for display.  You might want to set
 *                up the VGA palette registers according to the color
 *                palette that you chose for this room.
 *   INPUTS: r -- pointer to the new room
 *   OUTPUTS: none
 *   RETURN VALUE: none
 *   SIDE EFFECTS: changes recorded cur_room for this file
 */
void prep_room(const room_t* r) {
    /* Record the current room. */
	set_palette_192(room_photo(r)->palette);
    cur_room = r;
}


/*
 * read_obj_image
 *   DESCRIPTION: Read size and pixel data in 2:2:2 RGB format from a
 *                photo file and create an image structure from it.
 *   INPUTS: fname -- file name for input
 *   OUTPUTS: none
 *   RETURN VALUE: pointer to newly allocated photo on success, or NULL
 *                 on failure
 *   SIDE EFFECTS: dynamically allocates memory for the image
 */
image_t* read_obj_image(const char* fname) {
    FILE*    in;        /* input file               */
    image_t* img = NULL;    /* image structure          */
    uint16_t x;            /* index over image columns */
    uint16_t y;            /* index over image rows    */
    uint8_t  pixel;        /* one pixel from the file  */

    /*
     * Open the file, allocate the structure, read the header, do some
     * sanity checks on it, and allocate space to hold the image pixels.
     * If anything fails, clean up as necessary and return NULL.
     */
    if (NULL == (in = fopen(fname, "r+b")) ||
        NULL == (img = malloc(sizeof (*img))) ||
        NULL != (img->img = NULL) || /* false clause for initialization */
        1 != fread(&img->hdr, sizeof (img->hdr), 1, in) ||
        MAX_OBJECT_WIDTH < img->hdr.width ||
        MAX_OBJECT_HEIGHT < img->hdr.height ||
        NULL == (img->img = malloc
        (img->hdr.width * img->hdr.height * sizeof (img->img[0])))) {
        if (NULL != img) {
            if (NULL != img->img) {
                free(img->img);
            }
            free(img);
        }
        if (NULL != in) {
            (void)fclose(in);
        }
        return NULL;
    }

    /*
     * Loop over rows from bottom to top.  Note that the file is stored
     * in this order, whereas in memory we store the data in the reverse
     * order(top to bottom).
     */
    for (y = img->hdr.height; y-- > 0; ) {

        /* Loop over columns from left to right. */
        for (x = 0; img->hdr.width > x; x++) {

            /*
             * Try to read one 8-bit pixel.  On failure, clean up and
             * return NULL.
             */
            if (1 != fread(&pixel, sizeof (pixel), 1, in)) {
                free(img->img);
                free(img);
                (void)fclose(in);
                return NULL;
            }

            /* Store the pixel in the image data. */
            img->img[img->hdr.width * y + x] = pixel;
        }
    }

    /* All done.  Return success. */
    (void)fclose(in);
    return img;
}


/*
 * read_photo
 *   DESCRIPTION: Read size and pixel data in 5:6:5 RGB format from a
 *                photo file and create a photo structure from it.
 *                Code provided simply maps to 2:2:2 RGB.  You must
 *                replace this code with palette color selection, and
 *                must map the image pixels into the palette colors that
 *                you have defined.
 *   INPUTS: fname -- file name for input
 *   OUTPUTS: none
 *   RETURN VALUE: pointer to newly allocated photo on success, or NULL
 *                 on failure
 *   SIDE EFFECTS: dynamically allocates memory for the photo
 */
photo_t* read_photo(const char* fname) {
    FILE*    in;    	/* input file               */
    photo_t* p = NULL;  /* photo structure          */
    uint16_t x;       	/* index over image columns */
    uint16_t y;        	/* index over image rows    */
    uint16_t pixel;    	/* one pixel from the file  */

    /*
     * Open the file, allocate the structure, read the header, do some
     * sanity checks on it, and allocate space to hold the photo pixels.
     * If anything fails, clean up as necessary and return NULL.
     */
    if (NULL == (in = fopen(fname, "r+b")) ||
        NULL == (p = malloc(sizeof (*p))) ||
        NULL != (p->img = NULL) || /* false clause for initialization */
        1 != fread(&p->hdr, sizeof (p->hdr), 1, in) ||
        MAX_PHOTO_WIDTH < p->hdr.width ||
        MAX_PHOTO_HEIGHT < p->hdr.height ||
        NULL == (p->img = malloc
        (p->hdr.width * p->hdr.height * sizeof (p->img[0])))) {
        if (NULL != p) {
            if (NULL != p->img) {
                free(p->img);
            }
            free(p);
        }
        if (NULL != in) {
            (void)fclose(in);
        }
        return NULL;
    }

	/* Added by Sean 10/09/2017 */
	int i;
	/* initialize the colorIndex128 */
	for(i=0;i<NUM_IN_LEVER_4_OCTREE;i++)
	{
		colorIndex128[i].count = 0;
		colorIndex128[i].index = i;
		colorIndex128[i].sum_B = 0;
		colorIndex128[i].sum_G = 0;
		colorIndex128[i].sum_R = 0;
	}
	for(i=0;i<NUM_IN_LEVER_2_OCTREE;i++)
	{
		colorIndex64[i].count = 0;
		colorIndex64[i].index = i;
		colorIndex64[i].sum_B = 0;
		colorIndex64[i].sum_G = 0;
		colorIndex64[i].sum_R = 0;
	}
	
	/* added by Sean 10/11/2017 */
	/* store all pixels in order to assign palette value later */
	uint16_t pixelSet[p->hdr.height * p->hdr.width];
    /*
     * Loop over rows from bottom to top.  Note that the file is stored
     * in this order, whereas in memory we store the data in the reverse
     * order(top to bottom).
     */
    for (y = p->hdr.height; y-- > 0; ) {

        /* Loop over columns from left to right. */
        for (x = 0; p->hdr.width > x; x++) {

            /*
             * Try to read one 16-bit pixel.  On failure, clean up and
             * return NULL.
             */
            if (1 != fread(&pixel, sizeof (pixel), 1, in)) {
                free(p->img);
                free(p);
                (void)fclose(in);
                return NULL;
            }
            /*
             * 16-bit pixel is coded as 5:6:5 RGB(5 bits red, 6 bits green,
             * and 6 bits blue).  We change to 2:2:2, which we've set for the
             * game objects.  You need to use the other 192 palette colors
             * to specialize the appearance of each photo.
             *
             * In this code, you need to calculate the p->palette values,
             * which encode 6-bit RGB as arrays of three uint8_t's.  When
             * the game puts up a photo, you should then change the palette
             * to match the colors needed for that photo.
             */
            //p->img[p->hdr.width * y + x] = (((pixel >> 14) << 4) | (((pixel >> 9) & 0x3) << 2) | ((pixel >> 3) & 0x3));
			
			/* added by Sean 10/11/2017 */
			/* store each pixel for future use */
			pixelSet[p->hdr.width * y + x] = pixel;
			
			/* 
			 * Added by Sean 10/09/2017
			 * Deal with level 4
			 * Collectin data for colorIndex
			 * Calculate the 4 significant bits for red, green, and blue as index
			 */
			/* Get the each color first */
			const unsigned int red = (pixel>>11) & 0x1f;		// red 5-bit
			const unsigned int green = (pixel>>5) & 0x3f;		// green 6-bit
			const unsigned int blue = pixel & 0x1f;				// blue 5-bit
			
			int index128 = ((red >> 1) << 8) + ((green >> 2) << 4) + (blue >> 1);
			/* colorIndex128's count++ */
			colorIndex128[index128].count++;
			/* Collect the data for RGB */
			colorIndex128[index128].sum_R += red;
			colorIndex128[index128].sum_G += green;
			colorIndex128[index128].sum_B += blue;
			
			/* 
			 * Added by Sean 10/11/2017
			 * Deal with level 2
			 * Collectin data for colorIndex
			 * Calculate the 2 significant bits for red, green, and blue as index
			 */
			int index64 = ((red >> 3) << 4) + ((green >> 4) << 2) + (blue >> 3);
			/* colorIndex128's count++ */
			colorIndex64[index64].count++;
			/* Collect the data for RGB */
			colorIndex64[index64].sum_R += red;
			colorIndex64[index64].sum_G += green;
			colorIndex64[index64].sum_B += blue;
        }
    }
	
	/* Added by Sean 10/10/2017 */
	/* qsort is used to sort the colors in the order that from frequent to infrequent */
	qsort(colorIndex128, NUM_IN_LEVER_4_OCTREE, sizeof(colorIndex128[0]), compareFrequency);

	/*
	* Added by Sean 10/10/2017
	* calculate the average for red, green, and blue of former 128 colors in level 4
	* to find out which value for each color should be
	* Besides, also subtract the value appears in level 2 array
	* results will be stored in palette
	*/
	for(i=0;i<NUM_COLOR_LEVEL_4;i++)
	{
		if(colorIndex128[i].count)
		{
			/* 64 initial colors cannot be changed*/
			p->palette[i][0] = (uint8_t)(colorIndex128[i].sum_R / colorIndex128[i].count)<<1;
			p->palette[i][1] = (uint8_t)(colorIndex128[i].sum_G / colorIndex128[i].count);
			p->palette[i][2] = (uint8_t)(colorIndex128[i].sum_B / colorIndex128[i].count)<<1;
			
			/* get the r, g, b respectively */
			const unsigned int red = (colorIndex128[i].index >> 10) & 0x3;
			const unsigned int green = (colorIndex128[i].index >> 6) & 0x3;
			const unsigned int blue = (colorIndex128[i].index >> 2) & 0x3;
			
			/* calculate the index for level 2 */
			int indexLevel2 = (red << 4) + (green << 2) + (blue);
			
			/* get rid of the count and the sum in level 2*/
			colorIndex64[indexLevel2].count -= colorIndex128[i].count;
			colorIndex64[indexLevel2].sum_R -= colorIndex128[i].sum_R;
			colorIndex64[indexLevel2].sum_G -= colorIndex128[i].sum_G;
			colorIndex64[indexLevel2].sum_B -= colorIndex128[i].sum_B;
		}
	}
	
	/*
	* Added by Sean 10/11/2017
	* calculate the average for red, green, and blue of 64 colors in level 2
	* to find out which value for each color should be
	*/
	for(i=0;i<NUM_COLOR_LEVEL_2;i++)
	{
		if(colorIndex64[i].count > 0)
		{
			/* 64 initial colors cannot be changed*/
			p->palette[i+NUM_COLOR_LEVEL_4][0] = (uint8_t)(colorIndex64[i].sum_R / colorIndex64[i].count)<<1;
			p->palette[i+NUM_COLOR_LEVEL_4][1] = (uint8_t)(colorIndex64[i].sum_G / colorIndex64[i].count);
			p->palette[i+NUM_COLOR_LEVEL_4][2] = (uint8_t)(colorIndex64[i].sum_B / colorIndex64[i].count)<<1;
		}
	}
	
	/*
	* Added by Sean 10/11/2017
	* assign each pixel in image to palette index
	*/
	for(i=0;i<p->hdr.height * p->hdr.width;i++)
	{
		p->img[i] = findIndex(pixelSet[i]);
	}
	
    /* All done.  Return success. */
    (void)fclose(in);
    return p;
}

/* 
 * added by Sean 10/09/2017
 * compareFrequency
 *   DESCRIPTION: Used as parameter in "qsort" function
 *   INPUTS:(a,b) -- value for each two arguments which used to compare who's greater
 *   RETURN VALUE: int, the result of comparison
 *   SIDE EFFECTS: none
 */
int compareFrequency(const void* a, const void* b)
{
	struct select_Colors* x = (struct select_Colors*) a;
	struct select_Colors* y = (struct select_Colors*) b;
	
	return (x->count < y->count);
}


/*
 * Added by Sean 10/11/2017
 * 	 DESCRIPTION: Find the corresponding index for the pixel in palette
 *   INPUTS:(pixel) -- value for pixel
 *   RETURN VALUE: int, the result of index for the pixel
 *   SIDE EFFECTS: none
*/
uint8_t findIndex(uint16_t pixel)
{
	/* Get the each color first */
	const int red = (pixel>>11) & 0x1f;		// red 5-bit
	const int green = (pixel>>5) & 0x3f;	// green 6-bit
	const int blue = pixel & 0x1f;			// blue 5-bit
	
	/* calculate the index in level 4 */
	int indexLevel4 = ((red >> 1) << 8) + ((green >> 2) << 4) + (blue >> 1);
	
	/* find if it is inside 128 colors. If yes, assign the index, if not, find level 2 */
	int i;
	for(i=0;i<NUM_COLOR_LEVEL_4;i++)
	{
		if(indexLevel4 == colorIndex128[i].index)
		{
			return (64+i);
		}
	}
	
	/* calculate the index in level 2 */
	int indexLevel2 = ((red>>3) << 4) + ((green>>4) << 2) + (blue>>3);
	/* assign the corresponding value in level 2 */
	return (64+NUM_COLOR_LEVEL_4+indexLevel2);
}	
