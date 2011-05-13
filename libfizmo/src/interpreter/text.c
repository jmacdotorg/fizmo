
/* text.c
 *
 * This file is part of fizmo.
 *
 * Copyright (c) 2009-2011 Christoph Ender.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#ifndef text_c_INCLUDED
#define text_c_INCLUDED

#include <string.h>
#include <stdio.h>
#include <ctype.h>

#include "../tools/tracelog.h"
#include "../tools/i18n.h"
#include "../tools/types.h"
#include "../tools/z_ucs.h"
#include "text.h"
#include "fizmo.h"
#include "routine.h"
#include "object.h"
#include "zpu.h"
#include "variable.h"
#include "misc.h"
#include "streams.h"
#include "history.h"
#include "fizmo.h"
#include "math.h"
#include "stack.h"
#include "config.h"
#include "output.h"
#include "savegame.h"
#include "streams.h"
#include "undo.h"
#include "../locales/libfizmo_locales.h"

#ifndef DISABLE_COMMAND_HISTORY
#include "cmd_hst.h"
#endif /* DISABLE_COMMAND_HISTORY */


static uint16_t zchar_storage_word;
static uint16_t zchar_storage_symbols_stored;
static uint8_t *zchar_storage_output;
static uint8_t *zchar_storage_output_index;
static uint8_t *zchar_storage_index_behind;
static uint8_t zchar_storage_word_index;
static size_t interpreter_command_buffer_size = 0;
/*@only@*/ /*@null@*/ z_ucs *interpreter_command_buffer = NULL;

// The following two variables will keep track of the src pointer in
// the various abbreviation depths. In case we return from processing
// an abbreviation the last src pointer may be fetched here, or in case
// the output buffer was full while decoding a Z-Char-encoded string,
// we can continue decoding by using this data.
static uint8_t *zchar_to_z_ucs_src_index[MAX_ABBREVIATION_DEPTH + 1];
static uint8_t zchar_to_z_ucs_current_alphabet[MAX_ABBREVIATION_DEPTH + 1];
static uint8_t zchar_to_z_ucs_current_z_char[MAX_ABBREVIATION_DEPTH + 1];
static uint8_t zchar_to_z_ucs_multibyte_stage[MAX_ABBREVIATION_DEPTH + 1];
static uint8_t zchar_to_z_ucs_multi_z_char[MAX_ABBREVIATION_DEPTH + 1];
static int zchar_to_z_ucs_abbreviation_level;

static uint32_t number_of_commands = 0;
static uint8_t first_word_found;
static z_ucs z_ucs_output_buffer[Z_UCS_OUTPUT_BUFFER_SIZE];

static uint8_t dictionary_entry_length;
static int16_t number_of_dictionary_entries;
static bool dictionary_is_unsorted;
static uint8_t *dictionary_start;

z_ucs z_ucs_newline_string[] = { Z_UCS_NEWLINE, 0 };
static char fizmo_command_prefix_string[] = { FIZMO_COMMAND_PREFIX, '\0' };

uint8_t alphabet_table_v1[] =
{ 
  // A0:
  0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d,
  0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a,
  // A1:
  0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d,
  0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a,
  // A2:
  0x00, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x2e, 0x2c,
  0x21, 0x3f, 0x5f, 0x23, 0x27, 0x22, 0x2f, 0x5c, 0x3c, 0x2d, 0x3a, 0x28, 0x29
};


uint8_t alphabet_table_after_v1[] =
{ 
  // A0:
  0x61, 0x62, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d,
  0x6e, 0x6f, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a,
  // A1:
  0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d,
  0x4e, 0x4f, 0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a,
  // A2:
  0x00, 0x0d, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x2e,
  0x2c, 0x21, 0x3f, 0x5f, 0x23, 0x27, 0x22, 0x2f, 0x5c, 0x2d, 0x3a, 0x28, 0x29
};


static uint8_t default_extra_char_to_unicode_table[] =
{
    69,
  0x00, 0xe4, 0x00, 0xf6, 0x00, 0xfc, 0x00, 0xc4, 0x00, 0xd6, 0x00, 0xdc,
  0x00, 0xdf, 0x00, 0xbb, 0x00, 0xab, 0x00, 0xeb, 0x00, 0xef, 0x00, 0xff,
  0x00, 0xcb, 0x00, 0xcf, 0x00, 0xe1, 0x00, 0xe9, 0x00, 0xed, 0x00, 0xf3,
  0x00, 0xfa, 0x00, 0xfd, 0x00, 0xc1, 0x00, 0xc9, 0x00, 0xcd, 0x00, 0xd3,
  0x00, 0xda, 0x00, 0xdd, 0x00, 0xe0, 0x00, 0xe8, 0x00, 0xec, 0x00, 0xf2,
  0x00, 0xf9, 0x00, 0xc0, 0x00, 0xc8, 0x00, 0xcc, 0x00, 0xd2, 0x00, 0xd9,
  0x00, 0xe2, 0x00, 0xea, 0x00, 0xee, 0x00, 0xf4, 0x00, 0xfb, 0x00, 0xc2,
  0x00, 0xca, 0x00, 0xce, 0x00, 0xd4, 0x00, 0xdb, 0x00, 0xe5, 0x00, 0xc5,
  0x00, 0xf8, 0x00, 0xd8, 0x00, 0xe3, 0x00, 0xf1, 0x00, 0xf5, 0x00, 0xc3,
  0x00, 0xd1, 0x00, 0xd5, 0x00, 0xe6, 0x00, 0xc6, 0x00, 0xe7, 0x00, 0xc7,
  0x00, 0xfe, 0x00, 0xf0, 0x00, 0xde, 0x00, 0xd0, 0x00, 0xa3, 0x01, 0x53,
  0x01, 0x52, 0x00, 0xa1, 0x00, 0xbf
};


// This function must be called prior to zchar_storage_write calls.
// - uint8_t *output: The output buffer that's supposed to hold the result.
//   The maximum supported output buffer size is 2^16 = 65536 bytes.
// - uint16_t output_buffer_size: The maximum size that the output buffer
//   can hold. The size should be a multiple of 2 (we have to store words),
//   in case it's not, the last byte of the output buffer won't be used.
static void zchar_storage_start(uint8_t *output, uint16_t output_buffer_size)
{
  zchar_storage_word = 0;
  zchar_storage_output = output;
  zchar_storage_output_index = output;
  zchar_storage_index_behind = output + (output_buffer_size & 0xfffe);
  zchar_storage_word_index = 0;
  zchar_storage_symbols_stored = 0;
}


// This method will accept input for the zchar_storage.
// - unsigned int five_bits: Ths input bits are expected to be stored in the
//   lower five bits of this parameter.
// - Return value:
//   *  0 in case there is at least space for one more zchar in the buffer.
//   *  1 in case no more zchars may be written to the current buffer. In
//        this case, the output should be read, stored away, and after that
//        the zchar_storage_reset method should be called.
//   * -1 in case the given five_bits could not be stored. These will be lost.
static int zchar_storage_write(unsigned int five_bits)
{
  five_bits &= 0x1f;

  if (zchar_storage_word_index == 0)
    zchar_storage_word |= (five_bits << 10);
  else if (zchar_storage_word_index == 1)
    zchar_storage_word |= (five_bits << 5);
  else
    zchar_storage_word |= five_bits;

  zchar_storage_symbols_stored++;

  if (++zchar_storage_word_index == 3)
  {
    if (zchar_storage_output_index == zchar_storage_index_behind)
      return -1;

    store_word(zchar_storage_output_index, zchar_storage_word);
    zchar_storage_output_index += 2;
    zchar_storage_word = 0;
    zchar_storage_word_index = 0;
  }

  if (zchar_storage_output_index == zchar_storage_index_behind)
    return 1;
  else
    return 0;
}


// This function will throw away the contents of the Z-Char output buffer
// and empty the word that's currently assembled. This is used in case
// the output buffer is full (zchar_storage_write returns 1). In this case
// the buffer may be emptied and storage may continue by calling this
// function.
// PLEASE NOTE: In case the output is supposed to be finished directly
// after filling up the buffer, the zchar_storage_finish has to be
// called directly after the last write call. If you receive a 1 from the
// zchar_storage_write function, empty and reset the output buffer and
// call the zchar_storage_finish function after that, the output will
// contain an extra empty padding word. Technically it should be okay though,
// the spec says that an "indefinite sequence of shift and shift lock
// characters is legal" (section 3.2.4).
void zchar_storage_reset()
{
  zchar_storage_output_index = zchar_storage_output;
  zchar_storage_word = 0;
  zchar_storage_word_index = 0;
  zchar_storage_symbols_stored = 0;
}


// This function will finish the current Z-Char-String in production by
// setting bit 15 on the last word, padding it with 5s if required and
// storing it in the output buffer.
static void zchar_storage_finish()
{
  if (zchar_storage_word_index == 0)
    // In case we arrive here, there's no output in the current word. We'll
    // check if there's any output already stored in the output buffer.
    if (zchar_storage_output_index != zchar_storage_output)
      // In case there's some output, write the string termination to the
      // last word of the output.
      *(zchar_storage_output_index-2) |= 0x80;
    else
      // In case the buffer is completely empty, write an empty word
      // padded with 5s and terminated with bit 15 set.
      store_word(zchar_storage_output, 0x94a5);

  else
  {
    // In case the current word has some content, we'll just have pad it
    // up with 5s and store it.
    if (zchar_storage_word_index == 1)
      zchar_storage_word |= (5 << 5);

    zchar_storage_word |= 0x8005;

    store_word(zchar_storage_output_index, zchar_storage_word);
  }
}


static void store_ZSCII_as_zchar(zscii zscii_char)
{
  uint8_t i;

  if (active_z_story == NULL)
    return;

  if (zscii_char == 32)
  {
    (void)zchar_storage_write(0);
    return;
  }

  for (i=0; i<78; i++)
    if (active_z_story->alphabet_table[i] == zscii_char)
      break;

  if (i < 26)
    (void)zchar_storage_write((unsigned)(i + 6));
  else if (i < 52)
  {
    (void)zchar_storage_write(4);
    (void)zchar_storage_write((unsigned)(i - 20));
  }
  else if (i < 78)
  {
    (void)zchar_storage_write(5);
    (void)zchar_storage_write((unsigned)(i - 46));
  }
  else
  {
    /*
    1-7     ----
    8       delete  Input
    9       tab (V6)        Output
    10      ----    
    11      sentence space (V6)     Output
    12      ----    
    13      newline         Input/Output 
    14-26   ----    
    27      escape  Input
    28-31   ----
    32-126  standard ASCII  Input/Output
    127-128         ----
    129-132         cursor u/d/l/r  Input
    133-144         function keys f1 to f12         Input
    145-154         keypad 0 to 9   Input
    155-251         extra characters        Input/Output
    252     menu click (V6)         Input
    253     double-click (V6)       Input
    254     single-click    Input
    255-1023        ----
    */
    if (
        (zscii_char == 8) || // delete
        //(zscii_char == 13) || // newline (?)
        (zscii_char == 27) || // escape
        // 32 has already been handeled at the start of this function.
        ((zscii_char >= 33) && (zscii_char <= 126))  || //standard ASCII
        //((zscii_char >= 129) && (zscii_char <= 132)) || //crsr u/d/l/r
        //((zscii_char >= 133) && (zscii_char <= 144)) || //f1-f12
        //((zscii_char >= 145) && (zscii_char <= 154)) || //keypad 0-9
        ((zscii_char >= 155) && (zscii_char <= 251)) // || //extra chars
        //(zscii_char == 252) // menu click(V6)
        //(zscii_char == 253) // double click(V6)
        //(zscii_char == 254) // single click(V6)
       )
    {
      //store_z_char(active_z_story->alphabet_table[52]);
      //store_z_char((zscii_char & 0xe0) >> 5);
      //store_z_char(zscii_char & 0x1f);

      // Wrong: never used.
      //zchar_storage_write(active_z_story->alphabet_table[52]);

      (void)zchar_storage_write(5);
      (void)zchar_storage_write(6);
      (void)zchar_storage_write((unsigned)((zscii_char & 0xe0) >> 5));
      (void)zchar_storage_write((unsigned)(zscii_char & 0x1f));
    }
    else
      i18n_translate_and_exit(
          libfizmo_module_name,
          i18n_libfizmo_UNKNOWN_CHAR_CODE_P0D,
          -1,
          (long int)zscii_char);
  }
}


/*@dependent@*/ static uint8_t *get_current_unicode_table_address()
{
  // 3.8.5.2: In Version 5 or later, if Word 3 of the header extension
  // table is present and non-zero then it is interpreted as the byte
  // address of the Unicode translation table.
  //
  if (
      (ver < 5)
      ||
      (header_extension_table_size < 3)
      ||
      (load_word(header_extension_table + 0x6) == 0)
     )
  {
    return default_extra_char_to_unicode_table;
  }
  else
  {
    return z_mem + load_word(header_extension_table + 0x6);
  }
}


static z_ucs zscii_extra_char_to_unicode(zscii zscii_extra_char)
{
  uint8_t *unicode_table = get_current_unicode_table_address();
  uint8_t table_index = zscii_extra_char - 155;

  TRACE_LOG("Extra-char-translation: %d, %p, %d.\n",
      *unicode_table - 1, unicode_table, table_index);

  if (table_index > *unicode_table - 1)
  {
    TRACE_LOG("too large\n");
    return latin1_char_to_zucs_char('?');
  }
  else
    return (z_ucs)load_word(unicode_table + 1 + (table_index * 2));
}


static zscii find_zscii_extra_char_in_unicode_table(uint16_t unicode_char)
{
  uint8_t *unicode_table = get_current_unicode_table_address();
  uint8_t *unicode_data = unicode_table + 1;
  int number_of_table_entries = (int)(*unicode_table - 1);
  int table_index = 0;

  TRACE_LOG("Looking for extra-char %d in unicode table.\n", unicode_char);

  while (table_index <= number_of_table_entries)
  {
    if (load_word(unicode_data + (table_index * 2)) == unicode_char)
      return (zscii)(table_index + 155);
    table_index++;
  }

  return 0xff;
}


zscii unicode_char_to_zscii_input_char(z_ucs unicode_char)
{
  zscii result;

  if (unicode_char == 8)
    result = 8;
  else if (unicode_char == 10)
    result = 13;
  else if (unicode_char == 13)
    result = 13;
  else if (unicode_char == 27)
    result = 27;
  else if ((unicode_char >= 32) && (unicode_char <= 126))
    result = (uint8_t)unicode_char;
  else
  {
    // In case we haven't found out input yet, there's still a change that
    // this unicode character has been defined in the translation table, thus
    // we try to look it up there.
    result = find_zscii_extra_char_in_unicode_table(unicode_char);
  }

  return result;
}


// This will write into(!) the provided textbuffer!
static uint8_t locate_dictionary_entry(
    uint8_t *text_buffer,
    uint8_t **parse_position,
    uint8_t word_position,
    uint8_t token_length,
    bool dont_write_unrecognized_words_to_parse_buffer)
{
  uint8_t *dictionary_index = dictionary_start;
  uint16_t i,j;
  uint16_t word_length;

  // "Dictionarize" entry.
  if (ver >= 4)
  {
    word_length = 6;

    if ((text_buffer[0] & 0x80) != 0)
    {
      text_buffer[0] &= 0x7f;
      store_word(text_buffer + 2, 0x14a5);
      store_word(text_buffer + 4, 0x94a5);
    }
    else if ((text_buffer[2] & 0x80) != 0)
    {
      text_buffer[2] &= 0x7f;
      store_word(text_buffer + 4, 0x94a5);
    }
    else if ((text_buffer[4] & 0x80) == 0)
    {
      text_buffer[4] |= 0x80;
    }
  }
  else
  {
    word_length = 4;

    if ((text_buffer[0] & 0x80) != 0)
    {
      text_buffer[0] &= 0x7f;
      store_word(text_buffer + 2, 0x94a5);
    }
    else if ((text_buffer[2] & 0x80) == 0)
    {
      text_buffer[2] |= 0x80;
    }
  }

  // FIXME: Improve searching (use "dictionary_is_unsorted").
  for (i=0; i<number_of_dictionary_entries; i++)
  {
    /*
    TRACE_LOG("%d (at %x): %x%x%x%x / %x%x%x%x\n",
        i,
        dictionary_index - z_mem,
        text_buffer[0],
        text_buffer[1],
        text_buffer[2],
        text_buffer[3],
        dictionary_index[0],
        dictionary_index[1],
        dictionary_index[2],
        dictionary_index[3]);
    */

    for (j=0; j<word_length; j++)
      if (text_buffer[j] != dictionary_index[j])
        break;

    if (j == word_length)
      break;

    dictionary_index += dictionary_entry_length;
  }

  if (i != number_of_dictionary_entries)
  {
    TRACE_LOG("Found string at index %d.\n", i);
    TRACE_LOG("Storing %lx at %lx.\n",
        (unsigned long int)(dictionary_index - z_mem),
        (unsigned long int)(*parse_position - z_mem));
    store_word(*parse_position, (uint16_t)(dictionary_index - z_mem));
    (*parse_position) += 2;
  }
  else
  {
    TRACE_LOG("Could not find string.\n");
    if (dont_write_unrecognized_words_to_parse_buffer == false)
    {
      store_word(*parse_position, 0);
      (*parse_position) += 2;
    }
  }

  if (dont_write_unrecognized_words_to_parse_buffer == false)
  {
    TRACE_LOG("Storing word length %d at %lx.\n",
        token_length,
        (unsigned long int)(*parse_position - z_mem));
    **parse_position = token_length;
    (*parse_position)++;

    TRACE_LOG("Storing word position %d at %lx.\n", word_position,
        (unsigned long int)(*parse_position - z_mem));
    **parse_position = word_position;
    (*parse_position)++;
  }

  if (i != number_of_dictionary_entries)
  {
    if (word_position == 1)
      first_word_found = 1;
    return 1;
  }
  else
    return 0;
}


static void tokenise(
    uint8_t *z_text_buffer,
    uint8_t z_text_buffer_offset,
    uint16_t input_length,
    uint8_t *z_parse_buffer,
    uint8_t *dictionary,
    bool dont_write_unrecognized_words_to_parse_buffer)
{
  // One normal input byte will result in a maximum of three Z-Chars: one
  // for the '6' header chars, then 2 Z-Chars containing the value. Thus
  // we'll need ((n*3)/3)*2 = n*2 bytes of input length: input_length * 3
  // gives the maximum possible number of Z-Chars. One word will take up
  // 3 Z-Chars, so we'll need a maximum of (n*3)/3 words, meaning n*2 bytes.
  uint8_t tokenize_buffer_length = input_length * 2;
  uint8_t tokenize_buffer[tokenize_buffer_length];

  // The number and values of non-space-seperating input codes:
  uint8_t number_of_input_codes;
  uint8_t *input_codes;

  uint8_t words_parsed = 0;
  uint8_t current_char;
  uint8_t space_found = 0;
  uint8_t non_space_seperator_found = 0;
  uint8_t end_of_line_found;
  uint8_t maximum_words;
  uint8_t number_of_words_found = 0;
  uint8_t current_word_start = z_text_buffer_offset;
  uint8_t *parse_buffer_index = z_parse_buffer + 2;
  uint8_t i;
  //int last_token_start_index;

  TRACE_LOG("Parse buffer at %lx.\n",
    (unsigned long int)(z_parse_buffer - z_mem));

  maximum_words = *z_parse_buffer;
  parse_buffer_index = z_parse_buffer + 2;

  TRACE_LOG("Maximum number of parsed words: %d.\n", maximum_words);

  number_of_input_codes = *(dictionary++);
  input_codes = dictionary;
  dictionary += number_of_input_codes;
  dictionary_entry_length = *(dictionary++);
  number_of_dictionary_entries = (int16_t)load_word(dictionary);
  if (number_of_dictionary_entries < 0)
  {
    dictionary_is_unsorted = true;
    number_of_dictionary_entries = -number_of_dictionary_entries;
  }
  else
  {
    dictionary_is_unsorted = false;
  }
  dictionary_start = dictionary + 2;

#ifdef ENABLE_TRACING
  TRACE_LOG("Number of input codes: %d.\n", number_of_input_codes);
  for (i=0; i<number_of_input_codes; i++)
    TRACE_LOG("Input code[%d]:%x.\n", i, input_codes[i]);
  TRACE_LOG("entry_length: %d.\n", dictionary_entry_length);
  TRACE_LOG("input length: %d.\n", input_length);
  TRACE_LOG("Number entries: %d.\n", number_of_dictionary_entries);
#endif /* ENABLE_TRACING */

  // tokenize_buffer does not have to be defined, is output buffer.
  /*@-compdef@*/
  zchar_storage_start(tokenize_buffer, tokenize_buffer_length);
  /*@-compdef@*/

  end_of_line_found = 0;
  //last_token_start_index = 0;
  while ((words_parsed < maximum_words) && (end_of_line_found == 0))
  {
    TRACE_LOG("Looking at %c/%d.\n", z_text_buffer[z_text_buffer_offset],
        z_text_buffer[z_text_buffer_offset]);
    TRACE_LOG("current-word-start: %d.\n", current_word_start);
    TRACE_LOG("z-text-buffer-offset: %d.\n", z_text_buffer_offset);

    current_char = z_text_buffer[z_text_buffer_offset];

    if (
        (current_char == 0)
        ||
        ( (ver >= 5) && (z_text_buffer_offset -2 == input_length) )
       )
      end_of_line_found = 1;

    else if (current_char == 32)
      space_found = 1;

    else
    {
      for (i=0; i<number_of_input_codes; i++)
        if (current_char == input_codes[i])
        {
          non_space_seperator_found = 1;
          break;
        }

      /*
      if ((non_space_seperator_found == 0) && (last_token_start_index == -1))
        last_token_start_index = z_text_buffer_offset;
        */
    }

    if (space_found == 1)
    {
      // In case we've found a space, things are simple. We don't have to
      // store anything, since spaces are ommitted in the parse list, we
      // only stop storing the current word and start parsing a new one.
      if (zchar_storage_symbols_stored > 0)
      {
        zchar_storage_finish();
        number_of_words_found++;

        (void)locate_dictionary_entry(
            tokenize_buffer,
            &parse_buffer_index,
            current_word_start,
            z_text_buffer_offset - current_word_start,
            //z_text_buffer_offset - last_token_start_index - 1,
            dont_write_unrecognized_words_to_parse_buffer);

        zchar_storage_start(tokenize_buffer, tokenize_buffer_length);
      }
      space_found = 0;
      current_word_start = z_text_buffer_offset + 1;
    }

    else if (non_space_seperator_found == 1)
    {
      // Non-space-seperator are a bit more work, since they also count
      // as a single word. So we stop the current word and after that,
      // initiate a new one containing only the current char.
      //if (z_symbols_stored > 0)
      if (zchar_storage_symbols_stored > 0)
      {
        zchar_storage_finish();
        number_of_words_found++;

        (void)locate_dictionary_entry(tokenize_buffer,
            &parse_buffer_index,
            current_word_start,
            z_text_buffer_offset - current_word_start,
            //z_text_buffer_offset - last_token_start_index - 1,
            dont_write_unrecognized_words_to_parse_buffer);

        zchar_storage_start(tokenize_buffer, tokenize_buffer_length);
      }

      store_ZSCII_as_zchar(current_char);
      zchar_storage_finish();
      number_of_words_found ++;

      (void)locate_dictionary_entry(
          tokenize_buffer,
          &parse_buffer_index,
          z_text_buffer_offset,
          1,
          dont_write_unrecognized_words_to_parse_buffer);
      zchar_storage_start(tokenize_buffer, tokenize_buffer_length);

      non_space_seperator_found = 0;
      current_word_start = z_text_buffer_offset + 1;
    }

    else if (end_of_line_found == 1)
    {
      zchar_storage_finish();
      if (zchar_storage_symbols_stored != 0)
      {
        /*
        TRACE_LOG("%x / %x / %d / %d / %d\n",
            tokenize_buffer,
            parse_buffer_index,
            current_word_start,
            z_text_buffer_offset,
            last_token_start_index);
        */
        number_of_words_found ++;

        (void)locate_dictionary_entry(
            tokenize_buffer,
            &parse_buffer_index,
            current_word_start,
            //z_text_buffer_offset - last_token_start_index - 1);
            // line above from "kill troll ###" bug.
            //z_text_buffer_offset - current_word_start);
            z_text_buffer_offset - current_word_start,
            dont_write_unrecognized_words_to_parse_buffer);

      }
    }

    else
      store_ZSCII_as_zchar(current_char);

    z_text_buffer_offset++;
  }

  TRACE_LOG("Found %d word(s), storing %d to %lx.\n", number_of_words_found,
      number_of_words_found,
      (unsigned long int)(z_parse_buffer + 1 - z_mem));

  z_parse_buffer[1] = number_of_words_found;
}


// - z_ucs *z_ucs_dest: A pointer to the output buffer.
// - uint16_t z_ucs_dest_length: The size of the output buffer.
// - uint8_t *zscii_src: Pointer to source data. In case it is NULL,
//     it means continue processing where we left off last time because
//     if buffer size constraints (see below).
// - Return value:
//   * The pointer to the byte behind the zscii_src, in case there was
//     enough space in z_ucs_dest to write everthing into that buffer,
//   * NULL in case the string could not be decoded completely due to
//     buffer size constraints. In this case, simply call zchar_to_z_ucs
//     again with zscii_src == NULL after clearing the buffer to continue
//     decoding.
/*@temp@*/ /*@null@*/ static uint8_t *zchar_to_z_ucs(
    z_ucs *z_ucs_dest,
    uint16_t z_ucs_dest_length,
    uint8_t *zchar_src)
{
  z_ucs *z_ucs_dest_index = z_ucs_dest;
  z_ucs *z_ucs_dest_last_valid_index = z_ucs_dest + z_ucs_dest_length - 1;
  uint16_t current_three_zchars;
  uint8_t current_zchar;
  uint8_t current_alphabet;
  uint8_t next_char_alphabet = 0;
  uint8_t abbreviation_block = 0xff;
  uint8_t *abbreviation_entry;
  uint8_t i;
  uint16_t output_data = 0; // init to inhibit compiler warning
  int output_data_ready = 0;

  // zchar-processing is not possible with a z-story for the abbreviation
  // decoding.
  if (active_z_story == NULL)
    return NULL;

  // In case we continue processing an old string which was interrupted
  // due to a full output buffer, we keep the current source-indices.
  // Else, we initialize the source information.
  if (zchar_src == NULL)
  {
    zchar_src
      = zchar_to_z_ucs_src_index[zchar_to_z_ucs_abbreviation_level];
    current_alphabet
      = zchar_to_z_ucs_current_alphabet[zchar_to_z_ucs_abbreviation_level];
    i =
      zchar_to_z_ucs_current_z_char[zchar_to_z_ucs_abbreviation_level];
  }
  else
  {
    zchar_to_z_ucs_abbreviation_level = 0;
    zchar_to_z_ucs_multibyte_stage[0] = 0;
    current_alphabet = 0;
    i = 0;
  }

  TRACE_LOG("Starting conversion at %p, abbrlevel:%d, char[%d/3].\n",
      zchar_src, zchar_to_z_ucs_abbreviation_level, i);

  current_three_zchars = load_word(zchar_src);

  while (
      ((current_three_zchars & 0x8000) == 0) ||
      (zchar_to_z_ucs_abbreviation_level > 0) ||
      (i != 3))
  {
    TRACE_LOG("%p / %p / %p.\n",
        z_ucs_dest,
        z_ucs_dest_index,
        z_ucs_dest_last_valid_index);

    // Check if we're at a position in the output buffer that will accept
    // at least one more character.
    if (z_ucs_dest_index == z_ucs_dest_last_valid_index)
    {
      // If there is no more space in the output buffer, we'll store all
      // current source-index information and return NULL as an indication
      // that there's more input waiting to be processed.
      zchar_to_z_ucs_src_index[zchar_to_z_ucs_abbreviation_level]
        = zchar_src;
      zchar_to_z_ucs_current_alphabet[zchar_to_z_ucs_abbreviation_level]
        = current_alphabet;

      TRACE_LOG("(1:%d)\n", 
          zchar_to_z_ucs_current_z_char[zchar_to_z_ucs_abbreviation_level]);

      zchar_to_z_ucs_current_z_char[zchar_to_z_ucs_abbreviation_level]
        = i;

      TRACE_LOG("(2:%d)\n", 
          zchar_to_z_ucs_current_z_char[zchar_to_z_ucs_abbreviation_level]);

      TRACE_LOG("dest-index: %p / czc: %p/%p\n",
          z_ucs_dest_index,
          &(zchar_to_z_ucs_current_z_char[0]),
          &(zchar_to_z_ucs_current_z_char[1]));

      *z_ucs_dest_index = 0;

      TRACE_LOG("(3:%d)\n", 
          zchar_to_z_ucs_current_z_char[zchar_to_z_ucs_abbreviation_level]);

      TRACE_LOG("Interrupted conversion at %p, abbrlevel %d, [%d->%d/3].\n",
          zchar_src,
          zchar_to_z_ucs_abbreviation_level,
          i,
          zchar_to_z_ucs_current_z_char[zchar_to_z_ucs_abbreviation_level]);

      return NULL;
    }

    if (((current_three_zchars & 0x8000) != 0) && (i == 3))
    {
      // Here we're finishing an abbreviation (since the complete end-of-string
      // case is handeled in the loop's while condition)
      zchar_to_z_ucs_abbreviation_level--;

      zchar_src
        = zchar_to_z_ucs_src_index[zchar_to_z_ucs_abbreviation_level];
      current_alphabet
        =zchar_to_z_ucs_current_alphabet[zchar_to_z_ucs_abbreviation_level];
      current_three_zchars
        = load_word(zchar_src);
      i
        = zchar_to_z_ucs_current_z_char[zchar_to_z_ucs_abbreviation_level] + 1;

      next_char_alphabet = 0;

      // Re-start to check if we're already at the end of the string.
      continue;
    }

    if (i == 3)
    {
      // Read the next three Z-Chars from the input.
      zchar_src += 2;
      current_three_zchars = load_word(zchar_src);
      i = 0;
    }

    current_zchar = (current_three_zchars & (0x7c00 >> (i*5))) >> ((2-i)*5);

    TRACE_LOG("Current Z-Char: %d.\n", current_zchar);
    TRACE_LOG("Abbreviation-level: %d.\n", zchar_to_z_ucs_abbreviation_level);

    TRACE_LOG("%d/%d/%d/%d\n",
        ver, current_zchar, current_alphabet, current_zchar);

    if (abbreviation_block != 0xff)
    {
      if (zchar_to_z_ucs_abbreviation_level + 1 > MAX_ABBREVIATION_DEPTH)
        (void)i18n_translate_and_exit(
            libfizmo_module_name,
            i18n_libfizmo_MAXIMUM_ABBREVIATION_DEPTH_IS_P0D,
            -1,
            (long int)MAX_ABBREVIATION_DEPTH);

      zchar_to_z_ucs_src_index[zchar_to_z_ucs_abbreviation_level]
        = zchar_src;
      zchar_to_z_ucs_current_z_char[zchar_to_z_ucs_abbreviation_level]
        = i; // Remember that the current position has been processed.
      zchar_to_z_ucs_current_alphabet[zchar_to_z_ucs_abbreviation_level]
        = current_alphabet;

      // In case we're processing an abbreviation, calculate the location
      // of the abbreviation address in the abbreviation table.
      abbreviation_entry
        = active_z_story->abbreviations_table
        + (((abbreviation_block << 5) + current_zchar) << 1);

      current_alphabet = 0;
      zchar_src = z_mem + load_word(abbreviation_entry) * 2;
      current_three_zchars = load_word(zchar_src);
      i = 0;
      zchar_to_z_ucs_abbreviation_level++;
      zchar_to_z_ucs_multibyte_stage[zchar_to_z_ucs_abbreviation_level] = 0;
      abbreviation_block = 0xff;
    }

    else if (
        zchar_to_z_ucs_multibyte_stage[zchar_to_z_ucs_abbreviation_level] == 1)
    {
      // If we arrive here, we have to read the first part, the higher
      // five bits of the final ten-bit-code.
      zchar_to_z_ucs_multi_z_char[zchar_to_z_ucs_abbreviation_level]
        = (current_zchar << 5);

      // After reading, we'll store the information that in the next
      // iteration we'll have to process the lower five bits.
      zchar_to_z_ucs_multibyte_stage[zchar_to_z_ucs_abbreviation_level] = 2;
      i++;
    }

    else if (
        zchar_to_z_ucs_multibyte_stage[zchar_to_z_ucs_abbreviation_level] == 2)
    {
      // In case we're in stage 2, we read the lower five bits.
      output_data
        = zchar_to_z_ucs_multi_z_char[zchar_to_z_ucs_abbreviation_level]
        | current_zchar;

      //TRACE_LOG("Multibyte-output-data: %x '%c'.\n",output_data,output_data);

      // After that, we'll reset the multibyte-processing-stage to 0.
      zchar_to_z_ucs_multibyte_stage[zchar_to_z_ucs_abbreviation_level] = 0;

      output_data_ready = 1;
      i++;
    }

    else if ((next_char_alphabet == 2) && (current_zchar == 6))
    {
      zchar_to_z_ucs_multibyte_stage[zchar_to_z_ucs_abbreviation_level] = 1;
      i++;
    }

    else if (current_zchar == 0)
    {
      output_data_ready = 1;
      output_data = 32;
      i++;
    }

    else if (
        ((ver == 1) && (current_zchar == 1))
        ||
        (
         (
          (current_zchar == 7)
          &&
          (
           (
            (zchar_to_z_ucs_multibyte_stage[zchar_to_z_ucs_abbreviation_level]
             != 0)
            &&
            (current_alphabet == 2)
           )
           ||
           (
            (zchar_to_z_ucs_multibyte_stage[zchar_to_z_ucs_abbreviation_level]
             ==0)
            &&
            (next_char_alphabet == 2)
           )
           )
         )
        )
       )
    {
      // The upper line is for version 1, the lower for custom alphabets
      // (see 3.5.5.1).
      output_data_ready = 1;
      output_data = 13;
      i++;
    }

    else if ((ver <= 2) && (current_zchar == 4))
    {
      if (++current_alphabet == 3)
        current_alphabet = 0;
      next_char_alphabet = current_alphabet;
      i++;
    }

    else if ((ver <= 2) && (current_zchar == 5))
    {
      if (--current_alphabet == 255)
        current_alphabet = 2;
      next_char_alphabet = current_alphabet;
      i++;
    }

    else if (((ver <= 2) && (current_zchar == 2))
        || ((ver > 2) && (current_zchar == 4)))
    {
      if (++next_char_alphabet == 3)
        next_char_alphabet = 0;
      i++;
    }

    else if (((ver <= 2) && (current_zchar == 3))
        || ((ver > 2) && (current_zchar == 5)))
    {
      if (--next_char_alphabet == 255)
        next_char_alphabet = 2;
      i++;
    }

    else if (((ver >= 3)
          && ((current_zchar==1)||(current_zchar==2)||(current_zchar==3)))
        || ((ver == 2) && ((current_zchar == 1))))
    {
      abbreviation_block = current_zchar - 1;
      i++;
    }

    else if (current_zchar >= 6)
    {
      output_data_ready = 1;
      output_data = (active_z_story->alphabet_table
          + (next_char_alphabet * 26))[current_zchar - 6];
      i++;
    }

    else
      i18n_translate_and_exit(
          libfizmo_module_name,
          i18n_libfizmo_UNKNOWN_ERROR_CASE,
          -1);

    if (output_data_ready != 0)
    {
      /*@-usedef@*/
      *z_ucs_dest_index = zscii_output_char_to_z_ucs(output_data);
      /*@+usedef@*/

      z_ucs_dest_index++;

      //TRACE_LOG("output-data: %d.\n", output_data);
      //TRACE_LOG("output-ptr: %x.\n", z_ucs_dest_index);
      //zscii_char_to_z_ucs(&z_ucs_dest_index, output_data);
      //TRACE_LOG("output-ptr: %x.\n", z_ucs_dest_index);
      output_data_ready = 0;

      if (next_char_alphabet != current_alphabet)
        next_char_alphabet = current_alphabet;
    }
  }

  *z_ucs_dest_index = 0;

  TRACE_LOG("Finished conversion at %p.\n", zchar_src);

  return zchar_src + 2;
}


/*@dependent@*/ static uint8_t *output_zchar_to_streams(uint8_t *zchar_src)
{
  TRACE_LOG("Converting zchars from %lx.\n",
    (unsigned long int)(zchar_src - z_mem));

  do
  {
    zchar_src = zchar_to_z_ucs(
        z_ucs_output_buffer,
        Z_UCS_OUTPUT_BUFFER_SIZE,
        zchar_src);

    TRACE_LOG("zchar-src %p, mbstage: %d, currentchar: %d, abbrlevel %d\n",
        zchar_to_z_ucs_src_index[zchar_to_z_ucs_abbreviation_level],
        zchar_to_z_ucs_multibyte_stage[zchar_to_z_ucs_abbreviation_level],
        zchar_to_z_ucs_current_z_char[zchar_to_z_ucs_abbreviation_level],
        zchar_to_z_ucs_abbreviation_level);

    (void)streams_z_ucs_output(z_ucs_output_buffer);
  }
  while (zchar_src == NULL);

  return zchar_src;
}


void opcode_tokenise(void)
{
  /*@-nullderef@*/
  uint8_t *dictionary_table = active_z_story->dictionary_table;
  /*@-nullderef@*/

  bool dont_write_unrecognized_words_to_parse_buffer = false;
  int offset = (ver >= 5 ? 2 : 1);
  size_t string_length;

  TRACE_LOG("Opcode: TOKENISE.\n");

  if (number_of_operands > 2)
  {
    if (op[2] != 0)
      dictionary_table = z_mem + op[2];

    if ((number_of_operands > 3) && (op[3] != 0))
    {
      dont_write_unrecognized_words_to_parse_buffer = true;
    }
  }

  if (ver < 5)
    string_length = strlen((char*)(z_mem + op[0] + offset));
  else
    string_length = *(z_mem + op[0] + 1);

  TRACE_LOG("Offset: %d.\n", offset);
  TRACE_LOG("dont_write_unrecognized_words_to_parse_buffer: %d.\n",
      dont_write_unrecognized_words_to_parse_buffer);
  TRACE_LOG("Offset: %d.\n", offset);
  TRACE_LOG("String length: %d.\n", (int)string_length);

  tokenise(
      z_mem + op[0],
      (uint8_t)offset,
      string_length,
      z_mem + op[1],
      dictionary_table,
      dont_write_unrecognized_words_to_parse_buffer);
}


void display_status_line(void)
{
  uint16_t current_room_object;

  if (active_interface->is_status_line_available() == false)
    return;

  current_room_object = get_variable(0x10);
  
  if (current_room_object != 0)
    (void)zchar_to_z_ucs(
        z_ucs_output_buffer,
        Z_UCS_OUTPUT_BUFFER_SIZE,
        (get_objects_property_table(get_variable(0x10))) + 1);
  else
    *z_ucs_output_buffer = 0;

  active_interface->show_status(
      z_ucs_output_buffer,
      (int)active_z_story->score_mode,
      get_variable(0x11),
      get_variable(0x12));
}


z_ucs zscii_input_char_to_z_ucs(zscii zscii_input)
{
  // The result will fit into a 16 bit-wide space, since the Z-Spec 1.0
  // only defines 16-bit unicode characters.
  z_ucs result = -1; // init to inhibit compiler warning

  TRACE_LOG("Converting ZSCII-Input(!)-Char %d to UCS-4.\n", zscii_input);

  if (zscii_input == 0)
    result = (z_ucs)0;
  else if (zscii_input == 8)
    result = (z_ucs)8; // Delete
  else if (zscii_input == 13)
    result = Z_UCS_NEWLINE;
  else if (zscii_input == 27)
    result = (z_ucs)27; // Escape
  else if ((zscii_input >= 32) && (zscii_input <= 126))
    result = (z_ucs)zscii_input; // Identical to ASCII in this range.
  else if ((zscii_input >= 129) && (zscii_input <= 154))
    result = (z_ucs)zscii_input; // cursor, f1-f12, keypad
  else if ((zscii_input >= 155) && (zscii_input <= 251))
    result = zscii_extra_char_to_unicode(zscii_input);
  else if ((zscii_input >= 252) && (zscii_input <= 254))
    result = (z_ucs)zscii_input; // mouse-clicks
  else
    i18n_translate_and_exit(
        libfizmo_module_name,
        i18n_libfizmo_INVALID_ZSCII_INPUT_CODE_P0D,
        -1,
        (long)zscii_input);

  TRACE_LOG("Result: %d.\n", (unsigned)result);

  return result;
}


bool is_valid_zscii_output_char(zscii zscii_output)
{
  if (
      (zscii_output == 0)
      ||
      (zscii_output == 9)
      ||
      (zscii_output == 11)
      ||
      (zscii_output == 13)
      ||
      ( (zscii_output >= 32) && (zscii_output <= 126) )
      ||
      ( (zscii_output >= 155) && (zscii_output <= 251) )
     )
    return true;
  else
    return false;
}


z_ucs zscii_output_char_to_z_ucs(zscii zscii_output)
{
  z_ucs result = -1; // init for compiler warning

  TRACE_LOG("Converting ZSCII-Output(!)-Char %d to UCS-4.\n", zscii_output);

  if (zscii_output == 0)
    result = 0;
  else if (zscii_output == 9)
    result = latin1_char_to_zucs_char('\t');
  else if (zscii_output == 11)
    result = latin1_char_to_zucs_char('.');
  else if (zscii_output == 13)
    result = Z_UCS_NEWLINE;
  else if ((zscii_output >= 32) && (zscii_output <= 126))
    result = (z_ucs)zscii_output; // Identical to ASCII in this range.
  else if ((zscii_output >= 155) && (zscii_output <= 251))
    result = zscii_extra_char_to_unicode(zscii_output);
  else
    i18n_translate_and_exit(
        libfizmo_module_name,
        i18n_libfizmo_INVALID_ZSCII_OUTPUT_CODE_P0D,
        -1,
        (long)zscii_output);

  TRACE_LOG("Result: %d.\n", (unsigned)result);

  return result;
}


void opcode_print_paddr(void)
{
  uint32_t unpacked_address = get_packed_string_address(op[0]);

  TRACE_LOG("Opcode: PRINT_PADDR.\n");

  TRACE_LOG("Printing string at %x.\n", (unsigned)unpacked_address);

  (void)output_zchar_to_streams(z_mem + unpacked_address);
}


void opcode_print_num(void)
{
  char number[7]; // '-' + 32768 + '\0', 7 chars are okay.
  TRACE_LOG("Opcode: PRINT_NUM.\n");

  /*@-bufferoverflowhigh@*/
  sprintf(number, "%d", (int16_t)op[0]);
  /*@+bufferoverflowhigh@*/

  (void)streams_latin1_output(number);
}


void opcode_print_char(void)
{
  z_ucs *output_buffer_index = z_ucs_output_buffer;

  TRACE_LOG("Opcode: PRINT_CHAR.\n");

  // FIXME: Workaround for Beyond Zork
  if (is_valid_zscii_output_char(op[0]) == false)
    return;

  *output_buffer_index = zscii_output_char_to_z_ucs(op[0]);
  output_buffer_index++;
  *output_buffer_index = 0;
  (void)streams_z_ucs_output(z_ucs_output_buffer);
}


void opcode_print(void)
{
  TRACE_LOG("Opcode: PRINT.\n");

  pc = output_zchar_to_streams(pc);
}


void opcode_new_line(void)
{
  TRACE_LOG("Opcode: NEW_LINE.\n");

  (void)streams_latin1_output("\n");
}


static bool process_interpreter_command()
{
  z_ucs *prefixed_command;
  char *ptr;
  int i;

  TRACE_LOG("Checking for interpreter-command '");
  TRACE_LOG_Z_UCS(interpreter_command_buffer);
  TRACE_LOG("'.\n");

  if (*interpreter_command_buffer != latin1_char_to_zucs_char(
        FIZMO_COMMAND_PREFIX))
    return false;

  prefixed_command = interpreter_command_buffer + 1;

  if (z_ucs_cmp_latin1(prefixed_command, "help") == 0)
  {
    (void)streams_latin1_output("\n");
    (void)i18n_translate(libfizmo_module_name,i18n_libfizmo_VALID_COMMANDS_ARE);
    (void)streams_latin1_output("\n");
    (void)streams_latin1_output(fizmo_command_prefix_string);
    (void)streams_latin1_output("help\n");
    (void)streams_latin1_output(fizmo_command_prefix_string);
    (void)streams_latin1_output("info\n");
    (void)streams_latin1_output(fizmo_command_prefix_string);
    (void)streams_latin1_output("predictable\n");
    (void)streams_latin1_output(fizmo_command_prefix_string);
    (void)streams_latin1_output("recstart\n");
    (void)streams_latin1_output(fizmo_command_prefix_string);
    (void)streams_latin1_output("recstop\n");
    (void)streams_latin1_output(fizmo_command_prefix_string);
    (void)streams_latin1_output("fileinput\n");
    (void)streams_latin1_output(fizmo_command_prefix_string);
    (void)streams_latin1_output("config\n");
    //(void)streams_latin1_output("\n");
    return true;
  }
  else if (z_ucs_cmp_latin1(prefixed_command, "predictable") == 0)
  {
    ptr = get_configuration_value("random-mode");
    if ( (ptr == NULL) || (strcmp(ptr, "predictable") != 0) )
    {
      set_configuration_value("random-mode", "predictable");
      (void)i18n_translate(
          libfizmo_module_name,
          i18n_libfizmo_RANDOM_GENERATOR_IS_NOW_IN_PREDICTABLE_MODE);
    }
    else
    {
      set_configuration_value("random-mode", "random");
      (void)i18n_translate(
          libfizmo_module_name,
          i18n_libfizmo_RANDOM_GENERATOR_IS_NOW_IN_RANDOM_MODE);
    }
    (void)streams_latin1_output("\n");

    return true;
  }
  else if (z_ucs_cmp_latin1(prefixed_command, "info") == 0)
  {
    active_interface->output_interface_info();

    (void)i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_LIBFIZMO_VERSION_P0S,
        FIZMO_VERSION);
    (void)streams_latin1_output("\n");

    if (active_sound_interface != NULL)
    {
      streams_latin1_output(active_sound_interface->get_interface_name());
      streams_latin1_output(" ");
      streams_latin1_output("version ");
      streams_latin1_output(active_sound_interface->get_interface_version());
      streams_latin1_output("\n");
    }

    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_STORY_HAS_Z_VERSION_NUMBER_P0D,
        (long int)active_z_story->version);
    streams_latin1_output("\n");

    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_STORY_RELEASE_NUMBER,
        (long int)active_z_story->release_code);
    streams_latin1_output("\n");

    i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_STORY_SERIAL_NUMBER,
        active_z_story->serial_code);
    streams_latin1_output("\n");

    (void)i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_CURRENT_Z_STACK_SIZE_P0D_ENTRIES,
        current_z_stack_size);
    (void)streams_latin1_output("\n");

    (void)i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_CURRENT_Z_STACK_ENTRIES_IN_USE_P0D,
        z_stack_index - z_stack);
    (void)streams_latin1_output("\n");

    if (bool_equal(skip_active_routines_stack_check_warning, true))
    {
      (void)i18n_translate(
          libfizmo_module_name,
          i18n_libfizmo_HACK_01__ROUTINE_STACK_UNDERFLOW_CHECK_DISABLED);
      (void)streams_latin1_output("\n");
    }

    (void)i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_P0D_BYTES_USED_FOR_UNDO,
        (long)get_allocated_undo_memory_size());
    (void)streams_latin1_output("\n");

    (void)i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_P0D_BYTES_USED_BY_TEXT_HISTORY,
        (long)get_allocated_text_history_size(outputhistory[0]));
    (void)streams_latin1_output("\n");

    ptr = get_configuration_value("random-mode");
    if (ptr != NULL)
    {
      if (strcmp(ptr, "predictable") == 0)
        (void)i18n_translate(
            libfizmo_module_name,
            i18n_libfizmo_RANDOM_GENERATOR_IS_NOW_IN_PREDICTABLE_MODE);
      else
        (void)i18n_translate(
            libfizmo_module_name,
            i18n_libfizmo_RANDOM_GENERATOR_IS_NOW_IN_RANDOM_MODE);
      (void)streams_latin1_output("\n");
    }

    /*
#ifndef DISABLE_OUTPUT_HISTORY
    (void)i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_P0D_BYTES_USED_BY_TEXT_HISTORY,
        outputhistory[0]->z_history_buffer_size + 1);
    (void)streams_latin1_output("\n");
    (void)i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_P0D_BYTES_USED_BY_TEXT_HISTORY_METADATA,
        outputhistory[0]->history_buffer_metadata_size
        * sizeof(struct history_metadata));
    (void)streams_latin1_output("\n");
    (void)i18n_translate(
        libfizmo_module_name,
        i18n_libfizmo_P0D_BYTES_USED_BY_PARAGRAPH_CACHE,
        outputhistory[0]->pos_cache_size * sizeof(z_ucs*));
    (void)streams_latin1_output("\n");
#endif
*/

#ifndef DISABLE_BLOCKBUFFER
    if (upper_window_buffer != NULL)
    {
      (void)i18n_translate(
          libfizmo_module_name,
          i18n_libfizmo_P0D_BYTES_USED_BY_BLOCK_BUFFER,
          sizeof(struct blockbuf_char) * upper_window_buffer->width
          * upper_window_buffer->height);
      (void)streams_latin1_output("\n");
    }
#endif

    //(void)streams_latin1_output("\n");

    return true;
  }
  else if (z_ucs_cmp_latin1(prefixed_command, "recstart") == 0)
  {
    ptr = get_configuration_value("disable-external-streams");
    if ( (ptr == NULL) || (strcasecmp(ptr, "true") != 0) )
    {
      stream_4_active = true;
      ask_for_stream4_filename_if_required(true);
    }
    else
    {
      (void)i18n_translate(
          libfizmo_module_name,
          i18n_libfizmo_THIS_FUNCTION_HAS_BEEN_DISABLED);
      (void)streams_latin1_output("\n");
    }
    return true;
  }
  else if (z_ucs_cmp_latin1(prefixed_command, "recstop") == 0)
  {
    ptr = get_configuration_value("disable-external-streams");
    if ( (ptr == NULL) || (strcasecmp(ptr, "true") != 0) )
      stream_4_active = false;
    else
    {
      (void)i18n_translate(
          libfizmo_module_name,
          i18n_libfizmo_THIS_FUNCTION_HAS_BEEN_DISABLED);
      (void)streams_latin1_output("\n");
    }
    return true;
  }
  else if (z_ucs_cmp_latin1(prefixed_command, "fileinput") == 0)
  {
    open_input_stream_1();
    return true;
  }
  else if (z_ucs_cmp_latin1(prefixed_command, "config") == 0)
  {
    i = 0;
    while (configuration_options[i].name != NULL)
    {
      streams_latin1_output(configuration_options[i].name);
      streams_latin1_output(" = ");
      streams_latin1_output(
          get_configuration_value(configuration_options[i].name));
      streams_latin1_output("\n");
      i++;
    }
    return true;
  }
  else
  {
    return false;
  }
}


int save_and_quit_if_required(bool force_save)
{
  uint8_t *filename;
  char *save_and_quit_file;
  uint8_t *pc_buf;
  int length;

  save_and_quit_file
    = get_configuration_value("save-and-quit-file-before-read");

  TRACE_LOG("save_and_quit_file: %s.\n", save_and_quit_file);

  if (
      (save_and_quit_file != NULL)
      &&
      (strcmp(save_and_quit_file, "true") == 0)
      &&
      (
       (zpu_step_number != 1)
       ||
       (bool_equal(force_save, true))
      )
     )
  {
    pc_buf = pc;
    pc = current_instruction_location;

    TRACE_LOG("current_instruction_location: %lx\n",
      (unsigned long int)(current_instruction_location - z_mem));

    length = strlen(save_and_quit_file) + 1;

    filename = (uint8_t*)fizmo_malloc(length);

    length--;
    memcpy(filename + 1, save_and_quit_file, length);
    *filename = length;

    save_game(
      0,
      (uint16_t)(active_z_story->dynamic_memory_end - z_mem + 1),
      save_and_quit_file,
      true,
      false,
      NULL);

    pc = pc_buf;
    terminate_interpreter = INTERPRETER_QUIT_SAVE_BEFORE_READ;
    return 1;
  }
  else
    return 0;
}


int read_command_from_file(zscii *input_buffer, int input_buffer_size,
    int *input_delay_tenth_seconds)
{
  int c;
  int input_length = 0;
  long filepos;
  int milliseconds;
  int res;

  if (input_stream_1 == NULL)
  {
    if (input_stream_1_was_already_active == false)
      ask_for_input_stream_filename();
    TRACE_LOG("Trying to open \"%s\n", input_stream_1_filename);
    if ((input_stream_1 = fopen(input_stream_1_filename, "r")) == NULL)
    {
      input_stream_1_active = false;
      input_stream_1_filename_size = 0;
      free(input_stream_1_filename);
      return -1;
    }
  }

  filepos = ftell(input_stream_1);

  // Parse "(Waited for <n> ms)".
  res = fscanf(input_stream_1, "(Waited for %d ms)\n", &milliseconds);
  if (res == 1)
  {
    if (input_delay_tenth_seconds != NULL)
      *input_delay_tenth_seconds = milliseconds / 100;
  }
  else if (res == 0)
  {
    if (input_delay_tenth_seconds != NULL)
      *input_delay_tenth_seconds = 0;
    fseek(input_stream_1, filepos, SEEK_SET);
  }

  // FIMXE: Input conversion from latin1(?) to zscii-input.
  while (
      ((c = fgetc(input_stream_1)) != EOF)
      &&
      (c != '\n')
      &&
      (input_length != input_buffer_size)
      )
  {
    if (c != '\r')
    {
      *(input_buffer++) = unicode_char_to_zscii_input_char(c & 0x7f);
      input_length++;
    }
  }

  if (c != EOF)
  {
    // Seek next '\n'.
    while (c != '\n')
      if ((c = fgetc(input_stream_1)) == EOF)
        break;
  }

  TRACE_LOG("Read %d input chars.\n", input_length);

  // At EOF? If so, close stream and return last length.
  if (c == EOF)
  {
    fclose(input_stream_1);
    input_stream_1_active = false;
    input_stream_1 = NULL;
    return input_length;
  }

  // Check if there's something behind the '\n'.
  c = fgetc(input_stream_1);
  if (c == EOF)
  {
    input_stream_1_active = false;
    fclose(input_stream_1);
    input_stream_1 = NULL;
  }
  else
    ungetc(c, input_stream_1);

  return input_length;
}


void opcode_read(void)
{
  uint16_t maximum_length;
  int16_t input_length = -1; // init to inhibit compiler warning
  zscii *z_text_buffer = z_mem + op[0];
  uint16_t parsebuffer_offset = op[1];
  uint16_t tenth_seconds = op[2];
  uint16_t timed_routine_offset = op[3];
  bool interpreter_command_found;
  int i;
  z_ucs *current_line;
  bool stream_1_active_buf;
  int offset;
  size_t bytes_required;
  int tenth_seconds_elapsed = -1;
  char *time_output;
  int tenth_seconds_to_delay = 0;
  char *value;
  int timed_routine_retval;
#ifndef DISABLE_COMMAND_HISTORY
  uint8_t buf;
#endif /* DISABLE_COMMAND_HISTORY */

  TRACE_LOG("Opcode: READ.\n");

  if (save_and_quit_if_required(false) != 0)
    return;

  TRACE_LOG("Reading input (%x, %x).\n", op[0], parsebuffer_offset);

  // FIXME: Implement permanent buffer for current_line.
#ifndef DISABLE_OUTPUT_HISTORY
  current_line = get_current_line(outputhistory[active_window_number]);
  if (current_line != NULL)
  {
    TRACE_LOG("Current line (%p): '", current_line);
    TRACE_LOG_Z_UCS(current_line);
    TRACE_LOG("'.\n");
  }
#else
  current_line = NULL;
#endif /* DISABLE_OUTPUT_HISTORY */

  if (ver >= 5)
    read_z_result_variable();

  do
  {
    interpreter_command_found = false;

    if (ver <= 3)
      display_status_line();

    value = get_configuration_value("disable-external-streams");

    if (ver <= 4)
    {
      maximum_length = (*z_text_buffer) + 1;
      // FIXME: Verify minimum text buffer length.
      TRACE_LOG("Maximum length: %d.\n", maximum_length);

      input_length = -1;

      if (
          (input_stream_1_active == true)
          &&
          ( (value == NULL) || (strcasecmp(value, "true") != 0) )
         )
        input_length
          = read_command_from_file(z_text_buffer+1, maximum_length, NULL);

      if (input_length == -1)
        input_length
          = active_interface->read_line(
              (z_text_buffer+1), maximum_length, 0, 0, 0, NULL, false, false);

      TRACE_LOG("Resulting input length: %d.\n", input_length);

      // Since version 4 games don't have a timeout (tenth_seconds and
      // verification_routine are set to a fixed 0 in the call above)
      // the result stored in input_length is always >= 0.

      // Reduce to lower case
      // FIXME: Respect other (non-ASCII) languages?
      /*
      for (i=1; i<input_length+1; i++)
        if ((z_text_buffer[i] >= 0x41) && (z_text_buffer[i] <= 0x5a))
          z_text_buffer[i] += 0x20;
      */

      z_text_buffer[input_length + 1] = 0;

#ifndef DISABLE_COMMAND_HISTORY
      if (input_length > 0)
      {
        // Now we'll store the last command in the command history.
        store_command_in_history(z_text_buffer + 1);
      }
#endif /* DISABLE_COMMAND_HISTORY */

      for (i=1; i<(int)(input_length+2); i++)
        TRACE_LOG("zscii-input[%d] = %d.\n", i, z_text_buffer[i]);
    }
    else if (ver >= 5)
    {
      maximum_length = *z_text_buffer;
      TRACE_LOG("Maximum length: %d.\n", maximum_length);
      // FIXME: Verify minimum text buffer length.

#ifdef ENABLE_TRACING
      TRACE_LOG("Characters left over from previous input: %d.\n",
          z_text_buffer[1]);
#endif // ENABLE_TRACING

      // Preloaded input is only availiable in version 5.
      if (z_text_buffer[1] >  0)
      {
        if (active_interface->is_preloaded_input_available() == false)
          i18n_translate_and_exit(
              libfizmo_module_name,
              i18n_libfizmo_PRELOADED_INPUT_NOT_AVAILIABLE_IN_INTERFACE_P0S,
              -1,
              active_interface->get_interface_name());

        TRACE_LOG("Removing %d chars from history.\n", z_text_buffer[1]);
#ifndef DISABLE_OUTPUT_HISTORY
        remove_chars_from_history(outputhistory[0], z_text_buffer[1]);
#endif // DISABLE_OUTPUT_HISTORY
      }

      if ( (number_of_operands >= 4)
          && (tenth_seconds != 0) && (timed_routine_offset != 0) )
      {
        TRACE_LOG("Timed input requested: %d/10s, routine at %x.\n",
            tenth_seconds, timed_routine_offset);
        // In this case a time-intervall and routine to call for
        // timed input are given.
        input_length = -1;
        if (
            (input_stream_1_active == true)
            &&
            ( (value == NULL) || (strcasecmp(value, "true") != 0) )
           )
        {
          if ((input_length
                = read_command_from_file(z_text_buffer+2, maximum_length,
                  &tenth_seconds_to_delay)) != -1)
          {
            TRACE_LOG("1/10s to wait: %d\n", tenth_seconds_to_delay);
            stream_output_has_occured = false;
            while ((tenth_seconds_to_delay -= tenth_seconds) >= 0)
            {
              TRACE_LOG(
                  "Invoking verification routine, 1/10s counter: %d(%d).\n",
                  tenth_seconds_to_delay , tenth_seconds);
              if ((stream_output_has_occured == true) && (current_line != NULL))
              {
                TRACE_LOG("Restoring original prompt: '");
                TRACE_LOG_Z_UCS(current_line);
                TRACE_LOG("'.\n");
                (void)streams_z_ucs_output(current_line);
                stream_output_has_occured = false;
              }
              if ((timed_routine_retval=interpret_from_call(
                      get_packed_routinecall_address(timed_routine_offset)))!=0)
              {
                TRACE_LOG("verification routine returned != 0.\n");
                input_length = -1;
                break;
              }
            }
          }
        }

        if (input_length == -1)
        {
          if (active_interface->is_timed_keyboard_input_available() == false)
            i18n_translate_and_exit(
                libfizmo_module_name,
                i18n_libfizmo_TIMED_INPUT_NOT_IMPLEMENTED_IN_INTERFACE_P0S,
                -1,
                active_interface->get_interface_name());
          else
            input_length
              = active_interface->read_line(
                  z_text_buffer + 2,
                  maximum_length,
                  tenth_seconds,
                  get_packed_routinecall_address(timed_routine_offset),
                  z_text_buffer[1],
                  &tenth_seconds_elapsed,
                  false,
                  false);
        }

        if (terminate_interpreter != INTERPRETER_QUIT_NONE)
          return;

        // input_length always defined, since "i18n_translate_and_exit" does
        // not return.
        /*@-usedef@*/
        if (input_length >= 0)
        /*@+usedef@*/
        {
          set_variable(z_res_var, (zscii)10);
          z_text_buffer[1] = (zscii)input_length;
        }
        else
        {
          set_variable(z_res_var, 0);
          z_text_buffer[1] = 0;
        }
      }
      else
      {
        input_length = -1;
        if (
            (input_stream_1_active == true)
            &&
            ( (value == NULL) || (strcasecmp(value, "true") != 0) )
           )
          input_length
            = read_command_from_file(z_text_buffer+2, maximum_length, NULL);

        if (input_length == -1)
          input_length
            = active_interface->read_line(
                z_text_buffer + 2,
                maximum_length,
                0,
                0,
                z_text_buffer[1],
                NULL,
                false,
                false);

        TRACE_LOG("Resulting input length: %d.\n", input_length);
        z_text_buffer[1] = (zscii)input_length;
        set_variable(z_res_var, Z_UCS_NEWLINE);
      }

#ifndef DISABLE_COMMAND_HISTORY
      if (input_length >= 0)
      {
        buf = z_text_buffer[input_length + 2];
        z_text_buffer[input_length + 2] = (zscii)0;
        store_command_in_history(z_text_buffer + 2);
        z_text_buffer[input_length + 2] = buf;
      }
#endif /* DISABLE_COMMAND_HISTORY */
    }

    bytes_required = (input_length + 1) * sizeof(z_ucs);
    if (bytes_required > (size_t)interpreter_command_buffer_size)
    {
      interpreter_command_buffer
        = (z_ucs*)fizmo_realloc(interpreter_command_buffer, bytes_required);

      interpreter_command_buffer_size = bytes_required;
    }

    offset = (ver >= 5 ? 2 : 1);

    for (i=0; i<input_length; i++)
    {
      TRACE_LOG("%c\n", zscii_input_char_to_z_ucs(z_text_buffer[i + offset]));
      interpreter_command_buffer[i]
        = zscii_input_char_to_z_ucs(z_text_buffer[i + offset]);
    }

    interpreter_command_buffer[input_length] = 0;

    stream_1_active_buf = stream_1_active;
    stream_1_active = active_interface->input_must_be_repeated_by_story();

    if (bool_equal(stream_4_active, true))
    {
      if (tenth_seconds_elapsed > 0)
      {
        time_output = fizmo_malloc(40);
        snprintf(time_output, 40, "(Waited for %d ms)\n",
            tenth_seconds_elapsed * 100);
        stream_4_latin1_output(time_output);
        free(time_output);
      }

      if (input_length == -1)
        stream_4_latin1_output("(Interrupted input)");
    }

    if (input_length >= 0)
      (void)streams_z_ucs_output_user_input(interpreter_command_buffer);
    (void)streams_z_ucs_output_user_input(z_ucs_newline_string);

    stream_1_active = stream_1_active_buf;

    TRACE_LOG("input length:%d\n", input_length);
    if (input_length >= 0)
    {
      /*
      for (i=offset; i<(int)(input_length+offset); i++)
        // Test for isupper to avoid converting non-regular chars (tilde-a).
        if (isupper(z_text_buffer[i]) != 0)
          z_text_buffer[i] = tolower(z_text_buffer[i]);
          */

      first_word_found = 0;

      // Next, lexical analysis is performed on the text (except that in
      // Versions 5 and later, if parse-buffer is zero then this is omitted).
      if ((ver < 5) || (parsebuffer_offset != 0))
      {
        TRACE_LOG("Peforming lexical analysis.\n");

        tokenise(
            z_text_buffer,
            (uint8_t)(ver >= 5 ? 2 : 1),
            /*@-usedef@*/ (uint16_t)input_length /*@+usedef@*/,
            z_mem + parsebuffer_offset,
            active_z_story->dictionary_table,
            false);
      }

      if (first_word_found == 0)
      {
        interpreter_command_found = process_interpreter_command();

        if (bool_equal(interpreter_command_found, true))
        {
          if (current_line != NULL)
          {
            TRACE_LOG("Restoring original prompt: '");
            TRACE_LOG_Z_UCS(current_line);
            TRACE_LOG("'.\n");
            (void)streams_z_ucs_output(current_line);
          }
          else
            (void)streams_latin1_output(">");

          z_text_buffer[1] = 0;

          if (save_and_quit_if_required(true) != 0)
            return;
        }
      }

      // In case no lexical analysis is required and we've found no interpreter
      // command, we can quite the loop.
      if ( (ver >=5) && (parsebuffer_offset == 0)
          && (interpreter_command_found == false))
        break;

      if (active_sound_interface != NULL)
        active_sound_interface->keyboard_input_has_occurred();
    }
    else
      break;
  }
  // If we just had a command that was processed internally by the
  // interpreter, not the story, we'll continue with the input.
  while (interpreter_command_found != false);

  if (current_line != NULL)
  {
    TRACE_LOG("Free current line (%p): '", current_line);
    TRACE_LOG_Z_UCS(current_line);
    TRACE_LOG("'.\n");
    free(current_line);
  }

  number_of_commands++;
}


void opcode_print_obj(void)
{
  uint8_t *object_property_table = get_objects_property_table(op[0]);

  TRACE_LOG("Opcode: PRINT_OBJ.\n");

  (void)output_zchar_to_streams(object_property_table + 1);
}


void opcode_print_ret(void)
{
  TRACE_LOG("Opcode: PRINT_RET.\n");

  (void)output_zchar_to_streams(pc);
  pc = *zchar_to_z_ucs_src_index;
  (void)streams_latin1_output("\n");

  return_from_routine(1);
}


void opcode_print_addr(void)
{
  TRACE_LOG("Opcode: PRINT_ADDR.\n");

  TRACE_LOG("Printing string at %x.\n", op[0]);
  (void)output_zchar_to_streams(z_mem + op[0]);
}


void opcode_show_status(void)
{
  TRACE_LOG("Opcode: SHOW_STATUS.\n");
  display_status_line();
}


void opcode_read_char(void)
{
  //static z_ucs buf[] = { 0, 0 };
  static zscii command_input;
  char *time_output;
  int tenth_seconds_elapsed = -1;
  int input_char;
  int input_delay_tenth_seconds = 0;
  int input_length;
  char *value = get_configuration_value("disable-external-streams");
  int timed_routine_retval;

  TRACE_LOG("Opcode: READ_CHAR.\n");

  read_z_result_variable();

  // FIXME: Check for first parameter which must be 1.

  // FIXME: Convert to ZSCII.

  if (ver <= 3)
    display_status_line();


  if ((number_of_operands >= 3) && (ver >= 4))
  {
    TRACE_LOG("Starting reading character via timed input.\n");

    input_length = -1;

    if (
        (input_stream_1_active == true)
        &&
        ( (value == NULL) || (strcasecmp(value, "true") != 0) )
       )
      input_length
        = read_command_from_file(&command_input, 1, &input_delay_tenth_seconds);

    input_char 
      = (input_length == -1)
      ? active_interface->read_char(
          op[1],
          get_packed_routinecall_address(op[2]),
          &tenth_seconds_elapsed)
      : command_input;

    if (terminate_interpreter != INTERPRETER_QUIT_NONE)
      return;

    if (bool_equal(stream_4_active, true))
    {
      if (tenth_seconds_elapsed > 0)
      {
        time_output = fizmo_malloc(40);
        snprintf(time_output, 40, "(Waited for %d ms)\n",
            tenth_seconds_elapsed * 100);
        stream_4_latin1_output(time_output);
        free(time_output);
      }

      if (input_char == -1)
        stream_4_latin1_output("(Interrupted input)");
    }

    if (input_char == -1)
      input_char = 0;
    /*
    // No output of input in case of read_char.
    else
    {
      *buf = input_char;
      (void)streams_z_ucs_output_user_input(buf);
    }
    (void)streams_z_ucs_output_user_input(z_ucs_newline_string);
    */

    set_variable(z_res_var, input_char);

    TRACE_LOG("Reading character via timed input done.\n");
  }
  else
  {
    TRACE_LOG("Starting reading single character.\n");

    input_length = -1;

    if (
        (input_stream_1_active == true)
        &&
        ( (value == NULL) || (strcasecmp(value, "true") != 0) )
       )
      input_length
        = read_command_from_file(&command_input, 1, &input_delay_tenth_seconds);

    input_char 
      = (input_length == -1)
      ? active_interface->read_char(0, 0, NULL)
      : command_input;

    if (input_delay_tenth_seconds > 0)
    {
      TRACE_LOG("1/10s to wait: %d\n", input_delay_tenth_seconds);
      while ((input_delay_tenth_seconds -= op[1]) >= 0)
      {
        TRACE_LOG(
            "Invoking verification routine, 1/10s counter: %d(%d).\n",
            input_delay_tenth_seconds , op[1]);
        if ((timed_routine_retval = interpret_from_call(
                get_packed_routinecall_address(op[2]))) != 0)
        {
          TRACE_LOG("verification routine returned != 0.\n");
          input_length = -1;
          break;
        }
      }
    }

    /*
    *buf = input_char;
    // No output of input in case of read_char.
    (void)streams_z_ucs_output_user_input(buf);
    (void)streams_z_ucs_output_user_input(z_ucs_newline_string);
    */
    set_variable(z_res_var, input_char);
    TRACE_LOG("Reading single character done.\n");
  }

  if (tenth_seconds_elapsed != -1)
  {
    TRACE_LOG("Time elapsed: (%d/10)s.\n", tenth_seconds_elapsed);
  }

  if (active_sound_interface != NULL)
    active_sound_interface->keyboard_input_has_occurred();
}


void opcode_print_table(void)
{
  uint8_t *src= z_mem + (uint16_t)op[0];
  uint16_t width = (uint16_t)op[1];
  uint16_t height = 1;
  uint16_t skip = 0;
  int x,y;
  int buffer_index;
  int cursor_y = active_interface->get_cursor_row();
  int cursor_x = active_interface->get_cursor_column();

  TRACE_LOG("Opcode: PRINT_TABLE.\n");

  if (number_of_operands >= 3)
  {
    height = (uint16_t)op[2];

    if (number_of_operands >= 4)
      skip = (uint16_t)op[3];
  }

  for (y=0; y<height; y++)
  {
    buffer_index = 0;
    process_set_cursor(cursor_y, cursor_x, active_window_number);

    for (x=0; x<width; x++)
    {
      if (buffer_index + 1 == Z_UCS_OUTPUT_BUFFER_SIZE)
      {
        z_ucs_output_buffer[buffer_index] = 0;
        streams_z_ucs_output(z_ucs_output_buffer);
        buffer_index = 0;
      }

      z_ucs_output_buffer[buffer_index++] = zscii_output_char_to_z_ucs(*src);
      src++;
    }

    if (buffer_index != 0)
    {
      z_ucs_output_buffer[buffer_index] = 0;
      streams_z_ucs_output(z_ucs_output_buffer);
      buffer_index = 0;
    }

    TRACE_LOG("Finished table output line %d of %d.\n", y+1, height);

    cursor_y++;
    src += skip;
  }
}


void opcode_encode_text(void)
{
  uint8_t *src = z_mem + (uint16_t)op[0] + (uint16_t)op[2];
  uint16_t length = (uint16_t)op[1];
  uint8_t *dest = z_mem + (uint16_t)op[3];

  TRACE_LOG("Opcode: ENCODE_TEXT.\n");

  zchar_storage_start(dest, (length + 2) / 3);

  while ( (*src != 0) && (length > 0) )
  {
    store_ZSCII_as_zchar(*src);
    src++;
    length--;
  }

  zchar_storage_finish();
}


void opcode_print_unicode(void)
{
  z_ucs_output_buffer[0] = (uint16_t)op[0];
  z_ucs_output_buffer[1] = 0;
  (void)streams_z_ucs_output(z_ucs_output_buffer);
}


void opcode_check_unicode(void)
{
  uint16_t result = 0;

  read_z_result_variable();

  // FIXME: Check for output charset.

  if (unicode_char_to_zscii_input_char((uint16_t)op[0]) != 0xff)
    result = 3;
  else
    result = 1;

  set_variable(z_res_var, result);
}

#endif /* text_c_INCLUDED */
