#ifndef XPOST_MEMORY_H
#define XPOST_MEMORY_H

/**
 * @file xpost_memory.h
 * @brief The memory management data structures, #Xpost_Memory_File and
 * #Xpost_Memory_Table.
 */

/*
 *
 * Macros
 *
 */

/**
 * @def XPOST_MEMORY_TABLE_SIZE
 * @brief Number of entries in a single segment of the
 * Xpost_Memory_Table.
 *
 * This parameter may be tuned for performance.
 *
 * Most VM access (composite object data) has to go through the
 * #Xpost_Memory_Table, which is segmented. So depending on the number
 * of live entries (k), accessing data from an object may require
 * chasing through k/#XPOST_MEMORY_TABLE_SIZE tables before finding the
 * right one. This isn't a lengthy operation, but it is a complicated
 * address calcuation that may not be pipeline-friendly.
 */
#define XPOST_MEMORY_TABLE_SIZE 200

/*
 *
 * Structs
 *
 */

/**
 * @struct Xpost_Memory_File
 * @brief A memory region that may be suballocated. Bookkeeping data
 * for region allocator.
 *
 * Used as the basis for the Postscript Virtual Memory.
 */
typedef struct
{
    int fd; /**< file descriptor associated with this memory/file, or -1 if not used. */
    char fname[20]; /**< file name associated with this memory/file, or "" if not used. */
    /*@dependent@*/
    unsigned char *base; /**< pointer to mapped memory */
    unsigned int used;  /**< size used, cursor to free space */
    unsigned int max; /**< size available in memory pointed to by base */

    unsigned int start; /**< first 'live' entry in the memory_table. */
        /* the domain of the collector is entries >= start */
} Xpost_Memory_File;

/**
 * @struct Xpost_Memory_Table
 * @brief The segmented Memory Table structure.
 */
typedef struct
{
    unsigned int nexttab; /**< next table in chain */
    unsigned int nextent; /**< next slot in table, or #XPOST_MEMORY_TABLE_SIZE if full */
    struct {
        unsigned int adr; /**< allocation address */
        unsigned int sz; /**< size of allocation */
        unsigned int mark; /**< garbage collection metadata */
        unsigned int tag; /**< type of object using this allocation, if needed */
    } tab [ XPOST_MEMORY_TABLE_SIZE ];
} Xpost_Memory_Table;

/**
 * @typedef Xpost_Memory_Table_Mark_Data
 *
 * There are 4 "virtual" bitfields packed in what is assumed to be a
 * 32-bit unsigned field. These values are used in masking and
 * shifting operations to access the fields in a direct, portable
 * manner.
 */
typedef enum
{
    XPOST_MEMORY_TABLE_MARK_DATA_MARK_MASK = 0xFF000000,
    XPOST_MEMORY_TABLE_MARK_DATA_MARK_OFFSET = 24,
    XPOST_MEMORY_TABLE_MARK_DATA_REFCOUNT_MASK = 0x00FF0000,
    XPOST_MEMORY_TABLE_MARK_DATA_REFCOUNT_OFFSET = 16,
    XPOST_MEMORY_TABLE_MARK_DATA_LOLEVEL_MASK = 0x0000FF00,
    XPOST_MEMORY_TABLE_MARK_DATA_LOLEVEL_OFFSET = 8,
    XPOST_MEMORY_TABLE_MARK_DATA_TOPLEVEL_MASK = 0x000000FF,
    XPOST_MEMORY_TABLE_MARK_DATA_TOPLEVEL_OFFSET = 0,
} Xpost_Memory_Table_Mark_Data;

/**
 * @typedef Xpost_Memory_Table_Special
 * @brief Special entities occupy the first few slots of the first
 * #Xpost_Memory_Table in the #Xpost_Memory_File.
 */
typedef enum
{
    XPOST_MEMORY_TABLE_SPECIAL_FREE,
    XPOST_MEMORY_TABLE_SPECIAL_SAVE_STACK,
    XPOST_MEMORY_TABLE_SPECIAL_CONTEXT_LIST,
    XPOST_MEMORY_TABLE_SPECIAL_NAME_STACK,
    XPOST_MEMORY_TABLE_SPECIAL_NAME_TREE,
    XPOST_MEMORY_TABLE_SPECIAL_BOGUS_NAME,
    XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE,
} Xpost_Memory_Table_Special;

/*
 *
 * Variables
 *
 */

/**
 * @var xpost_memory_pagesize
 * @brief The 'grain' of the memory-file size.
 */
extern unsigned int xpost_memory_pagesize /*= getpagesize()*/; /*= 4096 on 32bit Linux*/

/*
 *
 * Functions
 *
 */

/**
 * @brief Initialize the memory file, possibly from file specified by
 * the given file descriptor.
 *
 * @param mem The memory file.
 * @param fd The file descriptor.
 *
 * This function initializes the memory file @p mem, possibly from
 * file specified by the file descriptor @p fd.
 */
void xpost_memory_file_init (Xpost_Memory_File *mem, int fd);

/**
 * @brief Destroy the given memory file, possibly writing to file.
 *
 * @param mem The memory file.
 *
 * This function destroys the memory file @p mem, possibly writing to
 * the file passed to xpost_memory_file_init().
 */
void xpost_memory_file_exit (Xpost_Memory_File *mem);

/**
 * @brief Resize the given memory file, possibly moving and
 * invalidating all vm pointers.
 *
 * @param mem The memory file
 * @param sz The size to increase.
 *
 * This function increases the memory used by @p mem by @p sz bites.
 */
void xpost_memory_file_grow (Xpost_Memory_File *mem, unsigned int sz);

/**
 * @brief Allocate memory in the gven memory file and return offset.
 *
 * @param mem The memory file.
 * @param sz The new size.
 * @return The offset.
 *
 * This function allocate @p sz bytes to @p mem and returns the
 * offset. If sz is 0, it just returns the offset.
 */
unsigned int xpost_memory_file_alloc (Xpost_Memory_File *mem, unsigned int sz);

/**
 * @brief Dump the given memory file metadata and contents to stdout.
 *
 * @param mem The memory file.
 *
 * This function dumps to stdout the metadata of @p mem.
 */
void xpost_memory_file_dump (Xpost_Memory_File *mem);

/**
 * @brief Dump the given memory table data and associated memory
 * locations to stdout.
 *
 * @param mem The memory table.
 * @param mtabadr ???
 *
 * This function dumps to stdout the data and associated memory of @p mem.
 */
void xpost_memory_table_dump (Xpost_Memory_Table *mem, unsigned int mtabadr);

#endif
