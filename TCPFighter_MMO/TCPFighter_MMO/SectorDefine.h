#pragma once

struct st_SECTOR_POS
{
	int iX;
	int iY;
};

struct st_SECTOR_AROUND
{
	int iCount;
	st_SECTOR_POS Around[9];
};