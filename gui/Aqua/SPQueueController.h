/* vim: ft=objc
 *
 * Mac DC++. An Aqua user interface for DC++.
 * Copyright (C) 2004 Jonathan Jansson, jonathan.dator@home.se
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#import <Cocoa/Cocoa.h>

@class SPOutlineView;

@interface SPQueueItem : NSObject
{
    NSString *target;
    NSString *filename;
    NSAttributedString *attributedFilename;
    NSString *path;
    NSString *tth;
    NSAttributedString *attributedTTH;
    NSNumber *size;
    NSNumber *priority;
    NSAttributedString *priorityString;
    NSAttributedString *status;

    NSMutableDictionary *sources;
    NSMutableArray *children;

    BOOL isFilelist;
    BOOL isDirectory;
    BOOL isFinished;
	
	// when a download is requested to be removed by the user, this is set to make it
	// easy to identify user-removed downloads from finished downloads later when the
	// action is complete (it is aborted) and it needs to be removed from the table.
	BOOL isWaitingToBeRemoved;
}
- (id)initWithTarget:(NSString *)aTarget;
- (NSString *)filename;
- (NSAttributedString *)attributedFilename;
- (NSString *)path;
- (NSString *)target;
- (NSString *)tth;
- (void)setTTH:(NSString *)aTTH;
- (NSAttributedString *)attributedTTH;
- (NSNumber *)size;
- (NSNumber *)exactSize;
- (void)setSize:(NSNumber *)aSize;
- (NSNumber *)priority;
- (NSAttributedString *)priorityString;
- (void)setPriority:(NSNumber *)aPriority;
- (NSAttributedString *)status;
- (void)setStatus:(NSString *)aStatusString;
- (void)addSource:(NSString *)sourcePath nick:(NSString *)nick;
- (void)removeSourceForNick:(NSString *)nick;
- (NSArray *)nicks;
- (NSMutableArray *)children;
- (NSString *)users;
- (int)filetype;
- (BOOL)isDirectory;
- (void)setIsDirectory;
- (BOOL)isFilelist;
- (void)setIsFilelist:(BOOL)aFlag;
- (BOOL)isFinished;
- (void)setFinished;
- (BOOL)isWaitingToBeRemoved;
- (void)setIsWaitingToBeRemoved:(BOOL)waitingToBeRemoved;

@end

@interface SPQueueController : NSWindowController
{
    IBOutlet NSView *queueView;
    IBOutlet SPOutlineView *tableView;

    IBOutlet NSMenu *queueMenu;

    IBOutlet NSMenu *menuRemoveSource;
    IBOutlet NSMenuItem *menuItemRemoveSource;

    IBOutlet NSMenu *menuRemoveUserFromQueue;
    IBOutlet NSMenuItem *menuItemRemoveUserFromQueue;

    IBOutlet NSMenu *menuBrowseUsersFiles;
    IBOutlet NSMenuItem *menuItemBrowseUsersFiles;

    IBOutlet NSMenuItem *menuPriorityPaused;
    IBOutlet NSMenuItem *menuPriorityLowest;
    IBOutlet NSMenuItem *menuPriorityLow;
    IBOutlet NSMenuItem *menuPriorityNormal;
    IBOutlet NSMenuItem *menuPriorityHigh;
    IBOutlet NSMenuItem *menuPriorityHighest;

    IBOutlet NSMenuItem *menuItemSearchByTTH;
    IBOutlet NSMenuItem *menuItemSearchForAlternates;
    IBOutlet NSMenuItem *menuItemRemoveQueue;

    IBOutlet NSMenu *columnsMenu;
    IBOutlet NSTableColumn *tcSize;
    IBOutlet NSTableColumn *tcUsers;
    IBOutlet NSTableColumn *tcStatus;
    IBOutlet NSTableColumn *tcPriority;
    IBOutlet NSTableColumn *tcPath;
    IBOutlet NSTableColumn *tcTTH;
    IBOutlet NSTableColumn *tcExactSize;

    NSMutableArray *rootItems;
}

// called when the user removes a download 
- (IBAction)removeFromQueue:(id)sender;

- (IBAction)removeSource:(id)sender;
- (IBAction)removeUserFromQueue:(id)sender;

- (IBAction)clearAllFinishedDownloads:(id)sender;
- (void)clearAllFinishedDownloadsRecursivelyInArray:(NSMutableArray *)anArray;

- (IBAction)setPriority:(id)sender;
- (IBAction)searchByTTH:(id)sender;
- (IBAction)searchForAlternates:(id)sender;
- (IBAction)browseUsersFiles:(id)sender;
- (IBAction)toggleColumn:(id)sender;
- (IBAction)openSelected:(id)sender;
- (IBAction)revealInFinder:(id)sender;

@end
