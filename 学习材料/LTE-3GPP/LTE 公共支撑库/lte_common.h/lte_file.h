/******************************************************************************* **
 ** Copyright (c) 2006-2010 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: lte_file.h
 ** Description: Header file for wrapping file stream operation functions.
 **
 ** Current Version: 1.0
 ** Author: Jihua Zhou (jhzhou@ict.ac.cn)
 ** Date: 2006.10.27
 **
 ******************************************************************************/

#ifndef FILE_H
#define FILE_H

#include "lte_type.h"

/* Dependencies ------------------------------------------------------------- */

#include <stdio.h>		/*FILE, fpos_t*/

/* Constants ---------------------------------------------------------------- */

/* Types -------------------------------------------------------------------- */

/* Macros ------------------------------------------------------------------- */

/* Globals ------------------------------------------------------------------ */

/* Functions ---------------------------------------------------------------- */
/******************************************************************************
 * Get the length of a file.
 *
 * Input: file_p: pointer to file stream.
 *
 * Output: return length of the file, if success.
 *                EOF, if error ocuurs.
 ******************************************************************************/
extern inline INT32 get_file_len(FILE *file_p);

/******************************************************************************
 * Obtain the current value of the file position indicator.
 *
 * Input: file_p: pointer to file stream.
 *
 * Output: return current position of the file, if success.
 *                EOF, if error ocuurs.
 ******************************************************************************/
extern inline INT32 tell_file(FILE *file_p);

/******************************************************************************
 * Storing the current value of the file offset into
 * the object referenced by pos.
 *
 * Input: file_p: pointer to file stream.
 *
 * Output: pos: pointer to object used to store current position.
 *         return 0, if success.
 *                EOF, if error ocuurs.
 ******************************************************************************/
extern inline INT32 getpos_file(FILE *file_p, fpos_t *pos);

/******************************************************************************
 * Setting the current value of the file offset from
 * the object referenced by pos.
 *
 * Input: file_p: pointer to file stream.
 *        pos: pointer to object storing position to be set.
 *
 * Output: return 0, if success.
 *                EOF, if error ocuurs.
 ******************************************************************************/
extern inline INT32 putpos_file(FILE *file_p, fpos_t *pos);

/******************************************************************************
 * Set the file position indicator.
 *
 * Input: file_p: pointer to file stream.
 *        offset: the new position, measured in bytes, is obtained by
 *                adding offset bytes to the position specified by whence.
 *        whence: where the offset is relative to.
 *                SEEK_SET: the start of the file.
 *                SEEK_CUR: the current position indicator.
 *                SEEK_END: the end of the file.
 *
 * Output: return 0, if success.
 *                EOF, if error ocuurs.
 ******************************************************************************/
extern inline INT32 seek_file(FILE *file_p, INT32 offset, INT32 whence);

/******************************************************************************
 * Test the end-of-file indicator.
 *
 * Input: file_p: pointer to file stream.
 *
 * Output: return non-zero, if end-of-file indicator is set.
 *                0, if it is not set.
 ******************************************************************************/
extern inline INT32 eof_file(FILE *file_p);

/******************************************************************************
 * Test the error indicator.
 *
 * Input: file_p: pointer to file stream.
 *
 * Output: return non-zero, if the error indicator is set.
 *                0, if it is not set.
 ******************************************************************************/
extern inline INT32 error_file(FILE *file_p);

/******************************************************************************
 * Open the file whose name is the string pointed to by path and
 * associates a stream with it.
 *
 * Input: path: Pointer to file path to be opened.
 *        flag: mode of the opening file.
 *              r : Open  text  file  for reading. 
 *                  The stream is positioned at the beginning of the file.
 *              r+: Open for reading and writing. 
 *                  The stream is positioned at the beginning of the file.
 *              w : Truncate  file  to zero length or create text file
 *                  for writing.
 *                  The stream is positioned at the beginning of the file.
 *              w+: Open for reading and writing. 
 *                  The file is created if it does not exist, otherwise
 *                  it is truncated.  
 *                  The stream is positioned at the beginning of the file.
 *              a : Open for appending (writing at end of file). 
 *                  The file is  created if it does not exist. 
 *                  The stream is positioned at the end of the file.
 *              a+: Open for reading and appending (writing at end of file). 
 *                  The file  is  created if it does not exist. 
 *                  The initial file position for reading is at the
 *                  beginning of the file, but output is always
 *                  appended to the end of the file.
 *
 * Output: return pointer to file stream, if succuss.
 *                NULL, if open failed.
 ******************************************************************************/
extern inline FILE *open_file(const char *path, const char *flag);
/******************************************************************************
 * Close a file stream, dissociating the named stream from its underlying
 * file or set of functions.
 * If the stream was being used for output, any buffered data is written first.
 *
 * Input: file_p: pointer to file stream.
 *
 * Output: return 0, if success.
 *                EOF, if error ocuurs.
 ******************************************************************************/
extern inline INT32 close_file(FILE *file_p);

/******************************************************************************
 * Read count elements of data, each size bytes long, from the stream
 * pointed to by file_p, storing them at the location given by buf.
 * 
 * Input: size: size, in unit of byte, of element to be read.
 *        count: how many elements to be read.
 *        file_p: pointer to file stream.
 *
 * Output: buf: pointer to buffer storing data read.
 *         return the number of items (not characters) successfully read,
 *         if an error occurs, or the end-of-file is reached, the return value
 *         may be less than item count (or zero).
 ******************************************************************************/
extern inline INT32 read_file(void *buf, INT32 size, INT32 count, FILE *file_p);

/******************************************************************************
 * Write count elements of data, each size bytes long, to the stream
 * pointed to by file_p, obtaining them from the location given by buf.
 *
 * Input: buf: pointer to buffer storing data to be written.
 *        size: size, in unit of byte, of element to be written.
 *        count: how many elements to be written.
 *        file_p: pointer to file stream.
 *
 * Output: return the number of items (not characters) successfully written,
 *         if an error occurs, or the end-of-file is reached, the return value
 *         may be less than item count (or zero).
 ******************************************************************************/
extern inline INT32 write_file(const void *buf, INT32 size, INT32 count, FILE *file_p);

/******************************************************************************
 * Flush a file stream, forcing a write of all buffered data for the given
 * output or update stream via the stream's underlying write function .
 *
 * Input: file_p: pointer to file stream.
 *
 * Output: return 0, if success.
 *                EOF, if error occurs.
 ******************************************************************************/
extern inline INT32 flush_file(FILE *file_p);

/******************************************************************************
 * Read the next character from a file stream.
 * 
 * Input: file_p: pointer to file stream.
 *
 * Output: return character read as an unsigned char cast to an int, if success.
 *                EOF, if on end of file.
 *                error number, if error occurs.
 ******************************************************************************/
extern inline INT32 getc_file(FILE *file_p);

/******************************************************************************
 * Write a character, cast to an unsigned char, to a file stream.
 * 
 * Input: ch: character to be written.
 *        file_p: pointer to file stream.
 *
 * Output: return the character written as an unsigned char cast to an int.
 *                EOF, if error occurs.
 ******************************************************************************/
extern inline INT32 putc_file(INT32 ch, FILE *file_p);
/******************************************************************************
 * Read in at most one less than size characters from a file stream.
 * 
 * Input: size: how many bytes to be read, (size - 1).
 *        file_p: pointer to file stream.
 *
 * Output: str: pointer to buffer storing string read.
 *         return pointer to str, if success.
 *                NULL, if error occurs, or when end of file occurs
 *                      while no characters have been read.
 ******************************************************************************/
extern inline char *gets_file(char *str, INT32 size, FILE *file_p);

/******************************************************************************
 * Writes a string to a file stream, without its trailing '\0'.
 * 
 * Input: str: pointer to string to be written.
 *        file_p: pointer to file stream.
 *
 * Output: return non-negative number, if success.
 *                EOF, if error occurs.
 ******************************************************************************/
extern inline INT32 puts_file(const char *str, FILE *file_p);

/******************************************************************************
 * Write a formatted string to a file stream.
 * 
 * Input: file_p: pointer to file stream.
 *
 * Output: return the number of characters printed, if success.
 *                negative value, if error occurs.
 ******************************************************************************/
extern inline INT32 printf_file(FILE *file_p, const char *format, ...);



#endif	/* FILE_H */
