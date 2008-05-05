/* vim: ft=objc fdm=indent foldnestmax=1
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

#import <Cocoa/Cocoa.h>

#include <spclient.h>

void sendNotification(NSString *notificationName, NSString *key1, id arg1, ...);

@class SPMainWindowController;

@interface SPApplicationController : NSObject
{
    BOOL connectedToBackend;
    SPMainWindowController *mainWindowController;
    NSDictionary *dispatchTable;
    NSData *socketEOL;
    int lastTag;
    sp_t *sp;
    CFSocketRef sphubdSocket;
    CFRunLoopSourceRef sphubdRunLoopSource;

    IBOutlet NSMenu *menuOpenRecent;
    IBOutlet NSMenu *menuFilelists;

    IBOutlet NSMenuItem *menuItemNextSidebarItem;
    IBOutlet NSMenuItem *menuItemPrevSidebarItem;

    BOOL automaticUpdate;
    IBOutlet NSWindow *updateWindow;
    IBOutlet NSTextField *currentVersionField;
    IBOutlet NSTextField *lastVersionField;
    IBOutlet NSTextField *releaseDateField;
    IBOutlet NSTextView *detailsView;
    IBOutlet NSTextField *updateTitleField;
    IBOutlet NSButton *downloadButton;
    IBOutlet NSButton *cancelButton;
}

- (IBAction)connectToBackendServer:(id)sender;
- (IBAction)showPreferences:(id)sender;
- (IBAction)openSPWebpage:(id)sender;
- (IBAction)openSPForums:(id)sender;
- (IBAction)reportBug:(id)sender;
- (IBAction)donate:(id)sender;
- (IBAction)showConnectView:(id)sender;
- (IBAction)showQueueView:(id)sender;
- (IBAction)showBookmarkView:(id)sender;
- (IBAction)showTransferView:(id)sender;
- (IBAction)closeCurrentSidebarItem:(id)sender;
- (IBAction)showAdvancedSearch:(id)sender;
- (IBAction)checkForUpdates:(id)sender;
- (IBAction)downloadUpdate:(id)sender;
- (IBAction)cancelUpdate:(id)sender;
- (IBAction)rescanSharedFolders:(id)sender;

- (IBAction)prevSidebarItem:(id)sender;
- (IBAction)nextSidebarItem:(id)sender;
- (IBAction)showServerMessages:(id)sender;

+ (SPApplicationController *)sharedApplicationController;
- (void)connectWithAddress:(NSString *)anAddress
                      nick:(NSString *)aNick
               description:(NSString *)aDescription
                  password:(NSString *)aPassword
                  encoding:(NSString *)anEncoding;
- (void)disconnectFromAddress:(NSString *)anAddress;
- (void)sendPublicMessage:(NSString *)aMessage toHub:(NSString *)hubAddress;
- (int)searchHub:(NSString *)aHubAddress
       forString:(NSString *)aSearchString
            size:(unsigned long long)aSize
 sizeRestriction:(int)aSizeRestriction
        fileType:(int)aFileType;
- (int)searchAllHubsForString:(NSString *)aSearchString
                          size:(unsigned long long)aSize
               sizeRestriction:(int)aSizeRestriction
                      fileType:(int)aFileType;
- (int)searchHub:(NSString *)aHubAddress forTTH:(NSString *)aTTH;
- (int)searchAllHubsForTTH:(NSString *)aTTH;
- (void)downloadFile:(NSString *)aFilename withSize:(NSNumber *)aSize
            fromNick:(NSString *)aNick onHub:(NSString *)aHubAddress
         toLocalFile:(NSString *)aLocalFilename
                 TTH:(NSString *)aTTH;
- (void)downloadDirectory:(NSString *)aDirectory
            fromNick:(NSString *)aNick onHub:(NSString *)aHubAddress
         toLocalDirectory:(NSString *)aLocalFilename;
- (void)downloadFilelistFromUser:(NSString *)aNick onHub:(NSString *)aHubAddress
                     forceUpdate:(BOOL)forceUpdateFlag autoMatch:(BOOL)autoMatchFlag;
- (void)removeSource:(NSString *)targetFilename nick:(NSString *)aNick;
- (void)removeQueue:(NSString *)targetFilename;
- (void)removeDirectory:(NSString *)targetDirectory;
- (void)removeFilelistForNick:(NSString *)aNick;
- (void)removeAllSourcesWithNick:(NSString *)aNick;
- (void)addSharedPath:(NSString *)aPath;
- (void)removeSharedPath:(NSString *)aPath;
- (void)cancelTransfer:(NSString *)targetFilename;
- (void)recentOpenHub:(id)sender;
- (void)setPort:(int)aPort;
- (void)setIPAddress:(NSString *)anIPAddress;
- (void)setPassword:(NSString *)aPassword forHub:(NSString *)aHubAddress;
- (void)updateUserEmail:(NSString *)email description:(NSString *)description speed:(NSString *)speed;
- (void)sendPrivateMessage:(NSString *)theMessage toNick:(NSString *)theNick hub:(NSString *)hubAddress;
- (void)sendRawCommand:(NSString *)rawCommand toHub:(NSString *)hubAddress;
- (void)setSlots:(int)slots perHub:(BOOL)perHubFlag;
- (void)setPassiveMode;
- (void)forgetSearch:(int)searchID;
- (void)versionMismatch:(NSString *)serverVersion;
- (void)setLogLevel:(NSString *)aLogLevel;
- (void)setPriority:(unsigned int)priority onTarget:(NSString *)targetFilename;
- (void)setRescanShareInterval:(unsigned int)rescanShareInterval;
- (void)setFollowHubRedirects:(BOOL)followFlag;
- (void)setAutoSearchNewSources:(BOOL)autoSearchFlag;
- (void)addRecentFilelist:(NSString *)nick;
- (void)grantExtraSlotToNick:(NSString *)nick;
- (void)pauseHashing;
- (void)resumeHashing;
- (void)setHashingPriority:(unsigned int)prio;
- (void)setDownloadFolder:(NSString *)downloadFolder;
- (void)setIncompleteFolder:(NSString *)incompleteFolder;

- (BOOL)setupSphubdConnection;
- (void)removeSphubdConnection;

@end

