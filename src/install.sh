#!/bin/bash
#This is a simple script to install Sandbox into the below directory
INSTALLDIR="/usr/local/sandbox"

if [ -a ${INSTALLDIR} ]
then
	echo "
	Warning! Previous installation potentially present in ${INSTALLDIR}
	Press Ctrl-C to abort deletion in 5 seconds"
	sleep 5
	echo "
	Deleting Current installation in ${INSTALLDIR}"
	#rm -rf ${INSTALLDIR}
fi

echo "
	Creating Directory, ${INSTALLDIR}..."
mkdir ${INSTALLDIR}
echo "
	Copying Files. This may take some time..."
cp -r ./../ /usr/local/sandbox/
echo "
	Setting Permissions, rwxr-xr-x..."
chmod -R +r ${INSTALLDIR}/
chmod +x ${INSTALLDIR}/bin/sandbox*
chmod +x ${INSTALLDIR}/sandbox_unix_usr

if [ -a /usr/bin/sandbox ]
then
	echo "
	/usr/bin/sandbox exists.. deleting..."
	rm -f /usr/bin/sandbox
fi
echo "
	Creating Symlinks..."
ln -s /usr/local/sandbox/sandbox_unix_usr /usr/bin/sandbox

if [ ${INSTALLDIR} != /usr/local/sandbox ]
then
	echo "
	Warning: the installation directory isn't /usr/local/sandbox
	so don't forget to modify the ${INSTALLDIR}/sandbox_unix_usr script to point to ${INSTALLDIR} instead of /usr/local/sandbox
	once done, type \"sandbox\" into your terminal emulator, followed with any desired command line options to run sandbox
	"
else
	echo "
	Platinum Arts Sandbox has been installed successfully
	Type \"sandbox\" into your terminal emulator, followed with any desired command line options to run sandbox
	"
fi
