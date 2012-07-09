
/* ios-autosave.m
 *
 * This file is part of fizmo.
 *
 * Copyright (c) 2012 Andrew Plotkin.
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

#import "GlkStream.h"
#import "GlkLibrary.h"
#include "fizmo.h"
#include "savegame.h"
#include "zpu.h"
#include "filesys_interface.h"
#include "glk_interface.h"
#include "glk_screen_if.h"
#include "glk_filesys_if.h"
#include "filesys.h"

static NSString *normal_start_save = nil;

static NSString *documents_dir() {
	/* We use an old-fashioned way of locating the Documents directory. (The NSManager method for this is iOS 4.0 and later.) */
	
	NSArray *dirlist = NSSearchPathForDirectoriesInDomains(NSDocumentDirectory, NSUserDomainMask, YES);
	if (!dirlist || [dirlist count] == 0) {
		NSLog(@"unable to locate Documents directory.");
		return nil;
	}
	
	return [dirlist objectAtIndex:0];
}

/* Do an auto-save of the game state, to an iOS-appropriate location. This also saves the Glk library state.
 
	The game goes into $DOCS/autosave.glksave; the library state into $DOCS/autosave.plist. However, we do this as atomically as possible -- we write to temp files and then rename.
 
	Returns 0 to indicate that the interpreter should not exit after saving. (If Fizmo is invoked from a CGI script, it can exit after every command cycle. But we're not doing that.)
 
	This is called in the VM thread, just before setting up line input and calling glk_select(). (So no window will actually be requesting line input at this time.)
 */
int iosglk_do_autosave() {
	GlkLibrary *library = [GlkLibrary singleton];
	uint8_t *orig_pc;

	NSString *dirname = documents_dir();
	if (!dirname)
		return 0;
	NSString *tmpgamepath = [dirname stringByAppendingPathComponent:@"autosave-tmp.glksave"];
	char *cpathname = (char *)[tmpgamepath cStringUsingEncoding:NSUTF8StringEncoding]; 
	// cpathname will be freed when the pathname is freed; openfile() will strdup it before that happens.
	z_file *save_file = fsi->openfile(cpathname, FILETYPE_DATA, FILEACCESS_WRITE);
	if (!save_file) {
		NSLog(@"unable to create z_file!");
		return 0;
	}
	
	orig_pc = pc;
    pc = current_instruction_location;

	int res = save_game_to_stream(0, (active_z_story->dynamic_memory_end - z_mem + 1), save_file, false);
	/* save_file is now closed */
	
	pc = orig_pc;
	
	if (!res) {
		NSLog(@"save_game_to_stream failed!");
		return 0;
	}
	
	NSString *tmplibpath = [dirname stringByAppendingPathComponent:@"autosave-tmp.plist"];
	res = [NSKeyedArchiver archiveRootObject:library toFile:tmplibpath];

	if (!res) {
		NSLog(@"library serialize failed!");
		return 0;
	}

	NSString *finalgamepath = [dirname stringByAppendingPathComponent:@"autosave.glksave"];
	NSString *finallibpath = [dirname stringByAppendingPathComponent:@"autosave.plist"];

	/* This is not really atomic, but we're already past the serious failure modes. */
	[library.filemanager removeItemAtPath:finallibpath error:nil];
	[library.filemanager removeItemAtPath:finalgamepath error:nil];

	res = [library.filemanager moveItemAtPath:tmpgamepath toPath:finalgamepath error:nil];
	if (!res) {
		NSLog(@"could not move game autosave to final position!");
		return 0;
	}
	res = [library.filemanager moveItemAtPath:tmplibpath toPath:finallibpath error:nil];
	if (!res) {
		/* We don't abort out in this case; we leave the game autosave in place by itself, which is not ideal but better than data loss. */
		NSLog(@"could not move library autosave to final position (continuing)");
	}

	return 0;
}

/* The argument is actually an NSString. We retain it for the next iosglk_find_autosave() call.
 */
void iosglk_queue_autosave(void *pathnameval) {
	NSString *pathname = pathnameval;
	normal_start_save = [pathname retain];
}

z_file *iosglk_find_autosave() {
	if (normal_start_save) {
		strid_t savefile = [[[GlkStreamFile alloc] initWithMode:filemode_Read rock:1 unicode:NO textmode:NO dirname:@"." pathname:normal_start_save] autorelease];
		[normal_start_save release];
		normal_start_save = nil;
		
		if (savefile) {
			z_file *zsavefile = zfile_from_glk_strid(savefile, "Restore", FILETYPE_SAVEGAME, FILEACCESS_READ);
			if (zsavefile)
				return zsavefile;
		}
	}
	
	NSString *dirname = documents_dir();	
	if (!dirname)
		return nil;
	NSString *finalgamepath = [dirname stringByAppendingPathComponent:@"autosave.glksave"];

	char *cpathname = (char *)[finalgamepath cStringUsingEncoding:NSUTF8StringEncoding]; 
	// cpathname will be freed when the pathname is freed; openfile() will strdup it before that happens.
	z_file *save_file = fsi->openfile(cpathname, FILETYPE_DATA, FILEACCESS_READ);
	return save_file;
}

/* Delete an autosaved game, if one exists.
 */
void iosglk_clear_autosave() {
	GlkLibrary *library = [GlkLibrary singleton];
	
	NSString *dirname = documents_dir();
	if (!dirname)
		return;
	
	NSString *finalgamepath = [dirname stringByAppendingPathComponent:@"autosave.glksave"];
	NSString *finallibpath = [dirname stringByAppendingPathComponent:@"autosave.plist"];
	
	[library.filemanager removeItemAtPath:finallibpath error:nil];
	[library.filemanager removeItemAtPath:finalgamepath error:nil];
}

/* Restore an autosaved game, if one exists. The file argument is closed in the process.
 
	Returns 1 if a game was restored successfully, 0 if not.
 
	This is called in the VM thread, from inside fizmo_start(). glkint_open_interface() has already happened, so we're going to have to replace the initial library state with the autosaved state.
 */
int iosglk_restore_autosave(z_file *save_file) {
	//NSLog(@"restore_autosave of file (%s)", (save_file->implementation==0) ? "glk" : "stdio");
	GlkLibrary *library = [GlkLibrary singleton];
	
	/* A normal file must be restored with evaluate_result. An autosave file must not be. Fortunately, we've arranged things so that normal files are opened with zfile_from_glk_strid(), and autosave files with fsi->openfile(). */
	bool is_normal_file = (save_file->implementation==FILE_IMPLEMENTATION_GLK);
	
	int res = restore_game_from_stream(0,
		(uint16_t)(active_z_story->dynamic_memory_end - z_mem + 1),
		save_file, is_normal_file);
	/* save_file is now closed */
	
	if (!res) {
		NSLog(@"unable to restore autosave file.");
		return 0;
	}

	GlkLibrary *newlib = nil;
	NSString *dirname = documents_dir();	
	if (dirname) {
		NSString *finallibpath = [dirname stringByAppendingPathComponent:@"autosave.plist"];
		
		@try {
			newlib = [NSKeyedUnarchiver unarchiveObjectWithFile:finallibpath];
		}
		@catch (NSException *ex) {
			// leave newlib as nil
			NSLog(@"Unable to restore autosave library: %@", ex);
		}
	}
	
	if (newlib) {
		[library updateFromLibrary:newlib];
		glkint_recover_library_state();
	}
	
	return 1;
}

