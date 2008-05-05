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

#import "SPHubController.h"
#import "SPMainWindowController.h"
#import "SPApplicationController.h"
#import "SPBookmarkController.h"
#import "SPUser.h"
#import "SPUserCommand.h"
#import "SPTransformers.h"
#import "SPLog.h"
#import "URLMutableAttributedString.h"
#import "NSMutableAttributedString-SmileyAdditions.h"
#import "SPPreferenceController.h"
#import "NSMenu-UserCommandAdditions.h"
#import "SPGrowlBridge.h"
#import "SPSideBar.h"
#import "FilteringArrayController.h"

#import "SPNotificationNames.h"
#import "SPUserDefaultKeys.h"

#include "util.h"

@implementation SPHubController

- (id)initWithAddress:(NSString *)anAddress nick:(NSString *)aNick
{
    self = [super initWithWindowNibName:@"Hub"];
    if (self) {
        usersTree = [[MHSysTree alloc] init];
        nops = 0;
        totsize = 0ULL;
        needUpdating = NO;
        address = [anAddress retain];
        name = [anAddress retain];
        nick = [aNick retain];
        [self setConnected];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(userUpdateNotification:)
                                                     name:SPNotificationUserUpdate
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(userLoginNotification:)
                                                     name:SPNotificationUserLogin
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(userLogoutNotification:)
                                                     name:SPNotificationUserLogout
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(publicMessageNotification:)
                                                     name:SPNotificationPublicMessage
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(hubRedirectNotification:)
                                                     name:SPNotificationHubRedirect
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(userCommandNotification:)
                                                     name:SPNotificationUserCommand
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(hubnameChangedNotification:)
                                                     name:SPNotificationHubnameChanged
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(hubDisconnectedNotification:)
                                                     name:SPNotificationHubDisconnected
                                                   object:nil];

        updateTimer = [NSTimer scheduledTimerWithTimeInterval:1.0
                                                       target:self
                                                     selector:@selector(updateUserTable:)
                                                     userInfo:nil
                                                      repeats:YES];
    }

    return self;
}

- (void)awakeFromNib 
{
    /* [userArrayController setSearchKeys:[NSArray arrayWithObjects:@"nick", @"descriptionString", @"tag", @"email", nil]]; */

    [userTable setTarget:self];
    [userTable setDoubleAction:@selector(browseUser:)];

    [tcNick retain];
    [tcShare retain];
    [tcTag retain];
    [tcSpeed retain];
    [tcDescription retain];
    [tcEmail retain];
    [tcIcon retain];

    NSArray *tcs = [userTable tableColumns];
    NSEnumerator *e = [tcs objectEnumerator];
    NSTableColumn *tc;
    while ((tc = [e nextObject]) != nil) {
        [[tc dataCell] setWraps:YES];

        if (tc == tcNick)
            [[columnsMenu itemWithTag:0] setState:NSOnState];
        else if (tc == tcShare)
            [[columnsMenu itemWithTag:1] setState:NSOnState];
        else if (tc == tcTag)
            [[columnsMenu itemWithTag:2] setState:NSOnState];
        else if (tc == tcSpeed)
            [[columnsMenu itemWithTag:3] setState:NSOnState];
        else if (tc == tcDescription)
            [[columnsMenu itemWithTag:4] setState:NSOnState];
        else if (tc == tcEmail)
            [[columnsMenu itemWithTag:5] setState:NSOnState];
        else if (tc == tcIcon)
            [[columnsMenu itemWithTag:6] setState:NSOnState];
    }

    [[userTable headerView] setMenu:columnsMenu];

    numStaticNickMenuEntries = [nickMenu numberOfItems];
}

- (void)filterUsers
{
    if (filter == nil || [filter count] == 0) {
        [filteredUsers release];
        filteredUsers = [users retain];
        return;
    }

    NSMutableArray *array = [NSMutableArray arrayWithCapacity:[users count]];
    NSEnumerator *e = [users objectEnumerator];
    SPUser *user;
    while ((user = [e nextObject]) != nil) {
        NSEnumerator *f = [filter objectEnumerator];
        NSString *fs;
        BOOL add = YES;
        while ((fs = [f nextObject]) != nil) {
            if ([fs length] > 0 &&
               [[user nick] rangeOfString:fs options:NSCaseInsensitiveSearch].location == NSNotFound) {
                add = NO;
                break;
            }
        }
        if (add)
            [array addObject:user];
    }

    [filteredUsers release];
    filteredUsers = [array retain];
}

- (void)updateUserTable:(NSTimer *)aTimer
{
    if (needUpdating) {
#if 0
        NSRect visibleRect = [userTable visibleRect];
        int firstIndex = [userTable rowAtPoint:NSMakePoint(visibleRect.origin.x, visibleRect.origin.y + visibleRect.size.height - 1)];
        SPUser *topUser = nil;
        if (firstIndex >= 0 && (unsigned int)firstIndex < [[userArrayController arrangedObjects] count]) {
            topUser = [[userArrayController arrangedObjects] objectAtIndex:firstIndex];
        }
#endif

        [users release];
        users = [[usersTree allObjects] retain];
        [self filterUsers];
        [userTable reloadData];
        needUpdating = NO;

#if 0
        if (topUser) {
            /* [[userScrollView contentView] scrollToPoint:visibleRect.origin]; */
            [userTable scrollRowToVisible:[[userArrayController arrangedObjects] indexOfObjectIdenticalTo:topUser]];
            /* [userTable scrollRowToVisible:firstIndex]; */
        }
#endif

        [hubStatisticsField setStringValue:[NSString stringWithFormat:@"%lu users, %u ops, %s",
            [users count], nops, str_size_human(totsize)]];
    }
}

- (void)dealloc 
{
    [[SPApplicationController sharedApplicationController] disconnectFromAddress:address];
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    
    [users release];
    [filteredUsers release];
    [usersTree release];
    [address release];
    [name release];
    [nick release];
    [encoding release];
    [descriptionString release];
    
    [tcNick release];
    [tcShare release];
    [tcTag release];
    [tcSpeed release];
    [tcDescription release];
    [tcEmail release];
    [tcIcon release];
    
    [super dealloc];
}

- (NSString *)address
{
    return address;
}

- (NSString *)nick
{
    return nick;
}

- (void)setName:(NSString *)aName
{
    if (name != aName) {
        [name release];
        name = [aName retain];
    }
}

- (void)setDescriptionString:(NSString *)aDescription
{
    if (aDescription != descriptionString) {
        [descriptionString release];
        descriptionString = [aDescription retain];
    }
}

- (void)setConnected
{
    disconnected = FALSE;
}

- (void)setEncoding:(NSString *)anEncoding
{
    if (encoding != anEncoding) {
        [encoding release];
        encoding = [anEncoding retain];
    }
}

#pragma mark -
#pragma mark Sidebar support

- (void)unbindControllers
{
    /* [userArrayController unbind:@"contentArray"]; */
    [updateTimer invalidate];
}

- (NSView *)view
{
    return [[self window] contentView];
}

- (BOOL)canClose
{
    return YES;
}

- (NSImage *)image
{
    return [NSImage imageNamed:@"TableHub"];
}

- (NSString *)title
{
    return [self name];
}

- (NSString *)name
{
    return name;
}

- (NSString *)sectionTitle
{
    return @"Hubs";
}

- (BOOL)isHighlighted
{
    return highlighted;
}

- (void)setHighlighted:(BOOL)aFlag
{
    highlighted = aFlag;
}

- (NSMenu *)menu
{
    return nickMenu;
}

#pragma mark -
#pragma mark Sphubd notifications

- (SPUser *)findUserWithNick:(NSString *)aNick
{
    SPUser *cmpUser = [SPUser userWithNick:aNick
                               description:nil
                                       tag:nil
                                     speed:nil
                                     email:nil
                                      size:nil
                                isOperator:NO
                                extraSlots:0];
    SPUser *user = [usersTree find:cmpUser];

    if (user == nil) {
        // no regular user found, let's see if there's any op with this nick.
        [cmpUser setIsOperator:YES];
        user = [usersTree find:cmpUser];
    }

    return user;
}

- (void)userLoginNotification:(NSNotification *)aNotification
{
    NSDictionary *userinfo = [aNotification userInfo];
    if ([[userinfo objectForKey:@"hubAddress"] isEqualToString:address]) {
        BOOL isOp = [[userinfo objectForKey:@"isOperator"] boolValue];
        NSString *theNick = [userinfo objectForKey:@"nick"];
        SPUser *user = [SPUser userWithNick:theNick
                                description:[userinfo objectForKey:@"description"]
                                        tag:[userinfo objectForKey:@"tag"]
                                      speed:[userinfo objectForKey:@"speed"]
                                      email:[userinfo objectForKey:@"email"]
                                       size:[userinfo objectForKey:@"size"]
                                 isOperator:isOp
                                 extraSlots:[[userinfo objectForKey:@"extraSlots"] unsignedIntValue]];

        [usersTree addObject:user];
        needUpdating = YES;

        totsize += [[userinfo objectForKey:@"size"] unsignedLongLongValue];
        if (isOp)
            nops++;
    }
}

- (void)userUpdateNotification:(NSNotification *)aNotification
{
    NSDictionary *userinfo = [aNotification userInfo];
    if ([[userinfo objectForKey:@"hubAddress"] isEqualToString:address]) {
        NSString *theNick = [userinfo objectForKey:@"nick"];
        SPUser *user = [self findUserWithNick:theNick];
        if (!user) {
            // for ops, we get a user-update notification before we even know they exist.
            [self userLoginNotification:aNotification];
        }
        else {
            totsize -= [user size];
            BOOL oldOperatorFlag = [user isOperator];
            BOOL newOperatorFlag = [[userinfo objectForKey:@"isOperator"] boolValue];

            if (oldOperatorFlag)
                nops--;

            if (oldOperatorFlag != newOperatorFlag) {
                /* Ouch, operator status changed, which affects sort ordering. */
                /* Remove and re-insert the entry. */
                [user retain];
                [usersTree removeObject:user];
                [user setIsOperator:newOperatorFlag];
                [usersTree addObject:user];
                [user release];
            }

            /* Now update the attributes. */

            [user setDescription:[userinfo objectForKey:@"description"]];
            [user setTag:[userinfo objectForKey:@"tag"]];
            [user setSpeed:[userinfo objectForKey:@"speed"]];
            [user setEmail:[userinfo objectForKey:@"email"]];
            [user setSize:[userinfo objectForKey:@"size"]];
            [user setExtraSlots:[[userinfo objectForKey:@"extraSlots"] unsignedIntValue]];

            totsize += [user size];
            if (newOperatorFlag)
                nops++;

            needUpdating = YES;
        }
    }
}

- (void)userLogoutNotification:(NSNotification *)aNotification
{
    if ([[[aNotification userInfo] objectForKey:@"hubAddress"] isEqualToString:address]) {
        NSString *theNick = [[aNotification userInfo] objectForKey:@"nick"];
        SPUser *user = [self findUserWithNick:theNick];
        if (user) {
            totsize -= [user size];
            if ([user isOperator])
                nops--;

            [usersTree removeObject:user];
        }
        needUpdating = YES;
    }
}

- (void)addChatMessage:(NSMutableAttributedString *)attrString
{
    [attrString detectURLs:[NSColor blueColor]];
    if ([[NSUserDefaults standardUserDefaults] boolForKey:SPPrefsShowSmileyIcons]) {
        [attrString replaceSmilies];
    }

    [[chatView textStorage] appendAttributedString:attrString];
    [chatView scrollRangeToVisible:NSMakeRange([[chatView textStorage] length], 0)];
    [[SPMainWindowController sharedMainWindowController] highlightItem:self];
}

- (void)addStatusMessage:(NSString *)aMessage
{
    NSMutableAttributedString *attrmsg = [[NSMutableAttributedString alloc] initWithString:aMessage];

    [attrmsg addAttribute:NSForegroundColorAttributeName
                    value:[NSColor orangeColor]
                    range:NSMakeRange(0, [aMessage length])];
    [self addChatMessage:attrmsg];
    [attrmsg release];
}

- (void)publicMessageNotification:(NSNotification *)aNotification
{
    if ([[[aNotification userInfo] objectForKey:@"hubAddress"] isEqualToString:address]) {
        NSString *dateString = [[NSDate date] descriptionWithCalendarFormat:@"%H:%M"
                                                                   timeZone:nil
                                                                     locale:nil];

        NSString *aNick = [[aNotification userInfo] objectForKey:@"nick"];
        NSString *aMessage = [[aNotification userInfo] objectForKey:@"message"];
        NSString *msg;
        BOOL meMessage = NO;
        if ([aMessage hasPrefix:@"/me "]) {
            msg = [NSString stringWithFormat:@"[%@] %@ %@\n",
                dateString, aNick, [aMessage substringWithRange:NSMakeRange(4, [aMessage length] - 4)]];
            meMessage = TRUE;
        }
        else {
            msg = [NSString stringWithFormat:@"[%@] <%@> %@\n", dateString, aNick, aMessage];
        }
        NSMutableAttributedString *attrmsg = [[[NSMutableAttributedString alloc] initWithString:msg] autorelease];

        NSColor *textColor;
        if ([aNick isEqualToString:nick]) {
            textColor = [NSColor blueColor];
        }
        else {
            textColor = [NSColor redColor];

            if ([aMessage rangeOfString:nick options:NSCaseInsensitiveSearch].location != NSNotFound) {
                /* our nick was mentioned in chat */
                [[SPGrowlBridge sharedGrowlBridge] notifyWithName:SP_GROWL_NICK_IN_MAINCHAT
                                                      description:[NSString stringWithFormat:@"%@: %@", aNick, aMessage]];

            }
        }

        unsigned int dateLength = [dateString length] + 3;
        if (meMessage) {
            [attrmsg addAttribute:NSForegroundColorAttributeName
                            value:textColor
                            range:NSMakeRange(dateLength, [attrmsg length] - dateLength)];
        }
        else {
            [attrmsg addAttribute:NSForegroundColorAttributeName
                            value:textColor
                            range:NSMakeRange(dateLength, 2 + [aNick length])];
        }

        [self addChatMessage:attrmsg];
    }
}

- (void)hubRedirectNotification:(NSNotification *)aNotification
{
    if ([[[aNotification userInfo] objectForKey:@"hubAddress"] isEqualToString:address]) {
        [self willChangeValueForKey:@"title"];
        [address release];
        address = [[[aNotification userInfo] objectForKey:@"newAddress"] retain];
        [self didChangeValueForKey:@"title"];
        [self setName:address];
        [self addStatusMessage:[NSString stringWithFormat:@"Redirected to hub %@\n", address]];
    }
}

- (void)executeHubUserCommand:(id)sender
{
    NSArray *nicks = nil;

    int row = [userTable selectedRow];
    if (row != -1) {
        SPUser *user = [filteredUsers objectAtIndex:row];

        NSMutableDictionary *parameters = [NSMutableDictionary dictionaryWithCapacity:1];
        [parameters setObject:[user nick] forKey:@"nick"];

        nicks = [NSArray arrayWithObject:parameters];
    }

    [[sender representedObject] executeForNicks:nicks myNick:nick];
}

- (void)userCommandNotification:(NSNotification *)aNotification
{
    if ([address isEqualToString:[[aNotification userInfo] objectForKey:@"hubAddress"]]) {
        int context = [[[aNotification userInfo] objectForKey:@"context"] intValue];
        if ((context & 3) > 0) /* context 1 (hub) or 2 (user) */
        {
            int type = [[[aNotification userInfo] objectForKey:@"type"] intValue];
            NSString *title = [[aNotification userInfo] objectForKey:@"description"];
            NSString *command = [[aNotification userInfo] objectForKey:@"command"];

            SPUserCommand *uc = [[SPUserCommand alloc] initWithTitle:title
                                                             command:command
                                                                type:type
                                                             context:context
                                                                 hub:address];

            [nickMenu addUserCommand:[uc autorelease]
                              action:@selector(executeHubUserCommand:)
                              target:self
                       staticEntries:numStaticNickMenuEntries];
        }
    }
}

- (void)hubnameChangedNotification:(NSNotification *)aNotification
{
    if ([address isEqualToString:[[aNotification userInfo] objectForKey:@"hubAddress"]]) {
        [self setName:[[aNotification userInfo] objectForKey:@"newHubname"]];
    }
}

- (void)hubDisconnectedNotification:(NSNotification *)aNotification
{
    if (!disconnected &&
       [address isEqualToString:[[aNotification userInfo] objectForKey:@"hubAddress"]]) {
        disconnected = YES;
        [usersTree removeAllObjects];
        needUpdating = YES;
        nops = 0;
        totsize = 0ULL;
        [self addStatusMessage:@"Disconnected from hub!\n"];
    }
}

#pragma mark -
#pragma mark Interface actions

- (IBAction)grantExtraSlot:(id)sender
{
    int row = [userTable selectedRow];
    if (row == -1)
        return;

    SPUser *user = [filteredUsers objectAtIndex:row];
    if (user) {
        [[SPApplicationController sharedApplicationController] grantExtraSlotToNick:[user nick]];
    }
}

- (IBAction)bookmarkHub:(id)sender
{
    BOOL result = [[SPBookmarkController sharedBookmarkController] addBookmarkWithName:address
                                                                               address:address
                                                                                  nick:nick
                                                                              password:@""
                                                                           description:descriptionString
                                                                           autoconnect:NO
                                                                              encoding:nil];
    if (result)
        [self addStatusMessage:[NSString stringWithFormat:@"Hub %@ added to bookmarks\n", address]];
    else
        [self addStatusMessage:[NSString stringWithFormat:@"Hub %@ already added to bookmarks\n", address]];
}

- (IBAction)sendMessage:(id)sender
{
    NSMutableString *message = [[[sender stringValue] mutableCopy] autorelease];

    if ([message hasPrefix:@"/"] && ![message hasPrefix:@"/me "]) {
        /* this is a command */
        NSArray *args = [message componentsSeparatedByString:@" "];
        NSString *cmd = [args objectAtIndex:0];
        
        // COMMAND: /fav
        if ([cmd isEqualToString:@"/fav"] || [cmd isEqualToString:@"/favorite"]) {
            [self bookmarkHub:self];
        }
        
        // COMMAND: /clear
        else if ([cmd isEqualToString:@"/clear"]) {
            [[chatView textStorage] setAttributedString:[[[NSMutableAttributedString alloc] initWithString:@""] autorelease]];
        }
        
        // COMMAND: /help
        else if ([cmd isEqualToString:@"/help"]) {
            [self addStatusMessage:@"Available commands:\n"
              "  /fav or /favorite: add this hub as a bookmark\n"
              "  /clear: clear the chat window\n"
              "  /pm <nick>: start a private chat with <nick>\n"
              "  /refresh: rescan shared files\n"
              "  /join <address>: connect to a hub\n"
              "  /search <keywords>: search for keywords\n"
              "  /userlist <nick>: load nicks filelist\n"
              "  /reconnect: reconnect to disconnected hub\n"
              "  /np: show current track in iTunes\n"];
        }
        
        // COMMAND: /pm
        else if ([cmd isEqualToString:@"/pm"]) {
            if ([args count] == 2) {
                sendNotification(SPNotificationStartChat,
                        @"remote_nick", [args objectAtIndex:1],
                        @"hubAddress", address,
                        @"my_nick", nick,
                        nil);
            }
            else
                [self addStatusMessage:@"No nick specified\n"];
        }
        
        // COMMAND: /refresh
        else if ([cmd isEqualToString:@"/refresh"]) {
            [[SPPreferenceController sharedPreferences] updateAllSharedPaths];
        }
        
        // COMMAND: /join
        else if ([cmd isEqualToString:@"/join"]) {
            if ([args count] >= 2) {
                [[SPApplicationController sharedApplicationController] connectWithAddress:[args objectAtIndex:1]
                                                                                     nick:nil
                                                                              description:nil
                                                                                 password:nil
                                                                                 encoding:nil];
            }
            else
                [self addStatusMessage:@"No hub address specified\n"];
        }
        
        // COMMAND: /search
        else if ([cmd isEqualToString:@"/search"]) {
            if ([args count] > 1) {
                [[SPMainWindowController sharedMainWindowController]
                    performSearchFor:[message substringFromIndex:8]
                                size:nil
                     sizeRestriction:SHARE_SIZE_NONE
                                type:SHARE_TYPE_ANY
                          hubAddress:address];
            }
            else
            {
                [self addStatusMessage:@"No search keywords specified\n"];
            }
        }
        
        // COMMAND: /userlist
        else if ([cmd isEqualToString:@"/userlist"]) {
            if ([args count] >= 2) {
                [[SPApplicationController sharedApplicationController]
                    downloadFilelistFromUser:[args objectAtIndex:1]
                                       onHub:address
                                 forceUpdate:NO
                                   autoMatch:NO];
            }
            else
            {
                [self addStatusMessage:@"No nick specified\n"];
            }
        }
        
        // COMMAND: /reconnect
        else if ([cmd isEqualToString:@"/reconnect"]) {
            if (disconnected == NO) {
                [self addStatusMessage:@"Still connected\n"];
            }
            else
            {
                [[SPApplicationController sharedApplicationController] connectWithAddress:address
                                                                                     nick:nick
                                                                              description:descriptionString
                                                                                 password:nil /* FIXME: can't we store the password? */
                                                                                 encoding:encoding];
            }
        }
        
        // COMMAND: /np
        else if ([cmd isEqualToString:@"/np"]) {
            // Display the current playing track in iTunes
            NSString *path = [[NSBundle mainBundle] pathForResource:@"np" ofType:@"scpt"];
            if (path != nil) {
                // Create the URL for the script
                NSURL* url = [NSURL fileURLWithPath:path];
                if (url != nil) {
                    // Set up an error dict and the script
                    NSDictionary *errors;
                    NSAppleScript* appleScript = [[NSAppleScript alloc] initWithContentsOfURL:url error:&errors];
                    if (appleScript != nil) {
                        // Run the script
                        NSAppleEventDescriptor *returnDescriptor = [appleScript executeAndReturnError:&errors];
                        [appleScript release];
                        if (returnDescriptor != nil) {
                            // We got some results
                            NSString *theTitle = [[returnDescriptor descriptorAtIndex:1] stringValue];
                            NSString *theArtist = [[returnDescriptor descriptorAtIndex:2] stringValue];
                            NSString *theMessage;
                            if (!theTitle || !theArtist)
                                theMessage = @"/me isn't listening to anything";
                            else
                                theMessage = [NSString stringWithFormat:@"/me is listening to %@ by %@", theTitle, theArtist];
                            [[SPApplicationController sharedApplicationController] sendPublicMessage:theMessage
                                                                                               toHub:address];
                        }
                        else {
                            // Something went wrong
                            NSLog(@"Script error: %@", [errors objectForKey: @"NSAppleScriptErrorMessage"]);
                        } // returnDescriptor
                    } // appleScript
                } // url
            } // path
        } // np
        else {
            [self addStatusMessage:[NSString stringWithFormat:@"Unknown command: %@\n", cmd]];
        }
    }
    else {
        if (disconnected) {
            [self addStatusMessage:@"Disconnected from hub, use /reconnect to reconnect\n"];
        }
        else {
            [[SPApplicationController sharedApplicationController] sendPublicMessage:message
                                                                               toHub:address];
        }
    }

    [sender setStringValue:@""];
    [self focusChatInput];
}

- (void)focusChatInput
{
    /* keep the text field the first responder (ie, give it input focus) */
    [[chatInput window] performSelector:@selector(makeFirstResponder:)
                          withObject:chatInput
                          afterDelay:0];
}

- (void)processFilelist:(id)sender autoMatch:(BOOL)autoMatchFlag
{
    int row = [userTable selectedRow];
    if (row == -1)
        return;

    SPUser *user = [filteredUsers objectAtIndex:row];
    if (user) {
        [[SPApplicationController sharedApplicationController] downloadFilelistFromUser:[user nick]
                                                                                  onHub:address
                                                                            forceUpdate:NO
                                                                              autoMatch:autoMatchFlag];
    }
}

- (IBAction)browseUser:(id)sender
{
    [self processFilelist:sender autoMatch:NO];
}

- (IBAction)autoMatchFilelist:(id)sender
{
    [self processFilelist:sender autoMatch:YES];
}

- (IBAction)startPrivateChat:(id)sender
{
    int row = [userTable selectedRow];
    if (row == -1)
        return;

    SPUser *user = [filteredUsers objectAtIndex:row];
    if (user) {
        sendNotification(SPNotificationStartChat,
                @"remote_nick", [user nick],
                @"hubAddress", address,
                @"my_nick", nick,
                nil);
    }
}

- (IBAction)toggleColumn:(id)sender
{
    NSTableColumn *tc = nil;
    switch([sender tag]) {
        case 0: tc = tcNick; break;
        case 1: tc = tcShare; break;
        case 2: tc = tcTag; break;
        case 3: tc = tcSpeed; break;
        case 4: tc = tcDescription; break;
        case 5: tc = tcEmail; break;
        case 6: tc = tcIcon; break;
    }
    if (tc == nil)
        return;

    if ([sender state] == NSOffState) {
        [sender setState:NSOnState];
        [userTable addTableColumn:tc];
    }
    else {
        [sender setState:NSOffState];
        [userTable removeTableColumn:tc];
    }
}

- (IBAction)filter:(id)sender
{
    NSString *filterString = [sender stringValue];
    [filter release];
    filter = nil;
    if ([filterString length] > 0)
        filter = [[filterString componentsSeparatedByString:@" "] retain];

    [self filterUsers];
    [userTable reloadData];
}

#pragma mark -
#pragma mark NSSplitView delegates

- (float)splitView:(NSSplitView *)sender constrainMinCoordinate:(float)proposedMin
       ofSubviewAt:(int)offset
{
    return 100;
}

- (float)splitView:(NSSplitView *)sender constrainMaxCoordinate:(float)proposedMax
       ofSubviewAt:(int)offset
{
    return proposedMax - 100;
}

- (BOOL)splitView:(NSSplitView *)sender canCollapseSubview:(NSView *)subview
{
    return YES;
}

- (void)splitView:(id)sender resizeSubviewsWithOldSize:(NSSize)oldSize
{
    NSRect newFrame = [sender frame];
    float dividerThickness = [sender dividerThickness];

    NSView *firstView = [[sender subviews] objectAtIndex:0];
    NSView *secondView = [[sender subviews] objectAtIndex:1];

    NSRect firstFrame = [firstView frame];
    NSRect secondFrame = [secondView frame];

    /* keep nick list in constant width */
    firstFrame.size.width = newFrame.size.width - (secondFrame.size.width + dividerThickness);
    firstFrame.size.height = newFrame.size.height;

    if (firstFrame.size.width < 0) {
        firstFrame.size.width = 0;
        secondFrame.size.width = newFrame.size.width - firstFrame.size.width - dividerThickness;
    }

    secondFrame.origin.x = firstFrame.size.width + dividerThickness;

    [firstView setFrame:firstFrame];
    [secondView setFrame:secondFrame];
    [sender adjustSubviews];
}

#pragma mark -
#pragma mark NSTableView data source

- (int)numberOfRowsInTableView:(NSTableView *)aTableView
{
    return [filteredUsers count];
}

- (id)tableView:(NSTableView *)aTableView
 objectValueForTableColumn:(NSTableColumn *)aTableColumn
            row:(int)rowIndex
{
    SPUser *user = [filteredUsers objectAtIndex:rowIndex];

    if (user) {
        NSString *identifier = [aTableColumn identifier];
        if ([identifier isEqualToString:@"nick"]) {
            return [user displayNick];
        }
        else if ([identifier isEqualToString:@"size"]) {
            return [NSString stringWithUTF8String:str_size_human([user size])];
        }
        else if ([identifier isEqualToString:@"tag"]) {
            return [user tag];
        }
        else if ([identifier isEqualToString:@"speed"]) {
            return [user speed];
        }
        else if ([identifier isEqualToString:@"email"]) {
            return [user email];
        }
        else if ([identifier isEqualToString:@"descriptionString"]) {
            return [user descriptionString];
        }
        else if ([identifier isEqualToString:@"icon"]) {
            return [[NickImageTransformer defaultNickImageTransformer] transformedValue:user];
        }
    }

    return nil;
}

- (void)tableView:(NSTableView *)tableView
sortDescriptorsDidChange:(NSArray *)oldDescriptors
{
	NSArray *newDescriptors = [tableView sortDescriptors];
	[users sortUsingDescriptors:newDescriptors];
	[tableView reloadData];
}

@end

