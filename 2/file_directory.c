#include <dirent.h>
#include <sys/types.h>
#include <stdio.h>
#include <string.h>

int main()
{
    struct dirent *item;
    DIR *dp;

    dp = opendir("/");
    if (dp != NULL)
    {
        for(;;)
        {
            item = readdir(dp);
            if (item == NULL)
                break;
            printf("DIR : %s\n", item->d_name);
        }
        closedir(dp);
    }
}
