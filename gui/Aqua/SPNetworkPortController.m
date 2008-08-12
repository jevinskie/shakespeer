/* vim: ft=objc
 *
 * Copyright 2008 Håkan Waara <hwaara@gmail.com>
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
 
#import "SPNetworkPortController.h"
#import "SPUserDefaultKeys.h"

#import <TCMPortMapper/TCMPortMapper.h>


@implementation SPNetworkPortController

static SPNetworkPortController *sharedPortController = nil;

+ (SPNetworkPortController *)sharedInstance
{
  if (!sharedPortController) {
    sharedPortController = [[SPNetworkPortController alloc] init];
  }
  return sharedPortController;
}

- (void)startup
{
  TCMPortMapper *pm = [TCMPortMapper sharedInstance];
                                               
  [self changePort:[[NSUserDefaults standardUserDefaults] integerForKey:SPPrefsPort]];
  
  [pm start];
}

- (void)shutdown
{
  TCMPortMapper *pm = [TCMPortMapper sharedInstance];
  
  if (lastPortMapping) {
    // XXX: is this needed/good, or should there only be a stopBlocking call?
    [pm removePortMapping:lastPortMapping];
    [lastPortMapping autorelease];
    lastPortMapping = nil;
  }
  
  if ([pm isRunning])
    [pm stopBlocking];
}

- (void)changePort:(int)port
{
  // avoid "changing" port to the current port (which should never happen, really); this
  // seems to confuse the TCMPortMapping framework
  if (lastPortMapping && port == [lastPortMapping localPort])
    return;
    
  TCMPortMapper *pm = [TCMPortMapper sharedInstance];
  
  if (lastPortMapping) {
    // remove the previous port mapping
    [pm removePortMapping:lastPortMapping];
    [lastPortMapping autorelease];
    lastPortMapping = nil;
  }
  
  TCMPortMapping *newMapping = 
    [TCMPortMapping portMappingWithLocalPort:port 
                         desiredExternalPort:port 
                           transportProtocol:TCMPortMappingTransportProtocolBoth // both TCP and UDP
                                    userInfo:nil];

  [pm addPortMapping:newMapping];
  lastPortMapping = [newMapping retain];
}


@end
