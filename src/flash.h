/*
 * flash.h
 *
 * Created: 10/29/2015 8:03:02 PM
 *  Author: James
 */ 


#ifndef FLASH_H_
#define FLASH_H_

#define SPI_FLASH				(&AVR32_SPI1)
#define SPI_SLAVECHIP_NUMBER    (0)
#define SPI_FLASH_BAUDRATE		33000000


#define DEFAULT_LEAD_IN			28300


typedef struct flash_header_s {
    uint8_t		name[240];			//null terminated ascii string
	uint16_t	next_disk;			//block number of next disk (after this one, for dual sided game or games with two disks)
	uint16_t	lead_in;			//number of bits for lead in (0 for default)
	uint8_t		reserved[12];
//    uint8_t		data[0xff00];	//disk data, beginning with lead in
} flash_header_t;

int flash_init(void);
void flash_write_disk(int block,flash_header_t *header,uint8_t *disk,int disklen);
void flash_read_disk_header(int block,flash_header_t *header);
void flash_read_disk_start(int block);
void flash_read_disk_stop(void);
void flash_read_disk(uint8_t *data,int len);


#endif /* FLASH_H_ */