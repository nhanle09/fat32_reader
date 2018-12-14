#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string>
#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <ctype.h>
#include <csignal>
#include <cstring>
#include <algorithm>
#include <fstream>
#include <stack>

using namespace std;


FILE *fp = NULL;

#define WHITESPACE " \t\n"      // We want to split our command line up into tokens
// so we need to define what delimits our tokens.
// In this case  white space
// will separate the tokens on our command line
#define MAX_COMMAND_SIZE 255    // The maximum command-line size
#define MAX_NUM_ARGUMENTS 5     // Mav shell only supports five arguments

#define BPB_BytesPerSec_Offset 11
#define BPB_BytesPerSec_Size 2

#define BPB_SecPerClus_Offset 13
#define BPB_SecPerClus_Size 1

#define BPB_RsvdSecCnt_Offset 14
#define BPB_RsvdSecCnt_Size 2

#define BPB_NumFATs_Offset 16
#define BPB_NumFATs_Size 1

#define BPB_RootEntCnt_Offset 17
#define BPB_RootEntCnt_Size 2

#define BPB_FATSz32_Offset 36
#define BPB_FATSz32_Size 4

#define BPB_VolLab_Offset 71
#define BPB_VolLab_Size 11


struct __attribute__((__packed__)) DirectoryEntry
{
    char DIR_Name[11]; 
    uint8_t DIR_Attr; 
    uint8_t Unused1[8];
    uint16_t DIR_FirstClusterHigh;
    uint8_t Unused2[4]; 
    uint16_t DIR_FirstClusterLow;
    uint32_t DIR_FileSize;
};

struct DirectoryEntry dir[16];

uint16_t    BPB_RsvdSecCnt;
uint16_t    BPB_BytesPerSec;
uint8_t     BPB_SecPerClus;
uint32_t    BPB_FATSz32;
uint8_t     BPB_NumFATs;
uint16_t    BPB_RootEntCnt;
uint32_t    RootClusAddess;
uint32_t    currDir;

// custom variables
bool fileOpened = false;
char BS_VolLab[11];


//Takes you to next cluster
int16_t nextLB(uint32_t sector)
{
    uint32_t FATAddress = (BPB_BytesPerSec * BPB_RsvdSecCnt) + (sector*4);
    int16_t val;
    fseek(fp, FATAddress, SEEK_SET);
    fread(&val, 2,1, fp);
    return val;
}

//Turns cluster into location
int LBAToOffset( uint32_t sector )
{
return ( ( sector - 2 ) * BPB_BytesPerSec ) + ( BPB_BytesPerSec * BPB_RsvdSecCnt ) + ( BPB_BytesPerSec * BPB_NumFATs * BPB_FATSz32 ) ;
}

void getDirInfo()
{
     //clears previous values within directory
    memset(dir,'\0',sizeof(dir));

    //move file pointer towards current directory
    fseek(fp, currDir, SEEK_SET);

    //itearate through 16 times to collect all files and directories within currentDirectory
    for (int i = 0; i < 16; i++)
    {
        fread(&dir[i], sizeof( struct DirectoryEntry), 1, fp);
    }
}

int main(int argc, char *argv[])
{
    
    char * cmd_str = (char*) malloc( MAX_COMMAND_SIZE );
    
    while( 1 )
    {
        // Print out the msh prompt
        printf ("mfs> ");
        
        // Read the command from the commandline.  The
        // maximum command that will be read is MAX_COMMAND_SIZE
        // This while command will wait here until the user
        // inputs something since fgets returns NULL when there
        // is no input
        while( !fgets (cmd_str, MAX_COMMAND_SIZE, stdin) );
        
        /* Parse input */
        char *token[MAX_NUM_ARGUMENTS];
        
        int   token_count = 0;
        
        // Pointer to point to the token
        // parsed by strsep
        char *arg_ptr;
        char *working_str  = strdup( cmd_str );
        
        // we are going to move the working_str pointer so
        // keep track of its original value so we can deallocate
        // the correct amount at the end
        char *working_root = working_str;
        
        // Tokenize the input strings with whitespace used as the delimiter
        while ( ( (arg_ptr = strsep(&working_str, WHITESPACE ) ) != NULL) &&
            (token_count<MAX_NUM_ARGUMENTS))
        {
            token[token_count] = strndup( arg_ptr, MAX_COMMAND_SIZE );
            if( strlen( token[token_count] ) == 0 )
            {
                token[token_count] = NULL;
            }
            token_count++;
        }
        
        // Does nothing on blank input
        if (token[0] == NULL)
        {   continue; }
        // Exit mfs on "quit" or "exit"
        if (strcmp(token[0], "quit") == 0 || strcmp(token[0], "exit") == 0)
        {
            free( working_root);
            exit(0);
        }
        // condition to get stat for files folder
        if (strcmp(token[0], "stat") == 0)
        {
            // declare variable(s)
            bool file_exist = false;
            // condition if file has not been open
            if (!fileOpened)
            {
                printf("Error: File system image must be opened first.\n");
                continue;
            }
            // condition if user did not input file/folder name
            if (token[1] == NULL) continue;
            // convert user file/folder input into uppercase
            string token_input = token[1];
            std::transform(token_input.begin(), token_input.end(), token_input.begin(), ::toupper);
            // iterate through all 16 files
            for(int i = 0; i < 16; i++)
            {
                // variable declaration and copying each file/folder into name char array
                // get both file and their extension, remove spaces from file name
                // and add the extension back to the file name then convert it to uppercase
                char name[9];
                char extension[4];
                memcpy(name, dir[i].DIR_Name, 8);
                name[8] = '\0';
                string fname = name;
                fname.erase(remove_if(fname.begin(), fname.end(), ::isspace), fname.end());
                memcpy(extension, &dir[i].DIR_Name[8], 3);
                extension[3] = '\0';
                string fext = extension;
                if (strstr(token[1], ".") != NULL)
                {
                    fname = fname + "." + fext;
                }
                std::transform(fname.begin(), fname.end(), fname.begin(), ::toupper);
                // if user input file matches then print out the stats from the file
                if ( (dir[i].DIR_Attr == 0x01 || dir[i].DIR_Attr == 0x20 || dir[i].DIR_Attr == 0x10) && fname.compare(token_input) == 0)
                {
                    file_exist = true;
                    printf("Attribute\tSize\tStarting Cluster Number\n");
                    printf("%d\t\t%d\t%d\n", dir[i].DIR_Attr, dir[i].DIR_FileSize, dir[i].DIR_FirstClusterLow);
                }
            }
            // print out status if file/folder not found to get stat from
            if (file_exist == false)
            {
                printf("Error: File not found\n");
            }
        }
        // condition to get file and put it to current directory
        if (strcmp(token[0], "get") == 0)
        {
            // condition if file has not been open
            if (!fileOpened)
            {
                printf("Error: File system image must be opened first.\n");
                continue;
            }
            // condition if file has not been open
            if (token[1] == NULL) continue;
            // convert user file/folder input into uppercase
            string token_input = token[1];
            // convert user file/folder input into uppercase
            std::transform(token_input.begin(), token_input.end(), token_input.begin(), ::toupper);
            // iterate through 16 files
            for(int i = 0; i < 16; i++)
            {
                // variable declaration and copying each file/folder into name char array
                // get both file and their extension, remove spaces from file name
                // and add the extension back to the file name then convert it to uppercase
                char name[9];
                char extension[4];
                memcpy(name, dir[i].DIR_Name, 8);
                memcpy(extension, &dir[i].DIR_Name[8], 3);
                name[8] = '\0';
                extension[3] = '\0';
                string fname = name;
                fname.erase(remove_if(fname.begin(), fname.end(), ::isspace), fname.end());
                fname = fname + "." + extension;
                std::transform(fname.begin(), fname.end(), fname.begin(), ::toupper);
                // condition for when the file is found
                if ( (dir[i].DIR_Attr == 0x01 || dir[i].DIR_Attr == 0x20) && fname.compare(token_input) == 0)
                {
                    // declaring variables
                    FILE *file = fopen(fname.c_str(), "w");
                    int curSector = dir[i].DIR_FirstClusterLow;
                    // iterate through all blocks until return -1 from nextLB
                    while(curSector != -1)
                    {
                        // seek to the current cluster address, read it into a variable
                        // and write the information of those file into a new file
                        // 512 bytes at the time then move on to the next cluster
                        uint8_t read_file[512];
                        fseek(fp, LBAToOffset(curSector), SEEK_SET);
                        fread( &read_file, 512, 1, fp);
                        fwrite(read_file, 1, 512, file);
                        curSector = nextLB(curSector);
                    }
                    fclose(file);
                }
            }
        }
        // condition to read a file in current directory
        if (strcmp(token[0], "read") == 0)
        {
            if (!fileOpened)
            {
                printf("Error: File system image must be opened first.\n");
                continue;
            }
            if (token[1] == NULL || token[2] == NULL || token[3] == NULL) continue;
            string token_input = token[1];
            std::transform(token_input.begin(), token_input.end(), token_input.begin(), ::toupper);
            for(int i = 0; i < 16; i++)
            {
                char name[9];
                char extension[4];
                memcpy(name, dir[i].DIR_Name, 8);
                memcpy(extension, &dir[i].DIR_Name[8], 3);
                name[8] = '\0';
                extension[3] = '\0';
                string fname = name;
                fname.erase(remove_if(fname.begin(), fname.end(), ::isspace), fname.end());
                fname = fname + "." + extension;
                std::transform(fname.begin(), fname.end(), fname.begin(), ::toupper);
                if ( (dir[i].DIR_Attr == 0x01 || dir[i].DIR_Attr == 0x20) && fname.compare(token_input) == 0)
                {
                    uint8_t test_read;
                    fseek(fp, LBAToOffset(dir[i].DIR_FirstClusterLow) + atoi(token[2]), SEEK_SET);
                    for (int j = 0; j < atoi(token[3]) ; j++)
                    {
                        fread( &test_read, 1, 1, fp);
                        printf("%x ", test_read);
                    }
                    printf("\n");
                }
            }
        }
        //handle open command to open img file
        if (strcmp(token[0], "open") == 0)
        {
            //check if file is already opened or not
            if (fileOpened)
            {
                printf("File is already opened.\n");
                continue;
            }
            fp = fopen(token[1], "r");

            if (!fp)
            {
                perror("Error openning file:");
                continue;
            }
            fileOpened = true;
            // seek and read all the information from reserved volume
            fseek( fp, BPB_BytesPerSec_Offset, SEEK_SET);
            fread( &BPB_BytesPerSec, BPB_BytesPerSec_Size, 1, fp);
            
            fseek(fp, BPB_SecPerClus_Offset, SEEK_SET);
            fread( &BPB_SecPerClus, BPB_SecPerClus_Size, 1, fp);
            
            fseek(fp, BPB_RsvdSecCnt_Offset, SEEK_SET);
            fread( &BPB_RsvdSecCnt, BPB_RsvdSecCnt_Size, 1, fp);            
            
            fseek(fp, BPB_NumFATs_Offset, SEEK_SET);
            fread( &BPB_NumFATs, BPB_NumFATs_Size, 1, fp);
            
            fseek(fp, BPB_FATSz32_Offset, SEEK_SET);
            fread( &BPB_FATSz32, BPB_FATSz32_Size, 1, fp);
            
            fseek(fp, BPB_VolLab_Offset, SEEK_SET);
            fread( &BS_VolLab, BPB_VolLab_Size, 1, fp);

            fseek(fp, BPB_RootEntCnt_Offset, SEEK_SET);
            fread( &BPB_RootEntCnt, BPB_RootEntCnt_Size, 1, fp);

            //Calculate the address of the root directory
            RootClusAddess = (BPB_NumFATs * BPB_FATSz32 * BPB_BytesPerSec) + (BPB_RsvdSecCnt * BPB_BytesPerSec);
            //Points to root directory            
            fseek(fp,RootClusAddess,SEEK_SET);
            currDir = RootClusAddess;

            
            for (int i = 0; i < 16; i++)
            {
                fread(&dir[i], sizeof( struct DirectoryEntry), 1, fp);
            }
            continue;
        }
        // handle info session
        if (strcmp(token[0], "info") == 0)
        {
            if (!fileOpened)
            {
                printf("Error: File system image must be opened first.\n");
                continue;
            }
            // print out all the reserved information from reserved volume
            printf("BPB_BytesPerSec: %d\n", BPB_BytesPerSec);
            printf("BPB_BytesPerSec: %x\n\n", BPB_BytesPerSec);
            
            printf("BPB_SecPerClus: %d\n", BPB_SecPerClus);
            printf("BPB_SecPerClus: %x\n\n", BPB_SecPerClus);
            
            printf("BPB_RsvdSecCnt: %d\n", BPB_RsvdSecCnt);
            printf("BPB_RsvdSecCnt: %x\n\n", BPB_RsvdSecCnt);
            
            printf("BPB_NumFATs: %d\n", BPB_NumFATs);
            printf("BPB_NumFATs: %x\n\n", BPB_NumFATs);

            printf("BPB_FATSz32: %d\n", BPB_FATSz32);
            printf("BPB_FATSz32: %x\n\n", BPB_FATSz32);
            continue;
        }
        //close the fat32 file 
        if (strcmp(token[0], "close") == 0)
        {
            if (fp)
            {
                fclose(fp); 
            } else 
            {
                printf("Already  closed\n");
            }
            continue;
        }
        // handle ls command
        if (strcmp(token[0], "ls") == 0)
        {
            if (!fileOpened)
            {
                printf("Error: File system image must be opened first.\n");
                continue;
            }
            getDirInfo();
            // store temp address so we can handle if user input .. 
            uint32_t temp = currDir;
            
            // check if the command ls .. and handle accordingly
            if (token[1] != NULL) {
                if (strcmp(token[1],"..")== 0){
                    if (dir[1].DIR_FirstClusterLow == 0)
                    {
                        temp = LBAToOffset(dir[1].DIR_FirstClusterLow + 2);
                    }
                    else
                    {
                        temp = LBAToOffset(dir[1].DIR_FirstClusterLow);
                    }
                } 
            }
            
            fseek(fp,temp,SEEK_SET);
            for(int i = 0; i < 16; i++)
            {
                fread(&dir[i], sizeof( struct DirectoryEntry), 1, fp);
                
                char name[sizeof(dir[i].DIR_Name) + 1];
                memcpy(name, dir[i].DIR_Name, sizeof(dir[i].DIR_Name));
                name[sizeof(dir[i].DIR_Name)] = '\0';

                if((dir[i].DIR_Attr == 0x01 || dir[i].DIR_Attr == 0x10 || dir[i].DIR_Attr == 0x20) && dir[i].DIR_Name[0] != -27)
                {
                    printf("%s\n", name);
                }
            }
            continue;
        }
        // handle change directory command
        if (strcmp(token[0], "cd") == 0)
        {   
            // condition for when file has not been open
            if (!fileOpened)
            {
                printf("Error: File system image must be opened first.\n");
                continue;
            }
            // condition for when user did not input file/folder name
            if (token[1] == NULL) continue;

            bool path_exist = false;
            // check if the token is absolute or relative path
            if(token[1][0] == '/')
            {
                currDir = RootClusAddess;
                getDirInfo();
            }
            // removing "/" character from absolute path
            char* folderName = strtok (token[1],"/");
            while (folderName != NULL)
            {
                //capitalize input token (foldername)
                for (int i=0; i < strlen(folderName); i++){ 
                    folderName[i]=toupper(folderName[i]);
                }
                // handles going to previous directory
                if (strcmp("..", folderName) == 0 && currDir == RootClusAddess)
                {
                    folderName = strtok(NULL, "/");
                    continue;
                }
                // go through the current directory and look for folder name
                for(int i = 0; i < 16; i++)
                {
                    // ignore non-directory file
                    if (dir[i].DIR_Attr != 0x10)
                    {
                        continue;
                    }
                    // parse the file name and remove any empty space
                    // for file shorter than 8 characters
                    char name[9];
                    memcpy(name, dir[i].DIR_Name, 8);
                    name[8] = '\0';
                    string fname = name;
                    fname.erase(remove_if(fname.begin(), fname.end(), ::isspace), fname.end());
                    // change current directory when the folder matches input
                    if (strcmp(fname.c_str(), folderName) == 0)
                    {
                        if (dir[i].DIR_FirstClusterLow == 0)
                        {
                            currDir = LBAToOffset(dir[i].DIR_FirstClusterLow + 2);
                        }
                        else
                        {
                            currDir = LBAToOffset(dir[i].DIR_FirstClusterLow);
                        }
                        path_exist = true;
                        getDirInfo();                                       
                    }
                }
                folderName = strtok (NULL, "/");
            }
            //go back to old dir if the input path end up not exist
            if (!path_exist) 
            {
                printf("Error: Folder not found\n");
            }
            continue;
        }
        // handles volume
        if (strcmp(token[0], "volume") == 0)
        {
            // condition for file has not been already open
            if (!fileOpened)
            {
                printf("Error: File system image must be opened first.\n");
                continue;
            }
            // seek and read the volume file from the cluster
            fseek(fp, BPB_VolLab_Offset, SEEK_SET);
            fread(BS_VolLab, BPB_VolLab_Size, 1, fp);
            
            if(BS_VolLab[0] != '\0')
            {
                printf("Volume Label: \"%s\"\n", BS_VolLab);
            }
            else
            {
                cout << "Error: volume name not found.\n";
            }
        }
        free( working_root );
    }
    return 0;
}

