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

@protocol SPSideBarItem;

#define SPTransferState_Downloading 0
#define SPTransferState_Uploading 1
#define SPTransferState_Idle 2
#define SPTransferState_Error 3

@interface SPTransferItem : NSObject
{
    NSString *nick;
    NSString *status;
    NSAttributedString *filename;
    NSAttributedString *pathname;
    unsigned long long exactSize;
    unsigned long long offset;
    unsigned int speed;
    NSString *targetFilename;
    NSString *hubAddress;
    int state;
    unsigned int totalTime;
    int direction;
}

- (id)copy;
- (id)initWithNick:(NSString *)aNick
          filename:(NSString *)aFilename
              size:(unsigned long long)aSize
        hubAddress:(NSString *)aHubAddress
         direction:(int)aDirection;
- (NSString *)nick;
- (void)setStatus:(NSString *)aStatusString;
- (NSString *)targetFilename;
- (void)setFilename:(NSString *)aFilename;
- (unsigned long long)size;
- (void)setSize:(unsigned long long)aSize;
- (void)setSpeed:(unsigned int)aSpeed;
- (unsigned int)ETA;
- (float)ratio;
- (void)setOffset:(unsigned long long)anOffset;
- (void)setState:(int)aState;
- (int)direction;

@end

@interface SPTransferController : NSObject < SPSideBarItem >
{
    NSMutableArray *transfers;

    IBOutlet NSMenu *transferMenu;
    IBOutlet NSView *transferView;
    IBOutlet NSTableView *transferTable;
    IBOutlet NSArrayController *arrayController;
    
    IBOutlet NSMenu *columnsMenu;
    IBOutlet NSTableColumn *tcUser;
    IBOutlet NSTableColumn *tcStatus;
    IBOutlet NSTableColumn *tcTimeLeft;
    IBOutlet NSTableColumn *tcSpeed;
    IBOutlet NSTableColumn *tcFilename;
    IBOutlet NSTableColumn *tcSize;
    IBOutlet NSTableColumn *tcPath;
    IBOutlet NSTableColumn *tcHub;
}

- (IBAction)cancelTransfer:(id)sender;
- (IBAction)removeSource:(id)sender;
- (IBAction)removeQueue:(id)sender;
- (IBAction)removeAllSourcesWithNick:(id)sender;
- (IBAction)browseUser:(id)sender;
- (IBAction)privateMessage:(id)sender;
- (IBAction)toggleColumn:(id)sender;

- (SPTransferItem *)findTransferItemWithTargetFilename:(NSString *)aTargetFilename;
- (SPTransferItem *)findTransferItemWithNick:(NSString *)aNick directions:(int)aDirectionMask;

- (void)cancelTransfersInArray:(NSArray *)selectedTransfers;
- (void)removeSourcesInArray:(NSArray *)selectedTransfers;
- (void)removeQueuesInArray:(NSArray *)selectedTransfers;
- (void)removeAllSourcesWithNicksInArray:(NSArray *)selectedTransfers;
- (void)browseUserInArray:(NSArray *)selectedTransfers;
- (void)privateMessageInArray:(NSArray *)selectedTransfers;

@end

