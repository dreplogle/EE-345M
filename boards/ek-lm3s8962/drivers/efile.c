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
#define MAX_NUM_FILES 10
FIL * files[MAX_NUM_FILES];
FIL *fp;

int eFile_Init(void) // initialize file system
{
  FATFS *fs;
  // Initilize values in fs
	fs->id = 1;				/* File system mount ID */
	fs->n_rootdir = 0;		/* Number of root directory entries */
	fs->winsect = 0;		/* Current sector appearing in the win[] */
	fs->fatbase = 0;		/* FAT start sector */
	fs->dirbase = 0;		/* Root directory start sector */
	fs->database = 10;		/* Data start sector */
	fs->sects_fat = 1000;		/* Sectors per fat */
//	fs->max_clust;		/* Maximum cluster# + 1 */
//#if !_FS_READONLY
//	CLUST	last_clust;		/* Last allocated cluster */
//	CLUST	free_clust;		/* Number of free clusters */
//#if _USE_FSINFO
//	DWORD	fsi_sector;		/* fsinfo sector */
//	BYTE	fsi_flag;		/* fsinfo dirty flag (1:must be written back) */
//	BYTE	pad1;
//#endif
//#endif
//	BYTE	fs_type;		/* FAT sub type */
//	BYTE	sects_clust;	/* Sectors per cluster */
//	BYTE	n_fats;			/* Number of FAT copies */
//	BYTE	winflag;		/* win[] dirty flag (1:must be written back) */
//	BYTE	win[512];		/* Disk access window for Directory/FAT/File */
  
    f_mount(0, fs);   // assign initialized FS object to FS pointer on drive 0 
  
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
  FIL* newFile; 
	
  if(f_open(newFile, name, FA_CREATE_NEW))    //params: empty FP, path ptr, mode
  {
    return 1;
  }
  return 0;
}

//---------- eFile_WOpen-----------------
// Open the file, read into RAM last block
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WOpen(char name[])      // open a file for writing 
{

}
//---------- eFile_Write-----------------
// save at end of the open file
// Input: data to be saved
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Write( char data)  
{

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
  FRESULT res = fclose(fp);			/* Open or create a file */
  if(res)
  {
    return 1; 
  }
  return 0; 
}
//---------- eFile_ROpen-----------------
// Open the file, read first block into RAM 
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble read to flash)
int eFile_ROpen( char name[])      // open a file for reading 
{

}   
//---------- eFile_ReadNext-----------------
// retreive data from open file
// Input: none
// Output: return by reference data
//         0 if successful and 1 on failure (e.g., end of file)
int eFile_ReadNext( char *pt)       // get next byte 
{
   WORD * numBytesRead;
   WORD numBytesToRead = 1;
   f_read(fp, pt, numBytesToRead, numBytesRead);			/* Read data from a file */
   if(numBytesRead == &numBytesToRead)
   {
      return 0;
   }
   return 1;
}                              
//---------- eFile_RClose-----------------
// close the reading file
// Input: none
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_RClose(void) // close the file for writing
{
  FRESULT res = fclose(fp);			/* Open or create a file */
  if(res)
  {
    return 1; 
  }
  return 0; 
}
//---------- eFile_Directory-----------------
// Display the directory with filenames and sizes
// Input: pointer to a function that outputs ASCII characters to display
// Output: characters returned by reference
//         0 if successful and 1 on failure (e.g., trouble reading from flash)
int eFile_Directory(void(*fp)(unsigned char))   
{

}
//---------- eFile_Delete-----------------
// delete this file
// Input: file name is a single ASCII letter
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Delete( char name[])  // remove this file 
{
  FRESULT res = f_unlink(name);   /* Delete an existing file or directory */
  if(res)
  {
    return 1; 
  }
  return 0;  						
}
//---------- eFile_RedirectToFile-----------------
// open a file for writing 
// Input: file name is a single ASCII letter
// stream printf data into file
// Output: 0 if successful and 1 on failure (e.g., trouble read/write to flash)
int eFile_RedirectToFile(char *name)
{
  FRESULT res = f_open(fp, name, FA_WRITE);			/* Open or create a file */
  if(res)
  {
    return 1; 
  }
  return 0; 
  
}
//---------- eFile_EndRedirectToFile-----------------
// close the previously open file
// redirect printf data back to UART
// Output: 0 if successful and 1 on failure (e.g., wasn't open)
int eFile_EndRedirectToFile(void)
{
  FRESULT res = fclose(fp);			/* Open or create a file */
  if(res)
  {
    return 1; 
  }
  return 0; 
}