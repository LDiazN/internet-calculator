#include "calc.h"
#include <stdlib.h>
#include <stdio.h>

/// This struct contains the general program state
struct Calc
{
    /* data */
    int x;
};

// -- < State manipulation functions > ---------------

/// Summary:
///     Create a Calc object and initialize it

struct Calc * calc_create()
{
    printf("Creating Calc Object");
    return (struct Calc *) malloc(sizeof(struct Calc));
}



