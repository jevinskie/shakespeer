/* vim: ft=objc
 *
 * Copyright 2005 Martin Hedenfalk <martin@bzero.se>
 *
 * This file is part of ShakesPeer.
 *
 * ShakesPeer is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * ShakesPeer is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with ShakesPeer; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#import "SPQueueController.h"
#import "SPApplicationController.h"
#import "NSStringExtensions.h"
#import "SPMainWindowController.h"
#import "SPTransformers.h"
#import "SPLog.h"
#import "SPOutlineView.h"
#import "SPNotificationNames.h"
#import "SPUserDefaultKeys.h"

#include "util.h"

@implementation SPQueueController

- (id)init
{
    if ((self = [super init])) {
        [NSBundle loadNibNamed:@"Downloads" owner:self];

        rootItems = [[NSMutableArray alloc] init];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(addTargetNotification:)
                                                     name:SPNotificationQueueAdd
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(addDirectoryNotification:)
                                                     name:SPNotificationQueueAddDirectory
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(addFilelistNotification:)
                                                     name:SPNotificationQueueAddFilelist
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(removeTargetNotification:)
                                                     name:SPNotificationQueueRemove
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(removeFilelistNotification:)
                                                     name:SPNotificationQueueRemoveFilelist
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(removeTargetNotification:)
                                                     name:SPNotificationQueueRemoveDirectory
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(addSourceNotification:)
                                                     name:SPNotificationSourceAdd
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(removeSourceNotification:)
                                                     name:SPNotificationSourceRemove
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(setPriorityNotification:)
                                                     name:SPNotificationSetPriority
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(downloadStartingNotification:)
                                                     name:SPNotificationDownloadStarting
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(transferStatsNotification:)
                                                     name:SPNotificationTransferStats
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(transferAbortedNotification:)
                                                     name:SPNotificationTransferAborted
                                                   object:nil];

        [[tableView outlineTableColumn] setDataCell:[[[NSBrowserCell alloc] init] autorelease]];
        [[tableView outlineTableColumn] setMinWidth:70.0];

        [tableView setTarget:self];
        [tableView setDoubleAction:@selector(onDoubleClick:)];
    }

    return self;
}

- (void)awakeFromNib
{
    [tcSize retain];
    [tcUsers retain];
    [tcStatus retain];
    [tcPriority retain];
    [tcPath retain];
    [tcTTH retain];
    [tcExactSize retain];
    
    NSArray *tcs = [tableView tableColumns];
    NSEnumerator *e = [tcs objectEnumerator];
    NSTableColumn *tc;
    while ((tc = [e nextObject]) != nil) {
        [[tc dataCell] setWraps:YES];
        [[tc dataCell] setFont:[NSFont systemFontOfSize:[[NSUserDefaults standardUserDefaults] floatForKey:@"fontSize"]]];
        
        if (tc == tcSize)
            [[columnsMenu itemWithTag:0] setState:NSOnState];
        else if (tc == tcUsers)
            [[columnsMenu itemWithTag:1] setState:NSOnState];
        else if (tc == tcStatus)
            [[columnsMenu itemWithTag:2] setState:NSOnState];
        else if (tc == tcPriority)
            [[columnsMenu itemWithTag:3] setState:NSOnState];
        else if (tc == tcPath)
            [[columnsMenu itemWithTag:4] setState:NSOnState];
        else if (tc == tcTTH)
            [[columnsMenu itemWithTag:5] setState:NSOnState];
        else if (tc == tcExactSize)
            [[columnsMenu itemWithTag:6] setState:NSOnState];
    }
    
    [[tableView headerView] setMenu:columnsMenu];
}

- (void)dealloc
{
    // Free table columns
    [tcSize release];
    [tcUsers release];
    [tcStatus release];
    [tcPriority release];
    [tcPath release];
    [tcTTH release];
    [tcExactSize release];
    
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [rootItems release];
    
    [super dealloc];
}

#pragma mark -
#pragma mark Sidebar support

- (id)view
{
    return queueView;
}

- (BOOL)canClose
{
    return NO;
}

- (NSImage *)image
{
    return [NSImage imageNamed:@"queue"];
}

- (NSString *)title
{
    return @"Downloads";
}

- (NSMenu *)menu
{
    return queueMenu;
}

#pragma mark -
#pragma mark Menu handling

- (void)emptyMenu:(NSMenu *)aMenu
{
    int i;
    for (i = [aMenu numberOfItems]; i > 0; i--)
        [aMenu removeItemAtIndex:i - 1];
}

- (BOOL)validateMenuItem:(id)sender
{
    /* populate menu, and only do it once */
    if (sender == menuItemSearchByTTH) {
        [self emptyMenu:menuRemoveSource];
        [self emptyMenu:menuRemoveUserFromQueue];
        [self emptyMenu:menuBrowseUsersFiles];

        /* create unique set of nicks */
        NSMutableSet *nicks = [[NSMutableSet alloc] init];
        NSIndexSet *selectedIndices = [tableView selectedRowIndexes];
        unsigned int i = [selectedIndices firstIndex];
        while (i != NSNotFound) {
            SPQueueItem *qi = [tableView itemAtRow:i];
            [nicks addObjectsFromArray:[qi nicks]];
            i = [selectedIndices indexGreaterThanIndex:i];
        }

        if ([nicks count]) {
            NSEnumerator *e = [nicks objectEnumerator];
            NSString *nickName;
            while ((nickName = [e nextObject]) != nil) {
                id nickMenuItem;

                nickMenuItem = [menuRemoveSource addItemWithTitle:nickName
                                                           action:@selector(removeSource:)
                                                    keyEquivalent:@""];
                [nickMenuItem setTarget:self];
                [nickMenuItem setEnabled:YES];

                nickMenuItem = [menuRemoveUserFromQueue addItemWithTitle:nickName
                                                                  action:@selector(removeUserFromQueue:)
                                                           keyEquivalent:@""];
                [nickMenuItem setTarget:self];
                [nickMenuItem setEnabled:YES];

                nickMenuItem = [menuBrowseUsersFiles addItemWithTitle:nickName
                                                               action:@selector(browseUsersFiles:)
                                                        keyEquivalent:@""];
                [nickMenuItem setTarget:self];
                [nickMenuItem setEnabled:YES];

                [menuItemRemoveSource setEnabled:YES];
                [menuItemRemoveUserFromQueue setEnabled:YES];
                [menuItemBrowseUsersFiles setEnabled:YES];
            }
        }
        else {
            [menuItemRemoveSource setEnabled:NO];
            [menuItemRemoveUserFromQueue setEnabled:NO];
            [menuItemBrowseUsersFiles setEnabled:NO];
        }
        [nicks release];
    }

    if (sender == menuItemSearchByTTH) {
        /* only enable if there is any TTH available */
        NSIndexSet *selectedIndices = [tableView selectedRowIndexes];
        unsigned int i = [selectedIndices firstIndex];
        BOOL hasTTH = NO;
        while (i != NSNotFound) {
            SPQueueItem *qi = [tableView itemAtRow:i];
            if ([[qi tth] length] > 0) {
                hasTTH = YES;
                break;
            }
            i = [selectedIndices indexGreaterThanIndex:i];
        }
        return hasTTH;
    }

#if 0
    /* if the tag is set to 4711, only enable if single selection */
    if ([sender tag] == 4711) {
        BOOL singleSelection = ([[tableView selectedRowIndexes] count] == 1);
        return singleSelection;
    }
#endif

    return YES;
}

#pragma mark -
#pragma mark Utility functions

- (SPQueueItem *)findFilelistItemWithNick:(NSString *)aNick
{
    NSEnumerator *e = [rootItems objectEnumerator];
    SPQueueItem *qi;
    while ((qi = [e nextObject]) != nil) {
        if ([qi isFilelist]) {
            NSString *filelistNick = [[qi nicks] objectAtIndex:0];
            if ([filelistNick isEqualToString:aNick])
                return qi;
        }
    }
    return nil;
}

- (SPQueueItem *)findItemWithFilename:(NSString *)name inArray:(NSArray *)anArray
{
    NSEnumerator *e = [anArray objectEnumerator];
    SPQueueItem *qi;
    while ((qi = [e nextObject]) != nil) {
        if ([[qi filename] isEqualToString:name]) {
            return qi;
        }
    }
    return nil;
}

- (NSMutableArray *)findParent:(NSArray *)components
                       inArray:(NSMutableArray *)anArray
                          root:(NSString *)root
{
    if ([components count] == 0) {
        return anArray;
    }

    NSString *parentName = [components objectAtIndex:0];
    NSString *newRoot = root ? [NSString stringWithFormat:@"%@/%@", root, parentName] : nil;
    SPQueueItem *parent = [self findItemWithFilename:parentName inArray:anArray];
    if (parent == nil) {
        if (root) {
            parent = [[SPQueueItem alloc] initWithTarget:newRoot];
            [anArray addObject:parent];
            [parent setIsDirectory];
            [parent release];
        }
        else
            return nil;
    }

    return [self findParent:[components subarrayWithRange:NSMakeRange(1, [components count]-1)]
                    inArray:[parent children]
                       root:newRoot];
}

- (NSArray *)extraPathComponents:(NSString *)targetFilename
{
    NSString *targetDirectory = [targetFilename stringByDeletingLastPathComponent];

    return [targetDirectory pathComponents];
}

- (SPQueueItem *)findItemWithTarget:(NSString *)targetFilename
{
    NSMutableArray *parentArray = [self findParent:[self extraPathComponents:targetFilename]
                                           inArray:rootItems
                                              root:nil];
    if (parentArray) {
        return [self findItemWithFilename:[targetFilename lastPathComponent] inArray:parentArray];
    }
    return nil;
}

#pragma mark -
#pragma mark Interface actions

- (void)onDoubleClick:(id)sender
{
    SPQueueItem *qi = [tableView itemAtRow:[tableView selectedRow]];

    if (qi) {
        if ([qi isDirectory]) {
            if ([tableView isItemExpanded:qi])
                [tableView collapseItem:qi];
            else
                [tableView expandItem:qi];
        }
        else {
            [self openSelected:self];
        }
    }
}

- (IBAction)removeFromQueue:(id)sender
{
    NSIndexSet *selectedIndices = [tableView selectedRowIndexes];
    unsigned int i = [selectedIndices firstIndex];
    while (i != NSNotFound) {
        SPQueueItem *qi = [tableView itemAtRow:i];
        
        // set the download as pending removal from the queue, once we have
        // the notification from the backend that the download is aborted.
        [qi setIsWaitingToBeRemoved:YES];
        
        if ([qi isFilelist]) {
            [[SPApplicationController sharedApplicationController] removeFilelistForNick:[[qi nicks] objectAtIndex:0]];
        }
        else if ([qi isDirectory]) {
            [[SPApplicationController sharedApplicationController] removeDirectory:[qi target]];
        }
        else {
            [[SPApplicationController sharedApplicationController] removeQueue:[qi target]];
        }

        i = [selectedIndices indexGreaterThanIndex:i];
    }
    [tableView reloadData];
}

- (IBAction)removeUserFromQueue:(id)sender
{
    NSString *aNick = [sender title];
    if (aNick) {
        [[SPApplicationController sharedApplicationController] removeAllSourcesWithNick:aNick];
    }
}

- (IBAction)removeSource:(id)sender
{
    NSString *aNick = [sender title];
    if (aNick) {
        NSIndexSet *selectedIndices = [tableView selectedRowIndexes];
        unsigned int i = [selectedIndices firstIndex];
        while (i != NSNotFound) {
            SPQueueItem *qi = [tableView itemAtRow:i];

            if ([qi isFilelist])
                [[SPApplicationController sharedApplicationController] removeFilelistForNick:aNick];
            else
                [[SPApplicationController sharedApplicationController] removeSource:[qi target] nick:aNick];

            i = [selectedIndices indexGreaterThanIndex:i];
        }
    }
}

- (IBAction)clearAllFinishedDownloads:(id)sender
{
    [self clearAllFinishedDownloadsRecursivelyInArray:rootItems];
    [tableView reloadData];
}

- (void)clearAllFinishedDownloadsRecursivelyInArray:(NSMutableArray *)anArray
{
    NSEnumerator *e = [anArray objectEnumerator];
    SPQueueItem *qi;
    while ((qi = [e nextObject]) != nil) {
        if ([qi isFinished]) {
            [anArray removeObject:qi];
        }
        else if ([qi isDirectory]) {
            [self clearAllFinishedDownloadsRecursivelyInArray:[qi children]];
        }
    }
}

- (void)setPriority:(unsigned int)priority recursivelyInArray:(NSArray *)anArray
{
    NSEnumerator *e = [anArray objectEnumerator];
    SPQueueItem *qi;
    while ((qi = [e nextObject]) != nil) {
        if ([qi isDirectory]) {
            [self setPriority:priority recursivelyInArray:[qi children]];
        }
        else {
            [[SPApplicationController sharedApplicationController] setPriority:priority onTarget:[qi target]];
        }
    }
}

- (IBAction)setPriority:(id)sender
{
    NSIndexSet *selectedIndices = [tableView selectedRowIndexes];
    unsigned int i = [selectedIndices firstIndex];
    while (i != NSNotFound) {
        SPQueueItem *qi = [tableView itemAtRow:i];

        if ([qi isDirectory]) {
            [self setPriority:[sender tag] recursivelyInArray:[qi children]];
        }
        else {
            [[SPApplicationController sharedApplicationController] setPriority:[sender tag] onTarget:[qi target]];
        }

        i = [selectedIndices indexGreaterThanIndex:i];
    }
}

- (IBAction)searchByTTH:(id)sender
{
    NSIndexSet *selectedIndices = [tableView selectedRowIndexes];
    unsigned int i = [selectedIndices firstIndex];
    while (i != NSNotFound) {
        SPQueueItem *qi = [tableView itemAtRow:i];

        NSString *tth = [qi tth];
        if ([tth length] > 0) {
            [[SPMainWindowController sharedMainWindowController] performSearchFor:tth
                                                                             size:0
                                                                  sizeRestriction:SHARE_SIZE_NONE
                                                                             type:SHARE_TYPE_TTH
                                                                       hubAddress:nil];
        }

        i = [selectedIndices indexGreaterThanIndex:i];
    }
}

- (IBAction)searchForAlternates:(id)sender
{
    NSIndexSet *selectedIndices = [tableView selectedRowIndexes];
    unsigned int i = [selectedIndices firstIndex];
    while (i != NSNotFound) {
        SPQueueItem *qi = [tableView itemAtRow:i];

        NSMutableString *searchString = [NSMutableString stringWithString:[qi filename]];
        NSCharacterSet *punctuationSet = [NSCharacterSet punctuationCharacterSet];
        while (1) {
            NSRange r = [searchString rangeOfCharacterFromSet:punctuationSet];
            if (r.location == NSNotFound)
                break;
            [searchString replaceCharactersInRange:r withString:@" "];
        }

        [[SPMainWindowController sharedMainWindowController]
            performSearchFor:[searchString lowercaseString]
                        size:nil
             sizeRestriction:SHARE_SIZE_NONE
                        type:[qi filetype]
                  hubAddress:nil];

        i = [selectedIndices indexGreaterThanIndex:i];
    }
}

- (IBAction)browseUsersFiles:(id)sender
{
    [[SPApplicationController sharedApplicationController] downloadFilelistFromUser:[sender title]
                                                                              onHub:@""
                                                                        forceUpdate:NO
                                                                          autoMatch:NO];
}

- (IBAction)toggleColumn:(id)sender
{
    NSTableColumn *tc = nil;
    switch([sender tag]) {
        case 0: tc = tcSize; break;
        case 1: tc = tcUsers; break;
        case 2: tc = tcStatus; break;
        case 3: tc = tcPriority; break;
        case 4: tc = tcPath; break;
        case 5: tc = tcTTH; break;
        case 6: tc = tcExactSize; break;
    }
    if (tc == nil)
        return;

    if ([sender state] == NSOffState) {
        [sender setState:NSOnState];
        [tableView addTableColumn:tc];
    }
    else {
        [sender setState:NSOffState];
        [tableView removeTableColumn:tc];
    }
}

- (IBAction)openSelected:(id)sender
{
    NSIndexSet *selectedIndices = [tableView selectedRowIndexes];
    unsigned int i = [selectedIndices firstIndex];
    while (i != NSNotFound) {
        SPQueueItem *qi = [tableView itemAtRow:i];

        NSString *downloadFolder = [[NSUserDefaults standardUserDefaults]
            stringForKey:SPPrefsDownloadFolder];
        NSString *path = [NSString stringWithFormat:@"%@/%@",
                 downloadFolder, [qi target]];
        [[NSWorkspace sharedWorkspace] openFile:path];

        i = [selectedIndices indexGreaterThanIndex:i];
    }
}

- (IBAction)revealInFinder:(id)sender
{
    NSIndexSet *selectedIndices = [tableView selectedRowIndexes];
    unsigned int i = [selectedIndices firstIndex];
    while (i != NSNotFound) {
        SPQueueItem *qi = [tableView itemAtRow:i];

        NSString *downloadFolder = [[NSUserDefaults standardUserDefaults]
            stringForKey:SPPrefsDownloadFolder];
        NSString *path = [NSString stringWithFormat:@"%@/%@",
                 downloadFolder, [qi target]];
        [[NSWorkspace sharedWorkspace] selectFile:path
                         inFileViewerRootedAtPath:nil];

        i = [selectedIndices indexGreaterThanIndex:i];
    }
}

#pragma mark -
#pragma mark NSOutlineView delegate methods

- (int)outlineView:(NSOutlineView *)outlineView numberOfChildrenOfItem:(id)item
{
    if (item == nil)
        return [rootItems count];
    return [[(SPQueueItem *)item children] count];
}

- (BOOL)outlineView:(NSOutlineView *)outlineView isItemExpandable:(id)item
{
    return [item isDirectory];
}

- (id)outlineView:(NSOutlineView *)outlineView child:(int)index ofItem:(id)item
{
    if (item == nil)
        return [rootItems objectAtIndex:index];
    return [[(SPQueueItem *)item children] objectAtIndex:index];
}

- (id)outlineView:(NSOutlineView *)outlineView objectValueForTableColumn:(NSTableColumn *)tableColumn byItem:(id)item
{
    SPQueueItem *qi = item;
    NSString *identifier = [tableColumn identifier];
    if ([identifier isEqualToString:@"filename"]) {
        NSAttributedString *attributedDisplayName = [[[qi displayName] truncatedString:NSLineBreakByTruncatingMiddle] autorelease];
        return attributedDisplayName;
    }
    else if ([identifier isEqualToString:@"tth"]) {
        return [qi attributedTTH];
    }
    else if ([identifier isEqualToString:@"path"]) {
        return nil;
    }
    else if ([identifier isEqualToString:@"users"]) {
        return [qi isDirectory] ? @"" : [qi users];
    }
    else if ([identifier isEqualToString:@"size"]) {
        if ([qi isDirectory])
            return [NSString stringWithFormat:@"%u files", [[(SPQueueItem *)item children] count]];
        else
           return [[HumanSizeTransformer defaultHumanSizeTransformer] transformedValue:[qi size]];
    }
    else if ([identifier isEqualToString:@"exactSize"]) {
        return [qi isDirectory] ? nil : [qi exactSize];
    }
    else if ([identifier isEqualToString:@"priority"]) {
        return [qi isDirectory] ? nil : [qi priorityString];
    }
    else if ([identifier isEqualToString:@"status"]) {
        return [qi isDirectory] ? nil : [qi status];
    }
    else
        return @"foo";
}

- (void)outlineView:(NSOutlineView *)outlineView
    willDisplayCell:(id)cell
     forTableColumn:(NSTableColumn *)tableColumn
               item:(id)item
{
    SPQueueItem *qi = item;

    if ([[tableColumn identifier] isEqualToString:@"filename"]) {
        share_type_t type = [qi filetype];
        if ([qi isDirectory])
            type = SHARE_TYPE_DIRECTORY;
        NSImage *img = [[FiletypeImageTransformer defaultFiletypeImageTransformer]
            transformedValue:[NSNumber numberWithInt:type]];
        [cell setImage:img];
        [cell setLeaf:YES];
        [cell setFont:[NSFont systemFontOfSize:[[NSUserDefaults standardUserDefaults] floatForKey:@"fontSize"]]];
    }

    if ([qi isFinished]) {
        NSAttributedString *as = [cell attributedStringValue];
        NSMutableAttributedString *mas = [[NSMutableAttributedString alloc] initWithAttributedString:as];
        [mas addAttribute:NSForegroundColorAttributeName
                    value:[NSColor grayColor]
                    range:NSMakeRange(0, [as length])];
        [cell setAttributedStringValue:[mas autorelease]];
    }
}

- (void)sortArray:(NSMutableArray *)anArray usingDescriptors:(NSArray *)sortDescriptors
{
    [anArray sortUsingDescriptors:sortDescriptors];

    NSEnumerator *e = [anArray objectEnumerator];
    SPQueueItem *item;
    while ((item = [e nextObject]) != nil) {
        NSMutableArray *children = [item children];
        if (children) {
            [self sortArray:children usingDescriptors:sortDescriptors];
        }
    }
}

- (void)outlineView:(NSOutlineView *)outlineView sortDescriptorsDidChange:(NSArray *)oldDescriptors
{
    /* prepend a sort descriptor that sorts by isDirectory (gives directories above files) */
    NSSortDescriptor *sd1 = [[[NSSortDescriptor alloc] initWithKey:@"isDirectory"
                                                         ascending:NO] autorelease];
    NSArray *newSortDescriptors = [[NSArray arrayWithObjects:sd1, nil]
                               arrayByAddingObjectsFromArray:[outlineView sortDescriptors]];
    [self sortArray:rootItems usingDescriptors:newSortDescriptors];
    [outlineView reloadData];
}

#pragma mark -
#pragma mark Sphubd notifications

- (void)addFilelistNotification:(NSNotification *)aNotification
{
    // set the text in the 'file name' column to something comprehensible ("MrFoo's filelist")
    NSString *displayname = [NSString stringWithFormat:@"%@'s filelist", [[aNotification userInfo] objectForKey:@"nick"]];
    
    SPQueueItem *item = [[SPQueueItem alloc] initWithTarget:@"<filelist>" displayName:displayname];
    [item addSource:@"<filelist>" nick:[[aNotification userInfo] objectForKey:@"nick"]];
    [item setPriority:[[aNotification userInfo] objectForKey:@"priority"]];
    [item setIsFilelist:YES];

    [rootItems addObject:item];
    [item release];

    [tableView reloadData];
}

- (void)addDirectoryNotification:(NSNotification *)aNotification
{
    NSString *targetDirectory = [[aNotification userInfo] objectForKey:@"targetDirectory"];

    SPQueueItem *qi = [self findItemWithTarget:targetDirectory];
    if (qi == nil) {
        /* assemble the queue item */
        SPQueueItem *qi = [[SPQueueItem alloc] initWithTarget:targetDirectory];
        [qi setIsDirectory];

        [rootItems addObject:qi];
        [qi release];

        [tableView reloadData];
    }
}

- (void)addTargetNotification:(NSNotification *)aNotification
{
    NSString *targetFilename = [[aNotification userInfo] objectForKey:@"targetFilename"];

    /* assemble the queue item */
    SPQueueItem *item = [[SPQueueItem alloc] initWithTarget:targetFilename];
    [item setSize:[[aNotification userInfo] objectForKey:@"size"]];
    [item setTTH:[[aNotification userInfo] objectForKey:@"tth"]];
    [item setPriority:[[aNotification userInfo] objectForKey:@"priority"]];

    /* insert it into the hierarchy */
    NSMutableArray *parentArray = [self findParent:[self extraPathComponents:targetFilename]
                                           inArray:rootItems
                                              root:@""];
    [parentArray addObject:item];
    [item release];

    [tableView reloadData];
}

- (void)addSourceNotification:(NSNotification *)aNotification
{
    NSString *targetFilename = [[aNotification userInfo] objectForKey:@"targetFilename"];
    SPQueueItem *qi = [self findItemWithTarget:targetFilename];

    if (qi) {
        NSString *sourceFilename = [[aNotification userInfo] objectForKey:@"sourceFilename"];
        NSString *nick = [[aNotification userInfo] objectForKey:@"nick"];
        [qi addSource:sourceFilename nick:nick];

        [tableView reloadData];
    }
}

- (void)setFinished:(SPQueueItem *)qi
{
    [qi setFinished];

    if ([qi isDirectory]) {
        NSEnumerator *e = [[qi children] objectEnumerator];
        SPQueueItem *qic;
        while ((qic = [e nextObject]) != nil) {
            [self setFinished:qic];
        }
    }
}

- (void)removeTargetNotification:(NSNotification *)aNotification
{
    NSString *targetFilename = [[aNotification userInfo] objectForKey:@"targetFilename"];
    NSMutableArray *parentArray = [self findParent:[self extraPathComponents:targetFilename]
                                           inArray:rootItems
                                              root:nil];

    if (parentArray) {
        SPQueueItem *qi = [self findItemWithFilename:[targetFilename lastPathComponent] inArray:parentArray];
        if (qi) {
            if ([qi isWaitingToBeRemoved]) {
                // download was aborted by the user, so remove it.
                [parentArray removeObject:qi];
            }
            else
                [self setFinished:qi];
                
            [tableView reloadData];
        }
    }
}

- (void)removeFilelistNotification:(NSNotification *)aNotification
{
    SPQueueItem *qi = [self findFilelistItemWithNick:[[aNotification userInfo] objectForKey:@"nick"]];
    if (qi) {
        /* filelist items are always in the root */
        [rootItems removeObject:qi];
        [tableView reloadData];
    }
}

- (void)removeSourceNotification:(NSNotification *)aNotification
{
    NSString *targetFilename = [[aNotification userInfo] objectForKey:@"targetFilename"];
    SPQueueItem *qi = [self findItemWithTarget:targetFilename];

    if (qi) {
        NSString *nick = [[aNotification userInfo] objectForKey:@"nick"];

        [qi removeSourceForNick:nick];
        [tableView reloadData];
    }
}

- (void)setPriorityNotification:(NSNotification *)aNotification
{
    NSString *targetFilename = [[aNotification userInfo] objectForKey:@"targetFilename"];
    SPQueueItem *qi = [self findItemWithTarget:targetFilename];

    if (qi) {
        [qi setPriority:[[aNotification userInfo] objectForKey:@"priority"]];
        [tableView reloadData];
    }
}

- (void)setStatus:(NSString *)aStatusString forTarget:(NSString *)targetFilename
{
    SPQueueItem *qi = [self findItemWithTarget:targetFilename];
    if (qi) {
        [qi setStatus:aStatusString];
        [tableView reloadData];
    }
}

- (void)downloadStartingNotification:(NSNotification *)aNotification
{
    NSString *targetFilename = [[aNotification userInfo] objectForKey:@"targetFilename"];
    [self setStatus:@"Downloading" forTarget:targetFilename];
}

- (void)transferStatsNotification:(NSNotification *)aNotification
{
    NSString *targetFilename = [[aNotification userInfo] objectForKey:@"targetFilename"];
    SPQueueItem *qi = [self findItemWithTarget:targetFilename];
    if (qi) {
        uint64_t offset = [[[aNotification userInfo] objectForKey:@"offset"]
            unsignedLongLongValue];
        uint64_t size = [[[aNotification userInfo] objectForKey:@"size"]
            unsignedLongLongValue];

        [qi setStatus:[NSString stringWithFormat:@"%.1f%% ready", (float)offset / (size ? size : 1) * 100]];
        [tableView reloadData];
    }
}

- (void)transferAbortedNotification:(NSNotification *)aNotification
{
    NSString *targetFilename = [[aNotification userInfo] objectForKey:@"targetFilename"];
    [self setStatus:@"Aborted" forTarget:targetFilename];
}

@end

@implementation SPQueueItem

- (id)initWithTarget:(NSString *)aTarget
{
    if ((self = [super init])) {
        target = [aTarget retain];
        path = [[[aTarget stringByDeletingLastPathComponent] stringByAbbreviatingWithTildeInPath] retain];
        filename = [[aTarget lastPathComponent] retain];
        displayName = [filename retain];
        size = [[NSNumber numberWithUnsignedLongLong:0] retain];
        sources = [[NSMutableDictionary alloc] init];
        [self setPriority:[NSNumber numberWithInt:3]]; /* Normal priority */
        [self setStatus:@"Queued"];
    }
    return self;
}

- (id)initWithTarget:(NSString *)aTarget displayName:(NSString *)aDisplayName
{
    if ((self = [self initWithTarget:aTarget])) {
        [displayName autorelease];
        displayName = [aDisplayName copy];
    }
    
    return self;
}

- (void)dealloc
{
    [target release];
    [filename release];
    [displayName release];
    [path release];
    [tth release];
    [attributedTTH release];
    [size release];
    [priority release];
    [priorityString release];
    [sources release];
    [children release];
    [status release];
    [super dealloc];
}

- (NSString *)filename
{
    return filename;
}

- (NSString *)displayName
{
    return displayName;
}

- (NSString *)path
{
    return path;
}

- (NSString *)target
{
    return target;
}

- (NSString *)tth
{
    return tth;
}

- (NSAttributedString *)attributedTTH
{
    return attributedTTH;
}

- (void)setTTH:(NSString *)aTTH
{
    if (aTTH != tth) {
        [tth release];
        tth = [aTTH retain];
        [attributedTTH release];
        attributedTTH = [tth truncatedString:NSLineBreakByTruncatingMiddle];
    }
}

- (NSNumber *)size
{
    return size;
}

- (NSNumber *)exactSize
{
    return size;
}

- (void)setSize:(NSNumber *)aSize
{
    if (aSize != size) {
        [size release];
        size = [aSize retain];
    }
}

- (void)addSource:(NSString *)sourcePath nick:(NSString *)nick
{
    [sources setObject:sourcePath forKey:nick];
}

- (void)removeSourceForNick:(NSString *)nick
{
    [sources removeObjectForKey:nick];
}

- (NSArray *)nicks
{
    return [sources allKeys];
}

- (NSMutableArray *)children
{
    return children;
}

- (void)setIsFilelist:(BOOL)aFlag
{
    isFilelist = aFlag;
}

- (BOOL)isFilelist
{
    return isFilelist;
}

- (void)setIsDirectory
{
    isDirectory = YES;
    if (children == nil)
        children = [[NSMutableArray alloc] init];
}

- (BOOL)isDirectory
{
    return isDirectory;
}

- (NSString *)users
{
    NSArray *nicks = [self nicks];

    NSString *usersString = nil;
    switch([nicks count]) {
        case 0:
            usersString = @"none";
            break;
        case 1:
            usersString = [nicks objectAtIndex:0];
            break;
        case 2:
            usersString = [NSString stringWithFormat:@"%@, %@", [nicks objectAtIndex:0], [nicks objectAtIndex:1]];
            break;
        case 3:
            usersString = [NSString stringWithFormat:@"%@, %@, %@", [nicks objectAtIndex:0], [nicks objectAtIndex:1], [nicks objectAtIndex:2]];
            break;
        default:
            usersString = [NSString stringWithFormat:@"%i users", [nicks count]];
            break;
    }
    return [[usersString truncatedString:NSLineBreakByTruncatingTail] autorelease];
}

- (int)filetype
{
    return share_filetype([filename UTF8String]);
}

- (NSNumber *)priority
{
    return priority;
}

- (NSAttributedString *)priorityString
{
    return priorityString;
}

- (void)setPriority:(NSNumber *)aPriority
{
    if (aPriority != priority) {
        [priority release];
        priority = [aPriority retain];

        [priorityString release];

        switch([aPriority unsignedIntValue]) {
            case 0:
                priorityString = [@"Paused" truncatedString:NSLineBreakByTruncatingMiddle];
                break;
            case 1:
                priorityString = [@"Lowest" truncatedString:NSLineBreakByTruncatingMiddle];
                break;
            case 2:
                priorityString = [@"Low" truncatedString:NSLineBreakByTruncatingMiddle];
                break;
            case 4:
                priorityString = [@"High" truncatedString:NSLineBreakByTruncatingMiddle];
                break;
            case 5:
                priorityString = [@"Highest" truncatedString:NSLineBreakByTruncatingMiddle];
                break;
            case 3:
           default:
                priorityString = [@"Normal" truncatedString:NSLineBreakByTruncatingMiddle];
                break;
        }
    }
}

- (void)setStatus:(NSString *)aStatusString
{
    [status release];
    status = [aStatusString truncatedString:NSLineBreakByTruncatingTail];
}

- (NSAttributedString *)status
{
    return status;
}

- (void)setFinished
{
    [self setStatus:@"Finished"];
    isFinished = YES;
}

- (BOOL)isFinished
{
    return isFinished;
}

- (BOOL)isWaitingToBeRemoved
{
    return isWaitingToBeRemoved;
}

- (void)setIsWaitingToBeRemoved:(BOOL)waitingToBeRemoved
{
    isWaitingToBeRemoved = waitingToBeRemoved;
}

@end

