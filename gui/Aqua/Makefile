SUBDIRS = Resources

TOP=../..
include ${TOP}/common.mk

ifneq (${XCODE},)
all-local: app_bundle.stamp
bin_PROGRAMS=ShakesPeer
else
all-local:
endif

SOURCES=FilteringArrayController.m \
	   MenuButton.m NSMenu-UserCommandAdditions.m \
	   NSMutableAttributedString-SmileyAdditions.m \
	   NSStringExtensions.m \
	   SPApplicationController.m SPBookmarkController.m \
	   SPChatWindowController.m \
	   SPFilelistController.m SPFinishedTransferController.m \
	   SPHubController.m SPLog.m \
	   SPMainWindowController-Toolbar.m SPMainWindowController.m \
	   SPPreferenceController.m SPPublicHubsController.m \
	   SPQueueController.m \
	   SPSearchWindowController.m SPSideBar.m SPTableView.m \
	   SPTransferController.m \
	   SPTransformers.m SPUser.m SPUserCommand.m SidebarCell.m \
	   URLMutableAttributedString.m URLTextView.m main.m \
	   SPOutlineView.m \
	   SPMessagePanel.m SPGrowlBridge.m \
	   SPNotificationNames.m SPUserDefaultKeys.m SPClientBridge.m \
	   MHSysTree.m SPKeychain.m

SPApplicationController.o SPClientBridge.o: ${TOP}/version.mk

OBJS=${SOURCES:.m=.o}

CFLAGS+=-pipe -fobjc-exceptions -fpascal-strings -fobjc-direct-dispatch -F.

FRAMEWORKS = -framework Cocoa \
	     -framework SystemConfiguration\
	     -framework Growl \
	     -framework Security

ShakesPeer: Growl.framework ${OBJS} \
	    ${TOP}/spclient/libspclient.a \
	    ${TOP}/splib/libsplib.a
	${CC} ${OBJS} -o $@ ${CFLAGS} ${UB_CFLAGS} ${LDFLAGS} ${LIBS} \
	${FRAMEWORKS} -bind_at_load

GROWL_VERSION=0.7.4

Growl-$(GROWL_VERSION)-SDK.dmg:
	ftp http://growl.info/files/Growl-$(GROWL_VERSION)-SDK.dmg

Growl.framework: Growl-$(GROWL_VERSION)-SDK.dmg
	p=`hdiutil attach Growl-$(GROWL_VERSION)-SDK.dmg -private | awk '$$1 ~ /^\/dev/ && $$2 == "Apple_HFS" {print $$3}'` && \
	cp -RP $$p/Frameworks/Growl.framework . && \
	hdiutil detach $$p

app_bundle.stamp: ShakesPeer
	${MAKE} -C Resources all
	@echo "------------------------------"
	@echo "Creating application bundle..."
	@echo "------------------------------"
	mkdir -p ShakesPeer.app/Contents/MacOS
	mkdir -p ShakesPeer.app/Contents/Frameworks
	mkdir -p ShakesPeer.app/Contents/Resources/English.lproj
	cp -f Resources/Info.plist ShakesPeer.app/Contents
	cp -fR Resources/English.lproj/*[^~].nib ShakesPeer.app/Contents/Resources/English.lproj
	cp -f Resources/English.lproj/InfoPlist.strings ShakesPeer.app/Contents/Resources/English.lproj
	cp -f Resources/Images/*.png ShakesPeer.app/Contents/Resources 
	cp -f Resources/Images/*.tiff ShakesPeer.app/Contents/Resources 
	cp -f Resources/Images/*.tif ShakesPeer.app/Contents/Resources 
	cp -f Resources/Images/Smileys/*.png ShakesPeer.app/Contents/Resources 
	cp -f Resources/Images/shakespeer.icns ShakesPeer.app/Contents/Resources 
	cp -f ../../sphubd/sphubd ShakesPeer.app/Contents/Resources
	cp -f ../../sphubd/sphashd ShakesPeer.app/Contents/Resources
	cp -f ShakesPeer ShakesPeer.app/Contents/MacOS
	test -d ShakesPeer.app/Contents/Frameworks/Growl.framework || cp -RP Growl.framework ShakesPeer.app/Contents/Frameworks

install:

clean-local:
	rm -f ${bin_PROGRAMS} ${OBJS}
	${MAKE} -C Resources clean

