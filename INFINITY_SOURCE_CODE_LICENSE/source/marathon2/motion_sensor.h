/*
MOTION_SENSOR.H
Friday, June 17, 1994 12:10:02 PM
*/

/* ---------- prototypes/MOTION_SENSOR.C */

void initialize_motion_sensor(shape_descriptor mount, shape_descriptor virgin_mounts,
	shape_descriptor alien, shape_descriptor friendly, shape_descriptor enemy,
	shape_descriptor network_compass, short side_length);
void reset_motion_sensor(short monster_index);
void motion_sensor_scan(short ticks_elapsed);
boolean motion_sensor_has_changed(void);
void adjust_motion_sensor_range(void);
