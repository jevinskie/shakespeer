/* vim: ft=objc
 *
 * Copyright 2008 ShakesPeer developers <shakespeer.googlecode.com>
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

#import "SPProgressIndicatorCell.h"

@implementation SPProgressIndicatorCell

- (void)awakeFromNib
{
    attributedStringValue = [[NSAttributedString alloc] initWithString:@""];
    
    // Setup our values programmatically to ensure consistency
    [self setLevelIndicatorStyle:NSContinuousCapacityLevelIndicatorStyle];
    [self setFloatValue:0.0];
    [self setMinValue:0.0];
    [self setMaxValue:100.0];
    [self setWarningValue:0.0];
    [self setCriticalValue:0.0];
}

- (void)dealloc
{
    [attributedStringValue release];
    [super dealloc];
}

- (void)drawWithFrame:(NSRect)cellFrame inView:(NSView *)controlView
{
    // Check if transfer is in progress
    float currentValue = [self floatValue];
    if (currentValue > 1.0 && currentValue < 100.0) {
        // Draw our progress bar
        [super drawWithFrame:cellFrame inView:controlView];
    }
	else {
        // Draw just the status string
        [[self attributedStringValue] drawInRect:cellFrame];
    }
}

- (NSAttributedString *)attributedStringValue
{
    return attributedStringValue;
}

- (void)setAttributedStringValue:(NSAttributedString *)obj
{
    if (obj != attributedStringValue) {
        [attributedStringValue release];
        attributedStringValue = [obj copy];
    }
}

@end
