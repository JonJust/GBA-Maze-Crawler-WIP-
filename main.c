#include "Intellisense.h"
#include <stdio.h>
#include "gba_reg.h"
#include "gba_input.h"
#include "gba_types.h"
#include "gba_macros.h"
#include "gba_gfx.h"
#include <math.h>

#define REG_DISPCNT *((unsigned int*)(0x04000000))
#define VIDEOMODE_3 0x0003
#define BG_ENABLE2 0x0400

#define REG_VCOUNT *((volatile unsigned short*)(0x04000006))

#define SCREENBUFFER ((unsigned short*)(0x06000000))
#define SCREEN_W 240
#define SCREEN_H 160

#define TILE_SIZE 20 // Tiles will be 20*20 pixels

v_u32 px,py,pa; //player position and angle

#define STAGE_W 14
#define STAGE_H 14

v_u32 stage[STAGE_W + 1][STAGE_H + 1]; // matrix representing the stage
v_u32 renderMatrix[12][8]; // matrix used to draw the stage. (GBA scr proportionate to tile size)
volatile int renderOffsetX = 0;
volatile int renderOffsetY = 0; // used to update renderMatrix

bool recentRenderX, recentRenderY; // Used for when the screen rolls over in 2d overhead

void fillRenderMatrix()
{

	v_u32 x,y;
	for(x = 0; x <= 11; ++x)
	{
		for( y = 0; y <= 7; ++y)
		{
			renderMatrix[x][y] = stage[x + renderOffsetX][y + renderOffsetY];		
		}
	}

}

void setOffSet()
{
	// updates render offsets
	// Anytime the render offset is change the render matrix is updated. (Less CPU intensive to have in this method)

	if (((px - TILE_SIZE*renderOffsetX) < 25) && (renderOffsetX >= 1))
	{
		clearPlayer2d(); // In case of future spooky bug clearSTage2d used to happen here instead of past the if statement
		renderOffsetX = renderOffsetX - 5;
		if (renderOffsetX >= 0) // Yes this is ugly but it needs to be checked for better performance
		{
		fillRenderMatrix();
		clearStage2D();
		renderStage2D();
		}

	}
	else if (((px - TILE_SIZE*renderOffsetX) > 215) && (renderOffsetX < (STAGE_W - 11))) // -11 pre bug test
	{	
		clearPlayer2d();
		renderOffsetX = renderOffsetX + 5;
		if (renderOffsetX <= (STAGE_W - 11))
		{
		clearStage2D();
		fillRenderMatrix();
		renderStage2D();
		}
	}

	if (((py - TILE_SIZE*renderOffsetY) < 25) && (renderOffsetY > 0))
	{
		clearPlayer2d();
		renderOffsetY = renderOffsetY - 5;
		if (renderOffsetY >= 0)
		{
		clearStage2D();
		fillRenderMatrix();
		renderStage2D();
		}
	}
	else if (((py - TILE_SIZE*renderOffsetY) > 135) && (renderOffsetY < (STAGE_H - 7)))
	{
		clearPlayer2d();
		renderOffsetY = renderOffsetY + 5;
		if (renderOffsetY <= (STAGE_H - 7))
		{
		clearStage2D();
		fillRenderMatrix();
		renderStage2D();
		}
	}

	//ensures render offsets don't go beyond their bounds
	if (renderOffsetX < 0)
	{
		clearStage2D();
		clearPlayer2d();
		renderOffsetX = 0;
		fillRenderMatrix();
		renderStage2D();
	}
	else if (renderOffsetX > (STAGE_W - 11)) 
	{
		clearStage2D();
		clearPlayer2d();		
		renderOffsetX = (STAGE_W - 11);
		fillRenderMatrix();
		renderStage2D();
	}
	
	if (renderOffsetY < 0)
	{
		clearStage2D();
		clearPlayer2d();		
		renderOffsetY = 0;
		fillRenderMatrix();
		renderStage2D();
	}
	else if (renderOffsetY > (STAGE_H - 7))
	{
		clearStage2D();
		clearPlayer2d();		
		renderOffsetY = (STAGE_H - 7);
		fillRenderMatrix();
		renderStage2D();		
	}

}

void initStage()
{
	// initializes stage matrix
	
	// Draw Borders
	v_u32 x,y;

	for(x = 0; x <=  STAGE_W; ++x)
	{
		for( y = 0; y <= STAGE_H; ++y)
		{
			renderMatrix[x][y] = stage[x + renderOffsetX][y + renderOffsetY];		
		}
	}

	for(x = 0; x <= STAGE_W; ++x)
	{

		for( y = 0; y <= STAGE_H; ++y)
		{

			if (x == 0)
			{
				stage[x][y] = 1;
			}
			else if (x == 14)
			{
				stage[x][y] = 1;
			}
			else if (y == 0)
			{
				stage[x][y] = 1;
			}
			else if (y == 14)
			{
				stage[x][y] = 1;
			}			
		}
	}

	stage[3][4] = 1;

}

unsigned short setColor(unsigned char a_red, unsigned char a_green, unsigned char a_blue)
{
	//Sets specific color to specified position in memory
	return (a_red & 0x1F) | (a_green & 0x1F) << 5 | (a_blue & 0x1F) << 10;
}

void clearTile2D(v_u32 xPos, v_u32 yPos)
{
	// Draws tiles in the 2-D plane
	//(Py pos it bottom-left indexed)
	v_u32 x,y;
	for(x = xPos; x < (xPos + TILE_SIZE - 1); ++x){

		for( y = yPos; y < (yPos + TILE_SIZE - 1); ++y){

			SCREENBUFFER[x+y*SCREEN_W] = setColor(10,10,10);

		}
	}

}

void clearStage2D()
{
	v_u32 x,y;

	for(x = 0; x <= (240/TILE_SIZE) - 1; ++x)
	{
		for(y = 0; y <= (160/TILE_SIZE) - 1; ++y)
		{
			if (renderMatrix[x][y] == 1 || renderMatrix[x][y] == 2)
			{
				clearTile2D(x*TILE_SIZE,y*TILE_SIZE);
			}
		}
	}
}

void drawTile2D(v_u32 xPos, v_u32 yPos, v_u32 type)
{
	// Draws tiles in the 2-D plane
	//(Py pos it bottom-left indexed)
	//type = 1 indicates wall tile, 2 indicating goal tile
	v_u32 x,y;

	switch(type)
	{
		case 1:
			
		for(x = xPos; x < (xPos + TILE_SIZE - 1); ++x){
			for( y = yPos; y < (yPos + TILE_SIZE - 1); ++y){

			SCREENBUFFER[x+y*SCREEN_W] = setColor(0x1F,x,y);

			}
		}

			break;

		case 2:
			
		for(x = xPos; x < (xPos + TILE_SIZE - 1); ++x){
			for( y = yPos; y < (yPos + TILE_SIZE - 1); ++y){

			SCREENBUFFER[x+y*SCREEN_W] = setColor(y,x,0);

			}
		}

			break;

	}

}


void renderStage2D()
{
	v_u32 x,y;

	for(x = 0; x <= (240/TILE_SIZE) - 1; ++x)
	{
		for(y = 0; y <= (160/TILE_SIZE) - 1; ++y)
		{
			if (renderMatrix[x][y] == 1)
			{
				drawTile2D(x*TILE_SIZE,y*TILE_SIZE,1);
			}

			switch(renderMatrix[x][y])			
			{
				case 1:

					drawTile2D(x*TILE_SIZE,y*TILE_SIZE,1);

					break;

				case 2:

					drawTile2D(x*TILE_SIZE,y*TILE_SIZE,2);

					break;

			}
		}
	}
}

void drawplayer2D()
{
	// Draws the player in the 2-d plane (For Debugging)
	// Player will be a 5x5 green square
	//(Py pos it top-left indexed)
	v_u32 x,y;
	for(x = (px - renderOffsetX*TILE_SIZE); x < (px - renderOffsetX*TILE_SIZE + 4); ++x){

		for( y = (py - renderOffsetY*TILE_SIZE); y < (py - renderOffsetY*TILE_SIZE + 4); ++y){

			SCREENBUFFER[x+y*SCREEN_W] = setColor(0,0x1F,0);

		}
	}


}

void clearPlayer2d()
{
	//Clears sets the pixels the player is standing on to the BG color
	v_u32 x,y;
	for(x = (px - renderOffsetX*TILE_SIZE); x < (px - renderOffsetX*TILE_SIZE + 4); ++x){

		for( y = (py - renderOffsetY*TILE_SIZE); y < (py - renderOffsetY*TILE_SIZE + 4); ++y){

			SCREENBUFFER[x+y*SCREEN_W] = setColor(10,10,10);

		}
	}
}

void drawBG2D()
{
	// Draws Grey BG in 2D (for Debugging)
		v_u32 x,y;

		for(x = 0; x < SCREEN_W; ++x){

			for( y = 0; y < SCREEN_H; ++y){

				SCREENBUFFER[x+y*SCREEN_W] = setColor(10,10,10);

			}
		}	

}

void vSync()
{
	while(REG_VCOUNT >= SCREEN_H);
	while(REG_VCOUNT < SCREEN_H);
}

void init()
{
	// initializes the player position and set rednder offset
	px = 30;
	py = 30;
	renderOffsetX = 0;
	renderOffsetY = 0;

}

bool tileCollide(v_u32 xPos, v_u32 yPos, volatile int dir)
{
	// Splits map up into a grid, each line 20 px apart.
	// direction can be used to check what direction to look for collision from player.
	//	0 - Left
	//	1 - Right
	//	2 - Down
	//	3 - Up
	//	4+ - Don't check direction (Can be used for power ups and the like)

	switch(dir)
	{

		case 0: // Left

			if (stage[(xPos/TILE_SIZE)][yPos/TILE_SIZE] == 1)
			{
				return true;
			}

			break;

		case 1:  // Right

			if (stage[((xPos + 4)/TILE_SIZE)][yPos/TILE_SIZE] == 1)
			{
				return true;
			}

			break; 

		case 2: 

			if (stage[((xPos)/TILE_SIZE)][(yPos)/TILE_SIZE] == 1)
			{
				return true;
			}

			break; // Down

		case 3: 

			if (stage[(xPos/TILE_SIZE)][(yPos + 4)/TILE_SIZE] == 1)
			{
				return true;
			}

			break; // Up

		default:

			if ((xPos/TILE_SIZE) % 20 == 1 || ((yPos/TILE_SIZE) % 20 == 1)) // used for left and down
			{

			if (stage[xPos/TILE_SIZE][yPos/TILE_SIZE] == 1)
				{
					return true;
				}

			if (stage[xPos/TILE_SIZE][yPos/TILE_SIZE] == 1)
				{
					return true;
				}

			}

			return false;
			break;

	}

	return false;

}

void getInput()
{

	if ((REG_KEYINPUT & (1 << 0)) ==  0) // A button is pressed 
	{

	}

	if ((REG_KEYINPUT & (1 << 1)) ==  0) // B button is pressed 
	{

	}

	if ((REG_KEYINPUT & (1 << 3)) ==  0) // A button is pressed 
	{

	}

	if ((REG_KEYINPUT & (1 << 4)) ==  0) // Right button is pressed 
	{
		
		if (tileCollide(px,py, 1) == false)
		{
			px = px + 1;
		}

	}

	if ((REG_KEYINPUT & (1 << 5)) ==  0) // Left button is pressed 
	{
		if (tileCollide(px,py, 0) == false)
		{
			px = px - 1;
		}
	}

	if ((REG_KEYINPUT & (1 << 6)) ==  0) // Up button is pressed 
	{
		if (tileCollide(px,py, 2) == false)
		{
			py = py - 1;
		}
	}

	if ((REG_KEYINPUT & (1 << 7)) ==  0) // Down button is pressed 
	{
		if (tileCollide(px,py, 3) == false)
		{
			py = py + 1;
		}
	}

	if ((REG_KEYINPUT & (1 << 8)) ==  0) // R button is pressed 
	{

	}

	if ((REG_KEYINPUT & (1 << 9)) ==  0) // L button is pressed 
	{

	}

}

void initStage1()
{
	//Assumes stage array is 15x15
	v_u32 stage1[15][15] = {

		{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1},
		{1,0,0,0,0,0,0,0,0,0,0,1,1,1,1},
		{1,1,0,1,1,0,1,1,1,1,0,1,1,1,1},
		{1,1,0,1,1,0,0,1,1,1,0,0,0,0,1},
		{1,0,0,0,1,1,0,0,0,1,1,1,1,0,1},
		{1,0,1,0,0,1,1,1,1,1,1,0,0,0,1},
		{1,0,0,1,0,1,1,1,0,0,0,0,1,1,1},
		{1,1,0,1,0,0,1,1,0,1,0,1,1,0,1},
		{1,1,0,1,0,0,0,1,0,1,0,1,1,0,1},
		{1,1,0,1,1,1,1,0,0,1,0,0,0,0,1},
		{1,1,1,0,1,1,1,0,1,1,0,1,1,1,1},
		{1,1,1,0,0,0,0,0,1,1,0,1,1,1,1},
		{1,1,1,0,1,1,1,1,1,1,1,1,1,1,1},
		{1,1,1,0,0,0,0,0,0,0,0,0,0,2,1},
		{1,1,1,1,1,1,1,1,1,1,1,1,1,1,1}

	};

	v_u32 x;
	v_u32 y;

	for(x = 0; x <=  STAGE_W; ++x)
	{
		for( y = 0; y <= STAGE_H; ++y)
		{

			stage[x][y] = stage1[y][x];	

		}
	}

	// initializes the player position and set rednder offset
	px = 30;
	py = 30;
	renderOffsetX = 0;
	renderOffsetY = 0;

}

int main()
{
	//set GBA rendering context to MODE 3 Bitmap Rendering
	REG_DISPCNT = VIDEOMODE_3 | BG_ENABLE2;

	drawBG2D();
	init(); // initilizes player pos
//	initStage(); // initialyzes stage
//	fillRenderMatrix();
//	renderStage2D();	

	initStage1();
	fillRenderMatrix();
	renderStage2D();

	while(1){

		clearPlayer2d();

		getInput();

		drawplayer2D();

		setOffSet();		

		vSync();


	}
	return 0;
}