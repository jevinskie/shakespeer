#import "SPFriendsController.h"
#import "SPMainWindowController.h"
#import "SPApplicationController.h"
#import "SPNotificationNames.h"
#import "SPUserDefaultKeys.h"
#import "SPHubController.h"
#import "SPUser.h"

@implementation SPFriendsController

#pragma mark Singleton implementation

// TODO: this is the recommended singleton implementation code from Apple:
// http://developer.apple.com/documentation/Cocoa/Conceptual/CocoaFundamentals/CocoaObjects/chapter_3_section_10.html
// it should be implemented for all other static sidebar items too

static SPFriendsController *sharedFriendsController = nil;

+ (SPFriendsController *)sharedFriendsController
{
	@synchronized(self) {
        if (sharedFriendsController == nil) {
            [[self alloc] init]; // assignment not done here
        }
    }

    return sharedFriendsController;
}

+ (id)allocWithZone:(NSZone *)zone
{
    @synchronized(self) {
        if (sharedFriendsController == nil) {
            sharedFriendsController = [super allocWithZone:zone];

            return sharedFriendsController; // assignment and return on first allocation
        }
    }

    return nil; // on subsequent allocation attempts return nil
}

- (id)copyWithZone:(NSZone *)zone
{
    return self;
}

- (id)retain
{
    return self;
}

- (unsigned)retainCount

{
    return UINT_MAX; // denotes an object that cannot be released
}

- (void)release

{
    // do nothing
}

- (id)autorelease
{
    return self;
}

#pragma mark -
#pragma mark Initialization and deallocation

- (id)init
{
	if ((self = [super init])) {
		[NSBundle loadNibNamed:@"Friends" owner:self];

		// load our set of friends from preferences
		NSArray *defaultFriends = [[NSUserDefaults standardUserDefaults] arrayForKey:SPFriends];
        [self setFriends:defaultFriends];
	}

	return self;
}

- (void)awakeFromNib
{
	[friendsTable setTarget:self];
    [friendsTable setDoubleAction:@selector(editFriendShow:)];
	[friendsTable setMenu:friendMenu];

	// sort by online status by default
	NSSortDescriptor *defaultSortDescriptor = [[[NSSortDescriptor alloc] initWithKey:@"isOnline"
																		   ascending:NO
																			selector:@selector(compare:)] autorelease];
	[friendsController setSortDescriptors:[NSArray arrayWithObject:defaultSortDescriptor]];
}

- (void)dealloc
{
	[updateTimer invalidate];
  [updateTimer release];
  updateTimer = nil;

  [super dealloc];
}

#pragma mark -
#pragma mark Sidebar support

- (NSView *)view
{
    return friendsView;
}

- (NSString *)title
{
    return @"Friends";
}

- (NSImage *)image
{
    return [NSImage imageNamed:@"user"];
}

- (NSMenu *)menu
{
    return nil;
}

#pragma mark -

- (void)setFriends:(NSArray *)newFriends
{
    if (newFriends != friends) {
		[friends release];
		friends = [[NSMutableArray alloc] initWithArray:newFriends];

		[[NSUserDefaults standardUserDefaults] setObject:friends forKey:SPFriends];
	}
}

// TODO: Instead of using plain NSDictionary entries, we should use a
// friend class SPFriend, inheriting from NSMutableDictionary

- (void)addFriendWithName:(NSString *)name comments:(NSString *)comments
{
    NSMutableDictionary *newFriend = [NSMutableDictionary dictionary];
    [newFriend setObject:name forKey:@"name"];
    [newFriend setObject:comments forKey:@"comments"];
	[newFriend setObject:[NSNumber numberWithBool:NO] forKey:@"isOnline"];
	[newFriend setObject:[NSDate distantFuture] forKey:@"lastSeen"]; // set to future if never seen
	[newFriend setObject:[NSNumber numberWithFloat:0.0] forKey:@"shareSize"];

    [self setFriends:[friends arrayByAddingObject:newFriend]];

    [[NSUserDefaults standardUserDefaults] setObject:friends forKey:SPFriends];
}

- (void)updateFriendTable
{
	// let's see if any friends are online
	NSDictionary *connectedHubs = [[SPMainWindowController sharedMainWindowController] connectedHubs];
	NSEnumerator *friendEnumerator = [friends objectEnumerator];
	id currentFriend = nil;
	while ((currentFriend = [friendEnumerator nextObject])) {
		BOOL didFindFriend = NO;
		NSEnumerator *hubEnumerator = [connectedHubs objectEnumerator];
		id currentHub = nil;
		while ((currentHub = [hubEnumerator nextObject])) {
			SPUser *foundUser = [currentHub findUserWithNick:[currentFriend objectForKey:@"name"]];
			if (foundUser) {
				[currentFriend setValue:[NSNumber numberWithBool:YES] forKey:@"isOnline"];
				[currentFriend setValue:[NSDate date] forKey:@"lastSeen"];
				[currentFriend setValue:[NSNumber numberWithUnsignedLongLong:[foundUser size]] forKey:@"shareSize"];
				[currentFriend setValue:[currentHub address] forKey:@"hub"];
				[currentFriend setValue:[currentHub nick] forKey:@"myName"];
				didFindFriend = YES;

				break; // the friend was found so we can check the next one
			}
		}

		// the user wasn't online, so mark as offline
		if (!didFindFriend)
			[currentFriend setValue:[NSNumber numberWithBool:NO] forKey:@"isOnline"];
	}
  
	[[NSUserDefaults standardUserDefaults] setObject:friends forKey:SPFriends];
	[friendsController setSortDescriptors:[friendsController sortDescriptors]];
}

#pragma mark -
#pragma mark Interface actions

- (IBAction)browseUser:(id)sender
{
	NSString *remoteUsername = [[sender representedObject] valueForKey:@"name"];
	NSString *hubAddress = [[sender representedObject] valueForKey:@"hub"];

	[[SPApplicationController sharedApplicationController] downloadFilelistFromUser:remoteUsername
																			  onHub:hubAddress
																		forceUpdate:NO
																		  autoMatch:NO];
}

- (IBAction)sendPrivateMessage:(id)sender
{
	NSString *myUsername = [[sender representedObject] valueForKey:@"myName"];
	NSString *remoteUsername = [[sender representedObject] valueForKey:@"name"];
	NSString *hubAddress = [[sender representedObject] valueForKey:@"hub"];

	sendNotification(SPNotificationStartChat,
					 @"remote_nick", remoteUsername,
					 @"hubAddress", hubAddress,
					 @"my_nick", myUsername,
					 nil);
}

#pragma mark -
#pragma mark Menu validation

- (BOOL)validateMenuItem:(NSMenuItem *)menuItem
{
	int clickedRow = [friendsTable clickedRow];

	if (clickedRow == -1) // the user clicked the table background
		return NO;

	NSDictionary *clickedObject = [[friendsController arrangedObjects] objectAtIndex:clickedRow];
	[menuItem setRepresentedObject:clickedObject];

	// TODO: Maybe we should be able to browse the share even when the user is offline
	return [[clickedObject valueForKey:@"isOnline"] boolValue];
}

#pragma mark -
#pragma mark Delegate methods

// this is called by the main window controller when we are selected
- (void)viewBecameSelected
{
	// first, update the table immediately
	[self updateFriendTable]; 

  if (!updateTimer)
    // then update the table every other second
    updateTimer = [[NSTimer scheduledTimerWithTimeInterval:1.0
                            target:self
                          selector:@selector(updateFriendTable)
                          userInfo:nil
                           repeats:YES] retain];
}

// this is called by the main window controller when we are deselected
- (void)viewBecameDeselected
{
	[updateTimer invalidate];
  [updateTimer release];
  updateTimer = nil;
}

#pragma mark -
#pragma mark New friend sheet

- (void)newFriendSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
    [sheet orderOut:self];
}

- (IBAction)newFriendShow:(id)sender
{
    // everything is empty by default
	[newFriendNameField setStringValue:@""];
    [newFriendCommentsField setStringValue:@""];

    // select the name field
    [newFriendNameField selectText:self];

    // launch sheet
    [NSApp beginSheet:newFriendSheet
       modalForWindow:[[SPMainWindowController sharedMainWindowController] window]
        modalDelegate:self
       didEndSelector:@selector(newFriendSheetDidEnd:returnCode:contextInfo:)
          contextInfo:nil];
}

- (IBAction)newFriendExecute:(id)sender
{
    // the user must fill in a name
    if ([[newFriendNameField stringValue] length] == 0) {
        NSBeep();
        [newFriendNameField selectText:self];

        return;
    }

    [NSApp endSheet:newFriendSheet];

    [self addFriendWithName:[newFriendNameField stringValue] comments:[newFriendCommentsField stringValue]];
}

- (IBAction)newFriendCancel:(id)sender
{
    [NSApp endSheet:newFriendSheet];
}

#pragma mark -
#pragma mark Edit friend sheet

// TODO: Merge the new/edit sheets, lots of redundancy here

- (void)editFriendSheetDidEnd:(NSWindow *)sheet returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
    [sheet orderOut:self];
}

- (IBAction)editFriendShow:(id)sender
{
    // read values from the selected friend
    NSArray *selectedObjects = [friendsController selectedObjects];
    if ([selectedObjects count]) {
        NSDictionary *friend = [selectedObjects objectAtIndex:0];

        [editFriendNameField setStringValue:[friend objectForKey:@"name"]];
        [editFriendCommentsField setStringValue:[friend objectForKey:@"comments"]];

        // select the name field
        [editFriendNameField selectText:self];

        // launch sheet
        [NSApp beginSheet:editFriendSheet
           modalForWindow:[[SPMainWindowController sharedMainWindowController] window]
            modalDelegate:self
           didEndSelector:@selector(editFriendSheetDidEnd:returnCode:contextInfo:)
              contextInfo:nil];
    }
}

- (IBAction)editFriendExecute:(id)sender
{
    // the user must fill in a name
    if ([[editFriendNameField stringValue] length] == 0) {
        NSBeep();
        [editFriendNameField selectText:self];

        return;
    }

    [NSApp endSheet:editFriendSheet];

    NSArray *selectedObjects = [friendsController selectedObjects];
    if ([selectedObjects count]) {
        NSMutableDictionary *friend = [selectedObjects objectAtIndex:0];
        [friend setObject:[editFriendNameField stringValue] forKey:@"name"];
        [friend setObject:[editFriendCommentsField stringValue] forKey:@"comments"];

        [friends replaceObjectAtIndex:[friendsTable selectedRow] withObject:friend];

        [[NSUserDefaults standardUserDefaults] setObject:friends forKey:SPFriends];
    }
}

- (IBAction)editFriendCancel:(id)sender
{
    [NSApp endSheet:editFriendSheet];
}

#pragma mark -
#pragma mark Remove friend sheet

- (void)removeFriendShow:(id)sender
{
    NSAlert *alert = [[NSAlert alloc] init];

    [alert addButtonWithTitle:@"OK"];
    [alert addButtonWithTitle:@"Cancel"];
    [alert setMessageText:@"Are you sure you want to delete this friend?"];
    [alert setAlertStyle:NSWarningAlertStyle];

    [alert beginSheetModalForWindow:[[SPMainWindowController sharedMainWindowController] window]
                      modalDelegate:self
                     didEndSelector:@selector(removeFriendSheetDidEnd:returnCode:contextInfo:)
                        contextInfo:nil];
}

- (void)removeFriendSheetDidEnd:(NSAlert *)alert returnCode:(int)returnCode contextInfo:(void *)contextInfo
{
    if (returnCode == NSAlertFirstButtonReturn) {
        NSArray *selectedObjects = [friendsController selectedObjects];
        if ([selectedObjects count]) {
            // delete the friend
            [friendsController removeObject:[selectedObjects objectAtIndex:0]];

            [[NSUserDefaults standardUserDefaults] setObject:friends forKey:SPFriends];
        }
    }

    [[alert window] orderOut:self];
}

@end
