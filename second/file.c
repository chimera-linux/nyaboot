/* File related stuff
   
   Copyright (C) 1999 Benjamin Herrenschmidt
   
   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include "ctype.h"
#include "types.h"
#include "stddef.h"
#include "stdlib.h"
#include "file.h"
#include "prom.h"
#include "string.h"
#include "partition.h"
#include "fs.h"
#include "errors.h"

extern char bootdevice[1024];

/* This function follows the device path in the devtree and separates
   the device name, partition number, and other datas (mostly file name)
   the string passed in parameters is changed since 0 are put in place
   of some separators to terminate the various strings
 */

int
parse_device_path(char *imagepath, char *defdevice, int defpart,
		  char *deffile, struct boot_fspec_t *result)
{
     char *ptr;
     char *ipath = strdup(imagepath);
     char *defdev = strdup(defdevice);

     result->dev = NULL;
     result->part = -1;
     result->file = NULL;

     if (!strstr(defdev, "ethernet") && !strstr(defdev, "enet")) {
	  if ((ptr = strrchr(defdev, ':')) != NULL)
	       *ptr = 0; /* remove trailing : from defdevice if necessary */
     }

     if (!imagepath)
	  goto punt;

     if ((ptr = strrchr(ipath, ',')) != NULL) {
	  result->file = strdup(ptr+1);
	  /* Trim the filename off */
	  *ptr = 0;
     }

     if (strstr(ipath, "ethernet") || strstr(ipath, "enet"))
	  if ((ptr = strstr(ipath, "bootp")) != NULL) { /* `n' key booting boots enet:bootp */
	       *ptr = 0;
	       result->dev = strdup(ipath);
	  } else
	  result->dev = strdup(ipath);
     else if ((ptr = strchr(ipath, ':')) != NULL) {
	  *ptr = 0;
	  result->dev = strdup(ipath);
	  if (*(ptr+1))
	       result->part = simple_strtol(ptr+1, NULL, 10);
     } else if (strlen(ipath)) {
          result->file = strdup(ipath);
     } else {
	  return 0;
     }
     
 punt:
     if (!result->dev)
	  result->dev = strdup(defdev);
     
     if (result->part < 0)
	  result->part = defpart;
     
     if (!result->file)
	  result->file = strdup(deffile);
     free(ipath);
     return 1;
}

#if 0
char *
parse_device_path(char *of_device, char **file_spec, int *partition)
{
	char *p, *last;

	if (file_spec)
		*file_spec = NULL;
	if (partition)
		*partition = -1;

	DEBUG_F("of_device before parsing: %s\n", of_device);
	p = strchr(of_device, ':');
	DEBUG_F("of_device after parsing: %s\n", p);

	if (!p) {                          /* if null terminated we are finished */
	     DEBUG_F("of_device: %s\n", of_device);
	     return of_device;
	}
#if 0 /* this is broken crap, breaks netboot entirely */
	else if (strstr(of_device, "ethernet") != NULL)
	     p = strchr(of_device, ',');  /* skip over ip all the way to the ',' */
	else if (strstr(of_device, "enet") != NULL)
	     p = strchr(of_device, ',');  /* skip over ip all the way to the ',' */
#endif
	*p = 0;
	last = ++p;                       /* sets to start of second part */
	while(*p && *p != ',') {
        if (!isdigit (*p)) {
	     p = last;
	     break;
	}
	++p;
	}
	if (p != last) {
	     *(p++) = 0;
	     if (partition)
		  *partition = simple_strtol(last, NULL, 10);
	}
	if (*p && file_spec)
	     *file_spec = p;

	DEBUG_F("of_device: %s\n", of_device);
	strcat(of_device, ":");
	DEBUG_F("of_device after strcat: %s\n", of_device);
	return of_device;
}

int
validate_fspec(		struct boot_fspec_t*	spec,
			char*			default_device,
			int			default_part)
{
    if (!spec->file) {
    	spec->file = spec->dev;
    	spec->dev = NULL;
    }
    if (spec->part == -1)
    	spec->part = default_part;
    if (!spec->dev)
	spec->dev = default_device;
    if (!spec->file)
	return FILE_BAD_PATH;
    else if (spec->file[0] == ',')
	spec->file++;

    return FILE_ERR_OK;
}

#endif

static int
file_block_open(	struct boot_file_t*	file,
			const char*		dev_name,
			const char*		file_name,
			int			partition)
{
	struct partition_t*	parts;
	struct partition_t*	p;
	struct partition_t*	found;
	
	parts = partitions_lookup(dev_name);
	found = NULL;
			
#if DEBUG
	if (parts)
		prom_printf("partitions:\n");
	else
		prom_printf("no partitions found.\n");
#endif
	for (p = parts; p && !found; p=p->next) {
		DEBUG_F("number: %02d, start: 0x%08lx, length: 0x%08lx\n",
			p->part_number, p->part_start, p->part_size );
		if (partition == -1) {
                        file->fs = fs_open( file, dev_name, p, file_name );
			if (file->fs != NULL)
				goto bail;
		}
		if ((partition >= 0) && (partition == p->part_number))
			found = p;
#if DEBUG
		if (found)
			prom_printf(" (match)\n");
#endif						
	}

	/* Note: we don't skip when found is NULL since we can, in some
	 * cases, let OF figure out a default partition.
	 */
        DEBUG_F( "Using OF defaults.. (found = %p)\n", found );
        file->fs = fs_open( file, dev_name, found, file_name );

bail:
	if (parts)
		partitions_free(parts);

	return fserrorno;
}

static int
file_net_open(	struct boot_file_t*	file,
		const char*		dev_name,
		const char*		file_name)
{
    file->fs = fs_of_netboot;
    return fs_of_netboot->open(file, dev_name, NULL, file_name);
}

static int
default_read(	struct boot_file_t*	file,
		unsigned int		size,
		void*			buffer)
{
	prom_printf("WARNING ! default_read called !\n");
	return FILE_ERR_EOF;
}

static int
default_seek(	struct boot_file_t*	file,
		unsigned int		newpos)
{
	prom_printf("WARNING ! default_seek called !\n");
	return FILE_ERR_EOF;
}

static int
default_close(	struct boot_file_t*	file)
{
	prom_printf("WARNING ! default_close called !\n");
	return FILE_ERR_OK;
}

static struct fs_t fs_default =
{
    "defaults",
    NULL,
    default_read,
    default_seek,
    default_close
};


int open_file(	const struct boot_fspec_t*	spec,
		struct boot_file_t*		file)
{
//	static char	temp[1024];
	static char	temps[64];
//	char		*dev_name;
//	char		*file_name = NULL;
	phandle		dev;
	int		result;
	int		partition;
	
	memset(file, 0, sizeof(struct boot_file_t*));
        file->fs        = &fs_default;

	/* Lookup the OF device path */
	/* First, see if a device was specified for the kernel
	 * if not, we hope that the user wants a kernel on the same
	 * drive and partition as yaboot itself */
#if 0 /* this is crap */
	if (!spec->dev)
		strcpy(spec->dev, bootdevice);
	strncpy(temp,spec->dev,1024);
	dev_name = parse_device_path(temp, &file_name, &partition);
	if (file_name == NULL)
		file_name = (char *)spec->file;
	if (file_name == NULL) {
	     prom_printf("Configuration error: null filename\n");
	     return FILE_ERR_NOTFOUND;
	}
	if (partition == -1)
#endif
		partition = spec->part;


	DEBUG_F("dev_path = %s\nfile_name = %s\npartition = %d\n",
		spec->dev, spec->file, partition);

	/* Find OF device phandle */
	dev = prom_finddevice(spec->dev);
	if (dev == PROM_INVALID_HANDLE) {
		return FILE_ERR_BADDEV;
	}

	DEBUG_F("dev_phandle = %p\n", dev);

	/* Check the kind of device */
	result = prom_getprop(dev, "device_type", temps, 63);
	if (result == -1) {
		prom_printf("can't get <device_type> for device\n");
		return FILE_ERR_BADDEV;
	}
	temps[result] = 0;
	if (!strcmp(temps, "block"))
		file->device_kind = FILE_DEVICE_BLOCK;
	else if (!strcmp(temps, "network"))
		file->device_kind = FILE_DEVICE_NET;
	else {
		prom_printf("Unkown device type <%s>\n", temps);
		return FILE_ERR_BADDEV;
	}
	
	switch(file->device_kind) {
	    case FILE_DEVICE_BLOCK:
		DEBUG_F("device is a block device\n");
	  	return file_block_open(file, spec->dev, spec->file, partition);
	    case FILE_DEVICE_NET:
		DEBUG_F("device is a network device\n");
	  	return file_net_open(file, spec->dev, spec->file);
	}
	return 0;
}

/* 
 * Local variables:
 * c-file-style: "K&R"
 * c-basic-offset: 5
 * End:
 */