/* vim: ft=objc
 *
 * Copyright 2004-2005 Martin Hedenfalk <martin@bzero.se>
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

#import "SPChatWindowController.h"
#import "SPApplicationController.h"
#import "SPMainWindowController.h"
#import "URLMutableAttributedString.h"
#import "NSMutableAttributedString-SmileyAdditions.h"
#import "SPGrowlBridge.h"
#import "SPSideBar.h"
#import "SPNotificationNames.h"
#import "SPUserDefaultKeys.h"

@implementation SPChatWindowController

- (id)initWithRemoteNick:(NSString *)remoteNick hub:(NSString *)aHubAddress myNick:(NSString *)aMyNick
{
    if ((self = [super init])) {
        [NSBundle loadNibNamed:@"ChatWindow" owner:self];
        nick = [remoteNick copy];
        hubAddress = [aHubAddress copy];
        
        if (aMyNick)
            myNick = [aMyNick copy];
        else
            myNick = [[[NSUserDefaults standardUserDefaults] stringForKey:SPPrefsNickname] retain];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(privateMessageNotification:)
                                                     name:SPNotificationPrivateMessage
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(userLoginNotification:)
                                                     name:SPNotificationUserLogin
                                                   object:nil];

        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(userLogoutNotification:)
                                                     name:SPNotificationUserLogout
                                                   object:nil];

        hubname = [[[[SPMainWindowController sharedMainWindowController] hubWithAddress:aHubAddress] name] retain];
        firstMessage = TRUE;
    }
    
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [nick release];
    [hubAddress release];
    [myNick release];
    [hubname release];
    [super dealloc];
}

- (void)unbindControllers
{
    sendNotification(SPNotificationEndChat, @"nick", nick, @"hubAddress", hubAddress, nil);
}

- (NSView *)view
{
    return chatView;
}

- (BOOL)canClose
{
    return YES;
}

- (NSImage *)image
{
    return [NSImage imageNamed:@"chat"];
}

- (NSString *)title
{
    return [NSString stringWithFormat:[NSString stringWithFormat:@"%@@%@", nick, hubname]];
}

- (NSMenu *)menu
{
    return nil;
}

- (NSString *)sectionTitle
{
    return @"Conversations";
}

- (BOOL)isHighlighted
{
    return highlighted;
}

- (void)setHighlighted:(BOOL)aFlag
{
    highlighted = aFlag;
}

- (void)addChatMessage:(NSString *)theMessage fromNick:(NSString *)theNick
{
    NSString *dateString = [[NSDate date] descriptionWithCalendarFormat:@"%H:%M"
                                                               timeZone:nil
                                                                 locale:nil];

    BOOL meMessage = NO;
    NSString *msg;
    if ([theMessage hasPrefix:@"/me "]) {
        msg = [NSString stringWithFormat:@"[%@] %@ %@\n",
                dateString, theNick, [theMessage substringWithRange:NSMakeRange(4, [theMessage length] - 4)]];
        meMessage = TRUE;
    }
    else {
        msg = [NSString stringWithFormat:@"[%@] <%@> %@\n", dateString, theNick, theMessage];
    }

    NSMutableAttributedString *attrmsg = [[NSMutableAttributedString alloc] initWithString:msg];

    NSColor *textColor;
    if ([theNick isEqualToString:myNick])
        textColor = [NSColor blueColor];
    else
        textColor = [NSColor redColor];

    if (meMessage) {
        unsigned int dateLength = [dateString length] + 3;
        [attrmsg addAttribute:NSForegroundColorAttributeName
                        value:textColor
                        range:NSMakeRange(dateLength, [attrmsg length] - dateLength)];
    }
    else {
        [attrmsg addAttribute:NSForegroundColorAttributeName
                        value:textColor
                        range:NSMakeRange([dateString length] + 3, 2 + [theNick length])];
    }

    [attrmsg detectURLs:[NSColor blueColor]];
    if ([[NSUserDefaults standardUserDefaults] boolForKey:SPPrefsShowSmileyIcons])
        [attrmsg replaceSmilies];
    [[chatTextView textStorage] appendAttributedString:attrmsg];
    [chatTextView scrollRangeToVisible:NSMakeRange([[chatTextView textStorage] length], 0)];
    [attrmsg release];
}

- (void)privateMessageNotification:(NSNotification *)aNotification
{
    NSString *theHubAddress = [[aNotification userInfo] objectForKey:@"hubAddress"];
    NSString *theNick = [[aNotification userInfo] objectForKey:@"nick"];
    NSString *theDisplayNick = [[aNotification userInfo] objectForKey:@"display_nick"];
    NSString *theMessage = [[aNotification userInfo] objectForKey:@"message"];

    if ([hubAddress isEqualToString:theHubAddress] && [nick isEqualToString:theNick]) {
        [self addChatMessage:theMessage fromNick:theDisplayNick];
        [[SPMainWindowController sharedMainWindowController] highlightItem:self];

        [[SPGrowlBridge sharedGrowlBridge] notifyWithName:firstMessage ? SP_GROWL_NEW_PRIVATE_CONVERSATION : SP_GROWL_PRIVATE_MESSAGE
                                              description:[NSString stringWithFormat:@"%@: %@", theDisplayNick, theMessage]];
        firstMessage = FALSE;
    }
}

- (void)commonLoginLogoutNotification:(NSNotification *)aNotification isLogin:(BOOL)loginFlag
{
    NSString *aNick = [[aNotification userInfo] objectForKey:@"nick"];
    if ([[[aNotification userInfo] objectForKey:@"hubAddress"] isEqualToString:hubAddress] &&
       [aNick isEqualToString:nick]) {
        NSString *dateString = [[NSDate date] descriptionWithCalendarFormat:@"%H:%M"
                                                                   timeZone:nil
                                                                     locale:nil];

        NSString *msg = [NSString stringWithFormat:@"[%@] %@ logged %@\n",
                         dateString, nick, loginFlag ? @"in" : @"out"];

        NSMutableAttributedString *attrmsg = [[NSMutableAttributedString alloc] initWithString:msg];
        unsigned int dateLength = [dateString length] + 3;
        [attrmsg addAttribute:NSForegroundColorAttributeName
                        value:[NSColor orangeColor]
                        range:NSMakeRange(dateLength, [attrmsg length] - dateLength)];
        [[chatTextView textStorage] appendAttributedString:attrmsg];
        [chatTextView scrollRangeToVisible:NSMakeRange([[chatTextView textStorage] length], 0)];
        [attrmsg release];
    }
}

- (void)userLoginNotification:(NSNotification *)aNotification
{
    [self commonLoginLogoutNotification:aNotification isLogin:YES];
}

- (void)userLogoutNotification:(NSNotification *)aNotification
{
    [self commonLoginLogoutNotification:aNotification isLogin:NO];
}

- (IBAction)sendMessage:(id)sender
{
    NSMutableString *message = [[[sender stringValue] mutableCopy] autorelease];
    
    if ([message hasPrefix:@"/"] && ![message hasPrefix:@"/me "]) {
        /* this is a command */
        NSArray *args = [message componentsSeparatedByString:@" "];
        NSString *cmd = [args objectAtIndex:0];
        
        // COMMAND: /np
        if ([cmd isEqualToString:@"/np"]) {
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
                            [[SPApplicationController sharedApplicationController] sendPrivateMessage:theMessage
                                                                                               toNick:nick
                                                                                                  hub:hubAddress];
                            [self addChatMessage:theMessage fromNick:myNick];
                        }
                        else {
                            // Something went wrong
                            NSLog(@"Script error: %@", [errors objectForKey: @"NSAppleScriptErrorMessage"]);
                        } // returnDescriptor
                    } // appleScript
                } // url
            } // path
        } // np
    }
    else {
        [[SPApplicationController sharedApplicationController] sendPrivateMessage:[sender stringValue]
                                                                           toNick:nick
                                                                              hub:hubAddress];
        [self addChatMessage:[sender stringValue] fromNick:myNick];
    }
    
    [sender setStringValue:@""];
    [self focusChatInput];
}

- (void)focusChatInput
{
    /* keep the text field the first responder (ie, give it input focus) */
    [[inputField window] performSelector:@selector(makeFirstResponder:)
                              withObject:inputField
                              afterDelay:0];
}

@end

