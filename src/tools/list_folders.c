/*  */


#include <stdio.h>
#include <stdlib.h>
#include <common/system/folder.h>

int main(int argc,char *argv[])
{


  folder_t folder;
  char buffer[512];
  char *file;
  state_t s;


  strcpy(buffer, argv[1]);
  // Initilizing folder scanning
  s = folder_open(&folder, buffer);

  if (state_fail(s)) {
    printf("Error opening folder %s\n", buffer);
  }

  while ((file = folder_getnext_type(&folder, "", "", DT_DIR)))
  {
    printf("Sub-folder/File detected %s\n", file);
  }

  printf("Done\n");

}
