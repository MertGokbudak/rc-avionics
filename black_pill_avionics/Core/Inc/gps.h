#ifndef INC_GPS_H_
#define INC_GPS_H_

#include <stdint.h>

void gps_init(void);
void gps_update(void);

extern int32_t gps_lat, gps_lon;
extern int16_t gps_speed;
extern int16_t gps_course;

#endif /* INC_GPS_H_ */
