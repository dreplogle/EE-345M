// ************** eFile.c *****************************
// Middle-level routines to implement a solid-state disk 
// Dustin Replogle and Katy Loeffler  3/21/2011


//---------- eFile_Init-----------------
// Activate the file system, without formating
// Input: none
// Output: 0 if successful and 1 on failure (already initialized)
// since this program initializes the disk, it must run with 
//    the disk periodic task operating

//#define MAX_NUM_FILES 10
#include <string.h>
#include "efile.h"            /* FatFs declarations */
#include "edisk.h"        /* Include file for user provided disk functions */
#include "drivers/tff.h"

//FIL * Files[10];
FIL * Fp;	//Global file pointer, only one file can be open at a time
DIR * Directory;  //Global directory pointer


int eFile_Init(void) // initialize file system
{

  FRESULT res;
  FATFS newFileSystem;
  FATFS *fs = &newFileSystem;
  unsigned short i;
  res = f_mount(0, fs);   // assign initialized FS object to FS pointer on drive 0

	fs->id = 1;				/* File system mount ID */
	fs->n_rootdir = 0;		/* Number of root directory entries */
	fs->winsect = 0;		/* Current sector appearing in the win[] */
	fs->fatbase = 0;		/* FAT start sector */
	fs->dirbase = 0;		/* Root directory start sector */
	fs->database = 40;		/* Data start sector */
	fs->sects_fat = 100000;		/* Sectors per fat */
	fs->max_clust = 11;		/* Maximum cluster# + 1 */
#if !_FS_READONLY
	fs->last_clust = 0;		/* Last allocated cluster */
	fs->free_clust = 10000;		/* Number of free clusters */
#if _USE_FSINFO
//	DWORD	fsi_sector;		/* fsinfo sector */
//	BYTE	fsi_flag;		/* fsinfo dirty flag (1:must be written back) */
//	BYTE	pad1;
#endif
#endif
	fs->fs_type = 1;		/* FAT sub type */
	fs->sects_clust = 10;	/* Sectors per cluster */
	fs->n_fats = 2;			/* Number of FAT copies */
//	BYTE	winflag;		/* win[] dirty flag (1:must be written back) */
	for(i=0; i < 512; i++){
		fs->win[i] = 0;		/* Disk access window for Directory/FAT/File */
	}
  
  res = f_mkdir("Root");
  if(res) return 1;  
  return 0; 
}
//---------- eFile_Format-----------------
// Erase all Files, create blank directory, initialize free space manager
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Format(void) // erase disk, add format
{   
  return 1;   
}
//---------- eFile_Create-----------------
// Create a new, empty file with one allocated block
// Input: file name is an ASCII string up to seven characters 
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_Create( char name[])  // create new file, make it empty 
{
  FRESULT res;
  res = f_open(Fp, name, FA_CREATE_NEW); 
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
  res = f_open(Fp, name, FA_WRITE);     //params: empty Fp, path ptr, mode
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

  res = f_write (Fp, dat, 1, bytesWritten);

  if(res) return 1;
  return 0;
}
//---------- eFile_Close-----------------
// Deactivate the file system
// Input: none
// Output: 0 if successful and 1 on failure (not currently open)
int eFile_Close(void) 
{
 	FRESULT res = f_mount(0, NULL);   // unmount initialized fs
  if(res)
  {
    return 1; 
  }
  return 0; 						
}

//---------- eFile_WClose-----------------
// close the file, left disk in a state power can be removed
// Input: none
// Output: 0 if successful and 1 on failure (e.g., trouble writing to flash)
int eFile_WClose(void) // close the file for writing
{
  FRESULT res = fclose(Fp);			/* Open or create a file */
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
  FRESULT res;
  res = f_open(Fp, name, FA_READ);     //params: empty Fp, path ptr, mode
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
   WORD * numBytesRead;
   WORD numBytesToRead = 1;
   f_read(Fp, pt, numBytesToRead, numBytesRead);			/* Read data from a file */
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
  FRESULT res = fclose(Fp);			/* Open or create a file */
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
int eFile_Directory(void(*Fp)(unsigned char*))   
{
	DIR foundDir;
	DIR *directory = &foundDir;
	f_opendir (directory, "Root");
	return print_dir(Fp, directory);
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
  FRESULT res = f_open(Fp, name, FA_WRITE);			/* Open or create a file */
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
  FRESULT res = fclose(Fp);			/* Open or create a file */
  if(res)
  {
    return 1; 
  }
  return 0; 
}