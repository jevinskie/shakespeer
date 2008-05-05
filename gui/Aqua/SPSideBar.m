/* vim: ft=objc
 *
 * Copyright 2005 Martin Hedenfalk <martin@bzero.se>
 * Based on code by Jonathan Jansson 
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

#import "SPSideBar.h"
#import "SidebarCell.h"

@implementation SPSideBar

- (void)setDelegate:(id)aDelegate
{
    if (aDelegate != delegate) {
        [delegate release];
        delegate = [aDelegate retain];
    }
}

- (void)tabView:(NSTabView *)tabView didSelectTabViewItem:(NSTabViewItem *)tabViewItem
{
    if (delegate && [delegate respondsToSelector:@selector(sideBar:didSelectItem:)])
        [delegate sideBar:self didSelectItem:[tabViewItem identifier]];
}

- (BOOL)tableView:(NSTableView *)aTableView shouldSelectRow:(int)rowIndex
{
    id item = [items objectAtIndex:rowIndex];
    return ! [item isKindOfClass:[NSString class]];
}

- (void)selectionDidChange:(NSNotification *)aNotification
{
    NSIndexSet *selectedIndexes = [self selectedRowIndexes];
    unsigned int selectedIndex = [selectedIndexes firstIndex];
    if (selectedIndex != NSNotFound)
        [self displayItem:[items objectAtIndex:selectedIndex]];
}

- (void)frameDidChange:(NSNotification *)aNotification
{
    NSRect frame = [[self superview] frame];
    [[[self tableColumns] objectAtIndex:0] setWidth:frame.size.width];
}

- (void)awakeFromNib
{   
    /* Remove all tabs */
    while ([tabView numberOfTabViewItems] > 0)
        [tabView removeTabViewItem:[tabView tabViewItemAtIndex:0]];
    
    [tabView setDelegate:self];

    /* Make the intercell spacing similar to that used in iCal's Calendars list. */
    [self setIntercellSpacing:NSMakeSize(0.0, 1.0)];

    /* Use our custom NSActionCell subclass for the only column */
    SidebarCell *theTextCell = [[SidebarCell alloc] init];
    [[[self tableColumns] objectAtIndex:0] setDataCell:theTextCell];
    [theTextCell release];

    /* Must set up target/action in code when using self as target. */
    [self setTarget:self];
    [self setDataSource:self];
    [super setDelegate:self];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(selectionDidChange:)
                                                 name:NSTableViewSelectionDidChangeNotification
                                               object:self];

    [[self superview] setPostsFrameChangedNotifications:YES];

    [[NSNotificationCenter defaultCenter] addObserver:self
                                             selector:@selector(frameDidChange:)
                                                 name:NSViewFrameDidChangeNotification
                                               object:[self superview]];

    [self frameDidChange:nil];
}

- (id)initWithCoder:(NSCoder *)decoder
{
    if ((self = [super initWithCoder:decoder])) {
        items = [[NSMutableArray alloc] init];
    }
    
    return self;
}
- (void)dealloc
{
    [items release];
    [super dealloc];
}

- (void)insertItem:(id <SPSideBarItem>)anItem atIndex:(int)anIndex
{
    /* Check that anItem is not already in the sidebar. */
    if ([tabView indexOfTabViewItemWithIdentifier:anItem] == NSNotFound) {
        NSTabViewItem *tabItem = [[NSTabViewItem alloc] initWithIdentifier:anItem];
        [tabItem setView:[anItem view]];
        [tabView addTabViewItem:tabItem];
        [tabItem release];

        [items insertObject:anItem atIndex:anIndex];
        // shift indexes to preserve selection
        NSIndexSet *selection = [self selectedRowIndexes];
        NSMutableIndexSet *newSelection = [[NSMutableIndexSet alloc] initWithIndexSet:selection];
        [newSelection shiftIndexesStartingAtIndex:anIndex by:1];
        [self reloadData];
        [self selectRowIndexes:newSelection byExtendingSelection:NO];
        [newSelection release];
    }
}

- (void)addItem:(id <SPSideBarItem>)anItem toSection:(NSString *)aSection
{
    [self insertItem:anItem atIndex:[items indexOfObject:aSection]+1];
}

- (void)addItem:(id <SPSideBarItem>)anItem
{
    if ([(NSObject *)anItem respondsToSelector:@selector(sectionTitle)]) {
        NSString *sectionTitle = [(id<SPSideBarItemInformalProtocol>)anItem sectionTitle];
        if ([self hasSection:sectionTitle] == NO)
            [self addSection:sectionTitle];
        [self addItem:anItem toSection:sectionTitle];
    }
    else {
        [self insertItem:anItem atIndex:[items count]];
    }
}

- (unsigned int)numberOfItemsInSection:(NSString *)aSection
{
    unsigned int index = [items indexOfObject:aSection];
    
    if (index == NSNotFound)
        return 0;
    
    unsigned int i = index + 1;
    unsigned int n = 0;
    while (i < [items count]) {
        if ([[items objectAtIndex:i] isKindOfClass:[NSString class]])
            break;
        i++;
        n++;
    }
    
    return n;
}

- (NSArray *)itemsInSection:(NSString *)aSection
{
    unsigned int startIndex = [items indexOfObject:aSection];
    if (startIndex == NSNotFound)
        return nil;
    NSRange range;
    range.location = startIndex + 1;
    range.length = [self numberOfItemsInSection:aSection];
    if (range.length == 0)
        return nil;
    
    return [items subarrayWithRange:range];
}

- (void)addSection:(NSString *)aSection
{
    [items addObject:aSection];
    [self reloadData];
}

- (BOOL)hasSection:(NSString *)aSection
{
    return [items indexOfObject:aSection] != NSNotFound;
}

- (id)itemWithTitle:(NSString *)aTitle
{
    NSEnumerator *e = [items objectEnumerator];
    id item;
    while ((item = [e nextObject]) != nil) {
        if ([item respondsToSelector:@selector(title)] && [aTitle isEqualToString:[item title]])
            return item;
    }
    
    return nil;
}

- (void)displayItem:(id)anItem
{
    /* Is the item in the tabView? */
    int index =  [tabView indexOfTabViewItemWithIdentifier:anItem];
    if (index != NSNotFound) {
        [tabView selectTabViewItemAtIndex:index];
        [self selectRowIndexes:[NSIndexSet indexSetWithIndex:[items indexOfObject:anItem]]
          byExtendingSelection:NO];

        if ([anItem respondsToSelector:@selector(setHighlighted:)])
            [anItem setHighlighted:NO];
    }
}

- (void)removeItem:(id)anItem
{
    int index = [tabView indexOfTabViewItemWithIdentifier:anItem];
    if (index != NSNotFound) {
        NSTabViewItem *tabViewItem = [tabView tabViewItemAtIndex:index];
        [tabView removeTabViewItem:tabViewItem];

        NSTabViewItem *selectedTabItem = [tabView selectedTabViewItem];
        if (selectedTabItem && ![anItem isKindOfClass:[NSString class]])
            index = [items indexOfObject:[selectedTabItem identifier]];
        else
            index = 0;
        
        [self selectRowIndexes:[NSIndexSet indexSetWithIndex:index] byExtendingSelection:NO];
    }

    /* hmmm, obviously the item refuses to dealloc since the
     * controller has retained it's target (if the target is File's
     * Owner), so we have to explicitly unbind the controller
     */
    if ([anItem respondsToSelector:@selector(unbindControllers)])
        [anItem unbindControllers];

    if ([delegate respondsToSelector:@selector(sideBar:willCloseItem:)])
        [delegate sideBar:self willCloseItem:anItem];

    [anItem retain];
    [items removeObject:anItem];

    if ([delegate respondsToSelector:@selector(sideBar:didCloseItem:)])
        [delegate sideBar:self didCloseItem:anItem];
    
    [anItem release];

    [self reloadData];
}

/* Called from the close icon in the table */
- (void)closeSelectedItem:(id)sender
{
    if ([self selectedRow] != -1) {
        id item = [items objectAtIndex:[self selectedRow]];
        if ([item respondsToSelector:@selector(canClose)] && [item canClose])
            [self removeItem:item];
    }
}

- (int)numberOfRowsInTableView:(NSTableView*)tableView
{       
    return [items count];
}

- (id)tableView:(NSTableView *)tableView objectValueForTableColumn:(NSTableColumn *)aTableColumn
            row:(int)rowIndex
{
    return [items objectAtIndex:rowIndex];
}

- (void)selectPreviousItem
{
    int index = [self selectedRow];
    do {
        if (--index < 0)
            index = [items count] - 1;
    } while ([[items objectAtIndex:index] isKindOfClass:[NSString class]]);
    
    [self displayItem:[items objectAtIndex:index]];
}

- (void)selectNextItem
{
    int index = [self selectedRow];
    do {
        if (++index >= [items count])
            index = 0;
    } while ([[items objectAtIndex:index] isKindOfClass:[NSString class]]);
    
    [self displayItem:[items objectAtIndex:index]];
}

@end

