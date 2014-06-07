if [ -x /usr/bin/fromdos ]
then
	echo "tofrodos found, converting enet to unix"
	fromdos enet/*
else
	echo "tofrodos isn't installed, enet source won't be converted into the unix format."
fi

if [ -x /usr/include/smpeg ]
then
	echo "the smpeg libraries were found, while not needed, this'll enable playback of MP3's"
else
	echo "the smpeg libraries were not found, while not needed, you won't be able to playback MP3's"
fi

if [ -x /usr/bin/make ]
then
	echo "Make is installed, this is good"
	if [ -x /usr/bin/g++ ]
	then
		echo "The G++ Compiler is installed, this is good"
		if [ -x /usr/include/SDL ]
		then
			echo "The SDL headers appear to be installed, this is good"
			if [ -a /usr/include/SDL/SDL_mixer.h ]
			then
				echo "The SDL Mixer headers appear to be installed, this is good"
				if [ -a /usr/include/SDL/SDL_image.h ]
				then
					echo "The SDL Image headers are installed, this is good"
					echo "You appear to have all the required headers installed, attempted to compile in 3..."
					echo ""
					sleep 3
					chmod +x enet/configure
					make clean
					make install
					exit
				else
					echo "The SDL Image headers don't appear to be installed, if you're sure they are, type make install to proceed anyway."
					exit 1
				fi
			else
				echo "The SDL Mixer headers don't appear to be installed, if you're sure they are, type make install to proceed anyway."
				exit 1
			fi
		else
			echo "The SDL headers don't appear to be installed, if you're sure they are, type make install to proceed anyway."
			exit 1
		fi
	else
		echo "G++ wasn't found, please install and try again"
		exit 1
	fi
else
	echo "Make isn't found, please install and try again."
	exit 1
fi