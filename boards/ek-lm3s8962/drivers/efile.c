// ************** eFile.c *****************************
// Middle-level routines to implement a solid-state disk 
// Dustin Replogle and Katy Loeffler  3/21/2011

//---------- eFile_Init-----------------
// Activate the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure (already initialized)
// since this program initializes the disk, it must run with 
//    the disk periodic task operating

#include <string.h>
#include "efile.h"            /* FatFs declarations */
#include "edisk.h"        /* Include file for user provided disk functions */
#include "drivers/tff.h"

FIL* fp;	//Global file pointer, only one file can be open at a time
DIR* Directory;  //Global directory pointer

int eFile_Init(void) // initialize file system
{
  FATFS *fs;
  FRESULT res;

  // Initilize values in fs
  fs->id = 1;				/* File system mount ID */
  fs->n_rootdir = 1;		/* Number of root directory entries */
  fs->winsect = 0;		/* Current sector appearing in the win[] */
  fs->fatbase = 0;		/* FAT start sector */
  fs->dirbase = 0;		/* Root directory start sector */
  fs->database = 10;		/* Data start sector */
  fs->sects_fat = 1000;		/* Sectors per fat */
  fs->max_clust;		/* Maximum cluster# + 1 */
#if !_FS_READONLY
  fs->last_clust;		/* Last allocated cluster */
  fs->free_clust;		/* Number of free clusters */
#if _USE_FSINFO
  fs->fsi_sector;		/* fsinfo sector */
  fs->fsi_flag;		/* fsinfo dirty flag (1:must be written back) */
  fs->pad1;
#endif
#endif
  fs->fs_type;		/* FAT sub type */
  fs->sects_clust;	/* Sectors per cluster */
  fs->n_fats;			/* Number of FAT copies */
  fs->winflag;		/* win[] dirty flag (1:must be written back) */
  fs->win[512];		/* Disk access window for Directory/FAT/File */
  
  res = f_mount (0, fs);   // assign initialized FS object to FS pointer on drive 0
  if(res == FR_EXIST)
  {
  return 1;				  // filesystem already exists on the disk
  }  
  return 0; 
}
//---------- eFile_Format-----------------
// Erase all files, create blank directory, initialize free space manager
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Format(void) // erase disk, add format
{

}
//---------- eFile_Create-----------------
// Create a new, empty file with one allocated block
// Input: file name is an ASCII string up to seven characters 
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Create( char name[])  // create new file, make it empty 
{
  FRESULT res;
  res = f_open(fp, name, FA_CREATE_NEW); 
  if(res) return 1;
  return 0;
}

//---------- eFile_WOpen-----------------
// Open the file, read into RAM last block
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WOpen(char name[])      // open a file for writing 
{
  FRESULT res;
  res = f_open(fp, name, FA_WRITE);     //params: empty FP, path ptr, mode
  if(res) return 1;
  return 0;	
}
//---------- eFile_Write-----------------
// save at end of the open file
// Input: data to be saved
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Write( char data)  
{
  const void * dat = &data;
  WORD * bytesWritten;
  FRESULT res;

  res = f_write (fp, dat, 1, bytesWritten);

  if(res) return 1;
  return 0;
}
//---------- eFile_Close-----------------
// Deactivate the file system
// Input: none
// Output: 0 if successful and 1 on failure (not currently open)
int eFile_Close(void) 
{

}

//---------- eFile_WClose-----------------
// close the file, left disk in a state power can be removed
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WClose(void) // close the file for writing
{

}
//---------- eFile_ROpen-----------------
// Open the file, read first block into RAM 
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble read to flash)
int eFile_ROpen( char name[])      // open a file for reading 
{
  FRESULT res;
  res = f_open(fp, name, FA_READ);     //params: empty FP, path ptr, mode
  if(res) return 1;
  return 0;	
}   
//---------- eFile_ReadNext-----------------
// retreive data from open file
// Input: none
// Output: return by reference data
//         0 if successful and 1 on failure (e.g., end of file)
int eFile_ReadNext( char *pt)       // get next byte 
{

}                              
//---------- eFile_RClose-----------------
// close the reading file
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_RClose(void) // close the file for writing
{

}
//---------- eFile_Directory-----------------
// Display the directory with filenames and sizes
// Input: pointer to a function that outputs ASCII characters to display
// Output: characters returned by reference
//         0 if successful and 1 on failure (e.g., trouble reading from flash)
int eFile_Directory(void(*fp)(unsigned char))   
{
	BYTE *dir, c;
	FRESULT res;
	FATFS *fs;
	dirobj = Directory;
	fs = dirobj->fs;
	
	while (dirobj->sect) {
		if (!move_window(dirobj->sect))
			return 1;  // error FR_RW_ERROR;
		dir = &fs->win[(dirobj->index & 15) * 32];		/* pointer to the directory entry */
		c = dir[DIR_Name];
		if (c == 0) break;								/* Has it reached to end of dir? */
		if (c != 0xE5 && !(dir[DIR_Attr] & AM_VOL))		/* Is it a valid entry? */
			get_fileinfo(finfo, dir);
		if (!next_dir_entry(dirobj)) dirobj->sect = 0;	/* Next entry */
	}

	return FR_OK;  
}
//---------- eFile_Delete-----------------
// delete this file
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Delete( char name[])  // remove this file 
{

}
//---------- eFile_RedirectToFile-----------------
// open a file for writing 
// Input: file name is a single ASCII letter
// stream printf data into file
// Output: 0 if successful and 1 on failure (e.g., trouble read/write to flash)
int eFile_RedirectToFile(char *name)
{

}
//---------- eFile_EndRedirectToFile-----------------
// close the previously open file
// redirect printf data back to UART
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_EndRedirectToFile(void)
{

}