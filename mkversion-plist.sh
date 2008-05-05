#!/bin/sh

. ./version.mk

echo "Generating automatic update check plist for version ${VERSION}"

echo "<?xml version=\"1.0\" encoding=\"UTF-8\"?>" > sp.plist
echo "<!DOCTYPE plist PUBLIC \"-//Apple Computer//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">" >> sp.plist
echo "<plist version=\"1.0\">" >> sp.plist
echo "<dict>" >> sp.plist
echo "<key>downloadLink</key>" >> sp.plist
echo "<string>http://prdownloads.sourceforge.net/shakespeer/shakespeer-${VERSION}.dmg?download</string>" >> sp.plist
echo "<key>features</key>" >> sp.plist
echo "<array>" >> sp.plist

sed -nE "
/^New in version ${VERSION}/,/^New in version / {
/^$/d
/^[^\*]/d
s/^\* (.*)$/\1/
s/</\&lt;/
s/>/\&gt;/
s/\&/\&amp;/
s/.*/<string>&<\/string>/p
}" NEWS >> sp.plist

echo "</array>" >> sp.plist
echo "<key>releaseDate</key>" >> sp.plist
releaseDate=`date '+%Y-%m-%dT00:00:00Z'` >> sp.plist
echo "<date>${releaseDate}</date>" >> sp.plist
echo "<key>versionString</key>" >> sp.plist
echo "<string>${VERSION}</string>" >> sp.plist
echo "</dict>" >> sp.plist
echo "</plist>" >> sp.plist

echo "opening generated plist"
open sp.plist

read -e -n 1 -p "Upload to sourceforge? (y/N)" sure
test "$sure" = "n" -o "$sure" = "N" -o -z "$sure" && exit

scp sp.plist mhe@shell.sf.net\:/home/users/m/mh/mhe/shakespeer_web/

