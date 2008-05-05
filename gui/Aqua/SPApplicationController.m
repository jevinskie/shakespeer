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

#include <sys/types.h>
#include <sys/time.h>
#include <event.h>

#include <stdarg.h>
#include <unistd.h>

#import <SystemConfiguration/SCNetwork.h>

#import "SPApplicationController.h"
#import "SPTransformers.h"
#import "SPPreferenceController.h"
#import "SPBookmarkController.h"
#import "SPMessagePanel.h"
#import "SPLog.h"
#import "SPMainWindowController.h"
#import "SPClientBridge.h"
#import "SPNotificationNames.h"
#import "SPUserDefaultKeys.h"

#include "nmdc.h"
#include "log.h"
#ifndef VERSION
# include "../../version.h"
#endif

static SPApplicationController *mySharedApplicationController = nil;

#pragma mark -
#pragma mark spclient command handlers

@implementation SPApplicationController

- (id)init
{
    if ((self = [super init])) {
        mySharedApplicationController = self;

        char *working_directory = get_working_directory();
        sp_log_init(working_directory, "shakespeer-aqua");
        free(working_directory);

        sp = sp_create(NULL);
        sp_register_callbacks(sp);

        NSDictionary *appDefaults = [NSDictionary dictionaryWithObjectsAndKeys:
            @"Bookmarks", SPPrefsLastSidebarItem,
            @"IdentityItem", SPPrefsLastPrefPane,
            [@"~/Desktop/ShakesPeer Downloads" stringByExpandingTildeInPath], SPPrefsDownloadFolder,
            [@"~/Desktop/ShakesPeer Downloads/Incomplete" stringByExpandingTildeInPath], SPPrefsIncompleteFolder,
            @"unconfigured-shakespeer-user", SPPrefsNickname,
            @"", SPPrefsEmail,
            @"DSL", SPPrefsSpeed,
            @"", SPPrefsDescription,
            [NSNumber numberWithInt:11], SPPrefsFontSize,
            [NSNumber numberWithInt:3], SPPrefsUploadSlots,
            [NSNumber numberWithBool:YES], SPPrefsUploadSlotsPerHub,
            [NSNumber numberWithInt:1412], SPPrefsPort,
            [NSNumber numberWithInt:0], SPPrefsConnectionMode,
            [NSArray array], SPPrefsSharedPaths,
            [NSArray array], SPPrefsRecentHubs,
            [NSArray arrayWithObjects:
                @"http://www.dc-resources.com/downloads/hublist.config.bz2",
                @"http://www.hublist.org/PublicHubList.config.bz2",
                @"http://www.Freenfo.net/PublicHubList.config.bz2",
                @"http://www.freeweb.hu/pankeey/dc-hubz/pankeey-dchubz.config.bz2",
                @"http://wza.digitalbrains.com/DC/hublist.bz2",
                @"http://dcinfo.sytes.net/publichublist.config.bz2",
                @"http://dreamland.gotdns.org/PublicHubList.config.bz2",
                @"http://gb.hublist.org/PublicHubList.config.bz2",
                @"http://www.dchublist.biz/all_hubs.config.bz2",
                @"http://dcinfo.sytes.net/hungaryhublist.config.bz2",
                nil], SPPrefsHublists,
            @"http://www.dc-resources.com/downloads/hublist.config.bz2", SPPrefsHublistURL,
            [NSNumber numberWithBool:NO], SPPrefsKeepServerRunning,
            @"Message", SPPrefsLogLevel,
            [NSNumber numberWithBool:YES], SPPrefsAutodetectIPAddress,
            [NSNumber numberWithBool:YES], SPPrefsAutoCheckUpdates,
            [NSNumber numberWithBool:YES], SPPrefsShowSmileyIcons,
            [NSNumber numberWithFloat:1.0], SPPrefsRescanShareInterval,
            [NSNumber numberWithBool:YES], SPPrefsFollowHubRedirects,
            [NSNumber numberWithBool:YES], SPPrefsAutoSearchNewSources,
            [NSNumber numberWithUnsignedInt:2], SPPrefsHashingPriority,
            [NSNumber numberWithBool:NO], SPPrefsDrawerIsVisible,
            [NSNumber numberWithInt:100], SPPrefsDrawerHeight,
            [NSNumber numberWithBool:NO], SPPrefsRequireOpenSlots,
            [NSNumber numberWithBool:NO], SPPrefsRequireTTH,
			[NSNumber numberWithBool:YES], SPPrefsSessionRestore,
            nil];
        [[NSUserDefaults standardUserDefaults] registerDefaults:appDefaults];

        sp_log_set_level([[[NSUserDefaults standardUserDefaults] stringForKey:SPPrefsLogLevel] UTF8String]);

        [[NSApplication sharedApplication] setDelegate:self];

        lastSearchID = 1;

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(serverDiedNotification:)
                                                     name:SPNotificationServerDied
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(filelistFinishedNotification:)
                                                     name:SPNotificationFilelistFinished
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(storedFilelistsNotification:)
                                                     name:SPNotificationStoredFilelists
                                                   object:nil];
    }
    
    return self;
}

- (void)registerUrlHandler
{
    [[NSAppleEventManager sharedAppleEventManager]
        setEventHandler:self
            andSelector:@selector(getUrl:withReplyEvent:)
          forEventClass:kInternetEventClass
             andEventID:kAEGetURL];
}

- (void)getUrl:(NSAppleEventDescriptor *)event
withReplyEvent:(NSAppleEventDescriptor *)replyEvent
{
    NSString *urlString = [[event paramDescriptorForKeyword:keyDirectObject] stringValue];
    NSURL *url = [NSURL URLWithString:urlString];
    if (url == nil) {
        NSLog(@"Malformed URL: %@", urlString);
    }
    else {
        NSString *address = nil;
        if ([url port])
            address = [NSString stringWithFormat:@"%@:%@", [url host], [url port]];
        else
            address = [url host];

        [self connectWithAddress:address
                            nick:[url user]
                     description:nil
                        password:[url password]
                        encoding:nil];
    }
}

+ (SPApplicationController *)sharedApplicationController
{
    return mySharedApplicationController;
}

- (BOOL)setupSphubdConnection
{
    NSString *remoteSphubdAddress = nil;
    if ([[NSUserDefaults standardUserDefaults] boolForKey:SPPrefsConnectRemotely] == YES) {
        remoteSphubdAddress = [[NSUserDefaults standardUserDefaults] stringForKey:SPPrefsRemoteSphubdAddress];
    }

    if (remoteSphubdAddress && [remoteSphubdAddress length] > 0) {
        if (sp_connect_remote(sp, [remoteSphubdAddress UTF8String]) != 0) {
            SPLog(@"Failed to connect to remote sphubd");
            /* FIXME: Must return YES here */
            return NO;
        }
    }
    else {
        NSString *launchPath = [NSString stringWithFormat:@"%@/sphubd", [[NSBundle mainBundle] resourcePath]];
        const char *workDir = "~/Library/Application Support/ShakesPeer";

        if (sp_connect(sp, workDir, [launchPath UTF8String]) != 0) {
            NSLog(@"Failed to execute sphubd");
            return NO;
        }
    }

    /* FIXME: this is just silly! We should not use any libevent stuff here! */
    sp->output = evbuffer_new();

    /* Attach the socket to the run loop */
    CFSocketContext spContext;
    spContext.version = 0;
    spContext.info = sp; /* user data passed to the callbacks */
    spContext.retain = nil;
    spContext.release = nil;
    spContext.copyDescription = nil;

    sphubdSocket = CFSocketCreateWithNative(kCFAllocatorDefault, sp->fd,
            kCFSocketReadCallBack | kCFSocketWriteCallBack, sp_callback, &spContext);
    if (sphubdSocket == NULL) {
        NSLog(@"Failed to create a CFSocket");
        return NO;
    }
    sp->user_data = sphubdSocket;
    sphubdRunLoopSource = CFSocketCreateRunLoopSource(kCFAllocatorDefault, sphubdSocket, 0);
    CFRunLoopAddSource(CFRunLoopGetCurrent(), sphubdRunLoopSource, kCFRunLoopDefaultMode);

    /* disable the write callback (only enable if sp_send_string returns EAGAIN, see SPClientBridge.m) */
    CFSocketDisableCallBacks(sphubdSocket, kCFSocketWriteCallBack);

    [self setLogLevel:[[NSUserDefaults standardUserDefaults] stringForKey:SPPrefsLogLevel]];
    sp_send_forget_search(sp, 0);
    sp_send_transfer_stats_interval(sp, 1);
    if ([[NSUserDefaults standardUserDefaults] boolForKey:SPPrefsAutodetectIPAddress] == NO) {
        sp_send_set_ip_address(sp, [[[NSUserDefaults standardUserDefaults] stringForKey:SPPrefsExternalIPAddress] UTF8String]);
    }

    [self setRescanShareInterval:[[NSUserDefaults standardUserDefaults] floatForKey:SPPrefsRescanShareInterval] * 3600];
    [self setFollowHubRedirects:[[NSUserDefaults standardUserDefaults] boolForKey:SPPrefsFollowHubRedirects]];
    [self setAutoSearchNewSources:[[NSUserDefaults standardUserDefaults] boolForKey:SPPrefsAutoSearchNewSources]];
    [self setHashingPriority:[[NSUserDefaults standardUserDefaults] integerForKey:SPPrefsHashingPriority]];
    [self setDownloadFolder:[[NSUserDefaults standardUserDefaults] stringForKey:SPPrefsDownloadFolder]];
    [self setIncompleteFolder:[[NSUserDefaults standardUserDefaults] stringForKey:SPPrefsIncompleteFolder]];

    return YES;
}

- (void)removeSphubdConnection
{
    SPLog(@"removing connection with sphubd");

    CFSocketInvalidate(sphubdSocket);
    CFRelease (sphubdSocket);
    sphubdSocket = NULL;

    CFRunLoopRemoveSource(CFRunLoopGetCurrent(), sphubdRunLoopSource, kCFRunLoopDefaultMode);
    CFRelease(sphubdRunLoopSource);
    sphubdRunLoopSource = NULL;
}

- (void)versionMismatchAlertDidEnd:(NSAlert *)alert returnCode:(int)returnCode
                       contextInfo:(void *)contextInfo
{
    if (returnCode == NSAlertSecondButtonReturn) {
        [[NSApplication sharedApplication] terminate:self];
    }
    else {
        sp_send_shutdown(sp);
        sleep(2);
        [self setupSphubdConnection];
    }
}

- (void)versionMismatch:(NSString *)serverVersion
{
    NSAlert *alert = [[[NSAlert alloc] init] autorelease];
    [alert addButtonWithTitle:@"Restart"];
    [alert addButtonWithTitle:@"Quit"];
    [alert setMessageText:@"Server version mismatch"];
    [alert setInformativeText:
       [NSString stringWithFormat:@"The server is running version %@, but version %s is required",
        serverVersion, VERSION]];
    [alert setAlertStyle:NSCriticalAlertStyle];

    /* Restarting requires us to reset some status, eg deallocate all hubs,
     * since this is not yet implemented, leave it disabled for now
     */
    [[[alert buttons] objectAtIndex:0] setEnabled:NO];

    [alert beginSheetModalForWindow:[mainWindowController window] modalDelegate:self
                     didEndSelector:@selector(versionMismatchAlertDidEnd:returnCode:contextInfo:)
                        contextInfo:nil];
}

- (void)addRecentHub:(NSString *)anAddress
{
    NSMenuItem *hubMenuItem = [menuOpenRecent addItemWithTitle:anAddress
                                                        action:@selector(recentOpenHub:)
                                                 keyEquivalent:@""];
    [hubMenuItem setTarget:self];
}

- (IBAction)connectToBackendServer:(id)sender
{
    if ([self setupSphubdConnection] == NO) {
        SPLog(@"Unable to exec/connect to sphubd!");

        NSAlert *alert = [NSAlert alertWithMessageText:@"Unable to start/connect to backend server"
                                         defaultButton:@"Exit"
                                       alternateButton:@"Show preferences"
                                           otherButton:nil
                             informativeTextWithFormat:@""];
        [alert setAlertStyle:NSCriticalAlertStyle];
        int rc = [alert runModal];
        if (rc == 1) {
            exit(1);
        }
        else if (rc == 0) {
            [self showPreferences:self];
        }
    }
    else {
        mainWindowController = [SPMainWindowController sharedMainWindowController];
        [mainWindowController showWindow:self];

        /* register shared paths with sphubd */
        NSArray *sharedPaths = [[NSUserDefaults standardUserDefaults] arrayForKey:SPPrefsSharedPaths];
        NSEnumerator *e = [sharedPaths objectEnumerator];
        NSString *sharedPath;
        while ((sharedPath = [e nextObject]) != nil) {
            sp_send_add_shared_path(sp, [sharedPath UTF8String]);
        }

        /* add the stored recent hubs to the menu */
        NSArray *recentHubs = [[NSUserDefaults standardUserDefaults] stringArrayForKey:SPPrefsRecentHubs];
        e = [recentHubs objectEnumerator];
        NSString *recentHub;
        while ((recentHub = [e nextObject]) != nil) {
            [self addRecentHub:recentHub];
        }

        [self setSlots:[[NSUserDefaults standardUserDefaults] integerForKey:SPPrefsUploadSlots]
                perHub:[[NSUserDefaults standardUserDefaults] boolForKey:SPPrefsUploadSlotsPerHub]];

        connectedToBackend = YES;
    }
}

- (BOOL)validateMenuItem:(id <NSMenuItem>)menuItem
{
    SEL action = [menuItem action];
    if (action == @selector(connectToBackendServer:))
        return !connectedToBackend;
    else if (action == @selector(showConnectView:))
        return connectedToBackend;
    else if (action == @selector(showAdvancedSearch:))
        return connectedToBackend;
    else if (action == @selector(showBookmarkView:))
        return connectedToBackend;
    else if (action == @selector(showQueueView:))
        return connectedToBackend;
    else if (action == @selector(showTransferView:))
        return connectedToBackend;
    else if (action == @selector(closeCurrentSidebarItem:))
        return connectedToBackend;
    /* else if (menuItem == menuItemRecentHubs) */
        /* return connectedToBackend; */
    return YES;
}

- (void)loadGUInibs
{
    /* create the server messages panel, but don't show it
     * this is needed, 'cause it must subscribe to statusMessage notifications */
    [SPMessagePanel sharedMessagePanel];

    /* [menuItemRecentHubs setEnabled:NO]; */
    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(versionResultReceived:)
                                                 name:@"DTCVMResultNotification"
                                               object:nil];
    
    registerSPTransformers();

    /* Allocate the preference controller, but don't show it. This is needed
     * because sphubd will send messages that the prefs controller is listening
     * to. */
    [SPPreferenceController sharedPreferences];

    [self connectToBackendServer:self];

    NSUserDefaults *standardUserDefaults = [NSUserDefaults standardUserDefaults];
    if ([standardUserDefaults boolForKey:SPPrefsAutoCheckUpdates] == YES) {
        NSDate *previousDate = [standardUserDefaults objectForKey:SPPrefsDateOfPreviousVersionCheck];
        NSDate *twoDaysAgo = [NSDate dateWithTimeIntervalSinceNow:-48.0*60.0*60.0];
        if (previousDate == nil || [previousDate compare:twoDaysAgo] == NSOrderedAscending) {
            automaticUpdate = YES;
            [self checkForUpdates:self];
        }
    }
}

- (void)awakeFromNib
{
    /* set Command-Down Arrow to select next sidebar item */
    unichar down = NSDownArrowFunctionKey;
    [menuItemNextSidebarItem setKeyEquivalent:[NSString stringWithCharacters:&down length:1]];
    [menuItemNextSidebarItem setKeyEquivalentModifierMask:NSCommandKeyMask];

    /* set Command-Up Arrow to select previous sidebar item */
    unichar up = NSUpArrowFunctionKey;
    [menuItemPrevSidebarItem setKeyEquivalent:[NSString stringWithCharacters:&up length:1]];
    [menuItemPrevSidebarItem setKeyEquivalentModifierMask:NSCommandKeyMask];

    [self performSelectorOnMainThread:@selector(loadGUInibs) withObject:nil waitUntilDone:NO];
}

- (void)applicationWillFinishLaunching:(NSNotification *)aNotification
{
    NSLog(@"registering url handler");
    [self registerUrlHandler];
}

#pragma mark -
#pragma mark sphubd command functions

- (void)connectWithAddress:(NSString *)anAddress
                      nick:(NSString *)aNick
               description:(NSString *)aDescription
                  password:(NSString *)aPassword
                  encoding:(NSString *)anEncoding
{
    NSString *nick;
    if (aNick == nil || [aNick length] == 0)
        nick = [[NSUserDefaults standardUserDefaults] stringForKey:SPPrefsNickname];
    else
        nick = aNick;

    NSString *description;
    if (aDescription == nil || [aDescription length] == 0)
        description = [[NSUserDefaults standardUserDefaults] stringForKey:SPPrefsDescription];
    else
        description = aDescription;

    NSString *email = [[NSUserDefaults standardUserDefaults] stringForKey:SPPrefsEmail];
    NSString *speed = [[NSUserDefaults standardUserDefaults] stringForKey:SPPrefsSpeed];
    int passive = [[NSUserDefaults standardUserDefaults] integerForKey:SPPrefsConnectionMode];

    sp_send_connect(sp, [anAddress UTF8String], [nick UTF8String], [email UTF8String],
            [description UTF8String], [speed UTF8String], passive, [aPassword UTF8String], [anEncoding UTF8String]);

    /* add the hub to the recent hubs menu */
    NSArray *recentHubs = [[NSUserDefaults standardUserDefaults] stringArrayForKey:SPPrefsRecentHubs];
    if ([recentHubs indexOfObject:anAddress] == NSNotFound) {
        [self addRecentHub:anAddress];
        [[NSUserDefaults standardUserDefaults] setObject:[recentHubs arrayByAddingObject:anAddress]
                                                  forKey:SPPrefsRecentHubs];
    }
}

- (void)disconnectFromAddress:(NSString *)anAddress
{
    sp_send_disconnect(sp, [anAddress UTF8String]);
}

- (void)sendPublicMessage:(NSString *)aMessage toHub:(NSString *)hubAddress
{
    char *msg = nmdc_escape([aMessage UTF8String]);
    sp_send_public_message(sp, [hubAddress UTF8String], msg);
    free(msg);
}

- (int)searchHub:(NSString *)aHubAddress
       forString:(NSString *)aSearchString
            size:(uint64_t)aSize
 sizeRestriction:(int)aSizeRestriction
        fileType:(int)aFileType
{
    char *search_string = nmdc_escape([aSearchString UTF8String]);
    sp_send_search(sp, [aHubAddress UTF8String], search_string,
            aSize, aSizeRestriction, aFileType, lastSearchID);
    free(search_string);
    return lastSearchID++;
}

- (int)searchAllHubsForString:(NSString *)aSearchString
                          size:(uint64_t)aSize
               sizeRestriction:(int)aSizeRestriction
                      fileType:(int)aFileType
{
    char *search_string = nmdc_escape([aSearchString UTF8String]);
    sp_send_search_all(sp, search_string, aSize, aSizeRestriction, aFileType, lastSearchID);
    free(search_string);
    return lastSearchID++;
}

- (int)searchHub:(NSString *)aHubAddress forTTH:(NSString *)aTTH
{
    sp_send_search(sp, [aHubAddress UTF8String], [[NSString stringWithFormat:@"TTH:%@", aTTH] UTF8String],
            0ULL, SHARE_SIZE_MIN, SHARE_TYPE_TTH, lastSearchID);
    return lastSearchID++;
}

- (int)searchAllHubsForTTH:(NSString *)aTTH
{
    sp_send_search_all(sp, [[NSString stringWithFormat:@"TTH:%@", aTTH] UTF8String], 0ULL, SHARE_SIZE_MIN,
            SHARE_TYPE_TTH, lastSearchID);
    return lastSearchID++;
}

- (void)downloadFile:(NSString *)aFilename withSize:(NSNumber *)aSize
            fromNick:(NSString *)aNick onHub:(NSString *)aHubAddress
         toLocalFile:(NSString *)aLocalFilename
                 TTH:(NSString *)aTTH
{
    sp_send_download_file(sp,
            [aHubAddress UTF8String], [aNick UTF8String], [aFilename UTF8String],
            [aSize unsignedLongLongValue], [aLocalFilename UTF8String], [aTTH UTF8String]);
}

- (void)downloadDirectory:(NSString *)aDirectory
            fromNick:(NSString *)aNick
               onHub:(NSString *)aHubAddress
         toLocalDirectory:(NSString *)aLocalDirectory;
{
    sp_send_download_directory(sp,
            [aHubAddress UTF8String], [aNick UTF8String], [aDirectory UTF8String],
            [aLocalDirectory UTF8String]);
}

- (void)downloadFilelistFromUser:(NSString *)aNick onHub:(NSString *)aHubAddress
                     forceUpdate:(BOOL)forceUpdateFlag autoMatch:(BOOL)autoMatchFlag
{
    sp_send_download_filelist(sp, [aHubAddress UTF8String], [aNick UTF8String], forceUpdateFlag, autoMatchFlag);
}

- (void)removeSource:(NSString *)targetFilename nick:(NSString *)aNick
{
    sp_send_queue_remove_source(sp, [targetFilename UTF8String], [aNick UTF8String]);
}

- (void)removeQueue:(NSString *)targetFilename
{
    sp_send_queue_remove_target(sp, [targetFilename UTF8String]);
}

- (void)removeDirectory:(NSString *)targetDirectory
{
    sp_send_queue_remove_directory(sp, [targetDirectory UTF8String]);
}

- (void)removeFilelistForNick:(NSString *)aNick
{
    sp_send_queue_remove_filelist(sp, [aNick UTF8String]);
}

- (void)removeAllSourcesWithNick:(NSString *)aNick
{
    sp_send_queue_remove_nick(sp, [aNick UTF8String]);
}

- (void)addSharedPath:(NSString *)aPath;
{
    sp_send_add_shared_path(sp, [aPath UTF8String]);
}

- (void)removeSharedPath:(NSString *)aPath
{
    sp_send_remove_shared_path(sp, [aPath UTF8String]);
}

- (void)setPort:(int)aPort
{
    sp_send_set_port(sp, aPort);
}

- (void)setIPAddress:(NSString *)anIPAddress
{
    if (anIPAddress && [anIPAddress length] > 0)
        sp_send_set_ip_address(sp, [anIPAddress UTF8String]);
    else
        sp_send_set_ip_address(sp, "auto-detect");
}

- (void)setPassword:(NSString *)aPassword forHub:(NSString *)aHubAddress
{
    sp_send_set_password(sp, [aHubAddress UTF8String], [aPassword UTF8String]);
}

- (void)updateUserEmail:(NSString *)email description:(NSString *)description speed:(NSString *)speed
{
    sp_send_update_user_info(sp, [speed UTF8String], [description UTF8String], [email UTF8String]);
}

- (void)sendPrivateMessage:(NSString *)theMessage toNick:(NSString *)theNick hub:(NSString *)hubAddress
{
    char *msg = nmdc_escape([theMessage UTF8String]);
    sp_send_private_message(sp, [hubAddress UTF8String], [theNick UTF8String], msg);
    free(msg);
}

- (void)setSlots:(int)slots perHub:(BOOL)perHubFlag
{
    sp_send_set_slots(sp, slots, perHubFlag);
}

- (void)setPassiveMode
{
    sp_send_set_passive(sp, 1);
}

- (void)forgetSearch:(int)searchID
{
    sp_send_forget_search(sp, searchID);
}

- (void)cancelTransfer:(NSString *)targetFilename
{
    sp_send_cancel_transfer(sp, [targetFilename UTF8String]);
}

- (void)sendRawCommand:(NSString *)rawCommand toHub:(NSString *)hubAddress
{
    char *cmd = nmdc_escape([rawCommand UTF8String]);
    sp_send_raw_command(sp, [hubAddress UTF8String], cmd);
    free(cmd);
}

- (void)setLogLevel:(NSString *)aLogLevel
{
    sp_send_log_level(sp, [aLogLevel UTF8String]);
}

- (void)setPriority:(unsigned int)priority onTarget:(NSString *)targetFilename
{
    sp_send_set_priority(sp, [targetFilename UTF8String], priority);
}

- (void)setRescanShareInterval:(unsigned int)rescanShareInterval
{
    sp_send_rescan_share_interval(sp, rescanShareInterval);
}

- (void)setFollowHubRedirects:(BOOL)followFlag
{
    sp_send_set_follow_redirects(sp, followFlag ? 1 : 0);
}

- (void)setAutoSearchNewSources:(BOOL)autoSearchFlag
{
    sp_send_set_auto_search(sp, autoSearchFlag ? 1 : 0);
}

- (void)grantExtraSlotToNick:(NSString *)nick
{
    sp_send_grant_slot(sp, [nick UTF8String]);
}

- (void)pauseHashing
{
    sp_send_pause_hashing(sp);
}

- (void)resumeHashing
{
    sp_send_resume_hashing(sp);
}

- (void)setHashingPriority:(unsigned int)prio
{
    sp_send_set_hash_prio(sp, prio);
}

- (void)setDownloadFolder:(NSString *)downloadFolder
{
    sp_send_set_download_directory(sp, [downloadFolder UTF8String]);
}

- (void)setIncompleteFolder:(NSString *)incompleteFolder
{
    sp_send_set_incomplete_directory(sp, [incompleteFolder UTF8String]);
}

#pragma mark -
#pragma mark Interface actions

- (IBAction)showPreferences:(id)sender
{
    [[SPPreferenceController sharedPreferences] show];
}

- (void)recentOpenHub:(id)sender
{
    [self connectWithAddress:[sender title] nick:nil description:nil password:nil encoding:nil];
}

- (IBAction)openSPWebpage:(id)sender
{
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://shakespeer.bzero.se/"]];
}

- (IBAction)openSPForums:(id)sender
{
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://shakespeer.bzero.se/forum/"]];
}

- (IBAction)reportBug:(id)sender
{
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://trac.bzero.se/trac/shakespeer/newticket"]];
}

- (IBAction)donate:(id)sender
{
    [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:@"http://shakespeer.bzero.se/donate.html"]];
}

- (IBAction)showConnectView:(id)sender
{
    [mainWindowController connectShow:sender];
}

- (IBAction)showPublicHubsView:(id)sender
{
    [mainWindowController openHublist:self];
}

- (IBAction)showBookmarksView:(id)sender
{
    [mainWindowController openBookmarks:self];
}

- (IBAction)showDownloadsView:(id)sender
{
    [mainWindowController openDownloads:self];
}

- (IBAction)showUploadsView:(id)sender
{
    [mainWindowController openUploads:self];
}

- (IBAction)closeCurrentSidebarItem:(id)sender
{
    if ([[SPPreferenceController sharedPreferences] isKeyWindow]) {
        [[SPPreferenceController sharedPreferences] close];
    }
    else if ([[SPMessagePanel sharedMessagePanel] isKeyWindow]) {
        [[SPMessagePanel sharedMessagePanel] close];
    }
    else {
        [mainWindowController closeCurrentSidebarItem];
    }
}

- (IBAction)showAdvancedSearch:(id)sender
{
    [mainWindowController advSearchShow:sender];
}

- (IBAction)prevSidebarItem:(id)sender
{
    [mainWindowController prevSidebarItem];
}

- (IBAction)nextSidebarItem:(id)sender
{
    [mainWindowController nextSidebarItem];
}

- (IBAction)showServerMessages:(id)sender
{
    SPMessagePanel *mp = [SPMessagePanel sharedMessagePanel];
    [mp show];
}

- (IBAction)rescanSharedFolders:(id)sender
{
    [[SPPreferenceController sharedPreferences] updateAllSharedPaths];
}

#pragma mark -
#pragma mark NSApplication delegate methods

- (void)applicationWillTerminate:(NSNotification *)aNotification
{
    SPLog(@"shutting down application");
    if ([[NSUserDefaults standardUserDefaults] boolForKey:SPPrefsKeepServerRunning] == NO) {
        /* read in pid from pidfile */
        sp_send_shutdown(sp);
        /* 'kill -0 pid' until sphubd is dead, with timeout */
    }
	
	if ([[NSUserDefaults standardUserDefaults] boolForKey:SPPrefsSessionRestore] == NO) {
		// if the pref for autconnecting to last connected hubs is off, make sure 
		// we clear any currently cached list on shutdown.
		[[NSUserDefaults standardUserDefaults] setObject:[NSArray array] forKey:SPPrefsLastConnectedHubs];
	}
}

#pragma mark -
#pragma mark Automatic updates

- (IBAction)downloadUpdate:(id)sender
{
    [NSApp stopModalWithCode:0];
}

- (IBAction)cancelUpdate:(id)sender
{
    [NSApp stopModalWithCode:1];
}

- (BOOL)isNewerVersion:(NSString *)lastVersion than:(NSString *)currentVersion
{
    if ([lastVersion isEqualToString:currentVersion])
        return FALSE;

    NSArray *lastVersionArray = [lastVersion componentsSeparatedByString:@"."];
    NSArray *currentVersionArray = [currentVersion componentsSeparatedByString:@"."];

    unsigned int i;
    int maxc = [lastVersionArray count];
    if ([currentVersionArray count] > maxc)
        maxc = [currentVersionArray count];

    for (i = 0; i < maxc; i++) {
        int l = (i >= [lastVersionArray count]) ?  0 : [[lastVersionArray objectAtIndex:i] intValue];
        int c = (i >= [currentVersionArray count]) ?  0 : [[currentVersionArray objectAtIndex:i] intValue];
        if (l > c)
            return TRUE;
        else if (c > l)
            return FALSE;
    }

    return FALSE;
}

- (void)compareVersions:(NSDictionary *)versionDict
{
    NSString *currentVersion = [[[NSBundle mainBundle] infoDictionary] objectForKey:@"CFBundleVersion"];
    NSString *lastVersion = [versionDict objectForKey:@"versionString"];
    NSDate *releaseDate = [versionDict objectForKey:@"releaseDate"];
    NSArray *features = [versionDict objectForKey:@"features"];
    NSString *downloadLink = [versionDict objectForKey:@"downloadLink"];

    NSLog(@"currentVersion = %@, lastVersion = %@", currentVersion, lastVersion);

    BOOL newVersion = [self isNewerVersion:lastVersion than:currentVersion];
    BOOL displayNotice = !automaticUpdate || newVersion;

    [[NSUserDefaults standardUserDefaults] setObject:[NSDate dateWithTimeIntervalSinceNow:0.0] forKey:SPPrefsDateOfPreviousVersionCheck];

    if (displayNotice) {
        if (newVersion) {
            [downloadButton setTitle:@"Download..."];
            [cancelButton setEnabled:YES];
            [updateTitleField setStringValue:@"There is a new version of ShakesPeer available."];
        }
        else {
            [downloadButton setTitle:@"OK"];
            [cancelButton setEnabled:NO];
            [updateTitleField setStringValue:@"You're running the latest version of ShakesPeer."];
        }

        [[detailsView textStorage] setAttributedString:[[[NSMutableAttributedString alloc] initWithString:@""] autorelease]];
        NSEnumerator *e = [features objectEnumerator];
        NSString *feat;
        while ((feat = [e nextObject]) != nil) {
            NSAttributedString *attrString = [[NSAttributedString alloc] initWithString:[NSString stringWithFormat:@"- %@\n", feat]];
            [[detailsView textStorage] appendAttributedString:[attrString autorelease]];
        }

        [releaseDateField setStringValue:[releaseDate descriptionWithCalendarFormat:@"%Y-%m-%d" timeZone:nil locale:nil]];
        [currentVersionField setStringValue:currentVersion];
        [lastVersionField setStringValue:lastVersion];

        [NSApp beginSheet:updateWindow
           modalForWindow:[[SPMainWindowController sharedMainWindowController] window]
            modalDelegate:nil
           didEndSelector:nil
              contextInfo:nil];

        int rc = [NSApp runModalForWindow:updateWindow];

        [NSApp endSheet:updateWindow];
        [updateWindow orderOut:self];

        if (rc == 0 && newVersion) {
            NSLog(@"download update");
            [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString:downloadLink]];
        }
    }
}

- (void)getVersion:(id)args
{
    NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

    NSURL *versionURL = [NSURL URLWithString:@"http://shakespeer.sourceforge.net/sp.plist"];
    BOOL reachable = NO;
    SCNetworkConnectionFlags flags;
    BOOL success = SCNetworkCheckReachabilityByName([[versionURL host] UTF8String], &flags);
    if (success && (flags & kSCNetworkFlagsReachable) && !(flags & kSCNetworkFlagsConnectionRequired)) {
        reachable = YES;
    }

    if (reachable) {
        NSDictionary *versionDict = [NSDictionary dictionaryWithContentsOfURL:versionURL];

        if (versionDict)   // check for valid/complete dictionary
        {
            [self performSelectorOnMainThread:@selector(compareVersions:) withObject:versionDict waitUntilDone:YES];
        }
        else {
            NSLog(@"Failed to get version dictionary");
        }
    }
    else {
        NSLog(@"host %@ not reachable", [versionURL host]);
    }
    automaticUpdate = NO;
    [pool release];
}

- (IBAction)checkForUpdates:(id)sender
{
    [NSThread detachNewThreadSelector:@selector(getVersion:) toTarget:self withObject:nil];
}

#pragma mark -
#pragma mark Notifications

- (void)serverDiedAlertDidEnd:(NSAlert *)alert returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
    if (returnCode == NSAlertSecondButtonReturn) {
        [[NSApplication sharedApplication] terminate:self];
    }
    else {
        [self setupSphubdConnection];
    }
}

- (void)serverDiedNotification:(NSNotification *)aNotification
{
    SPLog(@"server died!");

    NSAlert *alert = [[[NSAlert alloc] init] autorelease];
    [alert addButtonWithTitle:@"Restart"];
    [alert addButtonWithTitle:@"Quit"];
    [alert setMessageText:@"Server has unexpectedly died"];
    [alert setInformativeText:@"You've found a bug ;-)"];
    [alert setAlertStyle:NSCriticalAlertStyle];

    /* Restarting requires us to reset some status, eg deallocate all hubs,
     * since this is not yet implemented, leave it disabled for now
     */
    [[[alert buttons] objectAtIndex:0] setEnabled:NO];

    [alert beginSheetModalForWindow:[mainWindowController window] modalDelegate:self
                     didEndSelector:@selector(serverDiedAlertDidEnd:returnCode:contextInfo:)
                        contextInfo:nil];
}

- (void)storedFilelistsNotification:(NSNotification *)aNotification
{
    NSArray *nicks = [[aNotification userInfo] objectForKey:@"nicks"];

    NSEnumerator *e = [nicks objectEnumerator];
    NSString *nick;
    while ((nick = [e nextObject]) != nil) {
        [self addRecentFilelist:nick];
    }
}

- (void)openRecentFilelist:(id)sender
{
    NSString *nick = [sender representedObject];

    /* if this is truly a recent filelist, it won't actually be downloaded by
     * sphubd */
    [self downloadFilelistFromUser:nick onHub:nil forceUpdate:NO autoMatch:NO];
}

- (void)addRecentFilelist:(NSString *)nick
{
    NSMenuItem *browseMenuItem = [menuFilelists itemWithTitle:nick];
    if (browseMenuItem == nil) {
        browseMenuItem = (NSMenuItem *)[menuFilelists addItemWithTitle:nick
                                                                action:@selector(openRecentFilelist:)
                                                         keyEquivalent:@""];
    }
    [browseMenuItem setTarget:self];
    [browseMenuItem setRepresentedObject:nick];
}

- (void)filelistFinishedNotification:(NSNotification *)aNotification
{
    NSString *nick = [[aNotification userInfo] objectForKey:@"nick"];
    [self addRecentFilelist:nick];
}

@end

