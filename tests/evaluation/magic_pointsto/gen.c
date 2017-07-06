#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) 
{
    int i; 

    // get number of fields etc. 
    int nv = atoi(argv[1]); 
    int nh = atoi(argv[2]); 
    int nf = atoi(argv[3]); 

    // randomize assignments 
    for(i=0;i<nh;i++) { 
        // pick variable randomly 
        int v = (abs(rand())) % nv;
        printf("AssignAlloc(\"v%d\",\"h%d\").\n",v,i);
    }

    // randomize assignments 
    int na = (abs(rand())) % (nv * nv); 
    for(i=0;i<na;i++) { 
        // pick variables randomly 
        int v1 = (abs(rand())) % nv;
        int v2 = (abs(rand())) % nv;
        printf("PrimitiveAssign(\"v%d\",\"h%d\").\n",v1,v2);
    }

    // randomize assignments 
    for (i=0;i<nf; i++) {
        // pick variable randomly 
        int v1 = (abs(rand())) % nv;
        int v2 = (abs(rand())) % nv;
        int v3 = (abs(rand())) % nv;
        int v4 = (abs(rand())) % nv;
        printf("Load(\"v%d\",\"v%d\",\"f%d\").\n",v1,v2,i);
        printf("Store(\"v%d\",\"v%d\",\"f%d\").\n",v3,v4,i);
    }
    return 0;
}
