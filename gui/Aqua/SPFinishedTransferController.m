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

#import "SPFinishedTransferController.h"
#import "SPMainWindowController.h"
#import "SPTransferController.h"
#import "SPSideBar.h"

@implementation SPFinishedTransferController

+ (SPFinishedTransferController *)sharedFinishedTransferController
{
    static SPFinishedTransferController *sharedFinishedTransferController = nil;
    if(sharedFinishedTransferController == nil)
    {
        sharedFinishedTransferController = [[SPFinishedTransferController alloc] init];
    }
    return sharedFinishedTransferController;
}

- (id)init
{
    self = [super init];
    if(self)
    {
        [NSBundle loadNibNamed:@"FinishedTransfers" owner:self];
    }
    
    return self;
}

- (void)awakeFromNib
{
    [tcUser retain];
    [tcFilename retain];
    [tcSize retain];
    [tcSpeed retain];
    [tcTotal retain];
    [tcHub retain];
    
    NSArray *tcs = [finishedTransferTable tableColumns];
    NSEnumerator *e = [tcs objectEnumerator];
    NSTableColumn *tc;
    while((tc = [e nextObject]) != nil)
    {
        [[tc dataCell] setWraps:YES];
        
        if(tc == tcUser)
            [[columnsMenu itemWithTag:0] setState:NSOnState];
        else if(tc == tcFilename)
            [[columnsMenu itemWithTag:1] setState:NSOnState];
        else if(tc == tcSize)
            [[columnsMenu itemWithTag:2] setState:NSOnState];
        else if(tc == tcSpeed)
            [[columnsMenu itemWithTag:3] setState:NSOnState];
        else if(tc == tcTotal)
            [[columnsMenu itemWithTag:4] setState:NSOnState];
        else if(tc == tcHub)
            [[columnsMenu itemWithTag:5] setState:NSOnState];
    }
    
    [[finishedTransferTable headerView] setMenu:columnsMenu];
}

- (void)dealloc 
{
    // Free table columns
    [tcUser release];
    [tcFilename release];
    [tcSize release];
    [tcSpeed release];
    [tcTotal release];
    [tcHub release];
    
    [super dealloc];
}

- (NSView *)view
{
    return myView;
}

- (NSString *)title
{
    return @"Finished Transfers";
}

- (NSImage *)image
{
    return [NSImage imageNamed:@"finished_transfers"];
}

- (NSMenu *)menu
{
    return nil;
}

- (void)addItem:(SPTransferItem *)anItem
{
    [arrayController addObject:anItem];
}

- (IBAction)toggleColumn:(id)sender
{
    NSTableColumn *tc = nil;
    switch([sender tag])
    {
        case 0: tc = tcUser; break;
        case 1: tc = tcFilename; break;
        case 2: tc = tcSize; break;
        case 3: tc = tcSpeed; break;
        case 4: tc = tcTotal; break;
        case 5: tc = tcHub; break;
    }
    if(tc == nil)
        return;
    
    if([sender state] == NSOffState)
    {
        [sender setState:NSOnState];
        [finishedTransferTable addTableColumn:tc];
    }
    else
    {
        [sender setState:NSOffState];
        [finishedTransferTable removeTableColumn:tc];
    }
}

@end

