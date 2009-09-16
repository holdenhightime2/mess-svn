/* flopdrv provides simple emulation of a disc drive */
/* the 8271, nec765 and wd179x use this */

#ifndef FLOPDRV_H
#define FLOPDRV_H

#include "device.h"
#include "image.h"
#include "formats/flopimg.h"

/***************************************************************************
    TYPE DEFINITIONS
***************************************************************************/
/* floppy drive types */
typedef enum
{
	FLOPPY_DRIVE_SS_40,
	FLOPPY_DRIVE_DS_80
} floppy_type;

typedef struct floppy_config_t	floppy_config;
struct floppy_config_t
{
	floppy_type floppy_type;
	const struct FloppyFormat *formats;
};

typedef enum
{
	DEN_FM_LO = 0,
	DEN_FM_HI,
	DEN_MFM_LO,
	DEN_MFM_HI
} DENSITY;

/* sector has a deleted data address mark */
#define ID_FLAG_DELETED_DATA	0x0001
/* CRC error in id field */
#define ID_FLAG_CRC_ERROR_IN_ID_FIELD 0x0002
/* CRC error in data field */
#define ID_FLAG_CRC_ERROR_IN_DATA_FIELD 0x0004

int	floppy_status(const device_config *img, int new_status);

typedef struct chrn_id
{
	unsigned char C;
	unsigned char H;
	unsigned char R;
	unsigned char N;
	int data_id;			// id for read/write data command
	unsigned long flags;
} chrn_id;

/* set if disc is write protected - also set if drive is present but no disc in drive */
#define FLOPPY_DRIVE_DISK_WRITE_PROTECTED		0x0002
/* set if drive is connected and head is positioned over track 0 */
#define FLOPPY_DRIVE_HEAD_AT_TRACK_0			0x0004
/* set if drive is ready */
#define FLOPPY_DRIVE_READY						0x0010
/* set if index has just occured */
#define FLOPPY_DRIVE_INDEX						0x0020
/* motor state */
#define FLOPPY_DRIVE_MOTOR_ON					0x0040

/* a callback which will be executed if the ready state of the drive changes e.g. not ready->ready, ready->not ready */
void floppy_drive_set_ready_state_change_callback(const device_config *img, void (*callback)(const device_config *controller,const device_config *img, int state));

void floppy_drive_set_index_pulse_callback(const device_config *img, void (*callback)(const device_config *controller,const device_config *image, int state));

/* set flag state */
int floppy_drive_get_flag_state(const device_config *img, int flag);
/* get flag state */
void floppy_drive_set_flag_state(const device_config *img, int flag, int state);
/* get current physical track drive is on */
int floppy_drive_get_current_track(const device_config *img);

void floppy_drive_set_geometry(const device_config *img, floppy_type type);
void floppy_drive_set_geometry_absolute(const device_config *img, int tracks, int sides);

/* get next id from track, 1 if got a id, 0 if no id was got */
int floppy_drive_get_next_id(const device_config *img, int side, chrn_id *);
/* set ready state of drive. If flag == 1, set ready state only if drive present,
disk is in drive, and motor is on. Otherwise set ready state to the state passed */
void floppy_drive_set_ready_state(const device_config *img, int state, int flag);

void floppy_drive_set_motor_state(const device_config *img, int state);

/* seek up or down */
void floppy_drive_seek(const device_config *img, signed int signed_tracks);

void floppy_drive_read_track_data_info_buffer(const device_config *img, int side, void *ptr, int *length );
void floppy_drive_write_track_data_info_buffer(const device_config *img, int side, const void *ptr, int *length );
void floppy_drive_format_sector(const device_config *img, int side, int sector_index, int c, int h, int r, int n, int filler);
void floppy_drive_read_sector_data(const device_config *img, int side, int index1, void *pBuffer, int length);
void floppy_drive_write_sector_data(const device_config *img, int side, int index1, const void *pBuffer, int length, int ddam);
int	floppy_drive_get_datarate_in_us(DENSITY density);

/* set motor speed to get correct index pulses
   standard RPM are 300 RPM (common) and 360 RPM
   Note: this actually only works for soft sectored disks: one index pulse per
   track.
*/
void floppy_drive_set_rpm(const device_config *image, float rpm);

void floppy_drive_set_controller(const device_config *img, const device_config *controller);

enum
{
	MESS_DEVINFO_PTR_FLOPPY_OPTIONS = MESS_DEVINFO_PTR_DEV_SPECIFIC,
	MESS_DEVINFO_INT_KEEP_DRIVE_GEOMETRY = MESS_DEVINFO_INT_DEV_SPECIFIC
};

floppy_image *flopimg_get_image(const device_config *image);

/* hack for apple II; replace this when we think of something better */
void floppy_install_unload_proc(const device_config *image, void (*proc)(const device_config *image));

void floppy_install_load_proc(const device_config *image, void (*proc)(const device_config *image));

/* hack for TI99; replace this when we think of something better */
void floppy_install_tracktranslate_proc(const device_config *image, int (*proc)(const device_config *image, floppy_image *floppy, int physical_track));

const device_config *floppy_get_device(running_machine *machine,int drive);
int floppy_get_count(running_machine *machine);

int floppy_get_drive(const device_config *image);

void *flopimg_get_custom_data(const device_config *image);
void flopimg_alloc_custom_data(const device_config *image,int size);

#define FLOPPY	DEVICE_GET_INFO_NAME(floppy)
DEVICE_GET_INFO(floppy);

extern DEVICE_START( floppy );
extern DEVICE_IMAGE_LOAD( floppy );
extern DEVICE_IMAGE_CREATE( floppy );
extern DEVICE_IMAGE_UNLOAD( floppy );

/***************************************************************************
    DEVICE CONFIGURATION MACROS
***************************************************************************/
#define FLOPPY_0 "floppy0"
#define FLOPPY_1 "floppy1"
#define FLOPPY_2 "floppy2"
#define FLOPPY_3 "floppy3"


#define MDRV_FLOPPY_DRIVE_ADD(_tag, _config) 	\
	MDRV_DEVICE_ADD(_tag, FLOPPY, 0)			\
	MDRV_DEVICE_CONFIG(_config)

#define MDRV_FLOPPY_DRIVE_MODIFY(_tag, _config)	\
	MDRV_DEVICE_MODIFY(_tag)		\
	MDRV_DEVICE_CONFIG(_config)

#define MDRV_FLOPPY_4_DRIVES_ADD(_config) 	\
	MDRV_DEVICE_ADD(FLOPPY_0, FLOPPY, 0)		\
	MDRV_DEVICE_CONFIG(_config)	\
	MDRV_DEVICE_ADD(FLOPPY_1, FLOPPY, 0)		\
	MDRV_DEVICE_CONFIG(_config)	\
	MDRV_DEVICE_ADD(FLOPPY_2, FLOPPY, 0)		\
	MDRV_DEVICE_CONFIG(_config)	\
	MDRV_DEVICE_ADD(FLOPPY_3, FLOPPY, 0)		\
	MDRV_DEVICE_CONFIG(_config)	

#define MDRV_FLOPPY_4_DRIVES_MODIFY(_config) 	\
	MDRV_DEVICE_MODIFY(FLOPPY_0)		\
	MDRV_DEVICE_CONFIG(_config)	\
	MDRV_DEVICE_MODIFY(FLOPPY_1)		\
	MDRV_DEVICE_CONFIG(_config)	\
	MDRV_DEVICE_MODIFY(FLOPPY_2)		\
	MDRV_DEVICE_CONFIG(_config)	\
	MDRV_DEVICE_MODIFY(FLOPPY_3)		\
	MDRV_DEVICE_CONFIG(_config)	

#define MDRV_FLOPPY_4_DRIVES_REMOVE() 	\
	MDRV_DEVICE_REMOVE(FLOPPY_0)		\
	MDRV_DEVICE_REMOVE(FLOPPY_1)		\
	MDRV_DEVICE_REMOVE(FLOPPY_2)		\
	MDRV_DEVICE_REMOVE(FLOPPY_3)		
	
#define MDRV_FLOPPY_2_DRIVES_ADD(_config) 	\
	MDRV_DEVICE_ADD(FLOPPY_0, FLOPPY, 0)		\
	MDRV_DEVICE_CONFIG(_config)	\
	MDRV_DEVICE_ADD(FLOPPY_1, FLOPPY, 0)		\
	MDRV_DEVICE_CONFIG(_config)	

#define MDRV_FLOPPY_2_DRIVES_MODIFY(_config) 	\
	MDRV_DEVICE_MODIFY(FLOPPY_0)		\
	MDRV_DEVICE_CONFIG(_config)	\
	MDRV_DEVICE_MODIFY(FLOPPY_1)		\
	MDRV_DEVICE_CONFIG(_config)	

#define MDRV_FLOPPY_2_DRIVES_REMOVE() 	\
	MDRV_DEVICE_REMOVE(FLOPPY_0)		\
	MDRV_DEVICE_REMOVE(FLOPPY_1)		
			
#endif /* FLOPDRV_H */
