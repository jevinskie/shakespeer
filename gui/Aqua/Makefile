SUBDIRS = Resources

TOP=../..
include $(TOP)/common.mk

ifneq ($(XCODE),)
all-local: app_bundle.stamp
bin_PROGRAMS=ShakesPeer
else
all-local:
endif

SOURCES = \
	FilteringArrayController.m \
	MHSysTree.m \
	MenuButton.m \
	NSMenu-UserCommandAdditions.m \
	NSMutableAttributedString-SmileyAdditions.m \
	NSStringExtensions.m \
	SPApplicationController.m \
	SPBookmarkController.m \
	SPChatWindowController.m \
	SPClientBridge.m \
	SPFilelistController.m \
	SPGrowlBridge.m \
	SPHubController.m SPLog.m \
	SPKeychain.m \
	SPMainWindowController-Toolbar.m \
	SPMainWindowController.m \
	SPMessagePanel.m \
	SPNotificationNames.m \
	SPOutlineView.m \
	SPPreferenceController.m \
	SPPublicHubsController.m \
	SPQueueController.m \
	SPSearchWindowController.m \
	SPSideBar.m \
	SPTableView.m \
	SPTransferController.m \
	SPTransformers.m \
	SPUser.m \
	SPUserCommand.m \
	SPUserDefaultKeys.m \
	SidebarCell.m \
	URLMutableAttributedString.m \
	URLTextView.m \
	main.m

SPApplicationController.o SPClientBridge.o: $(TOP)/version.mk

OBJS=$(SOURCES:.m=.o)

CFLAGS+=-pipe -fobjc-exceptions -fpascal-strings -fobjc-direct-dispatch -F.

FRAMEWORKS = -framework Cocoa \
	     -framework SystemConfiguration\
	     -framework Growl \
	     -framework Security

ShakesPeer: Growl.framework $(OBJS) \
	    $(TOP)/spclient/libspclient.a \
	    $(TOP)/splib/libsplib.a
	$(CC) $(OBJS) -o $@ $(CFLAGS) $(UB_CFLAGS) $(LDFLAGS) $(LIBS) \
	$(FRAMEWORKS) -bind_at_load

GROWL_VERSION=0.7.4

Growl-$(GROWL_VERSION)-SDK.dmg:
	ftp http://growl.info/files/Growl-$(GROWL_VERSION)-SDK.dmg

Growl.framework: Growl-$(GROWL_VERSION)-SDK.dmg
	p=`hdiutil attach Growl-$(GROWL_VERSION)-SDK.dmg -private | \
		awk '$$1 ~ /^\/dev/ && $$2 == "Apple_HFS" {print $$3}'` && \
	cp -RP $$p/Frameworks/Growl.framework . && \
	hdiutil detach $$p

APP=ShakesPeer.app
CONTENTS=$(APP)/Contents
RES=$(CONTENTS)/Resources
FWDIR=$(CONTENTS)/Frameworks

app_bundle.stamp: ShakesPeer
	$(MAKE) -C Resources all
	@echo "------------------------------"
	@echo "Creating application bundle..."
	@echo "------------------------------"
	mkdir -p ShakesPeer.app/Contents/MacOS
	mkdir -p $(FWDIR)
	mkdir -p $(RES)/English.lproj
	cp -f Resources/Info.plist ShakesPeer.app/Contents
	cp -fR Resources/English.lproj/*[^~].nib $(RES)/English.lproj
	cp -f Resources/English.lproj/InfoPlist.strings $(RES)/English.lproj
	cp -f Resources/Images/*.png $(RES)
	cp -f Resources/Images/*.tiff $(RES)
	cp -f Resources/Images/*.tif $(RES)
	cp -f Resources/Images/Smileys/*.png $(RES)
	cp -f Resources/Images/shakespeer.icns $(RES)
	cp -f ../../sphubd/sphubd $(RES)
	cp -f ../../sphubd/sphashd $(RES)
	strip $(RES)/sphubd
	strip $(RES)/sphashd
	cp -f ShakesPeer ShakesPeer.app/Contents/MacOS
	test -d $(FWDIR)/Growl.framework || \
		cp -RP Growl.framework $(FWDIR)
	for a in checkpoint dump load recover upgrade verify stat; do \
		cp -f ../../db-install/bin/db_$$a $(RES) ; \
	done

install:

clean-local:
	rm -f $(bin_PROGRAMS) $(OBJS)
	$(MAKE) -C Resources clean

