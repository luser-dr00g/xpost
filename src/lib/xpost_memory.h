/*
 * Xpost - a Level-2 Postscript interpreter
 * Copyright (C) 2013, Michael Joshua Ryan
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 * - Neither the name of the Xpost software product nor the names of its
 *   contributors may be used to endorse or promote products derived from this
 *   software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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
    XPOST_MEMORY_TABLE_MARK_DATA_MARK_MASK       = 0x7F000000,
    XPOST_MEMORY_TABLE_MARK_DATA_MARK_OFFSET     =     24,
    XPOST_MEMORY_TABLE_MARK_DATA_REFCOUNT_MASK   = 0x00FF0000,
    XPOST_MEMORY_TABLE_MARK_DATA_REFCOUNT_OFFSET =       16,
    XPOST_MEMORY_TABLE_MARK_DATA_LOWLEVEL_MASK   = 0x0000FF00,
    XPOST_MEMORY_TABLE_MARK_DATA_LOWLEVEL_OFFSET =         8,
    XPOST_MEMORY_TABLE_MARK_DATA_TOPLEVEL_MASK   = 0x000000FF,
    XPOST_MEMORY_TABLE_MARK_DATA_TOPLEVEL_OFFSET =           0
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
    XPOST_MEMORY_TABLE_SPECIAL_OPERATOR_TABLE
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
typedef struct Xpost_Memory_File
{
    int fd; /**< file descriptor associated with this memory/file,
                  or -1 if not used. */
    char fname[20]; /**< file name associated with this memory/file,
                          or "" if not used. */
    /*@dependent@*/
    unsigned char *base; /**< pointer to mapped memory */
    unsigned int used;  /**< size used, cursor to free space */
    unsigned int max; /**< size available in memory pointed to by base */

    unsigned int start; /**< first 'live' entry in the memory_table. */
        /* the domain of the collector is entries >= start */

    int free_list_alloc_is_installed;
    int (*free_list_alloc)(struct Xpost_Memory_File *mem,
                           unsigned sz,
                           unsigned tag,
                           unsigned int *entity);

    int garbage_collect_is_installed;
    unsigned int (*garbage_collect)(struct Xpost_Memory_File *mem,
                                    int dosweep,
                                    int markall);
} Xpost_Memory_File;

/**
 * @struct Xpost_Memory_Table
 * @brief The segmented Memory Table structure.
 */
typedef struct
{
    unsigned int nexttab; /**< next table in chain */
    unsigned int nextent; /**< next slot in table,
                                or #XPOST_MEMORY_TABLE_SIZE if full */
    struct
    {
        unsigned int adr; /**< allocation address */
        unsigned int sz; /**< size of allocation */
        unsigned int mark; /**< garbage collection metadata */
        unsigned int tag; /**< type of object using this allocation, if needed */
    } tab [ XPOST_MEMORY_TABLE_SIZE ]; /**< table entries in this segment */
} Xpost_Memory_Table;

/*
 *
 * Variables
 *
 */

/**
 * @var xpost_memory_page_size
 * @brief The 'grain' of the memory-file size.
 */
extern unsigned int xpost_memory_page_size;


/*
 *
 * Functions
 *
 */

/**
 * @brief Initialize the memory module.
 *
 * This function initializes the memory module. Currently, it only set
 * the value of #xpost_memory_page_size. It is called by
 * xpost_init().
 */
int xpost_memory_init(void);

/*
   Xpost_Memory_File functions
*/

/**
 * @brief Initialize the memory file, possibly from file specified by
 * the given file descriptor.
 *
 * @param[in,out] mem The memory file.
 * @param[in] fname The file name.
 * @param[in] fd The file descriptor.
 * @return 1 on success, 0 on failure.
 *
 * This function initializes the memory file @p mem, possibly from
 * file specified by the file descriptor @p fd, if not -1.
 */
int xpost_memory_file_init (
        Xpost_Memory_File *mem,
        const char *fname,
        int fd);

/**
 * @brief Destroy the given memory file, possibly writing to file.
 *
 * @param[in,out] mem The memory file.
 * @return 1 on success, 0 on failure.
 *
 * This function destroys the memory file @p mem, possibly writing to
 * the file passed to xpost_memory_file_init().
 */
int xpost_memory_file_exit (Xpost_Memory_File *mem);

/**
 * @brief Resize the given memory file, possibly moving the memory
 * and invalidating all vm pointers.
 *
 * @param[in,out] mem The memory file
 * @param[in] sz The size to increase.
 * @return 1 on success, 0 on failure.
 *
 * This function increases the memory used by @p mem by @p sz bites.
 */
int xpost_memory_file_grow (
        Xpost_Memory_File *mem,
        unsigned int sz);

/**
 * @brief Allocate memory in the given memory file and return offset.
 *
 * @param[in,out] mem The memory file.
 * @param[in] sz The new size.
 * @param[out] addr The offset.
 * @return 1 on success, 0 on failure.
 *
 * This function attempts to allocate @p sz bytes to @p mem and
 * if successful, copies the offset into @p addr.
 * If sz is 0, it just copies the offset and does not allocate any
 * memory at that address.
 *
 * @note May call xpost_memory_file_grow which may invalidate all pointers
 * derived from mem->base. MUST recalculate all VM pointers after this
 * function.
 */
int xpost_memory_file_alloc (
        Xpost_Memory_File *mem,
        unsigned int sz,
        unsigned int *addr);

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
 * @param[out] addr The offset.
 * @return 1 on success, 0 on failure.
 *
 * This function attempts to allocate and initialise
 * a new memory table in @p mem.
 * If successful, the offset of the allocated memory relative to
 * mem->base is stored through the @p addr pointer.
 *
 * MUST recalculate all VM pointers after this function.
 * See note in xpost_memory_file_alloc().
 */
int xpost_memory_table_init (
        Xpost_Memory_File *mem,
        unsigned int *addr);

int xpost_memory_register_free_list_alloc_function(Xpost_Memory_File *mem,
    int (*free_list_alloc)(struct Xpost_Memory_File *mem, unsigned sz, unsigned tag, unsigned int *entity));

int xpost_memory_register_garbage_collect_function(Xpost_Memory_File *mem,
    unsigned int (*garbage_collect)(struct Xpost_Memory_File *mem, int dosweep, int markall));

/**
 * @brief Allocate memory, returns table index.
 *
 * @param[in,out] mem The memory file.
 * @param[in] sz The allocation size.
 * @param[in] tag The allocation tag.
 * @param[out] entity The table index.
 * @return 1 on success, 0 on failure.
 *
 * This function attempts to allocate a new VM entity and associated
 * memory of size @p sz. If successful, the table index for the new
 * entity is stored through the @p entity pointer.
 *
 * MUST recalculate all VM pointers after this function.
 * See note in xpost_memory_file_alloc().
 */
int xpost_memory_table_alloc (
        Xpost_Memory_File *mem,
        unsigned int sz,
        unsigned int tag,
        unsigned int *entity);

/**
 * @brief Find the table and relative entity index for an absolute
 * entity index.
 *
 * @param[in,out] mem The memory file.
 * @param[out] atab The table.
 * @param[in,out] aent The entity.
 * @return 1 on success, 0 on failure.
 *
 * This function takes the memory file and an absolute entity,
 * and writes a pointer to the relevant segment of the table chain
 * through the @p atab pointer. It updates the referenced
 * absolute entity index with an index relative to the table segment.
 */
int xpost_memory_table_find_relative (
        Xpost_Memory_File *mem,
        Xpost_Memory_Table **atab,
        unsigned int *aent);

/**
 * @brief Get the address from an entity.
 *
 * @param[in] mem The memory file.
 * @param[in] ent The entity.
 * @param[out] addr The address.
 * @return 1 on success, 0 on failure.
 *
 * If successful, this function stores the address of the entity
 * @p ent in @p mem, through the @p addr pointer.
 */
int xpost_memory_table_get_addr (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int *addr);

/**
 * @brief Set the address for an entity.
 *
 * @param[in] mem The memory file.
 * @param[in] ent The entity.
 * @param[in] addr The new address.
 * @return 1 on success, 0 on failure.
 *
 * If successful, this function replaces the address for
 * @p ent in @p mem with a new address @p addr.
 */
int xpost_memory_table_set_addr (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int addr);

/**
 * @brief Get the size of an entity.
 *
 * @param[in] mem The memory file.
 * @param[in] ent The entity.
 * @param[out] sz The size.
 * @return 1 on success, 0 on failure.
 *
 * If successful, this function stores the size of the entity
 * @p ent in @p mem through the @p sz pointer.
 */
int xpost_memory_table_get_size (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int *sz);

/**
 * @brief Set the size for an entity.
 *
 * @param[in] mem The memory file.
 * @param[in] ent The entity.
 * @param[in] size The new size.
 * @return 1 on success, 0 on failure.
 *
 * If successful, this function replaces the size for
 * @p ent in @p mem with a new size @p size.
 */
int xpost_memory_table_set_size (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int size);

/**
 * @brief Get the mark field of an entity.
 *
 * @param[in] mem The memory file.
 * @param[in] ent The entity.
 * @param[out] mark The mark field.
 * @return 1 on success, 0 on failure.
 *
 * If successful, this function stores the mark field
 * of the entity @p ent in @p mem through the @p mark pointer.
 */
int xpost_memory_table_get_mark (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int *mark);

/**
 * @brief Set the mark field for an entity.
 *
 * @param[in] mem The memory file.
 * @param[in] ent The entity.
 * @param[in] mark The new mark field.
 * @return 1 on success, 0 on failure.
 *
 * If successful, this function replaces the mark field
 * of the entity @p ent in @p mem with the new value @p mark.
 */
int xpost_memory_table_set_mark (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int mark);

/**
 * @brief Get the tag of an entity.
 *
 * @param[in] mem The memory file.
 * @param[in] ent The entity.
 * @param[out] tag The tag.
 * @return 1 on success, 0 on failure.
 *
 * If successful, this function stores the tag field
 * of the entity @p ent in @p mem through the @p tag pointer.
 */
int xpost_memory_table_get_tag (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int *tag);

/**
 * @brief Set the tag for an entity.
 *
 * @param[in] mem The memory file.
 * @param[in] ent The entity.
 * @param[in] tag The new tag.
 * @return 1 on success, 0 on failure.
 *
 * If successful, this function replaces the tag field
 * of the entity @p ent in @p mem.
 */
int xpost_memory_table_set_tag (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int tag);

/**
 * @brief Fetch a value from a composite object.
 *
 * @param[in,out] mem The memory file.
 * @param[in] ent The entity.
 * @param[in] offset The offset, in units of @p sz bytes.
 * @param[in] sz The size of the transfer.
 * @param[out] dest A buffer
 * @return 1 on success, 0 on failure.
 *
 * This function performs a generic "get" operation from a composite object,
 * or other VM entity such as a file.
 * It is used to retrieve bytes from strings, objects from arrays,
 * FILE*s from files.
 */
int xpost_memory_get (
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
 * @param[in] offset The offset, in units of @p sz bytes.
 * @param[in] sz The size of the transfer.
 * @param[in] src A buffer
 * @return 1 on success, 0 on failure.
 *
 * This function performs a generic "put" operation into a composite object,
 * or other VM entity such as a file.
 * It is used to store bytes in strings, and objects in arrays.
 */
int xpost_memory_put (
        Xpost_Memory_File *mem,
        unsigned int ent,
        unsigned int offset,
        unsigned int sz,
        const void *src);

void xpost_memory_table_dump_ent (Xpost_Memory_File *mem,
                                  unsigned int ent);

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
