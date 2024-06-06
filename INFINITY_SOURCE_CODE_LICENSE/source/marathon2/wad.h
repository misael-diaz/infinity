/*
	WAD.H
	Thursday, June 30, 1994 10:55:20 PM

	Sunday, July 3, 1994 5:51:47 PM
	I wonder if I should include an element size in the entry_header structure...

	Tuesday, December 13, 1994 4:10:48 PM
	Included element size in the entry_header for the directory size.  The directory
	is application_specific_directory_data_size+sizeof(struct directory_entry).
*/

#define PRE_ENTRY_POINT_WADFILE_VERSION 0
#define WADFILE_HAS_DIRECTORY_ENTRY 1
#define WADFILE_SUPPORTS_OVERLAYS 2
#define CURRENT_WADFILE_VERSION (WADFILE_SUPPORTS_OVERLAYS)
// The Infinity demo was version 3.
// Infinity release will be version 4.
#ifdef DEMO
#define INFINITY_WADFILE_VERSION (CURRENT_WADFILE_VERSION+1)
#else
#define INFINITY_WADFILE_VERSION (CURRENT_WADFILE_VERSION+2)
#endif

#define MAXIMUM_DIRECTORY_ENTRIES_PER_FILE 64
#define MAXIMUM_WADFILE_NAME_LENGTH 64
#define MAXIMUM_UNION_WADFILES 16
#define MAXIMUM_OPEN_WADFILES 3

#include "portable_files.h"

/* ------------- typedefs */
typedef unsigned long WadDataType;

/* ------------- file structures */
struct wad_header { /* 128 bytes */
	short version;									/* Used internally */
	short data_version;								/* Used by the data.. */
	char file_name[MAXIMUM_WADFILE_NAME_LENGTH];
	unsigned long checksum;
	long directory_offset;
	short wad_count;
	short application_specific_directory_data_size;
	short entry_header_size;
	short directory_entry_base_size;
	unsigned long parent_checksum;	/* If non-zero, this is the checksum of our parent, and we are simply modifications! */
	short unused[20];
};

struct old_directory_entry { /* 8 bytes */
	long offset_to_start; /* From start of file */
	long length; /* Of total level */
};

struct directory_entry { /* 10 bytes */
	long offset_to_start; /* From start of file */
	long length; /* Of total level */
	short index; /* For inplace modification of the wadfile! */
};

struct old_entry_header { /* 12 bytes */
	WadDataType tag;
	long next_offset; /* From current file location-> ie directory_entry.offset_to_start+next_offset */
	long length; /* Of entry */

	/* Element size? */
	
	/* Data follows */
};

struct entry_header { /* 16 bytes */
	WadDataType tag;
	long next_offset; /* From current file location-> ie directory_entry.offset_to_start+next_offset */
	long length; /* Of entry */
	long offset; /* Offset for inplace expansion of data */

	/* Element size? */
	
	/* Data follows */
};

/* ---------- Memory Data structures ------------ */
struct tag_data {
	WadDataType tag;		/* What type of data is this? */
	byte *data; 			/* Offset into the wad.. */
	long length;			/* Length of the data */
	long offset;			/* Offset for patches */
};

/* This is what a wad * actually is.. */
struct wad_data {
	short tag_count;			/* Tag count */
	short padding;
	byte *read_only_data;		/* If this is non NULL, we are read only.... */
	struct tag_data *tag_data;	/* Tag data array */
};

/* ----- miscellaneous functions */
boolean wad_file_has_checksum(FileDesc *file, unsigned long checksum);
boolean wad_file_has_parent_checksum(FileDesc *file, unsigned long parent_checksum);

/* Find out how many wads there are in the map */
short number_of_wads_in_file(FileDesc *file); /* returns -1 on error */

/* ----- Open/Close functions */
/* Should be OSType, or extension for dos. */
FileError create_wadfile(FileDesc *file, unsigned long file_type);

fileref open_wad_file_for_reading(FileDesc *file);
fileref open_wad_file_for_writing(FileDesc *file);

void close_wad_file(fileref file_id);

/* ----- Hardware dependent functions (From macintosh_wad.c) */
FileError find_other_entries_that_reference_checksum(unsigned long checksum,
	FileDesc *files_array, short *count);

/* ----- Read File functions */

/* Read the header from the wad file */
boolean read_wad_header(fileref file_id, struct wad_header *header);

/* Read the indexed wad from the file */
struct wad_data *read_indexed_wad_from_file(fileref file_id, 
	struct wad_header *header, short index, boolean read_only);

/* Properly deal with the memory.. */
void free_wad(struct wad_data *wad);

long get_size_of_directory_data(struct wad_header *header);

/* -----  Read Wad functions */

/* Given a wad, extract the given tag from it */
void *extract_type_from_wad(struct wad_data *wad, WadDataType type, 
	long *length);

/* Calculate the length of the wad */
long calculate_wad_length(struct wad_header *file_header, struct wad_data *wad);

/* Note wad_count and directory offset in the header! better be correct! */
void *get_indexed_directory_data(struct wad_header *header, short index,
	void *directories);

void *read_directory_data(short file_ref, struct wad_header *header);

unsigned long read_wad_file_checksum(FileDesc *file);
unsigned long read_wad_file_parent_checksum(FileDesc *file);

boolean find_wad_file_that_has_checksum(FileDesc *matching_file,
	unsigned long file_type, short path_resource_id, unsigned long checksum);

/* Added in here for simplicity.  Really should be somewhere else.. */
boolean find_file_with_modification_date(FileDesc *matching_file,
	unsigned long file_type, short path_resource_id, unsigned long modification_date);

#ifdef mac
/* Handy function to have.  Given the fsspec of a directory, returns the id needed */
/*  to search inside it.. */
OSErr get_directories_parID(FSSpec *directory, long *parID);
#endif

/* ------------ Flat wad functions */
/* These functions are used for transferring data, and it completely encapsulates */
/*  a given wad from a given file... */
void *get_flat_data(FileDesc *base_file, boolean use_union, short wad_index);
long get_flat_data_length(void *data);

/* This is how you dispose of it-> you inflate it, then use free_wad() */
struct wad_data *inflate_flat_data(void *data, struct wad_header *header);

/* ------------ Union wad functions */
/* Both of these return 0 on error */
fileref open_union_wad_file_for_reading(FileDesc *base_file);
fileref open_union_wad_file_for_reading_by_list(FileDesc *files, short count);

/* ------------  Write File functions */
struct wad_data *create_empty_wad(void);
void fill_default_wad_header(FileDesc *file, short wadfile_version,
	short data_version, short wad_count, short application_directory_data_size,
	struct wad_header *header);
boolean write_wad_header(fileref file_id, struct wad_header *header);
boolean write_directorys(fileref file_id,  struct wad_header *header,
	void *entries);
void calculate_and_store_wadfile_checksum(fileref file_id);
boolean write_wad(fileref file_id, struct wad_header *file_header, 
	struct wad_data *wad, long offset);

void set_indexed_directory_offset_and_length(struct wad_header *header, 
	void *entries, short index, long offset, long length, short wad_index);

/* ------ Write Wad Functions */
struct wad_data *append_data_to_wad(
	struct wad_data *wad, 
	WadDataType type, 
	void *data, 
	long size, 
	long offset);

void remove_tag_from_wad(struct wad_data *wad, WadDataType type);
	
/* ------- debug function */
void dump_wad(struct wad_data *wad);
