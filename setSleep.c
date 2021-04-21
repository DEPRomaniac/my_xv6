#include "types.h"
#include "stat.h"
#include "user.h"
#include "date.h"

static unsigned short days[4][12] =
{
    {   0,  31,  60,  91, 121, 152, 182, 213, 244, 274, 305, 335},
    { 366, 397, 425, 456, 486, 517, 547, 578, 609, 639, 670, 700},
    { 731, 762, 790, 821, 851, 882, 912, 943, 974,1004,1035,1065},
    {1096,1127,1155,1186,1216,1247,1277,1308,1339,1369,1400,1430},
};

unsigned int date_time_to_epoch(struct rtcdate r)
{
    uint second = r.second; 
    uint minute = r.minute; 
    uint hour   = r.hour;   
    uint day    = r.day-1;  
    uint month  = r.month-1;
    uint year   = r.year-2000;
    return (((year/4*(365*4+1)+days[year%4][month]+day)*24+hour)*60+minute)*60+second;
}

int main(int argc, char *argv[])
{
  struct rtcdate r;
  printf(1, "starting process...\n");
  
  get_date(&r);
  uint start_time  = date_time_to_epoch(r);
  
  set_sleep(atoi(argv[1]));
  
  get_date(&r);
  uint duration = date_time_to_epoch(r) - start_time;
  printf(1, "Duration: %ds\n",duration);
  
  exit();
}