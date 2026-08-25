#include "commob_tool.h"

int16_t com_limit(int16_t value, int16_t min, int16_t max)
{
    int16_t result=value;
    if (value < min)
      { return min;}
    else if (value > max)
      { return max;}
    else
        return result;
}



