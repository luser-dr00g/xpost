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
 * Enums
 *
 */

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
    XPOST_MEMORY_TABLE_MARK_DATA_MARK_MASK       = 0xFF000000,
    XPOST_MEMORY_TABLE_MARK_DATA_MARK_OFFSET     = 24,
    XPOST_MEMORY_TABLE_MARK_DATA_REFCOUNT_MASK   = 0x00FF0000,
    XPOST_MEMORY_TABLE_MARK_DATA_REFCOUNT_OFFSET = 16,
    XPOST_MEMORY_TABLE_MARK_DATA_LOWLEVEL_MASK   = 0x0000FF00,
    XPOST_MEMORY_TABLE_MARK_DATA_LOWLEVEL_OFFSET = 8,
    XPOST_MEMORY_TABLE_MARK_DATA_TOPLEVEL_MASK   = 0x000000FF,
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

/*
   Xpost_Memory_File functions
*/

/**
 * @brief Initialize the memory file, possibly from file specified by
 * the given file descriptor.
 *
 * @param[in,out] mem The memory file.
 * @param[in] fname 
 * @param[in] fd The file descriptor.
 *
 * This function initializes the memory file @p mem, possibly from
 * file specified by the file descriptor @p fd.
 */
void xpost_memory_file_init (
        Xpost_Memory_File *mem,
        const char *fname,
        int fd);

/**
 * @brief Destroy the given memory file, possibly writing to file.
 *
 * @param[in,out] mem The memory file.
 *
 * This function destroys the memory file @p mem, possibly writing to
 * the file passed to xpost_memory_file_init().
 */
void xpost_memory_file_exit (Xpost_Memory_File *mem);

/**
 * @brief Resize the given memory file, possibly moving and
 * invalidating all vm pointers.
 *
 * @param[in,out] mem The memory file
 * @param[in] sz The size to increase.
 *
 * This function increases the memory used by @p mem by @p sz bites.
 */
void xpost_memory_file_grow (
        Xpost_Memory_File *mem,
        unsigned int sz);

/**
 * @brief Allocate memory in the given memory file and return offset.
 *
 * @param[in,out] mem The memory file.
 * @param[in] sz The new size.
 * @return The offset.
 *
 * This function allocate @p sz bytes to @p mem and returns the
 * offset. If sz is 0, it just returns the offset.
 *
 * Note: May call xpost_memory_file_grow which may invalidate all pointers
 * derived from mem->base. MUST recalculate all VM pointers after this
 * function.
 */
unsigned int xpost_memory_file_alloc (
        Xpost_Memory_File *mem,
        unsigned int sz);

/**
 * @brief Dump the given memory file metadata and contents to stdout.
 *
 * @param[in] mem The memory file.
 *
 * This function dumps to stdout the metadata of @p mem.
 */
void xpost_memory_file_dump (const Xpost_Memory_File *mem);


/*
   Xpost_Memory_Table functions
*/

/**
 * @brief Allocate and Initialize a new table and return the offset.
 *
 * @param[in,out] mem The memory file.
 * @return The offset.
 *
 * This function allocates and initialises a new memory table in
 * @p mem.
 *
 * MUST recalculate all VM pointers after this function.
 * See note in xpost_memory_file_alloc.
 */
unsigned int xpost_memory_table_init (Xpost_Memory_File *mem);

/**
 * @brief Allocate memory, returns table index.
 *
 * @param[in,out] mem The memory file.
 * @param[in] mtabadr The table offset.
 * @param[in] sz The table size.
 * @param[in] tag The table tag.
 * @return The table index.
 *
 * MUST recalculate all VM pointers after this function.
 * See note in xpost_memory_file_alloc.
 */
unsigned int xpost_memory_table_alloc (
        Xpost_Memory_File *mem,
        unsigned int sz,
        unsigned int tag);

/**
 * @brief Find the table and relative entity index for an absolute
 * entity index.
 *
 * @param[in,out] mem The memory file.
 * @param[out] atab The table.
 * @param[in,out] aent The entity.
 *
 * This function takes the memory file and an absolute entity,
 * and writes a pointer to the relevant segment of the table chain
 * into the variable pointed-to by @p atab. It updates the referenced
 * absolute entity index with the index relative to the returned 
 * table segment.
 */
void xpost_memory_table_find_relative (
        Xpost_Memory_File *mem,
        Xpost_Memory_Table **atab,
        unsigned int *aent);

/**
 * @brief Get the address from an entity.
 *
 * @param[in] mem The memory file.
 * @param[in] ent The entity.
 * @return The address.
 *
 * This function returns the address of the entity @p ent in @p mem.
 */
unsigned int xpost_memory_table_get_addr (
        Xpost_Memory_File *mem,
        unsigned int ent);

/**
 * @brief Set the address for an entity.
 *
 * @param[in] mem The memory file.
 * @param[in] ent The entity.
 * @param[in] addr The new address.
 * 
 * This function replaces the address for @p ent in @p mem with 
 * a new address @p addr.
 */
void xpost_memory_table_set_addr (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int addr);

/**
 * @brief Get the size of an entity.
 *
 * @param[in] mem The memory file.
 * @param[in] ent The entity.
 * @return The size.
 *
 * This function returns the size of the entity @p ent in @p mem.
 */
unsigned int xpost_memory_table_get_size (
        Xpost_Memory_File *mem,
        unsigned int ent);

/**
 * @brief Set the size for an entity.
 *
 * @param[in] mem The memory file.
 * @param[in] ent The entity.
 * @param[in] size The new size.
 *
 * This function replaces the size for @p ent in @p mem with
 * a new size @p size.
 */
void xpost_memory_table_set_size (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int size);

/**
 * @brief Get the mark field of an entity.
 *
 * @param[in] mem The memory file.
 * @param[in] ent The entity.
 * @return The mark field.
 *
 * This function returns the mark field of the entity @p ent in @p mem.
 */
unsigned int xpost_memory_table_get_mark (
        Xpost_Memory_File *mem,
        unsigned int ent);

/**
 * @brief Set the mark field for an entity.
 *
 * @param[in] mem The memory file.
 * @param[in] ent The entity.
 * @param[in] mark The new mark field.
 *
 * This function replaces the mark field of the entity @p ent in @p mem
 * with the new value @p mark.
 */
void xpost_memory_table_set_mark (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int mark);

/**
 * @brief Get the tag of an entity.
 *
 * @param[in] mem The memory file.
 * @param[in] ent The entity.
 *
 * This function returns the tag field of the entity @p ent in @p mem.
 */
unsigned int xpost_memory_table_get_tag (
        Xpost_Memory_File *mem,
        unsigned int ent);

/**
 * @brief Set the tag for an entity.
 *
 * @param[in] mem The memory file.
 * @param[in] ent The entity.
 * @param[in] tag The new tag.
 *
 * This function replaces the tag field of the entity @p ent in @p mem.
 */
void xpost_memory_table_set_tag (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int tag);

/**
 * @brief Fetch a value from a composite object.
 *
 * @param[in,out] mem The memory file.
 * @param[in] ent The entity.
 * @param[in] offset The offset.
 * @param[in] sz The entity size.
 * @param[out] dest A buffer
 *
 * This function performs a generic "get" operation from a composite object,
 * or other VM entity such as a file.
 * It is used to retrieve bytes from strings, and objects from arrays.
 */
void xpost_memory_get (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int offset,
        unsigned int sz,
        void *dest);

/**
 * @brief Put a value into a composite object.
 *
 * @param[in,out] mem The memory file.
 * @param[in] ent The entity.
 * @param[in] offset The offset.
 * @param[in] sz The entity size.
 * @param[in] src A buffer
 *
 * This function performs a generic "put" operation into a composite object,
 * or other VM entity such as a file.
 * It is used to store bytes in strings, and objects in arrays.
 */
void xpost_memory_put (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int offset,
        unsigned int sz,
        const void *src);

/**
 * @brief Dump the memory table data and associated memory
 * locations from the given memory file to stdout.
 *
 * @param[in] mem The memory file.
 *
 * This function dumps to stdout the data and associated memory of
 * the table at address 0 in @p mem.
 */
void xpost_memory_table_dump (const Xpost_Memory_File *mem);

#endif
