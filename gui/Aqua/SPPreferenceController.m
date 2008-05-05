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

#import "SPPreferenceController.h"
#import "SPApplicationController.h"
#import "SPLog.h"
#import "SPUserDefaultKeys.h"
#import "SPNotificationNames.h"

#include "test_connection.h"

static float ToolbarHeightForWindow(NSWindow *window);

/* this code is from the apple documentation...
 */
static float ToolbarHeightForWindow(NSWindow *window)
{
    NSToolbar *toolbar = [window toolbar];
    float toolbarHeight = 0.0;
    NSRect windowFrame;

    if(toolbar && [toolbar isVisible])
    {
        windowFrame = [NSWindow contentRectForFrameRect:[window frame]
                                              styleMask:[window styleMask]];
        toolbarHeight = NSHeight(windowFrame) - NSHeight([[window contentView] frame]);
    }

    return toolbarHeight;
}

@implementation SPPreferenceController

- (id)init
{
    self = [super initWithWindowNibName:@"Preferences"];
    if(self)
    {
        /* Setup toolbar
         */
        blankView = [[NSView alloc] init];
        prefsToolbar = [[NSToolbar alloc] initWithIdentifier:@"prefsToolbar"];
        [prefsToolbar autorelease];
        [prefsToolbar setDelegate:self];
        [prefsToolbar setAllowsUserCustomization:NO];
        [prefsToolbar setAutosavesConfiguration:NO];
        [[self window] setToolbar:prefsToolbar];


        NSString *lastPrefsPane = [[NSUserDefaults standardUserDefaults] stringForKey:@"lastPrefsPane"];
        if(lastPrefsPane)
        {
            if([lastPrefsPane isEqualToString:@"identity"])
            {
                [[self window] setContentSize:[identityView frame].size];
                [[self window] setContentView:identityView];
            }
            else if([lastPrefsPane isEqualToString:@"shares"])
            {
                [[self window] setContentSize:[sharesView frame].size];
                [[self window] setContentView:sharesView];
            }
            else if([lastPrefsPane isEqualToString:@"network"])
            {
                [[self window] setContentSize:[networkView frame].size];
                [[self window] setContentView:networkView];
            }
            else if([lastPrefsPane isEqualToString:@"advanced"])
            {
                [[self window] setContentSize:[advancedView frame].size];
                [[self window] setContentView:advancedView];
            }
        }
        else
        {
            [[self window] setContentSize:[identityView frame].size];
            [[self window] setContentView:identityView];
        }

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(shareStatsNotification:)
                                                     name:SPNotificationShareStats
                                                   object:nil];

        sharedPaths = [[NSMutableArray alloc] init];
        [self setTotalShareSize:0LL];

        NSEnumerator *e = [[[NSUserDefaults standardUserDefaults] stringArrayForKey:SPPrefsSharedPaths] objectEnumerator];
        NSString *path;
        while((path = [e nextObject]) != nil)
        {
            [self addSharedPathsPath:path];
        }

        [self setWindowFrameAutosaveName:@"PreferenceWindow"];
    }
    return self;
}

+ (SPPreferenceController *)sharedPreferences
{
    static SPPreferenceController *sharedPreferenceController = nil;
    if(sharedPreferenceController == nil)
    {
        sharedPreferenceController = [[SPPreferenceController alloc] init];
    }
    return sharedPreferenceController;
}

- (void)dealloc
{
    [sharedPaths release];
    [super dealloc];
}

- (void)show
{
    [[self window] makeKeyAndOrderFront:self];
}

- (void)close
{
    [[self window] performClose:self];
}

- (BOOL)isKeyWindow
{
    return [[self window] isKeyWindow];
}

- (void)setTotalShareSize:(unsigned long long)aNumber
{
    totalShareSize = aNumber;
}

- (void)shareStatsNotification:(NSNotification *)aNotification
{
    NSString *path = [[aNotification userInfo] objectForKey:@"path"];

    if([path isEqualToString:@""])
    {
        unsigned long long size = [[[aNotification userInfo] objectForKey:@"size"] unsignedLongLongValue];
        [self setTotalShareSize:size];
        return;
    }

    NSEnumerator *e = [sharedPaths objectEnumerator];
    NSMutableDictionary *dict;
    while((dict = [e nextObject]) != nil)
    {
        if([[dict objectForKey:@"path"] isEqualToString:path])
        {
            unsigned long long size = [[[aNotification userInfo] objectForKey:@"size"] unsignedLongLongValue];
            unsigned long long totsize = [[[aNotification userInfo] objectForKey:@"totsize"] unsignedLongLongValue];
            unsigned long long dupsize = [[[aNotification userInfo] objectForKey:@"dupsize"] unsignedLongLongValue];
            unsigned long long uniqsize = totsize - dupsize;
            unsigned nfiles = [[[aNotification userInfo] objectForKey:@"nfiles"] intValue];
            unsigned ntotfiles = [[[aNotification userInfo] objectForKey:@"ntotfiles"] intValue];
            unsigned nduplicates = [[[aNotification userInfo] objectForKey:@"nduplicates"] intValue];
            unsigned nunique = ntotfiles - nduplicates;
            unsigned percentComplete = (unsigned)(100 * ((double)size / (uniqsize ? uniqsize : 1)));

            [self willChangeValueForKey:@"size"];
            [self willChangeValueForKey:@"nfiles"];
            [self willChangeValueForKey:@"percentComplete"];
            [self willChangeValueForKey:@"nleft"];
            [self willChangeValueForKey:@"nduplicates"];

            [dict setObject:[NSNumber numberWithUnsignedLongLong:size] forKey:@"size"];
            [dict setObject:[NSNumber numberWithInt:nfiles] forKey:@"nfiles"];
            [dict setObject:[NSNumber numberWithInt:percentComplete] forKey:@"percentComplete"];
            [dict setObject:[NSNumber numberWithInt:nunique - nfiles] forKey:@"nleft"];
            [dict setObject:[NSNumber numberWithInt:nduplicates] forKey:@"nduplicates"];

            [self didChangeValueForKey:@"nduplicates"];
            [self didChangeValueForKey:@"nleft"];
            [self didChangeValueForKey:@"percentComplete"];
            [self didChangeValueForKey:@"nfiles"];
            [self didChangeValueForKey:@"size"];
        }
    }
}

- (void)resizeWindowToSize:(NSSize)newSize
{
    NSRect aFrame;

    float newHeight = newSize.height + ToolbarHeightForWindow([self window]);
    float newWidth = newSize.width;

    aFrame = [NSWindow contentRectForFrameRect:[[self window] frame]
                                     styleMask:[[self window] styleMask]];

    aFrame.origin.y += aFrame.size.height;
    aFrame.origin.y -= newHeight;
    aFrame.size.height = newHeight;
    aFrame.size.width = newWidth;

    aFrame = [NSWindow frameRectForContentRect:aFrame styleMask:[[self window] styleMask]];

    [[self window] setFrame:aFrame display:YES animate:YES];
}

- (void)prefsInitView:(NSView *)aView
{
    NSSize newSize;

    newSize = [aView frame].size;

    [[self window] setContentView:blankView];
    [self resizeWindowToSize:newSize];
    [[self window] setContentView:aView];
}

- (void)prefsInitIdentityView:(id)sender
{
    [self prefsInitView:identityView];
    [[NSUserDefaults standardUserDefaults] setObject:@"identity" forKey:@"lastPrefsPane"];
}

- (void)prefsInitSharesView:(id)sender
{
    [self prefsInitView:sharesView];
    [[NSUserDefaults standardUserDefaults] setObject:@"shares" forKey:@"lastPrefsPane"];
}

- (void)prefsInitNetworkView:(id)sender
{
    [self prefsInitView:networkView];
    [[NSUserDefaults standardUserDefaults] setObject:@"network" forKey:@"lastPrefsPane"];
}

- (void)prefsInitAdvancedView:(id)sender
{
    [self prefsInitView:advancedView];
    [[NSUserDefaults standardUserDefaults] setObject:@"advanced" forKey:@"lastPrefsPane"];
}

- (NSToolbarItem *)toolbar:(NSToolbar *)toolbar itemForItemIdentifier:(NSString *)itemIdentifier willBeInsertedIntoToolbar:(BOOL)flag
{
    NSToolbarItem *item = [[NSToolbarItem alloc] initWithItemIdentifier:itemIdentifier];

    [item setTag:0];
    [item setTarget:self];

    if([itemIdentifier isEqualToString:@"IdentityItem"])
    {
        [item setLabel:@"Identity"];
        [item setImage:[NSImage imageNamed:@"identity"]];
        [item setAction:@selector(prefsInitIdentityView:)];
    }
    else if([itemIdentifier isEqualToString:@"ShareItem"])
    {
        [item setLabel:@"Share"];
        [item setImage:[NSImage imageNamed:@"share"]];
        [item setAction:@selector(prefsInitSharesView:)];
    }
    else if([itemIdentifier isEqualToString:@"NetworkItem"])
    {
        [item setLabel:@"Network"];
        [item setImage:[NSImage imageNamed:@"network"]];
        [item setAction:@selector(prefsInitNetworkView:)];
    }
    else if([itemIdentifier isEqualToString:@"AdvancedItem"])
    {
        [item setLabel:@"Advanced"];
        [item setImage:[NSImage imageNamed:@"advanced-prefs"]];
        [item setAction:@selector(prefsInitAdvancedView:)];
    }
    [item setPaletteLabel:[item label]];
    return [item autorelease];
}

- (NSArray *)toolbarAllowedItemIdentifiers:(NSToolbar *)toolbar
{
    if(toolbar == prefsToolbar)
    {
        return [NSArray arrayWithObjects:
            NSToolbarSeparatorItemIdentifier,
            NSToolbarSpaceItemIdentifier,
            NSToolbarFlexibleSpaceItemIdentifier,
            @"IdentityItem",
            @"ShareItem",
            @"NetworkItem",
            @"AdvancedItem",
            nil];
    }
    return nil;
}

- (NSArray *)toolbarDefaultItemIdentifiers:(NSToolbar *)toolbar
{
    if(toolbar == prefsToolbar)
    {
        return [NSArray arrayWithObjects:
            @"IdentityItem",
            @"ShareItem",
            @"NetworkItem",
            @"AdvancedItem",
            nil];
    }
    return nil;
}

- (void)addSharedPathsPath:(NSString *)aPath
{
    NSMutableDictionary *newPath = [NSMutableDictionary dictionaryWithObjectsAndKeys:aPath, @"path",
                                                [NSNumber numberWithUnsignedLongLong:0L], @"size",
                                                             [NSNumber numberWithInt:0], @"nfiles",
                                                             [NSNumber numberWithInt:0], @"percentComplete",
                                                             [NSNumber numberWithInt:0], @"nleft",
                                                             [NSNumber numberWithInt:0], @"nduplicates", nil];

    [self willChangeValueForKey:@"sharedPaths"];
    [sharedPaths addObject:newPath];
    [self didChangeValueForKey:@"sharedPaths"];
}

- (IBAction)addSharedPath:(id)sender
{
    NSOpenPanel *op = [NSOpenPanel openPanel];

    [op setCanChooseDirectories:YES];
    [op setCanChooseFiles:NO];
    [op setAllowsMultipleSelection:YES];

    if([op runModalForTypes:nil] == NSOKButton)
    {
        NSEnumerator *e = [[op filenames] objectEnumerator];
        NSString *path;

        while((path = [e nextObject]))
        {
            [[SPApplicationController sharedApplicationController] addSharedPath:path];

            NSArray *tmp = [[NSUserDefaults standardUserDefaults] stringArrayForKey:SPPrefsSharedPaths];
            [[NSUserDefaults standardUserDefaults] setObject:[tmp arrayByAddingObject:path]
                                                      forKey:SPPrefsSharedPaths];

            [self addSharedPathsPath:path];

        }
    }
}

- (IBAction)removeSharedPath:(id)sender
{
    NSArray *selectedPaths = [sharedPathsController selectedObjects];
    NSEnumerator *enumerator = [selectedPaths objectEnumerator];
    NSDictionary *record;

    while((record = [enumerator nextObject]))
    {
        [[SPApplicationController sharedApplicationController] removeSharedPath:[record objectForKey:@"path"]];

        NSMutableArray *x = [[[NSMutableArray alloc] init] autorelease];
        [x setArray:[[NSUserDefaults standardUserDefaults] stringArrayForKey:SPPrefsSharedPaths]];
        [x removeObject:[record objectForKey:@"path"]];
        [[NSUserDefaults standardUserDefaults] setObject:x forKey:SPPrefsSharedPaths];

        NSEnumerator *e = [sharedPaths objectEnumerator];
        NSMutableDictionary *dict;
        while((dict = [e nextObject]) != nil)
        {
            if([[dict objectForKey:@"path"] isEqualToString:[record objectForKey:@"path"]])
            {
                [self willChangeValueForKey:@"sharedPaths"];
                [sharedPaths removeObject:dict];
                [self didChangeValueForKey:@"sharedPaths"];
                break;
            }
        }
    }
}

- (void)updateAllSharedPaths
{
    NSEnumerator *enumerator = [sharedPaths objectEnumerator];
    NSDictionary *record;
    while((record = [enumerator nextObject]))
    {
        [[SPApplicationController sharedApplicationController] addSharedPath:[record objectForKey:@"path"]];
    }
}

- (IBAction)updateSharedPaths:(id)sender
{
    NSArray *selectedPaths = [sharedPathsController selectedObjects];
    NSEnumerator *enumerator = [selectedPaths objectEnumerator];
    NSDictionary *record;
    while((record = [enumerator nextObject]))
    {
        [[SPApplicationController sharedApplicationController] addSharedPath:[record objectForKey:@"path"]];
    }
}

- (NSString *)selectFolder
{
    NSOpenPanel *op = [NSOpenPanel openPanel];

    [op setCanChooseDirectories:YES];
    [op setCanChooseFiles:NO];
    [op setAllowsMultipleSelection:NO];

    if([op runModalForTypes:nil] == NSOKButton)
    {
        return [[op filenames] objectAtIndex:0];
    }
    return nil;
}

- (IBAction)selectDownloadFolder:(id)sender
{
    NSString *path = [self selectFolder];
    if(path)
    {
        [downloadFolder setStringValue:path];
        [[NSUserDefaults standardUserDefaults] setObject:path forKey:SPPrefsDownloadFolder];
    }
}

- (IBAction)selectCompleteFolder:(id)sender
{
    NSString *path = [self selectFolder];
    if(path)
    {
        [completeFolder setStringValue:path];
        [[NSUserDefaults standardUserDefaults] setObject:path forKey:SPPrefsCompleteFolder];
    }
}

- (IBAction)setPort:(id)sender
{
    [[SPApplicationController sharedApplicationController] setPort:[sender intValue]];
}

- (IBAction)setIPAddress:(id)sender
{
    if([IPAddressField isEnabled])
        [[SPApplicationController sharedApplicationController] setIPAddress:[IPAddressField stringValue]];
    else
        [[SPApplicationController sharedApplicationController] setIPAddress:@""];
}

- (IBAction)updateUserInfo:(id)sender
{
    NSString *email = [emailField stringValue];
    NSString *description = [descriptionField stringValue];
    NSString *speed = [speedField stringValue];
    [[SPApplicationController sharedApplicationController] updateUserEmail:email
                                                               description:description
                                                                     speed:speed];
}

- (void)updateTestConnectionResultFromErrors:(NSNumber *)wrappedStatus
{
    int status = [wrappedStatus intValue];

    [testConnectionProgress stopAnimation:self];
    if(status == TC_RET_OK)
    {
        [testResults setStringValue:@"Both TCP and UDP tested OK"];
    }
    else
    {
        [testResults setTextColor:[NSColor redColor]];
        NSString *errmsg = nil;
        if(status & TC_RET_PRIVPORT)
        {
            errmsg = @"Refused to test privileged port";
        }
        else if((status & TC_RET_TCP_FAIL) || (status & TC_RET_UDP_FAIL))
        {
            errmsg = @"TCP and/or UDP port unreachable";
        }
        else
        {
            errmsg = @"Internal error";
        }
        [testResults setStringValue:errmsg];
    }
}

- (void)testConnectionThread:(id)args
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    testInProgress = YES;
    int port = [portField intValue];

    int status = test_connection(port);

    NSNumber *wrappedStatus = [NSNumber numberWithInt:status];

    [self performSelectorOnMainThread:@selector(updateTestConnectionResultFromErrors:)
                           withObject:wrappedStatus
                        waitUntilDone:YES];

    testInProgress = NO;
    [pool release];
}

- (IBAction)testConnection:(id)sender
{
    [self setPort:portField];
    int port = [portField intValue];

    [testResults setTextColor:[NSColor blackColor]];
    [testResults setStringValue:[NSString stringWithFormat:@"Testing port %i", port]];
    [testConnectionProgress startAnimation:self];

    if(testInProgress == NO)
    {
        [NSThread detachNewThreadSelector:@selector(testConnectionThread:) toTarget:self withObject:nil];
    }

}

- (IBAction)setSlots:(id)sender
{
    [[SPApplicationController sharedApplicationController] setSlots:[slotsField intValue]
                                                             perHub:[slotsPerHubButton state]];
}

- (IBAction)setConnectionMode:(id)sender
{
    int passive = [sender indexOfSelectedItem];

    if(passive)
        [[SPApplicationController sharedApplicationController] setPassiveMode];
    else
        [self setPort:portField];
}

- (IBAction)setLogLevel:(id)sender
{
    [[SPApplicationController sharedApplicationController] setLogLevel:[sender title]];
}

- (IBAction)setRescanShareInterval:(id)sender
{
    [[SPApplicationController sharedApplicationController] setRescanShareInterval:[rescanShareField floatValue] * 3600];
}

- (IBAction)setFollowRedirects:(id)sender
{
    [[SPApplicationController sharedApplicationController] setFollowHubRedirects:[sender state] == NSOnState ? 1 : 0];
}

- (IBAction)togglePauseHashing:(id)sender
{
    if(hashingPaused)
    {
        [[SPApplicationController sharedApplicationController] resumeHashing];
        [sender setTitle:@"Pause hashing"];
        hashingPaused = NO;
    }
    else
    {
        [[SPApplicationController sharedApplicationController] pauseHashing];
        [sender setTitle:@"Resume hashing"];
        hashingPaused = YES;
    }
}

- (IBAction)setAutoSearchNewSources:(id)sender
{
    [[SPApplicationController sharedApplicationController] setAutoSearchNewSources:[sender state] == NSOnState ? 1 : 0];
}

- (IBAction)setHashingPriority:(id)sender
{
    [[SPApplicationController sharedApplicationController] setHashingPriority:[[sender selectedItem] tag]];
}

@end

