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

#import <Cocoa/Cocoa.h>

@interface SPPreferenceController : NSWindowController
{
    IBOutlet NSView *identityView;
    IBOutlet NSView *networkView;
    IBOutlet NSView *sharesView;
    IBOutlet NSView *advancedView;

    IBOutlet NSTextField *downloadFolder;
    IBOutlet NSTextField *completeFolder;
    IBOutlet NSTextField *emailField;
    IBOutlet NSTextField *descriptionField;
    IBOutlet NSComboBox *speedField;
    IBOutlet NSTextField *testResults;
    IBOutlet NSProgressIndicator *testConnectionProgress;
    IBOutlet NSTextField *portField;
    IBOutlet NSButton *testPortButton;
    IBOutlet NSTextField *IPAddressField;
    IBOutlet NSButton *slotsPerHubButton;
    IBOutlet NSTextField *slotsField;

    IBOutlet NSProgressIndicator *updateSharedPathsIndicator;
    IBOutlet NSArrayController *sharedPathsController;

    IBOutlet NSTextField *rescanShareField;

    NSView *blankView;
    NSToolbar *prefsToolbar;

    NSMutableArray *sharedPaths;

    unsigned long long totalShareSize;
    BOOL testInProgress;
    BOOL hashingPaused;
}
+ (SPPreferenceController *)sharedPreferences;

- (void)setTotalShareSize:(unsigned long long)aNumber;

- (IBAction)addSharedPath:(id)sender;
- (IBAction)removeSharedPath:(id)sender;
- (IBAction)updateSharedPaths:(id)sender;
- (IBAction)selectDownloadFolder:(id)sender;
- (IBAction)selectCompleteFolder:(id)sender;
- (IBAction)setPort:(id)sender;
- (IBAction)setIPAddress:(id)sender;
- (IBAction)updateUserInfo:(id)sender;
- (IBAction)testConnection:(id)sender;
- (IBAction)setSlots:(id)sender;
- (IBAction)setConnectionMode:(id)sender;
- (IBAction)setLogLevel:(id)sender;
- (IBAction)setRescanShareInterval:(id)sender;
- (IBAction)setFollowRedirects:(id)sender;
- (IBAction)togglePauseHashing:(id)sender;
- (IBAction)setAutoSearchNewSources:(id)sender;
- (IBAction)setHashingPriority:(id)sender;

- (void)show;
- (void)close;
- (BOOL)isKeyWindow;
- (void)addSharedPathsPath:(NSString *)aPath;
- (void)updateAllSharedPaths;

@end

