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
	SPNetworkPortController.m \
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
	main.m \
	UKCrashReporter.m \
	SPFriendsController.m \
	NSMenu-MassRemovalAdditions.m \
	NSTextView-ChatFormattingAdditions.m \
	SPProgressIndicatorCell.m \
	SPQueueItem.m

SPApplicationController.o SPClientBridge.o: $(TOP)/version.mk

OBJS=$(SOURCES:.m=.o)

CFLAGS+=-pipe -fobjc-exceptions -fpascal-strings -F.
ifeq ($(BUILD_PROFILE),release)
CFLAGS += -DBUILD_PROFILE=$(BUILD_PROFILE)
endif

FRAMEWORKS = -framework Cocoa \
	     -framework SystemConfiguration\
	     -framework Growl \
	     -framework TCMPortMapper \
	     -framework Security \
         -framework Sparkle

ShakesPeer: TCMPortMapper.framework \
			Growl.framework \
			Sparkle.framework $(OBJS) \
	    $(TOP)/spclient/libspclient.a \
	    $(TOP)/splib/libsplib.a
	$(CC) $(OBJS) -o $@ $(CFLAGS) $(UB_CFLAGS) $(LDFLAGS) $(LIBS) \
	$(FRAMEWORKS) -bind_at_load

GROWL_VERSION=1.3.1

Growl-$(GROWL_VERSION)-SDK.zip:
	ftp http://growl.cachefly.net/Growl-$(GROWL_VERSION)-SDK.zip

Growl.framework: Growl-$(GROWL_VERSION)-SDK.zip
	unzip Growl-$(GROWL_VERSION)-SDK.zip && \
	cp -RP Growl-$(GROWL_VERSION)-SDK/Framework/Growl.framework . && \
	rm -rf Growl-$(GROWL_VERSION)-SDK

PORTMAP_VERSION=1.3.1-r46

TCMPortMapper-$(PORTMAP_VERSION).zip:
	curl http://tcmportmapper.googlecode.com/files/TCMPortMapper-$(PORTMAP_VERSION).zip \
		>TCMPortMapper-$(PORTMAP_VERSION).zip

TCMPortMapper.framework:
	mkdir TCMPortMapper && \
	unzip TCMPortMapper-$(PORTMAP_VERSION).zip -d TCMPortMapper && \
	rm "TCMPortMapper/TCMPortMapper.framework/Resources/OUItoCompany.plist" && \
	cp -RP TCMPortMapper/TCMPortMapper.framework . && \
	rm -rf TCMPortMapper

SPARKLE_VERSION=1.5b6

Sparkle\ $(SPARKLE_VERSION).zip:
	ftp http://sparkle.andymatuschak.org/files/Sparkle%20$(SPARKLE_VERSION).zip

Sparkle.framework: Sparkle\ $(SPARKLE_VERSION).zip
	mkdir Sparkle && \
	unzip Sparkle\ $(SPARKLE_VERSION).zip -d Sparkle && \
	cp -RP "Sparkle/Sparkle.framework" . && \
	rm -rf Sparkle

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
	cp -f Resources/English.lproj/UKCrashReporter.strings $(RES)/English.lproj
	cp -f Resources/Credits.rtf $(RES)
	cp -f Resources/Images/*.png $(RES)
	cp -f Resources/Images/*.tiff $(RES)
	cp -f Resources/Images/*.tif $(RES)
	cp -f Resources/Images/Smileys/*.png $(RES)
	cp -f Resources/Images/shakespeer.icns $(RES)
	cp -f ../../sphubd/sphubd $(RES)
	cp -f ../../sphubd/sphashd $(RES)
	cp -f Resources/np.scpt $(RES)
	cp -f ShakesPeer ShakesPeer.app/Contents/MacOS
	test -d $(FWDIR)/Growl.framework || \
		cp -RP Growl.framework $(FWDIR)
	test -d $(FWDIR)/Sparkle.framework || \
		cp -RP Sparkle.framework $(FWDIR)
	test -d $(FWDIR)/TCMPortMapper.framework || \
		cp -RP TCMPortMapper.framework $(FWDIR)

install:

clean-local:
	rm -f $(bin_PROGRAMS) $(OBJS)
	rm -rf ShakesPeer.app
	$(MAKE) -C Resources clean

