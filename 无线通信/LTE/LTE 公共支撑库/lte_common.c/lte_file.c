/*******************************************************************************
 ** 
 ** Copyright (c) 2006-2010 ICT/CAS.
 ** All rights reserved.
 **
 ** File name: lte_file.c
 ** Description: source file for file functions.
 **
 ** Old Version: 1.0
 ** Author: Jihua Zhou (jhzhou@ict.ac.cn)
 ** Date: 2006.01.06
 **
 ******************************************************************************/

/* Dependencies ------------------------------------------------------------- */
#include "lte_file.h"

/* Dependencies ------------------------------------------------------------- */
#include <stdarg.h>	/*va_list,va_start,va_end*/
#include <stdio.h>

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

INT32 get_file_len(FILE *file_p)
{
	fpos_t cur_pos;
	INT32 len;
	
	if (!file_p) {
		return EOF;
	}
	if (getpos_file(file_p, &cur_pos) < 0) {
		return EOF;
	}
	if (seek_file(file_p, 0, SEEK_END) < 0) {
		fsetpos(file_p, &cur_pos);
		return EOF;
	}
	
	len = tell_file(file_p);
	putpos_file(file_p, &cur_pos);

	return len;
}

/******************************************************************************
 * Obtain the current value of the file position indicator.
 *
 * Input: file_p: pointer to file stream.
 *
 * Output: return current position of the file, if success.
 *                EOF, if error ocuurs.
 ******************************************************************************/
INT32 tell_file(FILE *file_p)
{
	return ftell(file_p);
}

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
INT32 getpos_file(FILE *file_p, fpos_t *pos)
{
	return fgetpos(file_p, pos);
}

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
INT32 putpos_file(FILE *file_p, fpos_t *pos)
{
    return fsetpos(file_p, pos);
}

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
INT32 seek_file(FILE *file_p, INT32 offset, INT32 whence)
{
	return fseek(file_p, offset, whence);
}

/******************************************************************************
 * Test the end-of-file indicator.
 *
 * Input: file_p: pointer to file stream.
 *
 * Output: return non-zero, if end-of-file indicator is set.
 *                0, if it is not set.
 ******************************************************************************/
INT32 eof_file(FILE *file_p)
{
	return feof(file_p);	
}

/******************************************************************************
 * Test the error indicator.
 *
 * Input: file_p: pointer to file stream.
 *
 * Output: return non-zero, if the error indicator is set.
 *                0, if it is not set.
 ******************************************************************************/
INT32 error_file(FILE *file_p)
{
	return ferror(file_p);
}

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
FILE *open_file(const char *path, const char *flag)
{
	return fopen(path, flag);
}

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
INT32 close_file(FILE *file_p)
{
	return fclose(file_p);
}

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
INT32 read_file(void *buf, INT32 size, INT32 count, FILE *file_p)
{
	return fread(buf, size, count, file_p);
}
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
INT32 write_file(const void *buf, INT32 size, INT32 count,
								FILE *file_p)
{
	return fwrite(buf, size, count, file_p);
}

/******************************************************************************
 * Flush a file stream, forcing a write of all buffered data for the given
 * output or update stream via the stream's underlying write function .
 *
 * Input: file_p: pointer to file stream.
 *
 * Output: return 0, if success.
 *                EOF, if error occurs.
 ******************************************************************************/
 INT32 flush_file(FILE *file_p)
{
    return fflush(file_p);
}

/******************************************************************************
 * Read the next character from a file stream.
 * 
 * Input: file_p: pointer to file stream.
 *
 * Output: return character read as an unsigned char cast to an int, if success.
 *                EOF, if on end of file.
 *                error number, if error occurs.
 ******************************************************************************/
INT32 getc_file(FILE *file_p)
{
    return fgetc(file_p);
}

/******************************************************************************
 * Write a character, cast to an unsigned char, to a file stream.
 * 
 * Input: ch: character to be written.
 *        file_p: pointer to file stream.
 *
 * Output: return the character written as an unsigned char cast to an int.
 *                EOF, if error occurs.
 ******************************************************************************/
INT32 putc_file(INT32 ch, FILE *file_p)
{
    return fputc(ch, file_p);
}

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
char *gets_file(char *str, INT32 size, FILE *file_p)
{
    return fgets(str, size, file_p);
}

/******************************************************************************
 * Writes a string to a file stream, without its trailing '\0'.
 * 
 * Input: str: pointer to string to be written.
 *        file_p: pointer to file stream.
 *
 * Output: return non-negative number, if success.
 *                EOF, if error occurs.
 ******************************************************************************/
INT32 puts_file(const char *str, FILE *file_p)
{
    return fputs(str, file_p);
}

/******************************************************************************
 * Write a formatted string to a file stream.
 * 
 * Input: file_p: pointer to file stream.
 *
 * Output: return the number of characters printed, if success.
 *                negative value, if error occurs.
 ******************************************************************************/
INT32 printf_file(FILE *file_p, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(file_p, format, args);
	va_end(args);
	return 0;
}

