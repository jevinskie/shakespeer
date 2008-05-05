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

#import "SPPublicHubsController.h"
#import "SPApplicationController.h"
#import "SPMainWindowController.h"
#import "NSStringExtensions.h"
#import "SPSideBar.h"
#import "FilteringArrayController.h"
#import "SPUserDefaultKeys.h"
#import "SPBookmarkController.h"

#include "util.h"
#include "hublist.h"

@implementation SPPublicHubsController

- (id)init
{
    self = [super init];
    if(self)
    {
        [NSBundle loadNibNamed:@"PublicHubs" owner:self];
        [hubTable setTarget:self];
        [hubTable setDoubleAction:@selector(tableDoubleActionConnect:)];
        
        [arrayController setSearchKeys:
             [NSArray arrayWithObjects:@"description", @"name",
            @"address", @"location", nil]];
        hubs = [[NSMutableArray alloc] init];

        char *hublist_filename = hl_get_current();

        if(hublist_filename)
        {
            xerr_t *err = 0;
            hublist_t *hublist = hl_parse_file(hublist_filename, &err);
            if(hublist == NULL)
            {
                [[SPMainWindowController sharedMainWindowController]
                    statusMessage:[NSString stringWithFormat:@"Failed to load hublist: %s", xerr_msg(err)]
                              hub:nil];
                xerr_free(err);
            }
            [self setHubsFromList:hublist];
            hl_free(hublist);
            free(hublist_filename);
        }
        else
        {
            [self refresh:self];
        }
    }
    
    return self;
}

- (void)awakeFromNib
{
    [tcName retain];
    [tcDescription retain];
    [tcAddress retain];
    [tcLocation retain];
    [tcUsers retain];
    [tcMinshare retain];
    [tcMinslots retain];
    [tcMaxhubs retain];
    
    NSArray *tcs = [hubTable tableColumns];
    NSEnumerator *e = [tcs objectEnumerator];
    NSTableColumn *tc;
    while((tc = [e nextObject]) != nil)
    {
        [[tc dataCell] setWraps:YES];
        
        if(tc == tcName)
            [[columnsMenu itemWithTag:0] setState:NSOnState];
        else if(tc == tcDescription)
            [[columnsMenu itemWithTag:1] setState:NSOnState];
        else if(tc == tcAddress)
            [[columnsMenu itemWithTag:2] setState:NSOnState];
        else if(tc == tcLocation)
            [[columnsMenu itemWithTag:3] setState:NSOnState];
        else if(tc == tcUsers)
            [[columnsMenu itemWithTag:4] setState:NSOnState];
        else if(tc == tcMinshare)
            [[columnsMenu itemWithTag:5] setState:NSOnState];
        else if(tc == tcMinslots)
            [[columnsMenu itemWithTag:6] setState:NSOnState];
        else if(tc == tcMaxhubs)
            [[columnsMenu itemWithTag:7] setState:NSOnState];
    }
    
    [[hubTable headerView] setMenu:columnsMenu];
    [hubTable setMenu:contextMenu];
}

- (void)unbindControllers
{
}

- (void)dealloc
{
    // Free top level nib-file objects
    [hubListView release];
    [arrayController release];
    
    // Free table columns
    [tcName release];
    [tcDescription release];
    [tcAddress release];
    [tcLocation release];
    [tcUsers release];
    [tcMinshare release];
    [tcMinslots release];
    [tcMaxhubs release];

    // Free other objects
    [hubs release];
    [super dealloc];
}

- (id)view
{
    return hubListView;
}

- (NSString *)title
{
    return @"Public Hubs";
}

- (NSImage *)image
{
    return [NSImage imageNamed:@"public_hubs"];
}

- (NSMenu *)menu
{
    return nil;
}

- (IBAction)tableDoubleActionConnect:(id)sender
{
    NSString *address = [[[arrayController selection] valueForKey:@"address"] string];
    if(address)
    {
        [[SPApplicationController sharedApplicationController] connectWithAddress:address
                                                                             nick:nil
                                                                      description:nil
                                                                         password:nil
                                                                         encoding:nil];
    }
}

- (IBAction)connect:(id)sender
{
    /* connect to all selected items in the table */
    NSString *address;
    NSArray *items = [arrayController selectedObjects];
    NSEnumerator *e = [items objectEnumerator];
    NSDictionary *dict;
    while((dict = [e nextObject]) != nil)
    {
        address = [[dict valueForKey:@"address"] string];
        [[SPApplicationController sharedApplicationController] connectWithAddress:address
                                                                             nick:nil
                                                                      description:nil
                                                                         password:nil
                                                                         encoding:nil];
    }
}

- (void)setHubsFromList:(hublist_t *)hublist
{
    NSMutableArray *hubArray = [[NSMutableArray alloc] init];
    if(hublist)
    {
        hublist_hub_t *h;
        LIST_FOREACH(h, hublist, link)
        {
            NSMutableDictionary *hub = [NSMutableDictionary dictionary];

            NSString *str = h->name ? [NSString stringWithUTF8String:h->name] : @"";
            [hub setObject:[[str truncatedString:NSLineBreakByTruncatingTail] autorelease] forKey:@"name"];

            str = h->address ? [NSString stringWithUTF8String:h->address] : @"";
            [hub setObject:[[str truncatedString:NSLineBreakByTruncatingTail] autorelease] forKey:@"address"];

            str = h->country ? [NSString stringWithUTF8String:h->country] : @"";
            [hub setObject:[[str truncatedString:NSLineBreakByTruncatingTail] autorelease] forKey:@"location"];

            str = h->description ? [NSString stringWithUTF8String:h->description] : @"";
            [hub setObject:[[str truncatedString:NSLineBreakByTruncatingTail] autorelease] forKey:@"description"];

            [hub setObject:[NSNumber numberWithInt:h->max_users] forKey:@"users"];
            [hub setObject:[NSNumber numberWithUnsignedLongLong:h->min_share] forKey:@"minShare"];
            [hub setObject:[NSNumber numberWithInt:h->min_slots] forKey:@"minSlots"];
            [hub setObject:[NSNumber numberWithInt:h->max_hubs] forKey:@"maxHubs"];

            [hubArray addObject:hub];
        }
    }

    [self willChangeValueForKey:@"hubs"];
    [hubs setArray:hubArray];
    [self didChangeValueForKey:@"hubs"];
    [hubArray release];
}

- (void)setHubsFromListWrapped:(NSValue *)wrappedPointer
{
    [self setHubsFromList:[wrappedPointer pointerValue]];
}

- (void)refreshThread:(id)args
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    refreshInProgress = YES;
    char *working_directory = get_working_directory();
    xerr_t *err = 0;
    hublist_t *hublist = hl_parse_url(
            [[[NSUserDefaults standardUserDefaults] stringForKey:SPPrefsHublistURL] UTF8String],
            working_directory, &err);
    if(hublist == NULL)
    {
        [[SPMainWindowController sharedMainWindowController]
            statusMessage:[NSString stringWithFormat:@"Failed to load hublist: %s", xerr_msg(err)]
                      hub:nil];
        xerr_free(err);
    }
    free(working_directory);
    NSValue *wrappedList = [NSValue valueWithPointer:hublist];
    [self performSelectorOnMainThread:@selector(setHubsFromListWrapped:)
                           withObject:wrappedList
                        waitUntilDone:YES];
    hl_free(hublist);
    refreshInProgress = NO;

    [pool release];
}

- (IBAction)refresh:(id)sender
{
    if(refreshInProgress == NO)
    {
        [NSThread detachNewThreadSelector:@selector(refreshThread:) toTarget:self withObject:nil];
    }
}

- (IBAction)toggleColumn:(id)sender
{
    NSTableColumn *tc = nil;
    switch([sender tag])
    {
        case 0: tc = tcName; break;
        case 1: tc = tcDescription; break;
        case 2: tc = tcAddress; break;
        case 3: tc = tcLocation; break;
        case 4: tc = tcUsers; break;
        case 5: tc = tcMinshare; break;
        case 6: tc = tcMinslots; break;
        case 7: tc = tcMaxhubs; break;
    }
    if(tc == nil)
        return;
    
    if([sender state] == NSOffState)
    {
        [sender setState:NSOnState];
        [hubTable addTableColumn:tc];
    }
    else
    {
        [sender setState:NSOffState];
        [hubTable removeTableColumn:tc];
    }
}

- (void)tableDidRecieveEnterKey:(id)sender
{
    [self tableDoubleActionConnect:sender];
}

- (void)addHubsToBookmarks:(id)sender
{
    // add all selected hubs to bookmarks
    NSArray *items = [arrayController selectedObjects];
    NSEnumerator *e = [items objectEnumerator];
    NSDictionary *dict;
    while((dict = [e nextObject]) != nil)
    {
        [[SPBookmarkController sharedBookmarkController] addBookmarkWithName:[[dict valueForKey:@"name"] string]
                                                                     address:[[dict valueForKey:@"address"] string]
                                                                        nick:@""
                                                                    password:@""
                                                                 description:@""
                                                                 autoconnect:NO
                                                                    encoding:nil];
    }
}

@end

