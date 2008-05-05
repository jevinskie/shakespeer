#import <Cocoa/Cocoa.h>
#import "SPSideBar.h"

@interface SPFriendsController : NSObject <SPSideBarItem>
{
	NSMutableArray *friends;

	IBOutlet NSArrayController *friendsController;
    IBOutlet NSView *friendsView;
    IBOutlet NSTableView *friendsTable;
	IBOutlet NSMenu *friendMenu;

	IBOutlet NSWindow *newFriendSheet;
    IBOutlet NSTextField *newFriendNameField;
    IBOutlet NSTextField *newFriendCommentsField;

    IBOutlet NSWindow *editFriendSheet;
    IBOutlet NSTextField *editFriendNameField;
    IBOutlet NSTextField *editFriendCommentsField;

	NSTimer *updateTimer;
}

+ (SPFriendsController *)sharedFriendsController;

- (void)setFriends:(NSArray *)newFriends;
- (void)addFriendWithName:(NSString *)name comments:(NSString *)comments;
- (void)updateFriendTable;

- (IBAction)browseUser:(id)sender;
- (IBAction)sendPrivateMessage:(id)sender;

- (void)newFriendSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo;
- (IBAction)newFriendShow:(id)sender;
- (IBAction)newFriendExecute:(id)sender;
- (IBAction)newFriendCancel:(id)sender;

- (void)editFriendSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo;
- (IBAction)editFriendShow:(id)sender;
- (IBAction)editFriendExecute:(id)sender;
- (IBAction)editFriendCancel:(id)sender;

- (void)removeFriendShow:(id)sender;
- (void)removeFriendSheetDidEnd:(NSAlert *)alert returnCode:(int)returnCode contextInfo:(void *)contextInfo;

@end
