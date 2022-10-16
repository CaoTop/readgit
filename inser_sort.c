#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void sort(int arry[],int count)
{
    int i,j;
    int temp;
    for(i=1;i<count;i++)
    {
        temp=arry[i];
        for(j=i;j>0&&arry[j-1]>temp;j--)
        {
            arry[j]=arry[j-1];
        }
        arry[j]=temp;
    }
    for(i=0;i<count;i++)
    {
        printf("%-4d",arry[i]);
    }
    printf("\n");
}

int main(int argc,const char *argv[])
{
    int arry[10]={10,26,5,66,18,24,25,88,95,2};
    sort(arry, 10);
    
    return 0;
}
