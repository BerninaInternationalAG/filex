/* This FileX test concentrates on the directory rename operations.  */

#ifndef FX_STANDALONE_ENABLE
#include   "tx_api.h"
#endif
#include   "fx_api.h"
#include   <stdio.h>
#include   "fx_ram_driver_test.h"

#ifdef FX_ENABLE_EXFAT

#define     DEMO_STACK_SIZE         8192
#define     CACHE_SIZE              FX_EXFAT_SECTOR_SIZE


/* Define the ThreadX and FileX object control blocks...  */

#ifndef FX_STANDALONE_ENABLE
static TX_THREAD                ftest_0;
#endif
static FX_MEDIA                 ram_disk;
static FX_FILE                  probe_file;


/* Define the counters used in the test application...  */

#ifndef FX_STANDALONE_ENABLE
static UCHAR                    *ram_disk_memory;
static UCHAR                    *cache_buffer;
#else
static UCHAR                    cache_buffer[CACHE_SIZE];
#endif


/* Define thread prototypes.  */

void    filex_directory_rename_exfat_application_define(void *first_unused_memory);
static void    ftest_0_entry(ULONG thread_input);
static UINT    path_probe(CHAR *file_path);

VOID  _fx_ram_driver(FX_MEDIA *media_ptr);
void  test_control_return(UINT status);



/* Define what the initial system looks like.  */

#ifdef CTEST
void test_application_define(void *first_unused_memory)
#else
void    filex_directory_rename_exfat_application_define(void *first_unused_memory)
#endif
{

#ifndef FX_STANDALONE_ENABLE
    UCHAR    *pointer;

    
    /* Setup the working pointer.  */
    pointer =  (UCHAR *) first_unused_memory;

    /* Create the main thread.  */
    tx_thread_create(&ftest_0, "thread 0", ftest_0_entry, 0,  
            pointer, DEMO_STACK_SIZE, 
            4, 4, TX_NO_TIME_SLICE, TX_AUTO_START);

    pointer =  pointer + DEMO_STACK_SIZE;

    /* Setup memory for the RAM disk and the sector cache.  */
    cache_buffer =  pointer;
    pointer =  pointer + CACHE_SIZE;
    ram_disk_memory =  pointer;

#endif

    /* Initialize the FileX system.  */
    fx_system_initialize();
#ifdef FX_STANDALONE_ENABLE
    ftest_0_entry(0);
#endif
}



/* Open and close the file at file_path to report whether that path
   resolves on this media.  */
static UINT    path_probe(CHAR *file_path)
{

UINT status;

    status =  fx_file_open(&ram_disk, &probe_file, file_path, FX_OPEN_FOR_READ);
    if (status == FX_SUCCESS)
    {
        fx_file_close(&probe_file);
    }

    return(status);
}


/* Define the test threads.  */

static void    ftest_0_entry(ULONG thread_input)
{

UINT        status;
#ifndef FX_MEDIA_STATISTICS_DISABLE
ULONG       entry_writes_before;
#endif

    FX_PARAMETER_NOT_USED(thread_input);

    /* Print out some test information banners.  */
    printf("FileX Test:   exFAT Directory rename test............................");

    /* Format the media.  This needs to be done before opening it!  */
    status =  fx_media_exFAT_format(&ram_disk, 
                            _fx_ram_driver,         // Driver entry
                            ram_disk_memory,        // RAM disk memory pointer
                            cache_buffer,           // Media buffer pointer
                            CACHE_SIZE,             // Media buffer size 
                            "MY_RAM_DISK",          // Volume Name
                            1,                      // Number of FATs
                            0,                      // Hidden sectors
                            256,                    // Total sectors 
                            FX_EXFAT_SECTOR_SIZE,   // Sector size   
                            1,                      // exFAT Sectors per cluster
                            12345,                  // Volume ID
                            0);                     // Boundary unit

    /* Determine if the format had an error.  */
    if (status)
    {

        printf("ERROR!\n");
        test_control_return(2);
    }

    /* Open the ram_disk.  */
    status =  fx_media_open(&ram_disk, "RAM DISK", _fx_ram_driver, ram_disk_memory, cache_buffer, CACHE_SIZE);

    /* Check the status.  */
    if (status != FX_SUCCESS)
    {

        /* Error, return error code.  */
        printf("ERROR!\n");
        test_control_return(3);
    }

    /* Simple 8.3 rename in the root directory.  */
    status =  fx_directory_create(&ram_disk, "MYTEST");
    status += fx_directory_rename(&ram_disk, "MYTEST", "OURTEST");
    
    /* Check the directory rename status.  */
    if (status != FX_SUCCESS)
    {

        /* Error renaming directory.  Return to caller.  */
        printf("ERROR!\n");
        test_control_return(4);
    }

    /* Now create a sub-directory in the "OURTEST" directory.  */
    status += fx_directory_create(&ram_disk, "OURTEST/newdir");
    status += fx_directory_rename(&ram_disk, "OURTEST/newdir", "newnewdir");
    status += fx_directory_rename(&ram_disk, "newnewdir", "OURTEST/newdir");
    status += fx_directory_create(&ram_disk, "/OURTEST/newdir/finaltest");
    
    /* Check the directory rename status.  */
    if (status != FX_SUCCESS)
    {

        /* Error renaming/moving directory.  Return to caller.  */
        printf("ERROR!\n");
        test_control_return(5);
    }

    /* An exFAT directory has no "." / ".." entries: the parent directory
       is not recorded inside the directory at all - the entries at index
       0 and 1 are records of the entry sets the directory contains - and
       a ".." path component consequently does not resolve on exFAT
       media. Moving a directory to a different parent therefore changes
       the two parent entries only; nothing inside the moved directory
       needs an update, which is why maintaining the ".." entry is a
       FAT-only operation. Walk every parent transition - between
       sub-directories, into and out of the root, plus a rename within
       one parent - and verify that the move works, that the moved
       content follows it, and that no FAT directory entry is written at
       all (_fx_directory_entry_write is the FAT-only writer and the only
       one counted in fx_media_directory_entry_writes).  */

    /* Move an empty directory to a different parent directory.  */
#ifndef FX_MEDIA_STATISTICS_DISABLE
    entry_writes_before =  ram_disk.fx_media_directory_entry_writes;
#endif
    status =  fx_directory_rename(&ram_disk, "/OURTEST/newdir/finaltest", "/OURTEST/finaltest");
    return_if_fail( status == FX_SUCCESS);
#ifndef FX_MEDIA_STATISTICS_DISABLE
    return_if_fail( ram_disk.fx_media_directory_entry_writes == entry_writes_before);
#endif

    /* Put a marker file inside the directory. A path through ".." does
       not resolve on exFAT media - there is no such entry to follow.  */
    status =  fx_file_create(&ram_disk, "/OURTEST/finaltest/MARK.TXT");
    return_if_fail( status == FX_SUCCESS);
    status =  path_probe("/OURTEST/finaltest/MARK.TXT");
    return_if_fail( status == FX_SUCCESS);
    status =  path_probe("/OURTEST/finaltest/../OURTEST/finaltest/MARK.TXT");
    return_if_fail( status == FX_NOT_FOUND);

    /* Move the directory into the root directory.  */
#ifndef FX_MEDIA_STATISTICS_DISABLE
    entry_writes_before =  ram_disk.fx_media_directory_entry_writes;
#endif
    status =  fx_directory_rename(&ram_disk, "/OURTEST/finaltest", "/finaltest");
    return_if_fail( status == FX_SUCCESS);
#ifndef FX_MEDIA_STATISTICS_DISABLE
    return_if_fail( ram_disk.fx_media_directory_entry_writes == entry_writes_before);
#endif
    status =  path_probe("/finaltest/MARK.TXT");
    return_if_fail( status == FX_SUCCESS);

    /* Move it back out of the root directory.  */
#ifndef FX_MEDIA_STATISTICS_DISABLE
    entry_writes_before =  ram_disk.fx_media_directory_entry_writes;
#endif
    status =  fx_directory_rename(&ram_disk, "/finaltest", "/OURTEST/newdir/finaltest");
    return_if_fail( status == FX_SUCCESS);
#ifndef FX_MEDIA_STATISTICS_DISABLE
    return_if_fail( ram_disk.fx_media_directory_entry_writes == entry_writes_before);
#endif
    status =  path_probe("/OURTEST/newdir/finaltest/MARK.TXT");
    return_if_fail( status == FX_SUCCESS);

    /* A rename within one parent directory - no parent change at all.  */
#ifndef FX_MEDIA_STATISTICS_DISABLE
    entry_writes_before =  ram_disk.fx_media_directory_entry_writes;
#endif
    status =  fx_directory_rename(&ram_disk, "/OURTEST/newdir/finaltest", "/OURTEST/newdir/final2");
    return_if_fail( status == FX_SUCCESS);
#ifndef FX_MEDIA_STATISTICS_DISABLE
    return_if_fail( ram_disk.fx_media_directory_entry_writes == entry_writes_before);
#endif
    status =  path_probe("/OURTEST/newdir/final2/MARK.TXT");
    return_if_fail( status == FX_SUCCESS);

    /* Still no ".." to follow after the moves.  */
    status =  path_probe("/OURTEST/newdir/final2/../final2/MARK.TXT");
    return_if_fail( status == FX_NOT_FOUND);

    
    /* Close the media.  */
    status =  fx_media_close(&ram_disk);

    /* Determine if the test was successful.  */
    if (status != FX_SUCCESS) 
    {

        printf("ERROR!\n");
        test_control_return(6);
    }
    else
    {

        printf("SUCCESS!\n");
        test_control_return(0);
    }
}

#endif /* FX_ENABLE_EXFAT */
