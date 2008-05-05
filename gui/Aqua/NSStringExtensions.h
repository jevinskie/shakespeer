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

@interface NSString (ShakesPeerExtensions)

- (NSComparisonResult)numericalCompare:(NSString *)aString;
- (NSString *)stringByDeletingLastWindowsPathComponent;
- (NSString *)lastWindowsPathComponent;
- (int)indexOfCharacter:(unichar)character fromIndex:(int)startIndex;
- (int)occurencesOfString:(NSString *)aString;
- (uint64_t)unsignedLongLongValue;
- (NSAttributedString *)truncatedString:(NSLineBreakMode)mode;

@end

@interface NSMutableAttributedString (ShakesPeerExtensions)
- (NSComparisonResult)compare:(NSMutableAttributedString *)aString;
@end

